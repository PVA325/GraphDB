#pragma once
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
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


struct PlannerError : public std::runtime_error {
  // todo at the end
  explicit PlannerError(const std::string& msg);
};

struct ExecutionError : public std::runtime_error {
  // todo at the end
  explicit ExecutionError(const std::string& msg);
};
} // namespace graph

namespace graph::planner {
struct CostModel;
// Cost estimates structure

struct CostEstimate {
  double row_count{0.0}; // count of rows that operator gives us
  double cpu_cost{0.0}; // nominal cost of cpu
  double io_cost{0.0};
  // nominal cost if read/write(or other heavy operations with ram, not cache) operations(for example for index scan)
  double startup_cost{0.0}; // cost to give first row

  CostEstimate() = default;
  CostEstimate(const CostEstimate& other) = default;
  CostEstimate(CostEstimate&& other) = default;
  CostEstimate& operator=(const CostEstimate& other) = default;
  CostEstimate& operator=(CostEstimate&& other) = default;
  ~CostEstimate() = default;

  [[nodiscard]] double total() const { return startup_cost + cpu_cost + 5.0 * io_cost; } // FOR TEST ONLY, REWRITE
};

} // namespace graph

namespace graph::PlannerUtils {
template <typename... Types>
struct overloads : Types... {
  using Types::operator()...;
};

String ValueToString(const Value& val);
String ConcatStrVector(const std::vector<String>& v);
String ConcatProperties(const std::vector<std::pair<String, Value>>& v);
String EdgeStrByDirection(ast::EdgeDirection dir);

template <typename PropertyM>
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
} // namespace graph::PlannerUtils

namespace graph::exec {
using storage::Node;
using storage::Edge;
struct PhysicalOp;
struct RowCursor;
struct Row;
struct ExecContext;

using RowSlot = std::variant<Node*, Edge*, Value>;
using PhysicalOpPtr = std::unique_ptr<PhysicalOp>;
using RowCursorPtr = std::unique_ptr<RowCursor>;

struct LabelIndexSeekOp;
struct NodeScanOp;
template <bool edge_outgoing>
struct ExpandOp;
using ExpandOutgoingOp = ExpandOp<true>;
using ExpandIngoingOp = ExpandOp<false>;

struct NestedLoopJoinOp;
struct KeyHashJoinOp;
struct PhysicalSetOp;
struct PhysicalCreateOp;
struct PhysicalDeleteOp;
} // namespace graph::exec

namespace graph::logical {
struct LogicalOp;
using LogicalOpPtr = std::unique_ptr<LogicalOp>;
using BuildPhysicalType = std::pair<exec::PhysicalOpPtr, planner::CostEstimate>;

struct LogicalOp {
  [[nodiscard]] virtual String DebugString() const = 0;
  virtual BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, planner::CostModel* cost_model,
                                          storage::GraphDB* db) const = 0;

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

  AliasedLogicalOp(std::string dst_alias) : dst_alias(std::move(dst_alias)) {
  }
};

struct LogicalScan : public AliasedLogicalOp {
  /// Scan Nodes that has specified labels
  /// and write out in row with slot name alias
  std::vector<String> labels;
  std::vector<std::pair<String, Value>> property_filters;

  LogicalScan(std::vector<String> labels, String dst_alias);

  LogicalScan(std::vector<String> labels, String alias, std::vector<std::pair<String, Value>> property_filters);

  BuildPhysicalType
  BuildPhysical(exec::ExecContext& ctx, planner::CostModel* cost_model, storage::GraphDB* db) const override;
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

  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, planner::CostModel* cost_model, storage::GraphDB* db) const override;

  [[nodiscard]] String DebugString() const override;
  [[nodiscard]] String SubtreeDebugString() const final;

  ~LogicalExpand() override = default;
};

struct LogicalFilter : public LogicalOpUnaryChild {
  /// Filter - not include int answer all values that do not satisfy predicate
  std::unique_ptr<ast::Expr> predicate;

  LogicalFilter() = delete;

  explicit LogicalFilter(LogicalOpPtr child, std::unique_ptr<ast::Expr> predicate);

  [[nodiscard]] String DebugString() const override;
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, planner::CostModel* cost_model, storage::GraphDB* db) const override;

  ~LogicalFilter() override = default;
};

