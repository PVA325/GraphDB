#include <gtest/gtest.h>

#include "parser/parser.hpp"
#include "parser/lexer.hpp"
#include "parser/error.hpp"

static ast::QueryAST ParseQuery(const std::string& query) {
    auto tokens = lexer::Lex(query);
    parser::Parser parser(tokens);
    return parser.ParseSingle();
}

static void ExpectParseError(const std::string& query) {
    auto tokens = lexer::Lex(query);
    parser::Parser parser(tokens);

    EXPECT_THROW(
        parser.ParseSingle(),
        ParseError
    );
}

TEST(ParserIntegration, SimpleMatch) {
    auto ast = ParseQuery("MATCH (n)");

    ASSERT_NE(ast.match_clause, nullptr);
    ASSERT_EQ(ast.match_clause->patterns.size(), 1u);

    const auto& pattern = ast.match_clause->patterns[0];

    ASSERT_EQ(pattern.elements.size(), 1u);

    const auto& node = pattern.elements[0].AsNode();

    EXPECT_EQ(node.alias, "n");
}

TEST(ParserIntegration, MatchWithLabels) {
    auto ast = ParseQuery(
        "MATCH (n:Person:Employee)"
    );

    const auto& node =
        ast.match_clause->patterns[0]
            .elements[0]
            .AsNode();

    ASSERT_EQ(node.labels.size(), 2u);

    EXPECT_EQ(node.labels[0], "Person");
    EXPECT_EQ(node.labels[1], "Employee");
}

TEST(ParserIntegration, MatchWithProperties) {
    auto ast = ParseQuery(
        R"(MATCH (n {name: "Alice", age: 25}))"
    );

    const auto& node =
        ast.match_clause->patterns[0]
            .elements[0]
            .AsNode();

    ASSERT_EQ(node.properties.size(), 2u);

    EXPECT_EQ(node.properties[0].first, "name");
    EXPECT_EQ(
        std::get<std::string>(node.properties[0].second.value),
        "Alice"
    );

    EXPECT_EQ(node.properties[1].first, "age");
    EXPECT_EQ(
        std::get<int64_t>(node.properties[1].second.value),
        25
    );
}

TEST(ParserIntegration, MatchEdge) {
    auto ast = ParseQuery(
        "MATCH (a)-[e:KNOWS]->(b)"
    );

    const auto& pattern =
        ast.match_clause->patterns[0];

    ASSERT_EQ(pattern.elements.size(), 3u);

    const auto& left =
        pattern.elements[0].AsNode();

    const auto& edge =
        pattern.elements[1].AsEdge();

    const auto& right =
        pattern.elements[2].AsNode();

    EXPECT_EQ(left.alias, "a");

    EXPECT_EQ(edge.alias, "e");
    EXPECT_EQ(edge.label, "KNOWS");

    EXPECT_EQ(right.alias, "b");
}

TEST(ParserIntegration, WhereComparison) {
    auto ast = ParseQuery(
        "MATCH (n) WHERE n.age > 18"
    );

    ASSERT_NE(ast.where_clause, nullptr);

    auto* cmp =
        dynamic_cast<ast::ComparisonExpr*>(
            ast.where_clause->expression.get()
        );

    ASSERT_NE(cmp, nullptr);

    EXPECT_EQ(cmp->op, ast::CompareOp::Gt);

    auto* left =
        dynamic_cast<ast::PropertyExpr*>(
            cmp->left_expr.get()
        );

    ASSERT_NE(left, nullptr);

    EXPECT_EQ(left->alias, "n");
    EXPECT_EQ(left->property, "age");

    auto* right =
        dynamic_cast<ast::LiteralExpr*>(
            cmp->right_expr.get()
        );

    ASSERT_NE(right, nullptr);

    EXPECT_EQ(
        std::get<int64_t>(right->literal.value),
        18
    );
}

TEST(ParserIntegration, LogicalAnd) {
    auto ast = ParseQuery(R"(
        MATCH (n)
        WHERE n.age > 18 AND n.active = true
    )");

    auto* logical =
        dynamic_cast<ast::LogicalExpr*>(
            ast.where_clause->expression.get()
        );

    ASSERT_NE(logical, nullptr);

    EXPECT_EQ(logical->op, ast::LogicalOp::And);
}

TEST(ParserIntegration, AndHasHigherPriorityThanOr) {
    auto ast = ParseQuery(R"(
        MATCH (n)
        WHERE n.a = 1 OR n.b = 2 AND n.c = 3
    )");

    auto* root =
        dynamic_cast<ast::LogicalExpr*>(
            ast.where_clause->expression.get()
        );

    ASSERT_NE(root, nullptr);

    EXPECT_EQ(root->op, ast::LogicalOp::Or);

    auto* rhs =
        dynamic_cast<ast::LogicalExpr*>(
            root->right_expr.get()
        );

    ASSERT_NE(rhs, nullptr);

    EXPECT_EQ(rhs->op, ast::LogicalOp::And);
}

