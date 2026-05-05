#ifndef GRAPHDB_EXPAND_HPP
#define GRAPHDB_EXPAND_HPP

#include <optional>

#include "physical_op_unary_child.hpp"

namespace graph::exec {
template <bool edge_outgoing>
struct ExpandOp : public PhysicalOpUnaryChild {
  /// write do dst_alias outgoing edge of edge_type
  String src_alias;
  String dst_edge_alias;
  String dst_node_alias;
  std::optional<String> edge_type;

  ExpandOp(String src_alias, String dst_edge_alias, String dst_node_alias, std::optional<String> edge_type,
           PhysicalOpPtr child);

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;

  ~ExpandOp() override = default;
};

}

#include "expand.tpp"

#endif //GRAPHDB_EXPAND_HPP