// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "StylerFromDb.hpp"

#include <cmath>
#include <fstream>
#include <magic_enum/magic_enum.hpp>
#include <stdexcept>

#include "BoundingBox.hpp"

namespace qin {

StylerFromDb::StylerFromDb(float stroke_width) : c_stroke_width(stroke_width) {}
StylerFromDb::~StylerFromDb() { m_font_reader.ReleaseFont(); }

void StylerFromDb::LoadCbor(const nlohmann::json& styler, const std::filesystem::path& font_file) {
    m_desc_map.clear();
    for (const auto& item : styler.at("vertices")) {
        VertexDesc desc;
        desc.type = item.at("type").get<std::string>();
        desc.pre_rotate = item.at("pre_rotate").get<float>();
        desc.dir = magic_enum::enum_cast<RotateDir>(item.at("direction").get<std::string>()).value_or(RotateDir::Next);
        const auto load_groups = [](const nlohmann::json& input) {
            std::vector<PathGroup> groups;
            for (const auto& group_data : input) {
                PathGroup group;
                group.key =
                    magic_enum::enum_cast<PathKey>(group_data.at("key").get<std::string>()).value_or(PathKey::MoveTo);
                for (const auto& offset_data : group_data.at("offsets")) {
                    group.offsets.push_back({offset_data.at("x_rel").get<float>(), offset_data.at("x_abs").get<float>(),
                                             offset_data.at("y_rel").get<float>(), offset_data.at("y_abs").get<float>(),
                                             offset_data.at("x_len").get<float>(),
                                             offset_data.at("y_len").get<float>()});
                }
                groups.push_back(std::move(group));
            }
            return groups;
        };
        desc.forward = load_groups(item.at("forward"));
        desc.backward = load_groups(item.at("backward"));
        const auto [it, inserted] = m_desc_map.emplace(desc.type, std::move(desc));
        if (!inserted) {
            throw std::runtime_error("Duplicate VertexDesc type: " + it->first);
        }
    }

    std::ifstream input(font_file, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Unable to open library font: " + font_file.string());
    }
    const auto length = std::filesystem::file_size(font_file);
    m_font_data = std::make_unique<char[]>(length);
    input.read(m_font_data.get(), static_cast<std::streamsize>(length));
    if (!input) {
        throw std::runtime_error("Unable to read library font: " + font_file.string());
    }
    m_font_reader.LoadFont(m_font_data.get(), length);
}

const float c_pi = 3.14159f;
const float c_half_pi = c_pi * 0.5f;

// 定义便于计算的函数

// 求pt2相对pt1的旋转角度，逆时针为正，单位弧度
float RelativeAngle(const Point2f& pt1, const Point2f& pt2) {
    auto rel = pt2 - pt1;
    return atan2(rel.y, rel.x);
}

float RelativeLength(const Point2f& pt1, const Point2f& pt2) {
    auto rel = pt2 - pt1;
    return std::sqrt(rel.x * rel.x + rel.y * rel.y);
}

// 将某个点绕另一个点旋转指定角度。逆时针为正，单位弧度
Point2f RotatePoint(const Point2f& pt, const Point2f& base, float angle) {
    Point2f ret;

    // 求相对位置
    Point2f rel = pt - base;
    // 二维平面旋转
    ret.x = rel.x * cos(angle) - rel.y * sin(angle);
    ret.y = rel.x * sin(angle) + rel.y * cos(angle);
    // 归位
    ret = ret + base;

    return ret;
}

std::vector<PathData> StylerFromDb::RenderChar(size_t codepoint) const {
    auto path_data = m_font_reader.GetPath(codepoint);

    // 进行一个放的缩
    float border = 0.00f;
    // h设为负数以进行翻转。TODO: 基线值0.12
    BoundingBox box = BoundingBox{0, 0.88, 0.001f, -0.001f};
    for (auto& p : path_data) {
        for (auto& pt : p.pts) {
            pt = box * pt;
        }
    }
    return path_data;
}

std::vector<PathData> StylerFromDb::RenderPath(const std::vector<Stroke>& strokes) const {
    std::vector<PathData> ret;

    for (auto& s : strokes) {
        if (s.vertice.size() == 1) {
            // 只有一个顶点，为了处理angle和length的问题，添加一个占位
            auto vertice = s.vertice;
            StrokeVertex vert;
            vert.type = "None";
            vert.pt = Point2f{0, 0};
            vert.region = VertexRegion::Top;
            vertice.push_back(vert);

            ProcessVertex(vertice[0], s.weight, Direction::Forward, ret);
            ProcessVertex(vertice[0], s.weight, Direction::Backward, ret);
        } else {
            // 正向循环处理每个顶点
            for (auto iter = s.vertice.begin(); iter != s.vertice.end(); ++iter) {
                ProcessVertex(*iter, s.weight, Direction::Forward, ret);
            }

            // 再反向循环处理每个顶点
            for (auto iter = s.vertice.rbegin(); iter != s.vertice.rend(); ++iter) {
                ProcessVertex(*iter, s.weight, Direction::Backward, ret);
            }
        }
        // 记得关闭路径
        ret.push_back(PathData{PathKey::Close, {}});
    }

    return ret;
}

float StylerFromDb::GetStrokeWidth() const { return c_stroke_width; }

void StylerFromDb::ProcessVertex(const StrokeVertex& v, float weight, Direction dir,
                                 std::vector<PathData>& path) const {
    // 根据方向加载描述
    const auto it = m_desc_map.find(v.type);
    if (it == m_desc_map.end()) {
        return;
    }
    const VertexDesc& desc = it->second;
    auto& groups = (dir == Direction::Forward) ? desc.forward : desc.backward;
    // 无点可算
    if (groups.empty()) {
        return;
    }

    // 根据前后计算角度
    auto angle = (desc.dir == RotateDir::Prev) ? RelativeAngle((&v)[-1].pt, v.pt) : RelativeAngle(v.pt, (&v)[1].pt);
    auto length = (desc.dir == RotateDir::Prev) ? RelativeLength((&v)[-1].pt, v.pt) : RelativeLength(v.pt, (&v)[1].pt);
    // 根据旋转模式添加预旋转角度（注意方向是反的，汉字大多数笔画朝顺时针旋转）
    if (desc.pre_rotate > 0) {
        angle -= desc.pre_rotate * 3.1415926 / 180.0;
    }

    // 逐点生成
    for (auto& group : groups) {
        // 根据path目前的点情况来决定下一个点加到哪里
        if (!path.empty() && ((path.rbegin()->key == PathKey::CubicTo && path.rbegin()->pts.size() < 3) ||
                              (path.rbegin()->key == PathKey::QuadTo && path.rbegin()->pts.size() < 2))) {
            // 前一个点是CubicTo且未填满，当前点应当加入现有组中
        } else {
            path.push_back(PathData{group.key, {}});
        }

        for (auto& offset : group.offsets) {
            Point2f pt = v.pt;
            // 计算偏移量
            if (desc.pre_rotate >= 0) {
                // 正常旋转
                pt.x += offset.x_rel * c_stroke_width * weight;
                pt.x += offset.x_abs;
                pt.x += offset.x_len * length;
                pt.y += offset.y_rel * c_stroke_width * weight;
                pt.y += offset.y_abs;
                pt.y += offset.y_len * length;
                pt = RotatePoint(pt, v.pt, angle);
            } else if (desc.pre_rotate == -1) {
                pt.x += offset.x_rel * c_stroke_width * weight;
                pt.x += offset.x_abs;
                pt.x += offset.x_len * length;
                // 保持两头竖直
                pt.y += offset.y_rel * c_stroke_width * weight / cos(angle);
                pt.y += offset.y_abs / cos(angle);
                pt.y += offset.y_len * length / cos(angle);
            } else if (desc.pre_rotate == -2) {
                // 保持两头平行
                angle -= 3.1415626 / 2;
                pt.x += offset.x_rel * c_stroke_width * weight / cos(angle);
                pt.x += offset.x_abs / cos(angle);
                pt.x += offset.x_len * length / cos(angle);

                pt.y += offset.y_rel * c_stroke_width * weight;
                pt.y += offset.y_abs;
                pt.y += offset.y_len * length;
            } else {
                // 不旋转
                pt.x += offset.x_rel * c_stroke_width * weight;
                pt.x += offset.x_abs;
                pt.x += offset.x_len * length;
                pt.y += offset.y_rel * c_stroke_width * weight;
                pt.y += offset.y_abs;
                pt.y += offset.y_len * length;
            }
            // ControlPoint会添加到前一个顶点里，
            // 否则加到新添加的点里
            path.rbegin()->pts.push_back(pt);
        }
    }
}

}  // namespace qin
