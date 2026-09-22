// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include <BoundingBox.hpp>
#include <Jianzi.hpp>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include "LilypondRenderer.hpp"

#ifdef _WIN32

#include <Windows.h>
#include <assert.h>

#endif

using namespace qin;

enum class ArrayDirection { Vertical, Horizontal };

// 阵列
Result<std::string> ProcessArray(const JianziLibrary &library, std::string jianzi_str,
                                 ArrayDirection dir) {
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

  std::vector<PathData> path_data;

  for (int i = 0; i < sub_strs.size(); ++i) {
    // 计算一个减字
    const auto formula = library.ParseNatural(sub_strs[i].c_str());
    if (!formula) return Result<std::string>::Failure(*formula.GetError());
    const auto jianzi = library.Parse(formula.GetValue()->c_str());
    if (!jianzi) return Result<std::string>::Failure(*jianzi.GetError());
    auto path = jianzi.GetValue()->RenderPath();
    if (!path) return Result<std::string>::Failure(*path.GetError());

    // 创建boundingbox，根据boundingbox缩放笔画
    BoundingBox box;
    box.w = sub_w;
    box.h = sub_w;
    if (dir == ArrayDirection::Vertical) {
      box.x = -0.5f * sub_w;
      box.y = i * sub_w;
      if (sub_strs.size() == 1) {
        // 只有一个字时，向下对齐
        box.y = 0.5f;
      }
    } else {
      box.x = ((float)i - 0.5f * sub_strs.size()) * sub_w;
      // 向下对齐
      box.y = 1.0f - sub_w;
    }

    for (auto &p : *path.GetValue()) {
      for (auto &pt : p.pts) {
        pt = box * pt;
      }
      path_data.push_back(p);
    }
  }

  // 根据path绘制
  qin::LilypondRenderer renderer;
  auto ly_result = renderer.Render(path_data);

  std::string ret = "\\markup{\n";
  ret += "\\scale \\jianziSize\n";

  ret += ly_result;

  ret += "}\n";

  return Result<std::string>::Success(std::move(ret));
}

// 单字
Result<std::string> ProcessSingle(const JianziLibrary &library, std::string jianzi_str) {
  const auto formula = library.ParseNatural(jianzi_str.c_str());
  if (!formula) return Result<std::string>::Failure(*formula.GetError());
  const auto jianzi = library.Parse(formula.GetValue()->c_str());
  if (!jianzi) return Result<std::string>::Failure(*jianzi.GetError());
  auto path_data = jianzi.GetValue()->RenderPath();
  if (!path_data) return Result<std::string>::Failure(*path_data.GetError());

  // 创建boundingbox，根据boundingbox缩放笔画
  BoundingBox box;
  box.x = -0.5f;  // 垂直居中

  for (auto &p : *path_data.GetValue()) {
    for (auto &pt : p.pts) {
      pt = box * pt;
    }
  }

  // 根据path绘制
  qin::LilypondRenderer renderer;
  auto ly_result = renderer.Render(*path_data.GetValue());

  std::string ret = "\\markup{\n";
  ret += "\\scale \\jianziSize\n";

  ret += ly_result;

  ret += "}\n";

  return Result<std::string>::Success(std::move(ret));
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

  std::string db_file = argv_strs[1];

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
  std::string reg_str = "\\\\\"(jz[v,h]?):(.*?)\"";
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

    if (type == "jz") {
      jianzi_str.insert(jianzi);
    } else if (type == "jzv") {
      jianzi_str_v.insert(jianzi);
    } else if (type == "jzh") {
      jianzi_str_h.insert(jianzi);
    }
  }

  // 初始化减字系统
  std::ifstream library_input(db_file, std::ios::binary);
  if (!library_input) {
    std::cerr << "Unable to open library file: " << db_file << std::endl;
    return -1;
  }
  std::vector<std::uint8_t> library_data{
      std::istreambuf_iterator<char>(library_input), std::istreambuf_iterator<char>()};
  if (library_input.bad()) {
    std::cerr << "Unable to read library file: " << db_file << std::endl;
    return -1;
  }
  auto loaded = qin::JianziLibrary::Load(library_data.data(), library_data.size());
  if (!loaded) {
    std::cerr << loaded.GetError()->message << std::endl;
    return -1;
  }
  auto library = std::move(*loaded.GetValue());

  // 根据提取结果输出文件
  std::ofstream fout("jianzidef.ly");
  fout << "jianziSize = #'(4 . 4)" << std::endl;
  for (auto &j : jianzi_str) {
    // 普通减字
    const auto rendered = ProcessSingle(library, j);
    if (!rendered) {
      std::cerr << rendered.GetError()->message << std::endl;
      return -1;
    }
    fout << "\"jz:" << j << "\" = " << *rendered.GetValue();
  }
  for (auto &j : jianzi_str_v) {
    // 竖排小字
    const auto rendered = ProcessArray(library, j, ArrayDirection::Vertical);
    if (!rendered) {
      std::cerr << rendered.GetError()->message << std::endl;
      return -1;
    }
    fout << "\"jzv:" << j << "\" = " << *rendered.GetValue();
  }
  for (auto &j : jianzi_str_h) {
    // 横排小字
    const auto rendered = ProcessArray(library, j, ArrayDirection::Horizontal);
    if (!rendered) {
      std::cerr << rendered.GetError()->message << std::endl;
      return -1;
    }
    fout << "\"jzh:" << j << "\" = " << *rendered.GetValue();
  }
  fout.close();

  return 0;
}
