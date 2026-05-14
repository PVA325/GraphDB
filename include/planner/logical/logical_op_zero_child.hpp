#pragma once

#include "logical_op.hpp"

namespace graph::logical {
struct LogicalOpZeroChild : LogicalOp {
  LogicalOpZeroChild() = default;

  [[nodiscard]] String SubtreeDebugString() const override { return DebugString(); }
  std::vector<LogicalOp*> GetChildren() override { return {}; }

  ~LogicalOpZeroChild() override = default;
};
}
