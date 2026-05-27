#pragma once
#include <common/cursor.hpp>
#include <common/error.hpp>
#include <common/source.hpp>
#include <frontend/token.hpp>
#include <optional>
#include <vector>

enum class LexState {
  Start,
  InIdentifier,
  InNumber,
  InFloat,
  InString,
  InStringEscape,
  InChar,
  InCharEscape,
  InCharClose,
  InLineComment,
};

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

  LexState state_ = LexState::Start;
  char char_value_ = 0;

  void step(std::vector<Token> &tokens, std::optional<char> c);

  using TokenValue = std::variant<std::pair<TokenKind, std::string>,
                                  std::pair<std::string, LiteralKind>,
                                  long long, double, char>;

  void emit_and_reset(std::vector<Token> &tokens, TokenValue value);
  void error(const std::string &msg);
  Span current_span() const { return {start_, pos_}; }

  template <typename... Args>
  void emit(std::vector<Token> &tokens, Args &&...args) {
    tokens.push_back(Token::make(std::forward<Args>(args)...));
  }
};