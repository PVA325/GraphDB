#ifndef GRAPHDB_NESTEED_LOOP_JOIN_CURSOR_HPP
#define GRAPHDB_NESTEED_LOOP_JOIN_CURSOR_HPP

#include "row_cursor.hpp"
#include "eval_context/eval_context.hpp"

namespace graph::exec {

struct NestedLoopJoinCursor : public RowCursor {
  /// do join for 2 expressions based on predicate
  RowCursorPtr left_cursor;
  RowCursorPtr right_cursor;
  PhysicalOp* right_operation;
  const ast::Expr* predicate;
  ExecContext& ctx;

  Row left_row;

  NestedLoopJoinCursor(RowCursorPtr left_cursor, PhysicalOp* right_operation,
                       const ast::Expr* pred, ExecContext& ctx);

  bool next(Row& out) override;

  void close() override;

  ~NestedLoopJoinCursor() override = default;
};

}

#endif //GRAPHDB_NESTEED_LOOP_JOIN_CURSOR_HPP