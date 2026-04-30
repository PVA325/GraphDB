#pragma once

#include <memory>
#include <string>
#include <variant>
#include <unordered_map>

#include "value.hpp"


namespace ast {

struct Expr;
struct EvalContext;
// Expression that stores the aliases used in request.
struct ExprAnalysis {
  std::vector<std::string> aliases;

  void CollectAliases(const Expr* expr);
};

// Abstract class for all expressions in AST.
enum class ExprType {
  Literal,
  Property,
  Comparison,
  Logical
};

struct Expr {
  virtual ~Expr() = default;

  Expr* copy() const { throw std::runtime_error("Not implemented"); }

  // Evaluate expression.
  virtual Value operator()(const EvalContext& ctx) const = 0;

  [[nodiscard]] virtual ExprType Type() const {throw std::runtime_error("Not implemented");}

  [[nodiscard]] virtual std::vector<std::string> CollectAliases() const { throw std::runtime_error("Not implemented"); }

  // Debug string representation.
  virtual std::string DebugString() const = 0;
};

using ExprPtr = std::unique_ptr<Expr>;

// Expression representing a literal.
struct LiteralExpr : Expr {
  Literal literal;

  Value operator()(const EvalContext& ctx) const override;

  std::string DebugString() const override;
};

// Access to a property: alias.property.
struct PropertyExpr : Expr {
  std::string alias;
  std::string property;

  Value operator()(const EvalContext& ctx) const override;

  std::string DebugString() const override;
};

// Comparison operations.
enum class CompareOp {
  Eq,
  NotEqual,
  Gt,
  Ge,
  Lt,
  Le
};

// Comparison expression.
struct ComparisonExpr : Expr {
  ExprPtr left_expr;
  CompareOp op;
  ExprPtr right_expr;

  Value operator()(const EvalContext& ctx) const override;

  std::string DebugString() const override;
};

// Logical operations.
enum class LogicalOp {
  And,
  Or
};

// Logical expression (AND / OR).
struct LogicalExpr : Expr {
  ExprPtr left_expr;
  LogicalOp op;
  ExprPtr right_expr;

  LogicalExpr() = default;
  LogicalExpr(Expr* l, LogicalOp op, Expr* r): left_expr(l), op(op), right_expr(r) {}

  Value operator()(const EvalContext& ctx) const override;

  std::string DebugString() const override;
};

} // namespace ast
