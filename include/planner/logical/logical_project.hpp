#ifndef GRAPHDB_LOGICAL_PROJECT_HPP
#define GRAPHDB_LOGICAL_PROJECT_HPP

#include "logical_op_unary_child.hpp"

namespace graph::logical {

struct LogicalProject : public LogicalOpUnaryChild {
  /// Project of values to items(can be field - string or Expression)
  std::vector<ast::ReturnItem> items;

  LogicalProject() = delete;

  LogicalProject(LogicalOpPtr child, std::vector<ast::ReturnItem> items);

  [[nodiscard]] String DebugString() const override;
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model, storage::GraphDB* db) const override;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Project; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return child->GetSubtreeAliases(); }

  ~LogicalProject() override = default;
};

}

#endif //GRAPHDB_LOGICAL_PROJECT_HPP