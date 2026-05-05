#pragma once
#include "planner/query_planner.hpp"

using graph::String;

struct FakeExpr final : ast::Expr {
  explicit FakeExpr(String s) : s_(std::move(s)) {}

  graph::Value operator()(const ast::EvalContext&) const override {
    return false;
  }

  virtual Expr* copy() const override { return new FakeExpr(*this); }

  // Get the type of the expression.
  virtual ast::ExprType Type() const override { return ast::ExprType::Literal; }

  // Collects all aliases used in the expression into the provided vector.
  virtual void CollectAliases(std::vector<std::string>& aliases) const override {}

  std::string DebugString() const override {
    return s_;
  }


  String s_;
};

inline ast::Pattern MakeSingleNodePattern(const String& alias, std::initializer_list<String> labels = {}) {
  ast::NodePattern node;
  node.alias = alias;
  node.labels = labels;

  ast::PatternElement el;
  el.element = node;

  ast::Pattern pat;
  pat.elements.push_back(std::move(el));
  return pat;
}

inline ast::QueryAST MakeQueryWithMatch(const String& alias = "a") {
  ast::QueryAST q;
  q.match_clause = std::make_unique<ast::MatchClause>();
  q.match_clause->patterns.push_back(MakeSingleNodePattern(alias, {"Person"}));
  return q;
}

inline void ExpectInOrder(const String& s, std::initializer_list<String> needles) {
  size_t pos = 0;
  for (const auto& needle : needles) {
    const size_t found = s.find(needle, pos);
    ASSERT_NE(found, String::npos) << "Missing substring: " << needle << "\nIn string:\n" << s;
    pos = found + needle.size();
  }
}

inline graph::planner::Planner MakePlanner(ast::QueryAST q) {
  graph::exec::ExecContext ctx;
  ctx.db = nullptr;
  return graph::planner::Planner(ctx, nullptr, std::move(q));
}

using graph::String;

inline size_t CountSubstr(const String& s, const String& needle) {
  size_t count = 0;
  size_t pos = 0;
  while ((pos = s.find(needle, pos)) != String::npos) {
    ++count;
    pos += needle.size();
  }
  return count;
}

inline ast::Pattern MakeNodePattern(const String& alias, std::initializer_list<String> labels = {}) {
  ast::NodePattern node;
  node.alias = alias;
  node.labels = labels;

  ast::PatternElement el;
  el.element = node;

  ast::Pattern pat;
  pat.elements.push_back(std::move(el));
  return pat;
}

inline ast::Pattern MakeEdgePattern(const String& left_alias,
                             const String& edge_alias,
                             const String& right_alias,
                             const String& label,
                             ast::EdgeDirection dir) {
  ast::NodePattern left;
  left.alias = left_alias;

  ast::MatchEdgePattern edge;
  edge.alias = edge_alias;
  edge.label = label;
  edge.direction = dir;

  ast::NodePattern right;
  right.alias = right_alias;

  ast::Pattern pat;

  ast::PatternElement e1;
  e1.element = left;
  pat.elements.push_back(std::move(e1));

  ast::PatternElement e2;
  e2.element = edge;
  pat.elements.push_back(std::move(e2));

  ast::PatternElement e3;
  e3.element = right;
  pat.elements.push_back(std::move(e3));

  return pat;
}

inline ast::QueryAST MakeSelectQueryTwoPatterns() {
  ast::QueryAST q;
  q.match_clause = std::make_unique<ast::MatchClause>();
  q.match_clause->patterns.push_back(MakeNodePattern("a", {"Person"}));
  q.match_clause->patterns.push_back(MakeNodePattern("b", {"City"}));

  q.return_clause = std::make_unique<ast::ReturnClause>();
  q.return_clause->items.push_back(ast::ReturnItem{std::string{"a"}});
  q.return_clause->items.push_back(ast::ReturnItem{std::string{"b"}});
  return q;
}

