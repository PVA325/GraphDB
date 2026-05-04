#include "planner/query_planner.hpp"

namespace graph::logical {
LogicalProject::LogicalProject(LogicalOpPtr child, std::vector<ast::ReturnItem> items) :
  LogicalOpUnaryChild(std::move(child)),
  items(std::move(items)) {
}

BuildPhysicalType LogicalProject::BuildPhysical(
  exec::ExecContext& ctx,
  optimizer::CostModel* cost_model,
  storage::GraphDB* db) const {
  auto child_build = child->BuildPhysical(ctx, cost_model, db);
  return std::make_pair(
    std::make_unique<exec::ProjectOp>(
      items,
      std::move(child_build.first)
      ),
      cost_model->EstimateProject(db, child_build.second, *this)
  );
}

}