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
    optimizer::EstimateHashJoin(left_aliases, right_aliases, predicate, ctx, cost_model, db, left_build.second,
                                right_build.second);

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

void FindFirstPairToJoin(
  const std::unique_ptr<ast::Expr>& predicate, std::vector<BuildPhysicalType>& builds,
  std::vector<std::vector<String>>& aliases,
  exec::ExecContext& ctx, optimizer::CostModel* cost_model, storage::GraphDB* db) {
  optimizer::CostEstimate first_pair_cost = optimizer::CostEstimate::GetMaxCostEstimate();
  std::pair<size_t, size_t> first_pair_idx = {0, 1};
  for (size_t i = 0; i < builds.size(); ++i) {
    for (size_t j = i + 1; j < builds.size(); ++j) {
      auto [hash_join_cost, _0, _1, _2] =
        optimizer::EstimateHashJoin(aliases[i], aliases[j], predicate, ctx, cost_model, db, builds[i].second,
                                    builds[j].second);

      optimizer::CostEstimate dummy_join_cost = cost_model->EstimateNestedJoin(
        db, builds[i].second, builds[j].second, predicate.get()
      );

      double cost = std::min(hash_join_cost.total(), dummy_join_cost.total());
      if (cost < first_pair_cost.total()) {
        first_pair_cost = (hash_join_cost.total() < dummy_join_cost.total() ? hash_join_cost : dummy_join_cost);
        first_pair_idx = {std::min(i, j), std::max(i, j)};
      }
    }
  }

  std::swap(builds[first_pair_idx.first], builds[0]);
  std::swap(aliases[first_pair_idx.first], aliases[0]);

  std::swap(builds[first_pair_idx.second], builds[1]);
  std::swap(aliases[first_pair_idx.second], aliases[1]);
}
void FindNewToJoin(size_t start_search_idx,
  const BuildPhysicalType& curr_join_build, const std::vector<String>& collected_aliases,
  const std::unique_ptr<ast::Expr>& predicate, std::vector<BuildPhysicalType>& builds,
  std::vector<std::vector<String>>& aliases,
  exec::ExecContext& ctx, optimizer::CostModel* cost_model, storage::GraphDB* db) {

  size_t children_cnt = builds.size();
  optimizer::CostEstimate min_cost = optimizer::CostEstimate::GetMaxCostEstimate();
  size_t best_idx = start_search_idx;
  for (size_t i = start_search_idx; i < children_cnt; ++i) {

    auto [hash_join_cost, _0, _1, _2] =
      optimizer::EstimateHashJoin(collected_aliases, aliases[i], predicate, ctx, cost_model, db, curr_join_build.second, builds[i].second);

    optimizer::CostEstimate dummy_join_cost = cost_model->EstimateNestedJoin(
      db, curr_join_build.second, builds[i].second, predicate.get()
    );

    double cost = std::min(hash_join_cost.total(), dummy_join_cost.total());
    if (cost < min_cost.total()) {
      min_cost = (hash_join_cost.total() < dummy_join_cost.total() ? hash_join_cost : dummy_join_cost);
      best_idx = i;
    }
  }

  std::swap(builds[best_idx], builds[start_search_idx]);
  std::swap(aliases[best_idx], aliases[start_search_idx]);
}
}

LogicalJoin::LogicalJoin(LogicalOpPtr left, LogicalOpPtr right) {
  children.emplace_back(std::move(left));
  children.emplace_back(std::move(right));
}

LogicalJoin::LogicalJoin(LogicalOpPtr left, LogicalOpPtr right, std::unique_ptr<ast::Expr> predicate_) :
  LogicalJoin(std::move(left), std::move(right)) {
  predicate = std::move(predicate_);
}

LogicalJoin::LogicalJoin(std::vector<LogicalOpPtr> children) :
  LogicalOpManyChildren(std::move(children)) {
};

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
  std::vector<BuildPhysicalType> builds;
  std::vector<std::vector<String>> aliases;
  builds.reserve(children.size());
  aliases.reserve(children.size());

  for (const auto& child : children) {
    builds.emplace_back(child->BuildPhysical(ctx, cost_model, db));
    aliases.emplace_back(std::move(child->GetSubtreeAliases()));
  }

  FindFirstPairToJoin(predicate, builds, aliases, ctx, cost_model, db);

  auto cur_build_plan = std::move(builds[0]);
  std::vector<String> collected_aliases = aliases[0];
  for (size_t i = 1; i < children.size(); ++i) {
    auto curr_child_build = std::move(builds[i]);

    auto right_aliases = children[i]->GetSubtreeAliases();
    cur_build_plan = std::move(merge_build(
      collected_aliases, right_aliases,
      cur_build_plan, curr_child_build,
      ctx, cost_model, db, predicate
    ));
    collected_aliases.insert(collected_aliases.end(), std::make_move_iterator(right_aliases.begin()),
                             std::make_move_iterator(right_aliases.end()));

    FindNewToJoin(i + 1, cur_build_plan, collected_aliases, predicate, builds, aliases, ctx, cost_model, db);
  }
  return cur_build_plan;
}

[[nodiscard]] std::vector<String> LogicalJoin::GetSubtreeAliases() const {
  std::vector<String> result;
  for (const auto& child : children) {
    std::vector<String> cur_aliases = child->GetSubtreeAliases();
    result.insert(result.end(), std::make_move_iterator(cur_aliases.begin()),
                  std::make_move_iterator(cur_aliases.end()));
  }

  return result;
}
}
