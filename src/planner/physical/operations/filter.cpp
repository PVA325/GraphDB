#include "planner/query_planner.hpp"

namespace graph::exec {

RowCursorPtr FilterOp::open(ExecContext& ctx) {
  return std::make_unique<FilterCursor>(
    std::move(child->open(ctx)),
    predicate
  );
}

FilterOp::FilterOp(
  ast::Expr* predicate,
  PhysicalOpPtr child) :
  PhysicalOpUnaryChild(std::move(child)),
  predicate(predicate) {
}

}