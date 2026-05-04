#include "planner/query_planner.hpp"

namespace graph::exec {

ProjectCursor::ProjectCursor(
  RowCursorPtr child_cursor,
  std::vector<ast::ReturnItem> items) :
  child_cursor(std::move(child_cursor)),
  items(std::move(items)) {
}

bool ProjectCursor::next(Row& out) {
  if (!child_cursor->next(out)) {
    return false;
  }
  Row new_row;
  for (const auto& ret_item : items) {
    if (ret_item.return_item.index() == 0) {
      String alias = std::get<0>(ret_item.return_item);
      size_t old_row_idx = out.slots_mapping.map_and_check(
        alias,
        std::format("PhysicalProject: Error, no alias \"{}\"", alias)
      );

      new_row.AddValue(
        out.slots[old_row_idx],
        alias,
        std::format("PhysicalProject: Error, alias {} is already in use", alias)
      );
      continue;
    }
    ast::PropertyExpr prop = std::get<1>(ret_item.return_item);
    size_t old_row_idx = out.slots_mapping.map_and_check(
      prop.alias,
      std::format("PhysicalProject: Error, no alias \"{}\"", prop.alias)
    );
    const auto& cur_prop_src = out.slots[old_row_idx];
    String new_alias = prop.alias + "." + prop.property;
    if (cur_prop_src.index() == 2) {
      throw std::runtime_error(std::format("PhysicalProject: Error, trying to project {}.{}, but {} is Value",
                                           prop.alias, prop.property, prop.alias));
    }

    String error_msg = std::format("PhysicalProject: {} is already in use", new_alias);
    if (cur_prop_src.index() == 0) {
      auto node = std::get<Node*>(cur_prop_src);
      new_row.AddValue(
        node->properties.at(prop.property),
        new_alias,
        error_msg
      );
      continue;
    }
    auto edge = std::get<Edge*>(cur_prop_src);
    new_row.AddValue(
      edge->properties.at(prop.property),
      new_alias,
      error_msg
    );
  }
  out = new_row;
  return true;
}

void ProjectCursor::close() {
  child_cursor->close();
}

}