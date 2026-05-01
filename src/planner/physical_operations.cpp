#include <format>

#include "planner/query_planner.hpp"

namespace graph::exec {
  bool SlotMapping::key_exists(const String& key) const {
    return alias_to_slot.contains(key);
  }

  size_t SlotMapping::map(const String& key) const {
    return alias_to_slot.at(key);
  }
  size_t SlotMapping::map_and_check(const String& key, const String& err_msg) const {
    auto it = alias_to_slot.find(key);
    if (it == alias_to_slot.end()) {
      throw std::runtime_error(err_msg);
    }
    return it->second;
  }

  void SlotMapping::add_map(const String& key, size_t idx) {
    alias_to_slot.emplace(key, idx);
  }

  NodeScanCursor::NodeScanCursor(
    std::unique_ptr<storage::NodeCursor> nodes_cursor,
    String out_alias) :
    nodes_cursor(std::move(nodes_cursor)),
    out_alias(std::move(out_alias)) {}

  bool NodeScanCursor::next(Row &out) {
    Node *node_ptr;

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

  void NodeScanCursor::close() {}

  LabelIndexSeekOp::LabelIndexSeekOp(
    String alias, String label) :
    out_alias(std::move(alias)),
    label(std::move(label)) {}

  RowCursorPtr LabelIndexSeekOp::open(ExecContext &ctx) {
    return std::make_unique<NodeScanCursor>(
      std::move(ctx.db->nodes_with_label(label)),
      out_alias
    );
  }

  template<typename T>
  bool isSubset(const std::vector<T>& larger, const std::vector<T>& smaller) {
    return std::all_of(smaller.begin(), smaller.end(), [&](const T& x) {
        return std::find(larger.begin(), larger.end(), x) != larger.end();
    });
  }

  NodePropertyFilterCursor::NodePropertyFilterCursor(RowCursorPtr child_cursor, String alias,
    std::vector<String> labels, std::vector<std::pair<String, Value>> properties):
    child_cursor(std::move(child_cursor)),
    alias(std::move(alias)),
    labels(std::move(labels)),
    properties(std::move(properties)) {}

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
      std::vector<std::pair<String, Value> > p(node->properties.begin(), node->properties.end());
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

  NodePropertyFilterOp::NodePropertyFilterOp(PhysicalOpPtr child, String alias, std::vector<String> labels,
    std::vector<std::pair<String, Value>> properties):
    PhysicalOpUnaryChild(std::move(child)),
    alias(std::move(alias)),
    labels(std::move(labels)),
    properties(std::move(properties))
  {}

  RowCursorPtr NodePropertyFilterOp::open(ExecContext& ctx) {
    return std::make_unique<NodePropertyFilterCursor>(
      std::move(child->open(ctx)),
      alias,
      labels,
      properties
    );
  }

  RowCursorPtr NodeScanOp::open(ExecContext &ctx) {
    return std::make_unique<NodeScanCursor>(
      std::move(ctx.db->all_nodes()),
      out_alias
    );
  }

  FilterCursor::FilterCursor(
    RowCursorPtr child_cursor,
    ast::Expr *predicate) :
    child_cursor(std::move(child_cursor)),
    predicate(predicate) {}

  bool FilterCursor::next(Row &out) {
    bool was_writing = false;
    Row mark = out;
    while (child_cursor->next(out)) {
      ast::EvalContext eval_ctx{out};
      Value val = (*predicate)(eval_ctx);
      if (PlannerUtils::ValueToBool(val)) {
        was_writing = true;
        break;
      }
      out = mark;
    }
    return was_writing;
  }

  void FilterCursor::close() {
    child_cursor->close();
  }

  RowCursorPtr FilterOp::open(ExecContext &ctx) {
    return std::make_unique<FilterCursor>(
      std::move(child->open(ctx)),
      predicate
    );
  }

  FilterOp::FilterOp(
    ast::Expr *predicate,
    PhysicalOpPtr child) :
    PhysicalOpUnaryChild(std::move(child)),
    predicate(predicate) {}

  ProjectCursor::ProjectCursor(
    RowCursorPtr child_cursor,
    std::vector<ast::ReturnItem> items) :
    child_cursor(std::move(child_cursor)),
    items(std::move(items)) {}

  bool ProjectCursor::next(Row &out) {
    if (!child_cursor->next(out)) {
      return false;
    }
    Row new_row;
    for (const auto &ret_item: items) {
      if (ret_item.return_item.index() == 0) {
        String alias = std::get<0>(ret_item.return_item);
        size_t old_row_idx = out.slots_mapping.map_and_check(
          alias,
          std::format("PhysicalProject: Error, no alias \"{}\"", alias)
        );

        new_row.AddValue(
          out.slots[old_row_idx],
          alias,
          std::format("PhysicalProject: Error, alias {} is already in use", alias)
        );
        continue;
      }
      ast::PropertyExpr prop = std::get<1>(ret_item.return_item);
      size_t old_row_idx = out.slots_mapping.map_and_check(
        prop.alias,
        std::format("PhysicalProject: Error, no alias \"{}\"", prop.alias)
      );
      const auto &cur_prop_src = out.slots[old_row_idx];
      String new_alias = prop.alias + "." + prop.property;
      if (cur_prop_src.index() == 2) {
        throw std::runtime_error(std::format("PhysicalProject: Error, trying to project {}.{}, but {} is Value", prop.alias, prop.property, prop.alias));
      }

      String error_msg = std::format("PhysicalProject: {} is already in use", new_alias);
      if (cur_prop_src.index() == 0) {
        auto node = std::get<Node *>(cur_prop_src);
        new_row.AddValue(
          node->properties.at(prop.property),
          new_alias,
          error_msg
        );
        continue;
      }
      auto edge = std::get<Edge *>(cur_prop_src);
      new_row.AddValue(
        edge->properties.at(prop.property),
        new_alias,
        error_msg
      );
    }
    out = new_row;
    return true;
  }

  void ProjectCursor::close() {
    child_cursor->close();
  }

  ProjectOp::ProjectOp(std::vector<ast::ReturnItem> items, PhysicalOpPtr child) :
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
    const ast::Expr* pred,
    ExecContext& ctx):
    left_cursor(std::move(left_cursor)),
    right_cursor(nullptr),
    right_operation(right_operation),
    predicate(pred),
    ctx(ctx)
  {}

