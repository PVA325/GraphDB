#include "planner/query_planner.hpp"

namespace graph::exec {
HashJoinOp::HashJoinOp(PhysicalOpPtr left, PhysicalOpPtr right,
                       std::vector<ast::ExprPtr>&& left_keys,
                       std::vector<ast::ExprPtr>&& right_keys) :
  PhysicalOpBinaryChild(std::move(left), std::move(right)),
  left_keys(std::move(left_keys)),
  right_keys(std::move(right_keys)) {
}


std::vector<ast::Expr*> HashJoinOp::ExprPtrVecToBasePtrVec(std::vector<ast::ExprPtr>& expr_vec) {
  static auto transf = std::for_each(expr_vec.begin(), expr_vec.end(), [](const ast::ExprPtr& ptr) {
    assert(ptr);
    return ptr.get();
  });
  std::vector<ast::Expr*> ans;
  std::transform(expr_vec.begin(), expr_vec.end(), std::back_inserter(ans), transf);
  return std::move(ans);
}

RowCursorPtr HashJoinOp::open(struct ExecContext& ctx) {
  return std::make_unique<HashJoinCursor>(
    left->open(ctx),
    right->open(ctx),
    ExprPtrVecToBasePtrVec(left_keys),
    ExprPtrVecToBasePtrVec(right_keys)
  );
}

}