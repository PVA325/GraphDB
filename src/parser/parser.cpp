#include "parser.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "error.hpp"
#include "lexer.hpp"

namespace ast {
namespace {

// Escapes special characters in a string for debug output.
std::string EscapeString(std::string_view text) {
  std::string result;
  result.reserve(text.size());

  for (char c : text) {
    switch (c) {
      case '\\': result += "\\\\"; break;
      case '"': result += "\\\""; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      default: result += c; break;
    }
  }

  return result;
}

// Converts a Value to a string for debug output.
std::string ValueToString(const Value& value) {
  return std::visit([](const auto& v) -> std::string {
    using T = std::decay_t<decltype(v)>;

    if constexpr (std::is_same_v<T, std::string>) {
      return '"' + EscapeString(v) + '"';
    } else if constexpr (std::is_same_v<T, bool>) {
      return v ? "true" : "false";
    } else {
      std::ostringstream out;
      out << v;
      return out.str();
    }
  }, value);
}

// Converts a PropertyMap to a string for debug output.
std::string PropertyMapToString(const PropertyMap& properties) {
  std::string result = "{";

  for (size_t i = 0; i < properties.size(); ++i) {
    result += properties[i].first;
    result += ": ";
    result += ValueToString(properties[i].second.value);
  
    if (i + 1 < properties.size()) {
      result += ", ";
    }
  }

  result += "}";
  return result;
}

// Converts a list of labels to a string for debug output.
std::string LabelsToString(const std::vector<std::string>& labels) {
  std::string result;

  for (const auto& label : labels) {
    result += ":" + label;
  }

  return result;
}

// Converts edge labels to a string for debug output.
std::string EdgeLabelsToString(const std::string& labels) {
  return labels.empty() ? std::string{} : ":" + labels;
}

// Converts a comparison operator to a string for debug output.
std::string CompareOpToString(CompareOp op) {
  switch (op) {
    case CompareOp::Eq: return "=";
    case CompareOp::NotEqual: return "!=";
    case CompareOp::Lt: return "<";
    case CompareOp::Le: return "<=";
    case CompareOp::Gt: return ">";
    case CompareOp::Ge: return ">=";
  }

  return "?";
}

// Converts a logical operator to a string for debug output.
std::string LogicalOpToString(LogicalOp op) {
  switch (op) {
    case LogicalOp::And: return "AND";
    case LogicalOp::Or: return "OR";
  }

  return "?";
}

} // namespace

// Debug string implementations for AST nodes.
std::string Literal::DebugString() const {
  return "Literal(" + ValueToString(value) + ")";
}

// LiteralExpr evaluation simply returns the literal value.
Value LiteralExpr::operator()(const EvalContext&) const {
  return literal.value;
}

// Debug string implementations for expressions and patterns.
std::string LiteralExpr::DebugString() const {
  return literal.DebugString();
}

// Placeholders for expression evaluation.
Value PropertyExpr::operator()(const EvalContext&) const {
  throw std::logic_error("PropertyExpr evaluation is not implemented in parser.cpp");
}

// Debug string for property access expressions.
std::string PropertyExpr::DebugString() const {
  return "Property(" + alias + "." + property + ")";
}

// Placeholder for comparison expression evaluation.
Value ComparisonExpr::operator()(const EvalContext&) const {
  throw std::logic_error("ComparisonExpr evaluation is not implemented in parser.cpp");
}

// Debug string for comparison expressions.
std::string ComparisonExpr::DebugString() const {
  return "Compare(" +
      (left ? left->DebugString() : std::string{"<null>"}) +
      " " +
      CompareOpToString(op) +
      " " +
      (right ? right->DebugString() : std::string{"<null>"}) +
      ")";
}

// Placeholder for logical expression evaluation.
Value LogicalExpr::operator()(const EvalContext&) const {
  throw std::logic_error("LogicalExpr evaluation is not implemented in parser.cpp");
}

// Debug string for logical expressions.
std::string LogicalExpr::DebugString() const {
  return "Logical(" +
      (left_expr ? left_expr->DebugString() : std::string{"<null>"}) +
      " " +
      LogicalOpToString(op) +
      " " +
      (right_expr ? right_expr->DebugString() : std::string{"<null>"}) +
      ")";
}

// Debug string for node patterns.
std::string NodePattern::DebugString() const {
  std::string result = "(";

  if (!alias.empty()) {
    result += alias;
  }

  result += LabelsToString(labels);

  if (!properties.empty()) {
    if (!alias.empty() || !labels.empty()) {
      result += ' ';
    }

    result += PropertyMapToString(properties);
  }

  result += ")";
  return result;
}

std::string MatchEdgePattern::DebugString() const {
  std::string result;

  if (direction == EdgeDirection::Left) {
    result += "<-";
  } else {
    result += "-";
  }

  result += "[";

  if (!alias.empty()) {
    result += alias;
  }

  result += EdgeLabelsToString(labels);

  if (!properties.empty()) {
    if (!alias.empty() || !labels.empty()) {
      result += ' ';
    }
    result += PropertyMapToString(properties);
  }

  result += "]";

  if (direction == EdgeDirection::Right) {
    result += "->";
  } else {
    result += "-";
  }

  return result;
}

// Checks if the pattern element is a node.
bool PatternElement::IsNode() const {
  return std::holds_alternative<NodePattern>(element);
}

// Checks if the pattern element is an edge.
bool PatternElement::IsEdge() const {
  return std::holds_alternative<MatchEdgePattern>(element);
}

// Gets the node pattern from the pattern element.
const NodePattern& PatternElement::AsNode() const {
  return std::get<NodePattern>(element);
}

// Gets the edge pattern from the pattern element.
const MatchEdgePattern& PatternElement::AsEdge() const {
  return std::get<MatchEdgePattern>(element);
}

// Debug string for pattern elements.
std::string PatternElement::DebugString() const {
  return std::visit([](const auto& el) { return el.DebugString(); }, element);
}

std::string Pattern::DebugString() const {
  std::string result;

  for (const auto& el : elements) {
    result += el.DebugString();
  }

  return result;
}

// Debug string for Node References in CREATE clause.
std::string CreateNodeRef::DebugString() const {
  return "(" + alias + ")";
}

// Debug string for edge patterns in CREATE clause.
std::string CreateEdgePattern::DebugString() const {
  std::string result = left_node.DebugString();

  if (direction == EdgeDirection::Left) {
    result += "<-";
  } else {
    result += "-";
  }

  result += "[";

  if (!alias.empty()) {
    result += alias;
  }

  result += EdgeLabelsToString(labels);

  if (!properties.empty()) {
    if (!alias.empty() || !labels.empty()) {
      result += ' ';
    }

    result += PropertyMapToString(properties);
  }

  result += "]";

  if (direction == EdgeDirection::Right) {
    result += "->";
  } else {
    result += "-";
  }

  result += right_node.DebugString();
  return result;
}

// Debug string for MATCH clause.
std::string MatchClause::DebugString() const {
  std::string result = "MATCH ";

  for (size_t i = 0; i < patterns.size(); ++i) {
    result += patterns[i].DebugString();

    if (i + 1 < patterns.size()) {
      result += ", ";
    }
  }

  return result;
}

// Debug string for WHERE clause.
std::string WhereClause::DebugString() const {
  return expression ? "WHERE " + expression->DebugString() : "WHERE <null>";
}

// Debug string for RETURN items.
std::string ReturnItem::DebugString() const {
  if (std::holds_alternative<std::string>(item)) {
    return std::get<std::string>(item);
  }

  return std::get<PropertyExpr>(item).DebugString();
}

// Debug string for RETURN clause.
std::string ReturnClause::DebugString() const {
  std::string result = "RETURN ";

  for (size_t i = 0; i < items.size(); ++i) {
    result += items[i].DebugString();

    if (i + 1 < items.size()) {
      result += ", ";
    }
  }

  return result;
}

// Debug string for DELETE clause.
std::string DeleteClause::DebugString() const {
  std::string result = "DELETE ";

  for (size_t i = 0; i < aliases.size(); ++i) {
    result += aliases[i];

    if (i + 1 < aliases.size()) {
      result += ", ";
    }
  }

  return result;
}

// Debug string for SET clause.
std::string SetClause::DebugString() const {
  return "SET " + target.DebugString() + " = " + value.DebugString();
}

// Debug string for ORDER BY item.
std::string OrderItem::DebugString() const {
  std::string result = property.DebugString();

  if (direction == OrderDirection::Desc) {
    result += " DESC";
  }

  return result;
}

// Debug string for ORDER BY clause.
std::string OrderClause::DebugString() const {
  std::string result = "ORDER BY ";

  for (size_t i = 0; i < items.size(); ++i) {
    result += items[i].DebugString();

    if (i + 1 < items.size()) {
      result += ", ";
    }
  }

  return result;
}

// Debug string for LIMIT clause.
std::string LimitClause::DebugString() const {
  return "LIMIT " + std::to_string(limit);
}

// Debug string for CREATE clause.
std::string CreateClause::DebugString() const {
  std::string result = "CREATE ";

  for (size_t i = 0; i < created_items.size(); ++i) {
    result += std::visit([](const auto& item) { return item.DebugString(); }, created_items[i]);

    if (i + 1 < created_items.size()) {
      result += ", ";
    }
  }

  return result;
}

// Debug string for the entire query AST.
std::string QueryAST::DebugString() const {
  std::string result;
  
  if (match) {
    result += match->DebugString() + "\n";
  }

  if (where) {
    result += where->DebugString() + "\n";
  }

  if (return_clause) {
    result += return_clause->DebugString() + "\n";
  }

  if (delete_clause) {
    result += delete_clause->DebugString() + "\n";
  }

  if (set_clause) {
    result += set_clause->DebugString() + "\n";
  }

  if (order) {
    result += order->DebugString() + "\n";
  }

  if (limit) {
    result += limit->DebugString() + "\n";
  }

  return result;
}

} // namespace ast

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

} // namespace

Parser::Parser(const std::vector<Token>& tokens)
      : tokens_(std::move(tokens)) {}

/*// Parses a single query.
ast::QueryAST Parser::ParseSingle() {
  ast::QueryAST query = ParseQuery();
  ConsumeSemicolons();
  ExpectEnd();
  return query;
}

// Parses multiple queries separated by semicolons.
std::vector<ast::QueryAST> Parser::ParseAll() {
  std::vector<ast::QueryAST> queries;
  ConsumeSemicolons();

  while (!Check(TokenType::END)) {
    ast::QueryAST query = ParseQuery();
    Validate(query);
    queries.push_back(std::move(query));
    ConsumeSemicolons();
  }

  return queries;
}*/

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

} // namespace parser
