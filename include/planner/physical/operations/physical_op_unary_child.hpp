#ifndef GRAPHDB_PHYSICAL_OP_UNARY_CHILD_HPP
#define GRAPHDB_PHYSICAL_OP_UNARY_CHILD_HPP
#include "common/common_value.hpp"
#include "physical_op.hpp"
namespace graph::exec {
struct PhysicalOpUnaryChild : PhysicalOp {
  PhysicalOpPtr child;

  PhysicalOpUnaryChild(PhysicalOpPtr child) : child(std::move(child)) {
  }

  [[nodiscard]] String SubtreeDebugString() const override;

  ~PhysicalOpUnaryChild() override = default;
};
}
#endif //GRAPHDB_PHYSICAL_OP_UNARY_CHILD_HPP