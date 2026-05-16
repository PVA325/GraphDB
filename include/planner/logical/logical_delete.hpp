#pragma once

#include "logical_op_unary_child.hpp"

namespace graph::logical {
struct LogicalDelete : LogicalOpUnaryChild {
  /// Create logical operation in a tree to delete a target
  LogicalDelete(const LogicalDelete&) = delete;

  LogicalDelete(LogicalDelete&& other) noexcept : LogicalOpUnaryChild(std::move(other.child)),
                                                  target(std::move(other.target)) {
  }

  LogicalDelete(LogicalOpPtr child, std::vector<String> target);

  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model,
                                  storage::GraphDB* db) const override;

  [[nodiscard]] String DebugString() const override;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::kDeleteType; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return child->GetSubtreeAliases(); }

  ~LogicalDelete() override = default;

public:
  std::vector<String> target;
};
}
