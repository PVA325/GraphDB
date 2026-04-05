#include "graph.hpp"

namespace graph::exec {
graph::exec::ScanNodeCursorPhysical::ScanNodeCursorPhysical(
  std::unique_ptr<storage::NodeCursor> nodes_cursor,
  String out_alias) :
  nodes_cursor(std::move(nodes_cursor)),
  out_alias(std::move(out_alias))
{}

bool graph::exec::ScanNodeCursorPhysical::next(graph::exec::Row &out) {
  storage::Node *node_ptr;
  nodes_cursor->next(node_ptr);
  if (!node_ptr) {
    return false;
  }
  int out_idx = out.slots_mapping.map(out_alias);
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
//  int out_idx = ctx.slots_mapping.map(out_alias);
  return std::make_unique<ScanNodeCursorPhysical>(
    std::move(ctx.db->nodes_with_label(label)),
    out_alias
  );
}

RowCursorPtr graph::exec::NodeScanOp::open(ExecContext &ctx) {
//  int out_idx = ctx.slots_mapping.map(out_alias);
  return std::make_unique<ScanNodeCursorPhysical>(
    std::move(ctx.db->all_nodes()),
    out_alias
  );
}


template<bool edge_outgoing>
graph::exec::ExpandNodeCursorPhysical<edge_outgoing>::ExpandNodeCursorPhysical(
  RowCursorPtr child_cursor,
  String src_alias,
  String dst_alias,
  std::function<bool(Edge *)> label_predicate,
  storage::GraphDB* db):
  child_cursor(std::move(child_cursor)),
  src_alias(std::move(src_alias)),
  dst_alias(std::move(dst_alias)),
  label_predicate(std::move(label_predicate)),
  edge_cursor(nullptr),
  db(db)
{}

template<bool edge_outgoing>
bool graph::exec::ExpandNodeCursorPhysical<edge_outgoing>::next(graph::exec::Row &out) {
  Edge* edge;
  size_t src_idx = out.slots_mapping.map(src_alias);
  size_t out_idx = out.slots_mapping.map(dst_alias);
  while (edge_cursor == nullptr || !edge_cursor->next(edge)) {

    if (!child_cursor->next(out)) {
      return false;
    }
    Node* node = std::get<Node*>(out.slots[src_idx]);
    if constexpr (edge_outgoing) {
      edge_cursor = db->outgoing_edges(node->id);
    } else {
      edge_cursor = db->incoming_edges(node->id);
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
    std::move(label_predicate),
    ctx
  );
}

graph::exec::FilterCursor::FilterCursor(
  RowCursorPtr nodes_cursor,
  String out_alias,
  std::unique_ptr<frontend::Expr> predicate):
  child_cursor(std::move(child_cursor)),
  out_alias(std::move(out_alias)),
  predicate(std::move(predicate))
{}

bool graph::exec::FilterCursor::next(graph::exec::Row &out) {
  bool was_writing = false;
  while (child_cursor->next(out)) {
    ast::EvalContext eval_ctx{out};
    Value val =(*predicate)(eval_ctx);
    if (PlannerUtils::ValueToBool(val)) {
      was_writing = true;
      break;
    }
  }
  return was_writing;
}

void graph::exec::FilterCursor::close() {}

graph::exec::RowCursorPtr graph::exec::FilterOp::open(graph::exec::ExecContext &ctx) {
  debugString_ = predicate->DebugString();
  auto child_cursor = child->open(ctx);
  return std::make_unique<FilterCursor>(
    std::move(child_cursor),
    out_alias,
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

template<typename T>
struct Debug {
  Debug() = delete;
};

graph::String graph::exec::FilterOp::DebugString() const {
  String ans = "FilterPhysical(";
  if (predicate) {
//    debugString_ = predicate->DebugString();
  }
  ans += debugString_;
  return ans;
}

graph::exec::ProjectCursor::ProjectCursor(
  RowCursorPtr child_cursor,
  std::vector<frontend::ReturnItem> items):
  child_cursor(std::move(child_cursor)),
  items(std::move(items))
{}

bool graph::exec::ProjectCursor::next(graph::exec::Row &out) {
  if (!child_cursor->next(out)) {
    return false;
  }
  Row new_row;
  for (const auto& return_item : items) {
    if (return_item.item.index() == 0) {
      String alias = std::get<0>(return_item.item);
      new_row.slots_names.emplace_back(alias);
      new_row.slots.emplace_back(out.slots[out.slots_mapping.map(alias)]);
      new_row.slots_mapping.add_map(alias, new_row.slots.size() - 1);
    } else {
      ast::PropertyExpr item = std::get<1>(return_item.item);
      const auto& cur_prop_src = out.slots[out.slots_mapping.map(item.alias)];
      String new_alias = "tmp_" + std::to_string(new_row.slots.size());
      if (cur_prop_src.index() == 2) {
        throw std::runtime_error("Error, trying to project from not valid source");
      }
      if (cur_prop_src.index() == 0) {
        auto node = std::get<Node*>(cur_prop_src);
        new_row.slots.emplace_back(node->properties.at(item.property));
        new_row.slots_names.emplace_back(new_alias);
        new_row.slots_mapping.add_map(new_alias, new_row.slots.size() - 1);
      }
    }
  }
  out = new_row;
  return true;
}

void graph::exec::ProjectCursor::close() {}


graph::exec::ProjectOp::ProjectOp(std::vector<frontend::ReturnItem> items, graph::exec::PhysicalOpPtr child):
  items(std::move(items)),
  child(std::move(child))
{}

graph::exec::RowCursorPtr graph::exec::ProjectOp::open(graph::exec::ExecContext &ctx) {
  return std::make_unique<ProjectCursor>(
    std::move(child->open(ctx)),
    items
  );
}

graph::String graph::exec::ProjectOp::DebugString() const {
  String ans = "";
  ans += "ProjectOp(";
  for (const auto& item : items) {
    ans += item.DebugString();
  }
  ans += ")";
  return ans;
}


}