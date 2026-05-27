#include <frontend/lexer.hpp>
#include <unordered_map>
#include <unordered_set>

static const std::unordered_set<std::string> KEYWORDS = {
    "let",    "type",    "struct", "match", "if",   "else",
    "import", "package", "public", "new",   "this",
};

static const std::unordered_map<char, std::string> DELIMITERS = {
    {')', ")"}, {'{', "{"}, {'}', "}"}, {'[', "["}, {']', "]"},
    {',', ","}, {'.', "."}, {':', ":"}, {';', ";"},
};

static const std::unordered_map<char, std::string> SIMPLE_OPERATORS = {
    {'+', "+"},
    {'*', "*"},
    {'%', "%"},
    {'&', "&"},
};

static const std::unordered_set<char> COMPOUND_OPERATORS = {
    '-', '=', '!', '<', '>', '|',
};

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;
  while (!is_at_end())
    step(tokens, advance());
  step(tokens, std::nullopt);
  tokens.push_back(Token::make(std::monostate{}, current_span()));
  return tokens;
}

void Lexer::step(std::vector<Token> &tokens, std::optional<char> c) {
  switch (state_) {

  case State::Start: {
    if (!c)
      return;

    auto delim = [&](std::string s) {
      emit_and_reset(tokens, std::pair{TokenKind::Delimiter, std::move(s)});
    };
    auto op = [&](std::string s) {
      emit_and_reset(tokens, std::pair{TokenKind::Operator, std::move(s)});
    };

    if (auto it = DELIMITERS.find(*c); it != DELIMITERS.end())
      delim(it->second);
    else if (auto it = SIMPLE_OPERATORS.find(*c); it != SIMPLE_OPERATORS.end())
      op(it->second);
    else if (COMPOUND_OPERATORS.count(*c)) {
      switch (*c) {
      case '-':
        op(match('>') ? "->" : "-");
        break;
      case '=':
        op(match('=') ? "==" : match('>') ? "=>" : "=");
        break;
      case '!':
        op(match('=') ? "!=" : "!");
        break;
      case '<':
        op(match('=') ? "<=" : "<");
        break;
      case '>':
        op(match('=') ? ">=" : ">");
        break;
      case '|':
        op(match('|') ? "||" : "|");
        break;
      }
    } else
      switch (*c) {
      case '(':
        !is_at_end() && peek() == ')' ? (advance(), delim("()")) : delim("(");
        break;
      case '_':
        (std::isalnum(peek()) || peek() == '_')
            ? (void)(state_ = State::InIdentifier)
            : delim("_");
        break;
      case '/':
        match('/') ? (void)(state_ = State::InLineComment) : op("/");
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
        state_ = State::InString;
        break;
      case '\'':
        (is_at_end() || peek() == '\n') ? error("unterminated char literal")
                                        : (void)(state_ = State::InChar);
        break;
      default:
        if (std::isdigit(*c))
          state_ = State::InNumber;
        else if (std::isalpha(*c))
          state_ = State::InIdentifier;
        else
          error("unexpected character '" + std::string(1, *c) + "'");
        break;
      }
    break;
  }
  case State::InIdentifier: {
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
  case State::InNumber: {
    if (c && std::isdigit(*c))
      return;
    if (c && *c == '.' && !is_at_end() && std::isdigit(peek())) {
      state_ = State::InFloat;
      return;
    }
    if (c)
      putback();
    emit_and_reset(tokens,
                   std::stoll(file_.source.substr(start_, pos_ - start_)));
    break;
  }
  case State::InFloat: {
    if (c && std::isdigit(*c))
      return;
    if (c)
      putback();
    emit_and_reset(tokens,
                   std::stod(file_.source.substr(start_, pos_ - start_)));
    break;
  }
  case State::InString: {
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
      state_ = State::InStringEscape;
      return;
    }
    if (*c == '"')
      emit_and_reset(
          tokens, std::pair{file_.source.substr(start_ + 1, pos_ - start_ - 2),
                            LiteralKind::String});
    break;
  }
  case State::InStringEscape: {
    if (!c) {
      error("unterminated string literal");
      return;
    }
    state_ = State::InString;
    break;
  }
  case State::InChar: {
    if (!c) {
      error("unterminated char literal");
      return;
    }
    if (*c == '\\') {
      state_ = State::InCharEscape;
      return;
    }
    char_value_ = *c;
    state_ = State::InCharClose;
    break;
  }
  case State::InCharEscape: {
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
    state_ = State::InCharClose;
    break;
  }
  case State::InCharClose: {
    if (!c || *c != '\'') {
      error("unterminated char literal — expected closing '");
      return;
    }
    emit_and_reset(tokens, char_value_);
    break;
  }
  case State::InLineComment: {
    if (!c || *c == '\n') {
      if (c) {
        line_++;
        line_start_ = pos_;
      }
      state_ = State::Start;
      start_ = pos_;
    }
    break;
  }
  }
}

void Lexer::emit_and_reset(std::vector<Token> &tokens, TokenValue value) {
  tokens.push_back(Token::make(std::move(value), current_span()));
  state_ = State::Start;
  start_ = pos_;
}

void Lexer::error(const std::string &msg) {
  reporter_.error(msg,
                  SourceLocation{file_.filename, file_.line_at(start_),
                                 file_.column_at(start_)},
                  file_.line_text_at(start_));
  state_ = State::Start;
  start_ = pos_;
}