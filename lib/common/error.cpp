#include <common/error.hpp>
#include <iostream>

std::string ErrorReporter::severity_label(Severity s) {
  switch (s) {
  case Severity::Error:
    return "error";
  case Severity::Warning:
    return "warning";
  case Severity::Note:
    return "note";
  case Severity::Hint:
    return "hint";
  }
  return "unknown";
}

void ErrorReporter::report(Severity severity, const std::string &message,
                           std::optional<SourceLocation> location,
                           std::optional<std::string> source_line) {
  diagnostics_.push_back({severity, message, location, source_line});

  if (severity == Severity::Error)
    error_count_++;
  if (severity == Severity::Warning)
    warning_count_++;

  const std::string &label = severity_label(severity);

  if (location) {
    std::cerr << location->file << ":" << location->line << ":"
              << location->column << ": " << label << ": " << message << "\n";

    if (source_line) {
      std::cerr << std::string(2, ' ') << *source_line << "\n";

      std::cerr << std::string(2, ' ') << std::string(location->column - 1, ' ')
                << "^"
                << "\n";
    }
  } else {
    std::cerr << label << ": " << message << "\n";
  }
}

void ErrorReporter::print_summary() const {
  if (error_count_ == 0 && warning_count_ == 0)
    return;

  std::cerr << "\n";

  if (error_count_ > 0) {
    std::cerr << "aborting due to " << error_count_
              << (error_count_ == 1 ? " error" : " errors") << "\n";
  }

  if (warning_count_ > 0) {
    std::cerr << warning_count_
              << (warning_count_ == 1 ? " warning" : " warnings") << " emitted"
              << "\n";
  }
}