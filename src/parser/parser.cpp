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

// Compares two values with the given comparison operator.
bool CompareValues(const Value& lhs, const Value& rhs, CompareOp op) {
  return std::visit([&](auto&& l, auto&& r) -> bool {
    using L = std::decay_t<decltype(l)>;
    using R = std::decay_t<decltype(r)>;

    if constexpr ((std::is_same_v<L, int64_t> || std::is_same_v<L, double>) &&
                  (std::is_same_v<R, int64_t> || std::is_same_v<R, double>)) {
      double dl = static_cast<double>(l);
      double dr = static_cast<double>(r);

      switch (op) {
        case CompareOp::Eq: return dl == dr;
        case CompareOp::NotEqual: return dl != dr;
        case CompareOp::Lt: return dl < dr;
        case CompareOp::Le: return dl <= dr;
        case CompareOp::Gt: return dl > dr;
        case CompareOp::Ge: return dl >= dr;
      }
    }

    else if constexpr (std::is_same_v<L, std::string> &&
                       std::is_same_v<R, std::string>) {
      switch (op) {
        case CompareOp::Eq: return l == r;
        case CompareOp::NotEqual: return l != r;
        case CompareOp::Lt: return l < r;
        case CompareOp::Le: return l <= r;
        case CompareOp::Gt: return l > r;
        case CompareOp::Ge: return l >= r;
      }
    }

    else if constexpr (std::is_same_v<L, bool> &&
                       std::is_same_v<R, bool>) {
      switch (op) {
        case CompareOp::Eq: return l == r;
        case CompareOp::NotEqual: return l != r;
        default:
          throw std::runtime_error("Invalid comparison operator for boolean values");
      }
    }

    throw std::runtime_error("Cannot compare values of different or unsupported types");
  }, lhs, rhs);
}

// Converts a logical operator to a string for debug output.
std::string LogicalOpToString(LogicalOp op) {
  switch (op) {
    case LogicalOp::And: return "AND";
    case LogicalOp::Or: return "OR";
  }

  return "?";
}

// Converts a Value to a boolean for logical operations.
bool ToBool(const Value& v) {
  return std::visit([](auto&& val) -> bool {
    using T = std::decay_t<decltype(val)>;

    if constexpr (std::is_same_v<T, bool>) {
      return val;
    }

    if constexpr (std::is_same_v<T, int64_t>) {
      return val != 0;
    }

    if constexpr (std::is_same_v<T, double>) {
      return val != 0.0;
    }

    if constexpr (std::is_same_v<T, std::string>) {
      return !val.empty();
    }

    return false;
  }, v);
}

} // namespace

// Debug string implementations for AST nodes.
std::string Literal::DebugString() const {
  return "Literal(" + ValueToString(value) + ")";
}

// LiteralExpr evaluation simply returns the literal value.
Value LiteralExpr::operator()(const EvalContext& ctx) const {
  return literal.value;
}

// Debug string implementations for expressions and patterns.
std::string LiteralExpr::DebugString() const {
  return literal.DebugString();
}

// Evaluation of property access expressions retrieves the property value from the context.
Value PropertyExpr::operator()(const EvalContext& ctx) const {
  return ctx.GetProperty(alias, property);
  // throw std::runtime_error("Not implemented");
}

// Debug string for property access expressions.
std::string PropertyExpr::DebugString() const {
  return "Property(" + alias + "." + property + ")";
}

// Evaluation of comparison expressions.
Value ComparisonExpr::operator()(const EvalContext& ctx) const {
  Value l = (*left_expr)(ctx);
  Value r = (*right_expr)(ctx);

  return CompareValues(l, r, op);
}

// Debug string for comparison expressions.
std::string ComparisonExpr::DebugString() const {
  return "Compare(" +
      (left_expr ? left_expr->DebugString() : std::string{"<null>"}) +
      " " +
      CompareOpToString(op) +
      " " +
      (right_expr ? right_expr->DebugString() : std::string{"<null>"}) +
      ")";
}

// Evaluation of logical expressions.
Value LogicalExpr::operator()(const EvalContext& ctx) const {
  bool lhs = ToBool((*left_expr)(ctx));

  if (op == LogicalOp::And) {
    if (!lhs) {
      return false;
    }

    return lhs && ToBool((*right_expr)(ctx));
  }

  if (op == LogicalOp::Or) {
    if (lhs) {
      return true;
    }

    return lhs || ToBool((*right_expr)(ctx));
  }

  throw std::runtime_error("Unknown logical operator");
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

  result += EdgeLabelsToString(label);

  if (!properties.empty()) {
    if (!alias.empty() || !label.empty()) {
      result += ' ';
    }

    result += PropertyMapToString(properties);

    result += "]";

    if (direction == EdgeDirection::Right) {
      result += "->";
    } else {
      result += "-";
    }
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

  result += EdgeLabelsToString(label);

  if (!properties.empty()) {
    if (!alias.empty() || !label.empty()) {
      result += ' ';
    }

    result += PropertyMapToString(properties);

    result += "]";

    if (direction == EdgeDirection::Right) {
      result += "->";
    } else {
      result += "-";
    }

    result += right_node.DebugString();
  }
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
  if (std::holds_alternative<std::string>(return_item)) {
    return std::get<std::string>(return_item);
  }

  return std::get<PropertyExpr>(return_item).DebugString();
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

// Debug string for SET item.
std::string SetItem::DebugString() const {
  std::string result = alias;

  for (const auto& [prop, value] : properties) {
    result += "." + prop + "=" + value.DebugString();
  }

  for (const auto& label : labels) {
    result += ":" + label;
  }

  return result;
}

// Debug string for SET clause.
std::string SetClause::DebugString() const {
  std::string result = "SET ";

  for (size_t i = 0; i < items.size(); ++i) {
    result += items[i].DebugString();

    if (i + 1 < items.size()) {
      result += ", ";
    }
  }

  return result;
}

// Debug string for ORDER BY item.
std::string OrderItem::DebugString() const {
  std::string result = property.DebugString();

  if (direction == OrderDirection::Desc) {
    result += " DESC";
  } else {
    result += " ASC";
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
  
  if (match_clause) {
    result += match_clause->DebugString() + "\n";
  }

  if (where_clause) {
    result += where_clause->DebugString() + "\n";
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

  if (order_clause) {
    result += order_clause->DebugString() + "\n";
  }

  if (limit_clause) {
    result += limit_clause->DebugString() + "\n";
  }

  return result;
}

// Сollects all aliases in a request into a vector
void ExprAnalysis::CollectAliases(const Expr* expr) {
  if (!expr) {
    return;
  }

  if (auto prop = dynamic_cast<const PropertyExpr*>(expr)) {
    // if (std::find(aliases.begin(), aliases.end(), prop->alias) == aliases.end()) {
      aliases.emplace_back(prop->alias);
    // }
  } else if (auto cmp = dynamic_cast<const ComparisonExpr*>(expr)) {
    CollectAliases(cmp->left_expr.get());
    CollectAliases(cmp->right_expr.get());
  } else if (auto log = dynamic_cast<const LogicalExpr*>(expr)) {
    CollectAliases(log->left_expr.get());
    CollectAliases(log->right_expr.get());
  } else {
    // unknown expr type, do nothing
  }
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
