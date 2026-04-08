#include <chrono>

#include "graph.hpp"

namespace graph::exec {
  ScanNodeCursorPhysical::ScanNodeCursorPhysical(
    std::unique_ptr<storage::NodeCursor> nodes_cursor,
    String out_alias) :
    nodes_cursor(std::move(nodes_cursor)),
    out_alias(std::move(out_alias)) {}

  bool ScanNodeCursorPhysical::next(Row &out) {
    storage::Node *node_ptr;
    nodes_cursor->next(node_ptr);
    if (!node_ptr) {
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

  void ScanNodeCursorPhysical::close() {}


  LabelIndexSeekOp::LabelIndexSeekOp(
    String label,
    String alias) :
    label(std::move(label)),
    out_alias(std::move(alias)) {}

  RowCursorPtr LabelIndexSeekOp::open(ExecContext &ctx) {
    return std::make_unique<ScanNodeCursorPhysical>(
      std::move(ctx.db->nodes_with_label(label)),
      out_alias
    );
  }

  RowCursorPtr NodeScanOp::open(ExecContext &ctx) {
    return std::make_unique<ScanNodeCursorPhysical>(
      std::move(ctx.db->all_nodes()),
      out_alias
    );
  }


  template<bool edge_outgoing>
  ExpandNodeCursorPhysical<edge_outgoing>::ExpandNodeCursorPhysical(
    RowCursorPtr child_cursor,
    String src_alias,
    String dst_alias,
    std::function<bool(Edge *)> label_predicate,
    storage::GraphDB *db):
    child_cursor(std::move(child_cursor)),
    src_alias(std::move(src_alias)),
    dst_alias(std::move(dst_alias)),
    label_predicate(std::move(label_predicate)),
    edge_cursor(nullptr),
    db(db) {}

  template<bool edge_outgoing>
  bool ExpandNodeCursorPhysical<edge_outgoing>::next(Row &out) {
    Edge *edge;
    size_t src_idx = out.slots_mapping.map(src_alias);
    if (out.slots_mapping.key_exists(dst_alias)) {
      throw std::runtime_error("Invalid alias for PhysicalExpand, alias already exists");
    }

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
    out.slots.emplace_back(edge);
    out.slots_names.emplace_back(dst_alias);
    out.slots_mapping.add_map(dst_alias, out.slots.size() - 1);
    return true;
  }


  template<bool edge_outgoing>
  void ExpandNodeCursorPhysical<edge_outgoing>::close() {}

  template<bool edge_outgoing>
  ExpandOp<edge_outgoing>::ExpandOp(
    graph::String src_alias,
    graph::String dst_alias,
    graph::String edge_type,
    PhysicalOpPtr child):
    PhysicalOpUnaryChild(std::move(child)),
    src_alias(std::move(src_alias)),
    dst_alias(std::move(dst_alias)),
    edge_type(std::move(edge_type)) {}

  template<bool edge_outgoing>
  RowCursorPtr ExpandOp<edge_outgoing>::open(ExecContext &ctx) {
    auto child_cursor = child->open(ctx);
    auto label_predicate = [edge_type = edge_type](Edge *e) { // can move edge type?
      return (edge_type.has_value() ? e->type == edge_type.value() : true);
    };
    return std::make_unique<ExpandNodeCursorPhysical<edge_outgoing>>(
      std::move(child_cursor),
      std::move(label_predicate),
      ctx
    );
  }

  FilterCursor::FilterCursor(
    RowCursorPtr child_cursor,
    String out_alias,
    std::unique_ptr<frontend::Expr> predicate) :
    child_cursor(std::move(child_cursor)),
    out_alias(std::move(out_alias)),
    predicate(std::move(predicate)) {}

  bool FilterCursor::next(Row &out) {
    bool was_writing = false;
    while (child_cursor->next(out)) {
      ast::EvalContext eval_ctx{out};
      Value val = (*predicate)(eval_ctx);
      if (PlannerUtils::ValueToBool(val)) {
        was_writing = true;
        break;
      }
    }
    return was_writing;
  }

  void FilterCursor::close() {}

  RowCursorPtr FilterOp::open(ExecContext &ctx) {
    debugString_ = predicate->DebugString();
    auto child_cursor = child->open(ctx);
    return std::make_unique<FilterCursor>(
      std::move(child_cursor),
      out_alias,
      std::move(predicate)
    );
  }

  FilterOp::FilterOp(
    std::unique_ptr<frontend::Expr> predicate,
    String out_alias,
    PhysicalOpPtr child) :
    PhysicalOpUnaryChild(std::move(child)),
    predicate(std::move(predicate)),
    out_alias(std::move(out_alias)) {}


  graph::String FilterOp::DebugString() const {
    String ans = "FilterPhysical(";
    if (predicate) {
//      debugString_ = predicate->DebugString();
    }
    ans += debugString_;
    return ans;
  }

  ProjectCursor::ProjectCursor(
    RowCursorPtr child_cursor,
    std::vector<frontend::ReturnItem> items) :
    child_cursor(std::move(child_cursor)),
    items(std::move(items)) {}

  bool ProjectCursor::next(Row &out) {
    if (!child_cursor->next(out)) {
      return false;
    }
    Row new_row;
    for (const auto &return_item: items) {
      if (return_item.item.index() == 0) {
        String alias = std::get<0>(return_item.item);
        if (!new_row.slots_mapping.key_exists(alias)) {
          throw std::runtime_error("No alias for PhysicalProject");
        }
        new_row.slots_names.emplace_back(alias);
        new_row.slots.emplace_back(out.slots[out.slots_mapping.map(alias)]);
        new_row.slots_mapping.add_map(alias, new_row.slots.size() - 1);
        continue;
      }
      ast::PropertyExpr item = std::get<1>(return_item.item);
      if (!new_row.slots_mapping.key_exists(item.alias)) {
        throw std::runtime_error("No alias for PhysicalProject");
      }
      const auto &cur_prop_src = out.slots[out.slots_mapping.map(item.alias)];
      String new_alias = "tmp_" + std::to_string(new_row.slots.size());
      if (cur_prop_src.index() == 2) {
        throw std::runtime_error("Error, trying to project from not valid source");
      }
      if (cur_prop_src.index() == 0) {
        auto node = std::get<Node *>(cur_prop_src);
        new_row.slots.emplace_back(node->properties.at(item.property));
      } else {
        auto edge = std::get<Edge *>(cur_prop_src);
        new_row.slots.emplace_back(edge->properties.at(item.property));
      }
      new_row.slots_names.emplace_back(new_alias);
      new_row.slots_mapping.add_map(new_alias, new_row.slots.size() - 1);

    }
    out = new_row;
    return true;
  }

  void ProjectCursor::close() {}


  ProjectOp::ProjectOp(std::vector<frontend::ReturnItem> items, PhysicalOpPtr child) :
    PhysicalOpUnaryChild(std::move(child)),
    items(std::move(items)) {}

  RowCursorPtr ProjectOp::open(ExecContext &ctx) {
    return std::make_unique<ProjectCursor>(
      std::move(child->open(ctx)),
      items
    );
  }

  graph::String ProjectOp::DebugString() const {
    String ans = "";
    ans += "ProjectOp(";
    for (const auto &item: items) {
      ans += item.DebugString();
    }
    ans += ")";
    return ans;
  }

  LimitCursor::LimitCursor(RowCursorPtr child_cursor, size_t limit_count) :
    child_cursor(std::move(child_cursor)),
    limit_count(limit_count) {}

  bool LimitCursor::next(Row &out) {
    if (used_count >= limit_count || !child_cursor->next(out)) {
      return false;
    }
    ++used_count;
    return true;
  }

  void LimitCursor::close() {}


  LimitOp::LimitOp(graph::Int limit_size, PhysicalOpPtr child) :
    PhysicalOpUnaryChild(std::move(child)),
    limit_size(limit_size) {}

  RowCursorPtr LimitOp::open(ExecContext &ctx) {
    return std::make_unique<LimitCursor>(
      std::move(child->open(ctx)),
      limit_size
    );
  }

  graph::String LimitOp::DebugString() const {
    String ans = "LimitOp(" + std::to_string(limit_size) + ")";
    return ans;
  }


  NestedLoopJoinCursor::NestedLoopJoinCursor(
    RowCursorPtr left_cursor,
    PhysicalOp *right_operation,
    frontend::Expr* pred,
    ExecContext& ctx):
    left_cursor(std::move(left_cursor)),
    right_operation(right_operation),
    predicate(pred),
    ctx(ctx)
  {}

  bool NestedLoopJoinCursor::next(Row &out) {
    while (true) {
      Row right_row;
      if (!right_cursor || !right_cursor->next(right_row)) {
        if (!left_cursor->next(left_row)) { /// left_row must have slots mapping
          return false;
        }
        right_cursor = right_operation->open(ctx);
      }
      if (right_cursor->next(right_row)) {
        Row new_row = MergeRows(left_row, right_row);
        ast::EvalContext exec_ctx{new_row};
        if (predicate != nullptr && PlannerUtils::ValueToBool((*predicate)(exec_ctx))) {
          out = std::move(new_row);
          return true;
        }
      }
    }
  }

  void NestedLoopJoinCursor::close() {}

  NestedLoopJoinOp::NestedLoopJoinOp(
    PhysicalOpPtr left,
    PhysicalOpPtr right,
    std::unique_ptr<frontend::Expr> pred):
    PhysicalOpBinaryChild(std::move(left), std::move(right)),
    predicate(std::move(pred))
  {}

  NestedLoopJoinOp::NestedLoopJoinOp(
    PhysicalOpPtr left,
    PhysicalOpPtr right):
    PhysicalOpBinaryChild(std::move(left), std::move(right)),
    predicate(nullptr)
  {}


  std::string PhysicalPlan::DebugString() const {
    return root->DebugSubtreeString();
  }

  RowCursorPtr NestedLoopJoinOp::open(ExecContext &ctx) {
    return std::make_unique<NestedLoopJoinCursor>(
      left->open(ctx),
      right.get(),
      predicate.get(),
      ctx
    );
  }

  std::string NestedLoopJoinOp::DebugString() const {
    String ans = "LoopJoin"; /// todo
    return ans;
  }

  KeyHashJoinCursor::KeyHashJoinCursor(RowCursorPtr left_cursor, RowCursorPtr right_cursor,
                                       String left_alias, String right_alias,
                                       String left_feature_key, String right_feature_key):
  left_cursor(std::move(left_cursor)),
  right_cursor(std::move(right_cursor)),
  left_alias(std::move(left_alias)),
  right_alias(std::move(right_alias)),
  left_feature_key(std::move(left_feature_key)),
  right_feature_key(std::move(right_feature_key)){
    Row cur;
    while (left_cursor->next(cur)) {
      auto cur_slot_val = cur.slots[cur.slots_mapping.map(left_alias)];
      if (cur_slot_val.index() == 2) {
        throw std::runtime_error("Error during KeyHashJoin: invalid alias type");
      }
      Value val;
      if (cur_slot_val.index() == 0) {
        Node* node = std::get<0>(cur_slot_val);
        val = node->properties[left_feature_key];
      } else {
        Edge* edge = std::get<1>(cur_slot_val);
        val = edge->properties[left_feature_key];
      }
      left_rows[val].emplace_back(cur);
    }
    it_left = left_rows.end();
  }

  bool KeyHashJoinCursor::next(Row& out) {
    while (it_left == left_rows.end() || it_left->second.size() <= vec_left_idx) {
      if (!right_cursor->next(last_right_row)) {
        return false;
      }
      Value right_value = last_right_row.slots[last_right_row.slots_mapping.map(right_feature_key)];
      auto it = left_rows.find(right_value);
      if (it != left_rows.end() && !it->second.empty()) {
        it_left = it;
        vec_left_idx = 0;
        break;
      }
    }

    Row left_row = it_left->second[vec_left_idx++];
    out = MergeRows(left_row, last_right_row);
    return true;
  }

  void KeyHashJoinCursor::close() {}

  graph::exec::Row graph::exec::MergeRows(graph::exec::Row &first, graph::exec::Row &second) {
    Row ans;
    ans.slots.insert(ans.slots.end(), first.slots.begin(), first.slots.end());
    ans.slots.insert(ans.slots.end(), second.slots.begin(), second.slots.end());

    ans.slots_names.insert(ans.slots_names.end(), first.slots_names.begin(), first.slots_names.end());
    ans.slots_names.insert(ans.slots_names.end(), second.slots_names.begin(), second.slots_names.end());

    ans.slots_mapping.alias_to_slot = first.slots_mapping.alias_to_slot;
    ans.slots_mapping.alias_to_slot.merge(second.slots_mapping.alias_to_slot);
    return ans;
  }


  KeyHashJoinOp::KeyHashJoinOp(PhysicalOpPtr left, PhysicalOpPtr right,
                  String left_alias, String right_alias,
                  String left_feature_key, String right_feature_key):
    PhysicalOpBinaryChild(std::move(left), std::move(right)),
    left_alias(std::move(left_alias)),
    right_alias(std::move(right_alias)),
    left_feature_key(std::move(left_feature_key)),
    right_feature_key(std::move(right_feature_key)) {}

  RowCursorPtr KeyHashJoinOp::open(struct ExecContext& ctx) {
    return std::make_unique<KeyHashJoinCursor>(
      std::move(left->open()),
      std::move(right->open()),
      left_alias,
      right_alias,
      left_feature_key,
      right_feature_key
    );
  }

  String KeyHashJoinOp::DebugString() const {
    String ans = "HashJoin(" + left_alias + "." + left_feature_key + "," + right_alias + "." + right_feature_key + ")";
    return ans;
  }

  PhysicalPlan::PhysicalPlan(PhysicalOpPtr root): root(std::move(root)) {}

}