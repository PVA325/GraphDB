#include "planner/query_planner.hpp"

namespace graph::exec {

PhysicalSortOp::PhysicalSortOp(PhysicalOpPtr child, std::vector<ast::OrderItem> items) :
  PhysicalOpUnaryChild(std::move(child)),
  items(std::move(items)) {
}

RowCursorPtr PhysicalSortOp::open(ExecContext& ctx) {
  return std::make_unique<SortCursor>(
    std::move(child->open(ctx)),
    items
  );
}

}