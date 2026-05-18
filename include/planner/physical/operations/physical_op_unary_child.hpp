#pragma once

#include "common/common_value.hpp"
#include "physical_op.hpp"
namespace graph::exec {
struct PhysicalOpUnaryChild : PhysicalOp {
  PhysicalOpUnaryChild(PhysicalOpPtr child) : child(std::move(child)) {
  }

  [[nodiscard]] String SubtreeDebugString() const override;

  ~PhysicalOpUnaryChild() override = default;

public:
  PhysicalOpPtr child;
};
}
