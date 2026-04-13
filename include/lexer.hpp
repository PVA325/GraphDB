#pragma once

#include <string>
#include <vector>

#include "token.hpp"

namespace lexer {

std::vector<Token> Lex(const std::string& source);

} // namespace lexer
