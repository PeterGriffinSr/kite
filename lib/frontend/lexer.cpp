#include <cctype>
#include <frontend/lexer.hpp>
#include <functional>
#include <unordered_map>
#include <unordered_set>

static const std::unordered_set<std::string> KEYWORDS = {
    "let",    "type",    "struct", "match",   "if",  "else",
    "import", "package", "public", "private", "new", "this",
};

static const std::unordered_map<char, std::string> DELIMITERS = {
    {')', ")"}, {'{', "{"}, {'}', "}"}, {'[', "["}, {']', "]"},
    {',', ","}, {'.', "."}, {':', ":"}, {';', ";"},
};

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;
  while (!is_at_end())
    step(tokens, advance());
  step(tokens, std::nullopt);
  emit(tokens, current_span());
  return tokens;
}

void Lexer::step(std::vector<Token> &tokens, std::optional<char> c) {
  switch (state_) {

  case LexState::Start: {
    if (!c)
      return;

    auto delim = [&](std::string s) {
      emit_and_reset(tokens, std::pair{TokenKind::Delimiter, std::move(s)});
    };
    auto op = [&](std::string s) {
      emit_and_reset(tokens, std::pair{TokenKind::Operator, std::move(s)});
    };

    const std::unordered_map<char, std::function<std::string()>> OPERATORS = {
        {'+', [&] { return "+"; }},
        {'*', [&] { return "*"; }},
        {'%', [&] { return "%"; }},
        {'&', [&] { return "&"; }},
        {'-', [&] { return match('>') ? "->" : "-"; }},
        {'=', [&] { return match('=')   ? "=="
                           : match('>') ? "=>"
                                        : "="; }},
        {'!', [&] { return match('=') ? "!=" : "!"; }},
        {'<', [&] { return match('=') ? "<=" : "<"; }},
        {'>', [&] { return match('=') ? ">=" : ">"; }},
        {'|', [&] { return match('|') ? "||" : "|"; }},
    };

    if (auto it = DELIMITERS.find(*c); it != DELIMITERS.end())
      delim(it->second);
    else if (auto it = OPERATORS.find(*c); it != OPERATORS.end())
      op(it->second());
    else
      switch (*c) {
      case '(':
        !is_at_end() && peek() == ')' ? (advance(), delim("()")) : delim("(");
        break;
      case '_':
        (std::isalnum(peek()) || peek() == '_')
            ? (void)(state_ = LexState::InIdentifier)
            : delim("_");
        break;
      case '/':
        match('/') ? (void)(state_ = LexState::InLineComment) : op("/");
        break;
      case ' ':
      case '\r':
      case '\t':
        start_ = pos_;
        break;
      case '\n':
        line_++;
        line_start_ = pos_;
        start_ = pos_;
        break;
      case '"':
        state_ = LexState::InString;
        break;
      case '\'':
        (is_at_end() || peek() == '\n') ? error("unterminated char literal")
                                        : (void)(state_ = LexState::InChar);
        break;
      default:
        if (std::isdigit(*c))
          state_ = LexState::InNumber;
        else if (std::isalpha(*c))
          state_ = LexState::InIdentifier;
        else
          error("unexpected character '" + std::string(1, *c) + "'");
        break;
      }
    break;
  }
  case LexState::InIdentifier: {
    if (c && (std::isalnum(*c) || *c == '_'))
      return;
    if (c)
      putback();
    std::string text = file_.source.substr(start_, pos_ - start_);
    TokenKind kind =
        KEYWORDS.count(text) ? TokenKind::Keyword : TokenKind::Identifier;
    emit_and_reset(tokens, std::pair{kind, std::move(text)});
    break;
  }
  case LexState::InNumber: {
    if (c && std::isdigit(*c))
      return;
    if (c && *c == '.' && !is_at_end() && std::isdigit(peek())) {
      state_ = LexState::InFloat;
      return;
    }
    if (c)
      putback();
    emit_and_reset(tokens,
                   std::stoll(file_.source.substr(start_, pos_ - start_)));
    break;
  }
  case LexState::InFloat: {
    if (c && std::isdigit(*c))
      return;
    if (c)
      putback();
    emit_and_reset(tokens,
                   std::stod(file_.source.substr(start_, pos_ - start_)));
    break;
  }
  case LexState::InString: {
    if (!c) {
      error("unterminated string literal");
      return;
    }
    if (*c == '\n') {
      line_++;
      line_start_ = pos_;
      return;
    }
    if (*c == '\\') {
      state_ = LexState::InStringEscape;
      return;
    }
    if (*c == '"')
      emit_and_reset(
          tokens, std::pair{file_.source.substr(start_ + 1, pos_ - start_ - 2),
                            LiteralKind::String});
    break;
  }
  case LexState::InStringEscape: {
    if (!c) {
      error("unterminated string literal");
      return;
    }
    state_ = LexState::InString;
    break;
  }
  case LexState::InChar: {
    if (!c) {
      error("unterminated char literal");
      return;
    }
    if (*c == '\\') {
      state_ = LexState::InCharEscape;
      return;
    }
    char_value_ = *c;
    state_ = LexState::InCharClose;
    break;
  }
  case LexState::InCharEscape: {
    if (!c) {
      error("unterminated char literal");
      return;
    }
    switch (*c) {
    case 'n':
      char_value_ = '\n';
      break;
    case 't':
      char_value_ = '\t';
      break;
    case 'r':
      char_value_ = '\r';
      break;
    case '\'':
      char_value_ = '\'';
      break;
    case '\\':
      char_value_ = '\\';
      break;
    default:
      error("unknown escape sequence");
      return;
    }
    state_ = LexState::InCharClose;
    break;
  }
  case LexState::InCharClose: {
    if (!c || *c != '\'') {
      error("unterminated char literal — expected closing '");
      return;
    }
    emit_and_reset(tokens, char_value_);
    break;
  }
  case LexState::InLineComment: {
    if (!c || *c == '\n') {
      if (c) {
        line_++;
        line_start_ = pos_;
      }
      state_ = LexState::Start;
      start_ = pos_;
    }
    break;
  }
  }
}

void Lexer::emit_and_reset(std::vector<Token> &tokens, TokenValue value) {
  std::visit(
      [&](auto &&v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::pair<TokenKind, std::string>>)
          emit(tokens, v.first, std::move(v.second), current_span());
        else if constexpr (std::is_same_v<T,
                                          std::pair<std::string, LiteralKind>>)
          emit(tokens, std::move(v.first), current_span(), v.second);
        else
          emit(tokens, std::forward<decltype(v)>(v), current_span());
      },
      value);
  state_ = LexState::Start;
  start_ = pos_;
}

void Lexer::error(const std::string &msg) {
  reporter_.error(msg,
                  SourceLocation{file_.filename, file_.line_at(start_),
                                 file_.column_at(start_)},
                  file_.line_text_at(start_));
  state_ = LexState::Start;
  start_ = pos_;
}