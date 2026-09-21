#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "JianziDefines.hpp"

namespace qin {

// StrokeDesc v2 的运行时预测器。拟合与样本处理留在网页编辑器中。
class StrokeDescRenderer {
 public:
  explicit StrokeDescRenderer(const nlohmann::json& descriptions);
  ~StrokeDescRenderer();

  bool Contains(std::string_view name) const;
  std::vector<PathData> Render(const std::vector<Stroke>& strokes) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace qin
