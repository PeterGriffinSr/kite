#pragma once

#include <common/error.hpp>
#include <functional>

struct Options {
  std::string input;
  std::string output = "a.out";
  bool verbose = false;
};

class ArgParser {
public:
  ArgParser(ErrorReporter &reporter);
  Options parse(int argc, char **argv);

private:
  using Handler = std::function<void(ArgParser &, Options &,
                                     std::vector<std::string> &, size_t &)>;

  void register_flag(const std::string &flag, Handler handler);

  ErrorReporter &reporter_;
  std::unordered_map<std::string, Handler> handlers_;
};

Options runCli(int argc, char **argv);