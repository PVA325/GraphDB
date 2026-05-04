#include "planner/query_planner.hpp"

namespace graph::logical {
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
  exec::ExecContext& ctx, optimizer::CostModel* cost_model, storage::GraphDB* db) const {
  exec::PhysicalOpPtr physical_root;

  auto node_scan_cost = cost_model->EstimateAllNodeScan(db, *this);
  auto [index_label_scan_cost, label_for_index] = cost_model->EstimateIndexSeekLabel(db, *this);
  optimizer::CostEstimate child_cost;
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

}