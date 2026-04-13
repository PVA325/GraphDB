#pragma once
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include <set>
#include <unordered_map>
#include <numeric>
#include <functional>
#include "ast.hpp"
#include "storage.hpp"

namespace graph {
using Int = int64_t;
using Double = double;
using String = std::string;
using Bool = bool;

using Value = std::variant<Int, Double, String, Bool>;


struct PlannerError : public std::runtime_error { // todo at the end
  explicit PlannerError(const std::string &msg);
};

struct ExecutionError : public std::runtime_error { // todo at the end
  explicit ExecutionError(const std::string &msg);
};


namespace PlannerUtils {
  template<typename... Types>
  struct overloads : Types... {
    using Types::operator()...;
  };
  String toString(const Value& val);
  String ConcatStrVector(const std::vector<String>& v);
  String ConcatProperties(const std::vector<std::pair<String, Value>>& v);
  String EdgeStrByDirection(ast::EdgeDirection dir);
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

namespace graph::exec {
  using storage::Node;
  using storage::Edge;
  struct PhysicalOp;
  struct RowCursor;
  struct Row;
  struct ExecContext;

  using RowSlot = std::variant<Node *, Edge *, Value>;
  using PhysicalOpPtr = std::unique_ptr<PhysicalOp>;
  using RowCursorPtr = std::unique_ptr<RowCursor>;

  struct LabelIndexSeekOp;
  struct NodeScanOp;
  template<bool edge_outgoing>
  struct ExpandOp;
  using ExpandOutgoingOp = ExpandOp<true>;
  using ExpandIngoingOp = ExpandOp<false>;

  struct NestedLoopJoinOp;
  struct KeyHashJoinOp;
  struct PhysicalSetOp;
  struct PhysicalCreateOp;
  struct PhysicalDeleteOp;
}

namespace graph::logical {
  struct LogicalOp;
  using LogicalOpPtr = std::unique_ptr<LogicalOp>;

  struct LogicalOp {
    [[nodiscard]] virtual String DebugString() const = 0;
    virtual exec::PhysicalOpPtr BuildPhysical(exec::ExecContext& ctx) const = 0;

    [[nodiscard]] virtual String SubtreeDebugString() const = 0;
    virtual ~LogicalOp() = default;
  };

  struct LogicalOpUnaryChild : LogicalOp {
    LogicalOpPtr child;

    explicit LogicalOpUnaryChild(LogicalOpPtr child);

    [[nodiscard]] String SubtreeDebugString() const override;
    ~LogicalOpUnaryChild() override = default;
  };

  struct LogicalOpBinaryChild : LogicalOp {
    LogicalOpPtr left;
    LogicalOpPtr right;

    LogicalOpBinaryChild(LogicalOpPtr left, LogicalOpPtr right);

    [[nodiscard]] String SubtreeDebugString() const override;

    ~LogicalOpBinaryChild() override = default;
  };
  struct LogicalOpZeroChild : LogicalOp {
    LogicalOpZeroChild() = default;

    [[nodiscard]] String SubtreeDebugString() const override { return DebugString(); }

    ~LogicalOpZeroChild() override = default;
  };

  struct AliasedLogicalOp : public LogicalOpZeroChild {
    std::string dst_alias;

    AliasedLogicalOp(std::string dst_alias) : dst_alias(std::move(dst_alias)) {}
  };

  struct LogicalScan : public AliasedLogicalOp {
    /// Scan Nodes that has specified labels
    /// and write out in row with slot name alias
    std::vector<String> labels;
    std::vector<std::pair<String, Value> > property_filters;

    LogicalScan(std::vector<String> labels, String dst_alias);

    LogicalScan(std::vector<String> labels, String alias, std::vector<std::pair<String, Value> > property_filters);

    exec::PhysicalOpPtr BuildPhysical(exec::ExecContext& ctx) const override;
    [[nodiscard]] String DebugString() const override;

    ~LogicalScan() override = default;
  };

  struct LogicalExpand : public AliasedLogicalOp {
    /// Expand Nodes that are located in row by $src_alias slot name and move to $dst_alias
    LogicalOpPtr child;

    String src_alias;
    String edge_alias;

    std::optional<String> edge_type;
    std::vector<String> dst_vertex_labels;
    std::vector<std::pair<String, Value>> dst_vertex_properties;
    ast::EdgeDirection direction;

