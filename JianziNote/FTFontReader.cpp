// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "FTFontReader.hpp"

#include <freetype/freetype.h>
#include <freetype/fttypes.h>

#include <cstddef>

using namespace qin;

bool FTFontReader::LoadFont(const char *data, size_t length) {
  // 初始化Freetype
  int err = FT_Init_FreeType(&m_library);
  if (err) {
    return false;
  }
  FT_Open_Args args = {};
  args.flags = FT_OPEN_MEMORY;
  args.memory_base = (FT_Byte *)data;
  args.memory_size = length;
  err = FT_Open_Face(m_library, &args, 0, &m_face);
  if (err) {
    return false;
  }

  return true;
}

void FTFontReader::ReleaseFont() {
  if (m_face != nullptr) {
    FT_Done_Face(m_face);
  }
  if (m_library != nullptr) {
    FT_Done_FreeType(m_library);
  }
}

std::vector<qin::JianziStyler::PathData> FTFontReader::GetPath(
    size_t codepoint) const {
  int err = 0;
  // code转成index
  size_t index = FT_Get_Char_Index(m_face, codepoint);
  // 打开index，获取outline
  err = FT_Load_Glyph(m_face, index, FT_LOAD_DEFAULT);
  FT_Outline outline = m_face->glyph->outline;

  // 将outline转换成path
  std::vector<qin::JianziStyler::PathData> ret;

  int cur_pt_idx = 0;
  for (int i = 0; i < outline.n_contours; ++i) {
    bool is_first_pt = true;
    qin::Point2f first_pt;
    // 逐个contour进行

    // 先新建一个空的data(默认是close)
    ret.push_back(qin::JianziStyler::PathData());
    while (true) {
      // 循环取点直至点的index超过contours数组指示的边界
      qin::JianziStyler::PathData &data = ret.back();
      // 读一点
      qin::Point2f pt;
      pt.x = outline.points[cur_pt_idx].x;
      pt.y = outline.points[cur_pt_idx].y;
      data.pts.push_back(pt);
      int tag = outline.tags[cur_pt_idx];

      if (is_first_pt) {
        // 第一个点必然是moveto
        data.key = qin::JianziStyler::PathKey::MoveTo;
        // 一组结束
        ret.push_back(qin::JianziStyler::PathData());

        first_pt = pt;  // 将第一个点记住，以备后续之需
        is_first_pt = false;
      } else if (tag & 1) {
        // 在线上，根据当前data的key判断：
        if (data.key == qin::JianziStyler::PathKey::Close) {
          // 这是新的一组点，记为LineTo
          data.key = qin::JianziStyler::PathKey::LineTo;
        }
        // 总之意味着一组结束
        ret.push_back(qin::JianziStyler::PathData());
      } else if (tag & 2) {
        // 三次曲线
        data.key = qin::JianziStyler::PathKey::CubicTo;
      } else {
        // 二次曲线，再读入一点
        data.key = qin::JianziStyler::PathKey::QuadTo;
      }
      cur_pt_idx++;

      if (cur_pt_idx > outline.contours[i] || cur_pt_idx >= outline.n_points) {
        // 一条曲线数据结束
        while (ret.back().key == qin::JianziStyler::PathKey::CubicTo &&
                   ret.back().pts.size() < 3 ||
               ret.back().key == qin::JianziStyler::PathKey::QuadTo &&
                   ret.back().pts.size() < 2) {
          // 此处意味着最后一条线的终点应当接着起点
          ret.back().pts.push_back(first_pt);
        }
        // 终点应该是close
        if (ret.back().key != qin::JianziStyler::PathKey::Close) {
          ret.push_back(qin::JianziStyler::PathData());
        }

        is_first_pt = true;  // 重置第一点状态
        break;
      }
    }
  }

  return ret;
}
