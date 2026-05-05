#include "planner/query_planner.hpp"

namespace graph::exec {

LimitCursor::LimitCursor(RowCursorPtr child_cursor, size_t limit_count) :
  child_cursor(std::move(child_cursor)),
  limit_count(limit_count) {
}

bool LimitCursor::next(Row& out) {
  if (used_count >= limit_count || !child_cursor->next(out)) {
    return false;
  }
  ++used_count;
  return true;
}

void LimitCursor::close() {
  child_cursor->close();
}


}