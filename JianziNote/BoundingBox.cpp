// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "BoundingBox.hpp"

namespace qin {

BoundingBox BoundingBox::operator*(const BoundingBox &other) const {
  BoundingBox ret;
  ret.w = w * other.w;
  ret.h = h * other.h;
  ret.x = w * other.x + x;
  ret.y = h * other.y + y;

  return ret;
}

Point2f BoundingBox::operator*(const Point2f &pt) const {
  Point2f ret;
  ret.x = w * pt.x + x;
  ret.y = h * pt.y + y;

  return ret;
}

}  // namespace qin
