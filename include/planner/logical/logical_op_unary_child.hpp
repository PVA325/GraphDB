#ifndef GRAPHDB_LOGICAL_OP_UNARY_CHILD_HPP
#define GRAPHDB_LOGICAL_OP_UNARY_CHILD_HPP

#include "common/common_value.hpp"
#include "logical_op.hpp"
#include "planner/utils.hpp"

namespace graph::logical {
struct LogicalOpUnaryChild : LogicalOp {
  LogicalOpPtr child;

  explicit LogicalOpUnaryChild(LogicalOpPtr child): child(std::move(child)) {}
  std::vector<LogicalOp*> GetChildren() override { return {PlannerUtils::GetUniqPtrVal(child)}; }

  [[nodiscard]] String SubtreeDebugString() const override;
  ~LogicalOpUnaryChild() override = default;
};
}
#endif //GRAPHDB_LOGICAL_OP_UNARY_CHILD_HPP
