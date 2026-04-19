#pragma once
#include "planner.hpp"

using graph::String;

struct FakeExpr final : ast::Expr {
  explicit FakeExpr(String s) : s_(std::move(s)) {}

  graph::Value operator()(const ast::EvalContext&) const override {
    return false;
  }

  [[nodiscard]] std::string DebugString() const override {
    return s_;
  }

  String s_;
};

ast::Pattern MakeSingleNodePattern(const String& alias, std::initializer_list<String> labels = {}) {
  ast::NodePattern node;
  node.alias = alias;
  node.labels = labels;

  ast::PatternElement el;
  el.element = node;

  ast::Pattern pat;
  pat.elements.push_back(std::move(el));
  return pat;
}

ast::QueryAST MakeQueryWithMatch(const String& alias = "a") {
  ast::QueryAST q;
  q.match_clause = std::make_unique<ast::MatchClause>();
  q.match_clause->patterns.push_back(MakeSingleNodePattern(alias, {"Person"}));
  return q;
}

void ExpectInOrder(const String& s, std::initializer_list<String> needles) {
  size_t pos = 0;
  for (const auto& needle : needles) {
    const size_t found = s.find(needle, pos);
    ASSERT_NE(found, String::npos) << "Missing substring: " << needle << "\nIn string:\n" << s;
    pos = found + needle.size();
  }
}

graph::planner::Planner MakePlanner(ast::QueryAST q) {
  graph::exec::ExecContext ctx;
  ctx.db = nullptr;
  return graph::planner::Planner(ctx, std::move(q));
}

using graph::String;

size_t CountSubstr(const String& s, const String& needle) {
  size_t count = 0;
  size_t pos = 0;
  while ((pos = s.find(needle, pos)) != String::npos) {
    ++count;
    pos += needle.size();
  }
  return count;
}


ast::Pattern MakeNodePattern(const String& alias, std::initializer_list<String> labels = {}) {
  ast::NodePattern node;
  node.alias = alias;
  node.labels = labels;

  ast::PatternElement el;
  el.element = node;

  ast::Pattern pat;
  pat.elements.push_back(std::move(el));
  return pat;
}

ast::Pattern MakeEdgePattern(const String& left_alias,
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

ast::QueryAST MakeSelectQueryTwoPatterns() {
  ast::QueryAST q;
  q.match_clause = std::make_unique<ast::MatchClause>();
  q.match_clause->patterns.push_back(MakeNodePattern("a", {"Person"}));
  q.match_clause->patterns.push_back(MakeNodePattern("b", {"City"}));

  q.return_clause = std::make_unique<ast::ReturnClause>();
  q.return_clause->items.push_back(ast::ReturnItem{std::string{"a"}});
  q.return_clause->items.push_back(ast::ReturnItem{std::string{"b"}});
  return q;
}
