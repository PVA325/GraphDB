#ifndef GRAPHDB_LIMIT_HPP
#define GRAPHDB_LIMIT_HPP

#include "physical_op_unary_child.hpp"

namespace graph::exec {
struct LimitOp : public PhysicalOpUnaryChild {
  /// just physical limit
  Int limit_size;

  LimitOp(Int limit_size, PhysicalOpPtr child);

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;
  ~LimitOp() override = default;
};
}

#endif //GRAPHDB_LIMIT_HPP