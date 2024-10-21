// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "Point.hpp"

namespace qin {

struct BoundingBox {
  float x = 0.0f;
  float y = 0.0f;
  float w = 1.0f;
  float h = 1.0f;

  BoundingBox operator*(const BoundingBox &other) const;
  Point2f operator*(const Point2f &pt) const;
};

}  // namespace qin
