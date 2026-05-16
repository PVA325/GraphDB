#pragma once

#include "common/common_value.hpp"
#include "physical_op.hpp"
namespace graph::exec {
struct PhysicalOpBinaryChild : PhysicalOp {
  PhysicalOpBinaryChild(PhysicalOpPtr left, PhysicalOpPtr right) : left(std::move(left)), right(std::move(right)) {
  }

  [[nodiscard]] String SubtreeDebugString() const override;

  ~PhysicalOpBinaryChild() override = default;

public:
  PhysicalOpPtr left;
  PhysicalOpPtr right;

};
}
