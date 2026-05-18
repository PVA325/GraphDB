#include "parser/lexer.hpp"

#include <unordered_map>

#include "parser/error.hpp"

namespace lexer {

namespace {

class Lexer {
 public:
  explicit Lexer(const std::string& src)
      : source_(src) {}

  // Scans the entire source and returns the list of tokens.
  std::vector<Token> Run() {
    while (!IsAtEnd()) {
      start = current_;
      start_col_ = col_;

      ScanToken();
    }

    tokens_.push_back({TokenType::END, "", line_, col_});

    return tokens_;
  }

 private:
  // Checks if we've reached the end of the source.
  bool IsAtEnd() const {
    return current_ >= source_.size();
  }

  // Advances and returns the next character.
  char Advance() {
    char c = source_[current_++];
    col_++;

    return c;
  }

  // Peeks at the current character without consuming it.
  char Peek() const {
    if (IsAtEnd()) return '\0';

    return source_[current_];
  }

  // Peeks at the next character without consuming it.
  char PeekNext() const {
    if (current_ + 1 >= source_.size()) return '\0';

    return source_[current_ + 1];
  }

  // Matches the expected character and advances if it matches.
  bool Match(char expected) {
    if (IsAtEnd()) return false;

    if (source_[current_] != expected) return false;

    current_++;
    col_++;

    return true;
  }

  // Adds a token of the given type with the current lexeme.
  void AddToken(TokenType type) {
    tokens_.push_back({
      type,
      source_.substr(start, current_ - start),
      line_,
      start_col_
    });
  }

  // Scans a single token and adds it to the list.
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

      case '!':
        if (Match('=')) {
          AddToken(TokenType::NOT_EQUAL);
        } else {
          throw LexError(line_, col_, "Unexpected character '!' without '='");
        }
        break;

      case '=': AddToken(TokenType::EQUAL); break;

      case '>': 
        if (Match('=')) {
          AddToken(TokenType::GREATER_EQUAL);
        } else {
          AddToken(TokenType::GREATER);
        }
        break;

      case '<':
        if (Match('-')) {
          AddToken(TokenType::ARROW_LEFT);
        } else if (Match('=')) {
          AddToken(TokenType::LESS_EQUAL);
        }  else {
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
        line_++;
        col_ = 1;
        break;

      default:
        if (std::isdigit(c)) {
          Number();
        } else if (std::isalpha(c) || c == '_') {
          Identifier();
        } else {
          throw LexError(line_, col_, "Unexpected character");
        }
    }
  }

  // Handles string literals enclosed in double quotes.
  void String() {
    while (Peek() != '"' && !IsAtEnd()) {
      if (Peek() == '\n') {
        line_++;
        col_ = 1;
      }

      Advance();
    }

    if (IsAtEnd()) {
      throw LexError(line_, col_, "Unterminated string");
    }

    Advance();

    AddToken(TokenType::STRING);
  }

  // Handles integer and floating-point literals.
  void Number() {
    while (std::isdigit(Peek())) Advance();

    if (Peek() == '.' && std::isdigit(PeekNext())) {
      Advance();

      while (std::isdigit(Peek())) Advance();
    }

    AddToken(TokenType::NUMBER);
  }

  // Handles identifiers and keywords.
  void Identifier() {
    while (std::isalnum(Peek()) || Peek() == '_') Advance();

    std::string text = source_.substr(start, current_ - start);

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
        {"true", TokenType::TRUE},
        {"false", TokenType::FALSE},
    };

    auto iter = keywords.find(text);

    if (iter != keywords.end()) {
      tokens_.push_back({iter->second, text, line_, start_col_});
    } else {
      tokens_.push_back({TokenType::IDENTIFIER, text, line_, start_col_});
    }
  }

  // Input source and output tokens.
  const std::string& source_;
  std::vector<Token> tokens_;

  // Current scanning position.
  size_t start = 0;
  size_t current_ = 0;

  // For error reporting.
  size_t line_ = 1;
  size_t col_ = 1;
  size_t start_col_ = 1;
};

} // namespace

// Public API function to lex the input source code.
std::vector<Token> Lex(const std::string& source_) {
  return Lexer(source_).Run();
}

} // namespace lexer
