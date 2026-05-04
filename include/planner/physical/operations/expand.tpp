#ifndef GRAPHDB_EXPAND_TPP
#define GRAPHDB_EXPAND_TPP

namespace graph::exec {
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
}


#endif //GRAPHDB_EXPAND_TPP