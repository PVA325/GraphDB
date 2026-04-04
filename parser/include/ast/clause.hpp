#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "value.hpp"
#include "expression.hpp"
#include "pattern.hpp"

namespace ast {

// MATCH clause
struct MatchClause {
  std::vector<Pattern> patterns;

  std::string DebugString() const;
};

// WHERE clause
struct WhereClause {
  ExprPtr expression;

  std::string DebugString() const;
};

// RETURN item: either alias or property
struct ReturnItem {
  std::variant<std::string, PropertyExpr> item;

  std::string DebugString() const;
};

// RETURN clause
struct ReturnClause {
  std::vector<ReturnItem> items;

  std::string DebugString() const;
};

// DELETE clause
struct DeleteClause {
  std::vector<std::string> aliases;

  std::string DebugString() const;
};

// SET clause
struct SetClause {
  PropertyExpr target;
  Literal value;

  std::string DebugString() const;
};

// ORDER BY direction
enum class OrderDirection {
  Asc,
  Desc
};

// ORDER BY item
struct OrderItem {
  PropertyExpr property;
  OrderDirection direction = OrderDirection::Asc;

  std::string DebugString() const;
};

// ORDER BY clause
struct OrderClause {
  std::vector<OrderItem> items;

  std::string DebugString() const;
};

// LIMIT clause
struct LimitClause {
  size_t limit;

  std::string DebugString() const;
};

// Node reference in CREATE clause
struct CreateNodeRef {
  std::string alias;

  size_t line = 0;
  size_t col = 0;

  std::string DebugString() const;
};

// CREATE edge pattern
struct CreateEdgePattern {
  CreateNodeRef left_node;
  std::string alias;
  std::vector<std::string> labels;
  PropertyMap properties;
  EdgeDirection direction;
  CreateNodeRef right_node;

  size_t line = 0;
  size_t col = 0;

  std::string DebugString() const;
};

using CreateItem = std::variant<NodePattern, CreateEdgePattern>;

// CREATE clause
struct CreateClause {
  std::vector<CreateItem> created_items;

  std::string DebugString() const;
};

// AST for query
struct QueryAST {
  std::unique_ptr<MatchClause> match;
  std::unique_ptr<WhereClause> where;

  std::unique_ptr<ReturnClause> return_clause;
  std::unique_ptr<DeleteClause> delete_clause;
  std::unique_ptr<SetClause> set_clause;

  std::unique_ptr<OrderClause> order;
  std::unique_ptr<LimitClause> limit;

  std::unique_ptr<CreateClause> create;

  std::string DebugString() const;
};

} // namespace ast
