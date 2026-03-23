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
#include "tmp_folder/ast.hpp"
#include "tmp_folder/store.hpp"

namespace graph {
  using Int = int64_t;
  using Double = double;
  using String = std::string;
  using Bool = bool;
  using NodeId = uint64_t;
  using EdgeId = uint64_t;
  enum class EdgeDirection { Outgoing, Incoming, Undirected };

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
    String ConcatProperties(const std::vector<std::pair<const String, Value>>& v);
    String EdgeStrByDirection(EdgeDirection dir);
  }
}

namespace graph {
  namespace frontend {
    using ast::Expr;
    using ast::ReturnItem;
    using ast::QueryAST;
    using ast::MatchClause;
    using store::GraphDB;
    using ast::Pattern;
    using ast::PatternElement;
    using ast::NodePattern;
  }
};

namespace graph {
  namespace planner {
    struct LogicalOp;
    using LogicalOpPtr = std::unique_ptr<LogicalOp>;

    struct LogicalOp {
      virtual ~LogicalOp() = default;
      virtual String DebugString() const  = 0;
    };
    struct LogicalScan : public LogicalOp {
      /// Scan Nodes that has specified labels(and fit by property filters \
      /// and write out in row with slot name alias
      std::vector<String> labels;
      String alias;
      std::vector<std::pair<const String, Value> > property_filters;

      LogicalScan() = delete;
      LogicalScan(std::vector<String> labels, String  alias,
                  std::vector<std::pair<const String, Value>> property_filters);
      String DebugString() const override;
      ~LogicalScan() override = default;
    };
    struct LogicalExpand : public LogicalOp {
      /// Expand Nodes that are located in row by $src_alias slot name and move to $dst_alias
      String src_alias;
      String dst_alias;
      /// bool optional; can add for Optional match
      std::optional<std::vector<String>> edge_labels;
      EdgeDirection direction;

      LogicalExpand() = delete;
      LogicalExpand(String src_alias, String dst_alias, EdgeDirection direction = EdgeDirection::Outgoing);
      LogicalExpand(String src_alias, String dst_alias, std::vector<String> edge_labels, EdgeDirection direction = EdgeDirection::Outgoing);
      String DebugString() const override;
      ~LogicalExpand() override = default;
    };

    struct LogicalFilter : public LogicalOp {
      /// Filter - not include int answer all values that do not satisfy predicate
      std::unique_ptr<frontend::Expr> predicate;
      LogicalFilter() = delete;
      explicit LogicalFilter(std::unique_ptr<frontend::Expr> predicate): predicate(std::move(predicate)) {}
      String DebugString() const override;
      ~LogicalFilter() override = default;
    };

    struct LogicalProject : public LogicalOp {
      /// Project of values to items(can be field - string or Expression)
      std::vector<frontend::ReturnItem> items;
      LogicalProject() = delete;
      LogicalProject(std::vector<frontend::ReturnItem> items): items(std::move(items)) {}
      String DebugString() const override;
      ~LogicalProject() override = default;
    };

    struct LogicalLimit : public LogicalOp {
      /// Logical Limit - nothing to comment
      Int limit_size;
      LogicalLimit() = delete;
      explicit LogicalLimit(size_t limit_size): limit_size(limit_size) {}
      String DebugString() const override;
      ~LogicalLimit() override = default;
    };

    struct LogicalSort : LogicalOp {
      /// Logical Sort - sort by expression in keys
      /// for example
      /*
        {
          {expr1, true},   // ASC
          {expr2, false}   // DESC
        } and sort if by expr1 and if they are equal by expr2
       */
      std::vector<std::pair<std::unique_ptr<frontend::Expr>, bool> > keys;
      LogicalSort() = delete;
      explicit LogicalSort(std::vector<std::pair<std::unique_ptr<frontend::Expr>, bool> > keys): keys(std::move(keys)) {}
      String DebugString() const override;
      ~LogicalSort() override = default;
    };

    struct LogicalJoin : LogicalOp {
      /// Logical Joint for 2 results with predicate(optional)
      LogicalOpPtr left;
      LogicalOpPtr right;
      std::optional<std::unique_ptr<frontend::Expr>> predicate;
      LogicalJoin() = delete;
      LogicalJoin(LogicalOpPtr left, LogicalOpPtr right):
        left(std::move(left)), right(std::move(right)) {}
      LogicalJoin(LogicalOpPtr left, LogicalOpPtr right, std::unique_ptr<frontend::Expr> predicate) :
        left(std::move(left)), right(std::move(right)), predicate(std::move(predicate)) {}
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
      ~LogicalPlan() override = default;
    };
  };
};


namespace graph {
  namespace exec {
    using store::Node;
    using store::Edge;
    struct PhysicalOp;

