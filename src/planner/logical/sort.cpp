#include "planner/query_planner.hpp"

namespace graph::logical {

LogicalSort::LogicalSort(LogicalOpPtr child, std::vector<ast::OrderItem> items) :
  LogicalOpUnaryChild(std::move(child)),
  items(std::move(items)) {
}

BuildPhysicalType LogicalSort::BuildPhysical(
  exec::ExecContext& ctx,
  optimizer::CostModel* cost_model,
  storage::GraphDB* db) const {
  auto child_build = child->BuildPhysical(ctx, cost_model, db);
  return std::make_pair(
    std::make_unique<exec::PhysicalSortOp>(
      std::move(child_build.first),
      items
    ),
    cost_model->EstimateSort(db, child_build.second)
  );
}

}