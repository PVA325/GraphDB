#include "lexer/lexer.hpp"

#include <cctype>
#include <unordered_map>

#include "common/error.hpp"

namespace lexer {

class Lexer {
 public:
  explicit Lexer(const std::string& src)
      : source(src) {}

  // Scans the entire source and returns the list of tokens
  std::vector<Token> Run() {
    while (!IsAtEnd()) {
      start = current;
      start_col = col;
      ScanToken();
    }

    tokens.push_back({TokenType::END, "", line, col});
    return tokens;
  }

 private:
  // Input source and output tokens
  const std::string& source;
  std::vector<Token> tokens;

  // Current scanning position
  size_t start = 0;
  size_t current = 0;

  // For error reporting
  size_t line = 1;
  size_t col = 1;
  size_t start_col = 1;

  // Checks if we've reached the end of the source
  bool IsAtEnd() const {
    return current >= source.size();
  }

  // Advances and returns the next character
  char Advance() {
    char c = source[current++];
    col++;
    return c;
  }

  // Peeks at the current character without consuming it
  char Peek() const {
    if (IsAtEnd()) return '\0';
    return source[current];
  }

  // Peeks at the next character without consuming it
  char PeekNext() const {
    if (current + 1 >= source.size()) return '\0';
    return source[current + 1];
  }

  // Matches the expected character and advances if it matches
  bool Match(char expected) {
    if (IsAtEnd()) return false;
    if (source[current] != expected) return false;

    current++;
    col++;
    return true;
  }

  // Adds a token of the given type with the current lexeme
  void AddToken(TokenType type) {
    tokens.push_back({
      type,
      source.substr(start, current - start),
      line,
      start_col
    });
  }

  // Scans a single token and adds it to the list
  void ScanToken() {
    char c = Advance();

    switch (c) {
      case '(': AddToken(TokenType::LPAREN); break;
      case ')': AddToken(TokenType::RPAREN); break;
      case '[': AddToken(TokenType::LBRACKET); break;
      case ']': AddToken(TokenType::RBRACKET); break;
      case '{': AddToken(TokenType::LBRACE); break;
      case '}': AddToken(TokenType::RBRACE); break;

      case ',': AddToken(TokenType::COMMA); break;
      case '.': AddToken(TokenType::DOT); break;
      case ';': AddToken(TokenType::SEMICOLON); break;
      case ':': AddToken(TokenType::COLON); break;

      case '=': AddToken(TokenType::EQUAL); break;
      case '>': AddToken(TokenType::GREATER); break;
      case '<':
        if (Match('-')) {
          AddToken(TokenType::ARROW_LEFT);
        } else {
          AddToken(TokenType::LESS);
        }
        break;

      case '-':
        if (Match('>')) {
          AddToken(TokenType::ARROW_RIGHT);
        } else {
          AddToken(TokenType::DASH);
        }
        break;

      case '"': String(); break;

      case ' ':
      case '\r':
      case '\t':
        break;

      case '\n':
        line++;
        col = 1;
        break;

      default:
        if (std::isdigit(c)) {
          Number();
        } else if (std::isalpha(c) || c == '_') {
          Identifier();
        } else {
          throw LexError(line, col, "Unexpected character");
        }
    }
  }

  // Handles string literals enclosed in double quotes
  void String() {
    while (Peek() != '"' && !IsAtEnd()) {
      if (Peek() == '\n') {
        line++;
        col = 1;
      }
      Advance();
    }

    if (IsAtEnd()) {
      throw LexError(line, col, "Unterminated string");
    }

    Advance();

    AddToken(TokenType::STRING);
  }

  // Handles integer and floating-point literals
  void Number() {
    while (std::isdigit(Peek())) Advance();

    if (Peek() == '.' && std::isdigit(PeekNext())) {
      Advance();
      while (std::isdigit(Peek())) Advance();
    }

    AddToken(TokenType::NUMBER);
  }

  // Handles identifiers and keywords
  void Identifier() {
    while (std::isalnum(Peek()) || Peek() == '_') Advance();

    std::string text = source.substr(start, current - start);

    static const std::unordered_map<std::string, TokenType> keywords = {
        {"MATCH", TokenType::MATCH},
        {"WHERE", TokenType::WHERE},
        {"RETURN", TokenType::RETURN},
        {"DELETE", TokenType::DELETE},
        {"SET", TokenType::SET},
        {"CREATE", TokenType::CREATE},
        {"ORDER", TokenType::ORDER},
        {"BY", TokenType::BY},
        {"LIMIT", TokenType::LIMIT},
        {"AND", TokenType::AND},
        {"OR", TokenType::OR},
        {"ASC", TokenType::ASC},
        {"DESC", TokenType::DESC},
        {"TRUE", TokenType::TRUE},
        {"FALSE", TokenType::FALSE},
    };

    auto iter = keywords.find(text);
    if (iter != keywords.end()) {
      tokens.push_back({iter->second, text, line, start_col});
    } else {
      tokens.push_back({TokenType::IDENTIFIER, text, line, start_col});
    }
  }
};

// Public API function to lex the input source code
std::vector<Token> Lex(const std::string& source) {
  return Lexer(source).Run();
}

} // namespace lexer
