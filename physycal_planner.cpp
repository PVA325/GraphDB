#include "graph.hpp"

namespace graph::exec {
graph::exec::ScanNodeCursorPhysical::ScanNodeCursorPhysical(
  std::unique_ptr<storage::NodeCursor> nodes_cursor,
  size_t out_idx) :
  nodes_cursor(std::move(nodes_cursor)),
  out_idx(out_idx)
{}

bool graph::exec::ScanNodeCursorPhysical::next(graph::exec::Row &out) {
  storage::Node *node_ptr;
  nodes_cursor->next(node_ptr);
  if (!node_ptr) {
    return false;
  }
  out.slots[out_idx] = node_ptr;
  return true;
}

void graph::exec::ScanNodeCursorPhysical::close() {}


LabelIndexSeekOp::LabelIndexSeekOp(
  String label,
  String alias) :
  label(std::move(label)),
  out_alias(std::move(alias))
{}

RowCursorPtr graph::exec::LabelIndexSeekOp::open(ExecContext &ctx) {
  int out_idx = ctx.slots_mapping.map(out_alias);
  return std::make_unique<ScanNodeCursorPhysical>(
    std::move(ctx.db->nodes_with_label(label)),
    out_idx
  );
}

RowCursorPtr graph::exec::NodeScanOp::open(ExecContext &ctx) {
  int out_idx = ctx.slots_mapping.map(out_alias);
  return std::make_unique<ScanNodeCursorPhysical>(
    std::move(ctx.db->all_nodes()),
    out_idx
  );
}


template<bool edge_outgoing>
graph::exec::ExpandNodeCursorPhysical<edge_outgoing>::ExpandNodeCursorPhysical(
  RowCursorPtr nodes_cursor,
  size_t src_idx,
  size_t out_idx,
  std::function<bool(Edge *)> label_predicate,
  ExecContext &ctx):
  nodes_cursor(std::move(nodes_cursor)),
  src_idx(src_idx),
  out_idx(out_idx),
  label_predicate(std::move(label_predicate)),
  edge_cursor(nullptr),
  ctx(ctx)
{}

template<bool edge_outgoing>
bool graph::exec::ExpandNodeCursorPhysical<edge_outgoing>::next(graph::exec::Row &out) {
  Edge* edge;
  while (edge_cursor == nullptr || !edge_cursor->next(edge)) {

    if (!nodes_cursor->next(out)) {
      return false;
    }
    Node* node = std::get<Node*>(out.slots[src_idx]);
    if constexpr (edge_outgoing) {
      edge_cursor = ctx.db->outgoing_edges(node->id);
    } else {
      edge_cursor = ctx.db->incoming_edges(node->id);
    }
  }
  out.slots[out_idx] = edge;
  return true;
}


template<bool edge_outgoing>
void graph::exec::ExpandNodeCursorPhysical<edge_outgoing>::close() {}

template<bool edge_outgoing>
graph::exec::ExpandOp<edge_outgoing>::ExpandOp(
  graph::String src_alias, 
  graph::String dst_alias,
  graph::String edge_type,
  PhysicalOpPtr child):
  src_alias(std::move(src_alias)),
  dst_alias(std::move(dst_alias)),
  edge_type(std::move(edge_type)),
  child(std::move(child))
{}

template<bool edge_outgoing>
RowCursorPtr graph::exec::ExpandOp<edge_outgoing>::open(ExecContext &ctx) {
  auto child_cursor = child->open(ctx);
  auto label_predicate = [edge_type= edge_type](Edge* e) { // can move edge type?
    return (edge_type.has_value() ? e->type == edge_type.value() : true);
  };
  return std::make_unique<ExpandNodeCursorPhysical<edge_outgoing>>(
    std::move(child_cursor),
    ctx.slots_mapping.map(src_alias),
    ctx.slots_mapping.map(dst_alias),
    std::move(label_predicate),
    ctx
  );
}


graph::exec::FilterCursor::FilterCursor(
  RowCursorPtr nodes_cursor,
  size_t out_idx,
  ExecContext& ctx,
  std::unique_ptr<frontend::Expr> predicate):
  nodes_cursor(std::move(nodes_cursor)),
  out_idx(out_idx),
  ctx(ctx),
  predicate(std::move(predicate))
{}

bool graph::exec::FilterCursor::next(graph::exec::Row &out) {
  bool was_writing = false;
  while (nodes_cursor->next(out)) {
    ast::EvalContext eval_ctx{ctx, out};
    Value val =(*predicate)(eval_ctx);
    if (PlannerUtils::ValueToBool(val)) {
      was_writing = true;
      break;
    }
  }
  return was_writing;
}

graph::exec::RowCursorPtr graph::exec::FilterOp::open(graph::exec::ExecContext &ctx) {
  auto child_cursor = child->open(ctx);
  return std::make_unique<FilterCursor>(
    std::move(child_cursor),
    ctx.slots_mapping.map(out_alias),
    ctx,
    std::move(predicate)
  );
}

graph::exec::FilterOp::FilterOp(
  std::unique_ptr<frontend::Expr> predicate,
  String out_alias,
  graph::exec::PhysicalOpPtr child):
  predicate(std::move(predicate)),
  out_alias(std::move(out_alias)),
  child(std::move(child))
{}
}