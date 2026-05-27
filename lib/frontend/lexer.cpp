#include <cctype>
#include <frontend/lexer.hpp>
#include <unordered_set>

static const std::unordered_set<std::string> KEYWORDS = {
    "let",    "type",    "struct", "match",   "if",  "else",
    "import", "package", "public", "private", "new", "this",
};

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;
  while (!is_at_end()) {
    start_ = pos_;
    scan(tokens);
  }
  emit(tokens, current_span());
  return tokens;
}

void Lexer::scan(std::vector<Token> &tokens) {
  char c = advance();
  auto delim = [&](std::string s) {
    emit(tokens, TokenKind::Delimiter, std::move(s), current_span());
  };
  auto op = [&](std::string s) {
    emit(tokens, TokenKind::Operator, std::move(s), current_span());
  };

  switch (c) {
  case ')':
    delim(")");
    break;
  case '(':
    delim(peek() == ')' ? (advance(), std::string("()")) : "(");
    break;
  case '{':
    delim("{");
    break;
  case '}':
    delim("}");
    break;
  case '[':
    delim("[");
    break;
  case ']':
    delim("]");
    break;
  case ',':
    delim(",");
    break;
  case '.':
    delim(".");
    break;
  case ':':
    delim(":");
    break;
  case ';':
    delim(";");
    break;
  case '_':
    (std::isalnum(peek()) || peek() == '_') ? scan_identifier_or_keyword(tokens)
                                            : delim("_");
    break;
  case '+':
    op("+");
    break;
  case '*':
    op("*");
    break;
  case '%':
    op("%");
    break;
  case '&':
    op("&");
    break;
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
  case '/':
    match('/') ? skip_line_comment() : op("/");
    break;
  case ' ':
  case '\r':
  case '\t':
    break;
  case '\n':
    line_++;
    line_start_ = pos_;
    break;

  case '\'': {
    if (is_at_end() || peek() == '\n') {
      error("unterminated char literal");
      return;
    }
    char value;
    if (peek() == '\\') {
      advance();
      switch (advance()) {
      case 'n':
        value = '\n';
        break;
      case 't':
        value = '\t';
        break;
      case 'r':
        value = '\r';
        break;
      case '\'':
        value = '\'';
        break;
      case '\\':
        value = '\\';
        break;
      default:
        error("unknown escape sequence");
        return;
      }
    } else {
      value = advance();
    }
    if (!match('\'')) {
      error("unterminated char literal — expected closing '");
      return;
    }
    emit(tokens, value, current_span());
    break;
  }

  case '"': {
    while (!is_at_end() && peek() != '"') {
      if (peek() == '\n') {
        line_++;
        line_start_ = pos_ + 1;
      }
      if (peek() == '\\')
        advance();
      advance();
    }
    if (is_at_end()) {
      error("unterminated string literal");
      return;
    }
    advance();
    emit(tokens, file_.source.substr(start_ + 1, pos_ - start_ - 2),
         current_span(), LiteralKind::String);
    break;
  }

  default:
    if (std::isdigit(c)) {
      while (std::isdigit(peek()))
        advance();
      if (peek() == '.' && std::isdigit(peek_next())) {
        advance();
        while (std::isdigit(peek()))
          advance();
        emit(tokens, std::stod(file_.source.substr(start_, pos_ - start_)),
             current_span());
      } else {
        emit(tokens, std::stoll(file_.source.substr(start_, pos_ - start_)),
             current_span());
      }
    } else if (std::isalpha(c) || c == '_') {
      scan_identifier_or_keyword(tokens);
    } else {
      error("unexpected character '" + std::string(1, c) + "'");
    }
  }
}

void Lexer::scan_identifier_or_keyword(std::vector<Token> &tokens) {
  while (std::isalnum(peek()) || peek() == '_')
    advance();
  std::string text = file_.source.substr(start_, pos_ - start_);
  if (KEYWORDS.count(text))
    emit(tokens, TokenKind::Keyword, text, current_span());
  else
    emit(tokens, std::move(text), current_span());
}

void Lexer::skip_line_comment() {
  while (!is_at_end() && peek() != '\n')
    advance();
}

void Lexer::error(const std::string &msg) {
  reporter_.error(msg,
                  SourceLocation{file_.filename, file_.line_at(start_),
                                 file_.column_at(start_)},
                  file_.line_text_at(start_));
}