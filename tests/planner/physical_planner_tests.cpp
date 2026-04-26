#include <algorithm>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "test_utils.hpp"

TEST(BuildPhysicalPlanComplex, MatchWhereReturnOrderLimitMapsToPhysicalTree) {
  ast::QueryAST q;

  q.match_clause = std::make_unique<ast::MatchClause>();
  q.match_clause->patterns.push_back(MakeNodePattern("a", {"Person"}));
  q.match_clause->patterns.push_back(MakeNodePattern("b", {"City"}));

  q.where_clause = std::make_unique<ast::WhereClause>();
  q.where_clause->expression = std::make_unique<FakeExpr>("a.age > b.population");

  q.return_clause = std::make_unique<ast::ReturnClause>();
  q.return_clause->items.push_back(ast::ReturnItem{std::string{"a"}});
  q.return_clause->items.push_back(ast::ReturnItem{std::string{"b"}});

  q.order_clause = std::make_unique<ast::OrderClause>();
  ast::OrderItem ord;
  ord.property.alias = "a";
  ord.property.property = "age";
  ord.direction = ast::OrderDirection::Desc;
  q.order_clause->items.push_back(ord);

  q.limit_clause = std::make_unique<ast::LimitClause>();
  q.limit_clause->limit = 7;

  graph::exec::ExecContext ctx;
  graph::planner::Planner planner(ctx, std::move(q));

  planner.build_logical_plan();
  planner.build_physical_plan();

  const auto s = planner.getPhysicalPlan().DebugString();

  EXPECT_TRUE(s.find("Project(") != String::npos);
  EXPECT_TRUE(s.find("Limit(") != String::npos);
  EXPECT_TRUE(s.find("Sort(") != String::npos);
  EXPECT_TRUE(s.find("Filter(") != String::npos);
  EXPECT_TRUE(s.find("Join(") != String::npos
           || s.find("NestedLoopJoin(") != String::npos
           || s.find("HashJoin(") != String::npos);

  EXPECT_TRUE(s.find("NodeScan(") != String::npos
           || s.find("LabelIndexSeek(") != String::npos);

  EXPECT_GE(CountSubstr(s, "Scan("), 0u);
}

TEST(BuildPhysicalPlanComplex, MatchExpandSetMapsToExpandAndSet) {
  ast::QueryAST q;

  q.match_clause = std::make_unique<ast::MatchClause>();
  q.match_clause->patterns.push_back(
      MakeEdgePattern("a", "e", "b", "KNOWS", ast::EdgeDirection::Right)
  );

  q.set_clause = std::make_unique<ast::SetClause>();
  q.set_clause->items = {ast::SetItem{"a", {std::make_pair("age", ast::Literal{42})}, {}}};

  graph::exec::ExecContext ctx;
  graph::planner::Planner planner(ctx, std::move(q));

  planner.build_logical_plan();
  planner.build_physical_plan();

  const auto s = planner.getPhysicalPlan().DebugString();

  EXPECT_TRUE(s.find("Set(") != String::npos);
  EXPECT_TRUE(s.find("Expand(") != String::npos);
  EXPECT_TRUE(s.find("NodeScan(") != String::npos
           || s.find("LabelIndexSeek(") != String::npos);
}

static void SeedGraph1(storage::GraphDB& db) {
  db.create_node({"Person"}, {{"age", storage::Value{10}}});
  db.create_node({"Person"}, {{"age", storage::Value{40}}});
  db.create_node({"Person"}, {{"age", storage::Value{50}}});

  db.create_node({"City"}, {{"population", storage::Value{15}}});
  db.create_node({"City"}, {{"population", storage::Value{30}}});
}

static std::unique_ptr<ast::Expr> MakeAgeGreaterThanPopulationExpr() {
  auto left = std::make_unique<ast::PropertyExpr>();
  left->alias = "a";
  left->property = "age";

  auto right = std::make_unique<ast::PropertyExpr>();
  right->alias = "b";
  right->property = "population";

  auto cmp = std::make_unique<ast::ComparisonExpr>();
  cmp->left = std::move(left);
  cmp->op = ast::CompareOp::Gt;
  cmp->right = std::move(right);

  return cmp;
}


static void SeedGraph2(storage::GraphDB& db) {
  auto p1 = db.create_node({"Person"}, {{"age", storage::Value{10}}});
  auto p2 = db.create_node({"Person"}, {{"age", storage::Value{40}}});
  auto p3 = db.create_node({"Person"}, {{"age", storage::Value{50}}});
  auto c1 = db.create_node({"City"}, {{"population", storage::Value{100}}});

  db.create_edge(p1, p2, "KNOWS", {});
  db.create_edge(p2, p3, "KNOWS", {});
  db.create_edge(p3, p1, "KNOWS", {});
  db.create_edge(p1, c1, "LIVES_IN", {});
}
TEST(ExecutionComplex, MatchExpandJoinProjectLimit2) {
  storage::GraphDB db;
  SeedGraph2(db);

  ast::QueryAST q;

  q.match_clause = std::make_unique<ast::MatchClause>();
  q.match_clause->patterns.push_back(
      MakeEdgePattern("a", "e", "b", "KNOWS", ast::EdgeDirection::Right)
  );
  q.match_clause->patterns.push_back(MakeNodePattern("c", {"City"}));

  q.return_clause = std::make_unique<ast::ReturnClause>();
  q.return_clause->items.push_back(ast::ReturnItem{std::string{"a"}});
  q.return_clause->items.push_back(ast::ReturnItem{std::string{"e"}});
  q.return_clause->items.push_back(ast::ReturnItem{std::string{"b"}});

  q.limit = std::make_unique<ast::LimitClause>();
  q.limit->limit = 3;

  graph::exec::ExecContext ctx;
  ctx.db = &db; // если у тебя уже есть ExecContext(&db), можно оставить тот вариант

  graph::planner::Planner planner(ctx, std::move(q));

  planner.build_logical_plan();
  planner.build_physical_plan();

  auto cursor = planner.getPhysicalPlan().root->open(ctx);

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

    ASSERT_TRUE(std::find(a->labels.begin(), a->labels.end(), "Person") != a->labels.end());
    ASSERT_TRUE(std::find(b->labels.begin(), b->labels.end(), "City") != b->labels.end());

    const auto age = std::get<int64_t>(a->properties.at("age"));
    const auto population = std::get<int64_t>(b->properties.at("population"));
    EXPECT_GT(age, population);
    row.clear();
  }

  EXPECT_EQ(count, 3u);
}