// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

namespace qin {

template <typename T>
struct Point2 {
  Point2 operator+(const Point2 &pt) const {
    Point2 ret;
    ret.x = x + pt.x;
    ret.y = y + pt.y;
    return ret;
  }
  Point2 operator-(const Point2 &pt) const {
    Point2 ret;
    ret.x = x - pt.x;
    ret.y = y - pt.y;
    return ret;
  }
  Point2 operator*(T scale) const {
    Point2 ret;
    ret.x = x * scale;
    ret.y = y * scale;
    return ret;
  }

  Point2 operator/(T scale) const {
    Point2 ret;
    ret.x = x / scale;
    ret.y = y / scale;
    return ret;
  }

  friend Point2 operator*(T scale, const Point2 &pt) {
    Point2 ret;
    ret.x = pt.x * scale;
    ret.y = pt.y * scale;
    return ret;
  }

  T x;
  T y;
};

using Point2f = Point2<float>;

}  // namespace qin
