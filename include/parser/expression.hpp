#pragma once

#include <memory>
#include <string>
#include <variant>
#include <unordered_map>

#include "value.hpp"
#include "../eval_context/eval_context.hpp"

namespace ast {

struct Expr;
struct EvalContext;

// Abstract class for all expressions in AST.
enum class ExprType {
  Literal,
  Property,
  Comparison,
  Logical
};

struct Expr {
  virtual ~Expr() = default;

  // Creates a deep copy of the expression.
  virtual std::unique_ptr<Expr> copy() const = 0;

  // Evaluate expression.
  virtual Value operator()(const EvalContext& ctx) const = 0;

  // Get the type of the expression.
  virtual ExprType Type() const = 0;

  // Collects all aliases used in the expression into the provided vector.
  virtual void CollectAliases(std::vector<std::string>& aliases) const = 0;

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
