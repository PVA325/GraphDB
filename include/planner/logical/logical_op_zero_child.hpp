#ifndef GRAPHDB_LOGICAL_OP_ZERO_CHILD_HPP
#define GRAPHDB_LOGICAL_OP_ZERO_CHILD_HPP

#include "logical_op.hpp"

namespace graph::logical {
struct LogicalOpZeroChild : LogicalOp {
  LogicalOpZeroChild() = default;

  [[nodiscard]] String SubtreeDebugString() const override { return DebugString(); }
  std::vector<LogicalOp*> GetChildren() override { return {}; }

  ~LogicalOpZeroChild() override = default;
};
}

#endif //GRAPHDB_LOGICAL_OP_ZERO_CHILD_HPP