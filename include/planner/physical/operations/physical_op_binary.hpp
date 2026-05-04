#ifndef GRAPHDB_PHYSICAL_OP_BINARY_HPP
#define GRAPHDB_PHYSICAL_OP_BINARY_HPP

#include "common/common_value.hpp"
#include "physical_op.hpp"
namespace graph::exec {
struct PhysicalOpBinaryChild : PhysicalOp {
  PhysicalOpPtr left;
  PhysicalOpPtr right;

  PhysicalOpBinaryChild(PhysicalOpPtr left, PhysicalOpPtr right) : left(std::move(left)), right(std::move(right)) {
  }

  [[nodiscard]] String SubtreeDebugString() const override;

  ~PhysicalOpBinaryChild() override = default;
};

}
#endif //GRAPHDB_PHYSICAL_OP_BINARY_HPP