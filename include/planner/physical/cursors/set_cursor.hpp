#ifndef GRAPHDB_SET_CURSOR_HPP
#define GRAPHDB_SET_CURSOR_HPP

#include "row_cursor.hpp"
#include "planner/logical/logical_set.hpp"

namespace graph::exec {

struct SetCursor : RowCursor {
  RowCursorPtr child;
  std::vector<logical::LogicalSet::Assignment> assignments;
  storage::GraphDB* db;

  SetCursor(RowCursorPtr child,
            std::vector<logical::LogicalSet::Assignment> assignments,
            storage::GraphDB* db);
  bool next(Row& out) override;
  void close() override;
};

}

#endif //GRAPHDB_SET_CURSOR_HPP