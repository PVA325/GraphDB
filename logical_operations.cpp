#include "graph.hpp"

namespace graph::logical {
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