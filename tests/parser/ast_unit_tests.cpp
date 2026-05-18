#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "parser/ast_utility.hpp"
#include "parser/clause.hpp"
#include "parser/error.hpp"
#include "parser/expression.hpp"
#include "parser/lexer.hpp"
#include "parser/parser.hpp"
#include "parser/pattern.hpp"
#include "parser/token.hpp"
#include "parser/value.hpp"

namespace {

void ExpectContains(const std::string& haystack, const std::string& needle) {
  EXPECT_NE(haystack.find(needle), std::string::npos)
      << "Expected to find [" << needle << "] in [" << haystack << "]";
}

}  // namespace

TEST(ErrorTest, LexErrorMessage) {
  try {
    throw LexError(3, 15, "unexpected character");
  } catch (const std::runtime_error& e) {
    EXPECT_EQ(std::string(e.what()),
              "Line 3, Column 15: unexpected character");
  }
}

TEST(ErrorTest, ParseErrorMessage) {
  try {
    throw ParseError(1, 2, "expected identifier");
  } catch (const std::runtime_error& e) {
    EXPECT_EQ(std::string(e.what()),
              "Line 1, Column 2: expected identifier");
  }
}

TEST(ErrorTest, SemanticErrorMessage) {
  try {
    throw SemanticError(10, 7, "duplicate alias");
  } catch (const std::runtime_error& e) {
    EXPECT_EQ(std::string(e.what()),
              "Line 10, Column 7: duplicate alias");
  }
}

TEST(ValueTest, LiteralDebugStringIsNotEmpty) {
  ast::Literal lit{ast::Value{int64_t{42}}};
  EXPECT_FALSE(lit.DebugString().empty());
}

TEST(ValueTest, ValueToStringForBasicTypes) {
  EXPECT_FALSE(ast::ValueToString(ast::Value{int64_t{42}}).empty());
  EXPECT_FALSE(ast::ValueToString(ast::Value{3.14}).empty());
  EXPECT_FALSE(ast::ValueToString(ast::Value{std::string("abc")}).empty());
  EXPECT_FALSE(ast::ValueToString(ast::Value{true}).empty());
}

TEST(ValueTest, CompareOpToString) {
  EXPECT_EQ(ast::CompareOpToString(ast::CompareOp::Eq), "=");
  EXPECT_EQ(ast::CompareOpToString(ast::CompareOp::NotEqual), "!=");
  EXPECT_EQ(ast::CompareOpToString(ast::CompareOp::Gt), ">");
  EXPECT_EQ(ast::CompareOpToString(ast::CompareOp::Ge), ">=");
  EXPECT_EQ(ast::CompareOpToString(ast::CompareOp::Lt), "<");
  EXPECT_EQ(ast::CompareOpToString(ast::CompareOp::Le), "<=");
}

TEST(ValueTest, LogicalOpToString) {
  EXPECT_EQ(ast::LogicalOpToString(ast::LogicalOp::And), "AND");
  EXPECT_EQ(ast::LogicalOpToString(ast::LogicalOp::Or), "OR");
}

TEST(ValueTest, EscapeStringKeepsPlainText) {
  EXPECT_EQ(ast::EscapeString("hello"), "hello");
}

TEST(ValueTest, LabelsToString) {
  EXPECT_FALSE(ast::LabelsToString({"Person", "Employee"}).empty());
}

TEST(ValueTest, EdgeLabelsToString) {
  EXPECT_FALSE(ast::EdgeLabelsToString("KNOWS").empty());
}

TEST(ValueTest, PropertyMapToString) {
  ast::PropertyMap props{
      {"name", ast::Literal{ast::Value{std::string("Alice")}}},
      {"age", ast::Literal{ast::Value{int64_t{25}}}},
  };
  EXPECT_FALSE(ast::PropertyMapToString(props).empty());
}

TEST(ExpressionTest, LiteralExprCopyAndType) {
  ast::LiteralExpr expr;
  expr.literal = ast::Literal{ast::Value{int64_t{123}}};

  std::unique_ptr<ast::LiteralExpr> copy(expr.copy());

  ASSERT_NE(copy, nullptr);
  EXPECT_NE(copy.get(), &expr);
  EXPECT_EQ(copy->Type(), ast::ExprType::Literal);
  EXPECT_FALSE(copy->DebugString().empty());
}

