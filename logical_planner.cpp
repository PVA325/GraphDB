#include "graph.hpp"

namespace graph::planner {
  String LogicalScan::DebugString() const {
    String ans = "LogicalScan(" + dst_alias + ":";
    if (!labels.empty()) {
      ans += PlannerUtils::ConcatStrVector(labels);
    }
//    if (!property_filters.empty()) {
//      ans += "{ " + PlannerUtils::ConcatProperties(property_filters) + "}";
//    }
    return ans;
  }

  LogicalScan::LogicalScan(std::vector<String> labels, String dst_alias, std::vector<std::pair<const String, Value> > property_filters):
    AliasedLogicalOp(std::move(dst_alias)),
    labels(std::move(labels)),
    property_filters(std::move(property_filters))
  {}

  LogicalExpand::LogicalExpand(String src_alias, String dst_alias, LogicalOpPtr child, frontend::Direction direction):
    AliasedLogicalOp(std::move(dst_alias)),
    src_alias(std::move(src_alias)),
    direction(direction),
    child(std::move(child))
  {}

  LogicalExpand::LogicalExpand(String src_alias, String dst_alias, LogicalOpPtr child, std::vector<String> edge_labels, frontend::Direction direction):
    AliasedLogicalOp(std::move(dst_alias)),
    src_alias(std::move(src_alias)),
    edge_labels(std::move(edge_labels)),
    direction(direction),
    child(std::move(child))
  {}


  String LogicalExpand::DebugString() const {
    String ans = "Expand(" + src_alias + PlannerUtils::EdgeStrByDirection(direction) + dst_alias + ")";
    return ans;
  }


  String LogicalFilter::DebugString() const {
    String ans = "LogicalFilter(" + predicate->DebugString() + ")";
    return ans;
  }

  String LogicalProject::DebugString() const {
    String ans = "Project(";
    const size_t initial_size = ans.size();
    for (const auto& cur : items) {
      if (ans.size() > initial_size) {
        ans += ", ";
      }
      ans += cur.DebugString();
    }
    ans += ")";
    return ans;
  }

  String LogicalLimit::DebugString() const {
    return "Limit(" + std::to_string(limit_size) + ")";
  }

  String LogicalSort::DebugString() const {
    String ans = "Sort(";
    for (const auto& [f, s] : keys) {
      ans += f.DebugString() + (s == frontend::OrderDirection::ASC ? "ASC" : "DESC") + "\n";
    }
    ans += ")";
    return ans;
  }

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

  String LogicalPlan::DebugString() const {
    String ans = (root ? root->DebugString() : "No Plan yet:(");
    return ans;
  }

  void ApplyLogicalMatchImpl(LogicalPlan& plan, const frontend::QueryAST &ast) {
    if (!ast.match) {
      return;
    }
    using frontend::Pattern;
    using frontend::PatternElement;
    using frontend::NodePattern;
    using frontend::EdgePattern;
    for (const Pattern& pattern_vec : ast.match->pattern) {
      std::unique_ptr<AliasedLogicalOp> cur_root = nullptr;
      for (const PatternElement& pattern : pattern_vec.elements) {
        // now we cant do like many matches in one
        if (pattern.node.has_value()) {
          const NodePattern &node_pattern = pattern.node.value();
          std::unique_ptr<LogicalScan> node_scan = std::make_unique<LogicalScan>(
            node_pattern.labels,
            node_pattern.alias
          );
          if (pattern.edge_to_next.has_value()) {
            throw std::runtime_error("Error in Apply match, invalid syntax(in Pattern it has both Node and Edge");
          }
          if (cur_root) {
            throw std::runtime_error("Error in Apply match, invalid syntax");
          }
          cur_root = std::move(node_scan);
          continue;
        }

        if (!pattern.edge_to_next.has_value()) {
          throw std::runtime_error("Invalid Pattern");
        }
        const EdgePattern &edge_pattern = pattern.edge_to_next.value();
        cur_root = std::move(std::make_unique<LogicalExpand>(
          cur_root->dst_alias,
          edge_pattern.alias, /// in edge alias must me alias of vertexes where to expand: (a)-(:Person)>(b) in edge should be alias b
          std::move(cur_root),
          edge_pattern.labels,
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

  void ApplyLogicalWhereImpl(LogicalPlan& plan, const frontend::QueryAST &ast) {
    if (!ast.where) {
      return;
    }
    plan.root = std::make_unique<LogicalFilter>(
      std::move(ast.where->expression), /// grab from artem, mb make expression shared_ptr?
      std::move(plan.root)
    );
  }

  void ApplyLogicalProjectImpl(LogicalPlan& plan, const frontend::QueryAST &ast) {
    if (!ast.ret) {
      return;
    }
    plan.root = std::make_unique<LogicalProject>(
      ast.ret->items,
      std::move(plan.root)
    );
  }
  void ApplyLogicalSortImpl(LogicalPlan& plan, const frontend::QueryAST &ast) {
    if (!ast.order) {
      return;
    }
    plan.root = std::make_unique<LogicalSort>(
      ast.order->items,
      std::move(plan.root)
    );
  }
  void ApplyLogicalLimitImpl(LogicalPlan& plan, const frontend::QueryAST &ast) {
    if (!ast.limit) {
      return;
    }
    plan.root = std::make_unique<LogicalLimit>(
      ast.limit->limit,
      std::move(plan.root)
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

    return plan;
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

graph::String graph::PlannerUtils::ConcatProperties(const std::vector<std::pair<const String, Value>> &v) {
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


