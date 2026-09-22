#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Jianzi.hpp"

namespace {
bool SamePaths(const std::vector<qin::PathData>& left, const std::vector<qin::PathData>& right) {
  if (left.size() != right.size()) return false;
  for (std::size_t i = 0; i < left.size(); ++i) {
    if (left[i].key != right[i].key || left[i].pts.size() != right[i].pts.size()) return false;
    for (std::size_t j = 0; j < left[i].pts.size(); ++j) {
      if (left[i].pts[j].x != right[i].pts[j].x || left[i].pts[j].y != right[i].pts[j].y) return false;
    }
  }
  return true;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "Usage: jianzi-instance-test <library.cbor> <text>\n";
    return 1;
  }

  std::ifstream input(argv[1], std::ios::binary);
  std::vector<std::uint8_t> libraryBytes{
      std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
  if (!input || libraryBytes.empty()) {
    std::cerr << "Unable to read the library into memory.\n";
    return 1;
  }
  auto firstLibrary = qin::JianziLibrary::Load(libraryBytes.data(), libraryBytes.size());
  auto secondLibrary = qin::JianziLibrary::Load(libraryBytes.data(), libraryBytes.size());
  libraryBytes.clear();
  libraryBytes.shrink_to_fit();

  const auto firstFormula = firstLibrary.ParseNatural(argv[2]);
  const auto secondFormula = secondLibrary.ParseNatural(argv[2]);
  auto first = firstLibrary.Parse(firstFormula.c_str());
  auto second = secondLibrary.Parse(secondFormula.c_str());

  const auto firstPaths = first.RenderPath();
  if (firstPaths.empty() || second.RenderPath().empty()) {
    std::cerr << "An independently loaded library failed to render.\n";
    return 1;
  }

  const auto copied = first;
  if (!SamePaths(firstPaths, copied.RenderPath())) {
    std::cerr << "Copy construction changed the rendered tree.\n";
    return 1;
  }

  auto assigned = firstLibrary.Parse(firstFormula.c_str());
  assigned = first;
  if (!SamePaths(firstPaths, assigned.RenderPath())) {
    std::cerr << "Copy assignment changed the rendered tree.\n";
    return 1;
  }

  bool rejected = false;
  try {
    const auto invalid = first & second;
    static_cast<void>(invalid);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  if (!rejected) {
    std::cerr << "Cross-library composition was not rejected.\n";
    return 1;
  }

  rejected = false;
  try {
    assigned = second;
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  if (!rejected) {
    std::cerr << "Cross-library assignment was not rejected.\n";
    return 1;
  }

  auto movedLibrary = std::move(firstLibrary);
  if (first.RenderPath().empty()) {
    std::cerr << "Moving the owning library invalidated its context address.\n";
    return 1;
  }

  return 0;
}