TEST(ExpressionTest, PropertyExprCopyAndAliases) {
  ast::PropertyExpr expr;
  expr.alias = "n";
  expr.property = "name";

  std::unique_ptr<ast::PropertyExpr> copy(expr.copy());

  ASSERT_NE(copy, nullptr);
  EXPECT_NE(copy.get(), &expr);
  EXPECT_EQ(copy->Type(), ast::ExprType::Property);
  EXPECT_FALSE(copy->DebugString().empty());

  std::vector<std::string> aliases;
  expr.CollectAliases(aliases);
  ASSERT_EQ(aliases.size(), 1u);
  EXPECT_EQ(aliases[0], "n");
}

TEST(ExpressionTest, ComparisonExprBasics) {
  auto left = std::make_unique<ast::LiteralExpr>();
  left->literal = ast::Literal{ast::Value{int64_t{1}}};

  auto right = std::make_unique<ast::LiteralExpr>();
  right->literal = ast::Literal{ast::Value{int64_t{2}}};

  ast::ComparisonExpr expr(std::move(left), ast::CompareOp::Lt, std::move(right));

  std::unique_ptr<ast::ComparisonExpr> copy(expr.copy());

  ASSERT_NE(copy, nullptr);
  EXPECT_EQ(copy->Type(), ast::ExprType::Comparison);
  EXPECT_FALSE(copy->DebugString().empty());
}

TEST(ExpressionTest, LogicalExprBasics) {
  auto left = std::make_unique<ast::LiteralExpr>();
  left->literal = ast::Literal{ast::Value{true}};

  auto right = std::make_unique<ast::LiteralExpr>();
  right->literal = ast::Literal{ast::Value{false}};

  ast::LogicalExpr expr(std::move(left), ast::LogicalOp::And, std::move(right));

  std::unique_ptr<ast::LogicalExpr> copy(expr.copy());

  ASSERT_NE(copy, nullptr);
  EXPECT_EQ(copy->Type(), ast::ExprType::Logical);
  EXPECT_FALSE(copy->DebugString().empty());
}


TEST(PatternTest, NodePatternDebugString) {
  ast::NodePattern node;
  node.alias = "n";
  node.labels = {"Person", "Employee"};
  node.properties = {
      {"name", ast::Literal{ast::Value{std::string("Alice")}}},
      {"age", ast::Literal{ast::Value{int64_t{25}}}},
  };

  const auto s = node.DebugString();
  ExpectContains(s, "n");
  ExpectContains(s, "Person");
  ExpectContains(s, "Employee");
}

TEST(PatternTest, PatternElementNodeAccessors) {
  ast::NodePattern node;
  node.alias = "n";

  ast::PatternElement el{node};

  EXPECT_TRUE(el.IsNode());
  EXPECT_FALSE(el.IsEdge());
  EXPECT_EQ(el.AsNode().alias, "n");
  EXPECT_FALSE(el.DebugString().empty());
}

TEST(PatternTest, PatternDebugString) {
  ast::NodePattern node;
  node.alias = "n";

  ast::Pattern pattern;
  pattern.elements.push_back(ast::PatternElement{node});

  EXPECT_FALSE(pattern.DebugString().empty());
}

TEST(PatternTest, CreateNodeRefDebugString) {
  ast::CreateNodeRef ref;
  ref.alias = "a";

  ExpectContains(ref.DebugString(), "a");
}

TEST(PatternTest, CreateEdgePatternDebugString) {
  ast::CreateEdgePattern edge;

  edge.left_node.alias = "a";
  edge.alias = "e";
  edge.label = "KNOWS";
  edge.right_node.alias = "b";

  EXPECT_FALSE(edge.DebugString().empty());
}

