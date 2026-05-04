#ifndef GRAPHDB_LOGICAL_SCAN_HPP
#define GRAPHDB_LOGICAL_SCAN_HPP
#include "common/common_value.hpp"
#include "logical_op_aliased.hpp"

namespace graph::logical {
struct LogicalScan : public AliasedLogicalOp {
  /// Scan Nodes that has specified labels
  /// and write out in row with slot name alias
  std::vector<String> labels;
  std::vector<std::pair<String, Value>> property_filters;

  LogicalScan(std::vector<String> labels, String dst_alias);

  LogicalScan(std::vector<String> labels, String alias, std::vector<std::pair<String, Value>> property_filters);

  BuildPhysicalType
  BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model, storage::GraphDB* db) const override;
  [[nodiscard]] String DebugString() const override;

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return {dst_alias}; }

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Scan; }

  ~LogicalScan() override = default;
};
}

#endif //GRAPHDB_LOGICAL_SCAN_HPP
