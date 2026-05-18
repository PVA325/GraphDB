#pragma once

#include "logical_op_many_children.hpp"

namespace graph::logical {
struct LogicalJoin : LogicalOpManyChildren {
  /// Logical Join for many results with predicate(optional)
  LogicalJoin() = delete;

  LogicalJoin(LogicalOpPtr left, LogicalOpPtr right);
  LogicalJoin(LogicalOpPtr left, LogicalOpPtr right, std::unique_ptr<ast::Expr> predicate);
  LogicalJoin(std::vector<LogicalOpPtr> children);

  LogicalJoin(std::vector<LogicalOpPtr> children, std::unique_ptr<ast::Expr> predicate);
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model,
                                  storage::GraphDB* db) const override;


  [[nodiscard]] String DebugString() const override;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::kJoinType; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final;

  ~LogicalJoin() override = default;

public:
  std::unique_ptr<ast::Expr> predicate;
};
}