    using RowSlot = std::variant<Node*, Edge*, Value>;
    using PhysicalOpPtr = std::unique_ptr<PhysicalOp>;


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
      SlotMapping slots;
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
      virtual std::unique_ptr<RowCursor> open(ExecContext& ctx) = 0;
      virtual String DebugString() const = 0;
    };

    struct LabelIndexSeekOp : public PhysicalOp {
      /// used for node filtering(for more optimized by label we dont need to do all node scan)
      String label;
      std::vector<String> property_keys; /// vector or just value, discuss with team
      std::vector<String> property_vals;
      String out_alias;
      LabelIndexSeekOp(const String& lbl, const String& key, const Value& v, const String& alias);
      LabelIndexSeekOp(const std::vector<String>& labels, const std::vector<String>& keys,
                       const std::vector<Value>& vals, const String& alias);
      std::unique_ptr<RowCursor> open(ExecContext& ctx) override;
      String DebugString() const override;
      ~LabelIndexSeekOp() override;
    };

    struct NodeScanOp : public PhysicalOp {
      /// co complete Node scan and return Row to every Node
      String out_alias;
      std::unique_ptr<RowCursor> open(ExecContext& ctx) override;
      String DebugString() const override;
      ~NodeScanOp() override;
    };

    struct ExpandOutgoingOp : public PhysicalOp {
      /// write do dst_alias outgoing edge of edge_type
      String src_alias;
      String dst_alias;
      std::optional<String> edge_type;

      std::unique_ptr<RowCursor> open(ExecContext& ctx) override;
      String DebugString() const override;
    };

    struct ExpandIngoingOp : public PhysicalOp {
      /// write do dst_alias ingoing edge of edge_type
      String src_alias;
      String dst_alias;
      std::optional<String> edge_type;

      std::unique_ptr<RowCursor> open(ExecContext& ctx) override;
      String DebugString() const override;
    };

    struct FilterOp : public PhysicalOp { /// add cursor?
      /// do Filter operation with predicate on Child like while (!predicate(child)) { child.next(row) }
      std::unique_ptr<frontend::Expr> predicate;
      PhysicalOpPtr child;
      FilterOp(std::unique_ptr<frontend::Expr> pred, PhysicalOpPtr ch);

      std::unique_ptr<RowCursor> open(ExecContext& ctx) override;
      String DebugString() const override;
    };

    struct ProjectOp : public PhysicalOp {
      /// child is next operator in the tree
      std::vector<frontend::ReturnItem> items;
      PhysicalOpPtr child;

      ProjectOp() = default;
      ProjectOp(const std::vector<frontend::ReturnItem>& items,
                const PhysicalOpPtr& child);
      std::unique_ptr<RowCursor> open(ExecContext& ctx) override;
      String DebugString() const override;
    };

    struct LimitOp : public PhysicalOp {
      /// just physycal limit
      Int limit_size;
      PhysicalOpPtr child;
      LimitOp(Int limit_size, PhysicalOpPtr child);
      std::unique_ptr<RowCursor> open(ExecContext& ctx) override;
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
      std::unique_ptr<RowCursor> open(class ExecContext &ctx) override;
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
      std::unique_ptr<RowCursor> open(class ExecContext &ctx) override;
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
      virtual CostEstimate estimate_scan(const store::GraphDB &cat,
                                         const LogicalScan &scan) const = 0;

      // estimate for expand
      virtual CostEstimate estimate_expand(const store::GraphDB &cat,
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
      CostEstimate estimate_scan(const store::GraphDB &cat,
                                 const LogicalScan &scan) const override;
      CostEstimate estimate_expand(const store::GraphDB &cat,
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
                                               const store::GraphDB &cat,
                                               const CostModel &cost_model) = 0;
    };
    class GreedyJoinOrder : public JoinOrderStrategy {
    public:
      std::vector<size_t> choose_order(
        const std::vector<LogicalOp*>& joins,
        const store::GraphDB& cat,
        const CostModel& cost_model) override {

        std::vector<size_t> order(joins.size());
        std::iota(order.begin(), order.end(), 0);
        return order;
      }
    };

    class Planner {
    public:
      Planner(const store::GraphDB &cat,
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
      const store::GraphDB &cat_;
      PlannerConfig cfg_;
      std::unique_ptr<CostModel> cost_model_;
      std::unique_ptr<JoinOrderStrategy> join_strategy_;

      // helper internal functions (signatures)
      void collect_aliases_from_match(const frontend::QueryAST &ast, std::set<std::string> &out) const;
      // converts pattern fragments into LogicalScan/Expand sequence
      LogicalOpPtr pattern_to_logical_ops(const frontend::MatchClause &match_clause) const;
    };
  }
};
