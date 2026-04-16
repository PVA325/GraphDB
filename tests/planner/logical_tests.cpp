#include <algorithm>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "test_utils.hpp"

TEST(BuildLogicalPlan, MatchOnlyBuildsScan) {
  auto planner = MakePlanner(MakeQueryWithMatch("a"));

  planner.build_logical_plan();
  const auto& plan = planner.getLogicalPlan();

  EXPECT_EQ(plan.SubtreeDebugString(), "LogicalScan(as=a, labels=[Person])");
}

TEST(BuildLogicalPlan, MatchWhereBuildsFilterOverScan) {
  auto q = MakeQueryWithMatch("a");
  q.where = std::make_unique<ast::WhereClause>();
  q.where->expression = std::make_unique<FakeExpr>("a.age > 18");

  auto planner = MakePlanner(std::move(q));
  planner.build_logical_plan();

  const auto& plan = planner.getLogicalPlan();
  const auto s = plan.SubtreeDebugString();

  ExpectInOrder(s, {
    "LogicalFilter(a.age > 18)",
    "LogicalScan(as=a, labels=[Person])"
  });
}

TEST(BuildLogicalPlan, MatchReturnBuildsProjectOverScan) {
  auto q = MakeQueryWithMatch("a");
  q.return_clause = std::make_unique<ast::ReturnClause>();
  q.return_clause->items.push_back(ast::ReturnItem{std::string{"a"}});

  auto planner = MakePlanner(std::move(q));
  planner.build_logical_plan();

  const auto& plan = planner.getLogicalPlan();
  const auto s = plan.SubtreeDebugString();

  ExpectInOrder(s, {
    "Project(a)",
    "LogicalScan(as=a, labels=[Person])"
  });
}

TEST(BuildLogicalPlan, MatchWhereReturnBuildsProjectFilterScanChain) {
  auto q = MakeQueryWithMatch("a");
  q.where = std::make_unique<ast::WhereClause>();
  q.where->expression = std::make_unique<FakeExpr>("a.age > 18");

  q.return_clause = std::make_unique<ast::ReturnClause>();
  q.return_clause->items.push_back(ast::ReturnItem{std::string{"a"}});

  auto planner = MakePlanner(std::move(q));
  planner.build_logical_plan();

  const auto& plan = planner.getLogicalPlan();
  const auto s = plan.SubtreeDebugString();

  ExpectInOrder(s, {
    "Project(a)",
    "LogicalFilter(a.age > 18)",
    "LogicalScan(as=a, labels=[Person])"
  });
}

TEST(BuildLogicalPlan, MatchOrderByLimitReturnBuildsAllExpectedNodes) {
  auto q = MakeQueryWithMatch("a");

  q.return_clause = std::make_unique<ast::ReturnClause>();
  q.return_clause->items.push_back(ast::ReturnItem{std::string{"a"}});

  q.order = std::make_unique<ast::OrderClause>();
  ast::OrderItem item;
  item.property.alias = "a";
  item.property.property = "age";
  item.direction = ast::OrderDirection::Desc;
  q.order->items.push_back(item);

  q.limit = std::make_unique<ast::LimitClause>();
  q.limit->limit = 10;

  auto planner = MakePlanner(std::move(q));
  planner.build_logical_plan();

  const auto& plan = planner.getLogicalPlan();
  const auto s = plan.SubtreeDebugString();

  EXPECT_THAT(s, ::testing::HasSubstr("Project(a)"));
  EXPECT_THAT(s, ::testing::HasSubstr("Sort(Property(a.age) DESC)"));
  EXPECT_THAT(s, ::testing::HasSubstr("Limit(10)"));
  EXPECT_THAT(s, ::testing::HasSubstr("LogicalScan(as=a, labels=[Person])"));
}

TEST(BuildLogicalPlan, MatchSetBuildsLogicalSet) {
  auto q = MakeQueryWithMatch("a");
  q.set_clause = std::make_unique<ast::SetClause>();
  q.set_clause->target.alias = "a";
  q.set_clause->target.property = "age";
  q.set_clause->value = ast::Literal{21};

  auto planner = MakePlanner(std::move(q));
  planner.build_logical_plan();

  const auto& plan = planner.getLogicalPlan();
  const auto s = plan.SubtreeDebugString();

  EXPECT_THAT(s, ::testing::HasSubstr("LogicalSet(a, key=age, value=21)"));
  EXPECT_THAT(s, ::testing::HasSubstr("LogicalScan(as=a, labels=[Person])"));
}

