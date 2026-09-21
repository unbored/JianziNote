// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "Jianzi.hpp"

#include <tinyutf8/tinyutf8.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <functional>
#include <limits>
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <optional>
#include <set>
#include <stack>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "Cbor.hpp"
#include "StrokeDescRenderer.hpp"

namespace qin

{

struct Jianzi::LibraryData {
    struct Glyph {
        JianziType type = JianziType::Other;
        BorderFlags border_flags;
        int vertical_segments = 1;
        std::optional<Capsule> capsule;
        std::vector<Stroke> strokes;
    };

    std::unordered_map<std::string, Glyph> glyphs;
    std::unordered_map<std::string, std::string> aliases;
};

struct JianziContext {
    Jianzi::LibraryData m_library;
    std::unique_ptr<StrokeDescRenderer> m_renderer;
    Jianzi::Layout m_layout;
    std::vector<Jianzi::JianziInfo> m_aliasList;
    std::vector<Jianzi::JianziInfo> m_jianziList;
};

namespace {

bool Flag(const nlohmann::json& flags, const char* long_name, const char* short_name) {
    if (flags.contains(long_name)) return flags.at(long_name).get<bool>();
    return flags.value(short_name, false);
}

}  // namespace

JianziLibrary JianziLibrary::LoadFile(const std::filesystem::path& file) {
    const auto result = cbor::Read(file);
    if (!result) {
        throw std::runtime_error(result.error->message);
    }
    const auto& library = result.document;
    if (library.value("format", std::string{}) != "jianzinote-stroke-library" ||
        library.value("format_version", 0) != 2 || !library.contains("glyphs") ||
        !library.at("glyphs").is_object() || !library.contains("stroke_descs")) {
        throw std::runtime_error("Jianzi requires a StrokeDesc v2 CBOR library.");
    }
    auto renderer = std::make_unique<StrokeDescRenderer>(library.at("stroke_descs"));

    Jianzi::Layout layout;
    if (const auto found = library.find("layout"); found != library.end() && found->is_object()) {
        layout.units_per_em = found->value("units_per_em", layout.units_per_em);
        layout.baseline_y = found->value("baseline_y", layout.baseline_y);
        if (const auto normalization = found->find("glyph_normalization");
            normalization != found->end() && normalization->is_object()) {
            layout.normalization_scale = normalization->value("scale", layout.normalization_scale);
            layout.normalization_tx = normalization->value("translate_x", layout.normalization_tx);
            layout.normalization_ty = normalization->value("translate_y", layout.normalization_ty);
        }
        if (const auto composition = found->find("composition");
            composition != found->end() && composition->is_object()) {
            layout.weight_area = composition->value("weight_area", layout.weight_area);
            layout.weight_base = composition->value("weight_base", layout.weight_base);
            layout.border_width = composition->value("border_width", layout.border_width);
            layout.zero_segment_edge_width =
                composition->value("zero_segment_edge_width", layout.zero_segment_edge_width);
            layout.capsule_weight_area = composition->value("capsule_weight_area", layout.capsule_weight_area);
            layout.capsule_weight_base = composition->value("capsule_weight_base", layout.capsule_weight_base);
        }
    }
    if (!std::isfinite(layout.normalization_scale) || std::abs(layout.normalization_scale) < 1e-9f) {
        throw std::runtime_error("layout.glyph_normalization.scale must be finite and non-zero.");
    }
    if (!std::isfinite(layout.normalization_tx) || !std::isfinite(layout.normalization_ty) ||
        !std::isfinite(layout.units_per_em) || layout.units_per_em <= 0 || !std::isfinite(layout.baseline_y)) {
        throw std::runtime_error("Library layout contains invalid metrics.");
    }
    const auto require_nonnegative = [](float value, const char* name) {
        if (!std::isfinite(value) || value < 0) throw std::runtime_error(std::string(name) + " must be non-negative.");
    };
    require_nonnegative(layout.weight_area, "layout.composition.weight_area");
    require_nonnegative(layout.weight_base, "layout.composition.weight_base");
    require_nonnegative(layout.zero_segment_edge_width, "layout.composition.zero_segment_edge_width");
    require_nonnegative(layout.capsule_weight_area, "layout.composition.capsule_weight_area");
    require_nonnegative(layout.capsule_weight_base, "layout.composition.capsule_weight_base");
    if (!std::isfinite(layout.border_width)) {
        throw std::runtime_error("layout.composition.border_width must be finite.");
    }

    auto context = std::make_unique<JianziContext>();
    for (auto source = library.at("glyphs").begin(); source != library.at("glyphs").end(); ++source) {
        Jianzi::LibraryData::Glyph glyph;
        glyph.type = magic_enum::enum_cast<JianziType>(source.value().at("type").get<std::string>())
                         .value_or(JianziType::Other);
        const auto& flags = source.value().at("border_flags");
        glyph.border_flags = {Flag(flags, "top", "t"), Flag(flags, "bottom", "b"),
                              Flag(flags, "left", "l"), Flag(flags, "right", "r")};
        glyph.vertical_segments = source.value().at("vertical_segments").get<int>();
        if (const auto capsule = source.value().find("capsule"); capsule != source.value().end()) {
            Jianzi::Capsule compiled;
            const auto& capsule_flags = capsule->at("border_flags");
            compiled.border_flags = {Flag(capsule_flags, "top", "t"), Flag(capsule_flags, "bottom", "b"),
                                     Flag(capsule_flags, "left", "l"), Flag(capsule_flags, "right", "r")};
            compiled.v_segments = capsule->at("vertical_segments").get<int>();
            compiled.tl = {capsule->at("top_left").at(0).get<float>(),
                           capsule->at("top_left").at(1).get<float>()};
            compiled.br = {capsule->at("bottom_right").at(0).get<float>(),
                           capsule->at("bottom_right").at(1).get<float>()};
            glyph.capsule = compiled;
        }
        for (const auto& stroke_source : source.value().at("strokes")) {
            Stroke stroke;
            stroke.desc = stroke_source.at("desc").get<std::string>();
            if (!renderer->Contains(stroke.desc)) {
                throw std::runtime_error("Glyph " + source.key() + " references unknown StrokeDesc " + stroke.desc + ".");
            }
            stroke.width = stroke_source.at("width").get<float>();
            for (const auto& vertex : stroke_source.at("nodes")) {
                stroke.vertice.push_back(
                    {{vertex.at("x").get<float>(), vertex.at("y").get<float>()},
                     magic_enum::enum_cast<VertexRegion>(vertex.value("region", std::string{"Top"}))
                         .value_or(VertexRegion::Top)});
            }
            glyph.strokes.push_back(std::move(stroke));
        }
        if (!context->m_library.glyphs.emplace(source.key(), std::move(glyph)).second) {
            throw std::runtime_error("Duplicate glyph name: " + source.key());
        }
    }

    for (const auto& [name, glyph] : context->m_library.glyphs) {
        context->m_jianziList.push_back({name, glyph.type});
    }
    for (const auto& alias : library.value("aliases", nlohmann::json::array())) {
        const auto name = alias.at("alias").get<std::string>();
        const auto target = alias.at("glyph").get<std::string>();
        if (name.empty() || target.empty() || !context->m_library.aliases.emplace(name, target).second) {
            throw std::runtime_error("Alias names and targets must be non-empty and names must be unique.");
        }
        context->m_aliasList.push_back(
            {name, magic_enum::enum_cast<JianziType>(alias.value("type", std::string{"Other"}))
                       .value_or(JianziType::Other)});
    }
    const auto compare = [](const Jianzi::JianziInfo& a, const Jianzi::JianziInfo& b) {
        tiny_utf8::string an = a.name, bn = b.name;
        return an.length() != bn.length() ? an.length() > bn.length() : an > bn;
    };
    std::sort(context->m_jianziList.begin(), context->m_jianziList.end(), compare);
    std::sort(context->m_aliasList.begin(), context->m_aliasList.end(), compare);
    context->m_renderer = std::move(renderer);
    context->m_layout = layout;
    return JianziLibrary(std::move(context));
}

JianziLibrary::JianziLibrary(std::unique_ptr<JianziContext> context) : m_context(std::move(context)) {}

JianziLibrary::~JianziLibrary() = default;

JianziLibrary::JianziLibrary(JianziLibrary&& other) noexcept = default;

JianziLibrary& JianziLibrary::operator=(JianziLibrary&& other) noexcept = default;

Jianzi::Jianzi(const JianziContext& context) : m_context(context) {}

Jianzi::Jianzi(const JianziContext& context, const char* name) : m_context(context), m_name(name) {
    if (m_name.empty()) return;
    const auto found = m_context.m_library.glyphs.find(m_name);
    if (found == m_context.m_library.glyphs.end()) return;
    const auto& glyph = found->second;
    m_border_flags = glyph.border_flags;
    m_v_segments = glyph.vertical_segments;
    m_type = glyph.type;
    if (glyph.capsule) m_capsule = std::make_unique<Capsule>(*glyph.capsule);
    m_node = std::make_unique<Node>();
    m_node->strokes = glyph.strokes;
}

Jianzi::Jianzi(const Jianzi& other) : m_context(other.m_context) { CopyData(other); }

Jianzi::Jianzi(Jianzi&& other) noexcept : m_context(other.m_context) { MoveData(std::move(other)); }

Jianzi& Jianzi::operator=(const Jianzi& other) {
    if (this == &other) {
        return *this;
    }
    CheckContext(other);
    CopyData(other);
    return *this;
}

Jianzi& Jianzi::operator=(Jianzi&& other) {
    if (this == &other) return *this;
    CheckContext(other);
    MoveData(std::move(other));
    return *this;
}

void Jianzi::CheckContext(const Jianzi& other) const {
    if (std::addressof(m_context) != std::addressof(other.m_context)) {
        throw std::invalid_argument("Cannot combine or assign Jianzi objects from different libraries.");
    }
}

void Jianzi::CopyData(const Jianzi& other) {
    m_name = other.m_name;
    m_type = other.m_type;
    // m_strokes = other.m_strokes;
    if (other.m_node) {
        m_node = Node::Clone(*other.m_node);
    } else {
        m_node.reset();
    }
    m_border_flags = other.m_border_flags;
    m_v_segments = other.m_v_segments;
    if (other.m_capsule) {
        m_capsule = std::make_unique<Capsule>();
        *m_capsule = *other.m_capsule;
    } else {
        m_capsule.reset();
    }
}

void Jianzi::MoveData(Jianzi&& other) {
    m_name = std::move(other.m_name);
    m_type = other.m_type;
    m_node = std::move(other.m_node);
    m_border_flags = other.m_border_flags;
    m_v_segments = other.m_v_segments;
    m_capsule = std::move(other.m_capsule);
}

struct JianziOperator {
    std::function<Jianzi(const Jianzi&, const Jianzi&)> op;
    size_t priority;
};

const std::map<char32_t, JianziOperator> c_jianzi_operators = {
    {U'(', {{}, 0}},
    {U')', {{}, 0}},
    {U'\'', {{}, 0}},

    {U'^', {[](const Jianzi& j1, const Jianzi& j2) { return j1 ^ j2; }, 1}},

    {U'|', {[](const Jianzi& j1, const Jianzi& j2) { return j1 | j2; }, 2}},

    {U'&', {[](const Jianzi& j1, const Jianzi& j2) { return j1 & j2; }, 3}},
    {U'<', {[](const Jianzi& j1, const Jianzi& j2) { return j1 < j2; }, 3}},
    {U'/', {[](const Jianzi& j1, const Jianzi& j2) { return j1 / j2; }, 3}},

    {U'*', {[](const Jianzi& j1, const Jianzi& j2) { return j1 * j2; }, 4}},
};

Jianzi JianziLibrary::Parse(const char* u8_str) const {
    Jianzi result(*m_context);

    using tiny_utf8::string;
    string str(u8_str);

    // 清除所有空格
    auto ws_pos = str.find(U' ');
    while (ws_pos != str.npos) {
        str = str.erase(ws_pos);
        ws_pos = str.find(U' ');
    }

    // 前后添加括号，有利于后续处理
    str = "(" + str + ")";

    std::stack<Jianzi> values;
    std::stack<char32_t> operators;
    size_t alias_expansions = 0;

    // 定义使用运算符op进行运算的函数
    auto Calc = [&values](char32_t op) {
        // std::cout << "Doing operation " << string(op) << std::endl;
        if (values.size() < 2) {
            // 减字已不足两个，算式有误，返回空
            return false;
        }

        auto v2 = values.top();
        values.pop();
        auto v1 = values.top();
        values.pop();

        values.push(c_jianzi_operators.at(op).op(v1, v2));

        return true;
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
                string sub = str.substr(0, sub_pos - str.begin());

                const std::string name = sub.cpp_str();
                if (m_context->m_library.glyphs.find(name) != m_context->m_library.glyphs.end()) {
                    values.push(Jianzi(*m_context, name.c_str()));
                } else {
                    const auto alias = m_context->m_library.aliases.find(name);
                    if (alias == m_context->m_library.aliases.end()) return Jianzi(*m_context);
                    if (++alias_expansions > 1024) {
                        throw std::runtime_error("Alias expansion did not terminate; the library probably contains a cycle.");
                    }
                    str = "(" + string(alias->second) + ")" + str.substr(sub_pos - str.begin());
                    continue;
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
                        if (!Calc(op)) {
                            return Jianzi(*m_context);
                        }
                    }
                }
                if (op != U'(') {
                    // 最终没有停在左括号上，算式有误，返回空
                    return Jianzi(*m_context);
                }
            } else if (c == U'\'') {
                // 引用，寻找配对引用并置换
                size_t curr_quote = sub_pos - str.begin();
                size_t next_quote = str.find(U'\'', curr_quote + 1);
                if (next_quote == str.npos) {
                    // 没有配对，算式有误，返回空
                    return Jianzi(*m_context);
                }
                // 生成算式
                string sub = str.substr(curr_quote + 1, next_quote - 1);
                string rep = ParseNatural(sub.c_str());
                // 替换内容
                str = "(" + rep + ")" + str.substr(next_quote + 1);
                continue;
            } else {
                // 普通算符

                if (operators.empty() ||
                    c_jianzi_operators.at(c).priority > c_jianzi_operators.at(operators.top()).priority) {
                    // 无旧运算符，或新运算符优先级更高
                    operators.push(c);
                } else {
                    // 旧运算符优先级更高或平级
                    // 先将旧运算符出栈进行运算
                    char32_t op = operators.top();
                    operators.pop();
                    // 进行运算
                    if (!Calc(op)) {
                        return Jianzi(*m_context);
                    }
                    // 再将新运算符入栈
                    operators.push(c);
                }
            }
            // 仅保留找到的运算符之后的字符串
            str = str.substr(sub_pos - str.begin() + 1);
        } else {
            break;
        }
    }
    if (str.length() > 0) {
        // 已经没有运算符，剩下的是值
        values.push(Jianzi(*m_context, str.c_str()));
    }

    // 将剩下的运算符算完
    while (!operators.empty()) {
        char32_t op = operators.top();
        operators.pop();
        // 进行运算
        if (!Calc(op)) {
            return Jianzi(*m_context);
        }
    }

    // values里剩下的唯一一个值就是结果
    // assert(values.size() <= 1);
    if (!values.empty()) {
        result = values.top();
    }
    return result;
}

