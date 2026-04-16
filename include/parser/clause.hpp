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
  std::vector<Pattern> patterns;

  std::string DebugString() const;
};

// WHERE clause.
struct WhereClause {
  ExprPtr expression;

  std::string DebugString() const;
};

// RETURN item: either alias or property.
struct ReturnItem {
  std::variant<std::string, PropertyExpr> return_item;

  std::string DebugString() const;
};

// RETURN clause.
struct ReturnClause {
  std::vector<ReturnItem> items;

  std::string DebugString() const;
};

// DELETE clause.
struct DeleteClause {
  std::vector<std::string> aliases;

  std::string DebugString() const;
};

// SET item.
struct SetItem {
  std::string alias;
  std::vector<std::pair<std::string, Literal>> properties;
  std::vector<std::string> labels;

  std::string DebugString() const;
};

// SET clause.
struct SetClause {
  std::vector<SetItem> items;

  std::string DebugString() const;
};

// ORDER BY direction.
enum class OrderDirection {
  Asc,
  Desc
};

// ORDER BY item.
struct OrderItem {
  PropertyExpr property;
  OrderDirection direction = OrderDirection::Asc;

  std::string DebugString() const;
};

// ORDER BY clause.
struct OrderClause {
  std::vector<OrderItem> items;

  std::string DebugString() const;
};

// LIMIT clause.
struct LimitClause {
  size_t limit;

  std::string DebugString() const;
};

using CreateItem = std::variant<NodePattern, CreateEdgePattern>;

// CREATE clause.
struct CreateClause {
  std::vector<CreateItem> created_items;

  std::string DebugString() const;
};

// AST for query.
struct QueryAST {
  std::unique_ptr<MatchClause> match_clause;
  std::unique_ptr<WhereClause> where_clause;

  std::unique_ptr<ReturnClause> return_clause;
  std::unique_ptr<DeleteClause> delete_clause;
  std::unique_ptr<SetClause> set_clause;

  std::unique_ptr<OrderClause> order_clause;
  std::unique_ptr<LimitClause> limit_clause;

  std::unique_ptr<CreateClause> create_clause;

  std::string DebugString() const;
};

} // namespace ast
