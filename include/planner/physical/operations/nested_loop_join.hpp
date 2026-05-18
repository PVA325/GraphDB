#pragma once


#include "physical_op_binary.hpp"

namespace graph::exec {
struct NestedLoopJoinOp : public PhysicalOpBinaryChild {
  /// do join for 2 expressions based on predicate
  NestedLoopJoinOp(PhysicalOpPtr left, PhysicalOpPtr right,
                   ast::Expr* pred);
  NestedLoopJoinOp(PhysicalOpPtr left, PhysicalOpPtr right);

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;
  ~NestedLoopJoinOp() override = default;

public:
  ast::Expr* predicate;
};
}
