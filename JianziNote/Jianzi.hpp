// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <memory>
#include <string>
#include <vector>

#include "BoundingBox.hpp"
#include "JianziDefines.hpp"
#include "JianziStyler.hpp"

namespace qin {
// 减字统一以1为单位，上下左右各留半个笔画宽度
class Jianzi {
 public:
  Jianzi() = default;

  Jianzi(const Jianzi &other);
  Jianzi &operator=(const Jianzi &other);

  Jianzi(Jianzi &&other) = default;
  Jianzi &operator=(Jianzi &&other) = default;

  Jianzi(const char *u8_ch);

  // 初始化数据库
  static void OpenDb(const char *file);

  // 根据公式生成减字。
  // 如“大九挑七”，可写成“(大&九)/(挑*七)”,
  // “散一大七急撮”可写成“急/(撮((大/七)|(散/一2))”
  static Jianzi Parse(const char *u8_str);
  // 根据自然字串生成公式
  static std::string ParseNatural(const char *u8_str);

  // 将两个减字横向合并，均分左右
  Jianzi operator&(const Jianzi &right) const;
  // 将两个减字横向合并，中间多留一个笔画宽度
  //（用于给“撮”“剌”等中间有一竖的减字留空间）
  Jianzi operator|(const Jianzi &right) const;
  // 将两个减字横向合并，但左侧只留30%
  Jianzi operator<(const Jianzi &right) const;
  // 将两个减字纵向合并
  Jianzi operator/(const Jianzi &below) const;
  // 将两个减字纵向合并，但限制上半部不超过一半
  Jianzi operator^(const Jianzi &below) const;
  // 对于普通减字，此运算与纵向合并相同
  // 对于带空间的减字，则是将指定减字放入填充空间内
  Jianzi operator*(const Jianzi &content) const;

  std::vector<JianziStyler::PathData> RenderPath(
      const JianziStyler &styler) const;

  // 边界避让标记
  struct BorderFlags {
    bool t = false;  // 顶部
    bool b = false;  // 底部
    bool l = false;  // 左边
    bool r = false;  // 右边
  };

  // 获取名称。减字名称会根据运算变化
  const char *GetName() const;

  // 获取边界避让标记
  BorderFlags GetBorderFlags() const;
  // 获取纵向间隔数
  int GetSegments() const;

 protected:
  // 数据库
  static std::unique_ptr<SQLite::Database> s_db;

  std::string m_name;  // 减字名称
  JianziType m_type = JianziType::Other;

  // 每个最小减字的包围框
  struct Node {
    BoundingBox box;
    std::vector<Stroke> strokes;  // 包围框内的笔画
    struct {
      float t = 0;
      float b = 0;
      float l = 0;
      float r = 0;
    } outer_border;  // 外部边界状态

    std::unique_ptr<Node> first;
    std::unique_ptr<Node> second;

    static std::unique_ptr<Node> Clone(const Node &node);
  };

  // std::vector<Stroke> m_strokes; // 减字所包含的所有端点
  std::unique_ptr<Node> m_node = nullptr;  // 节点组成减字

  BorderFlags m_border_flags;  // 避让标记
  int m_v_segments = 1;        // 纵向间隔数

  struct Capsule {
    Point2f tl;                // 左上角坐标
    Point2f br;                // 右下角坐标
    BorderFlags border_flags;  // 避让标记
    int v_segments = 1;        // 填充区的纵向间隔数
  };
  std::unique_ptr<Capsule> m_capsule;  // 用于填充另一减字的空间，简称填充区

  // 归一化方向
  enum class NormalizeDirection {
    Top = 8,
    Bottom = 4,
    Left = 2,
    Right = 1,
  };

  // 减字名称与类型
  struct JianziInfo {
    std::string name;
    JianziType type;
  };
  static std::vector<JianziInfo> s_alias_list;
  static std::vector<JianziInfo> s_jianzi_list;

 protected:
  // 递归获取所有笔画
  static void CollectStrokes(const Node &node, const BoundingBox &parent_box,
                             float stroke_width, std::vector<Stroke> &strokes);
  static BoundingBox TightBoundingBox(const Node &node);
  static void Normalize(Node &node, NormalizeDirection dir);
  static void NormalizeFunc(Node &node, const BoundingBox &box);
};

}  // namespace qin
