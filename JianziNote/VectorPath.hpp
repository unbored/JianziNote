// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <vector>

#include "Point.hpp"

namespace qin {

// 通用矢量路径描述，可由不同输出端直接消费。
enum class PathKey {
  Close,
  MoveTo,
  LineTo,
  QuadTo,
  CubicTo,
};

struct PathData {
  PathKey key = PathKey::Close;
  std::vector<Point2f> pts;
};

}  // namespace qin