    LogicalExpand() = delete;

    LogicalExpand(LogicalOpPtr child, String src_alias, String edge_alias, String dst_alias,
                  std::optional<String> edge_type, std::vector<String> dst_vertex_labels,
                  std::vector<std::pair<String, Value>> dst_vertex_properties,
                  ast::EdgeDirection direction);

    exec::PhysicalOpPtr BuildPhysical(exec::ExecContext& ctx) const override;
    [[nodiscard]] String DebugString() const override;

    ~LogicalExpand() override = default;
  };

  struct LogicalFilter : public LogicalOpUnaryChild {
    /// Filter - not include int answer all values that do not satisfy predicate
    std::unique_ptr<ast::Expr> predicate;

    LogicalFilter() = delete;

    explicit LogicalFilter(LogicalOpPtr child, std::unique_ptr<ast::Expr> predicate);

    [[nodiscard]] String DebugString() const override;
    exec::PhysicalOpPtr BuildPhysical(exec::ExecContext& ctx) const override;

    ~LogicalFilter() override = default;
  };

  struct LogicalProject : public LogicalOpUnaryChild {
    /// Project of values to items(can be field - string or Expression)
    std::vector<ast::ReturnItem> items;

    LogicalProject() = delete;

    LogicalProject(LogicalOpPtr child, std::vector<ast::ReturnItem> items);

    [[nodiscard]] String DebugString() const override;
    exec::PhysicalOpPtr BuildPhysical(exec::ExecContext& ctx) const override;

    ~LogicalProject() override = default;
  };

  struct LogicalLimit : public LogicalOpUnaryChild {
    /// Logical Limit - nothing to comment
    size_t limit_size;

    LogicalLimit() = delete;

    explicit LogicalLimit(LogicalOpPtr child, size_t limit_size);

    [[nodiscard]] String DebugString() const override;
    exec::PhysicalOpPtr BuildPhysical(exec::ExecContext& ctx) const override;

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
    std::vector<ast::OrderItem> keys;

    LogicalSort() = delete;

    explicit LogicalSort(LogicalOpPtr child, std::vector<ast::OrderItem> keys);

    [[nodiscard]] String DebugString() const override;

    ~LogicalSort() override = default;
  };

  struct LogicalJoin : LogicalOpBinaryChild {
    /// Logical Joint for 2 results with predicate(optional)

    std::unique_ptr<ast::Expr> predicate;

    LogicalJoin() = delete;

    LogicalJoin(LogicalOpPtr left, LogicalOpPtr right);

    LogicalJoin(LogicalOpPtr left, LogicalOpPtr right, std::unique_ptr<ast::Expr> predicate);
    exec::PhysicalOpPtr BuildPhysical(exec::ExecContext& ctx) const override;

    [[nodiscard]] String DebugString() const override;

    ~LogicalJoin() override = default;
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
    LogicalSet(LogicalSet &&other) noexcept : LogicalOpUnaryChild(std::move(other.child)), assignment(std::move(other.assignment)) {}

    LogicalSet(LogicalOpPtr child, String alias, String key, Value value);
    exec::PhysicalOpPtr BuildPhysical(exec::ExecContext& ctx) const override;

    [[nodiscard]] String DebugString() const override;

    ~LogicalSet() override = default;
  };

  struct LogicalDelete : LogicalOpUnaryChild {
    /// Create logical operation in a tree to delete a target
    std::vector<String> target;

    LogicalDelete(const LogicalDelete &) = delete;

    LogicalDelete(LogicalDelete &&other) noexcept : LogicalOpUnaryChild(std::move(other.child)), target(std::move(other.target)) {}

    LogicalDelete(LogicalOpPtr child, std::vector<String> target);

    exec::PhysicalOpPtr BuildPhysical(exec::ExecContext& ctx) const override;

    [[nodiscard]] String DebugString() const override;

    ~LogicalDelete() override = default;
  };

  struct CreateNodeSpec {
    std::vector<String> labels;
    std::vector<std::pair<String, Value>> properties;

    [[nodiscard]] String DebugString() const;

    CreateNodeSpec() = delete;

    explicit CreateNodeSpec(const ast::NodePattern &pattern);

    explicit CreateNodeSpec(ast::NodePattern &&pattern);
  };

  struct CreateEdgeSpec {
    String src_alias;
    String dst_alias;
    String edge_type;
    std::vector<std::pair<String, Value>> properties;
    ast::EdgeDirection direction;

