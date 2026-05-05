#include "parser/parser.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <algorithm>

#include "parser/error.hpp"
#include "parser/lexer.hpp"

namespace parser {
namespace {

using lexer::Token;
using lexer::TokenType;

// Converts a token type to a human-readable token name.
std::string TokenName(TokenType type) {
  switch (type) {
    case TokenType::MATCH: return "MATCH";
    case TokenType::WHERE: return "WHERE";
    case TokenType::RETURN: return "RETURN";
    case TokenType::DELETE: return "DELETE";
    case TokenType::SET: return "SET";
    case TokenType::CREATE: return "CREATE";
    case TokenType::ORDER: return "ORDER";
    case TokenType::BY: return "BY";
    case TokenType::LIMIT: return "LIMIT";
    case TokenType::AND: return "AND";
    case TokenType::OR: return "OR";
    case TokenType::ASC: return "ASC";
    case TokenType::DESC: return "DESC";
    case TokenType::IDENTIFIER: return "identifier";
    case TokenType::STRING: return "string";
    case TokenType::NUMBER: return "number";
    case TokenType::TRUE: return "TRUE";
    case TokenType::FALSE: return "FALSE";
    case TokenType::COLON: return ":";
    case TokenType::LPAREN: return "(";
    case TokenType::RPAREN: return ")";
    case TokenType::LBRACKET: return "[";
    case TokenType::RBRACKET: return "]";
    case TokenType::LBRACE: return "{";
    case TokenType::RBRACE: return "}";
    case TokenType::COMMA: return ",";
    case TokenType::DOT: return ".";
    case TokenType::SEMICOLON: return ";";
    case TokenType::DASH: return "-";
    case TokenType::ARROW_RIGHT: return "->";
    case TokenType::ARROW_LEFT: return "<-";
    case TokenType::EQUAL: return "=";
    case TokenType::NOT_EQUAL: return "!=";
    case TokenType::GREATER: return ">";
    case TokenType::GREATER_EQUAL: return ">=";
    case TokenType::LESS: return "<";
    case TokenType::LESS_EQUAL: return "<=";
    case TokenType::END: return "end of input";
  }

  return "token";
}

// Checks if a token type represents a literal value.
bool IsLiteralToken(TokenType type) {
  return type == TokenType::NUMBER || type == TokenType::STRING ||
         type == TokenType::TRUE || type == TokenType::FALSE;
}

// Strips quotes from a string literal.
std::string StripQuotes(const std::string& text) {
  if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
    return text.substr(1, text.size() - 2);
  }

  return text;
}

}  // namespace

Parser::Parser(const std::vector<Token>& tokens)
      : tokens_(std::move(tokens)) {}

// Parses a single query.
ast::QueryAST Parser::ParseSingle() {
  ast::QueryAST query = ParseQuery();
  ConsumeSemicolons();
  ExpectEnd();
  return query;
}

// Parses multiple queries separated by semicolons.
std::vector<ast::QueryAST> Parser::Parse() {
  std::vector<ast::QueryAST> queries;
  ConsumeSemicolons();

  while (!Check(TokenType::END)) {
    ast::QueryAST query = ParseQuery();
    queries.push_back(std::move(query));
    ConsumeSemicolons();
  }

  return queries;
}

// Returns the current token without consuming it.
const Token& Parser::Peek() const {
  return tokens_[current_];
}

// Returns the previous token (the one just consumed).
const Token& Parser::Previous() const {
  return tokens_[current_ - 1];
}

// Checks if the current token matches the expected type.
bool Parser::Check(TokenType type) const {
  return Peek().type == type;
}

// Checks if we've reached the end of the token stream.
bool Parser::IsAtEnd() const {
  return Peek().type == TokenType::END;
}

// Consumes the current token with promotion.
const Token& Parser::Advance() {
  if (!IsAtEnd()) {
    ++current_;
  }
  return Previous();
}

// Checks if the current token matches the expected type and consumes it if it does.
bool Parser::Match(TokenType type) {
  if (Check(type)) {
    Advance();
    return true;
  }

  return false;
}

// Checks if the current token matches any of the expected types and consumes it if it does.
bool Parser::MatchAny(std::initializer_list<TokenType> types) {
  for (TokenType type : types) {
    if (Check(type)) {
      Advance();
      return true;
    }
  }

  return false;
}

// Consumes the current token if it matches the expected type, otherwise throws an error.
const Token& Parser::Consume(TokenType type, const char* expected) {
  if (Check(type)) {
    return Advance();
  }

  const Token& got = Peek();
  throw ParseError(got.line, got.col, "expected " + std::string(expected) + " but got " + TokenName(got.type));
}

