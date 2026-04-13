#pragma once

#include <vector>
#include <memory>

#include "token.hpp"
#include "ast.hpp"

namespace parser {

using lexer::Token;
using lexer::TokenType;

class Parser {
 public:
  explicit Parser(const std::vector<Token>& tokens);

  ast::QueryAST ParseSingle();
  std::vector<ast::QueryAST> Parse();

 private:
  const Token& Peek() const;
  const Token& Previous() const;
  bool IsAtEnd() const;
  bool Check(TokenType type) const;
  const Token& Advance();
  bool Match(TokenType type);
  bool MatchAny(std::initializer_list<TokenType> types);
  const Token& Consume(TokenType type, const char* expected);
  const Token& ConsumeIdentifier(const char* expected = "identifier");
  void ExpectEnd();
  void ConsumeSemicolons();
  const Token& Anchor() const;

  std::unique_ptr<ast::MatchClause> ParseMatch();
  std::unique_ptr<ast::CreateClause> ParseCreate();
  std::unique_ptr<ast::WhereClause> ParseWhere();
  std::unique_ptr<ast::ReturnClause> ParseReturn();
  std::unique_ptr<ast::DeleteClause> ParseDelete();
  std::unique_ptr<ast::SetClause> ParseSet();
  std::unique_ptr<ast::OrderClause> ParseOrder();
  std::unique_ptr<ast::LimitClause> ParseLimit();

  ast::Pattern ParsePattern();
  ast::NodePattern ParseNodePattern();
  ast::MatchEdgePattern ParseEdgePattern();

  ast::ExprPtr ParseExpression();
  ast::ExprPtr ParseOr();
  ast::ExprPtr ParseAnd();
  ast::ExprPtr ParseComparison();
  ast::ExprPtr ParsePrimary();

  const std::vector<Token>& tokens_;
  size_t current_ = 0;
};

} // namespace parser
