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
  if (check(TokenKind::Keyword, "package"))
    return parse_package();
  if (check(TokenKind::Keyword, "import"))
    return parse_import();

  bool is_public = false;
  if (check(TokenKind::Keyword, "public")) {
    advance();
    is_public = true;
  }
  if (check(TokenKind::Keyword, "type"))
    return parse_type_decl(is_public);
  if (check(TokenKind::Keyword, "struct"))
    return parse_struct_decl(is_public);
  if (check(TokenKind::Keyword, "let"))
    return parse_let(is_public);
  error("expected top-level declaration", current());
  throw std::runtime_error("parse error");
}

ExprPtr Parser::parse_package() {
  Token start = advance(), name = expect(TokenKind::Identifier);
  return makeExpr(Package{name.text, Span::merge(start.span, name.span)});
}

ExprPtr Parser::parse_import() {
  Token start = advance();
  if (!current().is(LiteralKind::String)) {
    error("expected string path after 'import'", current());
    throw std::runtime_error("parse error");
  }
  Token path = advance();
  return makeExpr(Import{std::get<std::string>(path.value),
                         Span::merge(start.span, path.span)});
}

ExprPtr Parser::parse_type_decl(bool is_public) {
  Token start = advance(), name = expect(TokenKind::Identifier);
  expect(TokenKind::Operator, "=");
  std::vector<TypeVariant> variants;
  variants.push_back(parse_type_variant());
  while (match(TokenKind::Operator, "|"))
    variants.push_back(parse_type_variant());
  return makeExpr(TypeDecl{name.text, std::move(variants), is_public,
                           Span::merge(start.span, variants.back().span)});
}

TypeVariant Parser::parse_type_variant() {
  Token name = expect(TokenKind::Identifier);
  std::vector<std::string> fields;
  if (match(TokenKind::Delimiter, "(")) {
    while (!check(TokenKind::Delimiter, ")") && !is_at_end()) {
      fields.push_back(expect(TokenKind::Identifier).text);
      if (!match(TokenKind::Delimiter, ","))
        break;
    }
    expect(TokenKind::Delimiter, ")");
  }
  return TypeVariant{name.text, std::move(fields), name.span};
}

ExprPtr Parser::parse_struct_decl(bool is_public) {
  Token start = advance(), name = expect(TokenKind::Identifier);
  expect(TokenKind::Delimiter, "{");
  std::vector<StructField> fields;
  std::vector<StructMethod> methods;

  while (!check(TokenKind::Delimiter, "}") && !is_at_end()) {
    bool pub = false;
    if (check(TokenKind::Keyword, "public")) {
      advance();
      pub = true;
    }

    if (check(TokenKind::Keyword, "new")) {
      Token t = advance();
      methods.push_back(StructMethod{t.text, parse_param_list(),
                                     take_block(parse_block()), pub, true,
                                     t.span});
    } else if (peek_is_method()) {
      advance();
      Token t = expect(TokenKind::Identifier);
      methods.push_back(StructMethod{t.text, parse_param_list(),
                                     take_block(parse_block()), pub, false,
                                     t.span});
    } else {
      Token t = expect(TokenKind::Identifier);
      fields.push_back(StructField{t.text, pub, t.span});
      match(TokenKind::Delimiter, ",");
    }
  }

  Token end = expect(TokenKind::Delimiter, "}");

  return makeExpr(StructDecl{name.text, std::move(fields), std::move(methods),
                             is_public, Span::merge(start.span, end.span)});
}

ExprPtr Parser::parse_let(bool is_public) {
  Token start = advance(), name = expect(TokenKind::Identifier);
  if (check(TokenKind::Delimiter, "(") || check(TokenKind::Delimiter, "()")) {
    auto params = check(TokenKind::Delimiter, "()")
                      ? (advance(), std::vector<Param>{})
                      : parse_param_list();
    Block body = take_block(parse_block());
    Span span = Span::merge(start.span, body.span);
    return makeExpr(Let{name.text, std::move(params), makeExpr(std::move(body)),
                        is_public, span});
  }
  expect(TokenKind::Operator, "=");
  ExprPtr value = parse_expr();
  Span span = Span::merge(start.span, spanOf(*value));
  return makeExpr(Let{name.text, {}, std::move(value), is_public, span});
}

