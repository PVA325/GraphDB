#pragma once

#include "common/common_value.hpp"
#include "logical_op.hpp"
#include "planner/utils.hpp"

namespace graph::logical {
struct LogicalOpManyChildren : LogicalOp {
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

public:
  std::vector<LogicalOpPtr> children;
};
}
