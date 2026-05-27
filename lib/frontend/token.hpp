#pragma once

#include <common/span.hpp>
#include <variant>

enum class TokenKind { Literal, Identifier, Keyword, Operator, Delimiter, Eof };
enum class LiteralKind { Integer, Float, Char, String };

using TokenValue =
    std::variant<std::monostate, long long, double, char, std::string>;

struct Token {
  TokenKind kind;
  Span span;
  TokenValue value;
  LiteralKind literal_kind = {};
  std::string text;

private:
  Token(TokenKind kind, Span span, std::string text = {}, TokenValue value = {},
        LiteralKind lk = {})
      : kind(kind), span(span), value(std::move(value)), literal_kind(lk),
        text(std::move(text)) {}

  using TokenInit = std::variant<std::pair<TokenKind, std::string>,
                                 std::pair<std::string, LiteralKind>, long long,
                                 double, char, std::monostate>;

public:
  static Token make(TokenInit init, Span span) {
    return std::visit(
        [&](auto &&v) -> Token {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, std::pair<TokenKind, std::string>>)
            return {v.first, span, std::move(v.second)};
          else if constexpr (std::is_same_v<
                                 T, std::pair<std::string, LiteralKind>>)
            return {TokenKind::Literal, span, {}, std::move(v.first), v.second};
          else if constexpr (std::is_same_v<T, long long>)
            return {TokenKind::Literal, span, {}, v, LiteralKind::Integer};
          else if constexpr (std::is_same_v<T, double>)
            return {TokenKind::Literal, span, {}, v, LiteralKind::Float};
          else if constexpr (std::is_same_v<T, char>)
            return {TokenKind::Literal, span, {}, v, LiteralKind::Char};
          else
            return {TokenKind::Eof, span};
        },
        init);
  }

  template <typename T, typename... Rest>
  bool is(T &&first, Rest &&...rest) const {
    if constexpr (std::is_same_v<std::decay_t<T>, LiteralKind>)
      return kind == TokenKind::Literal && literal_kind == first;
    else if constexpr (std::is_same_v<std::decay_t<T>, TokenKind>) {
      if constexpr (sizeof...(rest) == 1)
        return kind == first && text == std::get<0>(std::tie(rest...));
      else
        return kind == first;
    } else {
      static_assert(sizeof(T) == 0,
                    "is() requires TokenKind or LiteralKind as first");
    }
  }
};