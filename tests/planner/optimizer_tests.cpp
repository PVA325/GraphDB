#include <gtest/gtest.h>

#include "planner/optimizer.hpp"
#include "planner/logical_planner.hpp"
#include "planner/physical_planner.hpp"
#include "parser/ast.hpp"
#include <memory>

using namespace graph;

namespace {

using LPtr = logical::LogicalOpPtr;

LPtr MakeScan(const std::string& alias) {
  return std::make_unique<logical::LogicalScan>(std::vector<String>{}, alias);
}

ast::ExprPtr MakeLiteral(int64_t v) {
  auto lit = std::make_unique<ast::LiteralExpr>();
  lit->literal = ast::Literal{v};
  return lit;
}

ast::ExprPtr MakeProp(const std::string& alias, const std::string& property) {
  auto prop = std::make_unique<ast::PropertyExpr>();
  prop->alias = alias;
  prop->property = property;
  return prop;
}

ast::ExprPtr MakeCmp(ast::ExprPtr lhs, ast::CompareOp op, ast::ExprPtr rhs) {
  return std::make_unique<ast::ComparisonExpr>(
      std::move(lhs), op, std::move(rhs)
  );
}

ast::ExprPtr MakeAnd(ast::ExprPtr lhs, ast::ExprPtr rhs) {
  return std::make_unique<ast::LogicalExpr>(std::move(lhs), ast::LogicalOp::And, std::move(rhs));
}

logical::LogicalPlan MakeFilterOverJoin(ast::ExprPtr pred) {
  auto left = MakeScan("a");
  auto right = MakeScan("b");
  auto join = std::make_unique<logical::LogicalJoin>(std::move(left), std::move(right));
  auto filter = std::make_unique<logical::LogicalFilter>(std::move(join), std::move(pred));
  return logical::LogicalPlan(std::move(filter));
}

logical::LogicalPlan MakeJoin(ast::ExprPtr pred) {
  auto left = MakeScan("a");
  auto right = MakeScan("b");
  auto join = std::make_unique<logical::LogicalJoin>(std::move(left), std::move(right), std::move(pred));
  return logical::LogicalPlan(std::move(join));
}

void RunLogicalOptimizer(logical::LogicalPlan& plan) {
  ASSERT_NE(plan.root, nullptr);
  optimizer::optimize_logical_plan_impl(plan.root);
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

}  // namespace

TEST(OptimizerFilterPushdown, PushesSidePredicatesBelowJoin) {
  // (a.x = 1) AND (b.y = 2) AND (a.id = b.id)
  auto pred =
      MakeAnd(
          MakeAnd(
              MakeCmp(MakeProp("a", "x"), ast::CompareOp::Eq, MakeLiteral(1)),
              MakeCmp(MakeProp("b", "y"), ast::CompareOp::Eq, MakeLiteral(2))
          ),
          MakeCmp(MakeProp("a", "id"), ast::CompareOp::Eq, MakeProp("b", "id"))
      );

  auto plan = MakeFilterOverJoin(std::move(pred));
  // std::cout << plan.SubtreeDebugString() << std::endl << "\n\n";
  RunLogicalOptimizer(plan);
  // std::cout << plan.SubtreeDebugString() << std::endl;

  auto* join = dynamic_cast<logical::LogicalJoin*>(plan.root.get());
  ASSERT_NE(join, nullptr);

  auto* left_filter = dynamic_cast<logical::LogicalFilter*>(join->left.get());
  auto* right_filter = dynamic_cast<logical::LogicalFilter*>(join->right.get());

  ASSERT_NE(left_filter, nullptr) << "Predicate for a.* should be pushed to the left side";
  ASSERT_NE(right_filter, nullptr) << "Predicate for b.* should be pushed to the right side";

  EXPECT_NE(dynamic_cast<logical::LogicalScan*>(left_filter->child.get()), nullptr);
  EXPECT_NE(dynamic_cast<logical::LogicalScan*>(right_filter->child.get()), nullptr);
}

TEST(OptimizerFilterPushdown, PushesSingleSidePredicateOnlyToThatSide) {
  auto pred = MakeCmp(MakeProp("a", "x"), ast::CompareOp::Eq, MakeLiteral(1));
  auto plan = MakeFilterOverJoin(std::move(pred));

  RunLogicalOptimizer(plan);

  auto* join = dynamic_cast<logical::LogicalJoin*>(plan.root.get());
  ASSERT_NE(join, nullptr);

  auto* left_filter = dynamic_cast<logical::LogicalFilter*>(join->left.get());
  ASSERT_NE(left_filter, nullptr);
  EXPECT_NE(dynamic_cast<logical::LogicalScan*>(left_filter->child.get()), nullptr);

  EXPECT_NE(dynamic_cast<logical::LogicalScan*>(join->right.get()), nullptr);
}

TEST(OptimizerFilterPushdown, KeepsCrossPredicateAtJoinLevel) {
  // a.id = b.id
  auto pred = MakeCmp(MakeProp("a", "id"), ast::CompareOp::Eq, MakeProp("b", "id"));
  auto plan = MakeFilterOverJoin(std::move(pred));

  RunLogicalOptimizer(plan);

  auto* join = dynamic_cast<logical::LogicalJoin*>(plan.root.get());
  ASSERT_NE(join, nullptr);

  EXPECT_NE(join->predicate, nullptr);
  EXPECT_EQ(dynamic_cast<logical::LogicalFilter*>(join->left.get()), nullptr);
  EXPECT_EQ(dynamic_cast<logical::LogicalFilter*>(join->right.get()), nullptr);
}

TEST(OptimizerHashJoin, ChoosesHashJoinForPureEquiJoin) {
  auto pred = MakeCmp(MakeProp("a", "id"), ast::CompareOp::Eq, MakeProp("b", "id"));
  auto plan = MakeJoin(std::move(pred));

  RunLogicalOptimizer(plan);

  MockCostModel cm;
  exec::ExecContext ctx(nullptr);

  auto [physical_root, cost] = plan.root->BuildPhysical(ctx, &cm, nullptr);
  ASSERT_NE(physical_root, nullptr);

  EXPECT_NE(dynamic_cast<exec::HashJoinOp*>(physical_root.get()), nullptr);
  EXPECT_EQ(dynamic_cast<exec::NestedLoopJoinOp*>(physical_root.get()), nullptr);
}

TEST(OptimizerHashJoin, KeepsNestedLoopForNonEquiJoin) {
  auto pred = MakeCmp(MakeProp("a", "id"), ast::CompareOp::Gt, MakeProp("b", "id"));
  auto plan = MakeJoin(std::move(pred));

  RunLogicalOptimizer(plan);

  MockCostModel cm;
  exec::ExecContext ctx(nullptr);

  auto [physical_root, cost] = plan.root->BuildPhysical(ctx, &cm, nullptr);
  // std::cout << physical_root->SubtreeDebugString() << std::endl;
  ASSERT_NE(physical_root, nullptr);

  EXPECT_NE(dynamic_cast<exec::NestedLoopJoinOp*>(physical_root.get()), nullptr);
}


