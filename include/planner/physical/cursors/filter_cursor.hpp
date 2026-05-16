#pragma once

#include "row_cursor.hpp"

namespace graph::exec {
struct FilterCursor : RowCursor {
  FilterCursor(const FilterCursor&) = delete;

  FilterCursor(FilterCursor&&) = default;

  FilterCursor(RowCursorPtr child_cursor, ast::Expr* predicate);

  bool next(Row& out) override;

  void close() override;

  ~FilterCursor() override = default;

public:
  RowCursorPtr child_cursor;
  ast::Expr* predicate;
};
}
