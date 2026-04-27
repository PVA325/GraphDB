#include "planner.hpp"

namespace graph::logical {
LogicalOpUnaryChild::LogicalOpUnaryChild(LogicalOpPtr child) :
  child(std::move(child)) {
}

LogicalOpBinaryChild::LogicalOpBinaryChild(LogicalOpPtr left, LogicalOpPtr right) :
  left(std::move(left)),
  right(std::move(right)) {
}

LogicalScan::LogicalScan(std::vector<String> labels, String dst_alias) :
  AliasedLogicalOp(std::move(dst_alias)),
  labels(std::move(labels)) {
}

LogicalScan::LogicalScan(std::vector<String> labels, String dst_alias,
                         std::vector<std::pair<String, Value>> property_filters) :
  AliasedLogicalOp(std::move(dst_alias)),
  labels(std::move(labels)),
  property_filters(std::move(property_filters)) {
}

BuildPhysicalType LogicalScan::BuildPhysical(
  exec::ExecContext& ctx, planner::CostModel* cost_model, storage::GraphDB* db) const {
  exec::PhysicalOpPtr physical_root;

  auto node_scan_cost = cost_model->EstimateAllNodeScan(db, *this);
  auto [index_label_scan_cost, label_for_index] = cost_model->EstimateIndexSeekLabel(db, *this);
  planner::CostEstimate child_cost;
  if (node_scan_cost.total() > index_label_scan_cost.total()) {
    physical_root = std::make_unique<exec::LabelIndexSeekOp>(
      dst_alias,
      label_for_index
    );
    child_cost = index_label_scan_cost;
  } else {
    physical_root = std::make_unique<exec::NodeScanOp>(
      dst_alias
    );
    child_cost = node_scan_cost;
  }

  physical_root = std::move(
    std::make_unique<exec::NodePropertyFilterOp>(
      std::move(physical_root),
      dst_alias,
      labels,
      property_filters
    )
  );

  return {std::move(physical_root), child_cost};
}

LogicalExpand::LogicalExpand(LogicalOpPtr child, String src_alias, String edge_alias, String dst_alias,
                             std::optional<String> edge_type, std::vector<String> dst_vertex_labels,
                             std::vector<std::pair<String, Value>> dst_vertex_properties,
                             ast::EdgeDirection direction) :
  AliasedLogicalOp(std::move(dst_alias)),
  child(std::move(child)),
  src_alias(std::move(src_alias)),
  edge_alias(std::move(edge_alias)),
  edge_type(std::move(edge_type)),
  dst_vertex_labels(std::move(dst_vertex_labels)),
  dst_vertex_properties(std::move(dst_vertex_properties)),
  direction(direction) {
}


BuildPhysicalType LogicalExpand::BuildPhysical(exec::ExecContext& ctx,
                                               planner::CostModel* cost_model, storage::GraphDB* db) const {
  exec::PhysicalOpPtr physical_root;
  auto child_answer = child->BuildPhysical(ctx, cost_model, db);

  if (direction == ast::EdgeDirection::Right) {
    physical_root = std::make_unique<exec::ExpandOutgoingOp>(
      src_alias,
      edge_alias,
      dst_alias,
      edge_type,
      std::move(child_answer.first)
    );
  } else {
    physical_root = std::make_unique<exec::ExpandIngoingOp>(
      src_alias,
      edge_alias,
      dst_alias,
      edge_type,
      std::move(child_answer.first)
    );
  }
  if (!dst_vertex_labels.empty() || !dst_vertex_properties.empty()) {
    physical_root = std::make_unique<exec::NodePropertyFilterOp>(
      std::move(physical_root),
      dst_alias,
      dst_vertex_labels,
      dst_vertex_properties
    );
  }
  planner::CostEstimate cost = cost_model->EstimateExpand(db, *this, child_answer.second);
  return std::make_pair(std::move(physical_root), cost);
}

LogicalFilter::LogicalFilter(LogicalOpPtr child, std::unique_ptr<ast::Expr> predicate) :
  LogicalOpUnaryChild(std::move(child)),
  predicate(std::move(predicate)) {
}

BuildPhysicalType LogicalFilter::BuildPhysical(
  exec::ExecContext& ctx,
  planner::CostModel* cost_model,
  storage::GraphDB* db) const {
  auto child_build = child->BuildPhysical(ctx, cost_model, db);
  return std::make_pair(
    std::make_unique<exec::FilterOp>(
      predicate.get(),
      std::move(child_build.first)
    ),
    cost_model->EstimateFilter(db, child_build.second, predicate.get())
  );
}

LogicalProject::LogicalProject(LogicalOpPtr child, std::vector<ast::ReturnItem> items) :
  LogicalOpUnaryChild(std::move(child)),
  items(std::move(items)) {
}

BuildPhysicalType LogicalProject::BuildPhysical(
  exec::ExecContext& ctx,
  planner::CostModel* cost_model,
  storage::GraphDB* db) const {
  auto child_build = child->BuildPhysical(ctx, cost_model, db);
  return std::make_pair(
    std::make_unique<exec::ProjectOp>(
      items,
      std::move(child_build.first)
      ),
      cost_model->EstimateProject(db, child_build.second, *this)
  );
}

LogicalLimit::LogicalLimit(LogicalOpPtr child, size_t limit_size) :
  LogicalOpUnaryChild(std::move(child)),
  limit_size(limit_size) {
}

BuildPhysicalType LogicalLimit::BuildPhysical(
  exec::ExecContext& ctx,
  planner::CostModel* cost_model,
  storage::GraphDB* db) const {
  auto child_build = child->BuildPhysical(ctx, cost_model, db);
  return std::make_pair(
    std::make_unique<exec::LimitOp>(
      limit_size,
      std::move(child_build.first)
    ),
    cost_model->EstimateLimit(db, child_build.second, limit_size)
  );
}

LogicalSort::LogicalSort(LogicalOpPtr child, std::vector<ast::OrderItem> items) :
  LogicalOpUnaryChild(std::move(child)),
  items(std::move(items)) {
}

BuildPhysicalType LogicalSort::BuildPhysical(
  exec::ExecContext& ctx,
  planner::CostModel* cost_model,
  storage::GraphDB* db) const {
  auto child_build = child->BuildPhysical(ctx, cost_model, db);
  return std::make_pair(
    std::make_unique<exec::PhysicalSortOp>(
      std::move(child_build.first),
      items
    ),
    cost_model->EstimateSort(db, child_build.second)
  );
}

LogicalJoin::LogicalJoin(LogicalOpPtr left, LogicalOpPtr right) :
  LogicalOpBinaryChild(std::move(left), std::move(right)), predicate{nullptr} {
}

LogicalJoin::LogicalJoin(
  LogicalOpPtr left, LogicalOpPtr right,
  std::unique_ptr<ast::Expr> predicate) :
  LogicalOpBinaryChild(std::move(left), std::move(right)),
  predicate(std::move(predicate)) {
}

BuildPhysicalType LogicalJoin::BuildPhysical(
  exec::ExecContext& ctx,
  planner::CostModel* cost_model,
  storage::GraphDB* db) const {
  // return std::make_unique<exec::KeyHashJoinOp>(
  //   std::move(left->BuildPhysical(ctx)),
  //   std::move(right->BuildPhysical(ctx)),
  //   left_alias, right_alias,
  //   left_feature_ket, right_feature_key
  // );
  auto left_build = left->BuildPhysical(ctx, cost_model, db);
  auto right_build = right->BuildPhysical(ctx, cost_model, db);

  return std::make_pair(
    std::make_unique<exec::NestedLoopJoinOp>(
      std::move(left_build.first),
      std::move(right_build.first),
      predicate.get()
    ),
    cost_model->EstimateNestedJoin(db, left_build.second, right_build.second, predicate.get())
  );
}

LogicalSet::LogicalSet(LogicalOpPtr child, std::vector<Assignment> assignment) :
  LogicalOpUnaryChild(std::move(child)),
  assignment(std::move(assignment)) {
}

BuildPhysicalType LogicalSet::BuildPhysical(
  exec::ExecContext& ctx,
  planner::CostModel* cost_model,
  storage::GraphDB* db) const {
  auto child_build = child->BuildPhysical(ctx, cost_model, db);

  return std::make_pair(
    std::make_unique<exec::PhysicalSetOp>(
      std::move(child_build.first),
      assignment
    ),
    cost_model->EstimateSet(db, child_build.second, *this)
  );
}

LogicalDelete::LogicalDelete(LogicalOpPtr child, std::vector<String> target) :
  LogicalOpUnaryChild(std::move(child)),
  target(std::move(target)) {}

BuildPhysicalType LogicalDelete::BuildPhysical(
  exec::ExecContext& ctx,
  planner::CostModel* cost_model,
  storage::GraphDB* db) const {
  auto child_build = child->BuildPhysical(ctx, cost_model, db);

  return std::make_pair(
    std::make_unique<exec::PhysicalDeleteOp>(
      target,
      std::move(child_build.first)
    ),
    cost_model->EstimateDelete(db, child_build.second, *this)
  );
}

CreateNodeSpec::CreateNodeSpec(const ast::NodePattern& pattern) :
  dst_alias(pattern.alias),
  labels(pattern.labels) {
  PlannerUtils::transferProperties(properties, pattern.properties);
}

CreateNodeSpec::CreateNodeSpec(ast::NodePattern&& pattern) :
  dst_alias(std::move(pattern.alias)),
  labels(std::move(pattern.labels)) {
  PlannerUtils::transferProperties(properties, std::move(pattern.properties));
}

CreateEdgeSpec::CreateEdgeSpec(const ast::CreateEdgePattern& pattern) :
  src_alias(pattern.left_node.alias),
  dst_node_alias(pattern.right_node.alias),
  edge_alias(pattern.alias),
  edge_type(pattern.label),
  direction(pattern.direction) {
  PlannerUtils::transferProperties(properties, pattern.properties);
}

CreateEdgeSpec::CreateEdgeSpec(ast::CreateEdgePattern&& pattern) :
  src_alias(std::move(pattern.left_node.alias)),
  dst_node_alias(std::move(pattern.right_node.alias)),
  edge_alias(std::move(pattern.alias)),
  edge_type(std::move(pattern.label)),
  direction(pattern.direction) {
  PlannerUtils::transferProperties(properties, std::move(pattern.properties));
}

LogicalCreate::LogicalCreate(LogicalOpPtr child, const std::vector<ast::CreateItem>& other_items) :
  LogicalOpUnaryChild(std::move(child)) {
  items.reserve(other_items.size());
  for (const auto& item : other_items) {
    if (item.index() == 0) {
      ;
      items.emplace_back(std::move(CreateNodeSpec(std::get<0>(item))));
    } else if (item.index() == 1) {
      items.emplace_back(std::move(CreateEdgeSpec(std::get<1>(item))));
    }
  }
}

BuildPhysicalType LogicalCreate::BuildPhysical(exec::ExecContext& ctx,
  planner::CostModel* cost_model, storage::GraphDB* db) const {
  if (!child) {
    return std::make_pair(
    std::make_unique<exec::PhysicalCreateOp>(
        items,
        nullptr
      ),
      planner::CostEstimate{}
    );
  }
  auto child_build = child->BuildPhysical(ctx, cost_model, db);
  return std::make_pair(
    std::make_unique<exec::PhysicalCreateOp>(
      items,
      std::move(child_build.first)
      ),
      cost_model->EstimateCreate(db, child_build.second, *this)
    );
}
} // namespace graph::logical

