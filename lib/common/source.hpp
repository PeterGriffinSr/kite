#pragma once

#include <string>

struct SourceFile {
  std::string filename;
  std::string source;

  size_t line_at(size_t offset) const {
    size_t line = 1;
    for (size_t i = 0; i < offset && i < source.size(); i++) {
      if (source[i] == '\n')
        line++;
    }
    return line;
  }

  size_t column_at(size_t offset) const {
    size_t col = offset;
    while (col > 0 && source[col - 1] != '\n')
      col--;
    return offset - col + 1;
  }

  std::string line_text_at(size_t offset) const {
    size_t start = offset;
    while (start > 0 && source[start - 1] != '\n')
      start--;
    size_t end = source.find('\n', offset);
    if (end == std::string::npos)
      end = source.size();
    return source.substr(start, end - start);
  }
};