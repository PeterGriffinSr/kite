#pragma once
#include <cstddef>

template <typename T> struct Cursor {
protected:
  const T *data_;
  size_t size_;
  size_t pos_ = 0;

  explicit Cursor(const T *data, size_t size) : data_(data), size_(size) {}

  const T &current() const {
    return pos_ < size_ ? data_[pos_] : data_[size_ - 1];
  }
  const T &peek(size_t n = 0) const {
    size_t i = pos_ + n;
    return i < size_ ? data_[i] : data_[size_ - 1];
  }
  bool is_at_end() const { return pos_ >= size_; }
  void putback() {
    if (pos_ > 0)
      --pos_;
  }
  T advance() { return pos_ < size_ ? data_[pos_++] : data_[size_ - 1]; }
  bool match(const T &expected) {
    if (is_at_end() || data_[pos_] != expected)
      return false;
    pos_++;
    return true;
  }
};