#pragma once
#include <common/span.hpp>
#include <string>
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

public:
  static Token make(TokenKind kind, std::string text, Span span) {
    return {kind, span, std::move(text)};
  }

  static Token make(std::string name, Span span) {
    return {TokenKind::Identifier, span, name, std::move(name)};
  }

  static Token make(long long v, Span span) {
    return {TokenKind::Literal, span, {}, v, LiteralKind::Integer};
  }

  static Token make(double v, Span span) {
    return {TokenKind::Literal, span, {}, v, LiteralKind::Float};
  }

  static Token make(char v, Span span) {
    return {TokenKind::Literal, span, {}, v, LiteralKind::Char};
  }

  static Token make(std::string v, Span span, LiteralKind lk) {
    return {TokenKind::Literal, span, {}, std::move(v), lk};
  }

  static Token make(Span span) { return {TokenKind::Eof, span}; }

  bool is(TokenKind k) const { return kind == k; }
  bool is_keyword(const std::string &kw) const {
    return kind == TokenKind::Keyword && text == kw;
  }

  bool is_operator(const std::string &op) const {
    return kind == TokenKind::Operator && text == op;
  }

  bool is_delimiter(const std::string &d) const {
    return kind == TokenKind::Delimiter && text == d;
  }

  bool is_literal(LiteralKind lk) const {
    return kind == TokenKind::Literal && literal_kind == lk;
  }
};