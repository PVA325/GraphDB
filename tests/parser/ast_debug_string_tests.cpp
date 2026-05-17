#include <gtest/gtest.h>

#include "parser/ast.hpp"
#include "parser/expression.hpp"
#include "parser/pattern.hpp"
#include "parser/value.hpp"

using namespace ast;

TEST(DebugStringTest, LiteralInt) {
  Literal lit{42};
  EXPECT_EQ(lit.DebugString(), "Literal(42)");
}

TEST(DebugStringTest, LiteralDouble) {
  Literal lit{3.14};
  EXPECT_EQ(lit.DebugString(), "Literal(3.14)");
}

TEST(DebugStringTest, LiteralBool) {
  Literal lit{true};
  EXPECT_EQ(lit.DebugString(), "Literal(true)");
}

TEST(DebugStringTest, LiteralString) {
  Literal lit{std::string("hello")};
  EXPECT_EQ(lit.DebugString(), R"(Literal("hello"))");
}

TEST(DebugStringTest, LiteralExpr) {
  LiteralExpr expr{Literal{123}};
  EXPECT_EQ(expr.DebugString(), "Literal(123)");
}

TEST(DebugStringTest, PropertyExpr) {
  PropertyExpr expr;
  expr.alias = "n";
  expr.property = "age";

  EXPECT_EQ(expr.DebugString(), "Property(n.age)");
}

TEST(DebugStringTest, ComparisonExpr) {
  auto lhs = std::make_unique<PropertyExpr>();
  lhs->alias = "n";
  lhs->property = "age";

  auto rhs = std::make_unique<LiteralExpr>();
  rhs->literal = Literal{18};

  ComparisonExpr expr(
      std::move(lhs),
      CompareOp::Ge,
      std::move(rhs));

  EXPECT_EQ(
      expr.DebugString(),
      "Compare(Property(n.age) >= Literal(18))");
}

TEST(DebugStringTest, LogicalExpr) {
  auto left_cmp = std::make_unique<ComparisonExpr>(
      std::make_unique<PropertyExpr>(
          PropertyExpr{"n", "age"}),
      CompareOp::Gt,
      std::make_unique<LiteralExpr>(
          LiteralExpr{Literal{18}}));

  auto right_cmp = std::make_unique<ComparisonExpr>(
      std::make_unique<PropertyExpr>(
          PropertyExpr{"n", "active"}),
      CompareOp::Eq,
      std::make_unique<LiteralExpr>(
          LiteralExpr{Literal{true}}));

  LogicalExpr expr(
      std::move(left_cmp),
      LogicalOp::And,
      std::move(right_cmp));

  EXPECT_EQ(
      expr.DebugString(),
      "Logical(Compare(Property(n.age) > Literal(18)) "
      "AND "
      "Compare(Property(n.active) = Literal(true)))");
}

TEST(DebugStringTest, NodePatternEmpty) {
  NodePattern node;
  EXPECT_EQ(node.DebugString(), "()");
}

TEST(DebugStringTest, NodePatternAliasOnly) {
  NodePattern node;
  node.alias = "n";

  EXPECT_EQ(node.DebugString(), "(n)");
}

TEST(DebugStringTest, NodePatternWithLabels) {
  NodePattern node;
  node.alias = "n";
  node.labels = {"Person", "Employee"};

  EXPECT_EQ(node.DebugString(), "(n:Person:Employee)");
}

TEST(DebugStringTest, NodePatternWithProperties) {
  NodePattern node;
  node.alias = "n";
  node.properties = {
      {"name", Literal{std::string("Alice")}},
      {"age", Literal{30}}
  };

  EXPECT_EQ(
      node.DebugString(),
      R"((n {name: "Alice", age: 30}))");
}

TEST(DebugStringTest, MatchEdgePatternRight) {
  MatchEdgePattern edge;
  edge.alias = "e";
  edge.label = "KNOWS";
  edge.direction = EdgeDirection::Right;

  EXPECT_EQ(
      edge.DebugString(),
      "-[e:KNOWS]->");
}

