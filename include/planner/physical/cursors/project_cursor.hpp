#pragma once

#include "row_cursor.hpp"

namespace graph::exec {

struct ProjectCursor : RowCursor {
  ProjectCursor(const ProjectCursor&) = delete;

  ProjectCursor(ProjectCursor&&) = default;

  ProjectCursor(RowCursorPtr child_cursor, std::vector<ast::ReturnItem> items);

  bool next(Row& out) override;

  void close() override;

  ~ProjectCursor() override = default;

public:
  RowCursorPtr child_cursor;
  std::vector<ast::ReturnItem> items;
};

} // namespace graph::exec

