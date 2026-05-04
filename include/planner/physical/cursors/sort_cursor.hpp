#ifndef GRAPHDB_SORT_CURSOR_HPP
#define GRAPHDB_SORT_CURSOR_HPP

#include "row_cursor.hpp"

namespace graph::exec {
struct SortCursor : RowCursor {
  RowCursorPtr child;
  std::vector<ast::OrderItem> items;
  std::vector<Row> rows;

  SortCursor(RowCursorPtr child, std::vector<ast::OrderItem> items);
  bool next(Row& out) override;
  void close() override;

  ~SortCursor() override = default;
};

}

#endif //GRAPHDB_SORT_CURSOR_HPP