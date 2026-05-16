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
  if (out.KeyExists(out_alias)) {
    throw std::runtime_error("Invalid alias for PhysicalScan, alias already exists");
  }
  out.AddSlot(node_ptr, out_alias);
  return true;
}

void NodeScanCursor::close() {
}
}
