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

FilterOp::FilterOp(std::unique_ptr<ast::Expr> predicate, PhysicalOpPtr child):
  PhysicalOpUnaryChild(std::move(child)),
  predicate_storage(std::move(predicate)),
  predicate(predicate_storage.get()) {}
}
