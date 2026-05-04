#include "planner/query_planner.hpp"

namespace graph::exec {

FilterCursor::FilterCursor(
  RowCursorPtr child_cursor,
  ast::Expr* predicate) :
  child_cursor(std::move(child_cursor)),
  predicate(predicate) {
}

bool FilterCursor::next(Row& out) {
  bool was_writing = false;
  Row mark = out;
  while (child_cursor->next(out)) {
    ast::EvalContext eval_ctx{out};
    Value val = (*predicate)(eval_ctx);
    if (PlannerUtils::ValueToBool(val)) {
      was_writing = true;
      break;
    }
    out = mark;
  }
  return was_writing;
}

void FilterCursor::close() {
  child_cursor->close();
}

}