struct LogicalProject : public LogicalOpUnaryChild {
  /// Project of values to items(can be field - string or Expression)
  std::vector<ast::ReturnItem> items;

  LogicalProject() = delete;

  LogicalProject(LogicalOpPtr child, std::vector<ast::ReturnItem> items);

  [[nodiscard]] String DebugString() const override;
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, planner::CostModel* cost_model, storage::GraphDB* db) const override;


  ~LogicalProject() override = default;
};

struct LogicalLimit : public LogicalOpUnaryChild {
  /// Logical Limit - nothing to comment
  size_t limit_size;

  LogicalLimit() = delete;

  explicit LogicalLimit(LogicalOpPtr child, size_t limit_size);

  [[nodiscard]] String DebugString() const override;
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, planner::CostModel* cost_model, storage::GraphDB* db) const override;


  ~LogicalLimit() override = default;
};

struct LogicalSort : LogicalOpUnaryChild {
  /// Logical Sort - sort by expression in items
  /// for example
  /*
    {
      {expr1, true},   // ASC
      {expr2, false}   // DESC
    } and sort if by expr1 and if they are equal by expr2
   */
  std::vector<ast::OrderItem> items;

  LogicalSort() = delete;

  explicit LogicalSort(LogicalOpPtr child, std::vector<ast::OrderItem> items);

  [[nodiscard]] String DebugString() const override;
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, planner::CostModel* cost_model, storage::GraphDB* db) const override;


  ~LogicalSort() override = default;
};

struct LogicalJoin : LogicalOpBinaryChild {
  /// Logical Joint for 2 results with predicate(optional)

  std::unique_ptr<ast::Expr> predicate;

  LogicalJoin() = delete;

  LogicalJoin(LogicalOpPtr left, LogicalOpPtr right);

  LogicalJoin(LogicalOpPtr left, LogicalOpPtr right, std::unique_ptr<ast::Expr> predicate);
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, planner::CostModel* cost_model, storage::GraphDB* db) const override;


  [[nodiscard]] String DebugString() const override;

  ~LogicalJoin() override = default;
};

struct LogicalSet : LogicalOpUnaryChild {
  /// Create LogicalSet; can set only 1 parameter through execution
  struct Assignment {
    String alias;
    std::vector<String> labels;
    std::vector<std::pair<String, Value>> properties;
  };

  std::vector<Assignment> assignment;

  LogicalSet(const LogicalSet&) = delete;

  LogicalSet(LogicalSet&& other) noexcept : LogicalOpUnaryChild(std::move(other.child)),
                                            assignment(std::move(other.assignment)) {
  }

  LogicalSet(LogicalOpPtr child, std::vector<Assignment> assignment);
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, planner::CostModel* cost_model, storage::GraphDB* db) const override;

  [[nodiscard]] String DebugString() const override;

  ~LogicalSet() override = default;
};

struct LogicalDelete : LogicalOpUnaryChild {
  /// Create logical operation in a tree to delete a target
  std::vector<String> target;

  LogicalDelete(const LogicalDelete&) = delete;

  LogicalDelete(LogicalDelete&& other) noexcept : LogicalOpUnaryChild(std::move(other.child)),
                                                  target(std::move(other.target)) {
  }

  LogicalDelete(LogicalOpPtr child, std::vector<String> target);

  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, planner::CostModel* cost_model, storage::GraphDB* db) const override;

  [[nodiscard]] String DebugString() const override;

  ~LogicalDelete() override = default;
};

struct CreateNodeSpec {
  String dst_alias;
  std::vector<String> labels;
  std::vector<std::pair<String, Value>> properties;

  [[nodiscard]] String DebugString() const;

  CreateNodeSpec() = delete;

  explicit CreateNodeSpec(const ast::NodePattern& pattern);

  explicit CreateNodeSpec(ast::NodePattern&& pattern);
};

struct CreateEdgeSpec {
  String src_alias;
  String dst_node_alias;
  String edge_alias;
  String edge_type;
  std::vector<std::pair<String, Value>> properties;
  ast::EdgeDirection direction;

  [[nodiscard]] String DebugString() const;

