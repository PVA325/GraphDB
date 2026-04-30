#include "planner.hpp"

namespace {
  using ast::Pattern;
  using ast::PatternElement;
  using ast::NodePattern;
  using ast::MatchEdgePattern;
  using graph::logical::LogicalPlan;
  using graph::logical::AliasedLogicalOp;
  using graph::logical::LogicalExpand;
  using graph::logical::LogicalJoin;
  using graph::logical::LogicalFilter;
  using graph::logical::LogicalProject;
  using graph::logical::LogicalLimit;
  using graph::logical::LogicalSet;
  using graph::logical::LogicalDelete;
  using graph::logical::LogicalCreate;
  using graph::logical::LogicalSort;
  using graph::logical::LogicalScan;
  void ApplyLogicalMatchImpl(LogicalPlan &plan, std::unique_ptr<ast::MatchClause> ast_match) {
    if (!ast_match) {
      return;
    }

    for (const Pattern &pattern_vec: ast_match->patterns) {
      std::unique_ptr<AliasedLogicalOp> cur_root = nullptr;
      for (size_t idx = 0; idx < pattern_vec.elements.size(); ++idx) {
        const PatternElement &pattern = pattern_vec.elements[idx];
        if (pattern.element.index() == 0) {
          if (cur_root) {
            throw std::runtime_error("Error in Apply match, invalid syntax");
          }
          const auto& node_pattern = std::get<NodePattern>(pattern.element);
          std::unique_ptr<LogicalScan> node_scan = std::make_unique<LogicalScan>(
            node_pattern.labels,
            node_pattern.alias
          );
          cur_root = std::move(node_scan);
          continue;
        }
        const auto &edge_pattern = std::get<MatchEdgePattern>(pattern.element);
        if (idx + 1 >= pattern_vec.elements.size() || pattern_vec.elements[idx + 1].element.index() != 0) {
          throw std::runtime_error("Invalid syntax in match");
        }
        ++idx;
        const auto& node_pattern = std::get<NodePattern>(pattern_vec.elements[idx].element);

        std::vector<std::pair<std::string, graph::Value>> prop;
        graph::PlannerUtils::transferProperties(prop, node_pattern.properties);
        cur_root = std::move(std::make_unique<LogicalExpand>(
          std::move(cur_root),
          cur_root->dst_alias,
          edge_pattern.alias,
          node_pattern.alias,
          edge_pattern.label,
          node_pattern.labels,
          prop,
          edge_pattern.direction
        ));
      }
      if (!plan.root) {
        plan.root = std::move(cur_root);
        continue;
      }
      plan.root = std::make_unique<LogicalJoin>(
        std::move(plan.root),
        std::move(cur_root)
      );
    }
  }

  void ApplyLogicalWhereImpl(LogicalPlan &plan, std::unique_ptr<ast::WhereClause> ast_where) {
    if (!ast_where) {
      return;
    }
    plan.root = std::make_unique<LogicalFilter>(
      std::move(plan.root),
      std::move(ast_where->expression)
    );
  }

  void ApplyLogicalProjectImpl(LogicalPlan &plan, std::unique_ptr<ast::ReturnClause> ast_return) {
    if (!ast_return) {
      return;
    }
    plan.root = std::make_unique<LogicalProject>(
      std::move(plan.root),
      std::move(ast_return->items)
    );
  }

  void ApplyLogicalSortImpl(LogicalPlan &plan, std::unique_ptr<ast::OrderClause> ast_order) {
    if (!ast_order) {
      return;
    }
    plan.root = std::make_unique<LogicalSort>(
      std::move(plan.root),
      std::move(ast_order->items)
    );
  }

  void ApplyLogicalLimitImpl(LogicalPlan &plan, std::unique_ptr<ast::LimitClause> ast_limit) {
    if (!ast_limit) {
      return;
    }
    plan.root = std::make_unique<LogicalLimit>(
      std::move(plan.root),
      ast_limit->limit
    );
  }

