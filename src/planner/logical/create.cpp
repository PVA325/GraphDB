#include "planner/query_planner.hpp"

namespace graph::logical {

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
  optimizer::CostModel* cost_model, storage::GraphDB* db) const {
  if (!child) {
    return std::make_pair(
    std::make_unique<exec::PhysicalCreateOp>(
        items,
        nullptr
      ),
      optimizer::CostEstimate{}
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
}