// Consumes the current token if it's an identifier, otherwise throws an error.
const Token& Parser::ConsumeIdentifier(const char* expected) {
  return Consume(TokenType::IDENTIFIER, expected);
}

// Expects the end of the token stream, otherwise throws an error.
void Parser::ExpectEnd() {
  if (!Check(TokenType::END)) {
    const Token& got = Peek();
    throw ParseError(got.line, got.col, "unexpected token " + TokenName(got.type));
  }
}

// Consumes semicolons between queries.
void Parser::ConsumeSemicolons() {
  while (Match(TokenType::SEMICOLON)) {
  }
}

// Placeholder in case of an empty or unusual request
const Token& Parser::Anchor() const {
  static const Token dummy{TokenType::END, std::string{}, 1, 1};
  return tokens_.empty() ? dummy : tokens_.front();
}

// Parses a single query and return its AST representation.
ast::QueryAST Parser::ParseQuery() {
  ast::QueryAST query;

  if (Match(TokenType::MATCH)) {
    query.match_clause = ParseMatch();

    if (Match(TokenType::WHERE)) {
      query.where_clause = ParseWhere();
    }

    ParseMatchTail(query);
  } else if (Match(TokenType::CREATE)) {
    query.create_clause = ParseCreate();

    if (Check(TokenType::WHERE) || Check(TokenType::RETURN) || Check(TokenType::DELETE) ||
        Check(TokenType::SET) || Check(TokenType::ORDER) || Check(TokenType::LIMIT)) {
      const Token& got = Peek();
      throw ParseError(got.line, got.col, "unexpected clause after CREATE");
    }
  } else {
    const Token& got = Peek();
    throw ParseError(got.line, got.col, "expected MATCH or CREATE but got " + TokenName(got.type));
  }

  return query;
}

// Parses Match Tail.
void Parser::ParseMatchTail(ast::QueryAST& query) {
  if (Match(TokenType::RETURN)) {
      query.return_clause = ParseReturn();
  } else if (Match(TokenType::DELETE)) {
      query.delete_clause = ParseDelete();
  } else if (Match(TokenType::SET)) {
      query.set_clause = ParseSet();
  }

  if (query.return_clause) {
    if (Match(TokenType::ORDER)) {
      Consume(TokenType::BY, "BY");
      query.order_clause = ParseOrder();
    }

    if (Match(TokenType::LIMIT)) {
      query.limit_clause = ParseLimit();
    }
  } else if (Check(TokenType::ORDER) || Check(TokenType::LIMIT)) {
    const Token& got = Peek();
    throw ParseError(got.line, got.col, "ORDER BY and LIMIT are only allowed with RETURN");
  }

  if (Check(TokenType::RETURN) || Check(TokenType::DELETE) || Check(TokenType::SET) ||
      Check(TokenType::WHERE) || Check(TokenType::ORDER) || Check(TokenType::LIMIT)) {
    const Token& got = Peek();
    throw ParseError(got.line, got.col, "unexpected token " + TokenName(got.type));
  }
}

// Parses Match clause patterns.
std::unique_ptr<ast::MatchClause> Parser::ParseMatch() {
  auto clause = std::make_unique<ast::MatchClause>();
  clause->patterns.push_back(ParseMatchPattern());

  while (Match(TokenType::COMMA)) {
    clause->patterns.push_back(ParseMatchPattern());
  }

  return clause;
}

// Parses pattern.
ast::Pattern Parser::ParseMatchPattern() {
  ast::Pattern pattern;
  pattern.elements.emplace_back(ParseNodePattern());

  while (Check(TokenType::DASH) || Check(TokenType::ARROW_RIGHT) || Check(TokenType::ARROW_LEFT)) {
    pattern.elements.emplace_back(ParseMatchEdgePattern());
    pattern.elements.emplace_back(ParseNodePattern());
  }

  return pattern;
}

// Parses Nodes/Edges properties.
ast::PropertyMap Parser::ParsePropertyMap() {
  ast::PropertyMap properties;
  Consume(TokenType::LBRACE, "{");

  if (!Check(TokenType::RBRACE)) {
    while (true) {
      const Token& key = ConsumeIdentifier("property key");
      Consume(TokenType::COLON, ":");
      properties.emplace_back(key.lexeme, ParseLiteral());

      if (!Match(TokenType::COMMA)) {
        break;
      }
    }
  }

  Consume(TokenType::RBRACE, "}");
  return properties;
}

