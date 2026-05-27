#pragma once

#include <frontend/ast.hpp>
#include <frontend/token.hpp>
#include <string>

inline std::string to_string(BinaryOp op) {
  switch (op) {
  case BinaryOp::Add:
    return "+";
  case BinaryOp::Sub:
    return "-";
  case BinaryOp::Mul:
    return "*";
  case BinaryOp::Div:
    return "/";
  case BinaryOp::Mod:
    return "%";
  case BinaryOp::Eq:
    return "==";
  case BinaryOp::NotEq:
    return "!=";
  case BinaryOp::Lt:
    return "<";
  case BinaryOp::LtEq:
    return "<=";
  case BinaryOp::Gt:
    return ">";
  case BinaryOp::GtEq:
    return ">=";
  case BinaryOp::And:
    return "&&";
  case BinaryOp::Or:
    return "||";
  case BinaryOp::Assign:
    return "=";
  }
  return "?";
}

inline std::string to_string(UnaryOp op) {
  switch (op) {
  case UnaryOp::Neg:
    return "-";
  case UnaryOp::Not:
    return "!";
  }
  return "?";
}

inline std::string to_string(LiteralKind lk) {
  switch (lk) {
  case LiteralKind::Integer:
    return "Integer";
  case LiteralKind::Float:
    return "Float";
  case LiteralKind::Char:
    return "Char";
  case LiteralKind::String:
    return "String";
  }
  return "Unknown";
}

inline std::string to_string(TokenKind kind, LiteralKind lk = {}) {
  switch (kind) {
  case TokenKind::Literal:
    return "Literal(" + to_string(lk) + ")";
  case TokenKind::Identifier:
    return "Identifier";
  case TokenKind::Keyword:
    return "Keyword";
  case TokenKind::Operator:
    return "Operator";
  case TokenKind::Delimiter:
    return "Delimiter";
  case TokenKind::Eof:
    return "Eof";
  }
  return "Unknown";
}