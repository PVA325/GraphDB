#pragma once

#include "logical_op_unary_child.hpp"

namespace graph::logical {

struct LogicalLimit : public LogicalOpUnaryChild {
  /// Logical Limit - nothing to comment
  LogicalLimit() = delete;

  explicit LogicalLimit(LogicalOpPtr child, size_t limit_size);

  [[nodiscard]] String DebugString() const override;

  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model,
                                  storage::GraphDB* db) const override;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::kLimitType; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return child->GetSubtreeAliases(); }

  ~LogicalLimit() override = default;

public:
  size_t limit_size;
};

} // namespace graph::logical