// Parses node pattern.
ast::NodePattern Parser::ParseNodePattern() {
  const Token& lparen = Consume(TokenType::LPAREN, "(");
  ast::NodePattern node;
  node.line = lparen.line;
  node.col = lparen.col;

  if (Check(TokenType::IDENTIFIER)) {
    node.alias = Advance().lexeme;
  }

  if (Match(TokenType::COLON)) {
    while (Check(TokenType::IDENTIFIER)) {
      node.labels.push_back(Advance().lexeme);
      if (!Match(TokenType::COLON)) {
        break;
      }
    }
  }

  if (Check(TokenType::LBRACE)) {
    node.properties = ParsePropertyMap();
  }

  Consume(TokenType::RPAREN, ")");
  return node;
}

// Parses edge pattern.
ast::MatchEdgePattern Parser::ParseMatchEdgePattern() {
  ast::MatchEdgePattern edge;
  edge.line = Peek().line;
  edge.col = Peek().col;
  bool started_with_arrow = false;

  if (Match(TokenType::ARROW_LEFT)) {
    edge.direction = ast::EdgeDirection::Left;
    started_with_arrow = true;
  } else {
    Consume(TokenType::DASH, "-");
    edge.direction = ast::EdgeDirection::Right;
  }

  Consume(TokenType::LBRACKET, "[");

  if (Check(TokenType::IDENTIFIER)) {
    edge.alias = Advance().lexeme;
  }

  if (Match(TokenType::COLON)) {
    while (Check(TokenType::IDENTIFIER)) {
      if (!edge.label.empty()) {
        edge.label += ":";
      }

      edge.label += Advance().lexeme;

      if (!Match(TokenType::COLON)) {
        break;
      }
    }
  }

  if (Check(TokenType::LBRACE)) {
    edge.properties = ParsePropertyMap();
  }

  Consume(TokenType::RBRACKET, "]");

  if (Match(TokenType::ARROW_RIGHT)) {
    if (started_with_arrow) {
      throw ParseError(edge.line, edge.col, "Bidirectional arrows are not supported");
    }
  } else if (Match(TokenType::DASH)) {
    if (!started_with_arrow) {
      const Token& got = Previous();
      throw ParseError(got.line, got.col, "Edge must be directed (missing '>' at the end)");
    }
  } else {
    const Token& got = Peek();
    throw ParseError(got.line, got.col, "Expected '-' or '->' at the end of edge");    
  }

  return edge;
}

// Parses literal.
ast::Literal Parser::ParseLiteral() {
  Token token = Peek();

  switch (token.type) {
    case TokenType::NUMBER: {
      Advance();
      ast::Literal literal;

      if (token.lexeme.find_first_of(".eE") != std::string::npos) {
        literal.value = std::stod(token.lexeme);
      } else {
        literal.value = static_cast<int64_t>(std::stoll(token.lexeme));
      }

      return literal;
    }

    case TokenType::STRING: {
      Advance();
      return ast::Literal{StripQuotes(token.lexeme)};
    }

    case TokenType::TRUE: {
      Advance();
      return ast::Literal{true};
    }

    case TokenType::FALSE: {
      Advance();
      return ast::Literal{false};
    }

    default:
      throw ParseError(token.line, token.col, "expected literal, but got " + TokenName(token.type));
  }
}

// Parses expression.
ast::ExprPtr Parser::ParseExpression() {
  return ParseOr();
}

// Parses Or.
ast::ExprPtr Parser::ParseOr() {
  ast::ExprPtr expr = ParseAnd();

  while (Match(TokenType::OR)) {
    auto right = ParseAnd();
    auto node = std::make_unique<ast::LogicalExpr>();
    node->left_expr = std::move(expr);
    node->op = ast::LogicalOp::Or;
    node->right_expr = std::move(right);
    expr = std::move(node);
  }

  return expr;
}

// Parses And.
ast::ExprPtr Parser::ParseAnd() {
  ast::ExprPtr expr = ParseComparison();

  while (Match(TokenType::AND)) {
    auto right = ParseComparison();
    auto node = std::make_unique<ast::LogicalExpr>();
    node->left_expr = std::move(expr);
    node->op = ast::LogicalOp::And;
    node->right_expr = std::move(right);
    expr = std::move(node);
  }

  return expr;
}

