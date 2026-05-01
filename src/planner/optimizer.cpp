#include "planner/query_planner.hpp"

namespace graph::optimizer {
double CostEstimate::kCostEstimateInf{1e15};

struct FilterPushdown {
  ast::Expr* push_left_expr{nullptr};
  ast::Expr* push_right_expr{nullptr};
  ast::Expr* cross_expr{nullptr};


  static FilterPushdown Merge(FilterPushdown& left, FilterPushdown& right) {
    return FilterPushdown{
      MergeExpr(left.push_left_expr, right.push_left_expr),
      MergeExpr(left.push_right_expr, right.push_right_expr),
      MergeExpr(left.cross_expr, right.cross_expr)
    };
  }

private:
  static ast::Expr* MergeExpr(ast::Expr* left, ast::Expr* right) {
    if (!left) {
      return right;
    }
    if (!right) {
      return left;
    }

    /// And ???
    return new ast::LogicalExpr(left, ast::LogicalOp::And, right);
  }
};

FilterPushdown ParseFilterExpr(const ast::Expr* expr,
                               const std::vector<String>& left_aliases,
                               const std::vector<String>& right_aliases) {
  static auto aliases_crosses = [](const std::vector<String>& l, const std::vector<String>& r) -> bool {
    return std::any_of(l.begin(), l.end(), [&r](const String& s) {
      return std::find(r.begin(), r.end(), s) != r.end();
    });
  };
  auto expr_labels = expr->CollectAliases();
  if (expr_labels.empty()) {
    return {};
  }

  if (!aliases_crosses(right_aliases, expr_labels)) {
    return {expr->copy(), nullptr, nullptr};
  }
  if (!aliases_crosses(left_aliases, expr_labels)) {
    return {nullptr, expr->copy(), nullptr};
  }
#ifdef DEBUG
  assert(expr->Type() == ast::ExprType::Logical || expr->Type() == ast::ExprType::Comparison);
#endif

  ast::Expr* left_expr = (expr->Type() == ast::ExprType::Logical
                            ? dynamic_cast<const ast::LogicalExpr*>(expr)->left_expr.get()
                            : dynamic_cast<const ast::ComparisonExpr*>(expr)->left_expr.get()
  );
  ast::Expr* right_expr = (expr->Type() == ast::ExprType::Logical
                             ? dynamic_cast<const ast::LogicalExpr*>(expr)->right_expr.get()
                             : dynamic_cast<const ast::ComparisonExpr*>(expr)->right_expr.get()
  );

  FilterPushdown left_pushdown = ParseFilterExpr(left_expr, left_aliases, right_aliases);
  FilterPushdown right_pushdown = ParseFilterExpr(right_expr, right_aliases, right_aliases);

  return FilterPushdown::Merge(left_pushdown, right_pushdown);
}

void PushFilter(logical::LogicalOp* op) {
  if (op->Type() != logical::LogicalOpType::Filter) {
    return;
  }
  auto* filter_op = dynamic_cast<logical::LogicalFilter*>(op);
  ast::Expr* expr = (filter_op->predicate ? filter_op->predicate.get() : nullptr);
  if (!expr) {
    return;
  }
  if (!filter_op->child) {
    throw std::runtime_error("Error, optimize_logical_plan_impl: filter has no child");
  }


  auto child_op = filter_op->child.get();
#ifdef DEBUG
  assert(child_op->Type() == logical::LogicalOpType::Scan || child_op->Type() == logical::LogicalOpType::Join);
#endif
  if (child_op->Type() == logical::LogicalOpType::Scan) {
    return;
  }
  auto child_join = dynamic_cast<logical::LogicalJoin*>(child_op);

  auto left_alias_plan = child_join->left->GetSubtreeAliases();
  auto right_alias_plan = child_join->right->GetSubtreeAliases();

  FilterPushdown new_expressions = ParseFilterExpr(expr, left_alias_plan, right_alias_plan);
  filter_op->predicate.reset(new_expressions.cross_expr);
  if (new_expressions.push_left_expr) {
    child_join->left = std::make_unique<logical::LogicalFilter>(
      std::move(child_join->left),
      new_expressions.push_left_expr
    );
  }
  if (new_expressions.push_right_expr) {
    child_join->right = std::make_unique<logical::LogicalFilter>(
      std::move(child_join->right),
      new_expressions.push_right_expr
    );
  }
}

void OptimizePushFilter(logical::LogicalOp* op) {
  if (op == nullptr) {
    return;
  }

  PushFilter(op);

  for (auto cur : op->GetChildren()) {
    optimize_logical_plan_impl(cur);
  }
}

void optimize_logical_plan_impl(logical::LogicalOp* op) {
  OptimizePushFilter(op);
}

std::tuple<bool, ExprPtrVec, ExprPtrVec> CutExpressionForHashJoin(
  const ast::Expr* expr,
  const std::vector<String>& left_join_aliases, const std::vector<String>& right_join_aliases) {
  if (expr == nullptr || expr->Type() == ast::ExprType::Literal || expr->Type() == ast::ExprType::Comparison) {
    /// if expression is literal then if the other side depends on only one Join child then we pushed it alread ->
    /// other side(of parent expression) depends from two sides of join -> we cant use HashJoin YET(can be fix if we transfer all right-depend features from the right, do it later)
    return {false, ExprPtrVec{}, ExprPtrVec{}};
  }

  if (expr->Type() == ast::ExprType::Logical) {
    auto logical_expr = dynamic_cast<const ast::LogicalExpr*>(expr);
    if (logical_expr->op != ast::LogicalOp::And) { /// need only and for join
      return {false, ExprPtrVec{}, ExprPtrVec{}};
    }
#ifdef DEBUG
    assert(logical_expr->left_expr && logical_expr->right_expr);
#endif
    auto left_cut = CutExpressionForHashJoin(logical_expr->left_expr.get(), left_join_aliases, right_join_aliases);
    auto right_cut = CutExpressionForHashJoin(logical_expr->right_expr.get(), left_join_aliases, right_join_aliases);

    if (std::get<0>(left_cut) == false || std::get<0>(right_cut) == false) {
      return {false, ExprPtrVec{}, ExprPtrVec{}};
    }

    return {true,
      std::move(PlannerUtils::MergeVectors(std::move(std::get<1>(left_cut)), std::move(std::get<1>(right_cut)))),
      std::move(PlannerUtils::MergeVectors(std::move(std::get<2>(left_cut)), std::move(std::get<2>(right_cut)))),
    };
  }

  if (expr->Type() == ast::ExprType::Property) {
    auto property_expr = dynamic_cast<const ast::PropertyExpr*>(expr);
    if (std::find(left_join_aliases.begin(), left_join_aliases.end(), property_expr->alias) != left_join_aliases.end()) {
      ExprPtrVec left_expr;
      left_expr.emplace_back(property_expr->copy());
      return {true, std::move(left_expr), ExprPtrVec{}};
    }
#ifdef DEBUG
    assert(std::find(right_join_aliases.begin(), right_join_aliases.end(), property_expr->alias) != left_join_aliases.end()))
#endif

    ExprPtrVec right_expr;
    right_expr.emplace_back(property_expr->copy());
    return {true, ExprPtrVec{}, std::move(right_expr)};
  }
  throw std::runtime_error("CutExpressionForHashJoin: Error, Invalid Expression type");
}

std::tuple<CostEstimate, ExprPtrVec, ExprPtrVec> EstimateHashJoin(
  const logical::LogicalJoin* join, exec::ExecContext& ctx,
  CostModel* cost_model, storage::GraphDB* db,
  const CostEstimate& left_cost, const CostEstimate& right_cost
) {
  if (!join->predicate) {
    return {CostEstimate::GetMaxCostEstimate(), ExprPtrVec{}, ExprPtrVec{}};
  }
  auto [is_hashjoin_possible, left_keys, right_keys] =
    CutExpressionForHashJoin(join->predicate.get(), join->left->GetSubtreeAliases(), join->right->GetSubtreeAliases());

  if (!is_hashjoin_possible) {
    return {CostEstimate::GetMaxCostEstimate(), ExprPtrVec{}, ExprPtrVec{}};
  }

  // throw std::runtime_error("Not implemented");
  CostEstimate cost = cost_model->EstimateHashJoin(db, left_cost, right_cost,
    exec::HashJoinOp::ExprPtrVecToBasePtrVec(left_keys),
    exec::HashJoinOp::ExprPtrVecToBasePtrVec(right_keys)
  );
  return {cost, std::move(left_keys), std::move(right_keys)};
}
} // namespace graph::optimizer
