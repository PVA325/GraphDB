#include "graph.hpp"

namespace graph::planner {
  String LogicalOpUnaryChild::SubtreeDebugString() const {
    return child->SubtreeDebugString() + "\n" + DebugString() + "\n";
  }

  LogicalOpUnaryChild::LogicalOpUnaryChild(LogicalOpPtr child) :
    child(std::move(child)) {}

  LogicalOpBinaryChild::LogicalOpBinaryChild(LogicalOpPtr left, LogicalOpPtr right) :
    left(std::move(left)),
    right(std::move(right)) {}

  String LogicalOpBinaryChild::SubtreeDebugString() const {
    return "1) BINARY OP:\n " + left->DebugString() + "\n2)\n" + right->DebugString() + "\n";
  }

  LogicalScan::LogicalScan(std::vector<String> labels, String dst_alias,
                           std::vector<std::pair<String, Value> > property_filters) :
    AliasedLogicalOp(std::move(dst_alias)),
    labels(std::move(labels)),
    property_filters(std::move(property_filters)) {}

  LogicalScan::LogicalScan(std::vector<String> labels, String dst_alias) :
    AliasedLogicalOp(std::move(dst_alias)),
    labels(std::move(labels)) {}

  String LogicalScan::DebugString() const {
    String ans = "LogicalScan(" + dst_alias + ":";
    if (!labels.empty()) {
      ans += PlannerUtils::ConcatStrVector(labels);
    }
    if (!property_filters.empty()) {
      ans += "{ " + PlannerUtils::ConcatProperties(property_filters) + "}";
    }
    return ans;
  }

  String LogicalScan::SubtreeDebugString() const {
    return DebugString();
  }


  LogicalExpand::LogicalExpand(
    LogicalOpPtr child, String src_alias, String edge_alias,
    String dst_alias, frontend::EdgeDirection direction
  ) :
    AliasedLogicalOp(std::move(dst_alias)),
    src_alias(std::move(src_alias)),
    edge_alias(std::move(edge_alias)),
    child(std::move(child)),
    direction(direction) {}

  LogicalExpand::LogicalExpand(
    LogicalOpPtr child, String src_alias, String edge_alias,
    String dst_alias, std::vector<String> edge_labels,
    std::vector<String> dst_vertex_labels, frontend::EdgeDirection direction
  ) :
    AliasedLogicalOp(std::move(dst_alias)),
    src_alias(std::move(src_alias)),
    edge_alias(std::move(edge_alias)),
    edge_labels(std::move(edge_labels)),
    dst_vertex_labels(std::move(dst_vertex_labels)),
    direction(direction),
    child(std::move(child)) {}

  String LogicalExpand::DebugString() const {
    String ans = "Expand(" + src_alias + PlannerUtils::EdgeStrByDirection(direction) + dst_alias + ")";
    return ans;
  }

  String LogicalExpand::SubtreeDebugString() const {
    return child->SubtreeDebugString() + "\n" + DebugString();
  }


  LogicalFilter::LogicalFilter(LogicalOpPtr child, std::unique_ptr<frontend::Expr> predicate) :
    LogicalOpUnaryChild(std::move(child)),
    predicate(std::move(predicate)) {}

  String LogicalFilter::DebugString() const {
    String ans = "LogicalFilter(" + predicate->DebugString() + ")";
    return ans;
  }

  LogicalProject::LogicalProject(LogicalOpPtr child, std::vector<frontend::ReturnItem> items) :
    LogicalOpUnaryChild(std::move(child)),
    items(std::move(items)) {}

  String LogicalProject::DebugString() const {
    String ans = "Project(";
    const size_t initial_size = ans.size();
    for (const auto &cur: items) {
      if (ans.size() > initial_size) {
        ans += ", ";
      }
      ans += cur.DebugString();
    }
    ans += ")";
    return ans;
  }

  LogicalLimit::LogicalLimit(LogicalOpPtr child, size_t limit_size) :
    LogicalOpUnaryChild(std::move(child)),
    limit_size(limit_size) {}

  String LogicalLimit::DebugString() const {
    return "Limit(" + std::to_string(limit_size) + ")";
  }

  LogicalSort::LogicalSort(LogicalOpPtr child, std::vector<frontend::OrderItem> keys) :
    LogicalOpUnaryChild(std::move(child)),
    keys(std::move(keys)) {}

  String LogicalSort::DebugString() const {
    String ans = "Sort(";
    for (const auto &[f, s]: keys) {
      ans += f.DebugString() + (s == frontend::OrderDirection::Asc ? "ASC" : "DESC") + "\n";
    }
    ans += ")";
    return ans;
  }

  LogicalJoin::LogicalJoin(LogicalOpPtr left, LogicalOpPtr right) :
    LogicalOpBinaryChild(std::move(left), std::move(right)) {}