// Parses Comparison.
ast::ExprPtr Parser::ParseComparison() {
  ast::ExprPtr left = ParsePrimary();

  if (MatchAny({TokenType::EQUAL, TokenType::NOT_EQUAL,
                TokenType::GREATER, TokenType:: GREATER_EQUAL,
                TokenType::LESS, TokenType::LESS_EQUAL})) {
    Token op = Previous();
    ast::ExprPtr right = ParsePrimary();
    auto node = std::make_unique<ast::ComparisonExpr>();
    node->left_expr = std::move(left);
    node->op = op.type == TokenType::EQUAL ? ast::CompareOp::Eq
               : op.type == TokenType::NOT_EQUAL ? ast::CompareOp::NotEqual
               : op.type == TokenType::GREATER ? ast::CompareOp::Gt
               : op.type == TokenType::GREATER_EQUAL ? ast::CompareOp::Ge
               : op.type == TokenType::LESS ? ast::CompareOp::Lt
               : ast::CompareOp::Le;
    node->right_expr = std::move(right);
    return node;
  }

  return left;
}

// Parse Primary.
ast::ExprPtr Parser::ParsePrimary() {
  if (Match(TokenType::LPAREN)) {
    auto expr = ParseExpression();
    Consume(TokenType::RPAREN, ")");
    return expr;
  }

  if (Check(TokenType::IDENTIFIER)) {
    auto prop = std::make_unique<ast::PropertyExpr>();
    *prop = ParsePropertyExpr();
    return prop;
  }

  if (IsLiteralToken(Peek().type)) {
    auto lit = std::make_unique<ast::LiteralExpr>();
    lit->literal = ParseLiteral();
    return lit;
  }

  const Token& got = Peek();
  throw ParseError(got.line, got.col, "expected expression but got " + TokenName(got.type));
}

// Parse Property Expression;
ast::PropertyExpr Parser::ParsePropertyExpr() {
  const Token& alias = ConsumeIdentifier("alias");
  Consume(TokenType::DOT, ".");
  const Token& property = ConsumeIdentifier("property name");

  ast::PropertyExpr expr;
  expr.alias = alias.lexeme;
  expr.property = property.lexeme;
  return expr;
}

// Parses Where Clause.
std::unique_ptr<ast::WhereClause> Parser::ParseWhere() {
  auto clause = std::make_unique<ast::WhereClause>();
  clause->expression = ParseExpression();
  return clause;
}

// Parsess Return Clause.
std::unique_ptr<ast::ReturnClause> Parser::ParseReturn() {
  auto clause = std::make_unique<ast::ReturnClause>();
  clause->items.push_back(ParseReturnItem());

  while (Match(TokenType::COMMA)) {
    clause->items.push_back(ParseReturnItem());
  }

  return clause;
}

// Parses Return Item.
ast::ReturnItem Parser::ParseReturnItem() {
  ast::ReturnItem item;

  if (Check(TokenType::IDENTIFIER) &&
      current_ + 1 < tokens_.size() &&
      tokens_[current_ + 1].type == TokenType::DOT) {
    item.return_item = ParsePropertyExpr();
  } else {
    item.return_item = ConsumeIdentifier("return item").lexeme;
  }

  return item;
}

// Parses Delete Clause.
std::unique_ptr<ast::DeleteClause> Parser::ParseDelete() {
  auto clause = std::make_unique<ast::DeleteClause>();
  clause->aliases.push_back(ConsumeIdentifier("alias").lexeme);

  while (Match(TokenType::COMMA)) {
    clause->aliases.push_back(ConsumeIdentifier("alias").lexeme);
  }

  return clause;
}

// Parses Set Clause.
std::unique_ptr<ast::SetClause> Parser::ParseSet() {
  auto clause = std::make_unique<ast::SetClause>();
  clause->items.push_back(ParseSetItem());

  while (Match(TokenType::COMMA)) {
    clause->items.push_back(ParseSetItem());
  }

  return clause;
}

// Parses Set Item.
ast::SetItem Parser::ParseSetItem() {
  ast::SetItem item;
  item.alias = ConsumeIdentifier("set item alias").lexeme;

  if (Match(TokenType::DOT)) {
    std::string prop = ConsumeIdentifier("property").lexeme;
    Consume(TokenType::EQUAL, "=");
    ast::Literal lit = ParseLiteral();

    item.properties.emplace_back(prop, lit);
  } else if (Match(TokenType::COLON)) {
    item.labels.push_back(ConsumeIdentifier("label").lexeme);

    while (Match(TokenType::COLON)) {
      item.labels.push_back(ConsumeIdentifier("label").lexeme);
    }
  } else {
    const Token& got = Peek();
    throw ParseError(got.line, got.col, "expected property or label but got " + TokenName(got.type));
  }

  return item;
}

