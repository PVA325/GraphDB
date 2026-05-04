#ifndef GRAPHDB_LOGICAL_OP_BINARY_CHILD_HPP
#define GRAPHDB_LOGICAL_OP_BINARY_CHILD_HPP

#include "common/common_value.hpp"
#include "logical_op.hpp"
#include "planner/utils.hpp"

namespace graph::logical {
struct LogicalOpBinaryChild : LogicalOp {
  LogicalOpPtr left;
  LogicalOpPtr right;

  LogicalOpBinaryChild(LogicalOpPtr left, LogicalOpPtr right);

  [[nodiscard]] String SubtreeDebugString() const override;

  std::vector<LogicalOp*> GetChildren() override {
    return {PlannerUtils::GetUniqPtrVal(left), PlannerUtils::GetUniqPtrVal(right)};
  }

  ~LogicalOpBinaryChild() override = default;
};
}

#endif //GRAPHDB_LOGICAL_OP_BINARY_CHILD_HPP
