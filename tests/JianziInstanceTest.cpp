#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

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
  auto fallbackDocument = nlohmann::json::from_cbor(libraryBytes);
  if (!fallbackDocument.contains("aliases")) fallbackDocument["aliases"] = nlohmann::json::array();
  fallbackDocument["aliases"].push_back(
      {{"alias", "fallback-test-alias"}, {"glyph", u8"😀"}, {"type", "Other"}});
  const auto fallbackBytes = nlohmann::json::to_cbor(fallbackDocument);
  auto fallbackLibrary = qin::JianziLibrary::Load(fallbackBytes.data(), fallbackBytes.size());
  libraryBytes.clear();
  libraryBytes.shrink_to_fit();

  const auto firstMetrics = firstLibrary.GetLayoutMetrics();
  const auto secondMetrics = secondLibrary.GetLayoutMetrics();
  if (!std::isfinite(firstMetrics.units_per_em) || firstMetrics.units_per_em <= 0 ||
      !std::isfinite(firstMetrics.baseline_y) ||
      firstMetrics.units_per_em != secondMetrics.units_per_em ||
      firstMetrics.baseline_y != secondMetrics.baseline_y) {
    std::cerr << "The loaded library returned invalid or inconsistent layout metrics.\n";
    return 1;
  }

  const auto firstFormula = firstLibrary.ParseNatural(argv[2]);
  const auto secondFormula = secondLibrary.ParseNatural(argv[2]);
  auto first = firstLibrary.Parse(firstFormula.c_str());
  auto second = secondLibrary.Parse(secondFormula.c_str());

  if (first.GetStatus() != qin::JianziStatus::Renderable ||
      second.GetStatus() != qin::JianziStatus::Renderable) {
    std::cerr << "A known formula was not marked renderable.\n";
    return 1;
  }

  constexpr auto fallbackName = u8"😀";
  const auto fallbackFormula = firstLibrary.ParseNatural(fallbackName);
  const auto fallback = firstLibrary.Parse(fallbackFormula.c_str());
  if (fallbackFormula != fallbackName || fallback.GetStatus() != qin::JianziStatus::Fallback ||
      fallback.GetFallbackName() != fallbackName || !fallback.GetMissingNames().empty() ||
      !fallback.RenderPath().empty()) {
    std::cerr << "A single unknown character did not produce a fallback result.\n";
    return 1;
  }

  const auto aliasFallback = fallbackLibrary.Parse("fallback-test-alias");
  if (aliasFallback.GetStatus() != qin::JianziStatus::Fallback ||
      aliasFallback.GetFallbackName() != fallbackName) {
    std::cerr << "An alias targeting an unknown character did not preserve the fallback target.\n";
    return 1;
  }

  const auto missing = firstLibrary.Parse(("(" + firstFormula + ")/" + fallbackName).c_str());
  if (missing.GetStatus() != qin::JianziStatus::Missing || missing.GetMissingNames().size() != 1 ||
      missing.GetMissingNames().front() != fallbackName || !missing.GetFallbackName().empty() ||
      !missing.RenderPath().empty()) {
    std::cerr << "An unknown character in a composition did not produce a missing result.\n";
    return 1;
  }

  if (firstLibrary.Parse(" ").GetStatus() != qin::JianziStatus::Empty) {
    std::cerr << "An empty formula was not marked empty.\n";
    return 1;
  }

  bool invalidFormulaRejected = false;
  try {
    static_cast<void>(firstLibrary.Parse(u8"😀/"));
  } catch (const std::invalid_argument&) {
    invalidFormulaRejected = true;
  }
  if (!invalidFormulaRejected) {
    std::cerr << "An invalid formula was not rejected.\n";
    return 1;
  }

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
