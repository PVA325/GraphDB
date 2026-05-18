#pragma once

#include <string>
#include <variant>

namespace ast {

using Value = std::variant<int64_t, double, std::string, bool>;

// Literal value.
struct Literal {
  [[nodiscard]] std::string DebugString() const;

  Value value;
};

} // namespace ast
