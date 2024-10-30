// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#ifndef STYLERFROMDB_H
#define STYLERFROMDB_H

#include <cstddef>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "FTFontReader.hpp"
#include "JianziStyler.hpp"

namespace qin {

class StylerFromDb : public JianziStyler {
 public:
  // 默认为SemiBold粗细，笔画宽度为0.1。
  // Regular粗细的宽度为0.07
  StylerFromDb(float stroke_width = 0.1f);
  ~StylerFromDb();

  static std::vector<std::string> GetStylerList(std::string db_file);
  void Load(std::string db_file, std::string styler_name);

  // JianziStyler interface
 public:
  virtual std::vector<PathData> RenderChar(size_t codepoint) const override;
  virtual std::vector<PathData> RenderPath(
      const std::vector<Stroke> &strokes) const override;

  float GetStrokeWidth() const override;

 private:
  const float c_stroke_width;

  enum class Direction { Forward, Backward };
  void ProcessVertex(const StrokeVertex &v, float weight, Direction dir,
                     std::vector<PathData> &path) const;

  enum class RotateDir { Prev, Next };

  struct PathOffset {
    float x_rel;
    float x_abs;
    float y_rel;
    float y_abs;
    float x_len;
    float y_len;
  };

  struct PathGroup {
    PathKey key;
    std::vector<PathOffset> offsets;
  };

  struct VertexDesc {
    float pre_rotate;
    RotateDir dir;
    std::vector<PathGroup> forward;
    std::vector<PathGroup> backward;
  };

  std::map<VertexType, VertexDesc> m_desc_map;

  FTFontReader m_font_reader;
  std::unique_ptr<char[]> m_font_data;
};

}  // namespace qin

#endif  // STYLERFROMDB_H