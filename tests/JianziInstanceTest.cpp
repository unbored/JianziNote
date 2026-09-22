#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
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
  auto firstLoaded = qin::JianziLibrary::Load(libraryBytes.data(), libraryBytes.size());
  auto secondLoaded = qin::JianziLibrary::Load(libraryBytes.data(), libraryBytes.size());
  if (!firstLoaded || !secondLoaded) {
    std::cerr << "Unable to load the test library.\n";
    return 1;
  }
  const std::uint8_t invalidCbor[] = {0xff};
  const auto invalidCborResult = qin::JianziLibrary::Load(invalidCbor, sizeof(invalidCbor));
  if (invalidCborResult ||
      invalidCborResult.GetError()->code != qin::JianziErrorCode::InvalidCbor) {
    std::cerr << "Invalid CBOR input was not rejected.\n";
    return 1;
  }
  auto firstLibrary = std::move(*firstLoaded.GetValue());
  auto secondLibrary = std::move(*secondLoaded.GetValue());
  auto fallbackDocument = nlohmann::json::from_cbor(
      libraryBytes.begin(), libraryBytes.end(), true, false);
  if (fallbackDocument.is_discarded()) return 1;
  auto invalidDocument = fallbackDocument;
  invalidDocument["format_version"] = "invalid";
  const auto invalidBytes = nlohmann::json::to_cbor(invalidDocument);
  const auto invalidLibrary = qin::JianziLibrary::Load(invalidBytes.data(), invalidBytes.size());
  if (invalidLibrary ||
      invalidLibrary.GetError()->code != qin::JianziErrorCode::InvalidLibrary) {
    std::cerr << "Invalid library schema was not rejected.\n";
    return 1;
  }
  if (!fallbackDocument.contains("aliases")) fallbackDocument["aliases"] = nlohmann::json::array();
  fallbackDocument["aliases"].push_back(
      {{"alias", "fallback-test-alias"}, {"glyph", u8"😀"}, {"type", "Other"}});
  const auto fallbackBytes = nlohmann::json::to_cbor(fallbackDocument);
  auto fallbackLoaded = qin::JianziLibrary::Load(fallbackBytes.data(), fallbackBytes.size());
  if (!fallbackLoaded) return 1;
  auto fallbackLibrary = std::move(*fallbackLoaded.GetValue());
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
  if (!firstFormula || !secondFormula) return 1;
  auto firstParsed = firstLibrary.Parse(firstFormula.GetValue()->c_str());
  auto secondParsed = secondLibrary.Parse(secondFormula.GetValue()->c_str());
  if (!firstParsed || !secondParsed) return 1;
  auto first = std::move(*firstParsed.GetValue());
  auto second = std::move(*secondParsed.GetValue());

  if (first.GetStatus() != qin::JianziStatus::Renderable ||
      second.GetStatus() != qin::JianziStatus::Renderable) {
    std::cerr << "A known formula was not marked renderable.\n";
    return 1;
  }

  constexpr auto fallbackName = u8"😀";
  const auto fallbackFormula = firstLibrary.ParseNatural(fallbackName);
  if (!fallbackFormula) return 1;
  const auto fallbackParsed = firstLibrary.Parse(fallbackFormula.GetValue()->c_str());
  if (!fallbackParsed) return 1;
  const auto& fallback = *fallbackParsed.GetValue();
  const auto fallbackPaths = fallback.RenderPath();
  if (*fallbackFormula.GetValue() != fallbackName || fallback.GetStatus() != qin::JianziStatus::Fallback ||
      fallback.GetFallbackName() != fallbackName || !fallback.GetMissingNames().empty() ||
      !fallbackPaths || !fallbackPaths.GetValue()->empty()) {
    std::cerr << "A single unknown character did not produce a fallback result.\n";
    return 1;
  }

  const auto aliasParsed = fallbackLibrary.Parse("fallback-test-alias");
  if (!aliasParsed || aliasParsed.GetValue()->GetStatus() != qin::JianziStatus::Fallback ||
      aliasParsed.GetValue()->GetFallbackName() != fallbackName) {
    std::cerr << "An alias targeting an unknown character did not preserve the fallback target.\n";
    return 1;
  }

  const auto missingParsed =
      firstLibrary.Parse(("(" + *firstFormula.GetValue() + ")/" + fallbackName).c_str());
  if (!missingParsed) return 1;
  const auto& missing = *missingParsed.GetValue();
  const auto missingPaths = missing.RenderPath();
  if (missing.GetStatus() != qin::JianziStatus::Missing || missing.GetMissingNames().size() != 1 ||
      missing.GetMissingNames().front() != fallbackName || !missing.GetFallbackName().empty() ||
      !missingPaths || !missingPaths.GetValue()->empty()) {
    std::cerr << "An unknown character in a composition did not produce a missing result.\n";
    return 1;
  }

  const auto empty = firstLibrary.Parse(" ");
  if (!empty || empty.GetValue()->GetStatus() != qin::JianziStatus::Empty) {
    std::cerr << "An empty formula was not marked empty.\n";
    return 1;
  }

  const auto invalidFormula = firstLibrary.Parse(u8"😀/");
  if (invalidFormula || invalidFormula.GetError()->code != qin::JianziErrorCode::InvalidFormula) {
    std::cerr << "An invalid formula was not rejected.\n";
    return 1;
  }

  const auto firstPaths = first.RenderPath();
  const auto secondPaths = second.RenderPath();
  if (!firstPaths || !secondPaths || firstPaths.GetValue()->empty() || secondPaths.GetValue()->empty()) {
    std::cerr << "An independently loaded library failed to render.\n";
    return 1;
  }

  const auto copied = first;
  const auto copiedPaths = copied.RenderPath();
  if (!copiedPaths || !SamePaths(*firstPaths.GetValue(), *copiedPaths.GetValue())) {
    std::cerr << "Copy construction changed the rendered tree.\n";
    return 1;
  }

  auto assignedParsed = firstLibrary.Parse(firstFormula.GetValue()->c_str());
  if (!assignedParsed) return 1;
  auto assigned = std::move(*assignedParsed.GetValue());
  assigned = first;
  const auto assignedPaths = assigned.RenderPath();
  if (!assignedPaths || !SamePaths(*firstPaths.GetValue(), *assignedPaths.GetValue())) {
    std::cerr << "Copy assignment changed the rendered tree.\n";
    return 1;
  }

  const auto invalidComposition = first & second;
  if (invalidComposition ||
      invalidComposition.GetError()->code != qin::JianziErrorCode::ContextMismatch) {
    std::cerr << "Cross-library composition was not rejected.\n";
    return 1;
  }

  assigned = second;
  const auto unchangedPaths = assigned.RenderPath();
  if (!unchangedPaths || !SamePaths(*firstPaths.GetValue(), *unchangedPaths.GetValue())) {
    std::cerr << "Cross-library assignment changed the destination.\n";
    return 1;
  }

  auto movedLibrary = std::move(firstLibrary);
  const auto movedPaths = first.RenderPath();
  if (!movedPaths || movedPaths.GetValue()->empty()) {
    std::cerr << "Moving the owning library invalidated its context address.\n";
    return 1;
  }

  return 0;
}
