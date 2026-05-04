#ifndef GRAPHDB_NESTED_LOOP_JOIN_HPP
#define GRAPHDB_NESTED_LOOP_JOIN_HPP

#include "physical_op_binary.hpp"

namespace graph::exec {
struct NestedLoopJoinOp : public PhysicalOpBinaryChild {
  /// do join for 2 expressions based on predicate
  ast::Expr* predicate;

  NestedLoopJoinOp(PhysicalOpPtr left, PhysicalOpPtr right,
                   ast::Expr* pred);
  NestedLoopJoinOp(PhysicalOpPtr left, PhysicalOpPtr right);

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;
  ~NestedLoopJoinOp() override = default;
};

}

#endif //GRAPHDB_NESTED_LOOP_JOIN_HPP