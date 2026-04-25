#include <cmath>

#include "planner.hpp"

namespace graph::planner {

CostEstimate DefaultCostModel::EstimateAllNodeScan(
  const storage::GraphDB* db,
  const logical::LogicalScan& scan) const {

  if (!db) {
    throw std::runtime_error("DefaultCostModel(estimate_all_node_scan): Error, db is null");
  }

  double total_nodes = static_cast<double>(db->node_count());
  const double labels_sel = EstimateNodeLabelsSelectivity(scan.labels, db);
  const double prop_sel = EstimateNodePropertySelectivity(scan.property_filters, db);

  CostEstimate node_scan;
  node_scan.row_count = total_nodes * labels_sel * prop_sel;

  node_scan.cpu_cost = total_nodes * kCpuPerRow
                     + node_scan.row_count * kFilterCpu
                     + (total_nodes - node_scan.row_count) * kFilterCpu * 0.3;

  node_scan.io_cost = total_nodes * kSeqReadCost * kIoPerRow;
  node_scan.startup_cost = 1.0;

  return node_scan;
}

std::pair<CostEstimate, String> DefaultCostModel::EstimateIndexSeekLabel(
  const storage::GraphDB* db,
  const logical::LogicalScan& scan) const {

  if (!db) {
    throw std::runtime_error("DefaultCostModel(estimate_index_seek_label): Error, db is null");
  }

  double total_nodes = static_cast<double>(db->node_count());
  const double labels_sel = EstimateNodeLabelsSelectivity(scan.labels, db);
  const auto [best_label_sel, best_label] = EstimateBiggestLabelSelectivity(scan.labels, db);
  const double prop_sel = EstimateNodePropertySelectivity(scan.property_filters, db);
  const double index_rows = best_label_sel * total_nodes;

  CostEstimate index_seek;
  index_seek.row_count = total_nodes * labels_sel * prop_sel;

  index_seek.cpu_cost = index_rows * kCpuPerRow
                      + index_seek.row_count * kFilterCpu
                      + (index_rows - index_seek.row_count) * kFilterCpu * 0.3;

  index_seek.io_cost = index_rows * kRandomReadCost * kIoPerRow;
  index_seek.startup_cost = 20.0;

  return {index_seek, best_label};
}

CostEstimate DefaultCostModel::EstimateExpand(
  const storage::GraphDB* db,
  const logical::LogicalExpand& expand,
  const CostEstimate& input) const {

  if (!db) {
    throw std::runtime_error("DefaultCostModel(estimate_index_seek_label): Error, db is null");
  }

  double total_rows = input.row_count *
    db->avg_out_degree(expand.edge_type.has_value() ? expand.edge_type.value() : "");

  double label_sel = EstimateNodeLabelsSelectivity(expand.dst_vertex_labels, db);
  double prop_sel = EstimateNodePropertySelectivity(expand.dst_vertex_properties, db);

  CostEstimate expand_cost;
  expand_cost.row_count = total_rows * label_sel * prop_sel;

  expand_cost.cpu_cost = input.cpu_cost
                       + total_rows * kCpuPerEdge
                       + total_rows * kFilterCpu;

  expand_cost.io_cost = input.io_cost
                      + total_rows * kIoPerEdge;

  expand_cost.startup_cost = input.startup_cost + 1.0;

  return expand_cost;
}

CostEstimate DefaultCostModel::EstimateFilter(
  const storage::GraphDB* db,
  const CostEstimate& input,
  const ast::Expr* pred) const {

  if (!db) {
    throw std::runtime_error("DefaultCostModel(estimate_filter): Error, db is null");
  }

  CostEstimate filter_cost;
  filter_cost.row_count = input.row_count;

  filter_cost.cpu_cost = input.cpu_cost
                       + input.row_count * kFilterCpu;

  filter_cost.io_cost = input.io_cost;
  filter_cost.startup_cost = input.startup_cost;

  return filter_cost;
}

CostEstimate DefaultCostModel::EstimateNestedJoin(
  const storage::GraphDB* db,
  const CostEstimate& left,
  const CostEstimate& right,
  const ast::Expr* pred) const {

  if (!db) {
    throw std::runtime_error("DefaultCostModel(estimate_nested_join): Error, db is null");
  }

  double pair_count = left.row_count * right.row_count;

  CostEstimate join_cost;
  join_cost.row_count = pair_count;

  join_cost.cpu_cost = left.cpu_cost
                     + right.cpu_cost
                     + pair_count * (kFilterCpu + kCpuPerRow);

  join_cost.io_cost = left.io_cost + right.io_cost;
  join_cost.startup_cost = left.startup_cost + right.startup_cost + 1.0;

  return join_cost;
}

CostEstimate DefaultCostModel::EstimateHashJoin(
  const storage::GraphDB* db,
  const CostEstimate& left,
  const CostEstimate& right,
  const ast::Expr* pred) const {

  if (!db) {
    throw std::runtime_error("DefaultCostModel(estimate_hash_join): Error, db is null");
  }

  double build = left.row_count;
  double probe = right.row_count;

  CostEstimate join_cost;
  join_cost.row_count = left.row_count * right.row_count;

  join_cost.cpu_cost = left.cpu_cost
                     + right.cpu_cost
                     + build * kCpuHashBuild
                     + probe * kCpuHashProbe;

  join_cost.io_cost = left.io_cost
                    + right.io_cost
                    + build * kIoPerRow;

  join_cost.startup_cost = left.startup_cost + right.startup_cost + 1.0;

  return join_cost;
}

CostEstimate DefaultCostModel::EstimateSort(
  const storage::GraphDB* db,
  const CostEstimate& input) const {

  if (!db) {
    throw std::runtime_error("DefaultCostModel(estimate_sort): Error, db is null");
  }

  CostEstimate sort_cost;
  sort_cost.row_count = input.row_count;

  sort_cost.cpu_cost = input.cpu_cost
                     + (input.row_count * std::log2(input.row_count)) * kCpuPerRow;

  sort_cost.io_cost = input.io_cost
                    + input.row_count * kIoPerRow;

  sort_cost.startup_cost = input.startup_cost + 1.0;

  return sort_cost;
}

CostEstimate DefaultCostModel::EstimateLimit(
  const storage::GraphDB* db,
  const CostEstimate& input,
  size_t limit) const {

  if (!db) {
    throw std::runtime_error("DefaultCostModel(estimate_limit): Error, db is null");
  }

  CostEstimate limit_cost;
  limit_cost.row_count = std::min(input.row_count, static_cast<double>(limit));

  limit_cost.cpu_cost = input.cpu_cost
                      + limit_cost.row_count * kCpuPerRow;

  limit_cost.io_cost = input.io_cost;
  limit_cost.startup_cost = input.startup_cost;

  return limit_cost;
}

double DefaultCostModel::EstimateNodePropertySelectivity(const std::vector<std::pair<String, Value>>& props,
  const storage::GraphDB* db) const {
  double selectivity = 1.0;
  for (const auto [key, _] : props) {
    std::optional<size_t> cur_distinct_cnt = db->property_distinct_count("", key);
    double cur_sel =
      (cur_distinct_cnt.has_value() ? static_cast<double>(cur_distinct_cnt.value()) : db->node_count()) / db->node_count();
    selectivity *= cur_sel;
  }
  return selectivity;
}

double DefaultCostModel::EstimateNodeLabelsSelectivity(const std::vector<String>& labels,
  const storage::GraphDB* db) const {
  double selectivity = 1.0;
  for (const auto label : labels) {
    double label_nodes_cnt = static_cast<double>(db->node_count_with_label(label));
    selectivity *= label_nodes_cnt / db->node_count();
  }
  return selectivity;
}

std::pair<double, String> DefaultCostModel::EstimateBiggestLabelSelectivity(const std::vector<String>& labels,
  const storage::GraphDB* db) const {
  if (labels.empty()) {
    return {1.0, ""};
  }
  double best_selectivity = 1.0;
  String best_label = "";
  for (const auto label : labels) {
    double cur_sel = static_cast<double>(db->node_count_with_label(label)) / db->node_count();
    if (cur_sel < best_selectivity) {
      best_selectivity = cur_sel;
      best_label = label;
    }
  }
  return {best_selectivity, best_label};
}

double DefaultCostModel::EstimateEdgeTypeSelectivity(const std::optional<std::string>& label,
  const storage::GraphDB* db) const {
  if (!label.has_value()) {
    return 1.0;
  }
  return db->edge_count_with_label(label.value() / db->node_count());
}
} // namespace graph::planner