TEST(DebugStringTest, MatchEdgePatternLeft) {
  MatchEdgePattern edge;
  edge.alias = "e";
  edge.label = "KNOWS";
  edge.direction = EdgeDirection::Left;

  EXPECT_EQ(
      edge.DebugString(),
      "<-[e:KNOWS]-");
}

TEST(DebugStringTest, MatchEdgePatternWithProperties) {
  MatchEdgePattern edge;
  edge.alias = "e";
  edge.label = "KNOWS";
  edge.direction = EdgeDirection::Right;

  edge.properties = {
      {"since", Literal{2020}}
  };

  EXPECT_EQ(
      edge.DebugString(),
      "-[e:KNOWS {since: 2020}]->");
}

TEST(DebugStringTest, PatternDebugString) {
  Pattern pattern;

  NodePattern left;
  left.alias = "a";

  MatchEdgePattern edge;
  edge.label = "KNOWS";
  edge.direction = EdgeDirection::Right;

  NodePattern right;
  right.alias = "b";

  pattern.elements.push_back(PatternElement{left});
  pattern.elements.push_back(PatternElement{edge});
  pattern.elements.push_back(PatternElement{right});

  EXPECT_EQ(
      pattern.DebugString(),
      "(a)-[:KNOWS]->(b)");
}

TEST(DebugStringTest, ReturnClause) {
  ReturnClause clause;

  ReturnItem item1;
  item1.return_item = std::string("n");

  PropertyExpr prop;
  prop.alias = "n";
  prop.property = "name";

  ReturnItem item2;
  item2.return_item = prop;

  clause.items = {item1, item2};

  EXPECT_EQ(
      clause.DebugString(),
      "RETURN n, Property(n.name)");
}

TEST(DebugStringTest, DeleteClause) {
  DeleteClause clause;
  clause.aliases = {"n", "m"};

  EXPECT_EQ(
      clause.DebugString(),
      "DELETE n, m");
}

TEST(DebugStringTest, LimitClause) {
  LimitClause clause(25);

  EXPECT_EQ(
      clause.DebugString(),
      "LIMIT 25");
}

TEST(DebugStringTest, CreateNodeRef) {
  CreateNodeRef ref;
  ref.alias = "n";

  EXPECT_EQ(
      ref.DebugString(),
      "(n)");
}

TEST(DebugStringTest, CreateEdgePattern) {
  CreateEdgePattern edge;

  edge.left_node.alias = "a";
  edge.right_node.alias = "b";

  edge.alias = "e";
  edge.label = "KNOWS";
  edge.direction = EdgeDirection::Right;

  EXPECT_EQ(
      edge.DebugString(),
      "(a)-[e:KNOWS]->(b)");
}

TEST(DebugStringTest, MatchClause) {
  MatchClause clause;

  Pattern pattern;

  NodePattern node;
  node.alias = "n";

  pattern.elements.push_back(PatternElement{node});

  clause.patterns.push_back(pattern);

  EXPECT_EQ(
      clause.DebugString(),
      "MATCH (n)");
}

TEST(DebugStringTest, WhereClause) {
  auto lhs = std::make_unique<PropertyExpr>();
  lhs->alias = "n";
  lhs->property = "age";

  auto rhs = std::make_unique<LiteralExpr>();
  rhs->literal = Literal{18};

  WhereClause clause;
  clause.expression = std::make_unique<ComparisonExpr>(
      std::move(lhs),
      CompareOp::Gt,
      std::move(rhs));

  EXPECT_EQ(
      clause.DebugString(),
      "WHERE Compare(Property(n.age) > Literal(18))");
}

TEST(DebugStringTest, QueryASTFull) {
  QueryAST query;

  query.match_clause = std::make_unique<MatchClause>();
  query.return_clause = std::make_unique<ReturnClause>();
  query.limit_clause = std::make_unique<LimitClause>();

  Pattern pattern;
  NodePattern node;
  node.alias = "n";

  pattern.elements.push_back(PatternElement{node});
  query.match_clause->patterns.push_back(pattern);

  ReturnItem item;
  item.return_item = std::string("n");

  query.return_clause->items.push_back(item);

  query.limit_clause->limit = 10;

  EXPECT_EQ(
      query.DebugString(),
      "MATCH (n)\n"
      "RETURN n\n"
      "LIMIT 10\n");
}
