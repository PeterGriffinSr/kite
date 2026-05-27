#pragma once

#include <string>

struct Span {
  size_t start;
  size_t end;

  size_t length() const { return end - start; }

  bool operator==(const Span &other) const {
    return start == other.start && end == other.end;
  }

  static Span merge(const Span &a, const Span &b) { return {a.start, b.end}; }
};

struct Location {
  std::string file;
  size_t line;
  size_t column;
  Span span;
};