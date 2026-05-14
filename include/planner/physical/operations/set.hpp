#pragma once

#include "physical_op_unary_child.hpp"
#include "planner/logical/logical_set.hpp"

namespace graph::exec {
struct PhysicalSetOp : public PhysicalOpUnaryChild {
  std::vector<logical::LogicalSet::Assignment> assignments;

  PhysicalSetOp(PhysicalOpPtr child, std::vector<logical::LogicalSet::Assignment> assignments);

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;

  ~PhysicalSetOp() override = default;
};

}