TEST(BuildLogicalPlan, MatchDeleteBuildsLogicalDelete) {
  auto q = MakeQueryWithMatch("a");
  q.delete_clause = std::make_unique<ast::DeleteClause>();
  q.delete_clause->aliases = {"a"};

  auto planner = MakePlanner(std::move(q));
  planner.build_logical_plan();

  const auto& plan = planner.getLogicalPlan();
  const auto s = plan.SubtreeDebugString();

  EXPECT_THAT(s, ::testing::HasSubstr("LogicalDelete(a)"));
  EXPECT_THAT(s, ::testing::HasSubstr("LogicalScan(as=a, labels=[Person])"));
}

TEST(BuildLogicalPlan, CreateClauseBuildsLogicalCreate) {
  ast::QueryAST q;
  q.create = std::make_unique<ast::CreateClause>();

  ast::NodePattern node;
  node.alias = "a";
  node.labels = {"Person"};

  ast::CreateEdgePattern edge;
  edge.left_node.alias = "a";
  edge.right_node.alias = "b";
  edge.alias = "e";
  edge.label = "KNOWS";
  edge.direction = ast::EdgeDirection::Right;

  q.create->created_items.emplace_back(node);
  q.create->created_items.emplace_back(edge);

  auto planner = MakePlanner(std::move(q));
  planner.build_logical_plan();

  const auto& plan = planner.getLogicalPlan();
  const auto s = plan.SubtreeDebugString();

  EXPECT_THAT(s, ::testing::HasSubstr("LogicalCreate("));
  EXPECT_THAT(s, ::testing::HasSubstr("NodeCreate("));
  EXPECT_THAT(s, ::testing::HasSubstr("EdgeCreate("));
}

//
TEST(BuildLogicalPlanComplex, TwoMatchPatternsBuildJoinTree) {
  auto q = MakeSelectQueryTwoPatterns();

  graph::exec::ExecContext ctx;
  graph::planner::Planner planner(ctx, std::move(q));
  planner.build_logical_plan();

  const auto& s = planner.getLogicalPlan().SubtreeDebugString();

  EXPECT_EQ(CountSubstr(s, "LogicalScan("), 2u);
  EXPECT_THAT(s, ::testing::HasSubstr("Join("));
  EXPECT_THAT(s, ::testing::HasSubstr("Project("));
  EXPECT_THAT(s, ::testing::HasSubstr("1)"));
  EXPECT_THAT(s, ::testing::HasSubstr("2)"));
}

TEST(BuildLogicalPlanComplex, MatchWithEdgePatternBuildsExpand) {
  ast::QueryAST q;
  q.match = std::make_unique<ast::MatchClause>();
  q.match->patterns.push_back(
      MakeEdgePattern("a", "e", "b", "KNOWS", ast::EdgeDirection::Right)
  );

  q.return_clause = std::make_unique<ast::ReturnClause>();
  q.return_clause->items.push_back(ast::ReturnItem{std::string{"a"}});
  q.return_clause->items.push_back(ast::ReturnItem{std::string{"b"}});

  graph::exec::ExecContext ctx;
  graph::planner::Planner planner(ctx, std::move(q));
  planner.build_logical_plan();

  const auto& s = planner.getLogicalPlan().SubtreeDebugString();

  EXPECT_THAT(s, ::testing::HasSubstr("LogicalExpand(a->b"));
  EXPECT_THAT(s, ::testing::HasSubstr("LogicalScan("));
  EXPECT_THAT(s, ::testing::HasSubstr("Project("));
}

TEST(BuildLogicalPlanComplex, TwoPatternsWithWhereReturnSortLimitContainsAllMainNodes) {
  ast::QueryAST q = MakeSelectQueryTwoPatterns();

  q.where = std::make_unique<ast::WhereClause>();
  q.where->expression = std::make_unique<FakeExpr>("a.age > b.age");

  q.order = std::make_unique<ast::OrderClause>();
  ast::OrderItem ord;
  ord.property.alias = "a";
  ord.property.property = "age";
  ord.direction = ast::OrderDirection::Desc;
  q.order->items.push_back(ord);

  q.limit = std::make_unique<ast::LimitClause>();
  q.limit->limit = 5;

  graph::exec::ExecContext ctx;
  graph::planner::Planner planner(ctx, std::move(q));
  planner.build_logical_plan();

  const auto& s = planner.getLogicalPlan().SubtreeDebugString();

  EXPECT_EQ(CountSubstr(s, "LogicalScan("), 2u);
  EXPECT_THAT(s, ::testing::HasSubstr("Join("));
  EXPECT_THAT(s, ::testing::HasSubstr("LogicalFilter(a.age > b.age)"));
  EXPECT_THAT(s, ::testing::HasSubstr("Sort(Property(a.age) DESC)"));
  EXPECT_THAT(s, ::testing::HasSubstr("Limit(5)"));
  EXPECT_THAT(s, ::testing::HasSubstr("Project(a, b)"));
}

