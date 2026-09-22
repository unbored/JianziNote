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

  auto renderer = qin::StrokeDescRenderer::Create(descriptions);
  if (!renderer) {
    std::cerr << renderer.GetError()->message << '\n';
    return 1;
  }
  qin::Stroke stroke;
  stroke.desc = "test-flipped-outline";
  stroke.width = 0.1f;
  stroke.vertice = {{{0, 0}, qin::VertexRegion::Top}, {{1, 0}, qin::VertexRegion::Top}};
  const auto path = (*renderer.GetValue())->Render({stroke});
  if (!path) {
    std::cerr << path.GetError()->message << '\n';
    return 1;
  }
  const auto& commands = *path.GetValue();
  if (commands.size() != 5 || commands[0].key != qin::PathKey::MoveTo ||
      commands[4].key != qin::PathKey::Close || !Near(commands[0].pts[0].x, 0) ||
      !Near(commands[0].pts[0].y, -0.05f) || !Near(commands[2].pts[0].x, 1) ||
      !Near(commands[2].pts[0].y, 0.05f)) {
    std::cerr << "Unexpected StrokeDesc runtime outline\n";
    return 1;
  }

  auto invalid_descriptions = descriptions;
  invalid_descriptions[0]["fit"]["model"]["weights"] = "invalid";
  const auto invalid_renderer = qin::StrokeDescRenderer::Create(invalid_descriptions);
  if (invalid_renderer ||
      invalid_renderer.GetError()->code != qin::JianziErrorCode::InvalidStrokeDesc) {
    std::cerr << "Invalid StrokeDesc input was not rejected\n";
    return 2;
  }

  stroke.width = 0;
  const auto invalid_geometry = (*renderer.GetValue())->Render({stroke});
  if (invalid_geometry ||
      invalid_geometry.GetError()->code != qin::JianziErrorCode::InvalidGeometry) {
    std::cerr << "Invalid stroke geometry was not rejected\n";
    return 3;
  }
  return 0;
}
