#ifndef GRAPHDB_OPTIMIZER_HPP
#define GRAPHDB_OPTIMIZER_HPP
#include "planner/utils.hpp"
#include "planner/logical_planner.hpp"
#include "common/common_value.hpp"

namespace {
  using ExprPtrVec = std::vector<std::unique_ptr<ast::Expr>>;
}

namespace graph::optimizer {
using ::graph::optimizer::CostEstimate;
void optimize_logical_plan_impl(std::unique_ptr<logical::LogicalOp>& op);

// Cost model interface
struct CostModel {
  virtual ~CostModel() = default;

  virtual CostEstimate EstimateAllNodeScan(const storage::GraphDB* db, const logical::LogicalScan& scan) const = 0;

  /// return estimated cost and label on which we should do index
  virtual std::pair<CostEstimate, String> EstimateIndexSeekLabel(const storage::GraphDB* db,
                                                                 const logical::LogicalScan& scan) const = 0;

  virtual CostEstimate EstimateExpand(
    const storage::GraphDB* db,
    const logical::LogicalExpand& expand,
    const CostEstimate& input) const = 0;

  virtual CostEstimate EstimateFilter(
    const storage::GraphDB* db,
    const CostEstimate& input,
    const ast::Expr* pred) const = 0;

  virtual CostEstimate EstimateNestedJoin(
    const storage::GraphDB* db,
    const CostEstimate& left,
    const CostEstimate& right,
    const ast::Expr* pred) const = 0;

  virtual CostEstimate EstimateHashJoin(
    const storage::GraphDB* db,
    const CostEstimate& left,
    const CostEstimate& right,
    const std::vector<ast::Expr*>& left_keys,
    const std::vector<ast::Expr*>& right_keys) const = 0;

  virtual CostEstimate EstimateProject(const storage::GraphDB* db, const CostEstimate& child, const logical::LogicalProject& proj) const = 0;

  virtual CostEstimate EstimateSort(const storage::GraphDB* db, const CostEstimate& input) const = 0;

  virtual CostEstimate EstimateLimit(const storage::GraphDB* db, const CostEstimate& input, size_t limit) const = 0;

  virtual CostEstimate EstimateSet(const storage::GraphDB* db, const CostEstimate& input, const logical::LogicalSet& set) const = 0;

  virtual CostEstimate EstimateCreate(const storage::GraphDB* db, const CostEstimate& input, const logical::LogicalCreate& create) const = 0;
  virtual CostEstimate EstimateDelete(const storage::GraphDB* db, const CostEstimate& input, const logical::LogicalDelete& del) const = 0;

private:
  double cpu_cost = 1.0;
  double io_cost = 3.0;
};

struct DefaultCostModel : CostModel {
  ~DefaultCostModel() override = default;
  CostEstimate EstimateAllNodeScan(const storage::GraphDB* db, const logical::LogicalScan& scan) const override;
  std::pair<CostEstimate, String> EstimateIndexSeekLabel(const storage::GraphDB* db,
                                                         const logical::LogicalScan& scan) const override;


  CostEstimate EstimateExpand(
    const storage::GraphDB* db,
    const logical::LogicalExpand& expand,
    const CostEstimate& input) const override;

  CostEstimate EstimateFilter(
    const storage::GraphDB* db,
    const CostEstimate& input,
    const ast::Expr* pred) const override;

  CostEstimate EstimateNestedJoin(
    const storage::GraphDB* db,
    const CostEstimate& left,
    const CostEstimate& right,
    const ast::Expr* pred) const override;

  CostEstimate EstimateHashJoin(
    const storage::GraphDB* db,
    const CostEstimate& left,
    const CostEstimate& right,
    const std::vector<ast::Expr*>& left_keys,
    const std::vector<ast::Expr*>& right_keys) const override;

  CostEstimate EstimateProject(
    const storage::GraphDB* db,
    const CostEstimate& child,
    const logical::LogicalProject& proj) const override;

  CostEstimate EstimateSort(const storage::GraphDB* db, const CostEstimate& input) const override;

  CostEstimate EstimateLimit(const storage::GraphDB* db, const CostEstimate& input, size_t limit) const override;

  CostEstimate EstimateSet(const storage::GraphDB* db, const CostEstimate& input, const logical::LogicalSet& set) const override;
  CostEstimate EstimateCreate(const storage::GraphDB* db, const CostEstimate& input, const logical::LogicalCreate& create) const override;
  CostEstimate EstimateDelete(const storage::GraphDB* db, const CostEstimate& input, const logical::LogicalDelete& del) const override;

private:
  [[nodiscard]] static double EstimateNodePropertySelectivity(const std::vector<std::pair<String, Value>>& props,
                                                              const storage::GraphDB* gb);
  [[nodiscard]] static double EstimateNodeLabelsSelectivity(const std::vector<String>& labels,
                                                     const storage::GraphDB* gb);
  [[nodiscard]] static std::pair<double, String> EstimateBiggestLabelSelectivity(
    const std::vector<String>& labels, const storage::GraphDB* gb);
  [[nodiscard]] static double EstimateEdgeTypeSelectivity(const std::optional<std::string>& label,
                                                    const storage::GraphDB* db);

  static constexpr double kRandomReadCost = 1.5;
  static constexpr double kSeqReadCost = 0.3;
  static constexpr double kCpuPerRow = 1.0;
  static constexpr double kIoPerRow = 1.0;
  static constexpr double kFilterCpu = 0.5;
  static constexpr double kCpuPerEdge = 1.0;
  static constexpr double kIoPerEdge = 1.0;
  static constexpr double kCpuHashBuild = 5.0;
  static constexpr double kCpuHashProbe = 5.0;
};

std::tuple<CostEstimate, ExprPtrVec, ExprPtrVec, std::unique_ptr<ast::Expr>> EstimateHashJoin(
  const logical::LogicalJoin* join, ::graph::exec::ExecContext& ctx,
  CostModel* cost_model, storage::GraphDB* db,
  const CostEstimate& left_cost, const CostEstimate& right_cost
);
} // namespace graph::optimizer;
#endif //GRAPHDB_OPTIMIZER_HPP