    [[nodiscard]] String DebugString() const;

    CreateEdgeSpec() = delete;

    explicit CreateEdgeSpec(const ast::CreateEdgePattern &pattern);

    explicit CreateEdgeSpec(ast::CreateEdgePattern &&pattern);

  };

  struct LogicalCreate : LogicalOpUnaryChild {
    std::vector<std::variant<CreateNodeSpec, CreateEdgeSpec>> items;

    LogicalCreate() = delete;

    LogicalCreate(LogicalOpPtr child, const std::vector<ast::CreateItem> &items);

    exec::PhysicalOpPtr BuildPhysical(exec::ExecContext& ctx) const override;

    [[nodiscard]] String DebugString() const override;
  };

  struct LogicalPlan : LogicalOpZeroChild {
    /// Container logical plan
    LogicalOpPtr root;

    LogicalPlan() : root(nullptr) {}
    LogicalPlan(LogicalPlan &&other) noexcept : root(std::move(other.root)) {}
    LogicalPlan& operator=(LogicalPlan &&other) noexcept {
      root = std::move(other.root);
      return *this;
    }
    explicit LogicalPlan(LogicalOpPtr r) : root(std::move(r)) {}

    [[nodiscard]] String DebugString() const override { return "LogicalPlan:"; }
    exec::PhysicalOpPtr BuildPhysical(exec::ExecContext& ctx) const override { return root->BuildPhysical(ctx); }

    ~LogicalPlan() override = default;
  };

}


namespace graph::exec {
  /// Operations need to live longer than cursors
  struct ExecOptions {
    /// options of execution; memory, in future parallelism and spill
    size_t memory_budget_bytes = 256ULL * 1024 * 1024;
  };

  struct SlotMapping {
    bool key_exists(const std::string &key) const;

    size_t map(const std::string &key) const;
    size_t map_and_check(const String& key, const String& err_msg = "") const;

    void add_map(const String &key, size_t idx);
  private:
    std::unordered_map<std::string, size_t> alias_to_slot;

    friend Row MergeRows(Row &first, Row &second);
  };

  struct ExecContext {
    /// Context of execution db
    storage::GraphDB *db;
    ExecOptions options;
    ExecContext(): db{nullptr} {}
  };

  struct Row {
    /// store current row values and names
    std::vector<RowSlot> slots;
    std::vector<String> slots_names;
    SlotMapping slots_mapping;

    template<typename T>
    requires std::is_constructible_v<RowSlot, T>
    void AddValue(const T &val, const String& alias, const String& error_msg = "") {
      if (slots_mapping.key_exists(alias)) {
        throw std::runtime_error(error_msg);
      }
      slots.emplace_back(val);
      slots_names.emplace_back(alias);
      slots_mapping.add_map(alias, slots.size() - 1);
    }
  };

  struct RowCursor {
    /// iterator by Row
    /// used to obtain next row(at the end need to be closed)
    RowCursor() = default;

    virtual bool next(Row &out) = 0;

    virtual void close() = 0;

    virtual ~RowCursor() = default;
  };

  struct PhysicalOp {
    virtual ~PhysicalOp() = default;

    /// open operator, return RowCursor
    /// tx may be required for storage access
    virtual RowCursorPtr open(ExecContext &ctx) = 0;

    [[nodiscard]] virtual String DebugString() const = 0;
    [[nodiscard]] virtual String SubtreeDebugString() const = 0;
  };
  struct PhysicalOpNoChild : PhysicalOp {
    PhysicalOpNoChild() = default;

    [[nodiscard]] String SubtreeDebugString() const override { return DebugString(); }

    ~PhysicalOpNoChild() override = default;
  };

  struct PhysicalOpUnaryChild : PhysicalOp {
    PhysicalOpPtr child;
    PhysicalOpUnaryChild(PhysicalOpPtr child): child(std::move(child)) {}

    [[nodiscard]] String SubtreeDebugString() const override;

    ~PhysicalOpUnaryChild() override = default;
  };

  struct PhysicalOpBinaryChild : PhysicalOp {
    PhysicalOpPtr left;
    PhysicalOpPtr right;
    PhysicalOpBinaryChild(PhysicalOpPtr left, PhysicalOpPtr right): left(std::move(left)), right(std::move(right)) {}

