// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "Jianzi.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>

#include "Cbor.hpp"
#include "StrokeDescRenderer.hpp"
#include "Utf8.hpp"

namespace qin

{

namespace {

struct JianziInfo {
    std::string name;
    std::string type = "Other";
    size_t codepoint_count = 0;
};

}  // namespace

struct Jianzi::LibraryData {
    struct Glyph {
        std::string type = "Other";
        BorderFlags border_flags;
        int vertical_segments = 1;
        std::optional<Capsule> capsule;
        BoundingBox range;
        std::vector<Stroke> strokes;
    };

    std::unordered_map<std::string, Glyph> glyphs;
    std::unordered_map<std::string, std::string> aliases;
};

struct JianziContext {
    static Result<std::unique_ptr<JianziContext>> Create(const nlohmann::json& library);

    Jianzi::LibraryData m_library;
    std::unique_ptr<StrokeDescRenderer> m_renderer;
    Jianzi::Layout m_layout;
    std::vector<JianziInfo> m_aliasList;
    std::vector<JianziInfo> m_jianziList;
};

namespace {

const nlohmann::json* Member(const nlohmann::json& object, const char* name) {
    if (!object.is_object()) return nullptr;
    const auto found = object.find(name);
    return found == object.end() ? nullptr : &*found;
}

template <typename T>
Result<T> LibraryError(std::string message) {
    return Result<T>::Failure({JianziErrorCode::InvalidLibrary, std::move(message)});
}

bool OptionalString(const nlohmann::json& object, const char* name, std::string& output) {
    const auto value = Member(object, name);
    if (!value) return true;
    if (!value->is_string()) return false;
    output = value->get<std::string>();
    return true;
}

bool OptionalFloat(const nlohmann::json& object, const char* name, float& output) {
    const auto value = Member(object, name);
    if (!value) return true;
    if (!value->is_number()) return false;
    const auto number = value->get<double>();
    if (!std::isfinite(number) || number < -std::numeric_limits<float>::max() ||
        number > std::numeric_limits<float>::max()) return false;
    output = static_cast<float>(number);
    return true;
}

bool RequiredInt(const nlohmann::json& object, const char* name, int& output) {
    const auto value = Member(object, name);
    if (!value || !value->is_number_integer()) return false;
    const auto number = value->get<std::int64_t>();
    if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()) return false;
    output = static_cast<int>(number);
    return true;
}

bool Flag(const nlohmann::json& flags, const char* long_name, const char* short_name, bool& output) {
    const auto value = Member(flags, long_name) ? Member(flags, long_name) : Member(flags, short_name);
    if (!value) {
        output = false;
        return true;
    }
    if (!value->is_boolean()) return false;
    output = value->get<bool>();
    return true;
}

bool ReadFlags(const nlohmann::json& flags, Jianzi::BorderFlags& output) {
    if (!flags.is_object() || !Flag(flags, "top", "t", output.t) ||
        !Flag(flags, "left", "l", output.l) || !Flag(flags, "right", "r", output.r)) return false;
    const auto bottom = Member(flags, "bottom") ? Member(flags, "bottom") : Member(flags, "b");
    output.bottom_mode = Jianzi::BottomAvoidance::Segment;
    if (!bottom) { output.b = false; return true; }
    if (bottom->is_boolean()) { output.b = bottom->get<bool>(); return true; }
    if (bottom->is_string() && bottom->get<std::string>() == "distance") {
        output.b = true;
        output.bottom_mode = Jianzi::BottomAvoidance::Distance;
        return true;
    }
    return false;
}

bool ReadPoint(const nlohmann::json& source, Point2f& output) {
    if (!source.is_array() || source.size() != 2 || !source[0].is_number() || !source[1].is_number()) {
        return false;
    }
    const auto x = source[0].get<double>();
    const auto y = source[1].get<double>();
    if (!std::isfinite(x) || !std::isfinite(y)) return false;
    output = {static_cast<float>(x), static_cast<float>(y)};
    return std::isfinite(output.x) && std::isfinite(output.y);
}

VertexRegion ParseVertexRegion(const std::string& region) {
    if (region == "Medium") return VertexRegion::Medium;
    if (region == "Bottom") return VertexRegion::Bottom;
    return VertexRegion::Top;
}

}  // namespace

Result<JianziLibrary> JianziLibrary::Load(const std::uint8_t* data, std::size_t size) {
    auto document = cbor::Read(data, size);
    if (!document) return Result<JianziLibrary>::Failure(*document.GetError());
    auto context = JianziContext::Create(*document.GetValue());
    if (!context) return Result<JianziLibrary>::Failure(*context.GetError());
    return Result<JianziLibrary>::Success(JianziLibrary(std::move(*context.GetValue())));
}

