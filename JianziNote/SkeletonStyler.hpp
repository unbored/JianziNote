// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include "JianziStyler.hpp"

namespace qin {
class SkeletonStyler : public JianziStyler {
 public:
  virtual std::vector<PathData> RenderPath(
      const std::vector<Stroke> &strokes) const override;
  float GetStrokeWidth() const override;
};
}  // namespace qin