    [[nodiscard]] String SubtreeDebugString() const override;

    ~PhysicalOpBinaryChild() override = default;
  };

  struct NodeScanCursor : RowCursor {
    std::unique_ptr<storage::NodeCursor> nodes_cursor;
    String out_alias;

    NodeScanCursor(NodeScanCursor &&) = default;

    NodeScanCursor(std::unique_ptr<storage::NodeCursor> nodes_cursor, String out_alias);

    bool next(Row &out) override;

    void close() override;

    ~NodeScanCursor() override = default;
  };

  struct LabelIndexSeekOp : public PhysicalOpNoChild {
    /// used for node filtering(for more optimized by label we dont need to do all node scan)
    String label;
    String out_alias;

    LabelIndexSeekOp(String label,
                     String alias);

    RowCursorPtr open(ExecContext &ctx) override;

    [[nodiscard]] String DebugString() const override;

    ~LabelIndexSeekOp() override = default;
  };

  struct NodeScanOp : public PhysicalOpNoChild {
    /// co complete Node scan and return Row to every Node
    String out_alias;

    NodeScanOp(String out_alias): out_alias(std::move(out_alias)) {}
    RowCursorPtr open(ExecContext &ctx) override;

    [[nodiscard]] String DebugString() const override;

    ~NodeScanOp() override = default;
  };

  struct NodePropertyFilterCursor : RowCursor {
    RowCursorPtr child_cursor;
    String alias;
    std::vector<String> labels;
    std::vector<std::pair<String, Value>> properties;

    NodePropertyFilterCursor(RowCursorPtr child_cursor, String alias, std::vector<String> labels, std::vector<std::pair<String, Value>> properties);
    bool next(Row &out) override;

    void close() override;

    ~NodePropertyFilterCursor() override = default;
  };

  struct NodePropertyFilterOp : PhysicalOpUnaryChild {
    String alias;
    std::vector<String> labels;
    std::vector<std::pair<String, Value>> properties;

    NodePropertyFilterOp(PhysicalOpPtr child, String alias, std::vector<String> labels, std::vector<std::pair<String, Value>> properties);
    RowCursorPtr open(ExecContext &ctx) override;

    [[nodiscard]] String DebugString() const override;

    ~NodePropertyFilterOp() override = default;
  };

  template<bool edge_outgoing>
  struct ExpandNodeCursorPhysical : RowCursor {
    enum class Direction {
      Outgoing, Ingoing
    };
    RowCursorPtr child_cursor;
    std::unique_ptr<storage::EdgeCursor> edge_cursor;
    std::function<bool(Edge *)> label_predicate;
    String src_alias;
    String dst_edge_alias;
    String dst_node_alias;
    storage::GraphDB *db;

    ExpandNodeCursorPhysical(const ExpandNodeCursorPhysical &) = delete;

    ExpandNodeCursorPhysical(ExpandNodeCursorPhysical &&) = default;

    ExpandNodeCursorPhysical(RowCursorPtr child_cursor, String src_alias, String dst_edge_alias,
                             String dst_node_alias, std::function<bool(Edge *)> label_predicate,
                             storage::GraphDB *db);

    bool next(Row &out) override;

    void close() override;

    ~ExpandNodeCursorPhysical() override = default;
  };


  template<bool edge_outgoing>
  struct ExpandOp : public PhysicalOpUnaryChild {
    /// write do dst_alias outgoing edge of edge_type
    String src_alias;
    String dst_edge_alias;
    String dst_node_alias;
    std::optional<String> edge_type;

    ExpandOp(String src_alias, String dst_edge_alias, String dst_node_alias, std::optional<String> edge_type, PhysicalOpPtr child);

    RowCursorPtr open(ExecContext &ctx) override;

    [[nodiscard]] String DebugString() const override;

    ~ExpandOp() override = default;
  };

  struct FilterCursor : RowCursor {
    RowCursorPtr child_cursor;
    ast::Expr *predicate;

    FilterCursor(const FilterCursor &) = delete;

    FilterCursor(FilterCursor &&) = default;

    FilterCursor(RowCursorPtr child_cursor, ast::Expr *predicate);

    bool next(Row &out) override;

    void close() override;

    ~FilterCursor() override = default;
  };

  struct FilterOp : public PhysicalOpUnaryChild {
    /// do Filter operation with predicate on Child like while (!predicate(child)) { child.next(row) }
    ast::Expr* predicate;