Result<std::unique_ptr<JianziContext>> JianziContext::Create(const nlohmann::json& library) {
    const auto format = Member(library, "format");
    const auto version = Member(library, "format_version");
    const auto glyphs = Member(library, "glyphs");
    const auto stroke_descs = Member(library, "stroke_descs");
    if (!format || !format->is_string() || format->get<std::string>() != "jianzinote-stroke-library" ||
        !version || !version->is_number_integer() || version->get<std::int64_t>() != 2 || !glyphs ||
        !glyphs->is_object() || !stroke_descs) {
        return LibraryError<std::unique_ptr<JianziContext>>(
            "Jianzi requires a StrokeDesc v2 CBOR library.");
    }
    auto renderer_result = StrokeDescRenderer::Create(*stroke_descs);
    if (!renderer_result) {
        return Result<std::unique_ptr<JianziContext>>::Failure(*renderer_result.GetError());
    }
    auto renderer = std::move(*renderer_result.GetValue());

    Jianzi::Layout layout;
    if (const auto found = Member(library, "layout")) {
        if (!found->is_object() || !OptionalFloat(*found, "units_per_em", layout.units_per_em) ||
            !OptionalFloat(*found, "baseline_y", layout.baseline_y)) {
            return LibraryError<std::unique_ptr<JianziContext>>("Library layout contains invalid metrics.");
        }
        if (const auto normalization = Member(*found, "glyph_normalization")) {
            if (!normalization->is_object() ||
                !OptionalFloat(*normalization, "scale", layout.normalization_scale) ||
                !OptionalFloat(*normalization, "translate_x", layout.normalization_tx) ||
                !OptionalFloat(*normalization, "translate_y", layout.normalization_ty)) {
                return LibraryError<std::unique_ptr<JianziContext>>(
                    "Library glyph normalization contains invalid values.");
            }
        }
        if (const auto composition = Member(*found, "composition")) {
            if (!composition->is_object() || !OptionalFloat(*composition, "weight_area", layout.weight_area) ||
                !OptionalFloat(*composition, "weight_base", layout.weight_base) ||
                !OptionalFloat(*composition, "border_width", layout.border_width) ||
                !OptionalFloat(*composition, "zero_segment_edge_width", layout.zero_segment_edge_width) ||
                !OptionalFloat(*composition, "capsule_weight_area", layout.capsule_weight_area) ||
                !OptionalFloat(*composition, "capsule_weight_base", layout.capsule_weight_base) ||
                !OptionalFloat(*composition, "aspect_limit", layout.aspect_limit)) {
                return LibraryError<std::unique_ptr<JianziContext>>(
                    "Library composition contains invalid values.");
            }
        }
    }
    if (!std::isfinite(layout.normalization_scale) || std::abs(layout.normalization_scale) < 1e-9f) {
        return LibraryError<std::unique_ptr<JianziContext>>(
            "layout.glyph_normalization.scale must be finite and non-zero.");
    }
    if (!std::isfinite(layout.normalization_tx) || !std::isfinite(layout.normalization_ty) ||
        !std::isfinite(layout.units_per_em) || layout.units_per_em <= 0 || !std::isfinite(layout.baseline_y)) {
        return LibraryError<std::unique_ptr<JianziContext>>("Library layout contains invalid metrics.");
    }
    const auto nonnegative = [](float value) {
        return std::isfinite(value) && value >= 0;
    };
    if (!nonnegative(layout.weight_area) || !nonnegative(layout.weight_base) ||
        !nonnegative(layout.zero_segment_edge_width) || !nonnegative(layout.capsule_weight_area) ||
        !nonnegative(layout.capsule_weight_base) || !std::isfinite(layout.border_width) ||
        !std::isfinite(layout.aspect_limit) || layout.aspect_limit < 1) {
        return LibraryError<std::unique_ptr<JianziContext>>("Library composition contains invalid metrics.");
    }

    auto context = std::make_unique<JianziContext>();
    for (auto source = glyphs->begin(); source != glyphs->end(); ++source) {
        if (!source.value().is_object()) {
            return LibraryError<std::unique_ptr<JianziContext>>("Glyph must be an object: " + source.key());
        }
        const auto decoded_name = utf8::Decode(source.key());
        if (!decoded_name || decoded_name.GetValue()->empty()) {
            return LibraryError<std::unique_ptr<JianziContext>>("Glyph name is not valid UTF-8.");
        }
        Jianzi::LibraryData::Glyph glyph;
        if (!OptionalString(source.value(), "type", glyph.type)) {
            return LibraryError<std::unique_ptr<JianziContext>>("Glyph type must be a string: " + source.key());
        }
        const auto flags = Member(source.value(), "border_flags");
        if (!flags || !ReadFlags(*flags, glyph.border_flags) ||
            !RequiredInt(source.value(), "vertical_segments", glyph.vertical_segments)) {
            return LibraryError<std::unique_ptr<JianziContext>>("Glyph metadata is invalid: " + source.key());
        }
        if (const auto range = Member(source.value(), "range_box")) {
            Point2f tl, br;
            const auto top_left = Member(*range, "top_left"), bottom_right = Member(*range, "bottom_right");
            if (!range->is_object() || !top_left || !bottom_right ||
                !ReadPoint(*top_left, tl) || !ReadPoint(*bottom_right, br) ||
                br.x <= tl.x || br.y <= tl.y || !std::isfinite(br.x-tl.x) || !std::isfinite(br.y-tl.y)) {
                return LibraryError<std::unique_ptr<JianziContext>>("Glyph range_box is invalid: " + source.key());
            }
            glyph.range = {tl.x, tl.y, br.x-tl.x, br.y-tl.y};
        }
        if (const auto capsule = Member(source.value(), "capsule")) {
            if (!capsule->is_object()) {
                return LibraryError<std::unique_ptr<JianziContext>>("Glyph capsule is invalid: " + source.key());
            }
            Jianzi::Capsule compiled;
            const auto capsule_flags = Member(*capsule, "border_flags");
            const auto top_left = Member(*capsule, "top_left");
            const auto bottom_right = Member(*capsule, "bottom_right");
            if (!capsule_flags || !ReadFlags(*capsule_flags, compiled.border_flags) ||
                !RequiredInt(*capsule, "vertical_segments", compiled.v_segments) || !top_left ||
                !ReadPoint(*top_left, compiled.tl) || !bottom_right || !ReadPoint(*bottom_right, compiled.br)) {
                return LibraryError<std::unique_ptr<JianziContext>>("Glyph capsule is invalid: " + source.key());
            }
            compiled.v_segments = std::max(0, compiled.v_segments);
            glyph.capsule = compiled;
        }
        const auto strokes = Member(source.value(), "strokes");
        if (!strokes || !strokes->is_array()) {
            return LibraryError<std::unique_ptr<JianziContext>>("Glyph strokes must be an array: " + source.key());
        }
        for (const auto& stroke_source : *strokes) {
            Stroke stroke;
            const auto desc = Member(stroke_source, "desc");
            const auto width = Member(stroke_source, "width");
            const auto nodes = Member(stroke_source, "nodes");
            if (!desc || !desc->is_string() || !width || !width->is_number() || !nodes || !nodes->is_array()) {
                return LibraryError<std::unique_ptr<JianziContext>>("Glyph stroke is invalid: " + source.key());
            }
            stroke.desc = desc->get<std::string>();
            if (!renderer->Contains(stroke.desc)) {
                return LibraryError<std::unique_ptr<JianziContext>>(
                    "Glyph " + source.key() + " references unknown StrokeDesc " + stroke.desc + ".");
            }
            const auto stroke_width = width->get<double>();
            if (!std::isfinite(stroke_width)) {
                return LibraryError<std::unique_ptr<JianziContext>>("Glyph stroke width is invalid: " + source.key());
            }
            stroke.width = static_cast<float>(stroke_width);
            for (const auto& vertex : *nodes) {
                const auto x = Member(vertex, "x");
                const auto y = Member(vertex, "y");
                if (!x || !x->is_number() || !y || !y->is_number()) {
                    return LibraryError<std::unique_ptr<JianziContext>>("Glyph stroke node is invalid: " + source.key());
                }
                const auto px = x->get<double>();
                const auto py = y->get<double>();
                std::string region = "Top";
                if (!std::isfinite(px) || !std::isfinite(py) || !OptionalString(vertex, "region", region)) {
                    return LibraryError<std::unique_ptr<JianziContext>>("Glyph stroke node is invalid: " + source.key());
                }
                stroke.vertice.push_back({{static_cast<float>(px), static_cast<float>(py)},
                                          ParseVertexRegion(region)});
            }
            glyph.strokes.push_back(std::move(stroke));
        }
        if (!context->m_library.glyphs.emplace(source.key(), std::move(glyph)).second) {
            return LibraryError<std::unique_ptr<JianziContext>>("Duplicate glyph name: " + source.key());
        }
    }

    for (const auto& [name, glyph] : context->m_library.glyphs) {
        const auto decoded = utf8::Decode(name);
        context->m_jianziList.push_back({name, glyph.type, decoded.GetValue()->size()});
    }
    const auto aliases = Member(library, "aliases");
    if (aliases && !aliases->is_array()) {
        return LibraryError<std::unique_ptr<JianziContext>>("Library aliases must be an array.");
    }
    if (aliases) for (const auto& alias : *aliases) {
        const auto name_source = Member(alias, "alias");
        const auto target_source = Member(alias, "glyph");
        if (!name_source || !name_source->is_string() || !target_source || !target_source->is_string()) {
            return LibraryError<std::unique_ptr<JianziContext>>("Alias names and targets must be strings.");
        }
        const auto name = name_source->get<std::string>();
        const auto target = target_source->get<std::string>();
        const auto decoded = utf8::Decode(name);
        const auto target_decoded = utf8::Decode(target);
        std::string type = "Other";
        if (!decoded || decoded.GetValue()->empty() || !target_decoded || target.empty() ||
            !OptionalString(alias, "type", type) ||
            !context->m_library.aliases.emplace(name, target).second) {
            return LibraryError<std::unique_ptr<JianziContext>>(
                "Alias names and targets must be non-empty valid UTF-8 and names must be unique.");
        }
        context->m_aliasList.push_back({name, type, decoded.GetValue()->size()});
    }
    const auto compare = [](const JianziInfo& a, const JianziInfo& b) {
        return a.codepoint_count != b.codepoint_count ? a.codepoint_count > b.codepoint_count : a.name > b.name;
    };
    std::sort(context->m_jianziList.begin(), context->m_jianziList.end(), compare);
    std::sort(context->m_aliasList.begin(), context->m_aliasList.end(), compare);
    context->m_renderer = std::move(renderer);
    context->m_layout = layout;
    return Result<std::unique_ptr<JianziContext>>::Success(std::move(context));
}

