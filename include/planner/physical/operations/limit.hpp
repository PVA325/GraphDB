#pragma once

#include "physical_op_unary_child.hpp"

namespace graph::exec {
struct LimitOp : public PhysicalOpUnaryChild {
  /// just physical limit
  LimitOp(LongInt limit_size, PhysicalOpPtr child);

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;
  ~LimitOp() override = default;

public:
  LongInt limit_size;
};
}
