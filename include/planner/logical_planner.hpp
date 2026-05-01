#ifndef GRAPHDB_LOGICAL_PLAN_HPP
#define GRAPHDB_LOGICAL_PLAN_HPP
#include "common/common_value.hpp"

namespace graph::logical {
using BuildPhysicalType = std::pair<exec::PhysicalOpPtr, optimizer::CostEstimate>;

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

struct LogicalOpUnaryChild : LogicalOp {
  LogicalOpPtr child;

  explicit LogicalOpUnaryChild(LogicalOpPtr child);
  std::vector<LogicalOp*> GetChildren() override { return { PlannerUtils::GetUniqPtrVal(child) }; }

  [[nodiscard]] String SubtreeDebugString() const override;
  ~LogicalOpUnaryChild() override = default;
};

struct LogicalOpBinaryChild : LogicalOp {
  LogicalOpPtr left;
  LogicalOpPtr right;

  LogicalOpBinaryChild(LogicalOpPtr left, LogicalOpPtr right);

  [[nodiscard]] String SubtreeDebugString() const override;

  std::vector<LogicalOp*> GetChildren() override { return { PlannerUtils::GetUniqPtrVal(left), PlannerUtils::GetUniqPtrVal(right) }; }

  ~LogicalOpBinaryChild() override = default;
};

struct LogicalOpZeroChild : LogicalOp {
  LogicalOpZeroChild() = default;

  [[nodiscard]] String SubtreeDebugString() const override { return DebugString(); }
  std::vector<LogicalOp*> GetChildren() override { return {}; }

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

  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model, storage::GraphDB* db) const override;

  [[nodiscard]] String DebugString() const override;
  [[nodiscard]] String SubtreeDebugString() const final;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Expand; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return {dst_alias}; }

  ~LogicalExpand() override = default;
};

struct LogicalFilter : public LogicalOpUnaryChild {
  /// Filter - not include int answer all values that do not satisfy predicate
  std::unique_ptr<ast::Expr> predicate;

  LogicalFilter() = delete;

  explicit LogicalFilter(LogicalOpPtr child, std::unique_ptr<ast::Expr> predicate);
  explicit LogicalFilter(LogicalOpPtr child, ast::Expr* predicate);

  [[nodiscard]] String DebugString() const override;
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model, storage::GraphDB* db) const override;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Filter; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return child->GetSubtreeAliases(); }

  ~LogicalFilter() override = default;
};

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

struct LogicalLimit : public LogicalOpUnaryChild {
  /// Logical Limit - nothing to comment
  size_t limit_size;

  LogicalLimit() = delete;

  explicit LogicalLimit(LogicalOpPtr child, size_t limit_size);

  [[nodiscard]] String DebugString() const override;
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model, storage::GraphDB* db) const override;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Limit; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return child->GetSubtreeAliases(); }

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
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model, storage::GraphDB* db) const override;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Sort; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return child->GetSubtreeAliases(); }

  ~LogicalSort() override = default;
};

struct LogicalJoin : LogicalOpBinaryChild {
  /// Logical Joint for 2 results with predicate(optional)

  std::unique_ptr<ast::Expr> predicate;

  LogicalJoin() = delete;

  LogicalJoin(LogicalOpPtr left, LogicalOpPtr right);

  LogicalJoin(LogicalOpPtr left, LogicalOpPtr right, std::unique_ptr<ast::Expr> predicate);
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model, storage::GraphDB* db) const override;


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
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model, storage::GraphDB* db) const override;

  [[nodiscard]] String DebugString() const override;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Set; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return child->GetSubtreeAliases(); }

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

  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model, storage::GraphDB* db) const override;

  [[nodiscard]] String DebugString() const override;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Delete; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return child->GetSubtreeAliases(); }

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

  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model, storage::GraphDB* db) const override;

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Create; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return child->GetSubtreeAliases(); }

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
  BuildPhysicalType BuildPhysical(exec::ExecContext& ctx, optimizer::CostModel* cost_model, storage::GraphDB* db) const override { return root->BuildPhysical(ctx, cost_model, db); }

  [[nodiscard]] LogicalOpType Type() const final { return LogicalOpType::Plan; }

  [[nodiscard]] std::vector<String> GetSubtreeAliases() const final { return root->GetSubtreeAliases(); }

  ~LogicalPlan() override = default;
};
} // namespace graph::logical

#endif //GRAPHDB_LOGICAL_PLAN_HPP