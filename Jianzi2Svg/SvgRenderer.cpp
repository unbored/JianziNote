// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "SvgRenderer.hpp"

#include <iomanip>
#include <sstream>

namespace qin {

std::string SvgRenderer::Render(const std::vector<PathData>& path_data) const {
  std::ostringstream output;
  output << std::setprecision(7);
  output << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 1 1\">\n"
         << "  <path fill=\"#000\" fill-rule=\"nonzero\" d=\"";

  for (const auto& item : path_data) {
    switch (item.key) {
      case PathKey::MoveTo:
        output << "M " << item.pts[0].x << ' ' << item.pts[0].y << ' ';
        break;
      case PathKey::LineTo:
        output << "L " << item.pts[0].x << ' ' << item.pts[0].y << ' ';
        break;
      case PathKey::QuadTo:
        output << "Q " << item.pts[0].x << ' ' << item.pts[0].y << ' '
               << item.pts[1].x << ' ' << item.pts[1].y << ' ';
        break;
      case PathKey::CubicTo:
        output << "C " << item.pts[0].x << ' ' << item.pts[0].y << ' '
               << item.pts[1].x << ' ' << item.pts[1].y << ' '
               << item.pts[2].x << ' ' << item.pts[2].y << ' ';
        break;
      case PathKey::Close:
        output << "Z ";
        break;
    }
  }

  output << "\"/>\n</svg>\n";
  return output.str();
}

}  // namespace qin
