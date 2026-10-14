#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

#include "Jianzi.hpp"

namespace qin {

Result<Jianzi::LayoutOutput> Jianzi::LayoutEngine::Run(const Node& source, const Layout& config, bool render) {
    // All state belongs to this evaluation. Original nodes and the library remain immutable.
    struct Engine {
        enum Side { T, B, L, R };
        using Sides = std::array<float, 4>;
        struct Bounds {
            BoundingBox box;
            bool empty = true;
        };
        struct Item {
            const Node* source = nullptr;
            char op = 0;
            Item *first = nullptr, *second = nullptr;
            std::array<std::vector<Item*>, 4> faces;
            Bounds original;
            Sides reserved{}, coefficients{}, outer{}, margins{};
            bool active_x = false, active_y = false;
            int base = 0, effective = 0, base_tree = -1, extra = 0, above = 0;
            float max_width = 0;
            BoundingBox allocation, transform;
            std::vector<float> ratios;
            std::vector<Stroke> adjusted;
            std::optional<Capsule> adjusted_capsule;
            float capsule_ratio = 1;
            bool capsule_adjusted = false, fitted = false;
        };
        struct Relation {
            Item* node;
            int side_a, side_b;
            std::vector<Item*> first, second;
            bool capsule = false, horizontal = false, wall = false;
        };
        const Layout& config;
        bool render;
        std::vector<std::unique_ptr<Item>> items;
        std::vector<Relation> relations;
        LayoutOutput output;
        std::string error;
        Item* root = nullptr;

        static float Epsilon() { return 1e-9f; }
        static float Area(const BoundingBox& box) { return std::max(0.0f, box.w) * std::max(0.0f, box.h); }
        static bool Horizontal(char op) { return op == '&' || op == '|' || op == '<'; }
        static bool Flag(const BorderFlags& f, int side) {
            return side == T ? f.t : side == B ? f.b : side == L ? f.l : f.r;
        }
        static void Add(std::vector<Item*>& into, const std::vector<Item*>& from) {
            for (auto* leaf : from)
                if (std::find(into.begin(), into.end(), leaf) == into.end()) into.push_back(leaf);
        }
        static Bounds Envelope(const std::vector<Stroke>& strokes, const Capsule* cap) {
            float x1 = std::numeric_limits<float>::max(), y1 = x1;
            float x2 = -x1, y2 = -x1;
            bool found = false;
            const auto add = [&](Point2f p) {
                x1 = std::min(x1, p.x);
                x2 = std::max(x2, p.x);
                y1 = std::min(y1, p.y);
                y2 = std::max(y2, p.y);
                found = true;
            };
            for (const auto& stroke : strokes)
                for (const auto& vertex : stroke.vertice) add(vertex.pt);
            if (cap) {
                add(cap->tl);
                add(cap->br);
            }
            return found ? Bounds{{x1, y1, x2 - x1, y2 - y1}, false} : Bounds{};
        }
        float Weight(float ratio) const {
            return config.weight_area * std::sqrt(std::max(0.0f, ratio)) + config.weight_base;
        }
        Item* Build(const Node& source) {
            items.push_back(std::make_unique<Item>());
            auto* item = items.back().get();
            item->source = &source;
            item->op = source.operation;
            if (!item->op) {
                item->original = Envelope(source.strokes, source.capsule.get());
                const auto& b = item->original.box;
                const auto& f = source.range;
                item->margins = {std::max(0.0f, b.y - f.y), std::max(0.0f, f.y + f.h - b.y - b.h),
                                 std::max(0.0f, b.x - f.x), std::max(0.0f, f.x + f.w - b.x - b.w)};
                item->base = item->effective = std::max(0, source.segments);
                item->base_tree = item->base;
                for (int side = 0; side < 4; ++side) item->faces[side] = {item};
                for (const auto& stroke : source.strokes) item->max_width = std::max(item->max_width, stroke.width);
                return item;
            }
            item->first = Build(*source.first);
            item->second = Build(*source.second);
            auto *a = item->first, *b = item->second;
            if (item->op == '*' && !a->source->capsule) item->op = '/';
            if (item->op == '*') {
                if (a->op) {
                    error = "Capsule host must be a single glyph.";
                    return item;
                }
                const auto& cap = *a->source->capsule;
                const auto& box = a->original.box;
                const std::array<bool, 4> exposed = {a->original.empty || cap.tl.y <= box.y + 1e-7f,
                                                     a->original.empty || cap.br.y >= box.y + box.h - 1e-7f,
                                                     a->original.empty || cap.tl.x <= box.x + 1e-7f,
                                                     a->original.empty || cap.br.x >= box.x + box.w - 1e-7f};
                for (int side = 0; side < 4; ++side) {
                    item->faces[side] = a->faces[side];
                    if (exposed[side]) Add(item->faces[side], b->faces[side]);
                    relations.push_back(
                        {item, side, side, {a}, b->faces[side], true, false, Flag(cap.border_flags, side)});
                }
            } else {
                const bool horizontal = Horizontal(item->op);
                for (int side = 0; side < 4; ++side) {
                    const bool first = horizontal ? side == L : side == T, second = horizontal ? side == R : side == B;
                    if (!second) Add(item->faces[side], a->faces[side]);
                    if (!first) Add(item->faces[side], b->faces[side]);
                }
                const int sa = horizontal ? R : B, sb = horizontal ? L : T;
                relations.push_back({item, sa, sb, a->faces[sa], b->faces[sb], false, horizontal, false});
            }
            return item;
        }
        int SegmentCount(std::int64_t value) {
            if (value < 0 || value > std::numeric_limits<int>::max()) {
                error = "Composition segment count exceeds the supported range.";
                return 0;
            }
            return static_cast<int>(value);
        }
        int Measure(Item& item) {
            if (!item.op) {
                item.effective = SegmentCount(static_cast<std::int64_t>(item.base) +
                                              static_cast<int>(item.reserved[T] + item.reserved[B]));
                item.extra = item.effective - item.base;
                return item.effective;
            }
            auto& a = *item.first;
            auto& b = *item.second;
            const int ca = Measure(a), cb = Measure(b);
            if (Horizontal(item.op))
                item.effective = std::max(ca, cb);
            else if (item.op == '*')
                item.effective =
                    SegmentCount(static_cast<std::int64_t>(ca) + std::max(cb, a.source->capsule->v_segments) -
                                 a.source->capsule->v_segments);
            else {
                item.above = ca;
                if (item.op == '^' && a.base_tree > 0 && b.base_tree > 0)
                    item.above = SegmentCount(
                        static_cast<std::int64_t>(std::max(std::min(a.base_tree, b.base_tree),
                                                           static_cast<int>(std::round(b.base_tree * .3f)))) +
                        a.extra);
                item.effective = SegmentCount(static_cast<std::int64_t>(item.above) + cb);
            }
            if (item.base_tree < 0) item.base_tree = item.effective;
            item.extra = std::max(0, item.effective - item.base_tree);
            return item.effective;
        }
        void Distribute(Item& item, BoundingBox box, const std::vector<float>& ratios = {}, bool provisional = false) {
            item.allocation = box;
            item.ratios = ratios;
            if (!item.op) return;
            auto& a = *item.first;
            auto& b = *item.second;
            const auto descend = [&](Item& child, BoundingBox target, std::optional<float> ratio = std::nullopt) {
                auto next = ratios;
                next.push_back(ratio.value_or(Area(box) > Epsilon() ? Area(target) / Area(box) : 1));
                Distribute(child, target, next, provisional);
            };
            if (Horizontal(item.op)) {
                const float split = item.op == '<' ? .33f : .5f;
                descend(a, {box.x, box.y, box.w * split, box.h});
                descend(b, {box.x + box.w * split, box.y, box.w * (1 - split), box.h});
            } else if (item.op == '*') {
                Distribute(a, box, ratios, provisional);
                const auto& cap = *a.source->capsule;
                const float ratio =
                    a.effective > 0 && item.effective > 0 ? static_cast<float>(a.effective) / item.effective : 1;
                const auto& host = a.original.box;
                const float top = host.y, bottom = host.y + host.h;
                auto adjusted = cap;
                adjusted.tl.y = top + (cap.tl.y - top) * ratio;
                adjusted.br.y = bottom - (bottom - cap.br.y) * ratio;
                if (!provisional) {
                    a.capsule_ratio = ratio;
                    a.capsule_adjusted = true;
                    a.adjusted = a.source->strokes;
                    for (auto& stroke : a.adjusted)
                        for (auto& vertex : stroke.vertice) {
                            if (vertex.region == VertexRegion::Top)
                                vertex.pt.y = top + (vertex.pt.y - top) * ratio;
                            else if (vertex.region == VertexRegion::Bottom)
                                vertex.pt.y = bottom - (bottom - vertex.pt.y) * ratio;
                            else if (std::abs(cap.br.y - cap.tl.y) > Epsilon())
                                vertex.pt.y = adjusted.tl.y + (vertex.pt.y - cap.tl.y) / (cap.br.y - cap.tl.y) *
                                                                  (adjusted.br.y - adjusted.tl.y);
                        }
                    a.adjusted_capsule = adjusted;
                }
                const auto& f = a.source->range;
                const Point2f start = box * Point2f{(adjusted.tl.x - f.x) / f.w, (adjusted.tl.y - f.y) / f.h},
                              end = box * Point2f{(adjusted.br.x - f.x) / f.w, (adjusted.br.y - f.y) / f.h};
                descend(b, {start.x, start.y, std::max(0.0f, end.x - start.x), std::max(0.0f, end.y - start.y)});
            } else if (a.base_tree == 0 || b.base_tree == 0) {
                auto& zero = a.base_tree == 0 ? a : b;
                auto& other = &zero == &a ? b : a;
                float width = 0;
                for (const auto& face : zero.faces)
                    for (auto* leaf : face) width = std::max(width, leaf->max_width);
                float inherited = 1;
                for (float ratio : ratios) inherited *= Weight(ratio);
                const int side = &zero == &a ? T : B;
                bool outer = false;
                for (auto* leaf : zero.faces[side]) outer |= leaf->outer[side] != 0;
                const float height =
                    std::min(box.h, 2 * width * inherited + (outer ? config.zero_segment_edge_width : 0));
                const float remaining = std::max(0.0f, box.h - height),
                            other_height = other.effective > 0
                                               ? remaining * other.effective /
                                                     (static_cast<float>(other.effective) + zero.effective)
                                               : remaining;
                const float gap = remaining - other_height, ratio = box.h > Epsilon() ? other_height / box.h : 1;
                descend(zero, {box.x, &zero == &a ? box.y : box.y + other_height + gap, box.w, height}, ratio);
                descend(other, {box.x, &zero == &a ? box.y + height + gap : box.y, box.w, other_height}, ratio);
            } else {
                const auto total = static_cast<std::int64_t>(item.above) + b.effective;
                const float split = total > 0 ? static_cast<float>(item.above) / total : .5f;
                descend(a, {box.x, box.y, box.w, box.h * split});
                descend(b, {box.x, box.y + box.h * split, box.w, box.h * (1 - split)});
            }
        }
        bool Overlap(const Item& a, const Item& b, bool horizontal) {
            const auto& x = a.allocation;
            const auto& y = b.allocation;
            return horizontal ? std::min(x.y + x.h, y.y + y.h) - std::max(x.y, y.y) > Epsilon()
                              : std::min(x.x + x.w, y.x + y.w) - std::max(x.x, y.x) > Epsilon();
        }
        bool Resolve() {
            for (const auto& relation : relations) {
                struct Pair {
                    Item *a, *b;
                    bool used = false;
                };
                std::vector<Pair> pairs;
                for (auto* a : relation.first)
                    for (auto* b : relation.second)
                        if (relation.capsule || Overlap(*a, *b, relation.horizontal)) pairs.push_back({a, b});
                for (size_t seed = 0; seed < pairs.size(); ++seed)
                    if (!pairs[seed].used) {
                        std::vector<Item*> as{pairs[seed].a}, bs{pairs[seed].b};
                        std::vector<size_t> group{seed};
                        pairs[seed].used = true;
                        bool changed = true;
                        while (changed) {
                            changed = false;
                            for (size_t i = 0; i < pairs.size(); ++i) {
                                auto& p = pairs[i];
                                if (!p.used && (std::find(as.begin(), as.end(), p.a) != as.end() ||
                                                std::find(bs.begin(), bs.end(), p.b) != bs.end())) {
                                    Add(as, {p.a});
                                    Add(bs, {p.b});
                                    group.push_back(i);
                                    p.used = true;
                                    changed = true;
                                }
                            }
                        }
                        for (size_t index : group) {
                            const auto& p = pairs[index];
                            auto& a = *p.a;
                            auto& b = *p.b;
                            const bool x = relation.horizontal || relation.side_b == L || relation.side_b == R;
                            if (!relation.capsule) {
                                if (x)
                                    a.active_x = true;
                                else
                                    a.active_y = true;
                            }
                            if (x)
                                b.active_x = true;
                            else
                                b.active_y = true;
                            const bool fa = relation.capsule ? relation.wall : Flag(a.source->flags, relation.side_a),
                                       fb = Flag(b.source->flags, relation.side_b), conflict = fa && fb;
                            if (!relation.capsule && as.size() > 1 && bs.size() > 1 && conflict) {
                                error = "Connected many-to-many border avoidance is not supported: " +
                                        relation.node->source->name;
                                return false;
                            }
                            if (conflict && !relation.horizontal && (relation.side_b == T || relation.side_b == B)) {
                                auto& owner = relation.capsule || bs.size() > 1 ? b : a;
                                owner.reserved[&owner == &b ? relation.side_b : relation.side_a] = 1;
                            }
                            if (relation.capsule) {
                                const bool vertical = relation.side_b == T || relation.side_b == B;
                                const float coefficient =
                                    relation.wall ? (vertical ? (fb ? 0.0f : 1.0f) : (fb ? 2.0f : 1.0f)) : 0;
                                b.coefficients[relation.side_b] = std::max(
                                    b.coefficients[relation.side_b], b.base == 0 && vertical ? 0.0f : coefficient);
                            } else {
                                const bool special = relation.node->op == '|';
                                const float coefficient = conflict   ? (relation.horizontal ? 1.0f : 0.0f)
                                                          : fa != fb ? .5f
                                                                     : .25f;
                                a.coefficients[relation.side_a] = std::max(a.coefficients[relation.side_a],
                                                                           special ? (fa ? 2.0f : 1.0f) : coefficient);
                                b.coefficients[relation.side_b] = std::max(b.coefficients[relation.side_b],
                                                                           special ? (fb ? 2.0f : 1.0f) : coefficient);
                            }
                        }
                    }
            }
            return true;
        }
        void Fit(Item& leaf, bool preserve = false) {
            const auto& source = leaf.capsule_adjusted ? leaf.adjusted : leaf.source->strokes;
            const Capsule* cap = leaf.adjusted_capsule ? &*leaf.adjusted_capsule : leaf.source->capsule.get();
            const auto envelope = Envelope(source, cap);
            if (envelope.empty) return;
            const auto& raw = envelope.box;
            const auto& original = leaf.original.box;
            const auto& box = leaf.allocation;
            const auto& range = leaf.source->range;
            const auto margin = root->op && !preserve ? leaf.margins : Sides{};
            const bool nx = leaf.active_x && !preserve, ny = leaf.active_y && !preserve, zero = leaf.base == 0;
            const float unit = !zero && leaf.effective > 0 ? box.h / leaf.effective : 0,
                        ct = box.y + leaf.reserved[T] * unit, cb = box.y + box.h - leaf.reserved[B] * unit;
            const float x1 = preserve        ? original.x
                             : leaf.outer[L] ? std::max(box.x, margin[L])
                                             : box.x,
                        x2 = preserve        ? original.x + original.w
                             : leaf.outer[R] ? std::min(box.x + box.w, 1 - margin[R])
                                             : box.x + box.w;
            const float y1 = preserve        ? original.y
                             : leaf.outer[T] ? std::max(ct, margin[T])
                                             : ct,
                        y2 = preserve        ? original.y + original.h
                             : leaf.outer[B] ? std::min(cb, 1 - margin[B])
                                             : cb;
            const float aw = std::max(0.0f, x2 - x1), ah = std::max(0.0f, y2 - y1);
            float sx = preserve                  ? 1
                       : nx && raw.w > Epsilon() ? aw / raw.w
                                                 : box.w / range.w,
                  sy = preserve                  ? 1
                       : ny && raw.h > Epsilon() ? ah / raw.h
                                                 : box.h / range.h;
            bool cx = nx, cy = ny || zero, ax = false, ay = false;
            const auto centered = [&](bool horizontal, float scale, float size, float low, float high) {
                const float anchor = horizontal ? box.x + box.w / 2 : (ct + cb) / 2, start = horizontal ? raw.x : raw.y,
                            center = horizontal ? range.x + range.w / 2 : range.y + range.h / 2;
                return std::max(low, std::min(anchor + (start - center) * scale, high - size));
            };
            if (raw.w * sx > aw + Epsilon() && raw.w > Epsilon()) {
                sx = aw / raw.w;
                cx = true;
            }
            if (!zero && raw.h * sy > ah + Epsilon() && raw.h > Epsilon()) {
                sy = ah / raw.h;
                cy = true;
            }
            if (zero)
                sy = 1;
            else {
                const float tx = original.w > Epsilon() ? sx * raw.w / original.w : sx,
                            ty = original.h > Epsilon() ? sy * raw.h / original.h : sy;
                if (tx > ty * config.aspect_limit) {
                    sx *= ty * config.aspect_limit / tx;
                    cx = ax = true;
                } else if (ty > tx * config.aspect_limit) {
                    sy *= tx * config.aspect_limit / ty;
                    cy = ay = true;
                }
            }
            if (zero && raw.h > ah + Epsilon()) sy = raw.h > Epsilon() ? ah / raw.h : 1;
            const float nw = raw.w * sx, nh = raw.h * sy, bx = box.x + box.w * (raw.x - range.x) / range.w,
                        by = box.y + box.h * (raw.y - range.y) / range.h;
            float x = ax   ? centered(true, sx, nw, x1, x2)
                      : cx ? x1 + (aw - nw) / 2
                           : std::max(x1, std::min(bx, x2 - nw));
            float y = ay   ? centered(false, sy, nh, y1, y2)
                      : cy ? y1 + (ah - nh) / 2
                           : std::max(y1, std::min(by, y2 - nh));
            float weight = 1;
            if (!preserve)
                for (float ratio : leaf.ratios) weight *= Weight(ratio);
            const float inherited = Area(box), actual = std::max(0.0f, sx * sy);
            if (!zero && !preserve)
                weight *= Weight(inherited > Epsilon() ? actual / inherited : actual);
            else if (zero && !preserve && box.w > Epsilon())
                weight *= Weight(std::max(0.0f, sx / box.w));
            if (leaf.capsule_adjusted && std::abs(leaf.capsule_ratio - 1) > Epsilon())
                weight *= config.capsule_weight_area * std::sqrt(leaf.capsule_ratio) + config.capsule_weight_base;
            Sides requested{}, insets{};
            const Sides gaps = {std::max(0.0f, y - box.y), std::max(0.0f, box.y + box.h - y - nh),
                                std::max(0.0f, x - box.x), std::max(0.0f, box.x + box.w - x - nw)};
            for (int side = 0; side < 4; ++side) {
                requested[side] = leaf.coefficients[side] * config.border_width * weight;
                insets[side] = requested[side] > 0 ? std::max(0.0f, requested[side] - gaps[side]) : requested[side];
            }
            float w = std::max(0.0f, nw - insets[L] - insets[R]), h = std::max(0.0f, nh - insets[T] - insets[B]);
            x += insets[L];
            y += insets[T];
            if (!zero && original.w > Epsilon() && original.h > Epsilon()) {
                const float tx = w / original.w, ty = h / original.h;
                if (tx > ty * config.aspect_limit) {
                    w = ty * config.aspect_limit * original.w;
                    x = centered(true, raw.w > Epsilon() ? w / raw.w : sx, w,
                                 std::max(x1, box.x + std::max(0.0f, requested[L])),
                                 std::min(x2, box.x + box.w - std::max(0.0f, requested[R])));
                } else if (ty > tx * config.aspect_limit) {
                    h = tx * config.aspect_limit * original.h;
                    y = centered(false, raw.h > Epsilon() ? h / raw.h : sy, h,
                                 std::max(y1, box.y + std::max(0.0f, requested[T])),
                                 std::min(y2, box.y + box.h - std::max(0.0f, requested[B])));
                }
            }
            const float fx = raw.w > Epsilon() ? w / raw.w : sx, fy = raw.h > Epsilon() ? h / raw.h : sy;
            leaf.transform = {x - fx * raw.x, y - fy * raw.y, fx, fy};
            leaf.fitted = true;
            if ((raw.w > Epsilon() && w <= Epsilon()) || (raw.h > Epsilon() && h <= Epsilon())) return;
            for (auto stroke : source) {
                stroke.width *= weight;
                for (auto& vertex : stroke.vertice) vertex.pt = leaf.transform * vertex.pt;
                output.strokes.push_back(std::move(stroke));
            }
        }
        void Place(Item& item) {
            if (!item.op) {
                Fit(item);
                return;
            }
            if (item.op == '*') {
                auto& host = *item.first;
                Fit(host, &item == root);
                const auto& cap = host.adjusted_capsule ? *host.adjusted_capsule : *host.source->capsule;
                if (host.fitted) {
                    const auto tl = host.transform * cap.tl, br = host.transform * cap.br;
                    const BoundingBox box{tl.x, tl.y, std::max(0.0f, br.x - tl.x), std::max(0.0f, br.y - tl.y)};
                    const float before = Area(item.second->allocation);
                    auto ratios = item.second->ratios;
                    ratios.push_back(before > Epsilon() ? Area(box) / before : 1);
                    Distribute(*item.second, box, ratios);
                    Place(*item.second);
                }
            } else {
                Place(*item.first);
                Place(*item.second);
            }
        }
        Result<LayoutOutput> Run(const Node& source) {
            root = Build(source);
            if (!error.empty()) return Result<LayoutOutput>::Failure({JianziErrorCode::InvalidGeometry, error});
            for (int side = 0; side < 4; ++side)
                for (auto* leaf : root->faces[side]) leaf->outer[side] = 1;
            Measure(*root);
            if (!error.empty()) return Result<LayoutOutput>::Failure({JianziErrorCode::InvalidGeometry, error});
            Distribute(*root, {}, {}, true);
            if (!Resolve()) return Result<LayoutOutput>::Failure({JianziErrorCode::InvalidGeometry, error});
            Measure(*root);
            if (!error.empty()) return Result<LayoutOutput>::Failure({JianziErrorCode::InvalidGeometry, error});
            output.segments = root->effective;
            std::array<bool, 4> flags{};
            for (int side = 0; side < 4; ++side)
                for (auto* leaf : root->faces[side]) flags[side] = flags[side] || Flag(leaf->source->flags, side);
            output.flags = {flags[T], flags[B], flags[L], flags[R]};
            if (render) {
                Distribute(*root, {});
                if (root->op)
                    Place(*root);
                else
                    output.strokes = source.strokes;
            }
            for (const auto& stroke : output.strokes) {
                if (!std::isfinite(stroke.width))
                    return Result<LayoutOutput>::Failure(
                        {JianziErrorCode::InvalidGeometry, "Layout produced an invalid width."});
                for (const auto& vertex : stroke.vertice)
                    if (!std::isfinite(vertex.pt.x) || !std::isfinite(vertex.pt.y))
                        return Result<LayoutOutput>::Failure(
                            {JianziErrorCode::InvalidGeometry, "Layout produced invalid coordinates."});
            }
            return Result<LayoutOutput>::Success(std::move(output));
        }
    };
    Engine engine{config, render};
    return engine.Run(source);
}
}  // namespace qin