  bool NestedLoopJoinCursor::next(Row &out) {
    Row right_row;
    while (true) {
      right_row.clear();
      if (!right_cursor || !right_cursor->next(right_row)) {
        left_row.clear();
        if (!left_cursor->next(left_row)) { /// left_row must have slots mapping
          return false;
        }
        right_cursor = std::move(right_operation->open(ctx));
      } else {
        break;
      }
    }
    Row new_row = MergeRows(left_row, right_row);
    ast::EvalContext exec_ctx{new_row};
    if (predicate == nullptr || PlannerUtils::ValueToBool((*predicate)(exec_ctx))) {
      out = std::move(new_row);
      return true;
    }
    return false;
  }

  void NestedLoopJoinCursor::close() {
    left_cursor->close();
    right_cursor->close();
  }

  NestedLoopJoinOp::NestedLoopJoinOp(
    PhysicalOpPtr left,
    PhysicalOpPtr right,
    ast::Expr* pred):
    PhysicalOpBinaryChild(std::move(left), std::move(right)),
    predicate(pred)
  {}

  NestedLoopJoinOp::NestedLoopJoinOp(
    PhysicalOpPtr left,
    PhysicalOpPtr right):
    PhysicalOpBinaryChild(std::move(left), std::move(right)),
    predicate(nullptr)
  {}


  std::string PhysicalPlan::DebugString() const {
    return root->SubtreeDebugString();
  }

