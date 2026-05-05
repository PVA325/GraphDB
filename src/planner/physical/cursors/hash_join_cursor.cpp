#include "planner/query_planner.hpp"

namespace graph::exec {

HashJoinCursor::CompositeKey HashJoinCursor::GetCompositeKey(const Row& row, const std::vector<ast::Expr*>& keys) {
  std::vector<Value> curr_key;
  for (ast::Expr* expr : keys) {
    curr_key.emplace_back(std::move((*expr)(ast::EvalContext(row))));
  }
  return curr_key;;
}

HashJoinCursor::HashJoinCursor(RowCursorPtr left_cursor_a, RowCursorPtr right_cursor_a,
                               const std::vector<ast::Expr*>& left_keys_a,
                               const std::vector<ast::Expr*>& right_keys_b) :
  left_cursor(std::move(left_cursor_a)),
  right_cursor(std::move(right_cursor_a)),
  left_keys(left_keys_a),
  right_keys(right_keys_b) {
  Row curr_row;
  while (left_cursor->next(curr_row)) {
    left_rows[GetCompositeKey(curr_row, left_keys)].emplace_back(std::move(curr_row));
    curr_row.clear();
  }
  it_left = left_rows.end();
}

bool HashJoinCursor::next(Row& out) {
  while (it_left == left_rows.end() || it_left->second.size() <= vec_left_idx) {
    last_right_row.clear();
    if (!right_cursor->next(last_right_row)) {
      return false;
    }
    CompositeKey right_key = GetCompositeKey(last_right_row, right_keys);
    auto it = left_rows.find(right_key);

    if (it != left_rows.end() && !it->second.empty()) {
      it_left = it;
      vec_left_idx = 0;
      break;
    }
  }

  Row left_row = it_left->second[vec_left_idx++];
  out = MergeRows(left_row, last_right_row);
  return true;
}

void HashJoinCursor::close() {
  left_cursor->close();
  right_cursor->close();
}

}