  CreateEdgeSpec() = delete;

  explicit CreateEdgeSpec(const ast::CreateEdgePattern& pattern);

  explicit CreateEdgeSpec(ast::CreateEdgePattern&& pattern);
};

struct LogicalCreate : LogicalOpUnaryChild {
  std::vector<std::variant<CreateNodeSpec, CreateEdgeSpec>> items;

  LogicalCreate() = delete;

  LogicalCreate(LogicalOpPtr child, const std::vector<ast::CreateItem>& items);

  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, planner::CostModel* cost_model, storage::GraphDB* db) const override;

  [[nodiscard]] String DebugString() const override;
};

struct LogicalPlan : LogicalOpZeroChild {
  /// Container logical plan
  LogicalOpPtr root;

  LogicalPlan() : root(nullptr) {
  }

  LogicalPlan(LogicalPlan&& other) noexcept : root(std::move(other.root)) {
  }

  LogicalPlan& operator=(LogicalPlan&& other) noexcept {
    root = std::move(other.root);
    return *this;
  }

  explicit LogicalPlan(LogicalOpPtr r) : root(std::move(r)) {
  }

  [[nodiscard]] String DebugString() const override { return "LogicalPlan:"; }
  [[nodiscard]] String SubtreeDebugString() const override { return root ? root->SubtreeDebugString() : "<empty>"; }
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, planner::CostModel* cost_model, storage::GraphDB* db) const override { return root->BuildPhysical(ctx, cost_model, db); }

  ~LogicalPlan() override = default;
};
} // namespace graph::logical


namespace graph::exec {
/// Operations need to live longer than cursors
struct ExecOptions {
  /// options of execution; memory, in future parallelism and spill
  size_t memory_budget_bytes = 256ULL * 1024 * 1024;
};

struct SlotMapping {
  SlotMapping() = default;
  SlotMapping(const SlotMapping&) = default;

  SlotMapping(SlotMapping&& other) noexcept : alias_to_slot(std::move(other.alias_to_slot)) {
  }

  SlotMapping& operator=(const SlotMapping&) = default;

  SlotMapping& operator=(SlotMapping&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    alias_to_slot = std::move(other.alias_to_slot);
    return *this;
  }

  ~SlotMapping() = default;

  bool key_exists(const std::string& key) const;

  size_t map(const std::string& key) const;
  size_t map_and_check(const String& key, const String& err_msg = "") const;

  void add_map(const String& key, size_t idx);

  void clear() { alias_to_slot.clear(); }

private:
  std::unordered_map<std::string, size_t> alias_to_slot;

  friend Row MergeRows(Row& first, Row& second);
};

struct ExecContext {
  /// Context of execution db
  storage::GraphDB* db;
  ExecOptions options;

  explicit ExecContext() : db{nullptr} {}

  ExecContext(storage::GraphDB* db) : db(db) {
  }
};

struct Row {
  /// store current row values and names
  std::vector<RowSlot> slots;
  std::vector<String> slots_names;
  SlotMapping slots_mapping;

  void clear() {
    slots.clear();
    slots_names.clear();
    slots_mapping.clear();
  }

  Row() = default;
  Row(const Row& other) = default;

  Row(Row&& other) noexcept :
    slots(std::move(other.slots)),
    slots_names(std::move(other.slots_names)),
    slots_mapping(std::move(other.slots_mapping)) {
  }

  Row& operator=(const Row& other) = default;

  Row& operator=(Row&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    slots = std::move(other.slots);
    slots_names = std::move(other.slots_names);
    slots_mapping = std::move(other.slots_mapping);
    return *this;
  }

  template <typename T>
    requires std::is_constructible_v<RowSlot, T>
  void AddValue(const T& val, const String& alias, const String& error_msg = "") {
    if (slots_mapping.key_exists(alias)) {
      throw std::runtime_error(error_msg);
    }
    slots.emplace_back(val);
    slots_names.emplace_back(alias);
    slots_mapping.add_map(alias, slots.size() - 1);
  }

  ~Row() = default;
};

struct RowCursor {
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

  PhysicalOpUnaryChild(PhysicalOpPtr child) : child(std::move(child)) {
  }