std::string JianziLibrary::ParseNatural(const char* u8_str) const {
    using tiny_utf8::string;
    // 初始化输入
    string input(u8_str);
    // 清除所有空格
    auto ws_pos = input.find(U' ');
    while (ws_pos != input.npos) {
        input = input.erase(ws_pos);
        ws_pos = input.find(U' ');
    }
    size_t input_length = input.length();

    if (input_length == 0) {
        // 无字符
        return std::string();
    } else if (input.front() == U'(' && input.back() == U')') {
        // 内容为公式，原样返回
        return std::string(u8_str);
    }

    // 检索标记
    std::vector<bool> input_marks(input_length, false);
    std::vector<Jianzi::JianziInfo> info_list(input_length);

    auto MarkInput = [&input, &input_marks, &info_list](const std::vector<Jianzi::JianziInfo>& input_list) {
        for (auto& info : input_list) {
            string info_name = info.name;
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
    };

    // 先对别名进行标记
    MarkInput(m_context->m_aliasList);
    // 再标记减字
    MarkInput(m_context->m_jianziList);

    if (input_length == 1 && !input_marks[0]) {
        return std::string();
    } else {
        // 检查是否所有位置均已处理
        for (auto m : input_marks) {
            if (!m) {
                // 输入有问题，返回空
                return std::string();
            }
        }
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
    string ret;

    // 处理连串数字。左闭右开区间
    auto ProcessNumbers = [&](size_t start_pos, size_t end_pos) {
        size_t p = start_pos;
        ret += "(" + info_list[p++].name;
        while (p < end_pos) {
            if (info_list[p].type != JianziType::Number) {
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

    //     if (info_list[start_pos].type == JianziType::LeftAlone)
    //     {
    //         // 单独指法，返回
    //         return p;
    //     }

    //     if (p > end_pos || info_list[p].type != JianziType::Number)
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
    JianziType prev_type;
    while (pos < info_list.size()) {
        if (pos > 0) {
            // 跟着前面的指法，加一个运算符
            if (prev_type == JianziType::Left || prev_type == JianziType::LeftAlone) {
                // if (info_list[pos].type == JianziType::GraceAbove)
                // {
                //     // 把修饰符归于上半，比例会更好看一些
                //     ret += "/";
                // }
                // else
                // {
                ret += "^";
                // }
            }
            // else if (prev_type == JianziType::GraceAbove)
            // {
            //     ret += "^";
            // }
            else if (prev_type == JianziType::GraceSide) {
                ret += "<";
            } else {
                ret += "/";
            }
        }
        prev_type = info_list[pos].type;  // 为下一个字做准备

        ret += info_list[pos++].name;

        // 左手指法
        if (prev_type == JianziType::Left) {
            // 后续是数字，处理数字
            if (pos < info_list.size() && info_list[pos].type == JianziType::Number) {
                ret += "&";
                pos = ProcessNumbers(pos, info_list.size());
            }
        }

        // 主字
        else if (prev_type == JianziType::Main) {
            if (pos < info_list.size() && info_list[pos].type == JianziType::Number) {
                // 后续跟的是数字，全部放入
                ret += "*";
                pos = ProcessNumbers(pos, info_list.size());
            }
        }

        // 复杂减字
        else if (prev_type == JianziType::MainComplex || prev_type == JianziType::MainShu) {
            // 后续还有内容
            if (pos < info_list.size()) {
                // 确定运算符
                string op = "&";
                if (prev_type == JianziType::MainShu) {
                    op = "|";
                }

                // 后续跟的是数字，为弦号
                if (info_list[pos].type == JianziType::Number) {
                    // 对于有竖笔的减字，单个数字应当放在下方。因此需要先计算数字个数
                    size_t number_count = 0;
                    size_t n = pos;
                    while (n < info_list.size()) {
                        if (info_list[n++].type == JianziType::Number) {
                            ++number_count;
                        } else {
                            break;
                        }
                    }
                    if (number_count == 1 && prev_type == JianziType::MainShu) {
                        ret += "/(" + info_list[pos++].name;
                    } else {
                        ret += "*(" + info_list[pos++].name;
                    }
                    // 后续继续跟的是数字，再取一个
                    if (pos < info_list.size() && info_list[pos].type == JianziType::Number) {
                        ret += op + info_list[pos++].name;
                    }
                    ret += ")";
                }
                // 后续跟的是左手指法
                else if (info_list[pos].type == JianziType::Left || info_list[pos].type == JianziType::LeftAlone) {
                    // 多加一个括号便于处理
                    ret += "*((" + info_list[pos++].name;
                    // 后续为左手指法，先统计指法后的数字个数，应当留出一个数字作为弦
                    size_t number_count = 0;
                    size_t n = pos;
                    while (n < info_list.size()) {
                        if (info_list[n++].type == JianziType::Number) {
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
                        (info_list[pos].type == JianziType::Left || info_list[pos].type == JianziType::LeftAlone)) {
                        ret += op + "(" + info_list[pos++].name;
                        // 再来一次：后续为左手指法，先统计指法后的数字个数，应当留出一个数字作为弦
                        size_t number_count = 0;
                        size_t n = pos;
                        while (n < info_list.size()) {
                            if (info_list[n++].type == JianziType::Number) {
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

    return ret.cpp_str();
}

Jianzi Jianzi::operator&(const Jianzi& right) const {
    CheckContext(right);
    Jianzi ret(m_context);
    ret.m_name = "(" + m_name + "&" + right.m_name + ")";
    ret.m_node = std::make_unique<Node>();
    ret.m_node->first = Node::Clone(*m_node);
    ret.m_node->second = Node::Clone(*right.m_node);
    auto& left_node = *ret.m_node->first;
    auto& right_node = *ret.m_node->second;
    const auto before_left = NodeArea(left_node);
    const auto before_right = NodeArea(right_node);
    const auto fixed_left = Normalize(left_node, NormalizeDirection::Right);
    const auto fixed_right = Normalize(right_node, NormalizeDirection::Left);
    const float gap = m_border_flags.r != right.m_border_flags.l
                          ? 0.5f
                          : (m_border_flags.r && right.m_border_flags.l ? 1.0f : 0.25f);
    left_node.outer_border.r = gap;
    right_node.outer_border.l = gap;
    const auto available = std::max(0.0f, 1.0f - fixed_left.l - fixed_right.r);
    const auto left_body = available * 0.5f;
    PlaceBody(left_node, true, 0, left_body, fixed_left.l, 0);
    PlaceBody(right_node, true, fixed_left.l + left_body, available - left_body, 0, fixed_right.r);
    RecordLayerRatio(left_node, before_left);
    RecordLayerRatio(right_node, before_right);
    ret.m_border_flags.t = m_border_flags.t || right.m_border_flags.t;
    ret.m_border_flags.b = m_border_flags.b || right.m_border_flags.b;
    ret.m_border_flags.l = m_border_flags.l;
    ret.m_border_flags.r = right.m_border_flags.r;

    ret.m_v_segments = std::max(m_v_segments, right.m_v_segments);
    return ret;
}

Jianzi Jianzi::operator|(const Jianzi& right) const {
    CheckContext(right);
    Jianzi ret(m_context);
    ret.m_name = "(" + m_name + "|" + right.m_name + ")";
    ret.m_node = std::make_unique<Node>();
    ret.m_node->first = Node::Clone(*m_node);
    ret.m_node->second = Node::Clone(*right.m_node);
    auto& left_node = *ret.m_node->first;
    auto& right_node = *ret.m_node->second;
    const auto before_left = NodeArea(left_node);
    const auto before_right = NodeArea(right_node);
    const auto fixed_left = Normalize(left_node, NormalizeDirection::Right);
    const auto fixed_right = Normalize(right_node, NormalizeDirection::Left);
    left_node.outer_border.r = m_border_flags.r ? 2.0f : 1.0f;
    right_node.outer_border.l = right.m_border_flags.l ? 2.0f : 1.0f;
    const auto available = std::max(0.0f, 1.0f - fixed_left.l - fixed_right.r);
    const auto left_body = available * 0.5f;
    PlaceBody(left_node, true, 0, left_body, fixed_left.l, 0);
    PlaceBody(right_node, true, fixed_left.l + left_body, available - left_body, 0, fixed_right.r);
    RecordLayerRatio(left_node, before_left);
    RecordLayerRatio(right_node, before_right);
    ret.m_border_flags.t = m_border_flags.t || right.m_border_flags.t;
    ret.m_border_flags.b = m_border_flags.b || right.m_border_flags.b;
    ret.m_border_flags.l = m_border_flags.l;
    ret.m_border_flags.r = right.m_border_flags.r;

    ret.m_v_segments = std::max(m_v_segments, right.m_v_segments);
    return ret;
}

Jianzi Jianzi::operator<(const Jianzi& right) const {
    CheckContext(right);
    Jianzi ret(m_context);
    ret.m_name = "(" + m_name + "<" + right.m_name + ")";
    ret.m_node = std::make_unique<Node>();
    ret.m_node->first = Node::Clone(*m_node);
    ret.m_node->second = Node::Clone(*right.m_node);
    auto& left_node = *ret.m_node->first;
    auto& right_node = *ret.m_node->second;
    const auto before_left = NodeArea(left_node);
    const auto before_right = NodeArea(right_node);
    const auto both = static_cast<NormalizeDirection>(static_cast<int>(NormalizeDirection::Left) |
                                                       static_cast<int>(NormalizeDirection::Right));
    const auto fixed_left = Normalize(left_node, both);
    const auto fixed_right = Normalize(right_node, NormalizeDirection::Left);
    const float gap = m_border_flags.r != right.m_border_flags.l
                          ? 0.5f
                          : (m_border_flags.r && right.m_border_flags.l ? 1.0f : 0.25f);
    left_node.outer_border.r = gap;
    right_node.outer_border.l = gap;
    const auto available = std::max(0.0f, 1.0f - fixed_left.l - fixed_right.r);
    const auto left_body = available * 0.3f;
    PlaceBody(left_node, true, 0, left_body, fixed_left.l, 0);
    PlaceBody(right_node, true, fixed_left.l + left_body, available - left_body, 0, fixed_right.r);
    RecordLayerRatio(left_node, before_left);
    RecordLayerRatio(right_node, before_right);
    ret.m_border_flags.t = m_border_flags.t || right.m_border_flags.t;
    ret.m_border_flags.b = m_border_flags.b || right.m_border_flags.b;
    ret.m_border_flags.l = m_border_flags.l;
    ret.m_border_flags.r = right.m_border_flags.r;

    ret.m_v_segments = std::max(m_v_segments, right.m_v_segments);
    return ret;
}

Jianzi Jianzi::operator/(const Jianzi& below) const {
    CheckContext(below);
    Jianzi ret(m_context);
    ret.m_name = "(" + m_name + "/" + below.m_name + ")";
    ret.m_node = std::make_unique<Node>();
    ret.m_node->first = Node::Clone(*m_node);
    ret.m_node->second = Node::Clone(*below.m_node);
    auto& above_node = *ret.m_node->first;
    auto& below_node = *ret.m_node->second;
    const auto before_above = NodeArea(above_node);
    const auto before_below = NodeArea(below_node);
    const bool above_participates = m_v_segments > 0;
    const bool below_participates = below.m_v_segments > 0;
    int extra = 0;
    float gap = 0.25f;
    if (m_border_flags.b != below.m_border_flags.t) {
        gap = 0.5f;
    } else if (m_border_flags.b && below.m_border_flags.t) {
        extra = 1;
        gap = 0;
    }
    const int total = m_v_segments + below.m_v_segments + extra;
    if (!above_participates && below_participates) {
        const auto stroke_height = std::min(1.0f, 2.0f * MaximumStrokeWidth(above_node));
        const auto edge_height =
            std::min(m_context.m_layout.zero_segment_edge_width, std::max(0.0f, 1.0f - stroke_height));
        const auto zero_height = stroke_height + edge_height;
        const auto fixed_below = Normalize(below_node, NormalizeDirection::Top);
        const auto available = std::max(0.0f, 1.0f - zero_height - fixed_below.b);
        const auto unit = available / static_cast<float>(below.m_v_segments + extra);
        PlaceBody(below_node, false, zero_height + unit * extra, unit * below.m_v_segments, 0, fixed_below.b);
        PlaceZeroSegmentCenter(above_node, edge_height + stroke_height * 0.5f);
        const auto ratio = RecordLayerRatio(below_node, before_below);
        above_node.layer_ratios.push_back(ratio);
    } else if (above_participates && !below_participates) {
        const auto stroke_height = std::min(1.0f, 2.0f * MaximumStrokeWidth(below_node));
        const auto edge_height =
            std::min(m_context.m_layout.zero_segment_edge_width, std::max(0.0f, 1.0f - stroke_height));
        const auto zero_height = stroke_height + edge_height;
        const auto fixed_above = Normalize(above_node, NormalizeDirection::Bottom);
        const auto available = std::max(0.0f, 1.0f - zero_height - fixed_above.t);
        const auto unit = available / static_cast<float>(m_v_segments + extra);
        PlaceBody(above_node, false, 0, unit * m_v_segments, fixed_above.t, 0);
        PlaceZeroSegmentCenter(below_node, 1.0f - edge_height - stroke_height * 0.5f);
        const auto ratio = RecordLayerRatio(above_node, before_above);
        below_node.layer_ratios.push_back(ratio);
    } else if (above_participates && below_participates) {
        const auto fixed_above = Normalize(above_node, NormalizeDirection::Bottom);
        const auto fixed_below = Normalize(below_node, NormalizeDirection::Top);
        const auto available = std::max(0.0f, 1.0f - fixed_above.t - fixed_below.b);
        const auto unit = available / static_cast<float>(total);
        PlaceBody(above_node, false, 0, unit * m_v_segments, fixed_above.t, 0);
        PlaceBody(below_node, false, fixed_above.t + unit * (m_v_segments + extra),
                  unit * below.m_v_segments, 0, fixed_below.b);
        RecordLayerRatio(above_node, before_above);
        RecordLayerRatio(below_node, before_below);
    } else if (total > 0) {
        PlaceZeroSegmentCenter(above_node, 0);
        PlaceZeroSegmentCenter(below_node, 1);
        RecordLayerRatio(above_node, before_above);
        RecordLayerRatio(below_node, before_below);
    } else {
        RecordLayerRatio(above_node, before_above);
        RecordLayerRatio(below_node, before_below);
    }
    above_node.outer_border.b = gap;
    below_node.outer_border.t = gap;
    ret.m_border_flags.t = m_border_flags.t;
    ret.m_border_flags.b = below.m_border_flags.b;
    ret.m_border_flags.l = m_border_flags.l || below.m_border_flags.l;
    ret.m_border_flags.r = m_border_flags.r || below.m_border_flags.r;

    ret.m_v_segments = total;
    return ret;
}

Jianzi Jianzi::operator^(const Jianzi& below) const {
    CheckContext(below);
    Jianzi above = *this;
    if (above.m_v_segments > 0 && below.m_v_segments > 0 && above.m_v_segments > below.m_v_segments) {
        above.m_v_segments = below.m_v_segments;
    } else if (above.m_v_segments > 0 && below.m_v_segments > 0 &&
               above.m_v_segments < below.m_v_segments * 0.3f) {
        above.m_v_segments = std::round(below.m_v_segments * 0.3f);
    }
    Jianzi ret = above / below;
    ret.m_name = "(" + m_name + "^" + below.m_name + ")";
    return ret;
}

Jianzi Jianzi::operator*(const Jianzi& content) const {
    CheckContext(content);
    if (!m_capsule) return *this / content;
    Jianzi ret(m_context);
    ret.m_name = "(" + m_name + "*" + content.m_name + ")";
    ret.m_node = std::make_unique<Node>();
    ret.m_node->first = Node::Clone(*m_node);
    ret.m_node->second = Node::Clone(*content.m_node);
    auto& base = *ret.m_node->first;
    auto& inside = *ret.m_node->second;
    const auto before_base = NodeArea(base);
    const auto before_inside = NodeArea(inside);
    const bool zero_content = content.m_v_segments == 0;
    const bool top_conflict = m_capsule->border_flags.t && content.m_border_flags.t;
    const bool bottom_conflict = m_capsule->border_flags.b && content.m_border_flags.b;
    int content_segments = content.m_v_segments;
    if (!zero_content) content_segments += static_cast<int>(top_conflict) + static_cast<int>(bottom_conflict);
    content_segments = std::max(content_segments, m_capsule->v_segments);
    const int final_segments = m_v_segments + content_segments - m_capsule->v_segments;
    const float ratio = m_v_segments > 0 && final_segments > 0
                            ? static_cast<float>(m_v_segments) / static_cast<float>(final_segments)
                            : 1.0f;

    const auto old_tl = m_capsule->tl;
    const auto old_br = m_capsule->br;
    auto host_box = TightBoundingBox(base);
    const float outer_top = std::min(old_tl.y, host_box.y);
    const float outer_bottom = std::max(old_br.y, host_box.y + host_box.h);
    const float envelope = outer_bottom - outer_top;
    const auto to_unit = [&](float value) { return envelope > 1e-9f ? (value - outer_top) / envelope : 0.0f; };
    const auto from_unit = [&](float value) { return outer_top + value * envelope; };
    Point2f tl{old_tl.x, from_unit(to_unit(old_tl.y) * ratio)};
    Point2f br{old_br.x, from_unit(1.0f - (1.0f - to_unit(old_br.y)) * ratio)};
    const auto map_interval = [](float value, float from_start, float from_end, float to_start, float to_end) {
        return std::abs(from_end - from_start) > 1e-9f
                   ? to_start + (value - from_start) / (from_end - from_start) * (to_end - to_start)
                   : value;
    };
    const auto capsule_weight =
        m_context.m_layout.capsule_weight_area * std::sqrt(ratio) + m_context.m_layout.capsule_weight_base;
    for (auto& stroke : base.strokes) {
        stroke.width *= capsule_weight;
        for (auto& vertex : stroke.vertice) {
            if (vertex.region == VertexRegion::Top) {
                vertex.pt.y = from_unit(to_unit(vertex.pt.y) * ratio);
            } else if (vertex.region == VertexRegion::Bottom) {
                vertex.pt.y = from_unit(1.0f - (1.0f - to_unit(vertex.pt.y)) * ratio);
            } else if (vertex.region == VertexRegion::Medium) {
                vertex.pt.y = map_interval(vertex.pt.y, old_tl.y, old_br.y, tl.y, br.y);
            }
        }
    }

    const auto capsule_height = std::max(0.0f, br.y - tl.y);
    const auto segment_height = content_segments > 0 ? capsule_height / content_segments : 0.0f;
    if (top_conflict && !zero_content) tl.y += segment_height;
    if (bottom_conflict && !zero_content) br.y -= segment_height;
    if (br.y < tl.y) throw std::runtime_error("Capsule has no vertical space after border avoidance.");

    if (!zero_content && (m_capsule->border_flags.t || tl.y > 0)) Normalize(inside, NormalizeDirection::Top);
    if (!zero_content && (m_capsule->border_flags.b || br.y < 1)) Normalize(inside, NormalizeDirection::Bottom);
    if (m_capsule->border_flags.l || tl.x > 0) Normalize(inside, NormalizeDirection::Left);
    if (m_capsule->border_flags.r || br.x < 1) Normalize(inside, NormalizeDirection::Right);
    inside.outer_border.t = zero_content ? 0 : (m_capsule->border_flags.t ? (content.m_border_flags.t ? 0 : 1) : 0);
    inside.outer_border.b = zero_content ? 0 : (m_capsule->border_flags.b ? (content.m_border_flags.b ? 0 : 1) : 0);
    inside.outer_border.l = m_capsule->border_flags.l ? (content.m_border_flags.l ? 2 : 1) : 0;
    inside.outer_border.r = m_capsule->border_flags.r ? (content.m_border_flags.r ? 2 : 1) : 0;
    if (zero_content) {
        inside.box = {tl.x, 0, br.x - tl.x, 1};
        PlaceZeroSegmentCenter(inside, (tl.y + br.y) * 0.5f);
    } else {
        inside.box = {tl.x, tl.y, br.x - tl.x, br.y - tl.y};
    }
    RecordLayerRatio(base, before_base);
    RecordLayerRatio(inside, before_inside);

    ret.m_border_flags = m_border_flags;
    if (tl.x == 0) ret.m_border_flags.l |= content.m_border_flags.l;
    if (std::abs(br.x - 1.0f) < FLT_EPSILON) ret.m_border_flags.r |= content.m_border_flags.r;
    if (tl.y == 0) ret.m_border_flags.t |= content.m_border_flags.t;
    if (std::abs(br.y - 1.0f) < FLT_EPSILON) ret.m_border_flags.b |= content.m_border_flags.b;
    ret.m_v_segments = final_segments;
    return ret;
}

std::vector<PathData> qin::Jianzi::RenderPath() const {
    if (!m_node) return {};
    std::vector<Stroke> strokes;
    CollectStrokes(*m_node, BoundingBox{}, 1.0f, strokes);
    auto paths = m_context.m_renderer->Render(strokes);
    for (auto& command : paths) {
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

Jianzi::BorderFlags Jianzi::GetBorderFlags() const { return m_border_flags; }

int Jianzi::GetSegments() const { return m_v_segments; }

BoundingBox Jianzi::TightBoundingBox(const Node& node) const {
    std::vector<Stroke> strokes;
    CollectStrokes(node, BoundingBox(), 1.0f, strokes);

    // 获取坐标最大最小值
    float x_min = std::numeric_limits<float>::max(), x_max = std::numeric_limits<float>::lowest();
    float y_min = std::numeric_limits<float>::max(), y_max = std::numeric_limits<float>::lowest();
    bool has_point = false;
    for (auto& s : strokes) {
        for (auto& v : s.vertice) {
            has_point = true;
            if (v.pt.x < x_min) {
                x_min = v.pt.x;
            }
            if (v.pt.x > x_max) {
                x_max = v.pt.x;
            }
            if (v.pt.y < y_min) {
                y_min = v.pt.y;
            }
            if (v.pt.y > y_max) {
                y_max = v.pt.y;
            }
        }
    }

    // 生成包围盒
    BoundingBox box;
    if (has_point) {
        box.x = x_min;
        box.y = y_min;
        box.w = x_min < x_max ? x_max - x_min : 1.0f;
        box.h = y_min < y_max ? y_max - y_min : 1.0f;
    }

    return box;
}

float Jianzi::LayerWeight(const Node& node) const {
    float result = 1.0f;
    for (const auto ratio : node.layer_ratios) {
        result *= m_context.m_layout.weight_area * std::sqrt(std::max(0.0f, ratio)) +
                  m_context.m_layout.weight_base;
    }
    return result;
}

float Jianzi::MaximumStrokeWidth(const Node& node, float inherited_weight) const {
    const auto weight = inherited_weight * LayerWeight(node);
    float result = 0;
    for (const auto& stroke : node.strokes) result = std::max(result, stroke.width * weight);
    if (node.first) result = std::max(result, MaximumStrokeWidth(*node.first, weight));
    if (node.second) result = std::max(result, MaximumStrokeWidth(*node.second, weight));
    return result;
}

float Jianzi::NodeArea(const Node& node) { return std::abs(node.box.w * node.box.h); }

float Jianzi::RecordLayerRatio(Node& node, float before_area) {
    const auto ratio = before_area > 1e-12f ? NodeArea(node) / before_area : 1.0f;
    node.layer_ratios.push_back(std::isfinite(ratio) && ratio >= 0 ? ratio : 1.0f);
    return node.layer_ratios.back();
}

void Jianzi::PlaceBody(Node& node, bool horizontal, float start, float body_size, float fixed_start,
                       float fixed_end) {
    const auto source_body = std::max(1e-9f, 1.0f - fixed_start - fixed_end);
    const auto scale = std::max(0.0f, body_size) / source_body;
    const auto offset = start + fixed_start - scale * fixed_start;
    if (horizontal) {
        node.box.x = offset + scale * node.box.x;
        node.box.w *= scale;
    } else {
        node.box.y = offset + scale * node.box.y;
        node.box.h *= scale;
    }
}

float Jianzi::SkeletonVerticalCenter(const Node& node) {
    float minimum = std::numeric_limits<float>::max();
    float maximum = std::numeric_limits<float>::lowest();
    std::function<void(const Node&, const BoundingBox&)> collect = [&](const Node& current,
                                                                       const BoundingBox& parent) {
        const auto box = parent * current.box;
        for (const auto& stroke : current.strokes) {
            for (const auto& vertex : stroke.vertice) {
                const auto point = box * vertex.pt;
                minimum = std::min(minimum, point.y);
                maximum = std::max(maximum, point.y);
            }
        }
        if (current.first) collect(*current.first, box);
        if (current.second) collect(*current.second, box);
    };
    collect(node, BoundingBox{});
    return minimum <= maximum ? (minimum + maximum) * 0.5f : 0.5f;
}

void Jianzi::PlaceZeroSegmentCenter(Node& node, float target) {
    node.box.y += target - SkeletonVerticalCenter(node);
}

void Jianzi::CollectStrokes(const Node& node, const BoundingBox& parent_box, float inherited_weight,
                            std::vector<Stroke>& strokes) const {
    BoundingBox box = parent_box * node.box;
    const auto weight = inherited_weight * LayerWeight(node);
    box.x += node.outer_border.l * m_context.m_layout.border_width * weight;
    box.y += node.outer_border.t * m_context.m_layout.border_width * weight;
    box.w -= (node.outer_border.l + node.outer_border.r) * m_context.m_layout.border_width * weight;
    box.h -= (node.outer_border.t + node.outer_border.b) * m_context.m_layout.border_width * weight;
    for (auto& s : node.strokes) {
        Stroke ns = s;
        ns.width *= weight;
        for (auto& v : ns.vertice) {
            v.pt = box * v.pt;
        }
        strokes.push_back(ns);
    }
    if (node.first) CollectStrokes(*node.first, box, weight, strokes);
    if (node.second) CollectStrokes(*node.second, box, weight, strokes);
}

Jianzi::FixedInsets Jianzi::Normalize(Node& node, NormalizeDirection dir) const {
    auto tight_box = TightBoundingBox(node);
    float xa = tight_box.x;
    float xb = tight_box.x + tight_box.w;
    float ya = tight_box.y;
    float yb = tight_box.y + tight_box.h;

    const auto directions = static_cast<int>(dir);
    const bool top = directions & static_cast<int>(NormalizeDirection::Top);
    const bool bottom = directions & static_cast<int>(NormalizeDirection::Bottom);
    const bool left = directions & static_cast<int>(NormalizeDirection::Left);
    const bool right = directions & static_cast<int>(NormalizeDirection::Right);
    const bool both_x = left && right;
    const bool both_y = top && bottom;
    FixedInsets fixed;
    if (bottom && !both_y) fixed.t = std::max(0.0f, ya);
    if (top && !both_y) fixed.b = std::max(0.0f, 1.0f - yb);
    if (right && !both_x) fixed.l = std::max(0.0f, xa);
    if (left && !both_x) fixed.r = std::max(0.0f, 1.0f - xb);
    BoundingBox norm_box;
    if (top && yb - ya > 1e-9f) {
        BoundingBox box;
        // 向上归一化：h*ya+y=0, h*yb+y=yb
        box.h = yb / (yb - ya);
        box.y = -box.h * ya;

        norm_box = norm_box * box;
    }
    if (bottom && yb - ya > 1e-9f) {
        BoundingBox box;
        // 向下归一化：h*ya+y=ya, h*yb+y=1
        box.h = (1.0f - ya) / (yb - ya);
        box.y = 1.0f - box.h * yb;

        norm_box = norm_box * box;
    }
    if (left && xb - xa > 1e-9f) {
        BoundingBox box;
        // 向上归一化：w*xa+x=0, w*xb+x=xb
        box.w = xb / (xb - xa);
        box.x = -box.w * xa;

        norm_box = norm_box * box;
    }
    if (right && xb - xa > 1e-9f) {
        BoundingBox box;
        // 向下归一化：w*xa+x=xa, w*xb+x=1
        box.w = (1.0f - xa) / (xb - xa);
        box.x = 1.0f - box.w * xb;

        norm_box = norm_box * box;
    }

    NormalizeFunc(node, norm_box);
    return fixed;
}

void Jianzi::NormalizeFunc(Node& node, const BoundingBox& box) {
    for (auto& s : node.strokes) {
        // 笔画缩放
        for (auto& v : s.vertice) {
            v.pt = box * v.pt;
        }
    }

    if (node.first) node.first->box = box * node.first->box;
    if (node.second) node.second->box = box * node.second->box;
}

std::unique_ptr<Jianzi::Node> Jianzi::Node::Clone(const Jianzi::Node& node) {
    std::unique_ptr<Node> ret = std::make_unique<Node>();
    ret->box = node.box;
    ret->strokes = node.strokes;
    ret->outer_border = node.outer_border;
    ret->layer_ratios = node.layer_ratios;
    if (node.first) {
        ret->first = Clone(*node.first);
    }
    if (node.second) {
        ret->second = Clone(*node.second);
    }
    return std::move(ret);
}

}  // namespace qin
