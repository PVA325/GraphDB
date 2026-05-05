#include "planner/query_planner.hpp"

namespace {
template <typename T>
bool isSubset(const std::vector<T>& larger, const std::vector<T>& smaller) {
  return std::all_of(smaller.begin(), smaller.end(), [&](const T& x) {
    return std::find(larger.begin(), larger.end(), x) != larger.end();
  });
}

}

namespace graph::exec {

NodePropertyFilterCursor::NodePropertyFilterCursor(RowCursorPtr child_cursor, String alias,
                                                   std::vector<String> labels,
                                                   std::vector<std::pair<String, Value>> properties) :
  child_cursor(std::move(child_cursor)),
  alias(std::move(alias)),
  labels(std::move(labels)),
  properties(std::move(properties)) {
}

bool NodePropertyFilterCursor::next(Row& out) {
  Row mark = out;
  bool found = false;
  while (child_cursor->next(out)) {
    size_t slot_idx = out.slots_mapping.map_and_check(
      alias,
      std::format("NodePropertyFilterCursor: Error, no src alias", alias)
    );
    const auto& row_slot = out.slots[slot_idx];
    if (!std::holds_alternative<Node*>(row_slot)) {
      throw std::runtime_error("NodePropertyFilterCursor: Error not a node");
    }
    auto* node = std::get<Node*>(row_slot);
    std::vector<std::pair<String, Value>> p(node->properties.begin(), node->properties.end());
    if (isSubset(node->labels, labels) &&
      isSubset(p, properties)) {
      found = true;
      break;
      }
    out = mark;
  }
  return found;
}

void NodePropertyFilterCursor::close() {
  child_cursor->close();
}

}