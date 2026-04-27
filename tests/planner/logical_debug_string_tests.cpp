#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "planner.hpp"

namespace {
using graph::String;

struct TestLeafOp final : graph::logical::LogicalOpZeroChild {
  explicit TestLeafOp(String debug) : debug_(std::move(debug)) {}

  String DebugString() const override {
    return debug_;
  }

  graph::logical::BuildPhysicalType BuildPhysical(
      graph::exec::ExecContext&,
      graph::planner::CostModel*,
      storage::GraphDB*) const override {
    return {nullptr, graph::planner::CostEstimate{}};
  }

  String debug_;
};

struct TestUnaryOp final : graph::logical::LogicalOpUnaryChild {
  explicit TestUnaryOp(graph::logical::LogicalOpPtr child, String debug)
      : LogicalOpUnaryChild(std::move(child)), debug_(std::move(debug)) {}

  String DebugString() const override {
    return debug_;
  }

  graph::logical::BuildPhysicalType BuildPhysical(
      graph::exec::ExecContext&,
      graph::planner::CostModel*,
      storage::GraphDB*) const override {
    return {nullptr, graph::planner::CostEstimate{}};
  }

  String debug_;
};

struct TestBinaryOp final : graph::logical::LogicalOpBinaryChild {
  TestBinaryOp(graph::logical::LogicalOpPtr left,
               graph::logical::LogicalOpPtr right,
               String debug)
      : LogicalOpBinaryChild(std::move(left), std::move(right)),
        debug_(std::move(debug)) {}

  String DebugString() const override {
    return debug_;
  }

  graph::logical::BuildPhysicalType BuildPhysical(
      graph::exec::ExecContext&,
      graph::planner::CostModel*,
      storage::GraphDB*) const override {
    return {nullptr, graph::planner::CostEstimate{}};
  }

  String debug_;
};

struct FakeExpr final : ast::Expr {
  explicit FakeExpr(String s) : s_(std::move(s)) {}

  graph::Value operator()(const ast::EvalContext&) const override {
    return false;
  }

  std::string DebugString() const override {
    return s_;
  }

  String s_;
};
} // namespace

TEST(LogicalDebugString, ZeroChildReturnsOwnDebugString) {
  TestLeafOp op("Leaf");
  EXPECT_EQ(op.DebugString(), "Leaf");
  EXPECT_EQ(op.SubtreeDebugString(), "Leaf");
}

TEST(LogicalDebugString, UnarySubtreeIsIndented) {
  auto child = std::make_unique<TestLeafOp>("Child");
  TestUnaryOp op(std::move(child), "Unary");

  EXPECT_EQ(op.DebugString(), "Unary");
  EXPECT_EQ(op.SubtreeDebugString(), "Unary\n  Child");
}

TEST(LogicalDebugString, BinarySubtreeHasLeftAndRightMarkers) {
  auto left = std::make_unique<TestLeafOp>("Left");
  auto right = std::make_unique<TestLeafOp>("Right");
  TestBinaryOp op(std::move(left), std::move(right), "Binary");

  EXPECT_EQ(op.DebugString(), "Binary");
  EXPECT_EQ(op.SubtreeDebugString(), "Binary\n1)\n  Left\n2)\n  Right");
}

TEST(LogicalDebugString, LogicalPlanIsStable) {
  graph::logical::LogicalPlan plan;
  EXPECT_EQ(plan.DebugString(), "LogicalPlan:");
  EXPECT_EQ(plan.SubtreeDebugString(), "<empty>");
}

TEST(LogicalDebugString, LogicalScanWithoutLabelsAndProperties) {
  graph::logical::LogicalScan scan({}, "n");

  EXPECT_EQ(scan.DebugString(), "LogicalScan(as=n)");
  EXPECT_EQ(scan.SubtreeDebugString(), "LogicalScan(as=n)");
}

TEST(LogicalDebugString, LogicalScanWithLabelsAndProperties) {
  graph::logical::LogicalScan scan(
      {"Person", "Employee"},
      "n",
      {{"age", graph::Value{42}}}
  );

  EXPECT_EQ(
      scan.DebugString(),
      "LogicalScan(as=n, labels=[Person, Employee], properties={age: 42})"
  );
}

