#include "graph.hpp"

namespace graph::planner {
  LogicalOpUnaryChild::LogicalOpUnaryChild(LogicalOpPtr child) :
    child(std::move(child)) {}

  LogicalOpBinaryChild::LogicalOpBinaryChild(LogicalOpPtr left, LogicalOpPtr right) :
    left(std::move(left)),
    right(std::move(right)) {}

  LogicalScan::LogicalScan(std::vector<String> labels, String dst_alias,
                           std::vector<std::pair<String, Value> > property_filters) :
    AliasedLogicalOp(std::move(dst_alias)),
    labels(std::move(labels)),
    property_filters(std::move(property_filters)) {}

  LogicalScan::LogicalScan(std::vector<String> labels, String dst_alias) :
    AliasedLogicalOp(std::move(dst_alias)),
    labels(std::move(labels)) {}

  LogicalExpand::LogicalExpand(
    LogicalOpPtr child, String src_alias, String edge_alias,
    String dst_alias, ast::EdgeDirection direction
  ) :
    AliasedLogicalOp(std::move(dst_alias)),
    child(std::move(child)),
    src_alias(std::move(src_alias)),
    edge_alias(std::move(edge_alias)),
    direction(direction) {}

  LogicalExpand::LogicalExpand(LogicalOpPtr child, String src_alias, String edge_alias, String dst_alias,
                  String edge_type, std::vector<String> dst_vertex_labels,
                  ast::EdgeDirection direction):
    AliasedLogicalOp(std::move(dst_alias)),
    child(std::move(child)),
    src_alias(std::move(src_alias)),
    edge_alias(std::move(edge_alias)),
    edge_type(std::move(edge_type)),
    dst_vertex_labels(std::move(dst_vertex_labels)),
    direction(direction) {}

  LogicalFilter::LogicalFilter(LogicalOpPtr child, std::unique_ptr<ast::Expr> predicate) :
    LogicalOpUnaryChild(std::move(child)),
    predicate(std::move(predicate)) {}

  LogicalProject::LogicalProject(LogicalOpPtr child, std::vector<ast::ReturnItem> items) :
    LogicalOpUnaryChild(std::move(child)),
    items(std::move(items)) {}

  LogicalLimit::LogicalLimit(LogicalOpPtr child, size_t limit_size) :
    LogicalOpUnaryChild(std::move(child)),
    limit_size(limit_size) {}

  LogicalSort::LogicalSort(LogicalOpPtr child, std::vector<ast::OrderItem> keys) :
    LogicalOpUnaryChild(std::move(child)),
    keys(std::move(keys)) {}

  LogicalJoin::LogicalJoin(LogicalOpPtr left, LogicalOpPtr right) :
    LogicalOpBinaryChild(std::move(left), std::move(right)), predicate{nullptr} {}

  LogicalJoin::LogicalJoin(
    LogicalOpPtr left, LogicalOpPtr right,
    std::unique_ptr<ast::Expr> predicate) :
    LogicalOpBinaryChild(std::move(left), std::move(right)),
    predicate(std::move(predicate)) {}

  LogicalSet::LogicalSet(LogicalOpPtr child, String alias, String key, Value value) :
    LogicalOpUnaryChild(std::move(child)),
    assignment{std::move(alias), std::move(key), std::move(value)} {}


  LogicalDelete::LogicalDelete(LogicalOpPtr child, std::vector<String> target) :
    LogicalOpUnaryChild(std::move(child)),
    target(std::move(target)) {}

  CreateNodeSpec::CreateNodeSpec(const ast::NodePattern &pattern) :
    labels(pattern.labels) {
    PlannerUtils::transferProperties(properties, pattern.properties);
  }

  CreateNodeSpec::CreateNodeSpec(ast::NodePattern &&pattern) :
    labels(std::move(pattern.labels)) {
    PlannerUtils::transferProperties(properties, std::move(pattern.properties));
  }

  CreateEdgeSpec::CreateEdgeSpec(const ast::CreateEdgePattern &pattern) :
    src_alias(pattern.left_node.alias),
    dst_alias(pattern.right_node.alias),
    edge_type(pattern.label),
    direction(pattern.direction) {
    PlannerUtils::transferProperties(properties, pattern.properties);
  }

