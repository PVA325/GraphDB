#pragma once

#include <string>

namespace lexer {

enum class TokenType {
  // Keywords.
  MATCH, WHERE, RETURN, DELETE, SET,
  CREATE, ORDER, BY, LIMIT,
  AND, OR,
  ASC, DESC,

  // Literals / identifiers.
  IDENTIFIER,
  STRING,
  NUMBER,
  TRUE,
  FALSE,

  // Symbols.
  COLON,        // :
  LPAREN,       // (
  RPAREN,       // )
  LBRACKET,     // [
  RBRACKET,     // ]
  LBRACE,       // {
  RBRACE,       // }

  COMMA,        // ,
  DOT,          // .
  SEMICOLON,    // ;

  DASH,         // -
  ARROW_RIGHT,  // ->
  ARROW_LEFT,   // <-

  EQUAL,        // =
  NOT_EQUAL,    // !=
  GREATER,      // >
  GREATER_EQUAL, // >=
  LESS,         // <
  LESS_EQUAL,   // <=

  END
};

struct Token {
  TokenType type;
  std::string lexeme;
  size_t line;
  size_t col;
};

} // namespace lexer