JianziLibrary::JianziLibrary(std::unique_ptr<JianziContext> context) : m_context(std::move(context)) {}

JianziLibrary::~JianziLibrary() = default;

JianziLibrary::JianziLibrary(JianziLibrary&& other) noexcept = default;

JianziLibrary& JianziLibrary::operator=(JianziLibrary&& other) noexcept = default;

JianziLibrary::LayoutMetrics JianziLibrary::GetLayoutMetrics() const {
    return {m_context->m_layout.units_per_em, m_context->m_layout.baseline_y};
}

Jianzi::Jianzi(const JianziContext& context) : m_context(context) {}

Jianzi::Jianzi(const JianziContext& context, const char* name) : m_context(context), m_name(name) {
    if (m_name.empty()) return;
    const auto found = m_context.m_library.glyphs.find(m_name);
    if (found == m_context.m_library.glyphs.end()) {
        const auto decoded = utf8::Decode(m_name);
        if (decoded && decoded.GetValue()->size() == 1) {
            m_status = JianziStatus::Fallback;
        } else {
            m_status = JianziStatus::Missing;
            m_missing_names.push_back(m_name);
        }
        return;
    }
    const auto& glyph = found->second;
    m_border_flags = glyph.border_flags;
    m_v_segments = glyph.vertical_segments;
    m_node = std::make_unique<Node>();
    m_node->strokes = glyph.strokes;
    m_node->name = m_name;
    m_node->range = glyph.range;
    m_node->flags = glyph.border_flags;
    m_node->segments = std::max(0, glyph.vertical_segments);
    if (glyph.capsule) m_node->capsule = std::make_unique<Capsule>(*glyph.capsule);
    m_status = JianziStatus::Renderable;
}

