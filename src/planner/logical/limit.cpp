#include "planner/query_planner.hpp"

namespace graph::logical {

LogicalLimit::LogicalLimit(LogicalOpPtr child, size_t limit_size) :
  LogicalOpUnaryChild(std::move(child)),
  limit_size(limit_size) {
}

BuildPhysicalType LogicalLimit::BuildPhysical(
  exec::ExecContext& ctx,
  optimizer::CostModel* cost_model,
  storage::GraphDB* db) const {
  auto child_build = child->BuildPhysical(ctx, cost_model, db);
  return std::make_pair(
    std::make_unique<exec::LimitOp>(
      limit_size,
      std::move(child_build.first)
    ),
    cost_model->EstimateLimit(db, child_build.second, limit_size)
  );
}

}