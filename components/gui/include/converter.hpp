#pragma once

#include <cerrno>
#include <climits>
#include <stdlib.h>

class Converter {
public:
  enum class Status { Success, Overflow, Underflow, Inconvertible };
  static Status str2int(int &i, char const *s, int base = 0);
};
