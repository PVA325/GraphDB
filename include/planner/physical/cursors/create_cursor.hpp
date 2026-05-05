#ifndef GRAPHDB_CREATE_CURSOR_HPP
#define GRAPHDB_CREATE_CURSOR_HPP

#include "row_cursor.hpp"
#include "planner/logical/logical_create.hpp"

namespace graph::exec {

struct CreateCursor : public RowCursor {
  RowCursorPtr child;
  std::vector<std::variant<logical::CreateNodeSpec, logical::CreateEdgeSpec>> items;
  storage::GraphDB* db;

  CreateCursor(RowCursorPtr child,
               std::vector<std::variant<logical::CreateNodeSpec, logical::CreateEdgeSpec>> items,
               storage::GraphDB* db);
  bool next(Row& out) override;
  void close() override;

private:
  bool was_writing{false};
};
}

#endif //GRAPHDB_CREATE_CURSOR_HPP