TEST(LogicalDebugString, LogicalExpandDebugString) {
  auto child = std::make_unique<TestLeafOp>("Scan");
  graph::logical::LogicalExpand op(
      std::move(child),
      "a",
      "e",
      "b",
      std::optional<String>{"LIKES"},
      {"Person"},
      {},
      ast::EdgeDirection::Right
  );

  EXPECT_EQ(
      op.DebugString(),
      "LogicalExpand(a->b, edge_as=e, edge_label= LIKES, dst_labels=[Person])"
  );
  EXPECT_EQ(
      op.SubtreeDebugString(),
      "LogicalExpand(a->b, edge_as=e, edge_label= LIKES, dst_labels=[Person])\n  Scan"
  );
}

TEST(LogicalDebugString, LogicalFilterDebugStringUsesPredicate) {
  auto child = std::make_unique<TestLeafOp>("Child");
  auto pred = std::make_unique<FakeExpr>("a.age > 18");

  graph::logical::LogicalFilter op(std::move(child), std::move(pred));

  EXPECT_EQ(op.DebugString(), "LogicalFilter(a.age > 18)");
  EXPECT_EQ(op.SubtreeDebugString(), "LogicalFilter(a.age > 18)\n  Child");
}

TEST(LogicalDebugString, LogicalProjectWithAliases) {
  auto child = std::make_unique<TestLeafOp>("Child");
  std::vector<ast::ReturnItem> items;
  items.push_back(ast::ReturnItem{std::string{"a"}});
  items.push_back(ast::ReturnItem{std::string{"b"}});

  graph::logical::LogicalProject op(std::move(child), std::move(items));

  EXPECT_EQ(op.DebugString(), "Project(a, b)");
  EXPECT_EQ(op.SubtreeDebugString(), "Project(a, b)\n  Child");
}

TEST(LogicalDebugString, LogicalJoinCrossDebugString) {
  auto left = std::make_unique<TestLeafOp>("Left");
  auto right = std::make_unique<TestLeafOp>("Right");
  graph::logical::LogicalJoin op(std::move(left), std::move(right));

  EXPECT_EQ(op.DebugString(), "Join(cross)");
  EXPECT_EQ(op.SubtreeDebugString(), "Join(cross)\n1)\n  Left\n2)\n  Right");
}

TEST(LogicalDebugString, LogicalJoinWithPredicate) {
  auto left = std::make_unique<TestLeafOp>("Left");
  auto right = std::make_unique<TestLeafOp>("Right");
  auto pred = std::make_unique<FakeExpr>("a.id = b.id");

  graph::logical::LogicalJoin op(std::move(left), std::move(right), std::move(pred));

  EXPECT_EQ(op.DebugString(), "Join(on=a.id = b.id)");
}

TEST(LogicalDebugString, LogicalSetDebugString) {
  auto child = std::make_unique<TestLeafOp>("Child");
  graph::logical::LogicalSet::Assignment ass{"n", {}, {std::make_pair("age", graph::Value(21))}};
  graph::logical::LogicalSet op(
      std::move(child),
      {ass}
  );

  EXPECT_EQ(op.DebugString(), "LogicalSet(alias=n, labels=, properties={age=21})");
  EXPECT_EQ(op.SubtreeDebugString(), "LogicalSet(alias=n, labels=, properties={age=21})\n  Child");
}

TEST(LogicalDebugString, LogicalDeleteDebugString) {
  auto child = std::make_unique<TestLeafOp>("Child");
  graph::logical::LogicalDelete op(
      std::move(child),
      {"n", "m"}
  );

  EXPECT_EQ(op.DebugString(), "LogicalDelete(n, m)");
  EXPECT_EQ(op.SubtreeDebugString(), "LogicalDelete(n, m)\n  Child");
}

TEST(LogicalDebugString, CreateNodeSpecDebugStringHasExpectedParts) {
  ast::NodePattern p;
  p.labels = {"Person", "Employee"};
  p.properties = {std::make_pair("age", ast::Literal{graph::Value{42}})};

  graph::logical::CreateNodeSpec spec(p);

  EXPECT_THAT(spec.DebugString(), ::testing::HasSubstr("NodeCreate("));
  EXPECT_THAT(spec.DebugString(), ::testing::HasSubstr("Person"));
  EXPECT_THAT(spec.DebugString(), ::testing::HasSubstr("Employee"));
  EXPECT_THAT(spec.DebugString(), ::testing::HasSubstr("age"));
}

