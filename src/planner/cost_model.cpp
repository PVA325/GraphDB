#include <cmath>
#include <sys/stat.h>

#include "planner/query_planner.hpp"

namespace graph::optimizer {

CostEstimate DefaultCostModel::EstimateAllNodeScan(
  const storage::GraphDB* db,
  const logical::LogicalScan& scan) const {

  if (!db) {
    throw std::runtime_error("DefaultCostModel(estimate_all_node_scan): Error, db is null");
  }

  auto total_nodes = static_cast<double>(db->node_count());
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

  auto total_nodes = static_cast<double>(db->node_count());
  const double labels_sel = EstimateNodeLabelsSelectivity(scan.labels, db);
  const auto [best_label_sel, best_label] = EstimateLowestLabelSelectivity(scan.labels, db);
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
  filter_cost.row_count = input.row_count * EstimateExprSelectivity(pred, db);

  filter_cost.cpu_cost = input.cpu_cost
                         + input.row_count * kFilterCpu * EstimateExprCpuCost(pred, db);

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
  join_cost.row_count = pair_count * EstimateExprSelectivity(pred, db);

  join_cost.cpu_cost = left.cpu_cost
                     + right.cpu_cost
                     + pair_count * (kFilterCpu + kCpuPerRow) * EstimateExprCpuCost(pred, db);

  join_cost.io_cost = left.io_cost + right.io_cost;
  join_cost.startup_cost = left.startup_cost + right.startup_cost + 1.0;

  return join_cost;
}

CostEstimate DefaultCostModel::EstimateHashJoin(
  const storage::GraphDB* db,
  const CostEstimate& left,
  const CostEstimate& right,
  const std::vector<ast::Expr*>& left_keys,
  const std::vector<ast::Expr*>& right_keys) const {

  if (!db) {
    throw std::runtime_error("DefaultCostModel(estimate_hash_join): Error, db is null");
  }

  double build = left.row_count;
  double probe = right.row_count;

  auto predicate = CreateExprByHashJoinKeys(left_keys, right_keys);
  double sel = EstimateExprSelectivity((predicate ? predicate.get() : nullptr), db);
  double predicate_evaluation_cost = EstimateExprCpuCost((predicate ? predicate.get() : nullptr), db);

  CostEstimate join_cost;
  join_cost.row_count = left.row_count * right.row_count * sel;

  join_cost.cpu_cost = left.cpu_cost
                     + right.cpu_cost
                     + (build * kCpuHashBuild + probe * kCpuHashProbe) * predicate_evaluation_cost;

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

  if (input.row_count >= 3) {
    sort_cost.cpu_cost = input.cpu_cost
                       + (input.row_count * std::log2(input.row_count)) * kCpuPerRow;
  } else {
    sort_cost.cpu_cost = input.cpu_cost;
  }

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

CostEstimate DefaultCostModel::EstimateSet(const storage::GraphDB* db, const CostEstimate& input,
  const logical::LogicalSet& set) const {

  if (!db) {
    throw std::runtime_error("DefaultCostModel(EstimateSet): Error, db is null");
  }

  CostEstimate set_cost = input;
  set_cost.cpu_cost += input.row_count * kCpuPerRow * static_cast<double>(set.assignment.size());
  set_cost.io_cost += input.row_count * kIoPerRow * static_cast<double>(set.assignment.size());

  return set_cost;
}

CostEstimate DefaultCostModel::EstimateCreate(const storage::GraphDB* db, const CostEstimate& input,
  const logical::LogicalCreate& create) const {

  if (!db) {
    throw std::runtime_error("DefaultCostModel(EstimateCreate): Error, db is null");
  }

  CostEstimate set_cost = input;
  set_cost.cpu_cost += input.row_count * kCpuPerRow * static_cast<double>(create.items.size());
  set_cost.io_cost += input.row_count * kIoPerRow * static_cast<double>(create.items.size());

  return set_cost;
}

CostEstimate DefaultCostModel::EstimateDelete(const storage::GraphDB* db, const CostEstimate& input,
  const logical::LogicalDelete& del) const {

  if (!db) {
    throw std::runtime_error("DefaultCostModel(EstimateDelete): Error, db is null");
  }

  CostEstimate del_cost = input;
  del_cost.cpu_cost += input.row_count * kCpuPerRow * static_cast<double>(del.target.size());
  del_cost.io_cost += input.row_count * kIoPerRow * static_cast<double>(del.target.size());

  return del_cost;
}

double DefaultCostModel::EstimateNodePropertySelectivity(const std::vector<std::pair<String, Value>>& props,
                                                         const storage::GraphDB* db) {
  double selectivity = 1.0;
  auto node_count = static_cast<double>(db->node_count());
  for (const auto& [key, _] : props) {
    std::optional<size_t> cur_distinct_cnt = db->property_distinct_count("", key);
    double cur_sel =
      (cur_distinct_cnt.has_value() ? static_cast<double>(cur_distinct_cnt.value()) : node_count) / node_count;
    selectivity *= cur_sel;
  }
  return selectivity;
}

double DefaultCostModel::EstimateNodeLabelsSelectivity(const std::vector<String>& labels,
  const storage::GraphDB* db) {
  double selectivity = 1.0;
  auto node_count = static_cast<double>(db->node_count());

  for (const auto& label : labels) {
    auto label_nodes_cnt = static_cast<double>(db->node_count_with_label(label));
    selectivity *= label_nodes_cnt / node_count;
  }
  return selectivity;
}

std::pair<double, String> DefaultCostModel::EstimateLowestLabelSelectivity(const std::vector<String>& labels,
  const storage::GraphDB* db) {
  if (labels.empty()) {
    return {1.0, ""};
  }
  double best_selectivity = 1.0;
  String best_label;
  auto node_count = static_cast<double>(db->node_count());

  for (const auto& label : labels) {
    double cur_sel = static_cast<double>(db->node_count_with_label(label)) / node_count;
    if (cur_sel < best_selectivity) {
      best_selectivity = cur_sel;
      best_label = label;
    }
  }
  return {best_selectivity, best_label};
}

double DefaultCostModel::EstimateEdgeTypeSelectivity(const std::optional<std::string>& label,
  const storage::GraphDB* db) {
  if (!label.has_value()) {
    return 1.0;
  }
  auto node_count = static_cast<double>(db->node_count());

  return static_cast<double>(db->edge_count_with_type(label.value())) / node_count;
  /// TODO: edge_count_with_type has strange realization; discuss
}

double DefaultCostModel::EstimateExprSelectivity(const ast::Expr* expr, const storage::GraphDB* db) {
  if (!expr) {
    return 1.0;
  }
  if (expr->Type() == ast::ExprType::Logical) {
    auto expr_logical = dynamic_cast<const ast::LogicalExpr*>(expr);
    double left_sel = EstimateExprSelectivity(expr_logical->left_expr.get(), db);
    double right_sel = EstimateExprSelectivity(expr_logical->right_expr.get(), db);
    if (expr_logical->op == ast::LogicalOp::And) {
      return left_sel * right_sel;
    }
    return left_sel + right_sel - left_sel * right_sel;
  }

  if (expr->Type() == ast::ExprType::Comparison) {
    auto expr_comparison = dynamic_cast<const ast::ComparisonExpr*>(expr);
    ast::CompareOp op = expr_comparison->op;

    ast::Expr* left = expr_comparison->left_expr.get();
    ast::Expr* right = expr_comparison->right_expr.get();
    if ((left->Type() != ast::ExprType::Literal && left->Type() != ast::ExprType::Property) ||
        (right->Type() != ast::ExprType::Literal && right->Type() != ast::ExprType::Property)) {
      return 1.0;
    }
    if (left->Type() == ast::ExprType::Literal && right->Type() == ast::ExprType::Literal) {
      return PlannerUtils::ValueToBool((*expr)(ast::EvalContext{exec::Row{}}));
    }
    if (left->Type() == ast::ExprType::Property && right->Type() == ast::ExprType::Property) {
      auto left_property_expr = dynamic_cast<const ast::PropertyExpr*>(left);
      auto right_property_expr = dynamic_cast<const ast::PropertyExpr*>(right);
      double left_selectivity = GetSelectivityByNodeCount(
        db->property_distinct_count(left_property_expr->property, "").value(), db
      );
      double right_selectivity = GetSelectivityByNodeCount(
        db->property_distinct_count(right_property_expr->property, "").value(), db
      );

      if (op == ast::CompareOp::Eq) {
        return left_selectivity * right_selectivity;
      }
      if (op == ast::CompareOp::NotEqual) {
        return 1.0 - left_selectivity * right_selectivity;
      }
      if (op == ast::CompareOp::Gt || op == ast::CompareOp::Ge) {
        return 0.5 * left_selectivity;
      }
      if (op == ast::CompareOp::Lt || op == ast::CompareOp::Le) {
        return 0.5 * right_selectivity;
      }
    }
    if (left->Type() == ast::ExprType::Property && right->Type() == ast::ExprType::Literal) {
      std::swap(left, right);
    }
#ifndef NDEBUG
    assert(left->Type() == ast::ExprType::Literal && right->Type() == ast::ExprType::Property);
#endif

    auto right_property_expr = dynamic_cast<const ast::PropertyExpr*>(right);
    double selectivity = GetSelectivityByNodeCount(
      db->property_distinct_count(right_property_expr->property, "").value(), db
    );
    return selectivity;
  }

#ifndef NDEBUG
    assert(expr->Type() == ast::ExprType::Literal || expr->Type() == ast::ExprType::Property);
#endif

  return 1.0;
}

double DefaultCostModel::EstimateExprCpuCost_impl(const ast::Expr* expr, const storage::GraphDB* db) {
  if (!expr) {
    return 0.0;
  }
  if (expr->Type() == ast::ExprType::Logical) {
    auto expr_logical = dynamic_cast<const ast::LogicalExpr*>(expr);
    double left_cost = EstimateExprCpuCost(expr_logical->left_expr.get(), db);
    double right_cost = EstimateExprCpuCost(expr_logical->right_expr.get(), db);
    return left_cost + right_cost + kExprLogical;
  }

  if (expr->Type() == ast::ExprType::Comparison) {
    auto expr_comparison = dynamic_cast<const ast::ComparisonExpr*>(expr);
    double left_cost = EstimateExprCpuCost(expr_comparison->left_expr.get(), db);
    double right_cost = EstimateExprCpuCost(expr_comparison->right_expr.get(), db);
    return left_cost + right_cost + kExprCompare;
  }

  if (expr->Type() == ast::ExprType::Property) {
    return kExprProperty;
  }
#ifndef NDEBUG
  assert(expr->Type() == ast::ExprType::Literal);
#endif

  return kExprLiteral;
}

double DefaultCostModel::EstimateExprCpuCost(const ast::Expr* expr, const storage::GraphDB* db) {
  return 1.0 + EstimateExprCpuCost_impl(expr, db);
}

double DefaultCostModel::GetSelectivityByNodeCount(const size_t& node_count, const storage::GraphDB* db) {
  return static_cast<double>(node_count) / db->node_count();
}

std::unique_ptr<ast::Expr> DefaultCostModel::CreateExprByHashJoinKeys(const std::vector<ast::Expr*>& left_keys,
  const std::vector<ast::Expr*>& right_keys) {
#ifndef NDEBUG
  assert(left_keys.size() == right_keys.size());
#endif
  if (left_keys.empty()) {
    return nullptr;
  }
  std::unique_ptr<ast::Expr> ans = nullptr;
  for (int i = 0; i < left_keys.size(); ++i) {
    auto l_key = left_keys[i];
    auto r_key = right_keys[i];

    auto cur_compare = std::make_unique<ast::ComparisonExpr>(l_key->copy(), ast::CompareOp::Eq, r_key->copy());
    if (!ans) {
      ans = std::move(cur_compare);
    } else {
      ans = std::make_unique<ast::LogicalExpr>(std::move(ans), ast::LogicalOp::And, std::move(cur_compare));
    }
  }
  return ans;
}

CostEstimate DefaultCostModel::EstimateProject(
  const storage::GraphDB* db, const CostEstimate& child,
  const logical::LogicalProject& proj) const {

  CostEstimate cost = child;
  cost.cpu_cost += child.row_count * kCpuPerRow * static_cast<double>(proj.items.size());

  return cost;
}
} // namespace graph::optimizer
