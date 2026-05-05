#include <netinet/in.h>

#include "planner/query_planner.hpp"

namespace {
  std::unique_ptr<ast::Expr> MergeExprAnd(std::unique_ptr<ast::Expr> left, std::unique_ptr<ast::Expr> right) {
    if (!left) { return std::move(right); }
    if (!right) { return std::move(left); }

    return std::make_unique<ast::LogicalExpr>(
      std::move(left),
      ast::LogicalOp::And,
      std::move(right)
    );
  }
}

namespace graph::optimizer {
double CostEstimate::kCostEstimateInf{1e15};

struct FilterPushdown {
  size_t child_cnt;
  std::vector<std::unique_ptr<ast::Expr>> push_child_expr;
  std::unique_ptr<ast::Expr> cross_expr{nullptr};

  FilterPushdown(size_t n) {
    child_cnt = n;
    push_child_expr.reserve(n);
    for (size_t i = 0; i < n; ++i) { push_child_expr.emplace_back(nullptr); }
  }
  FilterPushdown(size_t n, size_t idx, std::unique_ptr<ast::Expr> expr): FilterPushdown(n) {
    push_child_expr[idx] = std::move(expr);
  }

  FilterPushdown(size_t n, std::unique_ptr<ast::Expr> cross_expr_): FilterPushdown(n) {
    cross_expr = std::move(cross_expr_);
  }
  static FilterPushdown Merge(FilterPushdown& left, FilterPushdown& right) {
    assert(left.child_cnt == right.child_cnt);

    FilterPushdown ans(left.child_cnt);
    ans.child_cnt = left.child_cnt;
    ans.cross_expr = MergeExpr(std::move(left.cross_expr), std::move(right.cross_expr));
    for (size_t i = 0; i < ans.child_cnt; ++i) {
      ans.push_child_expr[i] = MergeExpr(std::move(left.push_child_expr[i]), std::move(right.push_child_expr[i]));
    }
    return ans;
  }

private:
  static std::unique_ptr<ast::Expr> MergeExpr(std::unique_ptr<ast::Expr> left, std::unique_ptr<ast::Expr> right) {
    if (!left) {
      return right;
    }
    if (!right) {
      return left;
    }

    /// And ???
    return std::make_unique<ast::LogicalExpr>(std::move(left), ast::LogicalOp::And, std::move(right));
  }
};

FilterPushdown ParseFilterExpr(const ast::Expr* expr,
                               const std::vector<std::vector<String> >& ljoin_aliases) {
  size_t child_cnt = ljoin_aliases.size();
  auto aliases_intersects = [](const std::vector<String>& l, const std::vector<String>& r) -> bool {
    return std::any_of(l.begin(), l.end(), [&r](const String& s) {
      return std::find(r.begin(), r.end(), s) != r.end();
    });
  };
  auto aliases_intersects_with_one = [&ljoin_aliases, &aliases_intersects](
    const std::vector<String>& aliases) {
    return std::count_if(ljoin_aliases.begin(), ljoin_aliases.end(), [&aliases, &aliases_intersects](const auto& cur_ljoin_a) {
      return aliases_intersects(aliases, cur_ljoin_a);
    }) == 1;
  };
  auto aliases_find_intersection_with_one = [&ljoin_aliases, &aliases_intersects](
    const std::vector<String>& aliases) {
    return std::find_if(ljoin_aliases.begin(), ljoin_aliases.end(), [&aliases, &aliases_intersects](const auto& cur_ljoin_a) {
      return aliases_intersects(aliases, cur_ljoin_a);
    }) - ljoin_aliases.begin();
  };


  // auto expr_aliases = expr->CollectAliases();
  std::vector<String> expr_aliases;
  expr->CollectAliases(expr_aliases);
  if (expr_aliases.empty()) {
    return {child_cnt};
  }
  if (aliases_intersects_with_one(expr_aliases)) {
    size_t join_idx = aliases_find_intersection_with_one(expr_aliases);
    return FilterPushdown(child_cnt, join_idx, expr->copy());
  }

#ifdef DEBUG
  assert(expr->Type() == ast::ExprType::Logical || expr->Type() == ast::ExprType::Comparison);
#endif
  if (expr->Type() == ast::ExprType::Comparison) {
    return {child_cnt, expr->copy()};
  }
  ast::Expr* left_expr = (expr->Type() == ast::ExprType::Logical
                            ? dynamic_cast<const ast::LogicalExpr*>(expr)->left_expr.get()
                            : dynamic_cast<const ast::ComparisonExpr*>(expr)->left_expr.get()
  );
  ast::Expr* right_expr = (expr->Type() == ast::ExprType::Logical
                             ? dynamic_cast<const ast::LogicalExpr*>(expr)->right_expr.get()
                             : dynamic_cast<const ast::ComparisonExpr*>(expr)->right_expr.get()
  );

  FilterPushdown left_pushdown = ParseFilterExpr(left_expr, ljoin_aliases);
  FilterPushdown right_pushdown = ParseFilterExpr(right_expr, ljoin_aliases);

  return FilterPushdown::Merge(left_pushdown, right_pushdown);
}

void PushFilterDown(logical::LogicalOp* op) {
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
#ifndef NDEBUG
  assert(child_op->Type() == logical::LogicalOpType::Scan || child_op->Type() == logical::LogicalOpType::Join);
#endif
  if (child_op->Type() == logical::LogicalOpType::Scan) {
    return;
  }
  auto child_join = dynamic_cast<logical::LogicalJoin*>(child_op);

  std::vector<std::vector<String>> ljoin_child_aliases(child_join->children.size());
  for (size_t i = 0; i < child_join->children.size(); ++i) {
    const auto& curr_join = child_join->children[i];
    ljoin_child_aliases[i] = curr_join->GetSubtreeAliases();
  }

  FilterPushdown new_expressions = ParseFilterExpr(expr, ljoin_child_aliases);
  filter_op->predicate = std::move(new_expressions.cross_expr);

  for (size_t i = 0; i < new_expressions.child_cnt; ++i) {
    auto& cur_push_expr = new_expressions.push_child_expr[i];
    if (cur_push_expr) {
      child_join->children[i] = std::make_unique<logical::LogicalFilter>(
        std::move(child_join->children[i]),
        std::move(new_expressions.push_child_expr[i])
      );
    }
  }

}

void OptimizePushFilter(logical::LogicalOp* op, logical::LogicalOp* par = nullptr) {
  if (op == nullptr) {
    return;
  }

  PushFilterDown(op);

  for (auto cur : op->GetChildren()) {
    OptimizePushFilter(cur);
  }
}

void PushFilterToJoin_impl(logical::LogicalOp* op, logical::LogicalOp* par) {
  if (op->Type() != logical::LogicalOpType::Filter) {
    return;
  }
  auto* filter_op = dynamic_cast<logical::LogicalFilter*>(op);

  auto child_op = filter_op->child.get();
  if (child_op->Type() == logical::LogicalOpType::Scan) {
    return;
  }
  auto join_op = dynamic_cast<logical::LogicalJoin*>(child_op);
  assert(join_op->predicate == nullptr);
  join_op->predicate = std::move(filter_op->predicate);
  dynamic_cast<logical::LogicalOpUnaryChild*>(par)->child = std::move(filter_op->child);

  //delete filter_op?
}

std::unique_ptr<logical::LogicalOp> FilterToJoin(std::unique_ptr<logical::LogicalOp> op) {
  if (!op) {
    return op;
  }

  if (auto* op_unary = dynamic_cast<logical::LogicalOpUnaryChild*>(op.get())) {
    op_unary->child = std::move(FilterToJoin(std::move(op_unary->child)));
  }
  if (auto* op_binary = dynamic_cast<logical::LogicalOpManyChildren*>(op.get())) {
    for (auto& child_op : op_binary->children) {
      child_op = std::move(FilterToJoin(std::move(child_op)));
    }
  }

  if (auto op_filter = dynamic_cast<logical::LogicalFilter*>(op.get())) {
    if (auto child_join = dynamic_cast<logical::LogicalJoin*>(op_filter->child.get())) {
      child_join->predicate = std::move(op_filter->predicate);
      return std::move(op_filter->child);
    }
  }
  return op;
}

void optimize_logical_plan_impl(std::unique_ptr<logical::LogicalOp>& op) {
  OptimizePushFilter(op.get());
  op = std::move(FilterToJoin(std::move(op)));
}

/// returns {is_possible, left_keys, right_keys, parrent_filter_expr}
std::tuple<bool, ExprPtrVec, ExprPtrVec, std::unique_ptr<ast::Expr>> CutExpressionForHashJoin(
  const ast::Expr* expr,
  const std::vector<String>& left_join_aliases, const std::vector<String>& right_join_aliases) {
  if (expr == nullptr || expr->Type() == ast::ExprType::Literal) {
    /// if expression is literal then if the other side depends on only one Join child then we pushed it alread ->
    /// other side(of parent expression) depends from two sides of join -> we cant use HashJoin YET(can be fix if we transfer all right-depend features from the right, do it later)
    return {false, ExprPtrVec{}, ExprPtrVec{}, nullptr};
  }

  if (expr->Type() == ast::ExprType::Logical) {
    auto logical_expr = dynamic_cast<const ast::LogicalExpr*>(expr);
    if (logical_expr->op != ast::LogicalOp::And) { /// need only and for join
      return {false, ExprPtrVec{}, ExprPtrVec{}, nullptr};
    }
#ifndef NDEBUG
    assert(logical_expr->left_expr && logical_expr->right_expr);
#endif
    auto left_cut = CutExpressionForHashJoin(logical_expr->left_expr.get(), left_join_aliases, right_join_aliases);
    auto right_cut = CutExpressionForHashJoin(logical_expr->right_expr.get(), left_join_aliases, right_join_aliases);

    if (std::get<0>(left_cut) == false || std::get<0>(right_cut) == false) {
      return {false, ExprPtrVec{}, ExprPtrVec{}, nullptr};
    }

    ExprPtrVec left_keys = std::move(PlannerUtils::MergeVectors(std::move(std::get<1>(left_cut)), std::move(std::get<1>(right_cut))));
    ExprPtrVec right_keys = std::move(PlannerUtils::MergeVectors(std::move(std::get<2>(left_cut)), std::move(std::get<2>(right_cut))));
    std::unique_ptr<ast::Expr> parent_filter_expr = MergeExprAnd(std::move(std::get<3>(left_cut)), std::move(std::get<3>(right_cut)));

    return {true,
      std::move(left_keys),
      std::move(right_keys),
      std::move(parent_filter_expr)
    };
  }

  if (expr->Type() == ast::ExprType::Property) {
    auto property_expr = dynamic_cast<const ast::PropertyExpr*>(expr);
    if (std::find(left_join_aliases.begin(), left_join_aliases.end(), property_expr->alias) != left_join_aliases.end()) {
      ExprPtrVec left_expr;
      left_expr.emplace_back(property_expr->copy());
      return {true, std::move(left_expr), ExprPtrVec{}, nullptr};
    }
#ifdef DEBUG
    assert(std::find(right_join_aliases.begin(), right_join_aliases.end(), property_expr->alias) != left_join_aliases.end()))
#endif

    ExprPtrVec right_expr;
    right_expr.emplace_back(property_expr->copy());
    return {true, ExprPtrVec{}, std::move(right_expr), nullptr};
  }

