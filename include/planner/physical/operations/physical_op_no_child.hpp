#pragma once

#include "common/common_value.hpp"
#include "physical_op.hpp"

namespace graph::exec {
struct PhysicalOpNoChild : PhysicalOp {
  PhysicalOpNoChild() = default;

  [[nodiscard]] String SubtreeDebugString() const override { return DebugString(); }

  ~PhysicalOpNoChild() override = default;
};
}
