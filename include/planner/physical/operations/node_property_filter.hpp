#pragma once

#include "physical_op_unary_child.hpp"

namespace graph::exec {
struct NodePropertyFilterOp : PhysicalOpUnaryChild {
  String alias;
  std::vector<String> labels;
  std::vector<std::pair<String, Value>> properties;

  NodePropertyFilterOp(PhysicalOpPtr child, String alias, std::vector<String> labels,
                       std::vector<std::pair<String, Value>> properties);
  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;

  ~NodePropertyFilterOp() override = default;
};
}
