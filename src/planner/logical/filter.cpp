#include "planner/query_planner.hpp"

namespace graph::logical {
LogicalFilter::LogicalFilter(LogicalOpPtr child, std::unique_ptr<ast::Expr> predicate) :
  LogicalOpUnaryChild(std::move(child)),
  predicate(std::move(predicate)) {
}
LogicalFilter::LogicalFilter(LogicalOpPtr child, ast::Expr* predicate) :
  LogicalOpUnaryChild(std::move(child)),
  predicate(predicate) {
}

BuildPhysicalType LogicalFilter::BuildPhysical(
  exec::ExecContext& ctx,
  optimizer::CostModel* cost_model,
  storage::GraphDB* db) const {
  auto child_build = child->BuildPhysical(ctx, cost_model, db);
  return std::make_pair(
    std::make_unique<exec::FilterOp>(
      predicate.get(),
      std::move(child_build.first)
    ),
    cost_model->EstimateFilter(db, child_build.second, predicate.get())
  );
}

}