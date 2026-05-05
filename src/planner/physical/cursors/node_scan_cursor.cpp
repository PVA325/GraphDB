#include "planner/query_planner.hpp"

namespace graph::exec {

NodeScanCursor::NodeScanCursor(
  std::unique_ptr<storage::NodeCursor> nodes_cursor,
  String out_alias) :
  nodes_cursor(std::move(nodes_cursor)),
  out_alias(std::move(out_alias)) {
}

bool NodeScanCursor::next(Row& out) {
  Node* node_ptr;

  if (!nodes_cursor->next(node_ptr)) {
    return false;
  }
  if (out.slots_mapping.key_exists(out_alias)) {
    throw std::runtime_error("Invalid alias for PhysicalScan, alias already exists");
  }
  out.slots.emplace_back(node_ptr);
  out.slots_names.emplace_back(out_alias);
  out.slots_mapping.add_map(out_alias, out.slots.size() - 1);
  return true;
}

void NodeScanCursor::close() {
}
}