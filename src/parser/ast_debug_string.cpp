#include "parser/parser.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "parser/ast_utility.hpp"
#include "parser/error.hpp"
#include "parser/lexer.hpp"

namespace ast {

// Debug string implementations for AST nodes.
std::string Literal::DebugString() const {
  return "Literal(" + ValueToString(value) + ")";
}

// Debug string implementations for expressions and patterns.
std::string LiteralExpr::DebugString() const {
  return literal.DebugString();
}

// Debug string for property access expressions.
std::string PropertyExpr::DebugString() const {
  return "Property(" + alias + "." + property + ")";
}

// Debug string for comparison expressions.
std::string ComparisonExpr::DebugString() const {
  return "Compare(" +
      (left_expr ? left_expr->DebugString() : std::string{"<null>"}) +
      " " +
      CompareOpToString(op) +
      " " +
      (right_expr ? right_expr->DebugString() : std::string{"<null>"}) +
      ")";
}

// Debug string for logical expressions.
std::string LogicalExpr::DebugString() const {
  return "Logical(" +
      (left_expr ? left_expr->DebugString() : std::string{"<null>"}) +
      " " +
      LogicalOpToString(op) +
      " " +
      (right_expr ? right_expr->DebugString() : std::string{"<null>"}) +
      ")";
}

// Debug string for node patterns.
std::string NodePattern::DebugString() const {
  std::string result = "(";

  if (!alias.empty()) {
    result += alias;
  }

  result += LabelsToString(labels);

  if (!properties.empty()) {
    if (!alias.empty() || !labels.empty()) {
      result += ' ';
    }

    result += PropertyMapToString(properties);
  }

  result += ")";
  return result;
}

std::string MatchEdgePattern::DebugString() const {
  std::string result;

  if (direction == EdgeDirection::Left) {
    result += "<-";
  } else {
    result += "-";
  }

  result += "[";

  if (!alias.empty()) {
    result += alias;
  }

  result += EdgeLabelsToString(label);

  if (!properties.empty()) {
    if (!alias.empty() || !label.empty()) {
      result += ' ';
    }

    result += PropertyMapToString(properties);

    result += "]";

    if (direction == EdgeDirection::Right) {
      result += "->";
    } else {
      result += "-";
    }
  }
  return result;
}

// Debug string for pattern elements.
std::string PatternElement::DebugString() const {
  return std::visit([](const auto& el) { return el.DebugString(); }, element);
}

std::string Pattern::DebugString() const {
  std::string result;

  for (const auto& el : elements) {
    result += el.DebugString();
  }

  return result;
}

// Debug string for Node References in CREATE clause.
std::string CreateNodeRef::DebugString() const {
  return "(" + alias + ")";
}

// Debug string for edge patterns in CREATE clause.
std::string CreateEdgePattern::DebugString() const {
  std::string result = left_node.DebugString();

  if (direction == EdgeDirection::Left) {
    result += "<-";
  } else {
    result += "-";
  }

  result += "[";

  if (!alias.empty()) {
    result += alias;
  }

  result += EdgeLabelsToString(label);

  if (!properties.empty()) {
    if (!alias.empty() || !label.empty()) {
      result += ' ';
    }

    result += PropertyMapToString(properties);

    result += "]";

    if (direction == EdgeDirection::Right) {
      result += "->";
    } else {
      result += "-";
    }

    result += right_node.DebugString();
  }
  return result;
}

// Debug string for MATCH clause.
std::string MatchClause::DebugString() const {
  std::string result = "MATCH ";

  for (size_t i = 0; i < patterns.size(); ++i) {
    result += patterns[i].DebugString();

    if (i + 1 < patterns.size()) {
      result += ", ";
    }
  }

  return result;
}

// Debug string for WHERE clause.
std::string WhereClause::DebugString() const {
  return expression ? "WHERE " + expression->DebugString() : "WHERE <null>";
}

// Debug string for RETURN items.
std::string ReturnItem::DebugString() const {
  if (std::holds_alternative<std::string>(return_item)) {
    return std::get<std::string>(return_item);
  }

  return std::get<PropertyExpr>(return_item).DebugString();
}

// Debug string for RETURN clause.
std::string ReturnClause::DebugString() const {
  std::string result = "RETURN ";

  for (size_t i = 0; i < items.size(); ++i) {
    result += items[i].DebugString();

    if (i + 1 < items.size()) {
      result += ", ";
    }
  }

  return result;
}

// Debug string for DELETE clause.
std::string DeleteClause::DebugString() const {
  std::string result = "DELETE ";

  for (size_t i = 0; i < aliases.size(); ++i) {
    result += aliases[i];

    if (i + 1 < aliases.size()) {
      result += ", ";
    }
  }

  return result;
}

// Debug string for SET item.
std::string SetItem::DebugString() const {
  std::string result = alias;

  for (const auto& [prop, value] : properties) {
    result += "." + prop + "=" + value.DebugString();
  }

  for (const auto& label : labels) {
    result += ":" + label;
  }

  return result;
}

// Debug string for SET clause.
std::string SetClause::DebugString() const {
  std::string result = "SET ";

  for (size_t i = 0; i < items.size(); ++i) {
    result += items[i].DebugString();

    if (i + 1 < items.size()) {
      result += ", ";
    }
  }

  return result;
}

// Debug string for ORDER BY item.
std::string OrderItem::DebugString() const {
  std::string result = property.DebugString();

  if (direction == OrderDirection::Desc) {
    result += " DESC";
  } else {
    result += " ASC";
  }

  return result;
}

// Debug string for ORDER BY clause.
std::string OrderClause::DebugString() const {
  std::string result = "ORDER BY ";

  for (size_t i = 0; i < items.size(); ++i) {
    result += items[i].DebugString();

    if (i + 1 < items.size()) {
      result += ", ";
    }
  }

  return result;
}

// Debug string for LIMIT clause.
std::string LimitClause::DebugString() const {
  return "LIMIT " + std::to_string(limit);
}

// Debug string for CREATE clause.
std::string CreateClause::DebugString() const {
  std::string result = "CREATE ";

  for (size_t i = 0; i < created_items.size(); ++i) {
    result += std::visit([](const auto& item) { return item.DebugString(); }, created_items[i]);

    if (i + 1 < created_items.size()) {
      result += ", ";
    }
  }

  return result;
}

// Debug string for the entire query AST.
std::string QueryAST::DebugString() const {
  std::string result;
  
  if (match_clause) {
    result += match_clause->DebugString() + "\n";
  }

  if (where_clause) {
    result += where_clause->DebugString() + "\n";
  }

  if (return_clause) {
    result += return_clause->DebugString() + "\n";
  }

  if (delete_clause) {
    result += delete_clause->DebugString() + "\n";
  }

  if (set_clause) {
    result += set_clause->DebugString() + "\n";
  }

  if (order_clause) {
    result += order_clause->DebugString() + "\n";
  }

  if (limit_clause) {
    result += limit_clause->DebugString() + "\n";
  }

  return result;
}

}  // namespace ast