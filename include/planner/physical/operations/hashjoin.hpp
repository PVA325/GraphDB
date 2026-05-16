#pragma once

#include "physical_op_binary.hpp"

namespace graph::exec {
struct HashJoinOp : public PhysicalOpBinaryChild {
  /// do hashJoin
  HashJoinOp(PhysicalOpPtr left, PhysicalOpPtr right,
                std::vector<ast::ExprPtr>&& left_keys,
                std::vector<ast::ExprPtr>&& right_keys);

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;
  ~HashJoinOp() override = default;

  static std::vector<ast::Expr*> ExprPtrVecToBasePtrVec(std::vector<ast::ExprPtr>& expr_vec);

public:
  std::vector<ast::ExprPtr> left_keys;
  std::vector<ast::ExprPtr> right_keys;
};

}
