#ifndef GRAPHDB_LOGICAL_OP_BINARY_CHILD_HPP
#define GRAPHDB_LOGICAL_OP_BINARY_CHILD_HPP

#include "common/common_value.hpp"
#include "logical_op.hpp"
#include "planner/utils.hpp"

namespace graph::logical {
struct LogicalOpManyChildren : LogicalOp {
  std::vector<LogicalOpPtr> children;

  LogicalOpManyChildren() = default;
  LogicalOpManyChildren(std::vector<LogicalOpPtr> children): children(std::move(children)) {}

  [[nodiscard]] String SubtreeDebugString() const override;

  std::vector<LogicalOp*> GetChildren() override {
    std::vector<LogicalOp*> children_opers;
    for (const auto& child : children) {
      children_opers.push_back(child.get());
    }
    return children_opers;
  }

  ~LogicalOpManyChildren() override = default;
};
}

#endif //GRAPHDB_LOGICAL_OP_BINARY_CHILD_HPP
