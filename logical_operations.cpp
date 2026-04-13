#include "graph.hpp"
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

  exec::PhysicalOpPtr LogicalScan::BuildPhysical(exec::ExecContext& ctx) const {
    exec::PhysicalOpPtr physical_root;
    if (labels.size() == 1) {
      physical_root = std::make_unique<exec::LabelIndexSeekOp>(
        dst_alias,
        labels[0]
      );
    } else {
      physical_root = std::make_unique<exec::NodeScanOp>(
        dst_alias
      );
    }

    physical_root = std::move(
      std::make_unique<exec::NodePropertyFilterOp>(
        std::move(physical_root),
        dst_alias,
        labels,
        property_filters
      )
    );

    return std::move(physical_root);
  }

  LogicalScan::LogicalScan(std::vector<String> labels, String dst_alias) :
    AliasedLogicalOp(std::move(dst_alias)),
    labels(std::move(labels)) {}

  LogicalExpand::LogicalExpand(LogicalOpPtr child, String src_alias, String edge_alias, String dst_alias,
          std::optional<String> edge_type, std::vector<String> dst_vertex_labels,
          std::vector<std::pair<String, Value>> dst_vertex_properties,
          ast::EdgeDirection direction) :
    AliasedLogicalOp(std::move(dst_alias)),
    child(std::move(child)),
    src_alias(std::move(src_alias)),
    edge_alias(std::move(edge_alias)),
    edge_type(std::move(edge_type)),
    dst_vertex_labels(std::move(dst_vertex_labels)),
    dst_vertex_properties(std::move(dst_vertex_properties)),
    direction(direction) {}


  exec::PhysicalOpPtr LogicalExpand::BuildPhysical(exec::ExecContext& ctx) const {
    exec::PhysicalOpPtr physical_root;
    if (direction == ast::EdgeDirection::Right) {
      physical_root = std::make_unique<exec::ExpandOutgoingOp>(
        src_alias,
        edge_alias,
        dst_alias,
        edge_type,
        std::move(child->BuildPhysical(ctx))
      );
    } else {
      physical_root = std::make_unique<exec::ExpandIngoingOp>(
        src_alias,
        edge_alias,
        dst_alias,
        edge_type,
        std::move(child->BuildPhysical(ctx))
      );
    }
    if (!dst_vertex_labels.empty() || !dst_vertex_properties.empty()) {
      physical_root = std::make_unique<exec::NodePropertyFilterOp>(
        std::move(physical_root),
        dst_alias,
        dst_vertex_labels,
        dst_vertex_properties
      );
    }
    return std::move(physical_root);
  }

  LogicalFilter::LogicalFilter(LogicalOpPtr child, std::unique_ptr<ast::Expr> predicate) :
    LogicalOpUnaryChild(std::move(child)),
    predicate(std::move(predicate)) {}

  exec::PhysicalOpPtr LogicalFilter::BuildPhysical(exec::ExecContext& ctx) const {
    return std::make_unique<exec::FilterOp>(
      predicate.get(),
      std::move(child->BuildPhysical(ctx))
    );
  }

  LogicalProject::LogicalProject(LogicalOpPtr child, std::vector<ast::ReturnItem> items) :
    LogicalOpUnaryChild(std::move(child)),
    items(std::move(items)) {}

  exec::PhysicalOpPtr LogicalProject::BuildPhysical(exec::ExecContext& ctx) const {
    return std::make_unique<exec::ProjectOp>(
      items,
      std::move(child->BuildPhysical(ctx))
      );
  }

  LogicalLimit::LogicalLimit(LogicalOpPtr child, size_t limit_size) :
    LogicalOpUnaryChild(std::move(child)),
    limit_size(limit_size) {}

  exec::PhysicalOpPtr LogicalLimit::BuildPhysical(exec::ExecContext& ctx) const {
    return std::make_unique<exec::LimitOp>(
      limit_size,
      child->BuildPhysical(ctx)
    );
  }

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

  exec::PhysicalOpPtr LogicalJoin::BuildPhysical(exec::ExecContext& ctx) const {
    // return std::make_unique<exec::KeyHashJoinOp>(
    //   std::move(left->BuildPhysical(ctx)),
    //   std::move(right->BuildPhysical(ctx)),
    //   left_alias, right_alias,
    //   left_feature_ket, right_feature_key
    // );
    return std::make_unique<exec::NestedLoopJoinOp>(
      std::move(left->BuildPhysical(ctx)),
      std::move(right->BuildPhysical(ctx)),
      predicate.get()
    );
  }

  LogicalSet::LogicalSet(LogicalOpPtr child, String alias, String key, Value value) :
    LogicalOpUnaryChild(std::move(child)),
    assignment{std::move(alias), std::move(key), std::move(value)} {}

  exec::PhysicalOpPtr LogicalSet::BuildPhysical(exec::ExecContext& ctx) const {
    ;
    return std::make_unique<exec::PhysicalSetOp>( // REWRITE AFTER ARTEM CHANGES
      std::vector<String>{assignment.alias},
      std::vector<std::vector<String> >{}, /// ARTEM TODO
      std::vector<std::vector<std::pair<String, Value> > >{{std::make_pair(assignment.key, assignment.value)}},
      std::move(child->BuildPhysical(ctx))
    );
  }

  LogicalDelete::LogicalDelete(LogicalOpPtr child, std::vector<String> target) :
    LogicalOpUnaryChild(std::move(child)),
    target(std::move(target)) {}

  exec::PhysicalOpPtr LogicalDelete::BuildPhysical(exec::ExecContext& ctx) const {
    return std::make_unique<exec::PhysicalDeleteOp>(
      target,
      std::move(child->BuildPhysical(ctx))
    );
  }

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

  exec::PhysicalOpPtr LogicalCreate::BuildPhysical(exec::ExecContext& ctx) const {
    return std::make_unique<exec::PhysicalCreateOp>(
      items,
      std::move(child->BuildPhysical(ctx))
    );
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