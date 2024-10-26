// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#pragma once

#include <ft2build.h>

#include <vector>

#include "JianziStyler.hpp"
#include FT_FREETYPE_H

namespace qin {

class FTFontReader {
 public:
  // 加载字体数据。数据不在此释放
  bool LoadFont(const char *data, size_t length);
  void ReleaseFont();

  // 获得指定code的pathdata
  std::vector<JianziStyler::PathData> GetPath(size_t codepoint) const;

 private:
  FT_Library m_library = nullptr;
  FT_Face m_face = nullptr;
};

}  // namespace qin