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
  q.return_clause->items.push_back(ast::ReturnItem{std::string{"c"}});

  q.limit_clause = std::make_unique<ast::LimitClause>();
  q.limit_clause->limit = 3;

  graph::exec::ExecContext ctx(&db);
  graph::planner::Planner planner(ctx, std::move(q));

  planner.build_logical_plan();
  planner.build_physical_plan();

  auto cursor = planner.getPhysicalPlan().root->open(ctx);

  graph::exec::Row row;
  size_t count = 0;

  while (cursor->next(row)) {
    ++count;
    ASSERT_EQ(row.slots.size(), 4u);
    ASSERT_TRUE(std::holds_alternative<storage::Node*>(row.slots[0]));
    ASSERT_TRUE(std::holds_alternative<storage::Edge*>(row.slots[1]));
    ASSERT_TRUE(std::holds_alternative<storage::Node*>(row.slots[2]));
    ASSERT_TRUE(std::holds_alternative<storage::Node*>(row.slots[3]));
    row.clear();
  }

  EXPECT_EQ(count, 3u);
}

TEST(ExecutionComplex, MatchSetAddsProperty2) {
  storage::GraphDB db;
  auto node_id = db.create_node({"Person"}, {{"age", storage::Value{21}}});

  ast::QueryAST q;
  q.match_clause = std::make_unique<ast::MatchClause>();
  q.match_clause->patterns.push_back(MakeNodePattern("a", {"Person"}));

  q.set_clause = std::make_unique<ast::SetClause>();
  ast::SetItem item;
  item.alias = "a";
  item.properties = {{"VIP", ast::Literal{true}}};
  q.set_clause->items.push_back(std::move(item));

  q.return_clause = std::make_unique<ast::ReturnClause>();
  q.return_clause->items.push_back(ast::ReturnItem{std::string{"a"}});

  graph::exec::ExecContext ctx(&db);
  graph::planner::Planner planner(ctx, std::move(q));

  planner.build_logical_plan();
  planner.build_physical_plan();

  auto cursor = planner.getPhysicalPlan().root->open(ctx);

  graph::exec::Row row;
  while (cursor->next(row)) {
    row.clear();
  }

  auto* node = db.get_node(node_id);
  ASSERT_NE(node, nullptr);
  ASSERT_TRUE(node->properties.contains("VIP"));
  EXPECT_EQ(std::get<bool>(node->properties.at("VIP")), true);
}

TEST(ExecutionComplex, MatchDeleteRemovesNode2) {
  storage::GraphDB db;
  db.create_node({"Person"}, {{"age", storage::Value{21}}});

  ast::QueryAST q;
  q.match_clause = std::make_unique<ast::MatchClause>();
  q.match_clause->patterns.push_back(MakeNodePattern("a", {"Person"}));

  q.delete_clause = std::make_unique<ast::DeleteClause>();
  q.delete_clause->aliases = {"a"};

  graph::exec::ExecContext ctx(&db);
  graph::planner::Planner planner(ctx, std::move(q));

  planner.build_logical_plan();
  planner.build_physical_plan();

  auto cursor = planner.getPhysicalPlan().root->open(ctx);
  graph::exec::Row row;
  while (cursor->next(row)) {
    row.clear();
  }

  EXPECT_EQ(db.node_count_with_label("Person"), 0u);
}

TEST(ExecutionComplex, CreateNodesAndEdge2) {
  storage::GraphDB db;

  ast::QueryAST q;
  q.create_clause = std::make_unique<ast::CreateClause>();

  ast::NodePattern a;
  a.alias = "a";
  a.labels = {"Person"};

  ast::NodePattern b;
  b.alias = "b";
  b.labels = {"City"};

  ast::CreateEdgePattern e;
  e.left_node.alias = "a";
  e.right_node.alias = "b";
  e.alias = "e";
  e.label = "LIVES_IN";
  e.direction = ast::EdgeDirection::Right;

  q.create_clause->created_items.emplace_back(a);
  q.create_clause->created_items.emplace_back(b);
  q.create_clause->created_items.emplace_back(e);

  graph::exec::ExecContext ctx(&db);
  graph::planner::Planner planner(ctx, std::move(q));

  planner.build_logical_plan();
  planner.build_physical_plan();

  std::cout << planner.getPhysicalPlan().DebugString() << std::endl;

  auto cursor = planner.getPhysicalPlan().root->open(ctx);
  graph::exec::Row row;
  while (cursor->next(row)) {
    row.clear();
  }

  EXPECT_EQ(db.node_count(), 2u);
  EXPECT_EQ(db.node_count_with_label("Person"), 1u);
  EXPECT_EQ(db.node_count_with_label("City"), 1u);
}
