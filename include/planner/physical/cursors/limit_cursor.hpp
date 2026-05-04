#ifndef GRAPHDB_LIMIT_CURSOR_HPP
#define GRAPHDB_LIMIT_CURSOR_HPP

#include "row_cursor.hpp"

namespace graph::exec {
struct LimitCursor : RowCursor {
  RowCursorPtr child_cursor;
  size_t limit_count;
  size_t used_count{0};

  LimitCursor(const LimitCursor&) = delete;

  LimitCursor(LimitCursor&&) = default;

  LimitCursor(RowCursorPtr child_cursor, size_t limit_count);

  bool next(Row& out) override;

  void close() override;

  ~LimitCursor() override = default;
};
}

#endif //GRAPHDB_LIMIT_CURSOR_HPP