TEST(BuildLogicalPlanComplex, SetQueryKeepsMatchAndAddsLogicalSet) {
  ast::QueryAST q;
  q.match = std::make_unique<ast::MatchClause>();
  q.match->patterns.push_back(MakeNodePattern("a", {"Person"}));

  q.set_clause = std::make_unique<ast::SetClause>();
  q.set_clause->target.alias = "a";
  q.set_clause->target.property = "age";
  q.set_clause->value = ast::Literal{21};

  graph::exec::ExecContext ctx;
  graph::planner::Planner planner(ctx, std::move(q));
  planner.build_logical_plan();

  const auto& s = planner.getLogicalPlan().SubtreeDebugString();

  EXPECT_THAT(s, ::testing::HasSubstr("LogicalSet(a, key=age, value=21)"));
  EXPECT_THAT(s, ::testing::HasSubstr("LogicalScan(as=a, labels=[Person])"));
}

TEST(BuildLogicalPlanComplex, DeleteQueryWithTwoAliasesBuildsBinaryTree) {
  ast::QueryAST q;
  q.match = std::make_unique<ast::MatchClause>();
  q.match->patterns.push_back(MakeNodePattern("a", {"Person"}));
  q.match->patterns.push_back(MakeNodePattern("b", {"City"}));

  q.delete_clause = std::make_unique<ast::DeleteClause>();
  q.delete_clause->aliases = {"a", "b"};

  graph::exec::ExecContext ctx;
  graph::planner::Planner planner(ctx, std::move(q));
  planner.build_logical_plan();

  const auto& s = planner.getLogicalPlan().SubtreeDebugString();

  EXPECT_THAT(s, ::testing::HasSubstr("LogicalDelete(a, b)"));
  EXPECT_THAT(s, ::testing::HasSubstr("Join("));
  EXPECT_EQ(CountSubstr(s, "LogicalScan("), 2u);
}

TEST(BuildLogicalPlanComplex, CreateQueryWithSeveralItemsBuildsLogicalCreate) {
  ast::QueryAST q;
  q.create = std::make_unique<ast::CreateClause>();

  ast::NodePattern node;
  node.alias = "a";
  node.labels = {"Person"};
  node.properties = {{"age", ast::Literal{graph::Value{42}}}};

  ast::CreateEdgePattern edge;
  edge.left_node.alias = "a";
  edge.right_node.alias = "b";
  edge.alias = "e";
  edge.label = "KNOWS";
  edge.direction = ast::EdgeDirection::Right;
  edge.properties = {{"weight", ast::Literal{graph::Value{3}}}};

  q.create->created_items.push_back(node);
  q.create->created_items.push_back(edge);

  graph::exec::ExecContext ctx;
  graph::planner::Planner planner(ctx, std::move(q));
  planner.build_logical_plan();

  const auto& s = planner.getLogicalPlan().SubtreeDebugString();

  EXPECT_THAT(s, ::testing::HasSubstr("LogicalCreate("));
  EXPECT_THAT(s, ::testing::HasSubstr("NodeCreate("));
  EXPECT_THAT(s, ::testing::HasSubstr("EdgeCreate("));
  EXPECT_THAT(s, ::testing::HasSubstr("Person"));
  EXPECT_THAT(s, ::testing::HasSubstr("KNOWS"));
}

TEST(BuildLogicalPlanComplex, MatchTwoPatternsWhereUsesBothSides) {
  ast::QueryAST q;

  q.match = std::make_unique<ast::MatchClause>();
  q.match->patterns.push_back(MakeNodePattern("a", {"Person"}));
  q.match->patterns.push_back(MakeNodePattern("b", {"City"}));

  q.where = std::make_unique<ast::WhereClause>();
  q.where->expression = std::make_unique<FakeExpr>("a.age > b.population");

  q.return_clause = std::make_unique<ast::ReturnClause>();
  q.return_clause->items.push_back(ast::ReturnItem{std::string{"a"}});
  q.return_clause->items.push_back(ast::ReturnItem{std::string{"b"}});

  graph::exec::ExecContext ctx;
  graph::planner::Planner planner(ctx, std::move(q));
  planner.build_logical_plan();

  const auto& s = planner.getLogicalPlan().SubtreeDebugString();

  EXPECT_EQ(CountSubstr(s, "LogicalScan("), 2u);

  EXPECT_THAT(s, ::testing::HasSubstr("Join("));

  EXPECT_THAT(s, ::testing::HasSubstr("LogicalFilter(a.age > b.population)"));

  EXPECT_THAT(s, ::testing::HasSubstr("Project(a, b)"));

  ExpectInOrder(s, {
    "Project(a, b)",
    "LogicalFilter(a.age > b.population)",
    "Join("
  });
}