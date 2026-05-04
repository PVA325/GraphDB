#ifndef GRAPHDB_NODE_PROPERTY_FILTER_CURSOR_HPP
#define GRAPHDB_NODE_PROPERTY_FILTER_CURSOR_HPP

#include "row_cursor.hpp"

namespace graph::exec {
struct NodePropertyFilterCursor : RowCursor {
  RowCursorPtr child_cursor;
  String alias;
  std::vector<String> labels;
  std::vector<std::pair<String, Value>> properties;

  NodePropertyFilterCursor(RowCursorPtr child_cursor, String alias, std::vector<String> labels,
                           std::vector<std::pair<String, Value>> properties);
  bool next(Row& out) override;

  void close() override;

  ~NodePropertyFilterCursor() override = default;
};
}

#endif //GRAPHDB_NODE_PROPERTY_FILTER_CURSOR_HPP