#include <frontend/parser.hpp>
#include <stdexcept>
#include <unordered_map>

static Block take_block(ExprPtr ptr) {
  return std::move(std::get<Block>(ptr->val));
}

struct OpInfo {
  BinaryOp op;
  int prec;
};

static std::optional<OpInfo> get_op(const Token &t) {
  static const std::unordered_map<std::string, OpInfo> table = {
      {"||", {BinaryOp::Or, 1}},   {"&&", {BinaryOp::And, 2}},
      {"==", {BinaryOp::Eq, 3}},   {"!=", {BinaryOp::NotEq, 3}},
      {"<", {BinaryOp::Lt, 4}},    {">", {BinaryOp::Gt, 4}},
      {"<=", {BinaryOp::LtEq, 4}}, {">=", {BinaryOp::GtEq, 4}},
      {"+", {BinaryOp::Add, 5}},   {"-", {BinaryOp::Sub, 5}},
      {"*", {BinaryOp::Mul, 6}},   {"/", {BinaryOp::Div, 6}},
      {"%", {BinaryOp::Mod, 6}},
  };
  if (t.kind != TokenKind::Operator)
    return std::nullopt;
  auto it = table.find(t.text);
  return it != table.end() ? std::optional(it->second) : std::nullopt;
}

std::vector<ExprPtr> Parser::parse() {
  std::vector<ExprPtr> program;
  while (!is_at_end()) {
    try {
      program.push_back(parse_top_level());
    } catch (const std::runtime_error &) {
      sync();
    }
  }
  return program;
}

ExprPtr Parser::parse_top_level() {
  if (check_keyword("package"))
    return parse_package();
  if (check_keyword("import"))
    return parse_import();
  if (check_keyword("type"))
    return parse_type_decl();
  if (check_keyword("struct"))
    return parse_struct_decl();

  bool is_public = false;
  if (check_keyword("public")) {
    advance();
    is_public = true;
  } else if (check_keyword("private")) {
    advance();
  }

  if (check_keyword("let"))
    return parse_let(is_public);
  error("expected top-level declaration", current());
  throw std::runtime_error("parse error");
}

ExprPtr Parser::parse_package() {
  Token start = advance(), name = expect_identifier();
  return makeExpr(Package{name.text, Span::merge(start.span, name.span)});
}

ExprPtr Parser::parse_import() {
  Token start = advance();
  if (!current().is_literal(LiteralKind::String)) {
    error("expected string path after 'import'", current());
    throw std::runtime_error("parse error");
  }
  Token path = advance();
  return makeExpr(Import{std::get<std::string>(path.value),
                         Span::merge(start.span, path.span)});
}

ExprPtr Parser::parse_type_decl() {
  Token start = advance(), name = expect_identifier();
  expect_operator("=");
  std::vector<TypeVariant> variants;
  variants.push_back(parse_type_variant());
  while (match_operator("|"))
    variants.push_back(parse_type_variant());
  return makeExpr(TypeDecl{name.text, std::move(variants),
                           Span::merge(start.span, variants.back().span)});
}

TypeVariant Parser::parse_type_variant() {
  Token name = expect_identifier();
  std::vector<std::string> fields;
  if (match_delimiter("(")) {
    while (!check_delimiter(")") && !is_at_end()) {
      fields.push_back(expect_identifier().text);
      if (!match_delimiter(","))
        break;
    }
    expect_delimiter(")");
  }
  return TypeVariant{name.text, std::move(fields), name.span};
}

ExprPtr Parser::parse_struct_decl() {
  Token start = advance(), name = expect_identifier();
  expect_delimiter("{");
  std::vector<StructField> fields;
  std::vector<StructMethod> methods;

  while (!check_delimiter("}") && !is_at_end()) {
    bool pub = false;
    if (check_keyword("public")) {
      advance();
      pub = true;
    } else if (check_keyword("private")) {
      advance();
    }

    if (check_keyword("new")) {
      Token t = advance();
      methods.push_back(StructMethod{t.text, parse_param_list(),
                                     take_block(parse_block()), pub, true,
                                     t.span});
    } else if (peek_is_method()) {
      advance();
      Token t = expect_identifier();
      methods.push_back(StructMethod{t.text, parse_param_list(),
                                     take_block(parse_block()), pub, false,
                                     t.span});
    } else {
      Token t = expect_identifier();
      fields.push_back(StructField{t.text, pub, t.span});
      match_delimiter(",");
    }
  }

  Token end = expect_delimiter("}");
  return makeExpr(StructDecl{name.text, std::move(fields), std::move(methods),
                             Span::merge(start.span, end.span)});
}