    FilterOp(ast::Expr* predicate, PhysicalOpPtr child);

    RowCursorPtr open(ExecContext &ctx) override;

    [[nodiscard]] String DebugString() const override;

    ~FilterOp() override = default;
  private:
    String debugString_;
  };

  struct ProjectCursor : RowCursor {
    RowCursorPtr child_cursor;
    std::vector<ast::ReturnItem> items;

    ProjectCursor(const ProjectCursor &) = delete;

    ProjectCursor(ProjectCursor &&) = default;

    ProjectCursor(RowCursorPtr child_cursor, std::vector<ast::ReturnItem> items);

    bool next(Row &out) override;

    void close() override;

    ~ProjectCursor() override = default;
  };

  struct ProjectOp : public PhysicalOpUnaryChild {
    /// child is next operator in the tree
    std::vector<ast::ReturnItem> items;

    ProjectOp(std::vector<ast::ReturnItem> items,
              PhysicalOpPtr child);

    RowCursorPtr open(ExecContext &ctx) override;

    [[nodiscard]] String DebugString() const override;

    ~ProjectOp() override = default;
  };

  struct LimitCursor : RowCursor {
    RowCursorPtr child_cursor;
    size_t limit_count;
    size_t used_count{0};

    LimitCursor(const LimitCursor &) = delete;

    LimitCursor(LimitCursor &&) = default;

    LimitCursor(RowCursorPtr child_cursor, size_t limit_count);

    bool next(Row &out) override;

    void close() override;

    ~LimitCursor() override = default;
  };

  struct LimitOp : public PhysicalOpUnaryChild {
    /// just physical limit
    Int limit_size;

    LimitOp(Int limit_size, PhysicalOpPtr child);

    RowCursorPtr open(ExecContext &ctx) override;

    [[nodiscard]] String DebugString() const override;
    ~LimitOp() override = default;
  };

  Row MergeRows(Row& first, Row& second);

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

    bool next(Row &out) override;

    void close() override;

    ~NestedLoopJoinCursor() override = default;
  };

  struct NestedLoopJoinOp : public PhysicalOpBinaryChild {
    /// do join for 2 expressions based on predicate
    ast::Expr* predicate;

    NestedLoopJoinOp(PhysicalOpPtr left, PhysicalOpPtr right,
                     ast::Expr* pred);
    NestedLoopJoinOp(PhysicalOpPtr left, PhysicalOpPtr right);

    RowCursorPtr open(ExecContext &ctx) override;

    [[nodiscard]] String DebugString() const override;
    ~NestedLoopJoinOp() override = default;
  };

  struct ValueHash {
    size_t operator()(const Value& k) const {
      const auto& visitor = PlannerUtils::overloads { // can make static in struct
        [](Int cur) { return std::hash<Int>()(cur); },
        [](const String& cur) { return std::hash<String>()(cur); },
        [](double cur) { return std::hash<double>()(cur); },
        [](bool cur) { return std::hash<bool>()(cur); }
      };
      return std::visit(visitor, k);
    }
  };

  Value GetFeatureFromSlot(const RowSlot& slot, const String& feature_key);
  struct KeyHashJoinCursor : public RowCursor {
    /// do join for 2 NestedLoopJoin Cursor expressions based on predicate

    RowCursorPtr left_cursor;
    RowCursorPtr right_cursor;
    String left_alias;
    String right_alias;
    String left_feature_key;
    String right_feature_key;

    std::unordered_map<Value, std::vector<Row>, ValueHash> left_rows;
    Row last_right_row;

    std::unordered_map<Value, std::vector<Row>, ValueHash>::iterator it_left;
    size_t vec_left_idx{std::numeric_limits<size_t>::max()};

    KeyHashJoinCursor(RowCursorPtr left_cursor, RowCursorPtr right_cursor,
                      String left_alias, String right_alias,
                      String left_feature_key, String right_feature_key);

    bool next(Row &out) override;

    void close() override;

    ~KeyHashJoinCursor() override = default;
  };

  struct KeyHashJoinOp : public PhysicalOpBinaryChild {
    /// do hashJoin
    String left_alias;
    String right_alias;
    String left_feature_key;
    String right_feature_key;

    KeyHashJoinOp(PhysicalOpPtr left, PhysicalOpPtr right,
                  String left_alias, String right_alias,
                  String left_feature_key, String right_feature_key);

