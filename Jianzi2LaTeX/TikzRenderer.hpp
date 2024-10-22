// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#ifndef TIKZRENDERER_HPP
#define TIKZRENDERER_HPP

#include <JianziStyler.hpp>
#include <string>

namespace qin {

class TikzRenderer {
 public:
  TikzRenderer() = default;
  ~TikzRenderer() = default;

 public:
  std::string Render(const std::vector<qin::JianziStyler::PathData> &path_data);
};

}  // namespace qin

#endif  // TIKZRENDERER_HPP
