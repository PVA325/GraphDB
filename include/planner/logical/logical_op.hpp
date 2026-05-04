#ifndef GRAPHDB_LOGICAL_OP_HPP
#define GRAPHDB_LOGICAL_OP_HPP
#include "common/common_value.hpp"

namespace graph::logical {

struct LogicalOp {
  [[nodiscard]] virtual String DebugString() const = 0;
  virtual BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model,
                                          storage::GraphDB* db) const = 0;

  [[nodiscard]] virtual String SubtreeDebugString() const = 0;
  [[nodiscard]] virtual LogicalOpType Type() const = 0;

  virtual std::vector<LogicalOp*> GetChildren() = 0;

  [[nodiscard]] virtual std::vector<String> GetSubtreeAliases() const = 0;

  virtual ~LogicalOp() = default;
};

}

#endif //GRAPHDB_LOGICAL_OP_HPP