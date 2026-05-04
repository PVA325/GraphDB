#ifndef GRAPHDB_LOGICAL_SET_HPP
#define GRAPHDB_LOGICAL_SET_HPP

#include "logical_op_unary_child.hpp"

namespace graph::logical {
struct LogicalSet : LogicalOpUnaryChild {
  /// Create LogicalSet; can set only 1 parameter through execution
  struct Assignment {
    String alias;
    std::vector<String> labels;
    std::vector<std::pair<String, Value>> properties;
  };

  std::vector<Assignment> assignment;

  LogicalSet(const LogicalSet&) = delete;

  LogicalSet(LogicalSet&& other) noexcept : LogicalOpUnaryChild(std::move(other.child)),
                                            assignment(std::move(other.assignment)) {
  }

  LogicalSet(LogicalOpPtr child, std::vector<Assignment> assignment);
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model,
                                  storage::GraphDB* db) const override;

  [[nodiscard]] String DebugString() const override;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Set; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return child->GetSubtreeAliases(); }

  ~LogicalSet() override = default;
};
}

#endif //GRAPHDB_LOGICAL_SET_HPP
