#include "parser/parser.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "parser/error.hpp"
#include "parser/lexer.hpp"

namespace ast {
namespace {

// Escapes special characters in a string for debug output.
std::string EscapeString(std::string_view text) {
  std::string result;
  result.reserve(text.size());

  for (char c : text) {
    switch (c) {
      case '\\': result += "\\\\"; break;
      case '"': result += "\\\""; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      default: result += c; break;
    }
  }

  return result;
}

// Converts a Value to a string for debug output.
std::string ValueToString(const Value& value) {
  return std::visit([](const auto& v) -> std::string {
    using T = std::decay_t<decltype(v)>;

    if constexpr (std::is_same_v<T, std::string>) {
      return '"' + EscapeString(v) + '"';
    } else if constexpr (std::is_same_v<T, bool>) {
      return v ? "true" : "false";
    } else {
      std::ostringstream out;
      out << v;
      return out.str();
    }
  }, value);
}

// Converts a PropertyMap to a string for debug output.
std::string PropertyMapToString(const PropertyMap& properties) {
  std::string result = "{";

  for (size_t i = 0; i < properties.size(); ++i) {
    result += properties[i].first;
    result += ": ";
    result += ValueToString(properties[i].second.value);
  
    if (i + 1 < properties.size()) {
      result += ", ";
    }
  }

  result += "}";
  return result;
}

// Converts a list of labels to a string for debug output.
std::string LabelsToString(const std::vector<std::string>& labels) {
  std::string result;

  for (const auto& label : labels) {
    result += ":" + label;
  }

  return result;
}

// Converts edge labels to a string for debug output.
std::string EdgeLabelsToString(const std::string& labels) {
  return labels.empty() ? std::string{} : ":" + labels;
}

// Converts a comparison operator to a string for debug output.
std::string CompareOpToString(CompareOp op) {
  switch (op) {
    case CompareOp::Eq: return "=";
    case CompareOp::NotEqual: return "!=";
    case CompareOp::Lt: return "<";
    case CompareOp::Le: return "<=";
    case CompareOp::Gt: return ">";
    case CompareOp::Ge: return ">=";
  }

  return "?";
}

// Compares two values with the given comparison operator.
bool CompareValues(const Value& lhs, const Value& rhs, CompareOp op) {
  return std::visit([&](auto&& l, auto&& r) -> bool {
    using L = std::decay_t<decltype(l)>;
    using R = std::decay_t<decltype(r)>;

    if constexpr ((std::is_same_v<L, int64_t> || std::is_same_v<L, double>) &&
                  (std::is_same_v<R, int64_t> || std::is_same_v<R, double>)) {
      double dl = static_cast<double>(l);
      double dr = static_cast<double>(r);

      switch (op) {
        case CompareOp::Eq: return dl == dr;
        case CompareOp::NotEqual: return dl != dr;
        case CompareOp::Lt: return dl < dr;
        case CompareOp::Le: return dl <= dr;
        case CompareOp::Gt: return dl > dr;
        case CompareOp::Ge: return dl >= dr;
      }
    }

    else if constexpr (std::is_same_v<L, std::string> &&
                       std::is_same_v<R, std::string>) {
      switch (op) {
        case CompareOp::Eq: return l == r;
        case CompareOp::NotEqual: return l != r;
        case CompareOp::Lt: return l < r;
        case CompareOp::Le: return l <= r;
        case CompareOp::Gt: return l > r;
        case CompareOp::Ge: return l >= r;
      }
    }

    else if constexpr (std::is_same_v<L, bool> &&
                       std::is_same_v<R, bool>) {
      switch (op) {
        case CompareOp::Eq: return l == r;
        case CompareOp::NotEqual: return l != r;
        default:
          throw std::runtime_error("Invalid comparison operator for boolean values");
      }
    }

    throw std::runtime_error("Cannot compare values of different or unsupported types");
  }, lhs, rhs);
}

// Converts a logical operator to a string for debug output.
std::string LogicalOpToString(LogicalOp op) {
  switch (op) {
    case LogicalOp::And: return "AND";
    case LogicalOp::Or: return "OR";
  }

  return "?";
}

// Converts a Value to a boolean for logical operations.
bool ToBool(const Value& v) {
  return std::visit([](auto&& val) -> bool {
    using T = std::decay_t<decltype(val)>;

    if constexpr (std::is_same_v<T, bool>) {
      return val;
    }

    if constexpr (std::is_same_v<T, int64_t>) {
      return val != 0;
    }

    if constexpr (std::is_same_v<T, double>) {
      return val != 0.0;
    }

    if constexpr (std::is_same_v<T, std::string>) {
      return !val.empty();
    }

    return false;
  }, v);
}

}  // namespace

}  // namespace ast