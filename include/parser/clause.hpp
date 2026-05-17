#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "value.hpp"
#include "expression.hpp"
#include "pattern.hpp"

namespace ast {

// MATCH clause.
struct MatchClause {
  [[nodiscard]] std::string DebugString() const;

  std::vector<Pattern> patterns;
};

// WHERE clause.
struct WhereClause {
  [[nodiscard]] std::string DebugString() const;

  ExprPtr expression;
};

// RETURN item: either alias or property.
struct ReturnItem {
  [[nodiscard]] std::string DebugString() const;

  std::variant<std::string, PropertyExpr> return_item;
};

// RETURN clause.
struct ReturnClause {
  [[nodiscard]] std::string DebugString() const;

  std::vector<ReturnItem> items;
};

// DELETE clause.
struct DeleteClause {
  [[nodiscard]] std::string DebugString() const;

  std::vector<std::string> aliases;
};

// SET item.
struct SetItem {
  [[nodiscard]] std::string DebugString() const;

  std::string alias;
  std::vector<std::pair<std::string, Literal>> properties;
  std::vector<std::string> labels;
};

// SET clause.
struct SetClause {
  [[nodiscard]] std::string DebugString() const;

  std::vector<SetItem> items;
};

// ORDER BY direction.
enum class OrderDirection {
  Asc,
  Desc
};

// ORDER BY item.
struct OrderItem {
  [[nodiscard]] std::string DebugString() const;

  PropertyExpr property;
  OrderDirection direction = OrderDirection::Asc;
};

// ORDER BY clause.
struct OrderClause {
  [[nodiscard]] std::string DebugString() const;

  std::vector<OrderItem> items;
};

// LIMIT clause.
struct LimitClause {
  [[nodiscard]] std::string DebugString() const;

  size_t limit;
};

using CreateItem = std::variant<NodePattern, CreateEdgePattern>;

// CREATE clause.
struct CreateClause {
  [[nodiscard]] std::string DebugString() const;

  std::vector<CreateItem> created_items;
};

// AST for query.
struct QueryAST {
  [[nodiscard]] std::string DebugString() const;

  std::unique_ptr<MatchClause> match_clause;
  std::unique_ptr<WhereClause> where_clause;

  std::unique_ptr<ReturnClause> return_clause;
  std::unique_ptr<DeleteClause> delete_clause;
  std::unique_ptr<SetClause> set_clause;

  std::unique_ptr<OrderClause> order_clause;
  std::unique_ptr<LimitClause> limit_clause;

  std::unique_ptr<CreateClause> create_clause;
};

} // namespace ast
