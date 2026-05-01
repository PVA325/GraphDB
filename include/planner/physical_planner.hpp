#ifndef GRAPHDB_PHYSICAL_PLAN_HPP
#define GRAPHDB_PHYSICAL_PLAN_HPP
#include "common/common_value.hpp"

namespace graph::exec {
/// Operations need to live longer than cursors
struct ExecOptions {
  /// options of execution; memory, in future parallelism and spill
  size_t memory_budget_bytes = 256ULL * 1024 * 1024;
};

struct ExecContext {
  /// Context of execution db
  storage::GraphDB* db;
  ExecOptions options;

  explicit ExecContext() : db{nullptr} {}

  ExecContext(storage::GraphDB* db) : db(db) {
  }
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

Value GetFeatureFromSlot(const RowSlot& slot, const String& feature_key);

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
        const std::vector<ast::Expr*>& left_keys_a, const std::vector<ast::Expr*>& right_keys_a);

  bool next(Row& out) override;

  void close() override;

  ~HashJoinCursor() override = default;
private:
  CompositeKey GetCompositeKey(const Row& row);
};

struct HashJoinOp : public PhysicalOpBinaryChild {
  /// do hashJoin
  std::vector<ast::ExprPtr> left_keys;
  std::vector<ast::ExprPtr> right_keys;

  HashJoinOp(PhysicalOpPtr left, PhysicalOpPtr right,
                std::vector<ast::ExprPtr>&& left_keys,
                std::vector<ast::ExprPtr>&& right_keys);

  RowCursorPtr open(ExecContext& ctx) override;

  [[nodiscard]] String DebugString() const override;
  ~HashJoinOp() override = default;

  static std::vector<ast::Expr*> ExprPtrVecToBasePtrVec(std::vector<ast::ExprPtr>& expr_vec);
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

#include "physical_planner.tpp"

#endif //GRAPHDB_PHYSICAL_PLAN_HPP