#ifndef GRAPHDB_LOGICAL_SORT_HPP
#define GRAPHDB_LOGICAL_SORT_HPP

#include "logical_op_unary_child.hpp"

namespace graph::logical {

struct LogicalSort : LogicalOpUnaryChild {
  /// Logical Sort - sort by expression in items
  /// for example
  /*
    {
      {expr1, true},   // ASC
      {expr2, false}   // DESC
    } and sort if by expr1 and if they are equal by expr2
   */
  std::vector<ast::OrderItem> items;

  LogicalSort() = delete;

  explicit LogicalSort(LogicalOpPtr child, std::vector<ast::OrderItem> items);

  [[nodiscard]] String DebugString() const override;
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model, storage::GraphDB* db) const override;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Sort; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return child->GetSubtreeAliases(); }

  ~LogicalSort() override = default;
};

}

#endif //GRAPHDB_LOGICAL_SORT_HPP