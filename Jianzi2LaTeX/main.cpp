// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include <BoundingBox.hpp>
#include <Jianzi.hpp>
#include <StylerFromDb.hpp>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <string>

#include "TikzRenderer.hpp"

#ifdef _WIN32

#include <Windows.h>
#include <assert.h>

#endif

using namespace qin;

enum class ArrayDirection { Vertical, Horizontal };

// 阵列
std::string ProcessArray(std::string db_file, std::string jianzi_str,
                         ArrayDirection dir) {
  // 初始化减字系统
  qin::Jianzi::OpenDb(db_file.c_str());
  auto styler_names = qin::StylerFromDb::GetStylerList(db_file);
  qin::StylerFromDb styler;
  styler.Load(db_file, styler_names[0]);

  std::string input_str = jianzi_str;
  // 根据逗号分割字符串
  std::vector<std::string> sub_strs;
  while (!input_str.empty()) {
    auto pos = input_str.find(',');
    sub_strs.push_back(input_str.substr(0, pos));

    if (pos == input_str.npos) {
      break;
    }
    input_str = input_str.substr(pos + 1);
  }

  // 根据子串个数确定每个字宽高
  float sub_w = 1.0f / sub_strs.size();
  if (sub_strs.size() == 1) {
    // 只有一个字时，默认半高
    sub_w = 0.5f;
  }

  std::vector<JianziStyler::PathData> path_data;

  for (int i = 0; i < sub_strs.size(); ++i) {
    // 计算一个减字
    Jianzi jianzi =
        Jianzi::Parse(Jianzi::ParseNatural(sub_strs[i].c_str()).c_str());
    auto path = jianzi.RenderPath(styler);

    // 创建boundingbox，根据boundingbox缩放笔画
    BoundingBox box;
    box.w = sub_w;
    box.h = sub_w;
    if (dir == ArrayDirection::Vertical) {
      box.y = i * sub_w;
      if (sub_strs.size() == 1) {
        // 只有一个字时，向下对齐
        box.y = 0.5f;
      }
    } else {
      box.x = i * sub_w;
      // 向下对齐
      box.y = 1.0f - sub_w;
    }

    for (auto &p : path) {
      for (auto &pt : p.pts) {
        pt = box * pt;
      }
      path_data.push_back(p);
    }
  }

  // 根据path绘制
  qin::TikzRenderer renderer;
  auto tikz_result = renderer.Render(path_data);

  std::string ret = tikz_result;

  return ret;
}

// 单字
std::string ProcessSingle(std::string db_file, std::string jianzi_str) {
  qin::Jianzi::OpenDb(db_file.c_str());
  Jianzi jianzi =
      Jianzi::Parse(Jianzi::ParseNatural(jianzi_str.c_str()).c_str());

  auto styler_names = qin::StylerFromDb::GetStylerList(db_file);
  qin::StylerFromDb styler;
  styler.Load(db_file, styler_names[0]);
  auto path_data = jianzi.RenderPath(styler);

  // // 创建boundingbox，根据boundingbox缩放笔画
  // BoundingBox box;
  // box.x = -0.5f; // 垂直居中
  // box.y = -0.5f; // 水平上移

  // for (auto &p : path_data)
  // {
  //     for (auto &pt : p.pts)
  //     {
  //         pt = box * pt;
  //     }
  // }

  // 根据path绘制
  qin::TikzRenderer renderer;
  auto tikz_result = renderer.Render(path_data);

  std::string ret = tikz_result;

  return ret;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    return -1;
  }

  std::vector<std::string> argv_strs;

#ifdef _WIN32

  // 获取UTF8格式的字符串
  auto cmd_line = GetCommandLineW();
  int argcw = 0;
  auto argvw = CommandLineToArgvW(cmd_line, &argcw);
  assert(argcw == argc);

  // 转换到UTF-8
  for (int i = 0; i < argcw; ++i) {
    int buf_size = WideCharToMultiByte(CP_UTF8, 0, argvw[i], -1, nullptr, 0,
                                       nullptr, nullptr);
    LPSTR buf = new char[buf_size];
    WideCharToMultiByte(CP_UTF8, 0, argvw[i], -1, buf, buf_size, nullptr,
                        nullptr);
    argv_strs.push_back(buf);
    delete[] buf;
  }

#else

  // 直接将字符串扔进
  for (int i = 0; i < argc; ++i) {
    argv_strs.push_back(argv[i]);
  }

#endif

  std::string db_file = argv[1];

  // 创建一个set记录所有检测到的减字
  std::set<std::string> jianzi_str;
  std::set<std::string> jianzi_str_v;
  std::set<std::string> jianzi_str_h;

  // 打开文件寻找字符串
  std::ifstream fin(argv[2]);

  // 全部读入
  std::string input_str((std::istreambuf_iterator<char>(fin)),
                        std::istreambuf_iterator<char>());
  fin.close();

  // 正则表达式遍历
  std::string reg_str = "\\\\(jz[a-z]?)\\{(.*?)\\}";
  std::regex jianzi_reg(reg_str);

  std::sregex_iterator pos(input_str.begin(), input_str.end(), jianzi_reg);
  for (; pos != std::sregex_iterator(); ++pos) {
    std::smatch matches = *pos;
    if (matches.size() < 3) {
      continue;
    }
    // 提取结果
    std::string type = matches[1];
    std::string jianzi = matches[2];

    // v或者h放到函数里分割
    if (type == "jzv") {
      jianzi_str_v.insert(jianzi);
    } else if (type == "jzh") {
      jianzi_str_h.insert(jianzi);
    } else {
      // 根据逗号分割字符串
      std::string input_str = jianzi;
      while (!input_str.empty()) {
        auto pos = input_str.find(',');
        jianzi_str.insert(input_str.substr(0, pos));

        if (pos == input_str.npos) {
          break;
        }
        input_str = input_str.substr(pos + 1);
      }
      // jianzi_str.insert(jianzi);
    }
  }

  // 根据提取结果输出文件
  std::ofstream fout("jianzilut.sty");

  // 普通减字
  fout << "\\definejianzitikz{" << std::endl;
  for (auto &j : jianzi_str) {
    fout << "{" << j << "}{" << ProcessSingle(db_file, j) << "}" << std::endl;
  }
  fout << "}" << std::endl;

  // 竖排减字
  fout << "\\definejianzitikzv{" << std::endl;
  for (auto &j : jianzi_str_v) {
    fout << "{" << j << "}{"
         << ProcessArray(db_file, j, ArrayDirection::Vertical) << "}"
         << std::endl;
  }
  fout << "}" << std::endl;

  // 横排减字
  fout << "\\definejianzitikzh{" << std::endl;
  for (auto &j : jianzi_str_h) {
    fout << "{" << j << "}{"
         << ProcessArray(db_file, j, ArrayDirection::Horizontal) << "}"
         << std::endl;
  }
  fout << "}" << std::endl;

  fout.close();

  return 0;
}