TEST(ParserIntegration, ReturnProperty) {
    auto ast = ParseQuery(
        "MATCH (n) RETURN n.name"
    );

    ASSERT_NE(ast.return_clause, nullptr);

    ASSERT_EQ(ast.return_clause->items.size(), 1u);

    const auto& item =
        ast.return_clause->items[0];

    ASSERT_TRUE(
        std::holds_alternative<ast::PropertyExpr>(
            item.return_item
        )
    );

    const auto& prop =
        std::get<ast::PropertyExpr>(
            item.return_item
        );

    EXPECT_EQ(prop.alias, "n");
    EXPECT_EQ(prop.property, "name");
}

TEST(ParserIntegration, OrderByDesc) {
    auto ast = ParseQuery(R"(
        MATCH (n)
        RETURN n
        ORDER BY n.age DESC
    )");

    ASSERT_NE(ast.order_clause, nullptr);

    ASSERT_EQ(ast.order_clause->items.size(), 1u);

    const auto& item =
        ast.order_clause->items[0];

    EXPECT_EQ(
        item.direction,
        ast::OrderDirection::Desc
    );

    EXPECT_EQ(item.property.alias, "n");
    EXPECT_EQ(item.property.property, "age");
}

TEST(ParserIntegration, LimitClause) {
    auto ast = ParseQuery(R"(
        MATCH (n)
        RETURN n
        LIMIT 15
    )");

    ASSERT_NE(ast.limit_clause, nullptr);

    EXPECT_EQ(ast.limit_clause->limit, 15u);
}

TEST(ParserIntegration, SetProperty) {
    auto ast = ParseQuery(
        "MATCH (n) SET n.age = 30"
    );

    ASSERT_NE(ast.set_clause, nullptr);

    ASSERT_EQ(ast.set_clause->items.size(), 1u);

    const auto& item =
        ast.set_clause->items[0];

    EXPECT_EQ(item.alias, "n");

    ASSERT_EQ(item.properties.size(), 1u);

    EXPECT_EQ(item.properties[0].first, "age");

    EXPECT_EQ(
        std::get<int64_t>(
            item.properties[0].second.value
        ),
        30
    );
}

TEST(ParserIntegration, SetLabels) {
    auto ast = ParseQuery(
        "MATCH (n) SET n:Person:Employee"
    );

    const auto& item =
        ast.set_clause->items[0];

    ASSERT_EQ(item.labels.size(), 2u);

    EXPECT_EQ(item.labels[0], "Person");
    EXPECT_EQ(item.labels[1], "Employee");
}

TEST(ParserIntegration, DeleteClause) {
    auto ast = ParseQuery(
        "MATCH (n) DELETE n"
    );

    ASSERT_NE(ast.delete_clause, nullptr);

    ASSERT_EQ(ast.delete_clause->aliases.size(), 1u);

    EXPECT_EQ(
        ast.delete_clause->aliases[0],
        "n"
    );
}

TEST(ParserIntegration, CreateNode) {
    auto ast = ParseQuery(
        "CREATE (n:Person)"
    );

    ASSERT_NE(ast.create_clause, nullptr);

    ASSERT_EQ(
        ast.create_clause->created_items.size(),
        1u
    );

    ASSERT_TRUE(
        std::holds_alternative<ast::NodePattern>(
            ast.create_clause->created_items[0]
        )
    );

    const auto& node =
        std::get<ast::NodePattern>(
            ast.create_clause->created_items[0]
        );

    EXPECT_EQ(node.alias, "n");

    ASSERT_EQ(node.labels.size(), 1u);

    EXPECT_EQ(node.labels[0], "Person");
}

TEST(ParserIntegration, CreateEdge) {
    auto ast = ParseQuery(
        "CREATE (a)-[e:KNOWS]->(b)"
    );

    ASSERT_EQ(
        ast.create_clause->created_items.size(),
        1u
    );

    ASSERT_TRUE(
        std::holds_alternative<ast::CreateEdgePattern>(
            ast.create_clause->created_items[0]
        )
    );

    const auto& edge =
        std::get<ast::CreateEdgePattern>(
            ast.create_clause->created_items[0]
        );

    EXPECT_EQ(edge.left_node.alias, "a");
    EXPECT_EQ(edge.alias, "e");
    EXPECT_EQ(edge.label, "KNOWS");
    EXPECT_EQ(edge.right_node.alias, "b");
}

TEST(ParserIntegration, OrderWithoutReturnFails) {
    ExpectParseError(R"(
        MATCH (n)
        ORDER BY n.age
    )");
}

TEST(ParserIntegration, LimitFloatFails) {
    ExpectParseError(R"(
        MATCH (n)
        RETURN n
        LIMIT 3.14
    )");
}

TEST(ParserIntegration, BidirectionalEdgeFails) {
    ExpectParseError(
        "MATCH (a)<-[e]->(b)"
    );
}

TEST(ParserIntegration, CreateReturnFails) {
    ExpectParseError(
        "CREATE (n) RETURN n"
    );
}