Jianzi::Jianzi(const Jianzi& other) : m_context(other.m_context) { CopyData(other); }

Jianzi::Jianzi(Jianzi&& other) noexcept : m_context(other.m_context) { MoveData(std::move(other)); }

Jianzi& Jianzi::operator=(const Jianzi& other) noexcept {
    if (this == &other) {
        return *this;
    }
    if (!HasSameContext(other)) return *this;
    CopyData(other);
    return *this;
}

Jianzi& Jianzi::operator=(Jianzi&& other) noexcept {
    if (this == &other) return *this;
    if (!HasSameContext(other)) return *this;
    MoveData(std::move(other));
    return *this;
}

bool Jianzi::HasSameContext(const Jianzi& other) const noexcept {
    return std::addressof(m_context) == std::addressof(other.m_context);
}

Jianzi Jianzi::MakeMissingCombination(const Jianzi& other, char operation) const {
    Jianzi result(m_context);
    result.m_name = "(" + m_name + operation + other.m_name + ")";
    result.m_status = JianziStatus::Missing;
    const auto append = [&result](const Jianzi& value) {
        const auto add = [&result](const std::string& name) {
            if (std::find(result.m_missing_names.begin(), result.m_missing_names.end(), name) ==
                result.m_missing_names.end()) {
                result.m_missing_names.push_back(name);
            }
        };
        if (value.m_status == JianziStatus::Fallback) {
            add(value.m_name);
        } else if (value.m_status == JianziStatus::Missing) {
            for (const auto& name : value.m_missing_names) add(name);
        }
    };
    append(*this);
    append(other);
    return result;
}

void Jianzi::CopyData(const Jianzi& other) {
    m_name = other.m_name;
    m_status = other.m_status;
    m_missing_names = other.m_missing_names;
    if (other.m_node) {
        m_node = std::make_unique<Node>(*other.m_node);
    } else {
        m_node.reset();
    }
    m_border_flags = other.m_border_flags;
    m_v_segments = other.m_v_segments;
}

void Jianzi::MoveData(Jianzi&& other) {
    m_name = std::move(other.m_name);
    m_status = other.m_status;
    m_missing_names = std::move(other.m_missing_names);
    m_node = std::move(other.m_node);
    m_border_flags = other.m_border_flags;
    m_v_segments = other.m_v_segments;
    other.m_status = JianziStatus::Empty;
}

struct JianziOperator {
    std::function<Result<Jianzi>(const Jianzi&, const Jianzi&)> op;
    size_t priority;
};

const std::map<char32_t, JianziOperator> c_jianzi_operators = {
    {U'(', {{}, 0}},
    {U')', {{}, 0}},
    {U'\'', {{}, 0}},

    {U'^', {[](const Jianzi& j1, const Jianzi& j2) { return j1 ^ j2; }, 1}},

    {U'|', {[](const Jianzi& j1, const Jianzi& j2) { return j1 | j2; }, 2}},

    {U'&', {[](const Jianzi& j1, const Jianzi& j2) { return j1 & j2; }, 4}},
    {U'<', {[](const Jianzi& j1, const Jianzi& j2) { return j1 < j2; }, 3}},
    {U'/', {[](const Jianzi& j1, const Jianzi& j2) { return j1 / j2; }, 3}},

    {U'*', {[](const Jianzi& j1, const Jianzi& j2) { return j1 * j2; }, 5}},
};

