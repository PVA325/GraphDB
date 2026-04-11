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
    edge_cursor(nullptr),
    label_predicate(std::move(label_predicate)),
    src_alias(std::move(src_alias)),
    dst_alias(std::move(dst_alias)),
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
  void ExpandNodeCursorPhysical<edge_outgoing>::close() {
    child_cursor->close();
  }

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
    auto label_predicate = [edge_type = edge_type](const Edge *e) { // can move edge type?
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

  void FilterCursor::close() {
    child_cursor->close();
  }

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

  void ProjectCursor::close() {
    child_cursor->close();
  }


  ProjectOp::ProjectOp(std::vector<frontend::ReturnItem> items, PhysicalOpPtr child) :
    PhysicalOpUnaryChild(std::move(child)),
    items(std::move(items)) {}

  RowCursorPtr ProjectOp::open(ExecContext &ctx) {
    return std::make_unique<ProjectCursor>(
      std::move(child->open(ctx)),
      items
    );
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

  void LimitCursor::close() {
    child_cursor->close();
  }


  LimitOp::LimitOp(graph::Int limit_size, PhysicalOpPtr child) :
    PhysicalOpUnaryChild(std::move(child)),
    limit_size(limit_size) {}

  RowCursorPtr LimitOp::open(ExecContext &ctx) {
    return std::make_unique<LimitCursor>(
      std::move(child->open(ctx)),
      limit_size
    );
  }

  NestedLoopJoinCursor::NestedLoopJoinCursor(
    RowCursorPtr left_cursor,
    PhysicalOp *right_operation,
    const frontend::Expr* pred,
    ExecContext& ctx):
    left_cursor(std::move(left_cursor)),
    right_cursor(nullptr),
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

  void NestedLoopJoinCursor::close() {
    left_cursor->close();
    right_cursor->close();
  }

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
      size_t slot_idx = cur.slots_mapping.map(left_alias);
      Value val = GetValueFromSlot(cur.slots[slot_idx], left_feature_key);

      left_rows[val].emplace_back(cur);
    }
    it_left = left_rows.end();
  }

  bool KeyHashJoinCursor::next(Row& out) {
    while (it_left == left_rows.end() || it_left->second.size() <= vec_left_idx) {
      if (!right_cursor->next(last_right_row)) {
        return false;
      }
      size_t slot_idx = last_right_row.slots_mapping.map(right_feature_key);
      Value right_value = GetValueFromSlot(last_right_row.slots[slot_idx], right_feature_key);

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

  void KeyHashJoinCursor::close() {
    left_cursor->close();
    right_cursor->close();
  }

  Value GetValueFromSlot(const RowSlot& slot, const String& feature_key) {
    if (slot.index() == 2) {
      if (feature_key != "") {
        throw std::runtime_error("Error during KeyHashJoin: invalid alias or property");
      }
      return std::get<2>(slot);
    }
    if (slot.index() == 1) {
      return std::get<1>(slot)->properties[feature_key];
    }
    return std::get<0>(slot)->properties[feature_key];
  }

  Row MergeRows(graph::exec::Row &first, graph::exec::Row &second) {
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
      std::move(left->open(ctx)),
      std::move(right->open(ctx)),
      left_alias,
      right_alias,
      left_feature_key,
      right_feature_key
    );
  }

  SetCursor::SetCursor(RowCursorPtr child, std::vector<String> aliases,
    std::vector<std::optional<std::vector<String>>> labels,
    std::vector<std::optional<std::vector<std::pair<String, Value>>>> properties):
    child(std::move(child)),
    aliases(std::move(aliases)),
    labels(std::move(labels)),
    properties(std::move(properties)) {}

  bool SetCursor::next(Row& out) {
    if (!child->next(out)) {
      return false;
    }
    for (size_t i = 0; i < aliases.size(); ++i) {
      const String& cur_alias = aliases[i];
      size_t cur_slot_idx = out.slots_mapping.map(cur_alias);
      RowSlot& row_slot = out.slots[cur_slot_idx];
      if (row_slot.index() == 2) {
        throw std::runtime_error("Error while setting, invalid type");
      }

      auto& obj_properties = (row_slot.index() == 0 ? std::get<0>(row_slot)->properties : std::get<1>(row_slot)->properties);

      if (properties[i].has_value()) {
        for (const auto& [key, val] : properties[i].value()) {
          obj_properties[key] = val;
        }
      }

      if (!labels[i].has_value() || labels[i].value().empty()) {
        continue;
      }
      if (row_slot.index() == 1) {
        auto& edge_label = std::get<1>(row_slot)->type;
        edge_label = labels[i].value().back();
        continue;
      }
      auto& node_labels = std::get<0>(row_slot)->labels;
      for (const auto& new_label : labels[i].value()) {
        if (std::ranges::find(node_labels, new_label) == node_labels.end()) {
          node_labels.emplace_back(new_label);
        }
      }
    }
    return true;
  }

  void SetCursor::close() {
    child->close();
  }

  PhysicalSetOp::PhysicalSetOp(std::vector<String> aliases,
    std::vector<std::optional<std::vector<String>>> labels,
    std::vector<std::optional<std::vector<std::pair<String, Value>>>> properties, PhysicalOpPtr child):
    PhysicalOpUnaryChild(std::move(child)),
    aliases(std::move(aliases)),
    labels(std::move(labels)),
    properties(std::move(properties)) {}

  RowCursorPtr PhysicalSetOp::open(struct ExecContext& ctx) {
    return std::make_unique<SetCursor>(
      std::move(child->open(ctx)),
      aliases,
      labels,
      properties
    );
  }


  CreateCursor::CreateCursor(RowCursorPtr child,
                             std::vector<std::variant<planner::CreateNodeSpec, planner::CreateEdgeSpec>> items, storage::GraphDB* db):
    child(std::move(child)),
    items(std::move(items)),
    db(db) {}

  bool CreateCursor::next(Row& out) {
    if (child == nullptr) {
      for (const auto& create_pattern : items) {
        if (create_pattern.index() != 0) {
          throw std::runtime_error("Error invalid syntax for create edge");
        }
        const auto& node_spec = std::get<planner::CreateNodeSpec>(create_pattern);
        std::unordered_map<String, Value> m(node_spec.properties.begin(), node_spec.properties.end());
        db->create_node(node_spec.labels, std::move(m));
      }
      return false;
    }
    if (!child->next(out)) {
      return false;
    }

    for (const auto& create_pattern : items) {
      if (create_pattern.index() == 0) {
        const auto& node_spec = std::get<planner::CreateNodeSpec>(create_pattern);
        std::unordered_map<String, Value> m(node_spec.properties.begin(), node_spec.properties.end());
        db->create_node(node_spec.labels, std::move(m));
        continue;
      }
      const auto& edge_spec = std::get<planner::CreateEdgeSpec>(create_pattern);
      ;

      const auto& src = out.slots[out.slots_mapping.map(edge_spec.src_alias)];
      const auto& dst = out.slots[out.slots_mapping.map(edge_spec.src_alias)];
      if (src.index() != 0 || dst.index() != 0) {
        throw std::runtime_error("Edge must be between vertexes");
      }

      if (edge_spec.direction == frontend::EdgeDirection::Right ||
        edge_spec.direction == frontend::EdgeDirection::Undirected) {
        db->create_edge(
          std::get<0>(src)->id,
          std::get<0>(dst)->id,
          edge_spec.label,
          std::move(std::unordered_map(edge_spec.properties.begin(), edge_spec.properties.end()))
        );
        }
      if (edge_spec.direction == frontend::EdgeDirection::Left ||
        edge_spec.direction == frontend::EdgeDirection::Undirected) {
        db->create_edge(
          std::get<0>(dst)->id,
          std::get<0>(src)->id,
          edge_spec.label,
          std::move(std::unordered_map(edge_spec.properties.begin(), edge_spec.properties.end()))
        );
        }
    }
    return true;
  }

  void CreateCursor::close() {
    child->close();
  }

  PhysicalCreateOp::PhysicalCreateOp(std::vector<std::variant<planner::CreateNodeSpec, planner::CreateEdgeSpec>> items):
    PhysicalOpUnaryChild(nullptr),
    items(std::move(items))
  {}

  PhysicalCreateOp::PhysicalCreateOp(std::vector<std::variant<planner::CreateNodeSpec, planner::CreateEdgeSpec>> items,
    PhysicalOpPtr child):
    PhysicalOpUnaryChild(std::move(child)),
    items(std::move(items))
  {}

  RowCursorPtr PhysicalCreateOp::open(ExecContext& ctx) {
    return std::make_unique<CreateCursor>(
      (child == nullptr ? nullptr : child->open(ctx)),
      items,
      ctx.db
    );
  }

  DeleteCursor::DeleteCursor(RowCursorPtr child, std::vector<String> aliases, storage::GraphDB* db):
    child(std::move(child)),
    aliases(std::move(aliases)),
    db{db} {}

  bool DeleteCursor::next(Row& out) {
    if (!child->next(out)) {
      return false;
    }
    for (const auto& alias : aliases) {
      size_t alias_idx = out.slots_mapping.map(alias);
      const RowSlot& row_slot = out.slots[alias_idx];
      if (row_slot.index() == 2) {
        continue;
      }
      if (row_slot.index() == 0) {
        db->delete_node(std::get<Node*>(row_slot)->id);
      } else {
        db->delete_edge(std::get<Edge*>(row_slot)->id);
      }
    }
    return true;
  }

  void DeleteCursor::close() {
    child->close();
  }

  PhysicalPlan::PhysicalPlan(PhysicalOpPtr root): root(std::move(root)) {}


  template <class Range, class F>
  String Join(const Range& r, std::string_view sep, F&& to_string) {
    String out;
    bool first = true;
    for (const auto& x : r) {
      if (!first) {
        out += sep;
      }
      first = false;
      out += to_string(x);
    }
    return out;
  }

  std::string ValueDebugString(const Value& v) {
    return std::visit([](const auto& x) -> std::string {
      using T = std::decay_t<decltype(x)>;
      if constexpr (std::is_same_v<T, std::string>) {
        return std::format("\"{}\"", x);
      } else if constexpr (std::is_same_v<T, bool>) {
        return x ? "true" : "false";
      } else if constexpr (std::is_arithmetic_v<T>) {
        return std::format("{}", x);
      } else if constexpr (std::is_same_v<T, std::monostate>) {
        return "null";
      } else {
        return "<value>";
      }
    }, v);
  }

  std::string PropertiesDebugString(const std::vector<std::pair<String, Value>>& props) {
    return std::format(
      "{{{}}}",
      Join(props, ", ", [](const auto& kv) {
        return std::format("{}: {}", kv.first, ValueDebugString(kv.second));
      })
    );
  }

  std::string LabelsDebugString(const std::vector<String>& labels) {
    return std::format(
      "[{}]",
      Join(labels, ", ", [](const auto& s) {
        return std::format("\"{}\"", s);
      })
    );
  }

  std::string OptionalLabelsDebugString(const std::optional<std::vector<String>>& labels) {
    if (!labels.has_value()) {
      return "";
    }
    return std::format("labels={}", LabelsDebugString(*labels));
  }

  std::string OptionalPropertiesDebugString(
    const std::optional<std::vector<std::pair<String, Value>>>& props
  ) {
    if (!props.has_value()) {
      return "";
    }
    return std::format("properties={}", PropertiesDebugString(*props));
  }

  std::string CreateNodeSpecDebugString(const planner::CreateNodeSpec& spec) {
    return std::format(
      "Node(labels={}, properties={})",
      LabelsDebugString(spec.labels),
      PropertiesDebugString(spec.properties)
    );
  }

  std::string CreateEdgeSpecDebugString(const planner::CreateEdgeSpec& spec) {
    std::string dir;
    switch (spec.direction) {
    case frontend::EdgeDirection::Right:      dir = "->"; break;
    case frontend::EdgeDirection::Left:       dir = "<-"; break;
    case frontend::EdgeDirection::Undirected: dir = "--"; break;
    }

    return std::format(
      "Edge(src={}, dst={}, dir={}, label=\"{}\", properties={})",
      spec.src_alias,
      spec.dst_alias,
      dir,
      spec.label,
      PropertiesDebugString(spec.properties)
    );
  }

  std::string ReturnItemDebugString(const frontend::ReturnItem& item) {
    if (item.item.index() == 0) {
      return std::get<0>(item.item);
    }
    const auto& prop = std::get<1>(item.item);
    return std::format("{}.{}", prop.alias, prop.property);
  }

  String LabelIndexSeekOp::DebugString() const {
    return std::format("LabelIndexSeek(label=\"{}\", as={})", label, out_alias);
  }

  String NodeScanOp::DebugString() const {
    return std::format("NodeScan(as={})", out_alias);
  }

  template <bool edge_outgoing>
  String ExpandOp<edge_outgoing>::DebugString() const {
    return std::format(
      "Expand({} {} {}, type={})",
      src_alias,
      (edge_outgoing ? "->" : "<-"),
      dst_alias,
      (edge_type.has_value() ? std::format("\"{}\"", edge_type.value()) : "*")
    );
  }

  String FilterOp::DebugString() const {
    return std::format(
      "Filter({})",
      predicate ? predicate->DebugString() : "<null>"
    );
  }

  String ProjectOp::DebugString() const {
    return std::format(
      "Project({})",
      Join(items, ", ", [](const auto& item) {
        return ReturnItemDebugString(item);
      })
    );
  }

  String LimitOp::DebugString() const {
    return std::format("Limit({})", limit_size);
  }

  String NestedLoopJoinOp::DebugString() const {
    if (predicate) {
      return std::format("NestedLoopJoin(on={})", predicate->DebugString());
    }
    return "NestedLoopJoin(cross)";
  }

  String KeyHashJoinOp::DebugString() const {
    return std::format(
      "HashJoin(on {}.{} = {}.{})",
      left_alias, left_feature_key,
      right_alias, right_feature_key
    );
  }

  String PhysicalSetOp::DebugString() const {
    std::string ans = "Set(";
    for (size_t i = 0; i < aliases.size(); ++i) {
      if (i) {
        ans += ", ";
      }
      ans += aliases[i];
      ans += " {";

      bool first = true;
      if (labels[i].has_value()) {
        ans += "labels=" + LabelsDebugString(labels[i].value());
        first = false;
      }
      if (properties[i].has_value()) {
        if (!first) {
          ans += ", ";
        }
        ans += "properties=" + PropertiesDebugString(properties[i].value());
      }

      ans += "}";
    }
    ans += ")";
    return ans;
  }

  String PhysicalCreateOp::DebugString() const {
    return std::format(
      "Create({})",
      Join(items, ", ", [](const auto& item) {
        return std::visit([](const auto& spec) -> String {
          using T = std::decay_t<decltype(spec)>;
          if constexpr (std::is_same_v<T, planner::CreateNodeSpec>) {
            return CreateNodeSpecDebugString(spec);
          } else {
            return CreateEdgeSpecDebugString(spec);
          }
        }, item);
      })
    );
  }

  String PhysicalDelete::DebugString() const {
    return std::format(
      "Delete({})",
      Join(aliases, ", ", [](const auto& a) {
        return a;
      })
    );
  }
} // namespace
