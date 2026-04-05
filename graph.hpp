#pragma once
#include <iostream>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include <map>
#include <set>
#include <unordered_map>
#include <numeric>
#include <functional>
#include "ast.hpp"
#include "storage.hpp"

namespace graph {
namespace frontend {
  using ast::Expr;
  using ast::ReturnItem;
  using ast::QueryAST;
  using ast::MatchClause;
  using storage::GraphDB;
  using ast::Pattern;
  using ast::PatternElement;
  using ast::NodePattern;
  using ast::MatchEdgePattern;
  using ast::EdgeDirection;
  using ast::OrderItem;
  using ast::OrderDirection;
  using ast::SetClause;
  using ast::DeleteClause;
}
};

namespace graph {
using Int = int64_t;
using Double = double;
using String = std::string;
using Bool = bool;
using NodeId = uint64_t;
using EdgeId = uint64_t;
//  enum class EdgeDirection { Outgoing, Incoming, Undirected };

using Value = std::variant<Int, Double, String, Bool>;


struct PlannerError : public std::runtime_error { // todo at the ened
  PlannerError(const std::string &msg);
};

struct ExecutionError : public std::runtime_error { // todo at the ened
  ExecutionError(const std::string &msg);
};


namespace PlannerUtils {
  template<typename... Types>
  struct overloads : Types... {
    using Types::operator()...;
  };
  String toString(const Value& val);
  String ConcatStrVector(const std::vector<String>& v);
  String ConcatProperties(const std::vector<std::pair<String, Value>>& v);
  String EdgeStrByDirection(frontend::EdgeDirection dir);
  template<typename PropertyM>
  requires(std::is_same_v<std::decay_t<PropertyM>, ast::PropertyMap>)
  void transferProperties(
      std::vector<std::pair<String, Value>>& prop,
      PropertyM&& other_prop) {
    if (!prop.empty()) {
      throw std::runtime_error("Properties transfer invariant violation");
    }
    prop.reserve(other_prop.size());
    for (size_t i = 0; i < other_prop.size(); ++i) {
      if constexpr (std::is_lvalue_reference_v<PropertyM>) {
        prop.emplace_back(
          other_prop[i].first,
          other_prop[i].second.value
        );
      } else {
        prop.emplace_back(
          std::move(other_prop[i].first),
          std::move(other_prop[i].second.value)
        );
      }

    }
  }
  bool ValueToBool(Value val);
}
}

namespace graph {
namespace planner {
struct LogicalOp;
using LogicalOpPtr = std::unique_ptr<LogicalOp>;

struct LogicalOp {
  virtual ~LogicalOp() = default;
  virtual String DebugString() const  = 0;
  virtual String SubtreeDebugString() const  = 0;
};

struct LogicalOpUnaryChild : LogicalOp {
  LogicalOpPtr child;
  explicit LogicalOpUnaryChild(LogicalOpPtr child);
  String SubtreeDebugString() const override;
};
struct LogicalOpBinaryChild : LogicalOp {
  LogicalOpPtr left;
  LogicalOpPtr right;
  LogicalOpBinaryChild(LogicalOpPtr left, LogicalOpPtr right);
  String SubtreeDebugString() const override;
};

struct AliasedLogicalOp: public LogicalOp {
  std::string dst_alias;
  AliasedLogicalOp(std::string dst_alias): dst_alias(std::move(dst_alias)) {}
};
struct LogicalScan : public AliasedLogicalOp {
  /// Scan Nodes that has specified labels
  /// and write out in row with slot name alias
  std::vector<String> labels;
  std::vector<std::pair<String, Value> > property_filters;

  LogicalScan() = delete;
  LogicalScan(const LogicalScan& other) = default;
  LogicalScan(LogicalScan&& other) = default;

  LogicalScan(std::vector<String> labels, String dst_alias);
  LogicalScan(std::vector<String> labels, String alias, std::vector<std::pair<String, Value> > property_filters);
  String DebugString() const override;
  virtual String SubtreeDebugString() const override;
  ~LogicalScan() override = default;
};
struct LogicalExpand : public AliasedLogicalOp {
  /// Expand Nodes that are located in row by $src_alias slot name and move to $dst_alias
  String src_alias;
  String edge_alias;
  /// bool optional; can add for Optional match
  std::optional<std::vector<String>> edge_labels;
  std::optional<std::vector<String>> dst_vertex_labels;
  frontend::EdgeDirection direction;
  LogicalOpPtr child;

