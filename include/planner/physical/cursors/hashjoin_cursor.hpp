#ifndef GRAPHDB_HASHJOIN_CURSOR_HPP
#define GRAPHDB_HASHJOIN_CURSOR_HPP

#include "row_cursor.hpp"
#include "planner/utils.hpp"

namespace graph::exec {
struct ValueHash {
  size_t operator()(const Value& k) const {
    const auto& visitor = PlannerUtils::overloads{
      [](Int cur) { return std::hash<Int>()(cur); },
      [](const String& cur) { return std::hash<String>()(cur); },
      [](double cur) { return std::hash<double>()(cur); },
      [](bool cur) { return std::hash<bool>()(cur); }
    };
    return std::visit(visitor, k);
  }
};
struct VecValueHash { /// can do better hash
  size_t operator()(const std::vector<Value>& vec) const {
    static ValueHash v_hash;
    size_t ans = 0;
    for (const auto& cur : vec) {
      ans ^= v_hash(cur);
    }
    return ans;
  }
};

struct HashJoinCursor : public RowCursor {
  /// add mark
  /// do join for 2 NestedLoopJoin Cursor expressions based on predicate
  using CompositeKey = std::vector<Value>;

  RowCursorPtr left_cursor;
  RowCursorPtr right_cursor;
  std::vector<ast::Expr*> left_keys;
  std::vector<ast::Expr*> right_keys;

  std::unordered_map<CompositeKey, std::vector<Row>, VecValueHash> left_rows;
  Row last_right_row;

  std::unordered_map<CompositeKey, std::vector<Row>, VecValueHash>::iterator it_left;
  size_t vec_left_idx{std::numeric_limits<size_t>::max()};

  HashJoinCursor(RowCursorPtr left_cursor_a, RowCursorPtr right_cursor_a,
        const std::vector<ast::Expr*>& left_keys_a, const std::vector<ast::Expr*>& right_keys_b);

  bool next(Row& out) override;

  void close() override;

  ~HashJoinCursor() override = default;
private:
  static CompositeKey GetCompositeKey(const Row& row, const std::vector<ast::Expr*>& keys);
};

}

#endif //GRAPHDB_HASHJOIN_CURSOR_HPP