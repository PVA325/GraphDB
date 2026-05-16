#pragma once

#include "row_cursor.hpp"

namespace graph::exec {
struct LimitCursor : RowCursor {
  LimitCursor(const LimitCursor&) = delete;

  LimitCursor(LimitCursor&&) = default;

  LimitCursor(RowCursorPtr child_cursor, size_t limit_count);

  bool next(Row& out) override;

  void close() override;

  ~LimitCursor() override = default;

public:
  RowCursorPtr child_cursor;
  size_t limit_count;
  size_t used_count{0};
};
}
