#pragma once

#include <iostream>

template <class... T>
inline void logf(const char *fmt, T... args) {
  printf(fmt, args...);
  printf("\n");
}

template <>
inline void logf(const char *str) {
  printf("%s\n", str);
}
