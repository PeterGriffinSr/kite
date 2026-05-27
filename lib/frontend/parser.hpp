#pragma once
#include <common/cursor.hpp>
#include <common/error.hpp>
#include <common/source.hpp>
#include <frontend/ast.hpp>
#include <frontend/token.hpp>
#include <stdexcept>
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
  ExprPtr parse_type_decl();
  ExprPtr parse_struct_decl();
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

  bool check(TokenKind k) const { return current().kind == k; }

  bool check_keyword(const std::string &s) const {
    return current().is_keyword(s);
  }

  bool check_operator(const std::string &s) const {
    return current().is_operator(s);
  }

  bool check_delimiter(const std::string &s) const {
    return current().is_delimiter(s);
  }

  bool match_keyword(const std::string &s) {
    if (!check_keyword(s))
      return false;
    advance();
    return true;
  }

  bool match_operator(const std::string &s) {
    if (!check_operator(s))
      return false;
    advance();
    return true;
  }

  bool match_delimiter(const std::string &s) {
    if (!check_delimiter(s))
      return false;
    advance();
    return true;
  }

  Token expect_keyword(const std::string &kw) {
    if (!check_keyword(kw)) {
      error("expected '" + kw + "'", current());
      throw std::runtime_error("parse error");
    }
    return advance();
  }

  Token expect_delimiter(const std::string &d) {
    if (!check_delimiter(d)) {
      error("expected '" + d + "'", current());
      throw std::runtime_error("parse error");
    }
    return advance();
  }

  Token expect_operator(const std::string &op) {
    if (!check_operator(op)) {
      error("expected '" + op + "'", current());
      throw std::runtime_error("parse error");
    }
    return advance();
  }

  Token expect_identifier() {
    if (!check(TokenKind::Identifier)) {
      error("expected identifier", current());
      throw std::runtime_error("parse error");
    }
    return advance();
  }

  bool is_at_end() const { return current().kind == TokenKind::Eof; }

  bool peek_is_method() {
    return check_keyword("let") && pos_ + 1 < size_ &&
           data_[pos_ + 1].kind == TokenKind::Identifier &&
           data_[pos_ + 2].is_delimiter("(");
  }

  void error(const std::string &msg, const Token &tok);
  void sync();
};