#include "parser/parser.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

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

}  // namespace parser