#include "planner/query_planner.hpp"

namespace graph::logical {
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
                                               optimizer::CostModel* cost_model, storage::GraphDB* db) const {
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
  optimizer::CostEstimate cost = cost_model->EstimateExpand(db, *this, child_answer.second);
  return std::make_pair(std::move(physical_root), cost);
}

}