Result<Jianzi> JianziLibrary::Parse(const char* u8_str) const {
    Jianzi result(*m_context);

    auto decoded_input = utf8::Decode(u8_str ? std::string_view(u8_str) : std::string_view{});
    if (!decoded_input) return Result<Jianzi>::Failure(*decoded_input.GetError());
    auto str = std::move(*decoded_input.GetValue());

    // 清除所有空格
    auto ws_pos = str.find(U' ');
    while (ws_pos != str.npos) {
        str = str.erase(ws_pos);
        ws_pos = str.find(U' ');
    }

    if (str.empty()) return Result<Jianzi>::Success(std::move(result));

    // 前后添加括号，有利于后续处理
    str = U"(" + str + U")";

    std::stack<Jianzi> values;
    std::stack<char32_t> operators;
    size_t alias_expansions = 0;

    // 定义使用运算符op进行运算的函数
    auto Calc = [&values](char32_t op) -> Result<void> {
        // std::cout << "Doing operation " << string(op) << std::endl;
        if (values.size() < 2) {
            return Result<void>::Failure(
                {JianziErrorCode::InvalidFormula, "Jianzi operator is missing an operand."});
        }

        auto v2 = values.top();
        values.pop();
        auto v1 = values.top();
        values.pop();

        const auto operation = c_jianzi_operators.find(op);
        if (operation == c_jianzi_operators.end() || !operation->second.op) {
            return Result<void>::Failure(
                {JianziErrorCode::InvalidFormula, "Jianzi formula contains an invalid operator."});
        }
        auto calculated = operation->second.op(v1, v2);
        if (!calculated) return Result<void>::Failure(*calculated.GetError());
        values.push(std::move(*calculated.GetValue()));
        return Result<void>::Success();
    };

    while (str.length() > 0) {
        // std::cout << str << std::endl;
        // 寻找运算符的位置，获得运算及优先级
        auto sub_pos = std::find_if(str.begin(), str.end(),
                                    [](char32_t c) { return c_jianzi_operators.find(c) != c_jianzi_operators.end(); });
        if (sub_pos != str.end()) {
            // 找到一个运算符，开始处理
            if (sub_pos != str.begin()) {
                // 不在字符串开头，先处理值
                const auto sub = str.substr(0, sub_pos - str.begin());

                auto encoded_name = utf8::Encode(sub);
                if (!encoded_name) return Result<Jianzi>::Failure(*encoded_name.GetError());
                const auto& name = *encoded_name.GetValue();
                if (m_context->m_library.glyphs.find(name) != m_context->m_library.glyphs.end()) {
                    values.push(Jianzi(*m_context, name.c_str()));
                } else {
                    const auto alias = m_context->m_library.aliases.find(name);
                    if (alias == m_context->m_library.aliases.end()) {
                        values.push(Jianzi(*m_context, name.c_str()));
                    } else {
                        if (++alias_expansions > 1024) {
                            return Result<Jianzi>::Failure(
                                {JianziErrorCode::AliasCycle,
                                 "Alias expansion did not terminate; the library probably contains a cycle."});
                        }
                        auto decoded_alias = utf8::Decode(alias->second);
                        if (!decoded_alias) return Result<Jianzi>::Failure(*decoded_alias.GetError());
                        str = U"(" + *decoded_alias.GetValue() + U")" + str.substr(sub_pos - str.begin());
                        continue;
                    }
                }
            }

            // 处理运算符
            char32_t c = *sub_pos;  // 当前字符

            if (c == U'(') {
                // 左括号，直接入栈
                operators.push(c);
            } else if (c == U')') {
                // 右括号，直到遇到左括号为止，进行计算
                char32_t op = 0;
                while (!operators.empty()) {
                    op = operators.top();
                    operators.pop();
                    if (op == U'(') {
                        // 遇到左括号停止
                        break;
                    } else {
                        // 进行运算
                        auto calculated = Calc(op);
                        if (!calculated) return Result<Jianzi>::Failure(*calculated.GetError());
                    }
                }
                if (op != U'(') {
                    return Result<Jianzi>::Failure(
                        {JianziErrorCode::InvalidFormula, "Jianzi formula has unmatched parentheses."});
                }
            } else if (c == U'\'') {
                // 引用，寻找配对引用并置换
                size_t curr_quote = sub_pos - str.begin();
                size_t next_quote = str.find(U'\'', curr_quote + 1);
                if (next_quote == str.npos) {
                    return Result<Jianzi>::Failure(
                        {JianziErrorCode::InvalidFormula, "Jianzi formula has an unmatched quote."});
                }
                // 生成算式
                const auto sub = str.substr(curr_quote + 1, next_quote - curr_quote - 1);
                auto sub_utf8 = utf8::Encode(sub);
                if (!sub_utf8) return Result<Jianzi>::Failure(*sub_utf8.GetError());
                auto natural = ParseNatural(sub_utf8.GetValue()->c_str());
                if (!natural) return Result<Jianzi>::Failure(*natural.GetError());
                auto rep = utf8::Decode(*natural.GetValue());
                if (!rep) return Result<Jianzi>::Failure(*rep.GetError());
                // 替换内容
                str = U"(" + *rep.GetValue() + U")" + str.substr(next_quote + 1);
                continue;
            } else {
                // 普通算符

                const auto current = c_jianzi_operators.find(c);
                if (current == c_jianzi_operators.end()) {
                    return Result<Jianzi>::Failure(
                        {JianziErrorCode::InvalidFormula, "Jianzi formula contains an invalid operator."});
                }
                while (!operators.empty() && operators.top() != U'(') {
                    const auto previous = c_jianzi_operators.find(operators.top());
                    if (previous == c_jianzi_operators.end() ||
                        previous->second.priority < current->second.priority) break;
                    const char32_t op = operators.top();
                    operators.pop();
                    auto calculated = Calc(op);
                    if (!calculated) return Result<Jianzi>::Failure(*calculated.GetError());
                }
                operators.push(c);
            }
            // 仅保留找到的运算符之后的字符串
            str = str.substr(sub_pos - str.begin() + 1);
        } else {
            break;
        }
    }
    if (str.length() > 0) {
        // 已经没有运算符，剩下的是值
        auto name = utf8::Encode(str);
        if (!name) return Result<Jianzi>::Failure(*name.GetError());
        values.push(Jianzi(*m_context, name.GetValue()->c_str()));
    }

    // 将剩下的运算符算完
    while (!operators.empty()) {
        char32_t op = operators.top();
        operators.pop();
        if (op == U'(' || op == U')') {
            return Result<Jianzi>::Failure(
                {JianziErrorCode::InvalidFormula, "Jianzi formula has unmatched parentheses."});
        }
        // 进行运算
        auto calculated = Calc(op);
        if (!calculated) return Result<Jianzi>::Failure(*calculated.GetError());
    }

    if (values.empty()) return Result<Jianzi>::Success(std::move(result));
    if (values.size() != 1) {
        return Result<Jianzi>::Failure(
            {JianziErrorCode::InvalidFormula, "Jianzi formula is missing an operator."});
    }
    result = values.top();
    if (result.m_node) {
        const auto checked = Jianzi::LayoutEngine::Run(*result.m_node, m_context->m_layout, false);
        if (!checked) return Result<Jianzi>::Failure(*checked.GetError());
    }
    return Result<Jianzi>::Success(std::move(result));
}

