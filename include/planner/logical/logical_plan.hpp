#ifndef GRAPHDB_LOGICAL_PLAN_HPP
#define GRAPHDB_LOGICAL_PLAN_HPP

#include "logical_op_zero_child.hpp"

namespace graph::logical {
struct LogicalPlan : LogicalOpZeroChild {
  /// Container logical plan
  LogicalOpPtr root;

  LogicalPlan() : root(nullptr) {
  }

  LogicalPlan(LogicalPlan&& other) noexcept : root(std::move(other.root)) {
  }

  LogicalPlan& operator=(LogicalPlan&& other) noexcept {
    root = std::move(other.root);
    return *this;
  }

  explicit LogicalPlan(LogicalOpPtr r) : root(std::move(r)) {
  }

  [[nodiscard]] String DebugString() const override { return "LogicalPlan:"; }
  [[nodiscard]] String SubtreeDebugString() const override { return root ? root->SubtreeDebugString() : "<empty>"; }

  BuildPhysicalType
  BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model, storage::GraphDB* db) const override {
    return root->BuildPhysical(ctx, cost_model, db);
  }

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Plan; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return root->GetSubtreeAliases(); }

  ~LogicalPlan() override = default;
};
}

#endif //GRAPHDB_LOGICAL_PLAN_HPP
