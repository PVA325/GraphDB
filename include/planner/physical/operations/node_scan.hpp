#ifndef GRAPHDB_NODE_SCAN_HPP
#define GRAPHDB_NODE_SCAN_HPP

#include "physical_op_no_child.hpp"

namespace graph::exec {
struct NodeScanOp : public PhysicalOpNoChild {
  /// co complete Node scan and return Row to every Node
  String out_alias;

  NodeScanOp(String out_alias) : out_alias(std::move(out_alias)) {
  }

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;

  ~NodeScanOp() override = default;
};
}

#endif //GRAPHDB_NODE_SCAN_HPP