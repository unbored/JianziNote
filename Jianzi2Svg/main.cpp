// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <Jianzi.hpp>

#include "SvgRenderer.hpp"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace {

#ifdef _WIN32
std::vector<std::string> GetUtf8Arguments() {
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  std::vector<std::string> result;
  result.reserve(argc);
  for (int i = 0; i < argc; ++i) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0,
                                         nullptr, nullptr);
    std::string utf8(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, utf8.data(), size, nullptr,
                        nullptr);
    utf8.pop_back();
    result.push_back(std::move(utf8));
  }
  LocalFree(argv);
  return result;
}
#endif

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
  const auto arguments = GetUtf8Arguments();
#else
  const std::vector<std::string> arguments(argv, argv + argc);
#endif
  if (arguments.size() != 4) {
    std::cerr << "Usage: jianzi2svg <library.cbor> <utf8-jianzi> <output.svg>\n";
    return 1;
  }

  try {
    auto library = qin::JianziLibrary::LoadFile(arguments[1]);
    const auto formula = library.ParseNatural(arguments[2].c_str());
    const auto jianzi = library.Parse(formula.c_str());
    const auto svg = qin::SvgRenderer().Render(jianzi.RenderPath());

    std::ofstream output(arguments[3], std::ios::binary);
    if (!output) {
      std::cerr << "Unable to open output file: " << arguments[3] << '\n';
      return 1;
    }
    output << svg;
    if (!output) {
      std::cerr << "Unable to write output file: " << arguments[3] << '\n';
      return 1;
    }
  } catch (const std::exception& error) {
    std::cerr << "jianzi2svg: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
