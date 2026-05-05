#include "planner/query_planner.hpp"

namespace graph::logical {

LogicalSet::LogicalSet(LogicalOpPtr child, std::vector<Assignment> assignment) :
  LogicalOpUnaryChild(std::move(child)),
  assignment(std::move(assignment)) {
}

BuildPhysicalType LogicalSet::BuildPhysical(
  exec::ExecContext& ctx,
  optimizer::CostModel* cost_model,
  storage::GraphDB* db) const {
  auto child_build = child->BuildPhysical(ctx, cost_model, db);

  return std::make_pair(
    std::make_unique<exec::PhysicalSetOp>(
      std::move(child_build.first),
      assignment
    ),
    cost_model->EstimateSet(db, child_build.second, *this)
  );
}

}