inline graph::planner::Planner MakePlanner(ast::QueryAST q, storage::GraphDB* db = nullptr) {
  graph::exec::ExecContext ctx;
  ctx.db = db;
  return graph::planner::Planner(ctx, db, std::move(q));
}

inline ast::ExprPtr MakeBoolLiteral(bool v) {
  auto expr = std::make_unique<ast::LiteralExpr>();
  expr->literal = ast::Literal{v};
  return expr;
}


//// TODO TESTS FOR RESIDIAL HASH
// TEST(HashJoinComplex, SelfJoinSameCityWithAgeOrderingUsesHashJoin) {
//   storage::GraphDB db;
//   graph::exec::ExecContext create_ctx(&db);
//
//   ast::QueryAST create_q;
//   create_q.create_clause = std::make_unique<ast::CreateClause>();
//
//   // 12 people, 4 cities, in each city 3 ages:
//   // city 0: 20, 24, 28
//   // city 1: 21, 25, 29
//   // city 2: 22, 26, 30
//   // city 3: 23, 27, 31
//   // For each city, ordered pairs with age(left) > age(right) are C(3,2)=3.
//   // Total expected rows = 4 * 3 = 12.
//   for (int i = 0; i < 12; ++i) {
//     AddCreatedNode(
//         *create_q.create_clause,
//         "p" + std::to_string(i),
//         "Person",
//         {
//             {"city_id", ast::Literal{i % 4}},
//             {"age", ast::Literal{20 + i}}
//         }
//     );
//   }
//
//   {
//     graph::planner::Planner creator(create_ctx, &db, std::move(create_q));
//     ExecuteAndConsume(creator, create_ctx);
//   }
//
//   ASSERT_EQ(db.node_count_with_label("Person"), 12u);
//
//   auto left = MakeScan("a", {"Person"});
//   auto right = MakeScan("b", {"Person"});
//
//   auto join_pred = MakeAnd(
//     MakeGt(MakeProp("a", "age"), MakeProp("b", "age")),
//     MakeEq(MakeProp("a", "city_id"), MakeProp("b", "city_id"))
//   );
//   auto join = std::make_unique<logical::LogicalJoin>(std::move(left), std::move(right), std::move(join_pred));
//
//   auto plan = logical::LogicalPlan(
//       std::move(join)
//   );
//
//   optimizer::optimize_logical_plan_impl(plan.root);
//
//   MockCostModel cm;
//   exec::ExecContext ctx(&db);
//   auto [root, cost] = plan.root->BuildPhysical(ctx, &cm, &db);
//   ASSERT_NE(root, nullptr);
//
//   auto cursor = root->open(ctx);
//   graph::exec::Row row;
//   size_t count = 0;
//
//   while (cursor->next(row)) {
//     ++count;
//     ASSERT_EQ(row.slots.size(), 2u);
//     ASSERT_TRUE(std::holds_alternative<storage::Node*>(row.slots[0]));
//     ASSERT_TRUE(std::holds_alternative<storage::Node*>(row.slots[1]));
//
//     auto* a = std::get<storage::Node*>(row.slots[0]);
//     auto* b = std::get<storage::Node*>(row.slots[1]);
//     ASSERT_NE(a, nullptr);
//     ASSERT_NE(b, nullptr);
//
//     EXPECT_EQ(std::get<int64_t>(a->properties.at("city_id")),
//               std::get<int64_t>(b->properties.at("city_id")));
//     EXPECT_GT(std::get<int64_t>(a->properties.at("age")),
//               std::get<int64_t>(b->properties.at("age")));
//
//     row.clear();
//   }
//
//   EXPECT_EQ(count, 12u);
//
//   auto* hash = dynamic_cast<exec::HashJoinOp*>(root.get());
//   if (hash == nullptr) {
//     auto* unary = dynamic_cast<exec::PhysicalOpUnaryChild*>(root.get());
//     ASSERT_NE(unary, nullptr);
//     hash = dynamic_cast<exec::HashJoinOp*>(unary->child.get());
//   }
//   ASSERT_NE(hash, nullptr);
// }
//
// TEST(HashJoinComplex, FactToDimensionJoinWithRightSideFilterUsesHashJoin) {
//   storage::GraphDB db;
//   graph::exec::ExecContext create_ctx(&db);
//
//   ast::QueryAST create_q;
//   create_q.create_clause = std::make_unique<ast::CreateClause>();
//
//   // Dimension table: 8 sessions, active for even sid.
//   for (int i = 0; i < 8; ++i) {
//     AddCreatedNode(
//         *create_q.create_clause,
//         "s" + std::to_string(i),
//         "Session",
//         {
//             {"sid", ast::Literal{i}},
//             {"active", ast::Literal{(i % 2) == 0}}
//         }
//     );
//   }
//
//   // Fact table: 20 logs, session_id = i % 8
//   // Even sessions are 0,2,4,6, so exactly 10 logs should survive the right-side active filter.
//   for (int i = 0; i < 20; ++i) {
//     AddCreatedNode(
//         *create_q.create_clause,
//         "l" + std::to_string(i),
//         "Log",
//         {
//             {"session_id", ast::Literal{i % 8}},
//             {"severity", ast::Literal{i % 3}},
//             {"ts", ast::Literal{i}}
//         }
//     );
//   }
//
//   {
//     graph::planner::Planner creator(create_ctx, &db, std::move(create_q));
//     ExecuteAndConsume(creator, create_ctx);
//   }
//
//   ASSERT_EQ(db.node_count_with_label("Session"), 8u);
//   ASSERT_EQ(db.node_count_with_label("Log"), 20u);
//
//   auto left = MakeScan("a", {"Log"});
//   auto right = MakeScan("b", {"Session"});
//
//   auto join_pred = MakeEq(MakeProp("a", "session_id"), MakeProp("b", "sid"));
//   auto join = std::make_unique<logical::LogicalJoin>(std::move(left), std::move(right), std::move(join_pred));
//
//   // Right-side filter that should be pushed down below join.
//   auto active_only = MakeEq(MakeProp("b", "active"), MakeBoolLiteral(true));
//   auto plan = logical::LogicalPlan(
//       std::make_unique<logical::LogicalFilter>(std::move(join), std::move(active_only))
//   );
//
//   optimizer::optimize_logical_plan_impl(plan.root);
//
//   MockCostModel cm;
//   exec::ExecContext ctx(&db);
//   auto [root, cost] = plan.root->BuildPhysical(ctx, &cm, &db);
//   ASSERT_NE(root, nullptr);
//
//   auto cursor = root->open(ctx);
//   graph::exec::Row row;
//   size_t count = 0;
//
//   while (cursor->next(row)) {
//     ++count;
//     ASSERT_EQ(row.slots.size(), 2u);
//     ASSERT_TRUE(std::holds_alternative<storage::Node*>(row.slots[0]));
//     ASSERT_TRUE(std::holds_alternative<storage::Node*>(row.slots[1]));
//
//     auto* log = std::get<storage::Node*>(row.slots[0]);
//     auto* session = std::get<storage::Node*>(row.slots[1]);
//     ASSERT_NE(log, nullptr);
//     ASSERT_NE(session, nullptr);
//
//     EXPECT_EQ(std::get<int64_t>(log->properties.at("session_id")),
//               std::get<int64_t>(session->properties.at("sid")));
//     EXPECT_TRUE(std::get<bool>(session->properties.at("active")));
//
//     row.clear();
//   }
//
//   EXPECT_EQ(count, 10u);
//
//   auto* hash = dynamic_cast<exec::HashJoinOp*>(root.get());
//   if (hash == nullptr) {
//     auto* unary = dynamic_cast<exec::PhysicalOpUnaryChild*>(root.get());
//     ASSERT_NE(unary, nullptr);
//     hash = dynamic_cast<exec::HashJoinOp*>(unary->child.get());
//   }
//   ASSERT_NE(hash, nullptr);
// }