  [[nodiscard]] String SubtreeDebugString() const override;

  ~PhysicalOpUnaryChild() override = default;
};

struct PhysicalOpBinaryChild : PhysicalOp {
  PhysicalOpPtr left;
  PhysicalOpPtr right;

  PhysicalOpBinaryChild(PhysicalOpPtr left, PhysicalOpPtr right) : left(std::move(left)), right(std::move(right)) {
  }

  [[nodiscard]] String SubtreeDebugString() const override;

  ~PhysicalOpBinaryChild() override = default;
};

struct NodeScanCursor : RowCursor {
  std::unique_ptr<storage::NodeCursor> nodes_cursor;
  String out_alias;

  NodeScanCursor(NodeScanCursor&&) = default;

  NodeScanCursor(std::unique_ptr<storage::NodeCursor> nodes_cursor, String out_alias);

  bool next(Row& out) override;

  void close() override;

  ~NodeScanCursor() override = default;
};

struct LabelIndexSeekOp : public PhysicalOpNoChild {
  /// used for node filtering(for more optimized by label we dont need to do all node scan)
  String out_alias;
  String label;

  LabelIndexSeekOp(String alias, String label);

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;

  ~LabelIndexSeekOp() override = default;
};

struct NodeScanOp : public PhysicalOpNoChild {
  /// co complete Node scan and return Row to every Node
  String out_alias;

  NodeScanOp(String out_alias) : out_alias(std::move(out_alias)) {
  }

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;

  ~NodeScanOp() override = default;
};

struct NodePropertyFilterCursor : RowCursor {
  RowCursorPtr child_cursor;
  String alias;
  std::vector<String> labels;
  std::vector<std::pair<String, Value>> properties;

  NodePropertyFilterCursor(RowCursorPtr child_cursor, String alias, std::vector<String> labels,
                           std::vector<std::pair<String, Value>> properties);
  bool next(Row& out) override;

  void close() override;

  ~NodePropertyFilterCursor() override = default;
};

struct NodePropertyFilterOp : PhysicalOpUnaryChild {
  String alias;
  std::vector<String> labels;
  std::vector<std::pair<String, Value>> properties;

  NodePropertyFilterOp(PhysicalOpPtr child, String alias, std::vector<String> labels,
                       std::vector<std::pair<String, Value>> properties);
  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;

  ~NodePropertyFilterOp() override = default;
};

template <bool edge_outgoing>
struct ExpandNodeCursorPhysical : RowCursor {
  enum class Direction {
    Outgoing, Ingoing
  };

  RowCursorPtr child_cursor;
  std::unique_ptr<storage::EdgeCursor> edge_cursor;
  std::function<bool(Edge*)> label_predicate;
  String src_alias;
  String dst_edge_alias;
  String dst_node_alias;
  storage::GraphDB* db;

  ExpandNodeCursorPhysical(const ExpandNodeCursorPhysical&) = delete;

  ExpandNodeCursorPhysical(ExpandNodeCursorPhysical&&) = default;

  ExpandNodeCursorPhysical(RowCursorPtr child_cursor, String src_alias, String dst_edge_alias,
                           String dst_node_alias, std::function<bool(Edge*)> label_predicate,
                           storage::GraphDB* db);

  bool next(Row& out) override;

  void close() override;

  ~ExpandNodeCursorPhysical() override = default;
};


template <bool edge_outgoing>
struct ExpandOp : public PhysicalOpUnaryChild {
  /// write do dst_alias outgoing edge of edge_type
  String src_alias;
  String dst_edge_alias;
  String dst_node_alias;
  std::optional<String> edge_type;

  ExpandOp(String src_alias, String dst_edge_alias, String dst_node_alias, std::optional<String> edge_type,
           PhysicalOpPtr child);

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;

  ~ExpandOp() override = default;
};

struct FilterCursor : RowCursor {
  RowCursorPtr child_cursor;
  ast::Expr* predicate;

  FilterCursor(const FilterCursor&) = delete;

  FilterCursor(FilterCursor&&) = default;

  FilterCursor(RowCursorPtr child_cursor, ast::Expr* predicate);

  bool next(Row& out) override;

  void close() override;