  LogicalExpand() = delete;
  LogicalExpand(LogicalOpPtr child, String src_alias, String edge_alias, String dst_alias, frontend::EdgeDirection direction);
  LogicalExpand(LogicalOpPtr child, String src_alias, String edge_alias, String dst_alias, std::vector<String> edge_labels, std::vector<String> dst_vertex_labels, frontend::EdgeDirection direction);
  String DebugString() const override;
  virtual String SubtreeDebugString() const override;
  ~LogicalExpand() override = default;
};

struct LogicalFilter : public LogicalOpUnaryChild {
  /// Filter - not include int answer all values that do not satisfy predicate
  std::unique_ptr<frontend::Expr> predicate;

  LogicalFilter() = delete;
  explicit LogicalFilter(LogicalOpPtr child, std::unique_ptr<frontend::Expr> predicate);

  String DebugString() const override;
  ~LogicalFilter() override = default;
};

struct LogicalProject : public LogicalOpUnaryChild {
  /// Project of values to items(can be field - string or Expression)
  std::vector<frontend::ReturnItem> items;

  LogicalProject() = delete;
  LogicalProject(LogicalOpPtr child, std::vector<frontend::ReturnItem> items);
  String DebugString() const override;
  ~LogicalProject() override = default;
};

struct LogicalLimit : public LogicalOpUnaryChild {
  /// Logical Limit - nothing to comment
  Int limit_size;

  LogicalLimit() = delete;
  explicit LogicalLimit(LogicalOpPtr child, size_t limit_size);
  String DebugString() const override;
  ~LogicalLimit() override = default;
};

struct LogicalSort : LogicalOpUnaryChild {
  /// Logical Sort - sort by expression in keys
  /// for example
  /*
    {
      {expr1, true},   // ASC
      {expr2, false}   // DESC
    } and sort if by expr1 and if they are equal by expr2
   */
  std::vector<frontend::OrderItem> keys;

  LogicalSort() = delete;
  explicit LogicalSort(LogicalOpPtr child, std::vector<frontend::OrderItem> keys);
  String DebugString() const override;
  ~LogicalSort() override = default;
};

struct LogicalJoin : LogicalOpBinaryChild {
  /// Logical Joint for 2 results with predicate(optional)

  std::optional<std::unique_ptr<frontend::Expr>> predicate;
  LogicalJoin() = delete;
  LogicalJoin(LogicalOpPtr left, LogicalOpPtr right);
  LogicalJoin(LogicalOpPtr left, LogicalOpPtr right, std::unique_ptr<frontend::Expr> predicate);
  String DebugString() const override;
  ~LogicalJoin() override = default;
};

struct LogicalPlan : LogicalOp {
  /// Container logical plan
  LogicalOpPtr root;
  LogicalPlan(): root(nullptr) {}
  LogicalPlan(LogicalPlan&& other): root(std::move(other.root)) {}

  explicit LogicalPlan(LogicalOpPtr r): root(std::move(r)) {}
  String DebugString() const override;
  String SubtreeDebugString() const override;

  ~LogicalPlan() override = default;
};

struct LogicalSet : LogicalOpUnaryChild {
  /// Create LogicalSet; can set only 1 parameter through execution
  struct Assignment {
    String alias;
    String key;
    Value value;
  };
  Assignment assignment;

  LogicalSet(const LogicalSet&) = delete;
  LogicalSet(LogicalSet&& other) = default;

  LogicalSet(LogicalOpPtr child, String alias, String key, Value value);
  String DebugString() const override;

  ~LogicalSet() = default;
};

struct LogicalDelete : LogicalOpUnaryChild {
  /// Create logical operation in a tree to delete a target
  std::vector<String> target;
  LogicalDelete(const LogicalDelete&) = delete;
  LogicalDelete(LogicalDelete&& other) = default;

  LogicalDelete(LogicalOpPtr child, std::vector<String> target);
  String DebugString() const override;

  ~LogicalDelete() override = default;
};

struct CreateNodeSpec {
  std::vector<String> labels;
  std::vector<std::pair<String, Value>> properties;

