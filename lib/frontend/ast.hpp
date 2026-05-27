#pragma once

#include <common/span.hpp>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

enum class BinaryOp {
  Add,
  Sub,
  Mul,
  Div,
  Mod,
  Eq,
  NotEq,
  Lt,
  LtEq,
  Gt,
  GtEq,
  And,
  Or,
  Assign
};
enum class UnaryOp { Neg, Not };

#define MOVE_ONLY(T)                                                           \
  T() = default;                                                               \
  T(T &&) = default;                                                           \
  T &operator=(T &&) = default;                                                \
  T(const T &) = delete;                                                       \
  T &operator=(const T &) = delete;

struct IntLiteral {
  long long value;
  Span span;
};

struct FloatLiteral {
  double value;
  Span span;
};

struct CharLiteral {
  char value;
  Span span;
};

struct CharList {
  std::vector<char> chars;
  Span span;
};

struct Identifier {
  std::string name;
  Span span;
};

struct This {
  Span span;
};

struct Package {
  std::string name;
  Span span;
};

struct Import {
  std::string path;
  Span span;
};

struct Param {
  std::string name;
  Span span;
};

struct Block {
  std::vector<ExprPtr> exprs;
  Span span;
  MOVE_ONLY(Block)
};

struct Unary {
  UnaryOp op;
  ExprPtr rhs;
  Span span;
};

struct Binary {
  ExprPtr lhs;
  BinaryOp op;
  ExprPtr rhs;
  Span span;
};

struct Call {
  ExprPtr callee;
  std::vector<ExprPtr> args;
  Span span;
};

struct FieldAccess {
  ExprPtr object;
  std::string field;
  Span span;
};

struct Lambda {
  std::vector<Param> params;
  Block body;
  Span span;
};

struct If {
  ExprPtr cond;
  Block then;
  std::optional<Block> els;
  Span span;
  MOVE_ONLY(If)
};

struct Let {
  std::string name;
  std::vector<Param> params;
  ExprPtr value;
  bool is_public;
  Span span;
};

struct MatchArm {
  std::string constructor;
  std::vector<std::string> bindings;
  ExprPtr body;
  Span span;
};

struct Match {
  ExprPtr subject;
  std::vector<MatchArm> arms;
  Span span;
};

struct New {
  std::string type_name;
  std::vector<ExprPtr> args;
  Span span;
};

struct TypeVariant {
  std::string name;
  std::vector<std::string> fields;
  Span span;
};

struct TypeDecl {
  std::string name;
  std::vector<TypeVariant> variants;
  bool is_public;
  Span span;
};

struct StructField {
  std::string name;
  bool is_public;
  Span span;
};

struct StructMethod {
  std::string name;
  std::vector<Param> params;
  Block body;
  bool is_public, is_constructor;
  Span span;
};

struct StructDecl {
  std::string name;
  std::vector<StructField> fields;
  std::vector<StructMethod> methods;
  bool is_public;
  Span span;
};

using ExprVariant =
    std::variant<IntLiteral, FloatLiteral, CharLiteral, CharList, Identifier,
                 This, Package, Import, Unary, Binary, Call, FieldAccess,
                 Lambda, If, Block, Let, Match, New, TypeDecl, StructDecl>;

struct Expr {
  ExprVariant val;
  template <typename T> explicit Expr(T &&node) : val(std::forward<T>(node)) {}
};

template <typename T> ExprPtr makeExpr(T &&node) {
  return std::make_unique<Expr>(std::forward<T>(node));
}
template <typename V> auto visitExpr(V &&v, const Expr &e) {
  return std::visit(std::forward<V>(v), e.val);
}
template <typename V> auto visitExpr(V &&v, Expr &e) {
  return std::visit(std::forward<V>(v), e.val);
}
inline Span spanOf(const Expr &e) {
  return visitExpr([](const auto &n) { return n.span; }, e);
}