ExprPtr Parser::parse_let(bool is_public) {
  Token start = advance(), name = expect_identifier();
  if (check_delimiter("(") || check_delimiter("()")) {
    auto params = check_delimiter("()") ? (advance(), std::vector<Param>{})
                                        : parse_param_list();
    Block body = take_block(parse_block());
    Span span = Span::merge(start.span, body.span);
    return makeExpr(Let{name.text, std::move(params), makeExpr(std::move(body)),
                        is_public, span});
  }
  expect_operator("=");
  ExprPtr value = parse_expr();
  Span span = Span::merge(start.span, spanOf(*value));
  return makeExpr(Let{name.text, {}, std::move(value), is_public, span});
}

ExprPtr Parser::parse_expr() { return parse_assignment(); }

ExprPtr Parser::parse_assignment() {
  auto lhs = parse_binary(0);
  if (check_operator("=")) {
    advance();
    auto rhs = parse_assignment();
    Span span = Span::merge(spanOf(*lhs), spanOf(*rhs));
    return makeExpr(
        Binary{std::move(lhs), BinaryOp::Assign, std::move(rhs), span});
  }
  return lhs;
}

ExprPtr Parser::parse_binary(int min_prec) {
  auto lhs = parse_unary();
  while (true) {
    auto info = get_op(current());
    if (!info || info->prec < min_prec)
      break;
    advance();
    auto rhs = parse_binary(info->prec + 1);
    Span span = Span::merge(spanOf(*lhs), spanOf(*rhs));
    lhs = makeExpr(Binary{std::move(lhs), info->op, std::move(rhs), span});
  }
  return lhs;
}

ExprPtr Parser::parse_unary() {
  if (check_operator("-") || check_operator("!")) {
    Token op = advance();
    auto rhs = parse_unary();
    Span span = Span::merge(op.span, spanOf(*rhs));
    return makeExpr(Unary{op.text == "-" ? UnaryOp::Neg : UnaryOp::Not,
                          std::move(rhs), span});
  }
  auto expr = parse_primary();
  while (true) {
    if (check_delimiter("("))
      expr = parse_call(std::move(expr));
    else if (check_delimiter("."))
      expr = parse_field_access(std::move(expr));
    else
      break;
  }
  return expr;
}

ExprPtr Parser::parse_call(ExprPtr callee) {
  advance();
  std::vector<ExprPtr> args;
  while (!check_delimiter(")") && !is_at_end()) {
    args.push_back(parse_expr());
    if (!match_delimiter(","))
      break;
  }
  Token close = expect_delimiter(")");
  Span span = Span::merge(spanOf(*callee), close.span);
  return makeExpr(Call{std::move(callee), std::move(args), span});
}

ExprPtr Parser::parse_field_access(ExprPtr object) {
  advance();
  Token field = expect_identifier();
  Span span = Span::merge(spanOf(*object), field.span);
  return makeExpr(FieldAccess{std::move(object), field.text, span});
}

ExprPtr Parser::parse_primary() {
  const Token &tok = current();
  if (tok.is_literal(LiteralKind::Integer)) {
    Token t = advance();
    return makeExpr(IntLiteral{std::get<long long>(t.value), t.span});
  }
  if (tok.is_literal(LiteralKind::Float)) {
    Token t = advance();
    return makeExpr(FloatLiteral{std::get<double>(t.value), t.span});
  }
  if (tok.is_literal(LiteralKind::Char)) {
    Token t = advance();
    return makeExpr(CharLiteral{std::get<char>(t.value), t.span});
  }
  if (tok.is_literal(LiteralKind::String)) {
    Token t = advance();
    std::string s = std::get<std::string>(t.value);
    return makeExpr(CharList{std::vector<char>(s.begin(), s.end()), t.span});
  }
  if (check_keyword("if"))
    return parse_if();
  if (check_keyword("match"))
    return parse_match();
  if (check_keyword("new"))
    return parse_new();
  if (check_keyword("this")) {
    Token t = advance();
    return makeExpr(This{t.span});
  }
  if (check_keyword("let"))
    return parse_let(false);
  if (check_operator("|"))
    return parse_lambda();
  if (check_delimiter("{"))
    return parse_block();
  if (check_delimiter("()")) {
    Token t = advance();
    return makeExpr(CharList{{}, t.span});
  }
  if (check_delimiter("(")) {
    advance();
    auto e = parse_expr();
    expect_delimiter(")");
    return e;
  }
  if (tok.is(TokenKind::Identifier)) {
    Token t = advance();
    return makeExpr(Identifier{t.text, t.span});
  }
  error("unexpected token '" + tok.text + "'", tok);
  throw std::runtime_error("parse error");
}

