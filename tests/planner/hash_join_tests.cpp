#include <gtest/gtest.h>

#include "test_utils.hpp"
#include "planner/optimizer.hpp"
#include "planner/logical_planner.hpp"
#include "planner/physical_planner.hpp"

using namespace graph;

namespace {

using LPtr = logical::LogicalOpPtr;

LPtr MakeScan(const std::string& alias, std::initializer_list<String> labels) {
  return std::make_unique<logical::LogicalScan>(std::vector<String>(labels), alias);
}

ast::ExprPtr MakeProp(const std::string& alias, const std::string& property) {
  auto e = std::make_unique<ast::PropertyExpr>();
  e->alias = alias;
  e->property = property;
  return e;
}

ast::ExprPtr MakeLiteral(int64_t value) {
  auto e = std::make_unique<ast::LiteralExpr>();
  e->literal = ast::Literal{value};
  return e;
}

ast::ExprPtr MakeGt(ast::ExprPtr lhs, ast::ExprPtr rhs) {
  auto e = std::make_unique<ast::ComparisonExpr>();
  e->left_expr = std::move(lhs);
  e->op = ast::CompareOp::Gt;
  e->right_expr = std::move(rhs);
  return e;
}

ast::ExprPtr MakeGe(ast::ExprPtr lhs, ast::ExprPtr rhs) {
  auto e = std::make_unique<ast::ComparisonExpr>();
  e->left_expr = std::move(lhs);
  e->op = ast::CompareOp::Ge;
  e->right_expr = std::move(rhs);
  return e;
}

ast::ExprPtr MakeEq(ast::ExprPtr lhs, ast::ExprPtr rhs) {
  auto e = std::make_unique<ast::ComparisonExpr>();
  e->left_expr = std::move(lhs);
  e->op = ast::CompareOp::Eq;
  e->right_expr = std::move(rhs);
  return e;
}

ast::ExprPtr MakeAnd(ast::ExprPtr lhs, ast::ExprPtr rhs) {
  return std::make_unique<ast::LogicalExpr>(std::move(lhs), ast::LogicalOp::And, std::move(rhs));
}

void ExecuteAndConsume(graph::planner::Planner& planner, graph::exec::ExecContext& ctx) {
  planner.build_logical_plan();
  (void)planner.build_physical_plan();
  auto cursor = planner.getPhysicalPlan().root->open(ctx);

  graph::exec::Row row;
  while (cursor->next(row)) {
    row.clear();
  }
  // cursor->close();
}

void AddCreatedNode(ast::CreateClause& clause,
                    const String& alias,
                    const String& label,
                    const std::vector<std::pair<String, ast::Literal>>& props) {
  ast::NodePattern n;
  n.alias = alias;
  n.labels = {label};
  n.properties = props;
  clause.created_items.emplace_back(std::move(n));
}

struct MockCostModel final : optimizer::CostModel {
  optimizer::CostEstimate EstimateAllNodeScan(const storage::GraphDB*,
                                              const logical::LogicalScan&) const override {
    return {100.0, 10.0, 1.0, 0.0};
  }

  std::pair<optimizer::CostEstimate, String> EstimateIndexSeekLabel(const storage::GraphDB*,
                                                                     const logical::LogicalScan&) const override {
    return {{1000.0, 100.0, 10.0, 0.0}, ""};
  }

  optimizer::CostEstimate EstimateExpand(const storage::GraphDB*,
                                         const logical::LogicalExpand&,
                                         const optimizer::CostEstimate& input) const override {
    return {input.row_count, input.cpu_cost + 1.0, input.io_cost, input.startup_cost};
  }

  optimizer::CostEstimate EstimateFilter(const storage::GraphDB*,
                                         const optimizer::CostEstimate& input,
                                         const ast::Expr*) const override {
    return {input.row_count * 0.5, input.cpu_cost + 1.0, input.io_cost, input.startup_cost};
  }

  optimizer::CostEstimate EstimateNestedJoin(const storage::GraphDB*,
                                             const optimizer::CostEstimate& left,
                                             const optimizer::CostEstimate& right,
                                             const ast::Expr*) const override {
    return {left.row_count * right.row_count, 1000.0, 1000.0, 100.0};
  }

