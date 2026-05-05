#ifndef GRAPHDB_DELETE_HPP
#define GRAPHDB_DELETE_HPP

#include "physical_op_unary_child.hpp"

namespace graph::exec {

struct PhysicalDeleteOp : PhysicalOpUnaryChild {
  std::vector<String> aliases;

  PhysicalDeleteOp(std::vector<String> aliases, PhysicalOpPtr child);

  RowCursorPtr open(ExecContext& ctx) override;
  [[nodiscard]] String DebugString() const override;

  ~PhysicalDeleteOp() override = default;
};

}

#endif //GRAPHDB_DELETE_HPP