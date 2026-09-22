// Copyright (c) 2024 King
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include <Jianzi.hpp>

#include "SvgRenderer.hpp"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace {

qin::Result<std::vector<std::uint8_t>> ReadBinaryFile(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return qin::Result<std::vector<std::uint8_t>>::Failure(
        {qin::JianziErrorCode::IoError, "Unable to open library file: " + path});
  }
  std::vector<std::uint8_t> result{
      std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
  if (input.bad()) {
    return qin::Result<std::vector<std::uint8_t>>::Failure(
        {qin::JianziErrorCode::IoError, "Unable to read library file: " + path});
  }
  return qin::Result<std::vector<std::uint8_t>>::Success(std::move(result));
}

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

  auto libraryData = ReadBinaryFile(arguments[1]);
  if (!libraryData) {
    std::cerr << "jianzi2svg: " << libraryData.GetError()->message << '\n';
    return 1;
  }
  auto loaded = qin::JianziLibrary::Load(libraryData.GetValue()->data(), libraryData.GetValue()->size());
  if (!loaded) {
    std::cerr << "jianzi2svg: " << loaded.GetError()->message << '\n';
    return 1;
  }
  auto library = std::move(*loaded.GetValue());
  const auto formula = library.ParseNatural(arguments[2].c_str());
  if (!formula) {
    std::cerr << "jianzi2svg: " << formula.GetError()->message << '\n';
    return 1;
  }
  const auto jianzi = library.Parse(formula.GetValue()->c_str());
  if (!jianzi) {
    std::cerr << "jianzi2svg: " << jianzi.GetError()->message << '\n';
    return 1;
  }
  const auto paths = jianzi.GetValue()->RenderPath();
  if (!paths) {
    std::cerr << "jianzi2svg: " << paths.GetError()->message << '\n';
    return 1;
  }
  const auto svg = qin::SvgRenderer().Render(*paths.GetValue());

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
  return 0;
}
