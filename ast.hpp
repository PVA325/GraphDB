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
  virtual std::string Debug() const = 0;
};

using ExprPtr = std::unique_ptr<Expr>;

// Literal value
struct Literal {
  Value value;

  std::string Debug() const;
};

// Expression representing a literal
struct LiteralExpr : Expr {
  Literal literal;

  Value operator()(const EvalContext& ctx) const override;

  std::string Debug() const override;
};

// Access to a property: alias.property
struct PropertyExpr : Expr {
  std::string alias;
  std::string property;

  Value operator()(const EvalContext& ctx) const override;

  std::string Debug() const override;
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

  std::string Debug() const override;
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

  std::string Debug() const override;
};

// Node pattern in MATCH clause
struct NodePattern {
  std::string alias;
  std::vector<std::string> labels;

  size_t line = 0;
  size_t col = 0;

  std::string Debug() const;
};

// Edge direction
enum class EdgeDirection {
  Left,
  Right,
  Undirected
};

// Edge pattern between nodes
struct EdgePattern {
  std::string alias;
  std::vector<std::string> labels;
  EdgeDirection direction;

  size_t line = 0;
  size_t col = 0;

  std::string Debug() const;
};

// Node or Edge pattern element
struct PatternElement {
  std::variant<NodePattern, EdgePattern> element;

  bool IsNode() const;
  bool IsEdge() const;
  const NodePattern& AsNode() const;
  const EdgePattern& AsEdge() const;

  std::string Debug() const;
};

// Full graph pattern
struct Pattern {
  std::vector<PatternElement> elements;

  std::string Debug() const;
};

// MATCH clause
struct MatchClause {
  std::vector<Pattern> patterns;

  std::string Debug() const;
};

// WHERE clause
struct WhereClause {
  ExprPtr expression;

  std::string Debug() const;
};

// RETURN item: either alias or property
struct ReturnItem {
  std::variant<std::string, PropertyExpr> item;

  std::string Debug() const;
};

// RETURN clause
struct ReturnClause {
  std::vector<ReturnItem> items;

  std::string Debug() const;
};

// DELETE clause
struct DeleteClause {
  std::vector<std::string> aliases;

  std::string Debug() const;
};

// SET clause
struct SetClause {
  PropertyExpr target;
  Literal value;

  std::string Debug() const;
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

  std::string Debug() const;
};

// ORDER BY clause
struct OrderClause {
  std::vector<OrderItem> items;

  std::string Debug() const;
};

// LIMIT clause
struct LimitClause {
  size_t limit;

  std::string Debug() const;
};

// CREATE clause
struct CreateClause {
  std::vector<Pattern> patterns;

  std::string Debug() const;
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

  std::string Debug() const;
};

}  // namespace ast

namespace parser {

// Parse a single query
ast::QueryAST ParseSingle(const std::string& source);

// Parse multiple queries
std::vector<ast::QueryAST> Parse(const std::string& source);

}  // namespace parser