#pragma once

#include <optional>
#include <string>
#include <vector>

enum class Severity { Error, Warning, Note, Hint };

struct SourceLocation {
  std::string file;
  size_t line;
  size_t column;
};

struct Diagnostic {
  Severity severity;
  std::string message;
  std::optional<SourceLocation> location;
  std::optional<std::string> source_line;
};

class ErrorReporter {
public:
  void report(Severity severity, const std::string &message,
              std::optional<SourceLocation> location = std::nullopt,
              std::optional<std::string> source_line = std::nullopt);

  void error(const std::string &message) { report(Severity::Error, message); }

  void error(const std::string &message, SourceLocation loc,
             const std::string &source_line) {
    report(Severity::Error, message, loc, source_line);
  }

  void warning(const std::string &message) {
    report(Severity::Warning, message);
  }

  void warning(const std::string &message, SourceLocation loc,
               const std::string &source_line) {
    report(Severity::Warning, message, loc, source_line);
  }

  void note(const std::string &message) { report(Severity::Note, message); }

  void hint(const std::string &message) { report(Severity::Hint, message); }

  bool has_errors() const { return error_count_ > 0; }
  size_t error_count() const { return error_count_; }
  size_t warning_count() const { return warning_count_; }

  void print_summary() const;

private:
  std::vector<Diagnostic> diagnostics_;
  size_t error_count_ = 0;
  size_t warning_count_ = 0;

  static std::string severity_label(Severity s);
};