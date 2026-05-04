#include "planner/query_planner.hpp"

namespace graph::exec {

LimitOp::LimitOp(graph::Int limit_size, PhysicalOpPtr child) :
  PhysicalOpUnaryChild(std::move(child)),
  limit_size(limit_size) {
}

RowCursorPtr LimitOp::open(ExecContext& ctx) {
  return std::make_unique<LimitCursor>(
    std::move(child->open(ctx)),
    limit_size
  );
}

}