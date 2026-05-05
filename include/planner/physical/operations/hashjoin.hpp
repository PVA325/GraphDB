#ifndef GRAPHDB_HASHJOIN_HPP
#define GRAPHDB_HASHJOIN_HPP

#include "physical_op_binary.hpp"

namespace graph::exec {
struct HashJoinOp : public PhysicalOpBinaryChild {
  /// do hashJoin
  std::vector<ast::ExprPtr> left_keys;
  std::vector<ast::ExprPtr> right_keys;

  HashJoinOp(PhysicalOpPtr left, PhysicalOpPtr right,
                std::vector<ast::ExprPtr>&& left_keys,
                std::vector<ast::ExprPtr>&& right_keys);

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;
  ~HashJoinOp() override = default;

  static std::vector<ast::Expr*> ExprPtrVecToBasePtrVec(std::vector<ast::ExprPtr>& expr_vec);
};

}

#endif //GRAPHDB_HASHJOIN_HPP