  String DebugString() const;
  CreateNodeSpec() = delete;
  CreateNodeSpec(const ast::NodePattern& pattern);
  CreateNodeSpec(ast::NodePattern&& pattern);
};

struct CreateEdgeSpec {
  String src_alias;
  String dst_alias;
  std::vector<String> labels;
  std::vector<std::pair<String, Value>> properties;
  frontend::EdgeDirection direction;
  String DebugString() const;
  CreateEdgeSpec() = delete;
  explicit CreateEdgeSpec(const ast::CreateEdgePattern& pattern);
  explicit CreateEdgeSpec(ast::CreateEdgePattern&& pattern);

};


struct LogicalCreate : LogicalOpUnaryChild {
  std::vector<std::variant<CreateNodeSpec, CreateEdgeSpec>> items;
  LogicalCreate() = delete;
  LogicalCreate(LogicalOpPtr child, const std::vector<ast::CreateItem>& items);
  String DebugString() const override;
};
}
}


namespace graph {
namespace exec {
using storage::Node;
using storage::Edge;
struct PhysicalOp;
struct RowCursor;

using RowSlot = std::variant<Node*, Edge*, Value>;
using PhysicalOpPtr = std::unique_ptr<PhysicalOp>;
using RowCursorPtr = std::unique_ptr<RowCursor>;

struct ExecOptions {
  /// options of execution; mymory, in future parallelism and spill
  size_t memory_budget_bytes = 256ULL * 1024 * 1024;
};
struct SlotMapping {
  std::unordered_map<std::string, size_t> alias_to_slot;
  size_t map(const std::string& key);
};

struct ExecContext {
  /// Context of execution db
  frontend::GraphDB* db;
  ExecOptions options;
  SlotMapping slots_mapping;
};

struct Row {
  /// store current row values and names
  std::vector<RowSlot> slots;
  std::vector<String> slots_names;
};
class RowCursor {
public:
  /// iterator by Row
  /// used to obtain next row(at the end need to be closed)
  RowCursor() = default;
  virtual bool next(Row& out) = 0;
  virtual void close() = 0;
  virtual ~RowCursor() = default;
};

struct PhysicalOp {
  virtual ~PhysicalOp() = default;
  /// open operator, return RowCursor
  /// tx may be required for storage access
  virtual RowCursorPtr open(ExecContext& ctx) = 0;
  virtual String DebugString() const = 0;
};

struct ScanNodeCursorPhysical : RowCursor {
  std::unique_ptr<storage::NodeCursor> nodes_cursor;
  size_t out_idx;

  ScanNodeCursorPhysical(const ScanNodeCursorPhysical&) = delete;
  ScanNodeCursorPhysical(ScanNodeCursorPhysical&&) = default;
  ScanNodeCursorPhysical(std::unique_ptr<storage::NodeCursor> nodes_cursor, size_t out_idx);
  bool next(Row& out) override;
  void close() override;

  ~ScanNodeCursorPhysical() override = default;
};

struct LabelIndexSeekOp : public PhysicalOp {
  /// used for node filtering(for more optimized by label we dont need to do all node scan)
  String label;
  String out_alias;

  LabelIndexSeekOp(String label,
                   String alias);
  RowCursorPtr open(ExecContext& ctx) override;
  String DebugString() const override;
  ~LabelIndexSeekOp() override = default;
};

struct NodeScanOp : public PhysicalOp {
  /// co complete Node scan and return Row to every Node
  String out_alias;
  RowCursorPtr open(ExecContext& ctx) override;
  String DebugString() const override;
  ~NodeScanOp() override = default;
};

template<bool edge_outgoing>
struct ExpandNodeCursorPhysical : RowCursor {
  enum class Direction { Outgoing, Ingoing };
  RowCursorPtr nodes_cursor;
  std::unique_ptr<storage::EdgeCursor> edge_cursor;
  std::function<bool(Edge*)> label_predicate;
  size_t src_idx;
  size_t out_idx;
  ExecContext ctx;

  ExpandNodeCursorPhysical(const ExpandNodeCursorPhysical&) = delete;
  ExpandNodeCursorPhysical(ExpandNodeCursorPhysical&&) = default;
  ExpandNodeCursorPhysical(RowCursorPtr nodes_cursor, size_t src_idx, size_t out_idx, std::function<bool(Edge*)> label_predicate, ExecContext &ctx);
  bool next(Row& out) override;
  void close() override;

  ~ExpandNodeCursorPhysical() override = default;
};

template<bool edge_outgoing>
struct ExpandOp : public PhysicalOp {
  /// write do dst_alias outgoing edge of edge_type
  String src_alias;
  String dst_alias;
  std::optional<String> edge_type;
  PhysicalOpPtr child;