    RowCursorPtr open(ExecContext &ctx) override;

    [[nodiscard]] String DebugString() const override;
    ~KeyHashJoinOp() override = default;
  };

  struct SetCursor : RowCursor {
    RowCursorPtr child;
    std::vector<String> aliases;
    std::vector<std::vector<String> > labels;
    std::vector<std::vector<std::pair<String, Value> > > properties;

    SetCursor(RowCursorPtr child,
              std::vector<String> aliases,
              std::vector<std::vector<String> > labels,
              std::vector<std::vector<std::pair<String, Value> > > properties);
    bool next(Row &out) override;
    void close() override;
  };

  struct PhysicalSetOp : public PhysicalOpUnaryChild {
    PhysicalOpPtr child;
    std::vector<String> aliases;
    std::vector<std::vector<String> > labels;
    std::vector<std::vector<std::pair<String, Value> > > properties;

    PhysicalSetOp(std::vector<String> aliases,
                  std::vector<std::vector<String> > labels,
                  std::vector<std::vector<std::pair<String, Value> > > properties,
                  PhysicalOpPtr child);

    RowCursorPtr open(ExecContext &ctx) override;

    [[nodiscard]] String DebugString() const override;

    ~PhysicalSetOp() override = default;
  };

  struct CreateCursor : public RowCursor {
    RowCursorPtr child;
    std::vector<std::variant<logical::CreateNodeSpec, logical::CreateEdgeSpec>> items;
    storage::GraphDB* db;

    CreateCursor(RowCursorPtr child,
                 std::vector<std::variant<logical::CreateNodeSpec, logical::CreateEdgeSpec>> items,
                 storage::GraphDB* db);
    bool next(Row &out) override;
    void close() override;
  };
  struct PhysicalCreateOp : PhysicalOpUnaryChild {
    std::vector<std::variant<logical::CreateNodeSpec, logical::CreateEdgeSpec>> items;

    PhysicalCreateOp(std::vector<std::variant<logical::CreateNodeSpec, logical::CreateEdgeSpec>> items);
    PhysicalCreateOp(std::vector<std::variant<logical::CreateNodeSpec, logical::CreateEdgeSpec>> items,
                     PhysicalOpPtr child);
    RowCursorPtr open(ExecContext& ctx) override;

    [[nodiscard]] String DebugString() const override;

    ~PhysicalCreateOp() override = default;
  };

  struct DeleteCursor : RowCursor {
    RowCursorPtr child;
    std::vector<String> aliases;
    storage::GraphDB* db;

    DeleteCursor(RowCursorPtr child, std::vector<String> aliases, storage::GraphDB* db);
    bool next(Row &out) override;
    void close() override;

    ~DeleteCursor() override = default;
  };
  struct PhysicalDeleteOp : PhysicalOpUnaryChild {
    std::vector<String> aliases;

    PhysicalDeleteOp(std::vector<String> aliases, PhysicalOpPtr child);

    RowCursorPtr open(ExecContext &ctx) override;
    [[nodiscard]] String DebugString() const override;

    ~PhysicalDeleteOp() override = default;
  };

  struct PhysicalSort {}; /// todo

  struct PhysicalPlan {
    /// creates physical plan
    PhysicalOpPtr root;

    PhysicalPlan() = default;

    PhysicalPlan(PhysicalPlan&& other): root(std::move(other.root)) {}
    PhysicalPlan& operator=(PhysicalPlan&& other) noexcept {
      root = std::move(other.root);
      return *this;
    }
    explicit PhysicalPlan(PhysicalOpPtr root);

    [[nodiscard]] std::string DebugString() const;
  };

  using ResultCursor = RowCursor;

  struct QueryResult {
    std::unique_ptr<ResultCursor> cursor;
  };

// execute physical plan
  std::unique_ptr<ResultCursor> execute_plan(const exec::PhysicalPlan &plan,
                                             ExecContext &ctx);

// (Planner->Executor glue)
  std::unique_ptr<ResultCursor> execute_query_ast(ast::QueryAST ast,
                                                  const storage::GraphDB &cat,
                                                  graph::exec::ExecOptions opts);

  PhysicalPlan build_physical_plan(ast::QueryAST ast);
}



namespace graph::planner {

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
  [[nodiscard]] virtual CostEstimate estimate_scan(const storage::GraphDB &cat,
                                     const logical::LogicalScan &scan) const = 0;