Result<std::string> JianziLibrary::ParseNatural(const char* u8_str) const {
    // 初始化输入
    auto decoded_input = utf8::Decode(u8_str ? std::string_view(u8_str) : std::string_view{});
    if (!decoded_input) return Result<std::string>::Failure(*decoded_input.GetError());
    auto input = std::move(*decoded_input.GetValue());
    // 清除所有空格
    auto ws_pos = input.find(U' ');
    while (ws_pos != input.npos) {
        input = input.erase(ws_pos);
        ws_pos = input.find(U' ');
    }
    size_t input_length = input.length();

    if (input_length == 0) {
        // 无字符
        return Result<std::string>::Success(std::string{});
    } else if (input.front() == U'(' && input.back() == U')') {
        // 内容为公式，原样返回
        return Result<std::string>::Success(std::string(u8_str));
    }

    // 检索标记
    std::vector<bool> input_marks(input_length, false);
    std::vector<JianziInfo> info_list(input_length);

    auto MarkInput = [&input, &input_marks, &info_list](const std::vector<JianziInfo>& input_list)
        -> Result<void> {
        for (auto& info : input_list) {
            auto decoded_name = utf8::Decode(info.name);
            if (!decoded_name) return Result<void>::Failure(*decoded_name.GetError());
            const auto& info_name = *decoded_name.GetValue();
            size_t info_length = info_name.length();
            // 寻找所有点位
            auto pos = input.find(info_name);
            while (pos != input.npos) {
                // 找到一个，先确认所有位置为空
                bool occupied = false;
                for (size_t i = pos; i < pos + info_length; ++i) {
                    if (input_marks[i]) {
                        occupied = true;
                        break;
                    }
                }
                if (occupied) {
                    // 位置已被占用，寻找下一个
                    pos = input.find(info_name, pos + info_length);
                    continue;
                }
                // 标记占用
                for (size_t i = pos; i < pos + info_length; ++i) {
                    input_marks[i] = true;
                }
                info_list[pos] = info;
                // 寻找下一个
                pos = input.find(info_name, pos + info_length);
            }
        }
        return Result<void>::Success();
    };

    // 先对别名进行标记
    auto marked = MarkInput(m_context->m_aliasList);
    if (!marked) return Result<std::string>::Failure(*marked.GetError());
    // 再标记减字
    marked = MarkInput(m_context->m_jianziList);
    if (!marked) return Result<std::string>::Failure(*marked.GetError());

    // 未知字符仍保留为独立项。单独出现时可由字体 fallback；
    // 与其他项组合时，Parse 会将最终结果标记为 Missing。
    for (size_t i = 0; i < input_length; ++i) {
        if (input_marks[i]) continue;
        auto encoded = utf8::Encode(std::u32string(1, input[i]));
        if (!encoded) return Result<std::string>::Failure(*encoded.GetError());
        info_list[i] = {std::move(*encoded.GetValue()), "Other", 1};
        input_marks[i] = true;
    }

    // 把标记的空位移除，虽然可能比较耗时，但省去后续处理的麻烦
    auto ws_iter = info_list.begin();
    while (ws_iter != info_list.end()) {
        if (ws_iter->name.empty()) {
            ws_iter = info_list.erase(ws_iter);
        } else {
            ++ws_iter;
        }
    }

    // 开始根据规则进行运算符号拼装
    std::string ret;

    // 处理连串数字。左闭右开区间
    auto ProcessNumbers = [&](size_t start_pos, size_t end_pos) {
        size_t p = start_pos;
        ret += "(" + info_list[p++].name;
        while (p < end_pos) {
            if (info_list[p].type != "Number") {
                // 后方已不是数字，终止
                break;
            }
            ret += "/" + info_list[p++].name;
        }
        ret += ")";

        return p;
    };

    // 当该字为左手指法时，向后合并数字
    // auto ProcessLeft = [&](size_t start_pos, size_t end_pos) {
    //     size_t p = start_pos;
    //     ret += info_list[p++].name;

    //     if (info_list[start_pos].type == "LeftAlone")
    //     {
    //         // 单独指法，返回
    //         return p;
    //     }

    //     if (p > end_pos || info_list[p].type != "Number")
    //     {
    //         // 后方无内容或不是数字则返回
    //         return p;
    //     }
    //     // 堆数字
    //     ret += "&";
    //     p = ProcessNumbers(p, end_pos);

    //     return p;
    // };

    size_t pos = 0;
    std::string prev_type;
    while (pos < info_list.size()) {
        if (pos > 0) {
            // 跟着前面的指法，加一个运算符
            if (prev_type == "Left" || prev_type == "LeftAlone") {
                // if (info_list[pos].type == "GraceAbove")
                // {
                //     // 把修饰符归于上半，比例会更好看一些
                //     ret += "/";
                // }
                // else
                // {
                ret += "^";
                // }
            }
            // else if (prev_type == "GraceAbove")
            // {
            //     ret += "^";
            // }
            else if (prev_type == "GraceSide") {
                ret += "<";
            } else {
                ret += "/";
            }
        }
        prev_type = info_list[pos].type;  // 为下一个字做准备

        ret += info_list[pos++].name;

        // 左手指法
        if (prev_type == "Left") {
            // 后续是数字，处理数字
            if (pos < info_list.size() && info_list[pos].type == "Number") {
                ret += "&";
                pos = ProcessNumbers(pos, info_list.size());
            }
        }

        // 主字
        else if (prev_type == "Main") {
            if (pos < info_list.size() && info_list[pos].type == "Number") {
                // 后续跟的是数字，全部放入
                ret += "*";
                pos = ProcessNumbers(pos, info_list.size());
            }
        }

        // 复杂减字
        else if (prev_type == "MainComplex" || prev_type == "MainShu") {
            // 后续还有内容
            if (pos < info_list.size()) {
                // 确定运算符
                std::string op = "&";
                if (prev_type == "MainShu") {
                    op = "|";
                }

                // 后续跟的是数字，为弦号
                if (info_list[pos].type == "Number") {
                    // 对于有竖笔的减字，单个数字应当放在下方。因此需要先计算数字个数
                    size_t number_count = 0;
                    size_t n = pos;
                    while (n < info_list.size()) {
                        if (info_list[n++].type == "Number") {
                            ++number_count;
                        } else {
                            break;
                        }
                    }
                    if (number_count == 1 && prev_type == "MainShu") {
                        ret += "/(" + info_list[pos++].name;
                    } else {
                        ret += "*(" + info_list[pos++].name;
                    }
                    // 后续继续跟的是数字，再取一个
                    if (pos < info_list.size() && info_list[pos].type == "Number") {
                        ret += op + info_list[pos++].name;
                    }
                    ret += ")";
                }
                // 后续跟的是左手指法
                else if (info_list[pos].type == "Left" || info_list[pos].type == "LeftAlone") {
                    // 多加一个括号便于处理
                    ret += "*((" + info_list[pos++].name;
                    // 后续为左手指法，先统计指法后的数字个数，应当留出一个数字作为弦
                    size_t number_count = 0;
                    size_t n = pos;
                    while (n < info_list.size()) {
                        if (info_list[n++].type == "Number") {
                            ++number_count;
                        } else {
                            break;
                        }
                    }
                    if (number_count > 1) {
                        ret += "&";
                        // 留下最后一个数字
                        pos = ProcessNumbers(pos, pos + number_count - 1);
                        ret += "/" + info_list[pos++].name;
                    } else if (number_count == 1) {
                        ret += "/" + info_list[pos++].name;
                    }
                    ret += ")";

                    // 后续再接一个左手指法
                    if (pos < info_list.size() &&
                        (info_list[pos].type == "Left" || info_list[pos].type == "LeftAlone")) {
                        ret += op + "(" + info_list[pos++].name;
                        // 再来一次：后续为左手指法，先统计指法后的数字个数，应当留出一个数字作为弦
                        size_t number_count = 0;
                        size_t n = pos;
                        while (n < info_list.size()) {
                            if (info_list[n++].type == "Number") {
                                ++number_count;
                            } else {
                                break;
                            }
                        }
                        if (number_count > 1) {
                            ret += "&";
                            // 留下最后一个数字
                            pos = ProcessNumbers(pos, pos + number_count - 1);
                            ret += "/" + info_list[pos++].name;
                        } else if (number_count == 1) {
                            ret += "/" + info_list[pos++].name;
                        }
                        ret += ")";
                    }
                    // 把括号对上
                    ret += ")";
                }
            }
        }
    }

    return Result<std::string>::Success(std::move(ret));
}