  ExpandOp(String src_alias, String dst_alias, String edge_type, PhysicalOpPtr child);
  RowCursorPtr open(ExecContext& ctx) override;
  String DebugString() const override;
  ~ExpandOp() override = default;
};

using ExpandOutgoingOp = ExpandOp<true>;
using ExpandIngoingOp = ExpandOp<false>;

struct FilterCursor : RowCursor {
  RowCursorPtr nodes_cursor;
  size_t out_idx;
  ExecContext& ctx;
  std::unique_ptr<frontend::Expr> predicate;

  FilterCursor(const FilterCursor&) = delete;
  FilterCursor(FilterCursor&&) = default;
  FilterCursor(RowCursorPtr nodes_cursor, size_t out_idx, ExecContext& ctx, std::unique_ptr<frontend::Expr> predicate);
  bool next(Row& out) override;
  void close() override;

  ~FilterCursor() override = default;
};

struct FilterOp : public PhysicalOp {
  /// do Filter operation with predicate on Child like while (!predicate(child)) { child.next(row) }
  std::unique_ptr<frontend::Expr> predicate;
  String out_alias;
  PhysicalOpPtr child;

  FilterOp(std::unique_ptr<frontend::Expr> predicate, String out_alias, PhysicalOpPtr child);

  RowCursorPtr open(ExecContext& ctx) override;
  String DebugString() const override;
};

struct ProjectOp : public PhysicalOp {
  /// child is next operator in the tree
  std::vector<frontend::ReturnItem> items;
  PhysicalOpPtr child;

  ProjectOp() = default;
  ProjectOp(const std::vector<frontend::ReturnItem>& items,
            const PhysicalOpPtr& child);
  RowCursorPtr open(ExecContext& ctx) override;
  String DebugString() const override;
};

struct LimitOp : public PhysicalOp {
  /// just physycal limit
  Int limit_size;
  PhysicalOpPtr child;
  LimitOp(Int limit_size, PhysicalOpPtr child);
  RowCursorPtr open(ExecContext& ctx) override;
  String DebugString() const override;
};

struct NestedLoopJoinOp : public PhysicalOp {
  /// do join for 2 expressions based on predicate
  PhysicalOpPtr left;
  PhysicalOpPtr right;
  std::unique_ptr<frontend::Expr> predicate;
  bool left_outer = false; //
  NestedLoopJoinOp(std::unique_ptr<PhysicalOp> l, std::unique_ptr<PhysicalOp> r,
                   std::unique_ptr<frontend::Expr> pred, bool left_outer_);
  RowCursorPtr open(class ExecContext &ctx) override;
  std::string DebugString() const override;
};

struct HashJoinOp : public PhysicalOp {
  /// do hashJoin
  std::unique_ptr<PhysicalOp> build_side;
  std::unique_ptr<PhysicalOp> probe_side;
  std::vector<std::string> build_keys;
  std::vector<std::string> probe_keys;
  bool left_outer = false;
  HashJoinOp(std::unique_ptr<PhysicalOp> build, std::unique_ptr<PhysicalOp> probe,
             std::vector<std::string> bkeys, std::vector<std::string> pkeys, bool left_outer_);
  RowCursorPtr open(class ExecContext &ctx) override;
  std::string DebugString() const override;
};

struct PhysicalPlan {
  /// creates physical plan
  std::unique_ptr<PhysicalOp> root;
  PhysicalPlan() = default;
  explicit PhysicalPlan(std::unique_ptr<PhysicalOp> r);
  std::string DebugString() const;
};

using ResultCursor = RowCursor;

struct QueryResult {
  std::unique_ptr<ResultCursor> cursor;
};

// execute physycal plan
std::unique_ptr<ResultCursor> execute_plan(const exec::PhysicalPlan &plan,
                                           ExecContext &ctx);

// (Planner->Executor glue)
std::unique_ptr<ResultCursor> execute_query_ast(const frontend::QueryAST &ast,
                                                const frontend::GraphDB &cat,
                                                graph::exec::ExecOptions opts);
}
}

namespace graph {
namespace planner {

struct PlannerConfig {
  // Planner configuration options
  bool enable_hash_join = true;
  bool enable_index_seek = true;
  bool push_down_filters = true;
};

// Cost estimates structure
struct CostEstimate {
  double row_count = 0.0;
  double cpu_cost = 0.0;
  double io_cost = 0.0;
};

// Cost model interface
struct CostModel {
  virtual ~CostModel() = default;
  // estimate cost for scanning label with optional property predicate
  virtual CostEstimate estimate_scan(const storage::GraphDB &cat,
                                     const LogicalScan &scan) const = 0;