  optimizer::CostEstimate EstimateHashJoin(const storage::GraphDB*,
                                           const optimizer::CostEstimate& left,
                                           const optimizer::CostEstimate& right,
                                           const std::vector<ast::Expr*>&,
                                           const std::vector<ast::Expr*>&) const override {
    return {std::min(left.row_count, right.row_count), 1.0, 1.0, 1.0};
  }

  optimizer::CostEstimate EstimateProject(const storage::GraphDB*,
                                          const optimizer::CostEstimate& child,
                                          const logical::LogicalProject&) const override {
    return child;
  }

  optimizer::CostEstimate EstimateSort(const storage::GraphDB*,
                                       const optimizer::CostEstimate& input) const override {
    return {input.row_count, input.cpu_cost + 50.0, input.io_cost, input.startup_cost};
  }

  optimizer::CostEstimate EstimateLimit(const storage::GraphDB*,
                                        const optimizer::CostEstimate& input,
                                        size_t limit) const override {
    return {std::min<double>(input.row_count, limit), input.cpu_cost, input.io_cost, input.startup_cost};
  }

  optimizer::CostEstimate EstimateSet(const storage::GraphDB*,
                                      const optimizer::CostEstimate& input,
                                      const logical::LogicalSet&) const override {
    return input;
  }

  optimizer::CostEstimate EstimateCreate(const storage::GraphDB*,
                                         const optimizer::CostEstimate& input,
                                         const logical::LogicalCreate&) const override {
    return input;
  }

  optimizer::CostEstimate EstimateDelete(const storage::GraphDB*,
                                         const optimizer::CostEstimate& input,
                                         const logical::LogicalDelete&) const override {
    return input;
  }
};

} // namespace

TEST(HashJoinComplex, LargeDatabaseFilterPushdownAndEquiJoin) {
  storage::GraphDB db;
  graph::exec::ExecContext create_ctx(&db);

  ast::QueryAST create_q;
  create_q.create_clause = std::make_unique<ast::CreateClause>();

  for (int i = 0; i < 12; ++i) {
    AddCreatedNode(
        *create_q.create_clause,
        "a" + std::to_string(i),
        "Person",
        {
            {"id", ast::Literal{i}},
            {"city_id", ast::Literal{i % 4}},
            {"age", ast::Literal{20 + i}}
        }
    );
  }

  for (int i = 0; i < 4; ++i) {
    AddCreatedNode(
        *create_q.create_clause,
        "b" + std::to_string(i),
        "City",
        {
            {"id", ast::Literal{i}},
            {"population", ast::Literal{5000 + i}}
        }
    );
  }

  {
    graph::planner::Planner creator(create_ctx, &db, std::move(create_q));
    ExecuteAndConsume(creator, create_ctx);
  }

  ASSERT_EQ(db.node_count_with_label("Person"), 12u);
  ASSERT_EQ(db.node_count_with_label("City"), 4u);

  auto left = MakeScan("a", {"Person"});
  auto right = MakeScan("b", {"City"});

  auto join_pred = MakeEq(MakeProp("a", "city_id"), MakeProp("b", "id"));
  auto join = std::make_unique<logical::LogicalJoin>(std::move(left), std::move(right));

  auto filter_pred = MakeAnd(
      std::move(join_pred),
      MakeAnd(
          MakeGe(MakeProp("a", "age"), MakeLiteral(25)),
          MakeGt(MakeProp("b", "population"), MakeLiteral(1000))
      )
  );

  auto filter = std::make_unique<logical::LogicalFilter>(std::move(join), std::move(filter_pred));
  logical::LogicalPlan plan(std::move(filter));

  optimizer::optimize_logical_plan_impl(plan.root);

  auto* phys_join = dynamic_cast<exec::HashJoinOp*>(
      plan.root->BuildPhysical(*(new graph::exec::ExecContext(&db)), new MockCostModel(), &db).first.get()
  );
  ASSERT_NE(phys_join, nullptr);

  MockCostModel cm;
  graph::exec::ExecContext ctx(&db);
  auto [root, cost] = plan.root->BuildPhysical(ctx, &cm, &db);
  ASSERT_NE(root, nullptr);
  ASSERT_NE(dynamic_cast<exec::HashJoinOp*>(root.get()), nullptr);

  auto cursor = root->open(ctx);

  graph::exec::Row row;
  size_t count = 0;
  while (cursor->next(row)) {
    ++count;
    ASSERT_EQ(row.slots.size(), 2u);
    ASSERT_TRUE(std::holds_alternative<storage::Node*>(row.slots[0]));
    ASSERT_TRUE(std::holds_alternative<storage::Node*>(row.slots[1]));

    auto* a = std::get<storage::Node*>(row.slots[0]);
    auto* b = std::get<storage::Node*>(row.slots[1]);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    ASSERT_TRUE(a->properties.contains("age"));
    ASSERT_TRUE(a->properties.contains("city_id"));
    ASSERT_TRUE(b->properties.contains("population"));
    ASSERT_TRUE(b->properties.contains("id"));

    EXPECT_GE(std::get<int64_t>(a->properties.at("age")), 25);
    EXPECT_GT(std::get<int64_t>(b->properties.at("population")), 1000);
    EXPECT_EQ(std::get<int64_t>(a->properties.at("city_id")),
              std::get<int64_t>(b->properties.at("id")));

    row.clear();
  }

  EXPECT_EQ(count, 7u);
}

