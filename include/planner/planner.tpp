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
    size_t src_idx = out.slots_mapping.map_and_check(src_alias, "Invalid alias for PhysicalExpand, src_alias do not exists");

    while (edge_cursor == nullptr || !edge_cursor->next(edge)) {
      if (!child_cursor->next(out)) {
        return false;
      }
      Node *node = std::get<Node *>(out.slots[src_idx]);
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

  template<bool edge_outgoing>
  ExpandOp<edge_outgoing>::ExpandOp(
    String src_alias,
    String dst_edge_alias,
    String dst_node_alias,
    std::optional<String> edge_type,
    PhysicalOpPtr child):
    PhysicalOpUnaryChild(std::move(child)),
    src_alias(std::move(src_alias)),
    dst_edge_alias(std::move(dst_edge_alias)),
    dst_node_alias(std::move(dst_node_alias)),
    edge_type(std::move(edge_type)) {}

  template<bool edge_outgoing>
  RowCursorPtr ExpandOp<edge_outgoing>::open(ExecContext &ctx) {
    auto label_predicate = [edge_type = this->edge_type](const Edge *e) {
      return (edge_type.has_value() ? e->type == edge_type.value() : true);
    };
    return std::make_unique<ExpandNodeCursorPhysical<edge_outgoing>>(
      std::move(child->open(ctx)),
      src_alias,
      dst_edge_alias,
      dst_node_alias,
      std::move(label_predicate),
      ctx.db
    );
  }

  template <bool edge_outgoing>
  String ExpandOp<edge_outgoing>::DebugString() const {
    return std::format(
      "Expand({} {} {}, type={})",
      src_alias,
      (edge_outgoing ? "-" : "<"),
      dst_edge_alias,
      (edge_outgoing ? ">" : "-"),
      dst_node_alias,
      (edge_type.has_value() ? std::format("\"{}\"", edge_type.value()) : "*")
    );
  }
} // namespace graph::exec