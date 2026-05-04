#include "planner/query_planner.hpp"

namespace graph::exec {
DeleteCursor::DeleteCursor(RowCursorPtr child, std::vector<String> aliases, storage::GraphDB* db) :
  child(std::move(child)),
  aliases(std::move(aliases)),
  db{db} {
}

bool DeleteCursor::next(Row& out) {
  if (!child->next(out)) {
    return false;
  }
  for (const auto& alias : aliases) {
    size_t alias_idx = out.slots_mapping.map_and_check(
      alias,
      std::format("DeleteCursor: Error, invalid source alias {}", alias)
    );
    const RowSlot& row_slot = out.slots[alias_idx];
    if (row_slot.index() == 2) {
      continue;
    }
    if (row_slot.index() == 0) {
      db->delete_node(std::get<Node*>(row_slot)->id);
    } else {
      db->delete_edge(std::get<Edge*>(row_slot)->id);
    }
  }
  return true;
}

void DeleteCursor::close() {
  child->close();
}

}