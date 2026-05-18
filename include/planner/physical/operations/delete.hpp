#pragma once

#include "physical_op_unary_child.hpp"

namespace graph::exec {

struct PhysicalDeleteOp : PhysicalOpUnaryChild {
  PhysicalDeleteOp(std::vector<String> aliases, PhysicalOpPtr child);

  RowCursorPtr open(ExecContext& ctx) override;
  [[nodiscard]] String DebugString() const override;

  ~PhysicalDeleteOp() override = default;

public:
  std::vector<String> aliases;
};

}