  if (expr->Type() == ast::ExprType::Comparison) {
    auto* comp_expr = dynamic_cast<const ast::ComparisonExpr*>(expr);


    auto left_cut = CutExpressionForHashJoin(comp_expr->left_expr.get(), left_join_aliases, right_join_aliases);
    auto right_cut = CutExpressionForHashJoin(comp_expr->right_expr.get(), left_join_aliases, right_join_aliases);
    if ((std::get<1>(left_cut).size() != 0 && std::get<1>(right_cut).size() != 0) ||
        (std::get<2>(left_cut).size() != 0 && std::get<2>(right_cut).size() != 0) ||
        std::get<0>(left_cut) == false || std::get<0>(right_cut) == false) {
      return {false, ExprPtrVec{}, ExprPtrVec{}, nullptr};
    }

    ExprPtrVec left_keys = std::move(!std::get<1>(left_cut).empty() ? std::get<1>(left_cut) : std::get<1>(right_cut));
    ExprPtrVec right_keys = std::move(!std::get<2>(left_cut).empty() ? std::get<2>(left_cut) : std::get<2>(right_cut));
    std::unique_ptr<ast::Expr> parent_filter_expr = MergeExprAnd(std::move(std::get<3>(left_cut)), std::move(std::get<3>(right_cut)));
    if (comp_expr->op != ast::CompareOp::Eq) {
      parent_filter_expr = MergeExprAnd(std::move(parent_filter_expr), comp_expr->copy());
      return {
        true,
        ExprPtrVec{},
        ExprPtrVec{},
        std::move(parent_filter_expr),
      };
    }
    return {
      true,
      std::move(left_keys),
      std::move(right_keys),
      std::move(parent_filter_expr)
    };
  }
  throw std::runtime_error("CutExpressionForHashJoin: Error, Invalid Expression type");
}