  RowCursorPtr NestedLoopJoinOp::open(ExecContext &ctx) {
    return std::make_unique<NestedLoopJoinCursor>(
      left->open(ctx),
      right.get(),
      predicate,
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
  right_feature_key(std::move(right_feature_key)) {
    Row cur;
    while (left_cursor->next(cur)) {
      size_t slot_idx = cur.slots_mapping.map_and_check(
        left_alias,
        std::format("KeyHashJoinCursor: Error, No source alias: {}", left_alias)
      );
      Value val = GetFeatureFromSlot(cur.slots[slot_idx], left_feature_key);

      left_rows[val].emplace_back(cur);
    }
    it_left = left_rows.end();
  }

  bool KeyHashJoinCursor::next(Row& out) {
    while (it_left == left_rows.end() || it_left->second.size() <= vec_left_idx) {
      if (!right_cursor->next(last_right_row)) {
        return false;
      }
      size_t slot_idx = last_right_row.slots_mapping.map_and_check(
        right_alias,
        std::format("KeyHashJoinCursor: Error, No source alias: {}", right_alias)
      );
      Value right_value = GetFeatureFromSlot(last_right_row.slots[slot_idx], right_feature_key);

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

  Value GetFeatureFromSlot(const RowSlot& slot, const String& feature_key) {
    if (slot.index() == 2) {
      if (!feature_key.empty()) {
        throw std::runtime_error("Error during KeyHashJoin: invalid alias or property");
      }
      return std::get<2>(slot);
    }
    if (slot.index() == 1) {
      return std::get<1>(slot)->properties[feature_key];
    }
    return std::get<0>(slot)->properties[feature_key];
  }

  Row MergeRows(Row &first, Row &second) {
    Row ans;
    ans.slots.insert(ans.slots.end(), first.slots.begin(), first.slots.end());
    ans.slots.insert(ans.slots.end(), second.slots.begin(), second.slots.end());

    ans.slots_names.insert(ans.slots_names.end(), first.slots_names.begin(), first.slots_names.end());
    ans.slots_names.insert(ans.slots_names.end(), second.slots_names.begin(), second.slots_names.end());

    for (size_t i = 0; i < ans.slots_names.size(); ++i) {
      ans.slots_mapping.add_map(ans.slots_names[i], i);
    }

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

  SetCursor::SetCursor(RowCursorPtr child,
    std::vector<logical::LogicalSet::Assignment> assignments,
    storage::GraphDB* db):
    child(std::move(child)),
    assignments(std::move(assignments)),
    db(db){}

  bool SetCursor::next(Row& out) {
    if (!child->next(out)) {
      return false;
    }
    for (const auto& assignment : assignments) {
      const String& cur_alias = assignment.alias;
      size_t cur_slot_idx = out.slots_mapping.map_and_check(
        cur_alias,
        std::format("SetCursor: Error, no src alias {}", cur_alias));

      RowSlot& row_slot = out.slots[cur_slot_idx];
      if (row_slot.index() == 2) {
        throw std::runtime_error("Error while setting, invalid type");
      }

      if (std::holds_alternative<Node*>(row_slot)) {
        auto* node = std::get<Node*>(row_slot);
        for (const auto& [key, val] : assignment.properties) {
          db->set_node_property(node->id, key, val);
        }

        // for (const auto& label : assignment.labels) {
        //   db->add_node_label(node->id, label);
        // }
      } else {
        auto* edge = std::get<Edge*>(row_slot);
        for (const auto& [key, val] : assignment.properties) {
          db->set_edge_property(edge->id, key, val);
        }

        // for (const auto& label : assignment.labels) {
        //   db->add_node_label(edge->id, label);
        // }
      }

      auto& obj_properties = (row_slot.index() == 0 ? std::get<0>(row_slot)->properties : std::get<1>(row_slot)->properties);
      for (const auto& [key, val] : assignment.properties) {
        obj_properties[key] = val;
      }

      if (assignment.labels.empty()) {
        continue;
      }

      if (std::holds_alternative<Edge*>(row_slot)) {
        if (assignment.labels.size() != -1) {
          throw std::runtime_error("SetCursor: Error, edge can have only 1 type");
        }
        auto& edge_type = std::get<1>(row_slot)->type;
        edge_type = assignment.labels.back();
        continue;
      }
      auto& node_labels = std::get<0>(row_slot)->labels;
      for (const auto& new_label : assignment.labels) {
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

  PhysicalSetOp::PhysicalSetOp(PhysicalOpPtr child, std::vector<logical::LogicalSet::Assignment> assignments):
    PhysicalOpUnaryChild(std::move(child)),
    assignments(std::move(assignments)) {}

  RowCursorPtr PhysicalSetOp::open(struct ExecContext& ctx) {
    return std::make_unique<SetCursor>(
      std::move(child->open(ctx)),
      assignments,
      ctx.db
    );
  }


  CreateCursor::CreateCursor(
    RowCursorPtr child,
    std::vector<std::variant<logical::CreateNodeSpec, logical::CreateEdgeSpec>> items,
    storage::GraphDB* db):
    child(std::move(child)),
    items(std::move(items)),
    db(db) {}

  bool CreateCursor::next(Row& out) {
    if (child == nullptr) {
      if (was_writing) {
        return false;
      }
      was_writing = true;
      for (const auto& create_pattern : items) {
        if (std::holds_alternative<logical::CreateNodeSpec>(create_pattern)) {
          const auto& node_spec = std::get<logical::CreateNodeSpec>(create_pattern);
          std::unordered_map<String, Value> m(node_spec.properties.begin(), node_spec.properties.end());

          storage::NodeId created_node = db->create_node(node_spec.labels, m);
          out.AddValue(db->get_node(created_node), node_spec.dst_alias,
                std::format("CreateCursor: Error, alias {} already exists", node_spec.dst_alias));
        } else {
          const auto& edge_spec = std::get<logical::CreateEdgeSpec>(create_pattern);

          size_t src_idx = out.slots_mapping.map_and_check(
            edge_spec.src_alias,
            std::format("CreateCursor: Error, no src alias {} for edge creating", edge_spec.src_alias)
          );
          size_t dst_idx = out.slots_mapping.map_and_check(
            edge_spec.dst_node_alias,
            std::format("CreateCursor: Error, no dst alias {} for edge creating", edge_spec.dst_node_alias)
          );

          if (!std::holds_alternative<Node*>(out.slots[src_idx]) ||
              !std::holds_alternative<Node*>(out.slots[dst_idx])) {
            throw std::runtime_error("CreateCursor: Error, edge should be between vertexes");
          }
          const auto& src = std::get<Node*>(out.slots[src_idx]);
          const auto& dst = std::get<Node*>(out.slots[dst_idx]);

          std::unordered_map<String, Value> m(edge_spec.properties.begin(), edge_spec.properties.end());

          storage::EdgeId created_edge = db->create_edge(src->id, dst->id, edge_spec.edge_type, m);
          out.AddValue(db->get_edge(created_edge), edge_spec.edge_alias,
                std::format("CreateCursor: Error, alias {} already exists", edge_spec.edge_alias));
        }
      }
      return true;
    }
    if (!child->next(out)) {
      return false;
    }

    for (const auto& create_pattern : items) {
      if (create_pattern.index() == 0) {
        const auto& node_spec = std::get<logical::CreateNodeSpec>(create_pattern);
        std::unordered_map<String, Value> m(node_spec.properties.begin(), node_spec.properties.end());
        db->create_node(node_spec.labels, m);
        continue;
      }
      const auto& edge_spec = std::get<logical::CreateEdgeSpec>(create_pattern);

      const auto& src = out.slots[
        out.slots_mapping.map_and_check(
          edge_spec.src_alias,
          std::format("CreateCursor: Error, invalid src alias {}", edge_spec.src_alias))
      ];
      const auto& dst = out.slots[
        out.slots_mapping.map_and_check(
            edge_spec.dst_node_alias,
            std::format("CreateCursor: Error, invalid src alias {}", edge_spec.dst_node_alias)
          )
        ];
      if (src.index() != 0 || dst.index() != 0) {
        throw std::runtime_error("CreateCursor: Error, edge must be between vertexes");
      }

      if (edge_spec.direction == ast::EdgeDirection::Right ||
        edge_spec.direction == ast::EdgeDirection::Undirected) {
        db->create_edge(
          std::get<0>(src)->id,
          std::get<0>(dst)->id,
          edge_spec.edge_type,
          std::unordered_map(edge_spec.properties.begin(), edge_spec.properties.end())
        );
        }
      if (edge_spec.direction == ast::EdgeDirection::Left ||
        edge_spec.direction == ast::EdgeDirection::Undirected) {
        db->create_edge(
          std::get<0>(dst)->id,
          std::get<0>(src)->id,
          edge_spec.edge_type,
          std::unordered_map(edge_spec.properties.begin(), edge_spec.properties.end())
        );
        }
    }
    return true;
  }

  void CreateCursor::close() {
    child->close();
  }

  PhysicalCreateOp::PhysicalCreateOp(std::vector<std::variant<logical::CreateNodeSpec, logical::CreateEdgeSpec>> items):
    PhysicalOpUnaryChild(nullptr),
    items(std::move(items))
  {}

  PhysicalCreateOp::PhysicalCreateOp(std::vector<std::variant<logical::CreateNodeSpec, logical::CreateEdgeSpec>> items,
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
      size_t alias_idx = out.slots_mapping.map_and_check(
        alias,
        std::format("DeleteCursor: Error, invalid source alias {}", alias)
      );
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
  PhysicalDeleteOp::PhysicalDeleteOp(std::vector<String> aliases, PhysicalOpPtr child):
    PhysicalOpUnaryChild(std::move(child)),
    aliases(std::move(aliases)) {}

  RowCursorPtr PhysicalDeleteOp::open(ExecContext& ctx) {
    return std::make_unique<DeleteCursor>(
      std::move(child->open(ctx)),
      aliases,
      ctx.db
    );
  }

  SortCursor::SortCursor(RowCursorPtr child, std::vector<ast::OrderItem> items):
    child(std::move(child)),
    items(std::move(items)) {
    Row cur;
    while (child->next(cur)) {
      rows.push_back(std::move(cur)); // move ok?
    }
    auto cmp = [items = std::move(this->items)](const Row& a, const Row& b) -> bool {
      for (const auto& item : items) {
        String err = std::format("SortCursor: Error, no such alias {}", item.property.alias);
        size_t a_idx = a.slots_mapping.map_and_check(item.property.alias, err);
        size_t b_idx = b.slots_mapping.map_and_check(item.property.alias, err);

        Value a_feature = GetFeatureFromSlot(a.slots[a_idx], item.property.property);
        Value b_feature = GetFeatureFromSlot(a.slots[a_idx], item.property.property);

        if (a_feature.index() != b_feature.index()) {
          throw std::runtime_error("SortCursor: Error type mistmatch");
        }

        if (a_feature != b_feature) {
          bool is_less = std::visit([](auto&& a, auto&& b) -> bool {
            using A = std::decay_t<decltype(a)>;
            using B = std::decay_t<decltype(b)>;

            if constexpr (std::is_same_v<A, B>) {
              return a < b;
            }
            return false;
          }, a_feature, b_feature);
          return (item.direction == ast::OrderDirection::Asc ? is_less : !is_less);
        }
      }
      return true;
    };
    std::sort(rows.begin(), rows.end(), cmp);
    std::reverse(rows.begin(), rows.end());
  }

  bool SortCursor::next(Row& out) {
    if (rows.empty()) {
      return false;
    }
    out = std::move(rows.back());
    rows.pop_back();
    return true;
  }

  void SortCursor::close() {
    child->close();
  }

  PhysicalSortOp::PhysicalSortOp(PhysicalOpPtr child, std::vector<ast::OrderItem> items):
    PhysicalOpUnaryChild(std::move(child)),
    items(std::move(items)) {}

  RowCursorPtr PhysicalSortOp::open(ExecContext& ctx) {
    return std::make_unique<SortCursor>(
      std::move(child->open(ctx)),
      items
    );
  }

  PhysicalPlan::PhysicalPlan(PhysicalOpPtr root): root(std::move(root)) {}
}  // namespace graph::exec