ExprPtr Parser::parse_expr() { return parse_assignment(); }

ExprPtr Parser::parse_assignment() {
  auto lhs = parse_binary(0);
  if (check(TokenKind::Operator, "=")) {
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
  if (check(TokenKind::Operator, "-") || check(TokenKind::Operator, "!")) {
    Token op = advance();
    auto rhs = parse_unary();
    Span span = Span::merge(op.span, spanOf(*rhs));
    return makeExpr(Unary{op.text == "-" ? UnaryOp::Neg : UnaryOp::Not,
                          std::move(rhs), span});
  }
  auto expr = parse_primary();
  while (true) {
    if (check(TokenKind::Delimiter, "("))
      expr = parse_call(std::move(expr));
    else if (check(TokenKind::Delimiter, "."))
      expr = parse_field_access(std::move(expr));
    else
      break;
  }
  return expr;
}

ExprPtr Parser::parse_call(ExprPtr callee) {
  advance();
  std::vector<ExprPtr> args;
  while (!check(TokenKind::Operator, ")") && !is_at_end()) {
    args.push_back(parse_expr());
    if (!match(TokenKind::Delimiter, ","))
      break;
  }
  Token close = expect(TokenKind::Delimiter, ")");
  Span span = Span::merge(spanOf(*callee), close.span);
  return makeExpr(Call{std::move(callee), std::move(args), span});
}

ExprPtr Parser::parse_field_access(ExprPtr object) {
  advance();
  Token field = expect(TokenKind::Identifier);
  Span span = Span::merge(spanOf(*object), field.span);
  return makeExpr(FieldAccess{std::move(object), field.text, span});
}

ExprPtr Parser::parse_primary() {
  const Token &tok = current();
  if (tok.is(LiteralKind::Integer)) {
    Token t = advance();
    return makeExpr(IntLiteral{std::get<long long>(t.value), t.span});
  }
  if (tok.is(LiteralKind::Float)) {
    Token t = advance();
    return makeExpr(FloatLiteral{std::get<double>(t.value), t.span});
  }
  if (tok.is(LiteralKind::Char)) {
    Token t = advance();
    return makeExpr(CharLiteral{std::get<char>(t.value), t.span});
  }
  if (tok.is(LiteralKind::String)) {
    Token t = advance();
    std::string s = std::get<std::string>(t.value);
    return makeExpr(CharList{std::vector<char>(s.begin(), s.end()), t.span});
  }
  if (check(TokenKind::Keyword, "if"))
    return parse_if();
  if (check(TokenKind::Keyword, "match"))
    return parse_match();
  if (check(TokenKind::Keyword, "new"))
    return parse_new();
  if (check(TokenKind::Keyword, "this")) {
    Token t = advance();
    return makeExpr(This{t.span});
  }
  if (check(TokenKind::Keyword, "let"))
    return parse_let(false);
  if (check(TokenKind::Operator, "|"))
    return parse_lambda();
  if (check(TokenKind::Delimiter, "{"))
    return parse_block();
  if (check(TokenKind::Delimiter, "()")) {
    Token t = advance();
    return makeExpr(CharList{{}, t.span});
  }
  if (check(TokenKind::Delimiter, "(")) {
    advance();
    auto e = parse_expr();
    expect(TokenKind::Delimiter, ")");
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
  Token start = expect(TokenKind::Delimiter, "{");
  std::vector<ExprPtr> exprs;
  while (!check(TokenKind::Delimiter, "}") && !is_at_end()) {
    if (check(TokenKind::Keyword, "public") ||
        check(TokenKind::Keyword, "struct") ||
        check(TokenKind::Keyword, "type") ||
        check(TokenKind::Keyword, "import") ||
        check(TokenKind::Keyword, "package")) {
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
  Token end = expect(TokenKind::Delimiter, "}");
  return makeExpr(Block{std::move(exprs), Span::merge(start.span, end.span)});
}

ExprPtr Parser::parse_if() {
  Token start = advance();
  auto cond = parse_expr();
  Block then = take_block(parse_block());
  std::optional<Block> els;
  if (match(TokenKind::Keyword, "else"))
    els = take_block(parse_block());
  Span span = els ? Span::merge(start.span, els->span)
                  : Span::merge(start.span, then.span);
  return makeExpr(If{std::move(cond), std::move(then), std::move(els), span});
}

ExprPtr Parser::parse_match() {
  Token start = advance();
  auto subject = parse_expr();
  expect(TokenKind::Delimiter, "{");
  std::vector<MatchArm> arms;
  while (!check(TokenKind::Delimiter, "}") && !is_at_end()) {
    arms.push_back(parse_match_arm());
    match(TokenKind::Delimiter, ",");
  }
  Token end = expect(TokenKind::Delimiter, "}");
  return makeExpr(Match{std::move(subject), std::move(arms),
                        Span::merge(start.span, end.span)});
}

MatchArm Parser::parse_match_arm() {
  Span arm_span = current().span;
  if (check(TokenKind::Delimiter, "_")) {
    advance();
    expect(TokenKind::Operator, "=>");
    return MatchArm{"_", {}, parse_expr(), arm_span};
  }
  if (current().is(LiteralKind::Integer)) {
    Token t = advance();
    expect(TokenKind::Operator, "=>");
    return MatchArm{std::to_string(std::get<long long>(t.value)),
                    {},
                    parse_expr(),
                    arm_span};
  }
  Token name = expect(TokenKind::Identifier);
  std::vector<std::string> bindings;
  if (match(TokenKind::Delimiter, "(")) {
    while (!check(TokenKind::Delimiter, ")") && !is_at_end()) {
      bindings.push_back(check(TokenKind::Delimiter, "_")
                             ? (advance(), std::string("_"))
                             : expect(TokenKind::Identifier).text);
      if (!match(TokenKind::Delimiter, ","))
        break;
    }
    expect(TokenKind::Delimiter, ")");
  }
  expect(TokenKind::Operator, "=>");
  return MatchArm{name.text, std::move(bindings), parse_expr(), name.span};
}

ExprPtr Parser::parse_lambda() {
  Token start = advance();
  std::vector<Param> params;
  while (!check(TokenKind::Operator, "|") && !is_at_end()) {
    params.push_back(parse_param());
    if (!match(TokenKind::Delimiter, ","))
      break;
  }
  expect(TokenKind::Operator, "|");
  Block body = take_block(parse_block());
  return makeExpr(Lambda{std::move(params), std::move(body),
                         Span::merge(start.span, body.span)});
}

ExprPtr Parser::parse_new() {
  Token start = advance(), name = expect(TokenKind::Identifier);
  expect(TokenKind::Delimiter, "(");
  std::vector<ExprPtr> args;
  while (!check(TokenKind::Delimiter, ")") && !is_at_end()) {
    args.push_back(parse_expr());
    if (!match(TokenKind::Delimiter, ","))
      break;
  }
  Token end = expect(TokenKind::Delimiter, ")");
  return makeExpr(
      New{name.text, std::move(args), Span::merge(start.span, end.span)});
}

Param Parser::parse_param() {
  Token t = expect(TokenKind::Identifier);
  return Param{t.text, t.span};
}

std::vector<Param> Parser::parse_param_list() {
  expect(TokenKind::Delimiter, "(");
  std::vector<Param> params;
  while (!check(TokenKind::Delimiter, ")") && !is_at_end()) {
    params.push_back(parse_param());
    if (!match(TokenKind::Delimiter, ","))
      break;
  }
  expect(TokenKind::Delimiter, ")");
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
    if (check(TokenKind::Keyword, "public") ||
        check(TokenKind::Keyword, "let") || check(TokenKind::Keyword, "type") ||
        check(TokenKind::Keyword, "struct") ||
        check(TokenKind::Keyword, "import") ||
        check(TokenKind::Keyword, "package") ||
        check(TokenKind::Delimiter, "}"))
      return;
    advance();
  }
}