Result<Jianzi> Jianzi::Combine(const Jianzi& other, char operation) const {
    if (!HasSameContext(other)) return Result<Jianzi>::Failure(
        {JianziErrorCode::ContextMismatch, "Cannot combine Jianzi objects from different libraries."});
    if (m_status == JianziStatus::Empty || other.m_status == JianziStatus::Empty)
        return Result<Jianzi>::Failure({JianziErrorCode::InvalidFormula, "Cannot combine an empty Jianzi."});
    if (m_status != JianziStatus::Renderable || other.m_status != JianziStatus::Renderable)
        return Result<Jianzi>::Success(MakeMissingCombination(other, operation));
    Jianzi result(m_context);
    result.m_name = "(" + m_name + operation + other.m_name + ")";
    result.m_status = JianziStatus::Renderable;
    result.m_node = std::make_unique<Node>();
    result.m_node->operation = operation;
    result.m_node->name = result.m_name;
    result.m_node->first = std::make_unique<Node>(*m_node);
    result.m_node->second = std::make_unique<Node>(*other.m_node);
    return Result<Jianzi>::Success(std::move(result));
}

Result<Jianzi> Jianzi::operator&(const Jianzi& right) const { return Combine(right, '&'); }
Result<Jianzi> Jianzi::operator|(const Jianzi& right) const { return Combine(right, '|'); }
Result<Jianzi> Jianzi::operator<(const Jianzi& right) const { return Combine(right, '<'); }
Result<Jianzi> Jianzi::operator/(const Jianzi& below) const { return Combine(below, '/'); }
Result<Jianzi> Jianzi::operator^(const Jianzi& below) const { return Combine(below, '^'); }
Result<Jianzi> Jianzi::operator*(const Jianzi& content) const { return Combine(content, '*'); }

