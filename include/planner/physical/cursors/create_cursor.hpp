#pragma once

#include "row_cursor.hpp"
#include "planner/logical/logical_create.hpp"

namespace graph::exec {

struct CreateCursor : public RowCursor {
  CreateCursor(RowCursorPtr child,
               std::vector<std::variant<logical::CreateNodeSpec, logical::CreateEdgeSpec>> items,
               storage::GraphDB* db);

  bool next(Row& out) override;
  void close() override;

public:
  RowCursorPtr child;
  std::vector<std::variant<logical::CreateNodeSpec, logical::CreateEdgeSpec>> items;
  storage::GraphDB* db;

private:
  bool was_writing{false};
};

} // namespace graph::exec

