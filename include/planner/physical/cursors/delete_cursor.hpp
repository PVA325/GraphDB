#pragma once

#include "row_cursor.hpp"

namespace graph::exec {
struct DeleteCursor : RowCursor {
  RowCursorPtr child;
  std::vector<String> aliases;
  storage::GraphDB* db;

  DeleteCursor(RowCursorPtr child, std::vector<String> aliases, storage::GraphDB* db);
  bool next(Row& out) override;
  void close() override;

  ~DeleteCursor() override = default;
};

}
