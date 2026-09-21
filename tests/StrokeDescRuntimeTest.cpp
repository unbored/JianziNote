#include <cmath>
#include <iostream>
#include <vector>

#include <nlohmann/json.hpp>

#include "StrokeDescRenderer.hpp"

namespace {

bool Near(float actual, float expected) { return std::abs(actual - expected) < 1e-5f; }

}  // namespace

int main() {
  using nlohmann::json;
  const std::vector<double> outline = {0, -0.5, 1, -0.5, 1, 0.5, 0, 0.5};
  json weights = json::array();
  weights.push_back(outline);
  for (int row = 0; row < 3; ++row) weights.push_back(std::vector<double>(outline.size(), 0));
  const json descriptions = json::array({
      {{"type", "test-flipped-outline"},
       {"default_width", 0.1},
       {"fit",
        {{"topology", {{"contours", json::array()}}},
         {"model",
          {{"means", {0, 0, 0}},
           {"scales", {1, 1, 1}},
           {"weights", weights},
           {"point_segment_bindings", {0, 0, 0, 0}},
           {"template_outline_local",
            json::array({json::array({{{"type", "M"}, {"x", 0}, {"y", 0}},
                                     {{"type", "L"}, {"x", 0}, {"y", 0}},
                                     {{"type", "L"}, {"x", 0}, {"y", 0}},
                                     {{"type", "L"}, {"x", 0}, {"y", 0}},
                                     {{"type", "Z"}}})})}}}}}}});

  qin::StrokeDescRenderer renderer(descriptions);
  qin::Stroke stroke;
  stroke.desc = "test-flipped-outline";
  stroke.width = 0.1f;
  stroke.vertice = {{{0, 0}, qin::VertexRegion::Top}, {{1, 0}, qin::VertexRegion::Top}};
  const auto path = renderer.Render({stroke});
  if (path.size() != 5 || path[0].key != qin::PathKey::MoveTo || path[4].key != qin::PathKey::Close ||
      !Near(path[0].pts[0].x, 0) || !Near(path[0].pts[0].y, -0.05f) ||
      !Near(path[2].pts[0].x, 1) || !Near(path[2].pts[0].y, 0.05f)) {
    std::cerr << "Unexpected StrokeDesc runtime outline\n";
    return 1;
  }
  return 0;
}
