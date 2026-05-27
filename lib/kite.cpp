#include <frontend/lexer.hpp>
#include <frontend/parser.hpp>
#include <fstream>
#include <kite.hpp>
#include <sstream>

static std::string read_file(const std::string &path, ErrorReporter &reporter) {
  std::ifstream file(path);
  if (!file) {
    reporter.error("could not open file '" + path + "'");
    return "";
  }
  std::ostringstream buf;
  buf << file.rdbuf();
  return buf.str();
}

void compile(const Options &opts, ErrorReporter &reporter) {
  SourceFile file;
  file.filename = opts.input;
  file.source = read_file(opts.input, reporter);
  if (reporter.has_errors())
    return;

  Lexer lexer(file, reporter);
  auto tokens = lexer.tokenize();
  if (reporter.has_errors())
    return;

  Parser parser(std::move(tokens), file, reporter);
  auto ast = parser.parse();
  if (reporter.has_errors())
    return;
}