  CreateEdgeSpec::CreateEdgeSpec(ast::CreateEdgePattern &&pattern) :
    src_alias(std::move(pattern.left_node.alias)),
    dst_alias(std::move(pattern.right_node.alias)),
    edge_type(std::move(pattern.label)),
    direction(pattern.direction) {
    PlannerUtils::transferProperties(properties, std::move(pattern.properties));
  }

  LogicalCreate::LogicalCreate(LogicalOpPtr child, const std::vector<ast::CreateItem> &other_items) :
    LogicalOpUnaryChild(std::move(child)) {
    items.reserve(other_items.size());
    for (const auto &item: other_items) {
      if (item.index() == 0) { ;
        items.emplace_back(std::move(CreateNodeSpec(std::get<0>(item))));
      } else if (item.index() == 1) {
        items.emplace_back(std::move(CreateEdgeSpec(std::get<1>(item))));
      }
    }
  }

  void ApplyLogicalMatchImpl(LogicalPlan &plan, std::unique_ptr<ast::MatchClause> ast_match) {
    if (!ast_match) {
      return;
    }
    using ast::Pattern;
    using ast::PatternElement;
    using ast::NodePattern;
    using ast::MatchEdgePattern;
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
        if (idx + 1 >= ast_match->patterns.size() || pattern_vec.elements[idx + 1].element.index() != 0) {
          throw std::runtime_error("Invalid syntax in match");
        }
        ++idx;
        const auto& node_pattern = std::get<NodePattern>(pattern_vec.elements[idx].element);
        cur_root = std::move(std::make_unique<LogicalExpand>(
          std::move(cur_root),
          cur_root->dst_alias,
          edge_pattern.alias,
          node_pattern.alias,
          edge_pattern.label,
          node_pattern.labels,
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

  LogicalPlan Planner::build_logical_plan(ast::QueryAST ast) const {
    using ast::QueryAST;
    LogicalPlan plan;

    ApplyLogicalMatchImpl(plan, std::move(ast.match));
    ApplyLogicalWhereImpl(plan, std::move(ast.where));
    ApplyLogicalProjectImpl(plan, std::move(ast.return_clause));
    ApplyLogicalSortImpl(plan, std::move(ast.order));
    ApplyLogicalLimitImpl(plan, std::move(ast.limit));
    ApplyLogicalSetImpl(plan, std::move(ast.set_clause));
    ApplyLogicalDeleteImpl(plan, std::move(ast.delete_clause));
    ApplyLogicalCreateImpl(plan, std::move(ast.create));

    return plan;
  }
}

graph::String graph::PlannerUtils::EdgeStrByDirection(ast::EdgeDirection dir) {
  if (dir == ast::EdgeDirection::Right) {
    return ">";
  }
  if (dir == ast::EdgeDirection::Left) {
    return "<";
  }
  return "-";
}

graph::String graph::PlannerUtils::toString(const graph::Value &val) {
  const auto visitor = overloads {
    [](Int cur) -> String { return std::to_string(cur); },
    [](Double cur) -> String { return std::to_string(cur); },
    [](Bool cur) -> String { return (cur ? "true" : "false"); },
    [](String cur) -> String { return std::move(cur); }
  };
  return std::visit(visitor, val);
}

bool graph::PlannerUtils::ValueToBool(graph::Value val) {
  const auto visitor = overloads {
    [](Int cur) -> bool { return cur; },
    [](Double cur) -> bool { return cur != 0.0; },
    [](Bool cur) -> bool { return cur; },
    [](const String& cur) -> bool { return (!cur.empty()); }
  };
  return std::visit(visitor, val);
}

graph::String graph::PlannerUtils::ConcatProperties(const std::vector<std::pair<String, Value>> &v) {
  String ans;
  for (const auto& cur : v) {
    if (!ans.empty()) {
      ans += ", ";
    }
    ans += cur.first + ":" + toString(cur.second);
  }
  return ans;
}

graph::String graph::PlannerUtils::ConcatStrVector(const std::vector<String> &v) {
  String ans;
  for (const auto& cur : v) {
    ans += cur;
  }
  return ans;
}