  ~FilterCursor() override = default;
};

struct FilterOp : public PhysicalOpUnaryChild {
  /// do Filter operation with predicate on Child like while (!predicate(child)) { child.next(row) }
  ast::Expr* predicate;

  FilterOp(ast::Expr* predicate, PhysicalOpPtr child);

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;

  ~FilterOp() override = default;

private:
  String debugString_;
};

struct ProjectCursor : RowCursor {
  RowCursorPtr child_cursor;
  std::vector<ast::ReturnItem> items;

  ProjectCursor(const ProjectCursor&) = delete;

  ProjectCursor(ProjectCursor&&) = default;

  ProjectCursor(RowCursorPtr child_cursor, std::vector<ast::ReturnItem> items);

  bool next(Row& out) override;

  void close() override;

  ~ProjectCursor() override = default;
};

struct ProjectOp : public PhysicalOpUnaryChild {
  /// child is next operator in the tree
  std::vector<ast::ReturnItem> items;

  ProjectOp(std::vector<ast::ReturnItem> items,
            PhysicalOpPtr child);

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;

  ~ProjectOp() override = default;
};

struct LimitCursor : RowCursor {
  RowCursorPtr child_cursor;
  size_t limit_count;
  size_t used_count{0};

  LimitCursor(const LimitCursor&) = delete;

  LimitCursor(LimitCursor&&) = default;

  LimitCursor(RowCursorPtr child_cursor, size_t limit_count);

  bool next(Row& out) override;

  void close() override;

  ~LimitCursor() override = default;
};

struct LimitOp : public PhysicalOpUnaryChild {
  /// just physical limit
  Int limit_size;

  LimitOp(Int limit_size, PhysicalOpPtr child);

  RowCursorPtr open(ExecContext& ctx) override;

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

  bool next(Row& out) override;

  void close() override;

  ~NestedLoopJoinCursor() override = default;
};

struct NestedLoopJoinOp : public PhysicalOpBinaryChild {
  /// do join for 2 expressions based on predicate
  ast::Expr* predicate;

