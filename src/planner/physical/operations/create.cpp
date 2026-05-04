#include "planner/query_planner.hpp"

namespace graph::exec {

PhysicalCreateOp::PhysicalCreateOp(std::vector<std::variant<logical::CreateNodeSpec, logical::CreateEdgeSpec>> items) :
  PhysicalOpUnaryChild(nullptr),
  items(std::move(items)) {
}

PhysicalCreateOp::PhysicalCreateOp(std::vector<std::variant<logical::CreateNodeSpec, logical::CreateEdgeSpec>> items,
                                   PhysicalOpPtr child) :
  PhysicalOpUnaryChild(std::move(child)),
  items(std::move(items)) {
}

RowCursorPtr PhysicalCreateOp::open(ExecContext& ctx) {
  return std::make_unique<CreateCursor>(
    (child == nullptr ? nullptr : child->open(ctx)),
    items,
    ctx.db
  );
}

}