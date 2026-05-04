#ifndef GRAPHDB_CREATE_HPP
#define GRAPHDB_CREATE_HPP

#include "physical_op_unary_child.hpp"
#include "planner/logical/logical_create.hpp"

namespace graph::exec {
struct PhysicalCreateOp : PhysicalOpUnaryChild {
  std::vector<std::variant<logical::CreateNodeSpec, logical::CreateEdgeSpec>> items;

  PhysicalCreateOp(std::vector<std::variant<logical::CreateNodeSpec, logical::CreateEdgeSpec>> items);
  PhysicalCreateOp(std::vector<std::variant<logical::CreateNodeSpec, logical::CreateEdgeSpec>> items,
                   PhysicalOpPtr child);
  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;

  ~PhysicalCreateOp() override = default;
};

}

#endif //GRAPHDB_CREATE_HPP