Result<std::vector<PathData>> qin::Jianzi::RenderPath() const {
    if (!m_node) return Result<std::vector<PathData>>::Success({});
    auto layout = LayoutEngine::Run(*m_node, m_context.m_layout);
    if (!layout) return Result<std::vector<PathData>>::Failure(*layout.GetError());
    auto paths = m_context.m_renderer->Render(layout.GetValue()->strokes);
    if (!paths) return Result<std::vector<PathData>>::Failure(*paths.GetError());
    for (auto& command : *paths.GetValue()) {
        for (auto& point : command.pts) {
            point.x = (point.x - m_context.m_layout.normalization_tx) /
                      m_context.m_layout.normalization_scale;
            point.y = (point.y - m_context.m_layout.normalization_ty) /
                      m_context.m_layout.normalization_scale;
        }
    }
    return paths;
}

const char* Jianzi::GetName() const { return m_name.c_str(); }

JianziStatus Jianzi::GetStatus() const noexcept { return m_status; }

std::string_view Jianzi::GetFallbackName() const noexcept {
    return m_status == JianziStatus::Fallback ? std::string_view(m_name) : std::string_view{};
}

const std::vector<std::string>& Jianzi::GetMissingNames() const noexcept { return m_missing_names; }

Jianzi::BorderFlags Jianzi::GetBorderFlags() const {
    if (!m_node) return m_border_flags;
    const auto layout = LayoutEngine::Run(*m_node, m_context.m_layout, false);
    return layout ? layout.GetValue()->flags : BorderFlags{};
}

int Jianzi::GetSegments() const {
    if (!m_node) return m_v_segments;
    const auto layout = LayoutEngine::Run(*m_node, m_context.m_layout, false);
    return layout ? layout.GetValue()->segments : 0;
}

Jianzi::Node::Node(const Node& other)
    : operation(other.operation), name(other.name), range(other.range), flags(other.flags),
      segments(other.segments), strokes(other.strokes) {
    if (other.capsule) capsule = std::make_unique<Capsule>(*other.capsule);
    if (other.first) first = std::make_unique<Node>(*other.first);
    if (other.second) second = std::make_unique<Node>(*other.second);
}

Jianzi::Node& Jianzi::Node::operator=(const Node& other) {
    if (this != &other) { Node copy(other); *this = std::move(copy); }
    return *this;
}
}  // namespace qin
