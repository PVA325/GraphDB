#include "planner/query_planner.hpp"

namespace graph::logical {

LogicalJoin::LogicalJoin(LogicalOpPtr left, LogicalOpPtr right) :
  LogicalOpBinaryChild(std::move(left), std::move(right)), predicate{nullptr} {
}

LogicalJoin::LogicalJoin(
  LogicalOpPtr left, LogicalOpPtr right,
  std::unique_ptr<ast::Expr> predicate) :
  LogicalOpBinaryChild(std::move(left), std::move(right)),
  predicate(std::move(predicate)) {
}

BuildPhysicalType LogicalJoin::BuildPhysical(
  exec::ExecContext& ctx,
  optimizer::CostModel* cost_model,
  storage::GraphDB* db) const {
  auto left_build = left->BuildPhysical(ctx, cost_model, db);
  auto right_build = right->BuildPhysical(ctx, cost_model, db);

  auto [hash_join_cost, left_keys_expr, right_keys_expr, parent_filter_expr] =
    optimizer::EstimateHashJoin(this, ctx, cost_model, db, left_build.second, right_build.second);

  optimizer::CostEstimate dummy_join_cost = cost_model->EstimateNestedJoin(
    db, left_build.second, right_build.second, predicate.get()
  );
  if (hash_join_cost.total() > dummy_join_cost.total()) {
    return std::make_pair(
      std::make_unique<exec::NestedLoopJoinOp>(
        std::move(left_build.first),
        std::move(right_build.first),
        predicate.get()
      ),
      dummy_join_cost
    );
  }
  auto hash_join = std::make_unique<exec::HashJoinOp>(
    std::move(left_build.first),
    std::move(right_build.first),
  std::move(left_keys_expr),
  std::move(right_keys_expr)
  );
  if (parent_filter_expr == nullptr) {
    return std::make_pair(
      std::move(hash_join),
      hash_join_cost
    );
  }
  return std::make_pair(
  std::make_unique<exec::FilterOp>(
      std::move(parent_filter_expr),
      std::move(hash_join)
    ),
    hash_join_cost
  );
}

}