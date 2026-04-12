#pragma once

#include <stdexcept>
#include <string>

class LexError : public std::runtime_error {
 public:
  LexError(size_t line, size_t col, const std::string& msg)
      : std::runtime_error(
          "Line " +
          std::to_string(line) +
          ", Column " +
          std::to_string(col) +
          ": " + msg) {}
};

class ParseError : public std::runtime_error {
 public:
  ParseError(size_t line, size_t col, const std::string& msg)
      : std::runtime_error(
          "Line " +
          std::to_string(line) +
          ", Column " +
          std::to_string(col) +
          ": " + msg) {}
};

class SemanticError : public std::runtime_error {
 public:
  SemanticError(size_t line, size_t col, const std::string& msg)
      : std::runtime_error(
          "Line " +
          std::to_string(line) +
          ", Column " +
          std::to_string(col) +
          ": " + msg) {}
};