TEST(ClauseTest, MatchClauseDebugString) {
  ast::NodePattern node;
  node.alias = "n";

  ast::Pattern pattern;
  pattern.elements.push_back(ast::PatternElement{node});

  ast::MatchClause clause;
  clause.patterns.push_back(pattern);

  const auto s = clause.DebugString();
  ExpectContains(s, "MATCH");
  ExpectContains(s, "n");
}

TEST(ClauseTest, WhereClauseDebugString) {
  auto expr = std::make_unique<ast::LiteralExpr>();
  expr->literal = ast::Literal{ast::Value{true}};

  ast::WhereClause clause;
  clause.expression = std::move(expr);

  const auto s = clause.DebugString();
  ExpectContains(s, "WHERE");
}

TEST(ClauseTest, ReturnItemAliasDebugString) {
  ast::ReturnItem item;
  item.return_item = std::string("n");

  ExpectContains(item.DebugString(), "n");
}

TEST(ClauseTest, ReturnItemPropertyDebugString) {
  ast::ReturnItem item;
  ast::PropertyExpr property;
  property.alias = "n";
  property.property = "name";
  item.return_item = std::move(property);

  EXPECT_FALSE(item.DebugString().empty());
}

TEST(ClauseTest, DeleteClauseDebugString) {
  ast::DeleteClause clause;
  clause.aliases = {"a", "b"};

  const auto s = clause.DebugString();
  ExpectContains(s, "a");
  ExpectContains(s, "b");
}

TEST(ClauseTest, SetItemDebugString) {
  ast::SetItem item;
  item.alias = "n";
  item.properties = {
      {"name", ast::Literal{ast::Value{std::string("Alice")}}},
  };
  item.labels = {"Person"};

  const auto s = item.DebugString();
  ExpectContains(s, "n");
  ExpectContains(s, "name");
  ExpectContains(s, "Person");
}

TEST(ClauseTest, OrderItemDebugString) {
  ast::OrderItem item;
  item.property.alias = "n";
  item.property.property = "age";
  item.direction = ast::OrderDirection::Asc;

  EXPECT_FALSE(item.DebugString().empty());
}

TEST(ClauseTest, LimitClauseDebugString) {
  ast::LimitClause clause{10};
  ExpectContains(clause.DebugString(), "10");
}

TEST(ClauseTest, CreateClauseDebugString) {
  ast::CreateClause clause;
  ast::NodePattern node;
  node.alias = "n";
  clause.created_items.emplace_back(node);

  EXPECT_FALSE(clause.DebugString().empty());
}

TEST(ClauseTest, QueryASTDebugString) {
  ast::QueryAST query;

  query.match_clause = std::make_unique<ast::MatchClause>();
  ast::NodePattern node;
  node.alias = "n";
  ast::Pattern pattern;
  pattern.elements.push_back(ast::PatternElement{node});
  query.match_clause->patterns.push_back(pattern);

  query.return_clause = std::make_unique<ast::ReturnClause>();
  ast::ReturnItem item;
  item.return_item = std::string("n");
  query.return_clause->items.push_back(item);

  const auto s = query.DebugString();
  ExpectContains(s, "MATCH");
  ExpectContains(s, "RETURN");
  ExpectContains(s, "n");
}

TEST(LexerTest, LexSimpleQuery) {
  const auto tokens = lexer::Lex("MATCH (n) RETURN n;");
  ASSERT_FALSE(tokens.empty());

  EXPECT_EQ(tokens.front().type_, lexer::TokenType::MATCH);
  EXPECT_EQ(tokens.back().type_, lexer::TokenType::END);
}

TEST(ParserTest, ParseSingleSimpleQuery) {
  const auto tokens = lexer::Lex("MATCH (n) RETURN n;");
  parser::Parser p(tokens);

  ast::QueryAST q = p.ParseSingle();

  const auto s = q.DebugString();
  ExpectContains(s, "MATCH");
  ExpectContains(s, "RETURN");
}

TEST(ParserTest, ParseMultipleQueries) {
  const auto tokens = lexer::Lex("MATCH (n) RETURN n; MATCH (m) RETURN m;");
  parser::Parser p(tokens);

  auto queries = p.Parse();
  ASSERT_GE(queries.size(), 2u);
}
