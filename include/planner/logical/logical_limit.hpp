#ifndef GRAPHDB_LOGICAL_LIMIT_HPP
#define GRAPHDB_LOGICAL_LIMIT_HPP

#include "logical_op_unary_child.hpp"

namespace graph::logical {
struct LogicalLimit : public LogicalOpUnaryChild {
  /// Logical Limit - nothing to comment
  size_t limit_size;

  LogicalLimit() = delete;

  explicit LogicalLimit(LogicalOpPtr child, size_t limit_size);

  [[nodiscard]] String DebugString() const override;
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model,
                                  storage::GraphDB* db) const override;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Limit; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return child->GetSubtreeAliases(); }

  ~LogicalLimit() override = default;
};
}

#endif //GRAPHDB_LOGICAL_LIMIT_HPP
