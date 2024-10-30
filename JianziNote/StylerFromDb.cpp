// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "StylerFromDb.hpp"

#include <SQLiteCpp/SQLiteCpp.h>

#include <cmath>
#include <iostream>
#include <magic_enum.hpp>
#include <memory>

#include "BoundingBox.hpp"


#define JE(key) "json_extract(value, '$." #key "') as " #key

namespace qin {

StylerFromDb::StylerFromDb(float stroke_width) : c_stroke_width(stroke_width) {}
StylerFromDb::~StylerFromDb() { m_font_reader.ReleaseFont(); }

std::vector<std::string> StylerFromDb::GetStylerList(std::string db_file) {
  std::unique_ptr<SQLite::Database> db;
  try {
    db = std::make_unique<SQLite::Database>(db_file);
  } catch (SQLite::Exception e) {
    std::cout << e.what() << std::endl;
    throw e;
  }

  std::vector<std::string> ret;
  try {
    // 查询已有的样式名称
    SQLite::Statement styler_query(*db, "select name from styler_list");
    while (styler_query.executeStep()) {
      auto name = styler_query.getColumn("name").getString();
      if (!name.empty()) {
        ret.push_back(name);
      }
    }
  } catch (SQLite::Exception e) {
    std::cout << "Error querying stylers: " << e.what() << std::endl;
    throw e;
  }

  return ret;
}

void StylerFromDb::Load(std::string db_file, std::string styler_name) {
  std::unique_ptr<SQLite::Database> db;
  try {
    db = std::make_unique<SQLite::Database>(db_file);
  } catch (SQLite::Exception e) {
    std::cout << e.what() << std::endl;
    throw e;
  }

  // 根据名称定位表的名称
  std::string table_name;
  try {
    // 查询已有的样式名称
    SQLite::Statement styler_query(
        *db,
        "select target from styler_list where name = \"" + styler_name + "\"");
    if (styler_query.executeStep()) {
      table_name = styler_query.getColumn("target").getString();
    }
  } catch (SQLite::Exception e) {
    std::cout << e.what() << std::endl;
    throw e;
  }

  if (table_name.empty()) {
    return;
  }

  // 根据表格名称加载数据
  try {
    SQLite::Statement vertex_query(*db, "select * from " + table_name);
    while (vertex_query.executeStep()) {
      VertexType type = magic_enum::enum_cast<VertexType>(
                            vertex_query.getColumn("type").getString())
                            .value_or(VertexType::None);
      VertexDesc desc;
      desc.pre_rotate = vertex_query.getColumn("pre_rotate").getDouble();
      desc.dir = magic_enum::enum_cast<RotateDir>(
                     vertex_query.getColumn("dir").getString())
                     .value_or(RotateDir::Next);
      // 解析PathGroup的json字串
      std::string group_str[2];
      group_str[0] = vertex_query.getColumn("forward").getString();
      group_str[1] = vertex_query.getColumn("backward").getString();
      std::vector<PathGroup> groups[2];

      SQLite::Statement group_query(
          *db, "select " JE(key) ", " JE(offsets) " from json_each( ? )");
      for (int i = 0; i < 2; ++i) {
        if (group_str[i].empty()) {
          continue;
        }
        group_query.reset();
        group_query.bind(1, group_str[i]);
        while (group_query.executeStep()) {
          // 新建一个group
          PathGroup group;
          group.key = magic_enum::enum_cast<PathKey>(
                          group_query.getColumn("key").getString())
                          .value_or(PathKey::MoveTo);
          // 解析PathOffset的json字串
          std::string offsets = group_query.getColumn("offsets");
          SQLite::Statement offset_query(
              *db,
              "select " JE(x_rel) ", " JE(x_abs) ", " JE(y_rel) ", " JE(
                  y_abs) ", " JE(x_len) ", " JE(y_len) " from json_each('" +
                  offsets + "')");
          while (offset_query.executeStep()) {
            // 新建一个offset
            PathOffset offset;
            offset.x_rel = offset_query.getColumn("x_rel").getDouble();
            offset.x_abs = offset_query.getColumn("x_abs").getDouble();
            offset.y_rel = offset_query.getColumn("y_rel").getDouble();
            offset.y_abs = offset_query.getColumn("y_abs").getDouble();
            offset.x_len = offset_query.getColumn("x_len").getDouble();
            offset.y_len = offset_query.getColumn("y_len").getDouble();
            // 记得把这个offset存进group里
            group.offsets.push_back(offset);
          }
          // 记得把这个group存进desc里
          groups[i].push_back(group);
        }
      }
      desc.forward = groups[0];
      desc.backward = groups[1];
      // 记得把这个desc存进map里
      m_desc_map[type] = desc;
    }
  } catch (SQLite::Exception e) {
    std::cout << e.what() << std::endl;
    throw e;
  }

  // 从数据库中加载字库
  try {
    SQLite::Statement font_query(
        *db, "select name,data,length(data) from font_data");
    if (font_query.executeStep()) {
      const char *font_data =
          (const char *)font_query.getColumn("data").getBlob();
      size_t length = font_query.getColumn("length(data)").getUInt();
      if (font_data != nullptr && length > 0) {
        // 拷贝数据
        m_font_data = std::make_unique<char[]>(length);
        memcpy(m_font_data.get(), font_data, length);
        // 加载字库
        m_font_reader.LoadFont(m_font_data.get(), length);
      }
    }
  } catch (SQLite::Exception e) {
    std::cout << e.what() << std::endl;
    throw e;
  }
}

const float c_pi = 3.14159f;
const float c_half_pi = c_pi * 0.5f;

// 定义便于计算的函数

// 求pt2相对pt1的旋转角度，逆时针为正，单位弧度
float RelativeAngle(const Point2f &pt1, const Point2f &pt2) {
  auto rel = pt2 - pt1;
  return atan2(rel.y, rel.x);
}

float RelativeLength(const Point2f &pt1, const Point2f &pt2) {
  auto rel = pt2 - pt1;
  return std::sqrt(rel.x * rel.x + rel.y * rel.y);
}

// 将某个点绕另一个点旋转指定角度。逆时针为正，单位弧度
Point2f RotatePoint(const Point2f &pt, const Point2f &base, float angle) {
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

std::vector<JianziStyler::PathData> StylerFromDb::RenderChar(
    size_t codepoint) const {
  auto path_data = m_font_reader.GetPath(codepoint);

  // 进行一个放的缩
  float border = 0.00f;
  // h设为负数以进行翻转。TODO: 基线值0.12
  BoundingBox box = BoundingBox{0, 0.88, 0.001f, -0.001f};
  for (auto &p : path_data) {
    for (auto &pt : p.pts) {
      pt = box * pt;
    }
  }
  return path_data;
}

std::vector<JianziStyler::PathData> StylerFromDb::RenderPath(
    const std::vector<Stroke> &strokes) const {
  std::vector<PathData> ret;

  for (auto &s : strokes) {
    if (s.vertice.size() == 1) {
      // 只有一个顶点，为了处理angle和length的问题，添加一个占位
      auto vertice = s.vertice;
      StrokeVertex vert;
      vert.type = VertexType::None;
      vert.pt = Point2f{0, 0};
      vert.belong = VertexBelong::Top;
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

void StylerFromDb::ProcessVertex(const StrokeVertex &v, float weight,
                                 Direction dir,
                                 std::vector<PathData> &path) const {
  // 根据方向加载描述
  VertexDesc desc;
  try {
    desc = m_desc_map.at(v.type);
  } catch (...) {
    // 没有数据，返回
    return;
  }
  auto &groups = (dir == Direction::Forward) ? desc.forward : desc.backward;
  // 无点可算
  if (groups.empty()) {
    return;
  }

  // 根据前后计算角度
  auto angle = (desc.dir == RotateDir::Prev) ? RelativeAngle((&v)[-1].pt, v.pt)
                                             : RelativeAngle(v.pt, (&v)[1].pt);
  auto length = (desc.dir == RotateDir::Prev)
                    ? RelativeLength((&v)[-1].pt, v.pt)
                    : RelativeLength(v.pt, (&v)[1].pt);
  // 根据旋转模式添加预旋转角度（注意方向是反的，汉字大多数笔画朝顺时针旋转）
  if (desc.pre_rotate > 0) {
    angle -= desc.pre_rotate * 3.1415926 / 180.0;
  }

  // 逐点生成
  for (auto &group : groups) {
    // 根据path目前的点情况来决定下一个点加到哪里
    if (!path.empty() && ((path.rbegin()->key == PathKey::CubicTo &&
                           path.rbegin()->pts.size() < 3) ||
                          (path.rbegin()->key == PathKey::QuadTo &&
                           path.rbegin()->pts.size() < 2))) {
      // 前一个点是CubicTo且未填满，当前点应当加入现有组中
    } else {
      path.push_back(PathData{group.key, {}});
    }

    for (auto &offset : group.offsets) {
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
