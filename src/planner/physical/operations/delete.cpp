#include "planner/query_planner.hpp"

namespace graph::exec {

PhysicalDeleteOp::PhysicalDeleteOp(std::vector<String> aliases, PhysicalOpPtr child) :
  PhysicalOpUnaryChild(std::move(child)),
  aliases(std::move(aliases)) {
}

RowCursorPtr PhysicalDeleteOp::open(ExecContext& ctx) {
  return std::make_unique<DeleteCursor>(
    std::move(child->open(ctx)),
    aliases,
    ctx.db
  );
}

}
