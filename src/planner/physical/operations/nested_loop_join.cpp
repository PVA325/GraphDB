#include "planner/query_planner.hpp"

namespace graph::exec {

NestedLoopJoinOp::NestedLoopJoinOp(
  PhysicalOpPtr left,
  PhysicalOpPtr right,
  ast::Expr* pred) :
  PhysicalOpBinaryChild(std::move(left), std::move(right)),
  predicate(pred) {
}

NestedLoopJoinOp::NestedLoopJoinOp(
  PhysicalOpPtr left,
  PhysicalOpPtr right) :
  PhysicalOpBinaryChild(std::move(left), std::move(right)),
  predicate(nullptr) {
}

RowCursorPtr NestedLoopJoinOp::open(ExecContext& ctx) {
  return std::make_unique<NestedLoopJoinCursor>(
    left->open(ctx),
    right.get(),
    predicate,
    ctx
  );
}
}