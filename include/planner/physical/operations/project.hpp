#pragma once

#include "physical_op_unary_child.hpp"

namespace graph::exec {
struct ProjectOp : public PhysicalOpUnaryChild {
  /// child is next operator in the tree
  ProjectOp(std::vector<ast::ReturnItem> items,
            PhysicalOpPtr child);

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;

  ~ProjectOp() override = default;

public:
  std::vector<ast::ReturnItem> items;
};
}