TEST(HashJoinComplex, CompositeKeyJoinOnLargeDataSet) {
  storage::GraphDB db;
  graph::exec::ExecContext create_ctx(&db);

  ast::QueryAST create_q;
  create_q.create_clause = std::make_unique<ast::CreateClause>();

  for (int i = 0; i < 12; ++i) {
    AddCreatedNode(
        *create_q.create_clause,
        "a" + std::to_string(i),
        "Person",
        {
            {"k1", ast::Literal{i}},
            {"k2", ast::Literal{i + 100}},
            {"age", ast::Literal{20 + i}}
        }
    );
  }

  for (int i = 0; i < 12; ++i) {
    AddCreatedNode(
        *create_q.create_clause,
        "b" + std::to_string(i),
        "Dept",
        {
            {"k1", ast::Literal{i}},
            {"k2", ast::Literal{i + 100}},
            {"weight", ast::Literal{1000 + i}}
        }
    );
  }

  {
    graph::planner::Planner creator(create_ctx, &db, std::move(create_q));
    ExecuteAndConsume(creator, create_ctx);
  }

  ASSERT_EQ(db.node_count_with_label("Person"), 12u);
  ASSERT_EQ(db.node_count_with_label("Dept"), 12u);

  auto left = MakeScan("a", {"Person"});
  auto right = MakeScan("b", {"Dept"});
  auto join = std::make_unique<logical::LogicalJoin>(std::move(left), std::move(right));

  auto pred = MakeAnd(
      MakeAnd(
          MakeEq(MakeProp("a", "k1"), MakeProp("b", "k1")),
          MakeEq(MakeProp("a", "k2"), MakeProp("b", "k2"))
      ),
      MakeAnd(
          MakeGe(MakeProp("a", "age"), MakeLiteral(25)),
          MakeGt(MakeProp("b", "weight"), MakeLiteral(1005))
      )
  );

  auto filter = std::make_unique<logical::LogicalFilter>(std::move(join), std::move(pred));
  logical::LogicalPlan plan(std::move(filter));

  optimizer::optimize_logical_plan_impl(plan.root);

  MockCostModel cm;
  graph::exec::ExecContext ctx(&db);

  auto [root, cost] = plan.root->BuildPhysical(ctx, &cm, &db);
  ASSERT_NE(root, nullptr);
  auto* hash = dynamic_cast<exec::HashJoinOp*>(root.get());
  ASSERT_NE(hash, nullptr) << "Composite equi-join should become HashJoinOp";
  ASSERT_EQ(hash->left_keys.size(), 2u);
  ASSERT_EQ(hash->right_keys.size(), 2u);

  auto cursor = root->open(ctx);

  graph::exec::Row row;
  size_t count = 0;
  while (cursor->next(row)) {
    ++count;
    ASSERT_EQ(row.slots.size(), 2u);

    auto* a = std::get<storage::Node*>(row.slots[0]);
    auto* b = std::get<storage::Node*>(row.slots[1]);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    EXPECT_GE(std::get<int64_t>(a->properties.at("age")), 25);
    EXPECT_GT(std::get<int64_t>(b->properties.at("weight")), 1005);
    EXPECT_EQ(std::get<int64_t>(a->properties.at("k1")),
              std::get<int64_t>(b->properties.at("k1")));
    EXPECT_EQ(std::get<int64_t>(a->properties.at("k2")),
              std::get<int64_t>(b->properties.at("k2")));

    row.clear();
  }

  EXPECT_EQ(count, 6u);
}
