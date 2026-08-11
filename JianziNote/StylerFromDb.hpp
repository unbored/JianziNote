// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#ifndef STYLERFROMDB_H
#define STYLERFROMDB_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <filesystem>
#include <nlohmann/json.hpp>

#include "FTFontReader.hpp"
#include "JianziDefines.hpp"

namespace qin {

class StylerFromDb {
 public:
  // 默认为SemiBold粗细，笔画宽度为0.1。
  // Regular粗细的宽度为0.07
  StylerFromDb(float stroke_width = 0.1f);
  ~StylerFromDb();

  void LoadCbor(const nlohmann::json& styler,
                const std::filesystem::path& font_file);

  std::vector<PathData> RenderChar(size_t codepoint) const;
  std::vector<PathData> RenderPath(const std::vector<Stroke> &strokes) const;

  float GetStrokeWidth() const;

 private:
  const float c_stroke_width;

  enum class Direction { Forward, Backward };
  void ProcessVertex(const StrokeVertex &v, float weight, Direction dir,
                     std::vector<PathData> &path) const;

  std::unordered_map<std::string, VertexDesc> m_desc_map;

  FTFontReader m_font_reader;
  std::unique_ptr<char[]> m_font_data;
};

}  // namespace qin

#endif  // STYLERFROMDB_H