  NestedLoopJoinOp(PhysicalOpPtr left, PhysicalOpPtr right,
                   ast::Expr* pred);
  NestedLoopJoinOp(PhysicalOpPtr left, PhysicalOpPtr right);

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;
  ~NestedLoopJoinOp() override = default;
};

struct ValueHash {
  size_t operator()(const Value& k) const {
    const auto& visitor = PlannerUtils::overloads{
      // can make static in struct
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
  /// add mark
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

  bool next(Row& out) override;

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

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;
  ~KeyHashJoinOp() override = default;
};

struct SetCursor : RowCursor {
  RowCursorPtr child;
  std::vector<logical::LogicalSet::Assignment> assignments;
  storage::GraphDB* db;

  SetCursor(RowCursorPtr child,
            std::vector<logical::LogicalSet::Assignment> assignments,
            storage::GraphDB* db);
  bool next(Row& out) override;
  void close() override;
};

struct PhysicalSetOp : public PhysicalOpUnaryChild {
  std::vector<logical::LogicalSet::Assignment> assignments;

  PhysicalSetOp(PhysicalOpPtr child, std::vector<logical::LogicalSet::Assignment> assignments);

  RowCursorPtr open(ExecContext& ctx) override;

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
  bool next(Row& out) override;
  void close() override;

private:
  bool was_writing{false};
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
  bool next(Row& out) override;
  void close() override;

  ~DeleteCursor() override = default;
};

struct PhysicalDeleteOp : PhysicalOpUnaryChild {
  std::vector<String> aliases;

  PhysicalDeleteOp(std::vector<String> aliases, PhysicalOpPtr child);

  RowCursorPtr open(ExecContext& ctx) override;
  [[nodiscard]] String DebugString() const override;

  ~PhysicalDeleteOp() override = default;
};

struct SortCursor : RowCursor {
  RowCursorPtr child;
  std::vector<ast::OrderItem> items;
  std::vector<Row> rows;

  SortCursor(RowCursorPtr child, std::vector<ast::OrderItem> items);
  bool next(Row& out) override;
  void close() override;

  ~SortCursor() override = default;
};

struct PhysicalSortOp : PhysicalOpUnaryChild {
  std::vector<ast::OrderItem> items;
  PhysicalSortOp(PhysicalOpPtr child, std::vector<ast::OrderItem> items);

  RowCursorPtr open(ExecContext& ctx) override;
  [[nodiscard]] String DebugString() const override;

  ~PhysicalSortOp() override = default;
}; /// todo

struct PhysicalPlan {
  /// creates physical plan
  PhysicalOpPtr root;

  PhysicalPlan() = default;

  PhysicalPlan(PhysicalPlan&& other) noexcept : root(std::move(other.root)) {}

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
std::unique_ptr<ResultCursor> execute_plan(const exec::PhysicalPlan& plan,
                                           ExecContext& ctx);

// (Planner->Executor glue)
std::unique_ptr<ResultCursor> execute_query_ast(ast::QueryAST ast,
                                                const storage::GraphDB& cat,
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
    const ast::Expr* pred) const = 0;

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
    const ast::Expr* pred) const override;

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
  [[nodiscard]] double EstimateNodePropertySelectivity(const std::vector<std::pair<String, Value>>& props,
                                                       const storage::GraphDB* gb) const;
  [[nodiscard]] double EstimateNodeLabelsSelectivity(const std::vector<String>& labels,
                                                     const storage::GraphDB* gb) const;
  [[nodiscard]] std::pair<double, String> EstimateBiggestLabelSelectivity(
    const std::vector<String>& labels, const storage::GraphDB* gb) const;
  [[no_discard]] double EstimateEdgeTypeSelectivity(const std::optional<std::string>& label,
                                                    const storage::GraphDB* db) const;

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

class Planner {
public:
  explicit Planner(const exec::ExecContext& ctx, storage::GraphDB* db, ast::QueryAST ast) :
    ctx_(ctx), ast_plan_(std::move(ast)), db_(db) {
    cost_model_ = std::make_unique<DefaultCostModel>();
  }

  // End-to-end: AST -> LogicalPlan
  void build_logical_plan();

  CostEstimate build_physical_plan();

  logical::LogicalPlan& getLogicalPlan() { return logical_plan_; }
  exec::PhysicalPlan& getPhysicalPlan() { return physical_plan_; }

  // Optimizations on logical plan (predicate pushdown, flattening)
  [[nodiscard]] logical::LogicalPlan optimize_logical_plan(logical::LogicalPlan plan) const;

  // For each logical scan enumerate alternatives (index vs scan)
  // returns vector of alternative LogicalScan nodes (candidates)
  [[nodiscard]] std::vector<logical::LogicalScan> enumerate_scan_alternatives(const logical::LogicalScan& scan) const;

  // chooses join order (returns permutation indices)
  [[nodiscard]] std::vector<size_t> choose_join_order(const std::vector<logical::LogicalOp*>& joins) const;

  // Map logical -> physical (core)
  [[nodiscard]] exec::PhysicalPlan map_logical_to_physical(const logical::LogicalPlan& logical_plan,
                                                           const exec::ExecOptions& opts) const;

  // Final physical optimizations (push limits, top-k)
  [[nodiscard]] exec::PhysicalPlan finalize_physical_plan(exec::PhysicalPlan plan,
                                                          const exec::ExecOptions& opts) const;

  // Full pipeline: AST -> PhysicalPlan
  [[nodiscard]] exec::PhysicalPlan build_execution_plan(const ast::QueryAST& ast,
                                                        const exec::ExecOptions& opts) const;

  // Debug / explain helpers
  [[nodiscard]] String explain_logical_plan(const logical::LogicalPlan& plan) const;
  [[nodiscard]] String explain_physical_plan(const exec::PhysicalPlan& plan) const;

private:
  exec::ExecContext ctx_;
  ast::QueryAST ast_plan_;
  logical::LogicalPlan logical_plan_;
  exec::PhysicalPlan physical_plan_;
  std::unique_ptr<CostModel> cost_model_;
  storage::GraphDB* db_;
  // PlannerConfig cfg_;

  // std::unique_ptr<JoinOrderStrategy> join_strategy_;
};
} // namespace graph::logical


namespace ast {
struct EvalContext {
  graph::exec::Row& row;
};
} // namespace ast


#include "planner.tpp"
