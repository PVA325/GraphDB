#include "planner/query_planner.hpp"

namespace graph::logical {

LogicalDelete::LogicalDelete(LogicalOpPtr child, std::vector<String> target) :
  LogicalOpUnaryChild(std::move(child)),
  target(std::move(target)) {}

BuildPhysicalType LogicalDelete::BuildPhysical(
  exec::ExecContext& ctx,
  optimizer::CostModel* cost_model,
  storage::GraphDB* db) const {
  auto child_build = child->BuildPhysical(ctx, cost_model, db);

  return std::make_pair(
    std::make_unique<exec::PhysicalDeleteOp>(
      target,
      std::move(child_build.first)
    ),
    cost_model->EstimateDelete(db, child_build.second, *this)
  );
}

}