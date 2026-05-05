#include "planner/query_planner.hpp"

namespace graph::exec {

ProjectOp::ProjectOp(std::vector<ast::ReturnItem> items, PhysicalOpPtr child) :
  PhysicalOpUnaryChild(std::move(child)),
  items(std::move(items)) {
}

RowCursorPtr ProjectOp::open(ExecContext& ctx) {
  return std::make_unique<ProjectCursor>(
    std::move(child->open(ctx)),
    items
  );
}

}