ExprPtr Parser::parse_block() {
  Token start = expect_delimiter("{");
  std::vector<ExprPtr> exprs;
  while (!check_delimiter("}") && !is_at_end()) {
    if (check_keyword("public") || check_keyword("private") ||
        check_keyword("struct") || check_keyword("type") ||
        check_keyword("import") || check_keyword("package")) {
      error("unexpected token '" + current().text + "' — missing '}'?",
            current());
      break;
    }
    try {
      exprs.push_back(parse_expr());
    } catch (const std::runtime_error &) {
      sync();
    }
  }
  Token end = expect_delimiter("}");
  return makeExpr(Block{std::move(exprs), Span::merge(start.span, end.span)});
}

ExprPtr Parser::parse_if() {
  Token start = advance();
  auto cond = parse_expr();
  Block then = take_block(parse_block());
  std::optional<Block> els;
  if (match_keyword("else"))
    els = take_block(parse_block());
  Span span = els ? Span::merge(start.span, els->span)
                  : Span::merge(start.span, then.span);
  return makeExpr(If{std::move(cond), std::move(then), std::move(els), span});
}

ExprPtr Parser::parse_match() {
  Token start = advance();
  auto subject = parse_expr();
  expect_delimiter("{");
  std::vector<MatchArm> arms;
  while (!check_delimiter("}") && !is_at_end()) {
    arms.push_back(parse_match_arm());
    match_delimiter(",");
  }
  Token end = expect_delimiter("}");
  return makeExpr(Match{std::move(subject), std::move(arms),
                        Span::merge(start.span, end.span)});
}

MatchArm Parser::parse_match_arm() {
  Span arm_span = current().span;
  if (check_delimiter("_")) {
    advance();
    expect_operator("=>");
    return MatchArm{"_", {}, parse_expr(), arm_span};
  }
  if (current().is_literal(LiteralKind::Integer)) {
    Token t = advance();
    expect_operator("=>");
    return MatchArm{std::to_string(std::get<long long>(t.value)),
                    {},
                    parse_expr(),
                    arm_span};
  }
  Token name = expect_identifier();
  std::vector<std::string> bindings;
  if (match_delimiter("(")) {
    while (!check_delimiter(")") && !is_at_end()) {
      bindings.push_back(check_delimiter("_") ? (advance(), std::string("_"))
                                              : expect_identifier().text);
      if (!match_delimiter(","))
        break;
    }
    expect_delimiter(")");
  }
  expect_operator("=>");
  return MatchArm{name.text, std::move(bindings), parse_expr(), name.span};
}

ExprPtr Parser::parse_lambda() {
  Token start = advance();
  std::vector<Param> params;
  while (!check_operator("|") && !is_at_end()) {
    params.push_back(parse_param());
    if (!match_delimiter(","))
      break;
  }
  expect_operator("|");
  Block body = take_block(parse_block());
  return makeExpr(Lambda{std::move(params), std::move(body),
                         Span::merge(start.span, body.span)});
}

ExprPtr Parser::parse_new() {
  Token start = advance(), name = expect_identifier();
  expect_delimiter("(");
  std::vector<ExprPtr> args;
  while (!check_delimiter(")") && !is_at_end()) {
    args.push_back(parse_expr());
    if (!match_delimiter(","))
      break;
  }
  Token end = expect_delimiter(")");
  return makeExpr(
      New{name.text, std::move(args), Span::merge(start.span, end.span)});
}

Param Parser::parse_param() {
  Token t = expect_identifier();
  return Param{t.text, t.span};
}

std::vector<Param> Parser::parse_param_list() {
  expect_delimiter("(");
  std::vector<Param> params;
  while (!check_delimiter(")") && !is_at_end()) {
    params.push_back(parse_param());
    if (!match_delimiter(","))
      break;
  }
  expect_delimiter(")");
  return params;
}

void Parser::error(const std::string &msg, const Token &tok) {
  reporter_.error(msg,
                  SourceLocation{file_.filename, file_.line_at(tok.span.start),
                                 file_.column_at(tok.span.start)},
                  file_.line_text_at(tok.span.start));
}

void Parser::sync() {
  if (!is_at_end())
    advance();
  while (!is_at_end()) {
    if (check_keyword("public") || check_keyword("private") ||
        check_keyword("let") || check_keyword("type") ||
        check_keyword("struct") || check_keyword("import") ||
        check_keyword("package") || check_delimiter("}"))
      return;
    advance();
  }
}