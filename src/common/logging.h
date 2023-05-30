#pragma once

#include <iostream>

template <class... T>
inline void logf(const char *fmt, T... args) {
  printf("[C++] ");
  printf(fmt, args...);
  printf("\n");
}

template <>
inline void logf(const char *str) {
  printf("[C++] %s\n", str);
}