  LogicalJoin::LogicalJoin(
    LogicalOpPtr left, LogicalOpPtr right,
    std::unique_ptr<frontend::Expr> predicate) :
    LogicalOpBinaryChild(std::move(left), std::move(right)),
    predicate(std::move(predicate)) {}

  String LogicalJoin::DebugString() const {
    String ans;
    ans += "Join(" + left->DebugString() + ", " + right->DebugString();
    if (predicate.has_value()) {
      ans += "on predicate " + predicate.value()->DebugString() + ")";
    } else {
      ans += ")";
    }
    return ans;
  }

  LogicalSet::LogicalSet(LogicalOpPtr child, String alias, String key, Value value) :
    LogicalOpUnaryChild(std::move(child)),
    assignment{std::move(alias), std::move(key), std::move(value)} {}

  String LogicalSet::DebugString() const {
    String ans = "LogicalSet(" + assignment.alias + ", key=" +
                 assignment.key + ", value=" + PlannerUtils::toString(assignment.value);
    return ans;
  }

  LogicalDelete::LogicalDelete(LogicalOpPtr child, std::vector<String> target) :
    LogicalOpUnaryChild(std::move(child)),
    target(std::move(target)) {}

  String LogicalDelete::DebugString() const {
    std::string ans = "LogicalDelete(" + PlannerUtils::ConcatStrVector(target) + ")";
    return ans;
  }

  CreateNodeSpec::CreateNodeSpec(const ast::NodePattern &pattern) :
    labels(pattern.labels) {
    PlannerUtils::transferProperties(properties, pattern.properties);
  }

  CreateNodeSpec::CreateNodeSpec(ast::NodePattern &&pattern) :
    labels(std::move(pattern.labels)) {
    properties.reserve(pattern.properties.size());
    PlannerUtils::transferProperties(properties, std::move(pattern.properties));
  }

  String CreateNodeSpec::DebugString() const {
    String ans = "NodeCreate(";
    ans += PlannerUtils::ConcatStrVector(labels);
    if (!properties.empty()) {
      ans += ", " + PlannerUtils::ConcatProperties(properties);
    }
    ans += ")";
    return ans;
  }


  CreateEdgeSpec::CreateEdgeSpec(const ast::CreateEdgePattern &pattern) :
    src_alias(pattern.left_node.alias),
    dst_alias(pattern.right_node.alias),
    labels(pattern.labels),
    direction(pattern.direction) {
    PlannerUtils::transferProperties(properties, pattern.properties);
  }

  CreateEdgeSpec::CreateEdgeSpec(ast::CreateEdgePattern &&pattern) :
    src_alias(std::move(pattern.left_node.alias)),
    dst_alias(std::move(pattern.right_node.alias)),
    labels(std::move(pattern.labels)),
    direction(pattern.direction) {
    PlannerUtils::transferProperties(properties, std::move(pattern.properties));
  }

  String CreateEdgeSpec::DebugString() const {
    String ans = "EdgeCreate(";
    ans += src_alias + PlannerUtils::EdgeStrByDirection(direction) + dst_alias + ",";
    ans += PlannerUtils::ConcatStrVector(labels);
    if (!properties.empty()) {
      ans += ", " + PlannerUtils::ConcatProperties(properties);
    }

    ans += ")";
    return ans;
  }

  LogicalCreate::LogicalCreate(LogicalOpPtr child, const std::vector<ast::CreateItem> &other_items) :
    LogicalOpUnaryChild(std::move(child)) {
    items.reserve(other_items.size());
    for (const auto &item: other_items) {
      if (item.index() == 0) { ;
        items.emplace_back(CreateNodeSpec(std::get<0>(item)));
      }
    }
  }

  String LogicalCreate::DebugString() const {
    String ans = "LogicalCreate(";
    for (const auto &pattern: items) {
      if (pattern.index() == 0) {
        ans += std::get<0>(pattern).DebugString();
      } else {
        ans += std::get<0>(pattern).DebugString();
      }
    }
    ans += ")";
    return ans;
  }

  String LogicalPlan::DebugString() const {
    String ans = (root ? root->SubtreeDebugString() : "No Plan yet:(");
    return ans;
  }

  String LogicalPlan::SubtreeDebugString() const {
    return DebugString();
  }

