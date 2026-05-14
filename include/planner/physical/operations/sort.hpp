#pragma once

#include "physical_op_unary_child.hpp"

namespace graph::exec {
struct PhysicalSortOp : PhysicalOpUnaryChild {
  std::vector<ast::OrderItem> items;
  PhysicalSortOp(PhysicalOpPtr child, std::vector<ast::OrderItem> items);

  RowCursorPtr open(ExecContext& ctx) override;
  [[nodiscard]] String DebugString() const override;

  ~PhysicalSortOp() override = default;
};
}
