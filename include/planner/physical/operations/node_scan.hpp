#pragma once

#include "physical_op_no_child.hpp"

namespace graph::exec {
struct NodeScanOp : public PhysicalOpNoChild {
  /// co complete Node scan and return Row to every Node
  NodeScanOp(String out_alias) : out_alias(std::move(out_alias)) {
  }

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;

  ~NodeScanOp() override = default;

public:
  String out_alias;
};
}
