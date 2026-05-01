#pragma once
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include <unordered_map>
#include <numeric>
#include <format>
#include <functional>

#include "common/common_value.hpp"

#include "parser/ast.hpp"
#include "storage/storage.hpp"

#include "eval_context/eval_context.hpp"
#include "utils.hpp"
#include "logical_planner.hpp"
#include "physical_planner.hpp"
#include "optimizer.hpp"

namespace graph::planner {
// struct PlannerConfig {
//   // Planner configuration options
//   bool enable_hash_join = true;
//   bool enable_index_seek = true;
//   bool push_down_filters = true;
// };


class Planner {
public:
  explicit Planner(const exec::ExecContext& ctx, storage::GraphDB* db, ast::QueryAST ast) :
    ctx_(ctx), ast_plan_(std::move(ast)), db_(db) {
    cost_model_ = std::make_unique<optimizer::DefaultCostModel>();
  }

  // End-to-end: AST -> LogicalPlan
  void build_logical_plan();

  optimizer::CostEstimate build_physical_plan();

  logical::LogicalPlan& getLogicalPlan() { return logical_plan_; }
  exec::PhysicalPlan& getPhysicalPlan() { return physical_plan_; }

  // Optimizations on logical plan (predicate pushdown, flattening)
  void optimize_logical_plan();

  // For each logical scan enumerate alternatives (index vs scan)
  // returns vector of alternative LogicalScan nodes (candidates)
  [[nodiscard]] std::vector<logical::LogicalScan> enumerate_scan_alternatives(const logical::LogicalScan& scan) const;

  // chooses join order (returns permutation indices)
  [[nodiscard]] std::vector<size_t> choose_join_order(const std::vector<logical::LogicalOp*>& joins) const;

  // Map logical -> physical (core)
  [[nodiscard]] exec::PhysicalPlan map_logical_to_physical(const logical::LogicalPlan& logical_plan,
                                                           const exec::ExecOptions& opts) const;

  // Final physical optimizations (push limits, top-k)
  [[nodiscard]] exec::PhysicalPlan finalize_physical_plan(exec::PhysicalPlan plan,
                                                          const exec::ExecOptions& opts) const;

  // Full pipeline: AST -> PhysicalPlan
  [[nodiscard]] exec::PhysicalPlan build_execution_plan(const ast::QueryAST& ast,
                                                        const exec::ExecOptions& opts) const;

  // Debug / explain helpers
  [[nodiscard]] String explain_logical_plan(const logical::LogicalPlan& plan) const;
  [[nodiscard]] String explain_physical_plan(const exec::PhysicalPlan& plan) const;

private:

  exec::ExecContext ctx_;
  ast::QueryAST ast_plan_;
  logical::LogicalPlan logical_plan_;
  exec::PhysicalPlan physical_plan_;
  std::unique_ptr<optimizer::CostModel> cost_model_;
  storage::GraphDB* db_;
  // PlannerConfig cfg_;

  // std::unique_ptr<JoinOrderStrategy> join_strategy_;
};
} // namespace graph::planner

