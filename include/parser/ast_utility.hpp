#pragma once

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "expression.hpp"
#include "pattern.hpp"
#include "value.hpp"
#include "parser/error.hpp"
#include "parser/lexer.hpp"

namespace ast {

std::string EscapeString(std::string_view text);
std::string ValueToString(const Value& value);
std::string PropertyMapToString(const PropertyMap& properties);
bool CompareValues(const Value& lhs, const Value& rhs, CompareOp op);
std::string CompareOpToString(CompareOp op);
std::string LabelsToString(const std::vector<std::string>& labels);
std::string EdgeLabelsToString(const std::string& labels);
std::string LogicalOpToString(LogicalOp op);
bool ToBool(const Value& v);

}  // namespace ast