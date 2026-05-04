#ifndef GRAPHDB_PHYSICAL_OP_NO_CHILD_HPP
#define GRAPHDB_PHYSICAL_OP_NO_CHILD_HPP

#include "common/common_value.hpp"
#include "physical_op.hpp"

namespace graph::exec {
struct PhysicalOpNoChild : PhysicalOp {
  PhysicalOpNoChild() = default;

  [[nodiscard]] String SubtreeDebugString() const override { return DebugString(); }

  ~PhysicalOpNoChild() override = default;
};
}

#endif //GRAPHDB_PHYSICAL_OP_NO_CHILD_HPP
