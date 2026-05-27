#include <cli/cli.hpp>
#include <iostream>
#include <kite.hpp>
#include <version.hpp>

void print_help() {
  std::cout << "Usage: kite <file> [options]\n"
               "\n"
               "Options:\n"
               "  -o, --output <file>  Output file (default: a.out)\n"
               "  --verbose            Enable verbose output\n"
               "  -v, --version        Print version\n"
               "  -h, --help           Print this help message\n";
}

void print_version() { std::cout << "kite " << KITE_VERSION << "\n"; }

ArgParser::ArgParser(ErrorReporter &reporter) : reporter_(reporter) {
  register_flag("-h", [](auto &, auto &, auto &, auto &) {
    print_help();
    std::exit(0);
  });
  register_flag("--help", [](auto &, auto &, auto &, auto &) {
    print_help();
    std::exit(0);
  });
  register_flag("-v", [](auto &, auto &, auto &, auto &) {
    print_version();
    std::exit(0);
  });
  register_flag("--version", [](auto &, auto &, auto &, auto &) {
    print_version();
    std::exit(0);
  });

  register_flag("--verbose", [](auto &, auto &opts, auto &, auto &) {
    opts.verbose = true;
  });

  auto output_handler = [](auto &self, auto &opts, auto &args, auto &i) {
    if (i + 1 >= args.size()) {
      self.reporter_.error("-o requires an argument");
      std::exit(1);
    }
    opts.output = args[++i];
  };
  register_flag("-o", output_handler);
  register_flag("--output", output_handler);
}

void ArgParser::register_flag(const std::string &flag, Handler handler) {
  handlers_[flag] = handler;
}

Options ArgParser::parse(int argc, char **argv) {
  Options opts;
  std::vector<std::string> args(argv + 1, argv + argc);

  for (size_t i = 0; i < args.size(); i++) {
    const std::string &arg = args[i];

    if (arg[0] == '-') {
      auto it = handlers_.find(arg);
      if (it == handlers_.end()) {
        reporter_.error("unknown flag '" + arg + "'");
        std::exit(1);
      }
      it->second(*this, opts, args, i);
    } else {
      if (!opts.input.empty()) {
        reporter_.error("unexpected argument '" + arg + "'");
        std::exit(1);
      }
      opts.input = arg;
    }
  }

  if (opts.input.empty()) {
    reporter_.error("no input file");
    print_help();
    std::exit(1);
  }

  return opts;
}

Options runCli(int argc, char **argv) {
  ErrorReporter reporter;
  ArgParser parser(reporter);
  Options opts = parser.parse(argc, argv);
  
  ErrorReporter compile_reporter;
  compile(opts, compile_reporter);
  compile_reporter.print_summary();

  reporter.print_summary();
  return opts;
}