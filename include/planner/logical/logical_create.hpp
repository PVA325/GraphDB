#ifndef GRAPHDB_LOGICAL_CREATE_HPP
#define GRAPHDB_LOGICAL_CREATE_HPP

#include "logical_op_unary_child.hpp"

namespace graph::logical {
struct CreateNodeSpec {
  String dst_alias;
  std::vector<String> labels;
  std::vector<std::pair<String, Value>> properties;

  [[nodiscard]] String DebugString() const;

  CreateNodeSpec() = delete;

  explicit CreateNodeSpec(const ast::NodePattern& pattern);

  explicit CreateNodeSpec(ast::NodePattern&& pattern);
};

struct CreateEdgeSpec {
  String src_alias;
  String dst_node_alias;
  String edge_alias;
  String edge_type;
  std::vector<std::pair<String, Value>> properties;
  ast::EdgeDirection direction;

  [[nodiscard]] String DebugString() const;

  CreateEdgeSpec() = delete;

  explicit CreateEdgeSpec(const ast::CreateEdgePattern& pattern);

  explicit CreateEdgeSpec(ast::CreateEdgePattern&& pattern);
};

struct LogicalCreate : LogicalOpUnaryChild {
  std::vector<std::variant<CreateNodeSpec, CreateEdgeSpec>> items;

  LogicalCreate() = delete;

  LogicalCreate(LogicalOpPtr child, const std::vector<ast::CreateItem>& items);

  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model,
                                  storage::GraphDB* db) const override;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Create; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return child->GetSubtreeAliases(); }

  [[nodiscard]] String DebugString() const override;
};
}

#endif //GRAPHDB_LOGICAL_CREATE_HPP