  // estimate for expand
  virtual CostEstimate estimate_expand(const storage::GraphDB &cat,
                                       const LogicalExpand &expand,
                                       const CostEstimate &input) const = 0;

  // estimate for join
  virtual CostEstimate estimate_join(const CostEstimate &left,
                                     const CostEstimate &right,
                                     const frontend::Expr *pred) const = 0;
};

// Default cost model
struct DefaultCostModel : public CostModel {
  DefaultCostModel();
  CostEstimate estimate_scan(const storage::GraphDB &cat,
                             const LogicalScan &scan) const override;
  CostEstimate estimate_expand(const storage::GraphDB &cat,
                               const LogicalExpand &expand,
                               const CostEstimate &input) const override;
  CostEstimate estimate_join(const CostEstimate &left,
                             const CostEstimate &right,
                             const frontend::Expr *pred) const override;
};

// Join ordering strategy interface
struct JoinOrderStrategy {
  virtual ~JoinOrderStrategy() = default;
  // choose join order given list of logical scans/joins
  virtual std::vector<size_t> choose_order(const std::vector<LogicalOp*> &join_roots,
                                           const storage::GraphDB &cat,
                                           const CostModel &cost_model) = 0;
};
class GreedyJoinOrder : public JoinOrderStrategy {
public:
  std::vector<size_t> choose_order(
    const std::vector<LogicalOp*>& joins,
    const storage::GraphDB& cat,
    const CostModel& cost_model) override {

    std::vector<size_t> order(joins.size());
    std::iota(order.begin(), order.end(), 0);
    return order;
  }
};

class Planner {
public:
  Planner(const storage::GraphDB &cat,
          PlannerConfig cfg = PlannerConfig(),
          std::unique_ptr<CostModel> cost_model = std::make_unique<DefaultCostModel>(),
          std::unique_ptr<JoinOrderStrategy> join_strategy = std::make_unique<GreedyJoinOrder>());
//              std::unique_ptr<JoinOrderStrategy> join_strategy = std::make_unique<DPJoinOrder>()); /// add later

  // End-to-end: AST -> LogicalPlan
  planner::LogicalPlan build_logical_plan(const frontend::QueryAST &ast) const;

  // Optimizations on logical plan (predicate pushdown, flattening)
  planner::LogicalPlan optimize_logical_plan(planner::LogicalPlan plan) const;

  // For each logical scan enumerate alternatives (index vs scan)
  // returns vector of alternative LogicalScan nodes (candidates)
  std::vector<planner::LogicalScan> enumerate_scan_alternatives(const planner::LogicalScan &scan) const;

  // chooses join order (returns permutation indices)
  std::vector<size_t> choose_join_order(const std::vector<planner::LogicalOp*> &joins) const;

  // Map logical -> physical (core)
  exec::PhysicalPlan map_logical_to_physical(const planner::LogicalPlan &lplan,
                                             const exec::ExecOptions &opts) const;

  // Final physical optimizations (push limits, top-k)
  exec::PhysicalPlan finalize_physical_plan(exec::PhysicalPlan pplan,
                                            const exec::ExecOptions &opts) const;

  // Full pipeline: AST -> PhysicalPlan
  exec::PhysicalPlan build_execution_plan(const frontend::QueryAST &ast,
                                          const exec::ExecOptions &opts) const;

  // Debug / explain helpers
  std::string explain_logical_plan(const planner::LogicalPlan &plan) const;
  std::string explain_physical_plan(const exec::PhysicalPlan &plan) const;

private:
  const storage::GraphDB &cat_;
  PlannerConfig cfg_;
  std::unique_ptr<CostModel> cost_model_;
  std::unique_ptr<JoinOrderStrategy> join_strategy_;

  // helper internal functions (signatures)
  void collect_aliases_from_match(const frontend::QueryAST &ast, std::set<std::string> &out) const;
  // converts pattern fragments into LogicalScan/Expand sequence
  LogicalOpPtr pattern_to_logical_ops(const frontend::MatchClause &match_clause) const;
};
}
}

namespace ast {
  struct EvalContext {
    graph::exec::ExecContext& exec_ctx;
    graph::exec::Row& row;
  };
}
