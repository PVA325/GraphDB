#ifndef GRAPHDB_SORT_HPP
#define GRAPHDB_SORT_HPP

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

#endif //GRAPHDB_SORT_HPP