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
    const RowSlot& row_slot = out.GetAliasedObj(
      alias,
      std::format("DeleteCursor: Error, invalid source alias {}", alias)
    );

    if (std::holds_alternative<Value>(row_slot.value)) {
      continue;
    }
    if (std::holds_alternative<Node*>(row_slot.value)) {
      db->delete_node(std::get<Node*>(row_slot.value)->id);
    } else {
      db->delete_edge(std::get<Edge*>(row_slot.value)->id);
    }
  }
  return true;
}

void DeleteCursor::close() {
  child->close();
}
}
