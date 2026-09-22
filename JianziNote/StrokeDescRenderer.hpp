#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "Result.hpp"
#include "Stroke.hpp"
#include "VectorPath.hpp"

namespace qin {

// StrokeDesc v2 的运行时预测器。拟合与样本处理留在网页编辑器中。
class StrokeDescRenderer {
 public:
  static Result<std::unique_ptr<StrokeDescRenderer>> Create(
      const nlohmann::json& descriptions);
  ~StrokeDescRenderer();

  bool Contains(std::string_view name) const;
  Result<std::vector<PathData>> Render(const std::vector<Stroke>& strokes) const;

 private:
  struct Impl;
  explicit StrokeDescRenderer(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

}  // namespace qin
