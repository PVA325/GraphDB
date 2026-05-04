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
  virtual Expr* copy() const = 0;

  // Evaluate expression.
  virtual Value operator()(const EvalContext& ctx) const = 0;

  // Get the type of the expression.
  virtual ExprType Type() const = 0;

  // Collects all aliases used in the expression into the provided vector.
  virtual std::string CollectAliases() const = 0;

  // Debug string representation.
  virtual std::string DebugString() const = 0;
};

using ExprPtr = std::unique_ptr<Expr>;

// Expression representing a literal.
struct LiteralExpr : Expr {
  Literal literal;

  virtual LiteralExpr* copy() const;

  Value operator()(const EvalContext& ctx) const override;

  ExprType Type() const override;
  void CollectAliases(std::vector<std::string>& aliases) const override;
  std::unique_ptr<Expr> copy() const override;

  std::string DebugString() const override;
};

// Access to a property: alias.property.
struct PropertyExpr : Expr {
  std::string alias;
  std::string property;

  PropertyExpr() = default;

  virtual PropertyExpr* copy() const;

  Value operator()(const EvalContext& ctx) const override;

  ExprType Type() const override;
  void CollectAliases(std::vector<std::string>& aliases) const override;
  std::unique_ptr<Expr> copy() const override;

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

  ComparisonExpr() = default;
  ComparisonExpr(Expr* l, CompareOp op, Expr* r): left_expr(l), op(op), right_expr(r) {}

  virtual ComparisonExpr* copy() const;

  Value operator()(const EvalContext& ctx) const override;

  ExprType Type() const override;
  void CollectAliases(std::vector<std::string>& aliases) const override;
  std::unique_ptr<Expr> copy() const override;

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

  LogicalExpr(ExprPtr l, LogicalOp op, ExprPtr r): left_expr(std::move(l)), op(op), right_expr(std::move(r)) {}

  virtual LogicalExpr* copy() const;

  Value operator()(const EvalContext& ctx) const override;

  ExprType Type() const override;
  void CollectAliases(std::vector<std::string>& aliases) const override;
  std::unique_ptr<Expr> copy() const override;

  std::string DebugString() const override;
};

} // namespace ast
