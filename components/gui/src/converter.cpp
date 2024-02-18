#include "converter.hpp"

Converter::Status Converter::str2int(int &i, char const *s, int base) {
  char *end;
  long l;
  errno = 0;
  l = strtol(s, &end, base);
  if ((errno == ERANGE && l == LONG_MAX) || l > INT_MAX) {
    return Status::Overflow;
  }
  if ((errno == ERANGE && l == LONG_MIN) || l < INT_MIN) {
    return Status::Underflow;
  }
  if (*s == '\0' || *end != '\0') {
    return Status::Inconvertible;
  }
  i = l;
  return Status::Success;
}
