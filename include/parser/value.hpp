#pragma once

#include <string>
#include <variant>

namespace ast {

using Value = std::variant<int64_t, double, std::string, bool>;

// Literal value
struct Literal {
  Value value;

  std::string DebugString() const;
};

} // namespace ast