  // estimate for expand
  [[nodiscard]] virtual CostEstimate estimate_expand(const storage::GraphDB &cat,
                                       const logical::LogicalExpand &expand,
                                       const CostEstimate &input) const = 0;

  // estimate for join
  virtual CostEstimate estimate_join(const CostEstimate &left,
                                     const CostEstimate &right,
                                     const ast::Expr *pred) const = 0;
};

// Default cost model
struct DefaultCostModel : public CostModel {
  DefaultCostModel();
  [[nodiscard]] CostEstimate estimate_scan(const storage::GraphDB &cat,
                             const logical::LogicalScan &scan) const override;
  [[nodiscard]] CostEstimate estimate_expand(const storage::GraphDB &cat,
                               const logical::LogicalExpand &expand,
                               const CostEstimate &input) const override;
  CostEstimate estimate_join(const CostEstimate &left,
                             const CostEstimate &right,
                             const ast::Expr *pred) const override;
};

// Join ordering strategy interface
struct JoinOrderStrategy {
  virtual ~JoinOrderStrategy() = default;
  // choose join order given list of logical scans/joins
  virtual std::vector<size_t> choose_order(const std::vector<logical::LogicalOp*> &join_roots,
                                           const storage::GraphDB &cat,
                                           const CostModel &cost_model) = 0;
};
class GreedyJoinOrder : public JoinOrderStrategy {
public:
  std::vector<size_t> choose_order(
    const std::vector<logical::LogicalOp*>& joins,
    const storage::GraphDB& cat,
    const CostModel& cost_model) override {

    std::vector<size_t> order(joins.size());
    std::iota(order.begin(), order.end(), 0);
    return order;
  }
};

class Planner {
public:
  explicit Planner(exec::ExecContext &ctx,
                   ast::QueryAST ast,
                   // PlannerConfig cfg = PlannerConfig(),
                   std::unique_ptr<CostModel> cost_model = std::make_unique<DefaultCostModel>(),
                   std::unique_ptr<JoinOrderStrategy> join_strategy = std::make_unique<GreedyJoinOrder>());
//              std::unique_ptr<JoinOrderStrategy> join_strategy = std::make_unique<DPJoinOrder>()); /// add later

  // End-to-end: AST -> LogicalPlan
  void build_logical_plan();

  void build_physical_plan();
  // Optimizations on logical plan (predicate pushdown, flattening)
  [[nodiscard]] logical::LogicalPlan optimize_logical_plan(logical::LogicalPlan plan) const;

  // For each logical scan enumerate alternatives (index vs scan)
  // returns vector of alternative LogicalScan nodes (candidates)
  [[nodiscard]] std::vector<logical::LogicalScan> enumerate_scan_alternatives(const logical::LogicalScan &scan) const;

  // chooses join order (returns permutation indices)
  [[nodiscard]] std::vector<size_t> choose_join_order(const std::vector<logical::LogicalOp*> &joins) const;

  // Map logical -> physical (core)
  [[nodiscard]] exec::PhysicalPlan map_logical_to_physical(const logical::LogicalPlan &logical_plan,
                                             const exec::ExecOptions &opts) const;

  // Final physical optimizations (push limits, top-k)
  [[nodiscard]] exec::PhysicalPlan finalize_physical_plan(exec::PhysicalPlan plan,
                                            const exec::ExecOptions &opts) const;

  // Full pipeline: AST -> PhysicalPlan
  [[nodiscard]] exec::PhysicalPlan build_execution_plan(const ast::QueryAST &ast,
                                          const exec::ExecOptions &opts) const;

  // Debug / explain helpers
  [[nodiscard]] String explain_logical_plan(const logical::LogicalPlan &plan) const;
  [[nodiscard]] String explain_physical_plan(const exec::PhysicalPlan &plan) const;

private:
  exec::ExecContext ctx_;
  ast::QueryAST ast_plan_;
  logical::LogicalOpPtr logical_plan_;
  exec::PhysicalOpPtr physical_plan_;
  // const storage::GraphDB &cat_;
  // PlannerConfig cfg_;
  std::unique_ptr<CostModel> cost_model_;
  std::unique_ptr<JoinOrderStrategy> join_strategy_;


};
}


namespace ast {
  struct EvalContext {
    graph::exec::Row& row;
  };
}
