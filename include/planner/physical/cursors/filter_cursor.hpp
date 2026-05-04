//
// Created by uladzislau on 04.05.2026.
//

#ifndef GRAPHDB_FILTER_CURSOR_HPP
#define GRAPHDB_FILTER_CURSOR_HPP

#include "row_cursor.hpp"

namespace graph::exec {
struct FilterCursor : RowCursor {
  RowCursorPtr child_cursor;
  ast::Expr* predicate;

  FilterCursor(const FilterCursor&) = delete;

  FilterCursor(FilterCursor&&) = default;

  FilterCursor(RowCursorPtr child_cursor, ast::Expr* predicate);

  bool next(Row& out) override;

  void close() override;

  ~FilterCursor() override = default;
};
}
#endif //GRAPHDB_FILTER_CURSOR_HPP