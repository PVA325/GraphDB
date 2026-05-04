#include "planner/query_planner.hpp"

namespace graph::exec {

NestedLoopJoinCursor::NestedLoopJoinCursor(
  RowCursorPtr left_cursor,
  PhysicalOp* right_operation,
  const ast::Expr* pred,
  ExecContext& ctx) :
  left_cursor(std::move(left_cursor)),
  right_cursor(nullptr),
  right_operation(right_operation),
  predicate(pred),
  ctx(ctx) {
}

bool NestedLoopJoinCursor::next(Row& out) {
  Row right_row;
  while (true) {
    right_row.clear();
    if (!right_cursor || !right_cursor->next(right_row)) {
      left_row.clear();
      if (!left_cursor->next(left_row)) {
        /// left_row must have slots mapping
        return false;
      }
      right_cursor = std::move(right_operation->open(ctx));
    } else  {
      Row new_row = MergeRows(left_row, right_row);
      ast::EvalContext exec_ctx{new_row};
      if (predicate == nullptr || PlannerUtils::ValueToBool((*predicate)(exec_ctx))) {
        out = std::move(new_row);
        return true;
      }

    }
  }
  return false;
}

void NestedLoopJoinCursor::close() {
  left_cursor->close();
  right_cursor->close();
}

}