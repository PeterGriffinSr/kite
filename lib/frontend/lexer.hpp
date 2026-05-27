#pragma once

#include <common/cursor.hpp>
#include <common/error.hpp>
#include <common/source.hpp>
#include <frontend/token.hpp>
#include <vector>

class Lexer : private Cursor<char> {
public:
  Lexer(const SourceFile &file, ErrorReporter &reporter)
      : Cursor<char>(file.source.data(), file.source.size()), file_(file),
        reporter_(reporter) {}

  std::vector<Token> tokenize();

private:
  const SourceFile &file_;
  ErrorReporter &reporter_;
  size_t start_ = 0;
  size_t line_ = 1;
  size_t line_start_ = 0;

  void scan(std::vector<Token> &tokens);
  void scan_identifier_or_keyword(std::vector<Token> &tokens);
  void skip_line_comment();

  char peek_next() const { return peek(1); }
  Span current_span() const { return {start_, pos_}; }
  void error(const std::string &msg);

  template <typename... Args>
  void emit(std::vector<Token> &tokens, Args &&...args) {
    tokens.push_back(Token::make(std::forward<Args>(args)...));
  }
};