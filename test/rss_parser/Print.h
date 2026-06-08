// Minimal Print.h stub for host unit tests.
// Replaces the Arduino Print class so that RssParser can compile without
// the Arduino framework.
#pragma once

#include <cstddef>
#include <cstdint>

class Print {
 public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t* buf, size_t size) = 0;
  virtual void flush() {}
};
