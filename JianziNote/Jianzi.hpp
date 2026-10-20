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
#include "Result.hpp"
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
  Jianzi &operator=(const Jianzi &other) noexcept;

  Jianzi(Jianzi &&other) noexcept;
  Jianzi &operator=(Jianzi &&other) noexcept;

  // 将两个减字横向合并，均分左右
  Result<Jianzi> operator&(const Jianzi &right) const;
  // 将两个减字横向合并，中间多留一个笔画宽度
  //（用于给“撮”“剌”等中间有一竖的减字留空间）
  Result<Jianzi> operator|(const Jianzi &right) const;
  // 将两个减字横向合并，左侧分配33%
  Result<Jianzi> operator<(const Jianzi &right) const;
  // 将两个减字纵向合并
  Result<Jianzi> operator/(const Jianzi &below) const;
  // 将两个减字纵向合并，但限制上半部不超过一半
  Result<Jianzi> operator^(const Jianzi &below) const;
  // 对于普通减字，此运算与纵向合并相同
  // 对于带空间的减字，则是将指定减字放入填充空间内
  Result<Jianzi> operator*(const Jianzi &content) const;

  // 使用字形包内置的样式渲染。
  Result<std::vector<PathData>> RenderPath() const;

  // 边界避让标记
  enum class BottomAvoidance { Segment, Distance };
  struct BorderFlags {
    bool t = false;  // 顶部
    bool b = false;  // 底部
    bool l = false;  // 左边
    bool r = false;  // 右边
    BottomAvoidance bottom_mode = BottomAvoidance::Segment; // b 为 true 时有效
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
    float aspect_limit = 1.5f;
  };
  std::string m_name;  // 减字名称
  JianziStatus m_status = JianziStatus::Empty;
  std::vector<std::string> m_missing_names;


  struct Capsule {
    Point2f tl;                // 左上角坐标
    Point2f br;                // 右下角坐标
    BorderFlags border_flags;  // 避让标记
    int v_segments = 1;        // 填充区的纵向间隔数
  };

  // Operators retain original leaves; layout happens after the whole structure is known.
  struct Node {
    Node() = default;
    Node(const Node &other);
    Node &operator=(const Node &other);
    Node(Node &&other) noexcept = default;
    Node &operator=(Node &&other) noexcept = default;
    char operation = 0;
    std::string name;
    BoundingBox range;
    BorderFlags flags;
    int segments = 1;
    std::unique_ptr<Capsule> capsule;
    std::vector<Stroke> strokes;
    std::unique_ptr<Node> first;
    std::unique_ptr<Node> second;
  };

  struct LayoutOutput {
    std::vector<Stroke> strokes;
    BorderFlags flags;
    int segments = 0;
  };
  struct LayoutEngine {
    static Result<LayoutOutput> Run(const Node &root, const Layout &layout, bool render = true);
  };

  // std::vector<Stroke> m_strokes; // 减字所包含的所有端点
  std::unique_ptr<Node> m_node = nullptr;  // 节点组成减字

  BorderFlags m_border_flags;  // 避让标记
  int m_v_segments = 1;        // 纵向间隔数

 private:
  friend class JianziLibrary;
  friend struct JianziContext;

  explicit Jianzi(const JianziContext &context);
  Jianzi(const JianziContext &context, const char *u8_ch);
  bool HasSameContext(const Jianzi &other) const noexcept;
  Jianzi MakeMissingCombination(const Jianzi &other, char operation) const;
  Result<Jianzi> Combine(const Jianzi &other, char operation) const;
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
  static Result<JianziLibrary> Load(const std::uint8_t *data, std::size_t size);

  // 获取字库排版指标，供外部排版系统缩放并对齐减字。
  LayoutMetrics GetLayoutMetrics() const;

  // 根据公式生成减字。
  Result<Jianzi> Parse(const char *u8_str) const;
  // 根据自然字串生成公式。
  Result<std::string> ParseNatural(const char *u8_str) const;

 private:
  explicit JianziLibrary(std::unique_ptr<JianziContext> context);

  std::unique_ptr<JianziContext> m_context;
};

}  // namespace qin
