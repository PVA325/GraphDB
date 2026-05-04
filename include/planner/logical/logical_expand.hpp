#ifndef GRAPHDB_LOGICAL_EXPAND_HPP
#define GRAPHDB_LOGICAL_EXPAND_HPP

#include "logical_op_aliased.hpp"
#include <optional>

namespace graph::logical {
struct LogicalExpand : public AliasedLogicalOp {
  /// Expand Nodes that are located in row by $src_alias slot name and move to $dst_alias
  LogicalOpPtr child;

  String src_alias;
  String edge_alias;

  std::optional<String> edge_type;
  std::vector<String> dst_vertex_labels;
  std::vector<std::pair<String, Value>> dst_vertex_properties;
  ast::EdgeDirection direction;

  LogicalExpand() = delete;

  LogicalExpand(LogicalOpPtr child, String src_alias, String edge_alias, String dst_alias,
                std::optional<String> edge_type, std::vector<String> dst_vertex_labels,
                std::vector<std::pair<String, Value>> dst_vertex_properties,
                ast::EdgeDirection direction);

  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model,
                                  storage::GraphDB* db) const override;

  [[nodiscard]] String DebugString() const override;
  [[nodiscard]] String SubtreeDebugString() const final;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Expand; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return {dst_alias}; }

  ~LogicalExpand() override = default;
};
}

#endif //GRAPHDB_LOGICAL_EXPAND_HPP
