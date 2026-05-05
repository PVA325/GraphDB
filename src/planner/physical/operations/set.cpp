#include "planner/query_planner.hpp"

namespace graph::exec {

PhysicalSetOp::PhysicalSetOp(PhysicalOpPtr child, std::vector<logical::LogicalSet::Assignment> assignments) :
  PhysicalOpUnaryChild(std::move(child)),
  assignments(std::move(assignments)) {
}

RowCursorPtr PhysicalSetOp::open(struct ExecContext& ctx) {
  return std::make_unique<SetCursor>(
    std::move(child->open(ctx)),
    assignments,
    ctx.db
  );
}

}