// Parses Order Clause.
std::unique_ptr<ast::OrderClause> Parser::ParseOrder() {
  auto clause = std::make_unique<ast::OrderClause>();
  clause->items.push_back(ParseOrderItem());

  while (Match(TokenType::COMMA)) {
    clause->items.push_back(ParseOrderItem());
  }

  return clause;
}

//Parses Order Item.
ast::OrderItem Parser::ParseOrderItem() {
  ast::OrderItem item;
  item.property = ParsePropertyExpr();

  if (Match(TokenType::ASC)) {
    item.direction = ast::OrderDirection::Asc;
  } else if (Match(TokenType::DESC)) {
    item.direction = ast::OrderDirection::Desc;
  }

  return item;
}

// Parses Limit Clause.
std::unique_ptr<ast::LimitClause> Parser::ParseLimit() {
  auto clause = std::make_unique<ast::LimitClause>();
  const Token& limit_number = Consume(TokenType::NUMBER, "number");

  if (limit_number.lexeme.find_first_of(".eE")  != std::string::npos) {
    throw ParseError(limit_number.line, limit_number.col, "LIMIT expects an integer");
  }

  clause->limit = static_cast<size_t>(std::stoull(limit_number.lexeme));
  return clause;
}

// Parses Create Clause.
std::unique_ptr<ast::CreateClause> Parser::ParseCreate() {
  auto clause = std::make_unique<ast::CreateClause>();
  clause->created_items.push_back(ParseCreateItem());

  while (Match(TokenType::COMMA)) {
    clause->created_items.push_back(ParseCreateItem());
  }

  return clause;
}

// Parses Create item.
ast::CreateItem Parser::ParseCreateItem() {
  if (!Check(TokenType::LPAREN)) {
    const Token& got = Peek();
    throw ParseError(got.line, got.col, "expected CREATE item but got " + TokenName(got.type));
  }

  size_t save = current_;
  ast::NodePattern node = ParseNodePattern();

  if (!Check(TokenType::DASH) && !Check(TokenType::ARROW_LEFT)) {
    return node;
  }

  current_ = save;
  ast::CreateNodeRef left_node_ref = ParseCreateNodeRef();
  return ParseCreateEdgePattern(left_node_ref);
}

// Parses Create Node Reference.
ast::CreateNodeRef Parser::ParseCreateNodeRef() {
  const Token& lparen = Consume(TokenType::LPAREN, "(");
  const Token& alias = ConsumeIdentifier("alias");
  Consume(TokenType::RPAREN, ")");
  ast::CreateNodeRef reference;
  reference.alias = alias.lexeme;
  reference.line = lparen.line;
  reference.col = lparen.col;
  return reference;
}

// Parses Create Edge Pattern.
ast::CreateEdgePattern Parser::ParseCreateEdgePattern(const ast::CreateNodeRef left_node_ref) {
  ast::CreateEdgePattern edge;
  edge.left_node = left_node_ref;
  edge.line = Peek().line;
  edge.col = Peek().col;
  bool started_with_arrow = false;

  if (Match(TokenType::ARROW_LEFT)) {
    edge.direction = ast::EdgeDirection::Left;
    started_with_arrow = true;
  } else {
    Consume(TokenType::DASH, "-");
    edge.direction = ast::EdgeDirection::Right;
  }

  Consume(TokenType::LBRACKET, "[");

  if (Check(TokenType::IDENTIFIER)) {
    edge.alias = Advance().lexeme;
  }

  if (Match(TokenType::COLON)) {
    while (Check(TokenType::IDENTIFIER)) {
      if (!edge.label.empty()) {
        edge.label += ":";
      }

      edge.label += Advance().lexeme;

      if (!Match(TokenType::COLON)) {
        break;
      }
    }
  }

  if (Check(TokenType::LBRACE)) {
    edge.properties = ParsePropertyMap();
  }

  Consume(TokenType::RBRACKET, "]");

  if (Match(TokenType::ARROW_RIGHT)) {
    if (started_with_arrow) {
      throw ParseError(edge.line, edge.col, "Bidirectional arrows are not supported");
    }
  } else if (Match(TokenType::DASH)) {
    if (!started_with_arrow) {
      const Token& got = Previous();
      throw ParseError(got.line, got.col, "Edge must be directed (missing '>' at the end)");
    }
  } else {
    const Token& got = Peek();
    throw ParseError(got.line, got.col, "Expected '-' or '->' at the end of edge");    
  }

  edge.right_node = ParseCreateNodeRef();
  return edge;
}

} // namespace parser
