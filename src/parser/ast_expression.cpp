#include "parser/parser.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "parser/error.hpp"
#include "parser/lexer.hpp"

namespace ast {

// LiteralExpr evaluation simply returns the literal value.
Value LiteralExpr::operator()(const EvalContext& ctx) const {
  return literal.value;
}

// Evaluation of property access expressions retrieves the property value from the context.
Value PropertyExpr::operator()(const EvalContext& ctx) const {
  return ctx.GetProperty(alias, property);
  // throw std::runtime_error("Not implemented");
}

// Evaluation of comparison expressions.
Value ComparisonExpr::operator()(const EvalContext& ctx) const {
  Value l = (*left_expr)(ctx);
  Value r = (*right_expr)(ctx);

  return CompareValues(l, r, op);
}

// Evaluation of logical expressions.
Value LogicalExpr::operator()(const EvalContext& ctx) const {
  bool lhs = ToBool((*left_expr)(ctx));

  if (op == LogicalOp::And) {
    if (!lhs) {
      return false;
    }

    return lhs && ToBool((*right_expr)(ctx));
  }

  if (op == LogicalOp::Or) {
    if (lhs) {
      return true;
    }

    return lhs || ToBool((*right_expr)(ctx));
  }

  throw std::runtime_error("Unknown logical operator");
}

// Type implementations for expressions.
ExprType LiteralExpr::Type() const {
  return ExprType::Literal;
}

ExprType PropertyExpr::Type() const {
  return ExprType::Property;
}

ExprType ComparisonExpr::Type() const {
  return ExprType::Comparison;
}

ExprType LogicalExpr::Type() const {
  return ExprType::Logical;
}

// Implementations of CollectAliases for each expression type.
std::string LiteralExpr::CollectAliases(std::vector<std::string>&) const override {
  return "";
}

std::string PropertyExpr::CollectAliases() const override {
  return alias;
}

std::string ComparisonExpr::CollectAliases() const override {
  std::string aliases;

  if (left_expr) {
    aliases += left_expr->CollectAliases() + " ";
  }

  if (right_expr) {
    aliases += right_expr->CollectAliases() + " ";
  }

  return aliases;
}

std::string LogicalExpr::CollectAliases() const override {
  std::string aliases;

  if (left_expr) {
    aliases += left_expr->CollectAliases() + " ";
  }

  if (right_expr) {
    aliases += right_expr->CollectAliases() + " ";
  }

  return aliases;
}

// Implementations of copy() for expressions to allow deep copying of expression trees.
Expr* LiteralExpr::copy() const override {
  return new LiteralExpr(*this);
}

Expr* PropertyExpr::copy() const override {
  return new PropertyExpr(*this);
}

Expr* ComparisonExpr::copy() const override {
  auto res = new ComparisonExpr();
  res->left_expr = left_expr ? left_expr->copy() : nullptr;
  res->op = op;
  res->right_expr = right_expr ? right_expr->copy() : nullptr;
  return res;
}

Expr* LogicalExpr::copy() const override {
  auto res = new LogicalExpr();
  res->left_expr = left_expr ? left_expr->copy() : nullptr;
  res->op = op;
  res->right_expr = right_expr ? right_expr->copy() : nullptr;
  return res;
}

}  // namespace ast