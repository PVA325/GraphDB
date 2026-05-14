#pragma once

#include "physical_op_unary_child.hpp"

namespace graph::exec {
struct FilterOp : public PhysicalOpUnaryChild {
  /// do Filter operation with predicate on Child like while (!predicate(child)) { child.next(row) }
  std::unique_ptr<ast::Expr> predicate_storage;
  ast::Expr* predicate;

  FilterOp(ast::Expr* predicate, PhysicalOpPtr child);
  FilterOp(std::unique_ptr<ast::Expr> predicate, PhysicalOpPtr child);

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;

  ~FilterOp() override = default;

private:
  String debugString_;
};
}
