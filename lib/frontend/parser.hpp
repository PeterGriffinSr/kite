#pragma once

#include <common/cursor.hpp>
#include <common/error.hpp>
#include <common/source.hpp>
#include <frontend/ast.hpp>
#include <frontend/token.hpp>
#include <vector>

class Parser : private Cursor<Token> {
public:
  Parser(std::vector<Token> tokens, const SourceFile &file,
         ErrorReporter &reporter)
      : Cursor<Token>(nullptr, 0), tokens_(std::move(tokens)), file_(file),
        reporter_(reporter) {
    data_ = tokens_.data();
    size_ = tokens_.size();
  }

  std::vector<ExprPtr> parse();

private:
  std::vector<Token> tokens_;
  const SourceFile &file_;
  ErrorReporter &reporter_;

  ExprPtr parse_top_level();
  ExprPtr parse_package();
  ExprPtr parse_import();
  ExprPtr parse_type_decl(bool is_public);
  ExprPtr parse_struct_decl(bool is_public);
  ExprPtr parse_let(bool is_public);
  ExprPtr parse_expr();
  ExprPtr parse_assignment();
  ExprPtr parse_binary(int min_prec);
  ExprPtr parse_unary();
  ExprPtr parse_call(ExprPtr callee);
  ExprPtr parse_field_access(ExprPtr object);
  ExprPtr parse_primary();
  ExprPtr parse_block();
  ExprPtr parse_if();
  ExprPtr parse_match();
  ExprPtr parse_lambda();
  ExprPtr parse_new();
  MatchArm parse_match_arm();
  TypeVariant parse_type_variant();
  Param parse_param();
  std::vector<Param> parse_param_list();

  template <typename T, typename... Rest>
  bool check(T &&first, Rest &&...rest) const {
    return current().is(std::forward<T>(first), std::forward<Rest>(rest)...);
  }

  template <typename T, typename... Rest>
  bool match(T &&first, Rest &&...rest) {
    if (!check(std::forward<T>(first), std::forward<Rest>(rest)...))
      return false;
    advance();
    return true;
  }

  template <typename T, typename... Rest>
  Token expect(T &&first, Rest &&...rest) {
    if (!check(std::forward<T>(first), std::forward<Rest>(rest)...))
      error("expected ...", current());
    return advance();
  }

  bool is_at_end() const { return current().kind == TokenKind::Eof; }

  bool peek_is_method() {
    return check(TokenKind::Keyword, "let") && pos_ + 1 < size_ &&
           data_[pos_ + 1].kind == TokenKind::Identifier &&
           data_[pos_ + 2].is(TokenKind::Delimiter, "(");
  }

  void error(const std::string &msg, const Token &tok);
  void sync();
};