  void ApplyLogicalSetImpl(LogicalPlan &plan, std::unique_ptr<ast::SetClause> ast_set) {
    if (!ast_set) {
      return;
    }

    std::vector<LogicalSet::Assignment> assignments;
    for (const auto& item : ast_set->items) {
      LogicalSet::Assignment cur;
      cur.alias = item.alias;
      cur.labels = item.labels;
      graph::PlannerUtils::transferProperties(cur.properties, item.properties);
      assignments.push_back(std::move(cur));
    }
    plan.root = std::make_unique<LogicalSet>(
      std::move(plan.root),
      assignments
    );
  }

  void ApplyLogicalDeleteImpl(LogicalPlan &plan, std::unique_ptr<ast::DeleteClause> ast_delete) {
    using ast::DeleteClause;
    if (!ast_delete) {
      return;
    }
    plan.root = std::make_unique<LogicalDelete>(
      std::move(plan.root),
      std::move(ast_delete->aliases)
    );
  }

  void ApplyLogicalCreateImpl(LogicalPlan &plan, std::unique_ptr<ast::CreateClause> ast_create) {
    if (!ast_create) {
      return;
    }
    plan.root = std::make_unique<LogicalCreate>(
      std::move(plan.root),
      std::move(ast_create->created_items)
    );
  }


  using graph::logical::LogicalOpPtr;
  using graph::exec::PhysicalOp;
  using graph::exec::PhysicalOpPtr;
}

namespace graph::planner {
  void Planner::build_logical_plan() {
    logical::LogicalPlan plan;

    ApplyLogicalMatchImpl(plan, std::move(ast_plan_.match_clause));
    ApplyLogicalWhereImpl(plan, std::move(ast_plan_.where_clause));
    ApplyLogicalCreateImpl(plan, std::move(ast_plan_.create_clause));
    ApplyLogicalSetImpl(plan, std::move(ast_plan_.set_clause));
    ApplyLogicalDeleteImpl(plan, std::move(ast_plan_.delete_clause));
    ApplyLogicalProjectImpl(plan, std::move(ast_plan_.return_clause));
    ApplyLogicalSortImpl(plan, std::move(ast_plan_.order_clause));
    ApplyLogicalLimitImpl(plan, std::move(ast_plan_.limit_clause));

    logical_plan_ = std::move(plan);
  }

  CostEstimate Planner::build_physical_plan() {
    auto child_build = logical_plan_.BuildPhysical(ctx_, cost_model_.get(), db_);
    physical_plan_ = exec::PhysicalPlan(std::move(child_build.first));
    return child_build.second;
  }


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

    ast::Expr* left_expr = (expr->Type() == ast::ExprType::Logical ?
      dynamic_cast<const ast::LogicalExpr*>(expr)->left_expr.get() :
      dynamic_cast<const ast::ComparisonExpr*>(expr)->left_expr.get()
    );
    ast::Expr* right_expr = (expr->Type() == ast::ExprType::Logical ?
      dynamic_cast<const ast::LogicalExpr*>(expr)->right_expr.get() :
      dynamic_cast<const ast::ComparisonExpr*>(expr)->right_expr.get()
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
#ifdef DEBUG
    if (!filter_op->child) {
      throw std::runtime_error("Error, optimize_logical_plan_impl: filter has no child");
    }
#endif

    auto child_op = filter_op->child.get();
#ifdef DEBUG
    assert(child_op->Type() == logical::LogicalOpType::Scan || child_op->Type() == logical::LogicalOpType::Join);
#endif
    if (child_op->Type() == logical::LogicalOpType::Scan) {
      return;
    }
    auto child_join = dynamic_cast<LogicalJoin*>(child_op);

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

  void Planner::optimize_logical_plan_impl(logical::LogicalOp* op) {
    if (op == nullptr) {
      return;
    }

    PushFilter(op);

    for (auto cur : op->GetChildren()) {
      optimize_logical_plan_impl(cur);
    }
  }

  void Planner::optimize_logical_plan() {
    optimize_logical_plan_impl(logical_plan_.root.get());
  }
}  // namespace graph::planner
