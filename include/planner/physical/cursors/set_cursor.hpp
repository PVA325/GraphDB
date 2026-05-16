#pragma once

#include "row_cursor.hpp"
#include "planner/logical/logical_set.hpp"

namespace graph::exec {

struct SetCursor : RowCursor {
  SetCursor(RowCursorPtr child,
            std::vector<logical::LogicalSet::Assignment> assignments,
            storage::GraphDB* db);

  bool next(Row& out) override;
  void close() override;

public:
  RowCursorPtr child;
  std::vector<logical::LogicalSet::Assignment> assignments;
  storage::GraphDB* db;
};

} // namespace graph::exec

