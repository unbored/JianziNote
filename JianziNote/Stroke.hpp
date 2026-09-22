// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <vector>

#include "Point.hpp"

namespace qin {

// 标记点属于哪个区域，影响变形时是否跟随 capsule。
enum class VertexRegion { Top = 0, Medium = 1, Bottom = 2 };

struct StrokeVertex {
  Point2f pt;
  VertexRegion region = VertexRegion::Top;
};

// StrokeDesc 的运行时输入：描述 ID、笔画宽度与骨架节点。
struct Stroke {
  std::string desc;
  float width = 0.1f;
  std::vector<StrokeVertex> vertice;
};

}  // namespace qin