TEST(LogicalDebugString, CreateEdgeSpecDebugStringHasExpectedParts) {
  ast::CreateEdgePattern p;
  p.left_node.alias = "a";
  p.right_node.alias = "b";
  p.alias = "e";
  p.label = "LIKES";
  p.direction = ast::EdgeDirection::Right;
  p.properties = {{"weight", ast::Literal{graph::Value{3}}}};

  graph::logical::CreateEdgeSpec spec(p);

  EXPECT_THAT(spec.DebugString(), ::testing::HasSubstr("EdgeCreate("));
  EXPECT_THAT(spec.DebugString(), ::testing::HasSubstr("a"));
  EXPECT_THAT(spec.DebugString(), ::testing::HasSubstr("b"));
  EXPECT_THAT(spec.DebugString(), ::testing::HasSubstr("LIKES"));
  EXPECT_THAT(spec.DebugString(), ::testing::HasSubstr("weight"));
}

TEST(LogicalDebugString, LogicalCreateDebugStringContainsAllItems) {
  ast::NodePattern np;
  np.labels = {"Person"};
  np.properties = {{"age", ast::Literal{graph::Value{42}}}};

  ast::CreateEdgePattern ep;
  ep.left_node.alias = "a";
  ep.right_node.alias = "b";
  ep.alias = "e";
  ep.label = "KNOWS";
  ep.direction = ast::EdgeDirection::Right;

  std::vector<ast::CreateItem> items;
  items.emplace_back(np);
  items.emplace_back(ep);

  auto child = std::make_unique<TestLeafOp>("Child");
  graph::logical::LogicalCreate op(std::move(child), items);

  EXPECT_THAT(op.DebugString(), ::testing::HasSubstr("LogicalCreate("));
  EXPECT_THAT(op.DebugString(), ::testing::HasSubstr("NodeCreate("));
  EXPECT_THAT(op.DebugString(), ::testing::HasSubstr("EdgeCreate("));
  EXPECT_EQ(op.SubtreeDebugString().substr(0, 14), "LogicalCreate(");
}

TEST(LogicalDebugString, LogicalProjectWithPropertyExpr) {
  auto child = std::make_unique<TestLeafOp>("Child");

  ast::PropertyExpr prop;
  prop.alias = "a";
  prop.property = "age";

  std::vector<ast::ReturnItem> items;
  items.push_back(ast::ReturnItem{prop});

  graph::logical::LogicalProject op(std::move(child), items);

  EXPECT_EQ(
      op.DebugString(),
      "Project(Property(a.age))"
  );
  EXPECT_EQ(
      op.SubtreeDebugString(),
      "Project(Property(a.age))\n  Child"
  );
}

TEST(LogicalDebugString, LogicalProjectMixedItems) {
  auto child = std::make_unique<TestLeafOp>("Child");

  ast::PropertyExpr prop;
  prop.alias = "a";
  prop.property = "age";

  std::vector<ast::ReturnItem> items;
  items.push_back(ast::ReturnItem{std::string{"a"}});
  items.push_back(ast::ReturnItem{prop});

  graph::logical::LogicalProject op(std::move(child), items);

  EXPECT_EQ(
      op.DebugString(),
      "Project(a, Property(a.age))"
  );
}

TEST(LogicalDebugString, LogicalSortSingleKeyAsc) {
  auto child = std::make_unique<TestLeafOp>("Child");

  ast::PropertyExpr prop;
  prop.alias = "a";
  prop.property = "age";

  ast::OrderItem item;
  item.property = prop;
  item.direction = ast::OrderDirection::Asc;

  graph::logical::LogicalSort op(std::move(child), {item});

  EXPECT_EQ(
      op.DebugString(),
      "Sort(Property(a.age) ASC)"
  );
  EXPECT_EQ(
      op.SubtreeDebugString(),
      "Sort(Property(a.age) ASC)\n  Child"
  );
}

TEST(LogicalDebugString, LogicalSortMultipleKeys) {
  auto child = std::make_unique<TestLeafOp>("Child");

  ast::PropertyExpr p1;
  p1.alias = "a";
  p1.property = "age";

  ast::PropertyExpr p2;
  p2.alias = "b";
  p2.property = "name";

  ast::OrderItem k1{p1, ast::OrderDirection::Asc};
  ast::OrderItem k2{p2, ast::OrderDirection::Desc};

  graph::logical::LogicalSort op(std::move(child), {k1, k2});

  EXPECT_EQ(
      op.DebugString(),
      "Sort(Property(a.age) ASC, Property(b.name) DESC)"
  );
}