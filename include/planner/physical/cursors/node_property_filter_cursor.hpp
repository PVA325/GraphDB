#pragma once

#include "row_cursor.hpp"

namespace graph::exec {

struct NodePropertyFilterCursor : RowCursor {
  NodePropertyFilterCursor(RowCursorPtr child_cursor, String alias, std::vector<String> labels,
                           std::vector<std::pair<String, Value>> properties);

  bool next(Row& out) override;

  void close() override;

  ~NodePropertyFilterCursor() override = default;

public:
  RowCursorPtr child_cursor;
  String alias;
  std::vector<String> labels;
  std::vector<std::pair<String, Value>> properties;
};

} // namespace graph::exec

