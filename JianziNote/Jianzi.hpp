// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "BoundingBox.hpp"
#include "Stroke.hpp"
#include "VectorPath.hpp"

namespace qin {
class StrokeDescRenderer;
class JianziLibrary;
struct JianziContext;

enum class JianziStatus {
  Empty,
  Renderable,
  Fallback,
  Missing,
};

// 减字统一以1为单位，上下左右各留半个笔画宽度
class Jianzi {
 public:
  Jianzi(const Jianzi &other);
  Jianzi &operator=(const Jianzi &other);

  Jianzi(Jianzi &&other) noexcept;
  Jianzi &operator=(Jianzi &&other);

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

  // 使用字形包内置的样式渲染。
  std::vector<PathData> RenderPath() const;

  // 边界避让标记
  struct BorderFlags {
    bool t = false;  // 顶部
    bool b = false;  // 底部
    bool l = false;  // 左边
    bool r = false;  // 右边
  };

  // 获取名称。减字名称会根据运算变化
  const char *GetName() const;

  JianziStatus GetStatus() const noexcept;
  std::string_view GetFallbackName() const noexcept;
  const std::vector<std::string> &GetMissingNames() const noexcept;

  // 获取边界避让标记
  BorderFlags GetBorderFlags() const;
  // 获取纵向间隔数
  int GetSegments() const;

 protected:
  struct LibraryData;

  struct Layout {
    float units_per_em = 1000.0f;
    float baseline_y = 880.0f;
    float normalization_scale = 1.0f;
    float normalization_tx = 0.0f;
    float normalization_ty = 0.0f;
    float weight_area = 0.7f;
    float weight_base = 0.3f;
    float border_width = 0.1f;
    float zero_segment_edge_width = 0.1f;
    float capsule_weight_area = 0.5f;
    float capsule_weight_base = 0.5f;
  };
  std::string m_name;  // 减字名称
  JianziStatus m_status = JianziStatus::Empty;
  std::vector<std::string> m_missing_names;

  // 归一化方向
  enum class NormalizeDirection {
    Top = 8,
    Bottom = 4,
    Left = 2,
    Right = 1,
  };

  struct FixedInsets {
    float t = 0;
    float b = 0;
    float l = 0;
    float r = 0;
  };

  // 每个最小减字的包围框
  struct Node {
    Node() = default;
    Node(const Node &other);
    Node &operator=(const Node &other);
    Node(Node &&other) noexcept = default;
    Node &operator=(Node &&other) noexcept = default;

    BoundingBox box;
    std::vector<Stroke> strokes;  // 包围框内的笔画
    struct {
      float t = 0;
      float b = 0;
      float l = 0;
      float r = 0;
    } outer_border;  // 外部边界状态
    std::vector<float> layer_ratios;

    std::unique_ptr<Node> first;
    std::unique_ptr<Node> second;

    float Area() const;
    float RecordLayerRatio(float before_area);
    void PlaceBody(bool horizontal, float start, float body_size,
                   float fixed_start = 0, float fixed_end = 0);
    float SkeletonVerticalCenter() const;
    void PlaceZeroSegmentCenter(float target);
    FixedInsets Normalize(NormalizeDirection dir, const Layout &layout);
    void ApplyTransform(const BoundingBox &transform);
    BoundingBox TightBoundingBox(const Layout &layout) const;
    float MaximumStrokeWidth(const Layout &layout,
                             float inherited_weight = 1.0f) const;
    std::vector<Stroke> Flatten(const Layout &layout) const;

   private:
    float LayerWeight(const Layout &layout) const;
    void CollectStrokes(const Layout &layout, const BoundingBox &parent_box,
                        float inherited_weight,
                        std::vector<Stroke> &output) const;
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

 private:
  friend class JianziLibrary;
  friend struct JianziContext;

  explicit Jianzi(const JianziContext &context);
  Jianzi(const JianziContext &context, const char *u8_ch);
  void CheckContext(const Jianzi &other) const;
  Jianzi MakeMissingCombination(const Jianzi &other, char operation) const;
  void CopyData(const Jianzi &other);
  void MoveData(Jianzi &&other);

  const JianziContext &m_context;

};

// 字库实例独占所有加载后的数据及渲染状态。由它生成的 Jianzi
// 只借用该状态，因此 JianziLibrary 必须比所有相关 Jianzi 活得更久。
class JianziLibrary {
 public:
  // 与 RenderPath() 输出相同坐标系中的排版指标。
  struct LayoutMetrics {
    float units_per_em = 1000.0f;
    float baseline_y = 880.0f;
  };

  ~JianziLibrary();

  JianziLibrary(const JianziLibrary &) = delete;
  JianziLibrary &operator=(const JianziLibrary &) = delete;

  JianziLibrary(JianziLibrary &&other) noexcept;
  JianziLibrary &operator=(JianziLibrary &&other) noexcept;

  // 从内存加载完整的 CBOR 字库。缓冲区只需在调用期间保持有效。
  static JianziLibrary Load(const std::uint8_t *data, std::size_t size);

  // 获取字库排版指标，供外部排版系统缩放并对齐减字。
  LayoutMetrics GetLayoutMetrics() const;

  // 根据公式生成减字。
  Jianzi Parse(const char *u8_str) const;
  // 根据自然字串生成公式。
  std::string ParseNatural(const char *u8_str) const;

 private:
  explicit JianziLibrary(std::unique_ptr<JianziContext> context);

  std::unique_ptr<JianziContext> m_context;
};

}  // namespace qin