  void ApplyLogicalMatchImpl(LogicalPlan &plan, const frontend::QueryAST &ast) {
    if (!ast.match) {
      return;
    }
    using frontend::Pattern;
    using frontend::PatternElement;
    using frontend::NodePattern;
    using frontend::MatchEdgePattern;
    for (const Pattern &pattern_vec: ast.match->patterns) {
      std::unique_ptr<AliasedLogicalOp> cur_root = nullptr;
      for (size_t idx = 0; idx < pattern_vec.elements.size(); ++idx) {
        const PatternElement &pattern = pattern_vec.elements[idx];
        if (pattern.element.index() == 0) {
          if (cur_root) {
            throw std::runtime_error("Error in Apply match, invalid syntax");
          }
          const NodePattern &node_pattern = std::get<NodePattern>(pattern.element);
          std::unique_ptr<LogicalScan> node_scan = std::make_unique<LogicalScan>(
            node_pattern.labels,
            node_pattern.alias
          );
          cur_root = std::move(node_scan);
          continue;
        } else {
          const auto &edge_pattern = std::get<MatchEdgePattern>(pattern.element);
          if (idx + 1 >= ast.match->patterns.size() || pattern_vec.elements[idx + 1].element.index() != 0) {
            throw std::runtime_error("Invalid syntax in match");
          }
          ++idx;
          const NodePattern &node_pattern = std::get<NodePattern>(pattern_vec.elements[idx].element);
          cur_root = std::move(std::make_unique<LogicalExpand>(
            std::move(cur_root),
            cur_root->dst_alias,
            edge_pattern.alias,
            node_pattern.alias,
            edge_pattern.labels,
            node_pattern.labels,
            edge_pattern.direction
          ));
        }
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

  void ApplyLogicalWhereImpl(LogicalPlan &plan, const frontend::QueryAST &ast) {
    if (!ast.where) {
      return;
    }
    plan.root = std::make_unique<LogicalFilter>(
      std::move(plan.root),
      std::move(ast.where->expression) /// grab from artem, mb make expression shared_ptr?
    );
  }

  void ApplyLogicalProjectImpl(LogicalPlan &plan, const frontend::QueryAST &ast) {
    if (!ast.return_clause) {
      return;
    }
    plan.root = std::make_unique<LogicalProject>(
      std::move(plan.root),
      ast.return_clause->items
    );
  }

  void ApplyLogicalSortImpl(LogicalPlan &plan, const frontend::QueryAST &ast) {
    if (!ast.order) {
      return;
    }
    plan.root = std::make_unique<LogicalSort>(
      std::move(plan.root),
      ast.order->items
    );
  }

  void ApplyLogicalLimitImpl(LogicalPlan &plan, const frontend::QueryAST &ast) {
    if (!ast.limit) {
      return;
    }
    plan.root = std::make_unique<LogicalLimit>(
      std::move(plan.root),
      ast.limit->limit
    );
  }

  void ApplyLogicalSetImpl(LogicalPlan &plan, const frontend::QueryAST &ast) {
    using frontend::SetClause;
    if (!ast.set_clause) {
      return;
    }

    plan.root = std::make_unique<LogicalSet>(
      std::move(plan.root),
      ast.set_clause->target.alias,
      ast.set_clause->target.property,
      ast.set_clause->value.value
    );
  }

  void ApplyLogicalDeleteImpl(LogicalPlan &plan, const frontend::QueryAST &ast) {
    using frontend::DeleteClause;
    if (!ast.delete_clause) {
      return;
    }
    plan.root = std::make_unique<LogicalDelete>(
      std::move(plan.root),
      ast.delete_clause->aliases
    );
  }

  void ApplyLogicalCreateImpl(LogicalPlan &plan, const frontend::QueryAST &ast) {
    if (!ast.create) {
      return;
    }
    const auto &create_item = ast.create->created_items;
    plan.root = std::make_unique<LogicalCreate>(
      std::move(plan.root),
      ast.create->created_items
    );
  }

  planner::LogicalPlan Planner::build_logical_plan(const frontend::QueryAST &ast) const {
    using QueryAST = frontend::QueryAST;
    LogicalPlan plan;

    ApplyLogicalMatchImpl(plan, ast);
    ApplyLogicalWhereImpl(plan, ast);
    ApplyLogicalProjectImpl(plan, ast);
    ApplyLogicalSortImpl(plan, ast);
    ApplyLogicalLimitImpl(plan, ast);
    ApplyLogicalSetImpl(plan, ast);
    ApplyLogicalDeleteImpl(plan, ast);
    ApplyLogicalCreateImpl(plan, ast);

    return plan;
  }
}

graph::String graph::PlannerUtils::EdgeStrByDirection(frontend::EdgeDirection dir) {
  if (dir == frontend::EdgeDirection::Right) {
    return ">";
  } else if (dir == frontend::EdgeDirection::Left) {
    return "<";
  } else {
    return "-";
  }
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
    [](Double cur) -> bool { return cur; },
    [](Bool cur) -> bool { return cur; },
    [](String cur) -> bool { return (!cur.empty()); }
  };
  return std::visit(visitor, val);
}

graph::String graph::PlannerUtils::ConcatProperties(const std::vector<std::pair<String, Value>> &v) {
  String ans = "";
  for (const auto& cur : v) {
    if (!ans.empty()) {
      ans += ", ";
    }
    ans += cur.first + ":" + toString(cur.second);
  }
  return ans;
}

graph::String graph::PlannerUtils::ConcatStrVector(const std::vector<String> &v) {
  String ans = "";
  for (const auto& cur : v) {
    ans += cur;
  }
  return ans;
}