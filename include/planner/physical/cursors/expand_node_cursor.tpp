#pragma once
#include <format>

namespace graph::exec {
  template<bool edge_outgoing>
    ExpandNodeCursorPhysical<edge_outgoing>::ExpandNodeCursorPhysical(
      RowCursorPtr child_cursor,
      String src_alias,
      String dst_edge_alias,
      String dst_node_alias,
      std::function<bool(Edge *)> label_predicate,
      storage::GraphDB *db):
      child_cursor(std::move(child_cursor)),
      edge_cursor(nullptr),
      label_predicate(std::move(label_predicate)),
      src_alias(std::move(src_alias)),
      dst_edge_alias(std::move(dst_edge_alias)),
      dst_node_alias(std::move(dst_node_alias)),
      db(db) {}

  template<bool edge_outgoing>
  bool ExpandNodeCursorPhysical<edge_outgoing>::next(Row &out) {
    Edge *edge;
    Row mark = out;

    while (edge_cursor == nullptr || !edge_cursor->next(edge) ||
          (label_predicate != nullptr && !label_predicate(edge))) {
      out = mark;
      if (!child_cursor->next(out)) {
        return false;
      }
      size_t src_idx = out.slots_mapping.map_and_check(src_alias, "Invalid alias for PhysicalExpand, src_alias do not exists");
      if (!std::holds_alternative<Node*>(out.slots[src_idx])) {
        throw std::logic_error("Invalid PhysicalExpand, src_alias is not a Node");
      }
      auto node = std::get<Node *>(out.slots[src_idx]);
      if constexpr (edge_outgoing) {
        edge_cursor = db->outgoing_edges(node->id);
      } else {
        edge_cursor = db->incoming_edges(node->id);
      }
    }

    String error_message = "Invalid alias for PhysicalExpand, dst_alias already exists";
    out.AddValue(edge, dst_edge_alias, error_message);
    out.AddValue(db->get_node(edge->dst), dst_node_alias, error_message);
    return true;
  }


  template<bool edge_outgoing>
  void ExpandNodeCursorPhysical<edge_outgoing>::close() {
    child_cursor->close();
  }
}  // namespace graph::exec