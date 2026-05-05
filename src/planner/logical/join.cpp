#include "planner/query_planner.hpp"

namespace graph::logical {

namespace {
BuildPhysicalType merge_build(
        const std::vector<String>& left_aliases, const std::vector<String>& right_aliases,
        BuildPhysicalType& left_build, BuildPhysicalType& right_build,
        exec::ExecContext& ctx,
        optimizer::CostModel* cost_model,
        storage::GraphDB* db,
        const std::unique_ptr<ast::Expr>& predicate) {
  auto [hash_join_cost, left_keys_expr, right_keys_expr, parent_filter_expr] =
    optimizer::EstimateHashJoin(left_aliases, right_aliases, predicate, ctx, cost_model, db, left_build.second, right_build.second);

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

LogicalJoin::LogicalJoin(LogicalOpPtr left, LogicalOpPtr right) {
  children.emplace_back(std::move(left));
  children.emplace_back(std::move(right));
}

LogicalJoin::LogicalJoin(LogicalOpPtr left, LogicalOpPtr right, std::unique_ptr<ast::Expr> predicate_):
  LogicalJoin(std::move(left), std::move(right)) {
  predicate = std::move(predicate_);
}

LogicalJoin::LogicalJoin(std::vector<LogicalOpPtr> children):
  LogicalOpManyChildren(std::move(children)) {};

LogicalJoin::LogicalJoin(
  std::vector<LogicalOpPtr> children,
  std::unique_ptr<ast::Expr> predicate) :
  LogicalOpManyChildren(std::move(children)),
  predicate(std::move(predicate)) {
}

BuildPhysicalType LogicalJoin::BuildPhysical(
  exec::ExecContext& ctx,
  optimizer::CostModel* cost_model,
  storage::GraphDB* db) const {

  std::vector<String> collected_aliases = children[0]->GetSubtreeAliases();
  auto cur_build_plan = children[0]->BuildPhysical(ctx, cost_model, db);
  for (size_t i = 1; i < children.size(); ++i) {
    auto curr_child_build = children[i]->BuildPhysical(ctx, cost_model, db);

    auto right_aliases = children[i]->GetSubtreeAliases();
    cur_build_plan = std::move(merge_build(
      collected_aliases, right_aliases,
      cur_build_plan, curr_child_build,
      ctx, cost_model, db, predicate
    ));
    collected_aliases.insert(collected_aliases.end(), std::make_move_iterator(right_aliases.begin()), std::make_move_iterator(right_aliases.end()));
  }
  return cur_build_plan;
}

[[nodiscard]] std::vector<String> LogicalJoin::GetSubtreeAliases() const {
  std::vector<String> result;
  for (const auto& child : children) {
    std::vector<String> cur_aliases = child->GetSubtreeAliases();
    result.insert(result.end(), std::make_move_iterator(cur_aliases.begin()), std::make_move_iterator(cur_aliases.end()));
  }

  return result;
}

}