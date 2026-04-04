#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ast {

using Value = std::variant<int64_t, double, std::string, bool>;

// Forward declaration of evaluation context
struct EvalContext;

// Abstract class for all expressions in AST
struct Expr {
  virtual ~Expr() = default;

  // Evaluate expression
  virtual Value operator()(const EvalContext& ctx) const = 0;

  // Debug string representation
  virtual std::string DebugString() const = 0;
};

using ExprPtr = std::unique_ptr<Expr>;

// Literal value
struct Literal {
  Value value;

  std::string DebugString() const;
};

// Expression representing a literal
struct LiteralExpr : Expr {
  Literal literal;

  Value operator()(const EvalContext& ctx) const override;

  std::string DebugString() const override;
};

// Access to a property: alias.property
struct PropertyExpr : Expr {
  std::string alias;
  std::string property;

  Value operator()(const EvalContext& ctx) const override;

  std::string DebugString() const override;
};

// Comparison operations
enum class CompareOp {
  Eq,
  Gt,
  Lt
};

// Comparison expression
struct ComparisonExpr : Expr {
  ExprPtr left;
  CompareOp op;
  ExprPtr right;

  Value operator()(const EvalContext& ctx) const override;

  std::string DebugString() const override;
};

// Logical operations
enum class LogicalOp {
  And,
  Or
};

// Logical expression (AND / OR)
struct LogicalExpr : Expr {
  ExprPtr left_expr;
  LogicalOp op;
  ExprPtr right_expr;

  Value operator()(const EvalContext& ctx) const override;

  std::string DebugString() const override;
};

using PropertyMap = std::vector<std::pair<std::string, Literal>>;

// Node pattern in MATCH clause
struct NodePattern {
  std::string alias;
  std::vector<std::string> labels;
  PropertyMap properties;

  size_t line = 0;
  size_t col = 0;

  std::string DebugString() const;
};

// Edge direction
enum class EdgeDirection {
  Left,
  Right,
  Undirected
};

// Edge pattern between nodes
struct MatchEdgePattern {
  std::string alias;
  std::vector<std::string> labels;
  PropertyMap properties;
  EdgeDirection direction;

  size_t line = 0;
  size_t col = 0;

  std::string DebugString() const;
};

using MatchItem = std::variant<NodePattern, MatchEdgePattern>;

// Node or Edge pattern element
struct PatternElement {
  MatchItem element;

  bool IsNode() const;
  bool IsEdge() const;
  const NodePattern& AsNode() const;
  const MatchEdgePattern& AsEdge() const;

  std::string DebugString() const;
};

// Full graph pattern
struct Pattern {
  std::vector<PatternElement> elements;

  std::string DebugString() const;
};

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

}  // namespace ast

namespace parser {

// Parse a single query
ast::QueryAST ParseSingle(const std::string& source);

// Parse multiple queries
std::vector<ast::QueryAST> Parse(const std::string& source);

}  // namespace parser