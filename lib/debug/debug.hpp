#pragma once

#include <frontend/ast.hpp>
#include <frontend/token.hpp>
#include <vector>

namespace Debug {
void print_tokens(const std::vector<Token> &tokens);
void print_ast(const std::vector<ExprPtr> &program);
} // namespace Debug