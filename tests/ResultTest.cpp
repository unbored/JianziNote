#include <memory>
#include <string>

#include "Result.hpp"

int main() {
  auto success = qin::Result<std::unique_ptr<int>>::Success(std::make_unique<int>(42));
  if (!success || success.GetError() || !success.GetValue() || !*success.GetValue() ||
      **success.GetValue() != 42) {
    return 1;
  }

  auto failure = qin::Result<std::string>::Failure(
      {qin::JianziErrorCode::InvalidFormula, "Invalid formula."});
  if (failure || failure.GetValue() || !failure.GetError() ||
      failure.GetError()->code != qin::JianziErrorCode::InvalidFormula ||
      failure.GetError()->message != "Invalid formula.") {
    return 2;
  }

  const auto empty_success = qin::Result<void>::Success();
  if (!empty_success || empty_success.GetError()) return 3;

  const auto empty_failure = qin::Result<void>::Failure(
      {qin::JianziErrorCode::InvalidCbor, "Invalid CBOR."});
  if (empty_failure || !empty_failure.GetError() ||
      empty_failure.GetError()->code != qin::JianziErrorCode::InvalidCbor) {
    return 4;
  }

  return 0;
}
