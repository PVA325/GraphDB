#ifndef GRAPHDB_LOGICAL_DELETE_HPP
#define GRAPHDB_LOGICAL_DELETE_HPP

#include "logical_op_unary_child.hpp"

namespace graph::logical {
struct LogicalDelete : LogicalOpUnaryChild {
  /// Create logical operation in a tree to delete a target
  std::vector<String> target;

  LogicalDelete(const LogicalDelete&) = delete;

  LogicalDelete(LogicalDelete&& other) noexcept : LogicalOpUnaryChild(std::move(other.child)),
                                                  target(std::move(other.target)) {
  }

  LogicalDelete(LogicalOpPtr child, std::vector<String> target);

  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model,
                                  storage::GraphDB* db) const override;

  [[nodiscard]] String DebugString() const override;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Delete; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return child->GetSubtreeAliases(); }

  ~LogicalDelete() override = default;
};
}

#endif //GRAPHDB_LOGICAL_DELETE_HPP
