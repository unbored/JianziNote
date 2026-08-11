// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <vector>

#include <JianziDefines.hpp>

namespace qin {

class SvgRenderer {
 public:
  std::string Render(const std::vector<PathData>& path_data) const;
};

}  // namespace qin
