#include <iostream>
#include <stdexcept>
#include <utility>

#include "Jianzi.hpp"

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "Usage: jianzi-instance-test <library.cbor> <text>\n";
    return 1;
  }

  auto firstLibrary = qin::JianziLibrary::LoadFile(argv[1]);
  auto secondLibrary = qin::JianziLibrary::LoadFile(argv[1]);

  const auto firstFormula = firstLibrary.ParseNatural(argv[2]);
  const auto secondFormula = secondLibrary.ParseNatural(argv[2]);
  auto first = firstLibrary.Parse(firstFormula.c_str());
  auto second = secondLibrary.Parse(secondFormula.c_str());

  if (first.RenderPath().empty() || second.RenderPath().empty()) {
    std::cerr << "An independently loaded library failed to render.\n";
    return 1;
  }

  auto assigned = firstLibrary.Parse(firstFormula.c_str());
  assigned = first;
  if (assigned.RenderPath().empty()) {
    std::cerr << "Same-library assignment failed.\n";
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

  auto movedLibrary = std::move(firstLibrary);
  if (first.RenderPath().empty()) {
    std::cerr << "Moving the owning library invalidated its context address.\n";
    return 1;
  }

  return 0;
}
