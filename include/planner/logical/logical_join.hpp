#ifndef GRAPHDB_LOGICAL_JOIN_HPP
#define GRAPHDB_LOGICAL_JOIN_HPP

#include "logical_op_binary_child.hpp"

namespace graph::logical {
struct LogicalJoin : LogicalOpBinaryChild {
  /// Logical Joint for 2 results with predicate(optional)

  std::unique_ptr<ast::Expr> predicate;

  LogicalJoin() = delete;

  LogicalJoin(LogicalOpPtr left, LogicalOpPtr right);

  LogicalJoin(LogicalOpPtr left, LogicalOpPtr right, std::unique_ptr<ast::Expr> predicate);
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model,
                                  storage::GraphDB* db) const override;


  [[nodiscard]] String DebugString() const override;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Join; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final {
    std::vector<String> result;
    auto l_ans = left->GetSubtreeAliases();
    auto r_ans = right->GetSubtreeAliases();

    result.insert(result.end(), std::make_move_iterator(l_ans.begin()), std::make_move_iterator(r_ans.end()));
    result.insert(result.end(), std::make_move_iterator(r_ans.begin()), std::make_move_iterator(r_ans.end()));
    return result;
  }

  ~LogicalJoin() override = default;
};
}

#endif //GRAPHDB_LOGICAL_JOIN_HPP