std::tuple<CostEstimate, ExprPtrVec, ExprPtrVec, std::unique_ptr<ast::Expr>> EstimateHashJoin(
  const std::vector<String>& left_aliases,
  const std::vector<String>& right_aliases,
  const std::unique_ptr<ast::Expr>& predicate,
  exec::ExecContext& ctx,
  CostModel* cost_model,
  storage::GraphDB* db,
  const CostEstimate& left_cost,
  const CostEstimate& right_cost
) {
  if (!predicate) {
    return {CostEstimate::GetMaxCostEstimate(), ExprPtrVec{}, ExprPtrVec{}, nullptr};
  }
  auto [is_hashjoin_possible, left_keys, right_keys, parent_filter_expr] =
    CutExpressionForHashJoin(predicate.get(), left_aliases, right_aliases);

  if (!is_hashjoin_possible) {
    return {CostEstimate::GetMaxCostEstimate(), ExprPtrVec{}, ExprPtrVec{}, nullptr};
  }

  CostEstimate cost_hashjoin = cost_model->EstimateHashJoin(db, left_cost, right_cost,
    exec::HashJoinOp::ExprPtrVecToBasePtrVec(left_keys),
    exec::HashJoinOp::ExprPtrVecToBasePtrVec(right_keys)
  );
  if (!parent_filter_expr) {
    return {cost_hashjoin, std::move(left_keys), std::move(right_keys), nullptr};
  }

  CostEstimate final_const = cost_model->EstimateFilter(db, cost_hashjoin, parent_filter_expr.get());
  return {final_const, std::move(left_keys), std::move(right_keys), std::move(parent_filter_expr)};
}
} // namespace graph::optimizer
