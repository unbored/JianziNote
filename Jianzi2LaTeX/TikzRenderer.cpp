// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "TikzRenderer.hpp"

namespace qin {

std::string TikzRenderer::Render(
    const std::vector<JianziStyler::PathData> &path_data) {
  std::string ret;

  // 注意y轴转换
  for (auto &p : path_data) {
    switch (p.key) {
      case JianziStyler::PathKey::MoveTo:
        ret += " (" + std::to_string(p.pts[0].x) + "," +
               std::to_string(1.0f - p.pts[0].y) + ")";
        break;
      case JianziStyler::PathKey::LineTo:
        ret += " -- (" + std::to_string(p.pts[0].x) + "," +
               std::to_string(1.0f - p.pts[0].y) + ")";
        break;
      case JianziStyler::PathKey::QuadTo: {
        // Tikz不提供二阶曲线的命令，升成三阶
        // 需要获得前一个路径点的最后一个点
        Point2f pt0 = (&p)[-1].pts.back();
        Point2f pt1 = 1.0f / 3.0f * pt0 + 2.0f / 3.0f * p.pts[0];
        Point2f pt2 = 2.0f / 3.0f * p.pts[0] + 1.0f / 3.0f * p.pts[1];
        Point2f pt3 = p.pts[1];
        ret += " .. controls (" + std::to_string(pt1.x) + "," +
               std::to_string(1.0f - pt1.y) + ")" + " and (" +
               std::to_string(pt2.x) + "," + std::to_string(1.0f - pt2.y) +
               ")" + " .. (" + std::to_string(pt3.x) + "," +
               std::to_string(1.0f - pt3.y) + ")";
      } break;
      case JianziStyler::PathKey::CubicTo:
        ret += " .. controls (" + std::to_string(p.pts[0].x) + "," +
               std::to_string(1.0f - p.pts[0].y) + ")" + " and (" +
               std::to_string(p.pts[1].x) + "," +
               std::to_string(1.0f - p.pts[1].y) + ")" + " .. (" +
               std::to_string(p.pts[2].x) + "," +
               std::to_string(1.0f - p.pts[2].y) + ")";
        break;
      case JianziStyler::PathKey::Close:
        ret += " -- cycle";
      default:
        break;
    }
  }

  return ret;
}

}  // namespace qin