graph::String graph::PlannerUtils::EdgeStrByDirection(ast::EdgeDirection dir) {
  if (dir == ast::EdgeDirection::Right) {
    return "->";
  }
  if (dir == ast::EdgeDirection::Left) {
    return "<-";
  }
  return "-";
}

graph::String graph::PlannerUtils::ValueToString(const graph::Value& val) {
  const auto visitor = overloads{
    [](Int cur) -> String { return std::to_string(cur); },
    [](Double cur) -> String { return std::to_string(cur); },
    [](Bool cur) -> String { return (cur ? "true" : "false"); },
    [](String cur) -> String { return std::move(cur); }
  };
  return std::visit(visitor, val);
}

bool graph::PlannerUtils::ValueToBool(graph::Value val) {
  const auto visitor = overloads{
    [](Int cur) -> bool { return cur; },
    [](Double cur) -> bool { return cur != 0.0; },
    [](Bool cur) -> bool { return cur; },
    [](const String& cur) -> bool { return (!cur.empty()); }
  };
  return std::visit(visitor, val);
}

graph::String graph::PlannerUtils::ConcatProperties(const std::vector<std::pair<String, Value>>& v) {
  String ans;
  for (const auto& cur : v) {
    if (!ans.empty()) {
      ans += ", ";
    }
    ans += cur.first + ": " + ValueToString(cur.second);
  }
  return ans;
}

graph::String graph::PlannerUtils::ConcatStrVector(const std::vector<String>& v) {
  String ans;
  for (const auto& cur : v) {
    ans += cur;
  }
  return ans;
}
