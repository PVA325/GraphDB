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

    plan.root = std::make_unique<LogicalSet>(
      std::move(plan.root),
      std::move(ast_set->target.alias),
      std::move(ast_set->target.property),
      std::move(ast_set->value.value)
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

    ApplyLogicalMatchImpl(plan, std::move(ast_plan_.match));
    ApplyLogicalWhereImpl(plan, std::move(ast_plan_.where));
    ApplyLogicalProjectImpl(plan, std::move(ast_plan_.return_clause));
    ApplyLogicalSortImpl(plan, std::move(ast_plan_.order));
    ApplyLogicalLimitImpl(plan, std::move(ast_plan_.limit));
    ApplyLogicalSetImpl(plan, std::move(ast_plan_.set_clause));
    ApplyLogicalDeleteImpl(plan, std::move(ast_plan_.delete_clause));
    ApplyLogicalCreateImpl(plan, std::move(ast_plan_.create));

    logical_plan_ = std::move(plan);
  }

  void Planner::build_physical_plan() {
    physical_plan_ = exec::PhysicalPlan(std::move(logical_plan_.BuildPhysical(ctx_)));
  }
}