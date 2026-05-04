#ifndef GRAPHDB_LOGICAL_FILTER_HPP
#define GRAPHDB_LOGICAL_FILTER_HPP

#include "logical_op_unary_child.hpp"

namespace graph::logical {

struct LogicalFilter : public LogicalOpUnaryChild {
  /// Filter - not include int answer all values that do not satisfy predicate
  std::unique_ptr<ast::Expr> predicate;

  LogicalFilter() = delete;

  explicit LogicalFilter(LogicalOpPtr child, std::unique_ptr<ast::Expr> predicate);
  explicit LogicalFilter(LogicalOpPtr child, ast::Expr* predicate);

  [[nodiscard]] String DebugString() const override;
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model, storage::GraphDB* db) const override;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Filter; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return child->GetSubtreeAliases(); }

  ~LogicalFilter() override = default;
};

}

#endif //GRAPHDB_LOGICAL_FILTER_HPP