#define _GNU_SOURCE
#include "numbers.h"
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#include <xlocale.h>
#endif
bool number_unsigned(const char *s, size_t n, unsigned base, uint64_t max, uint64_t *value) {
  uint64_t v = 0;
  size_t i = 0;
  if (!s || !value || (base != 10 && base != 16)) return false;
  if (base == 16 && n >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) i = 2;
  if (i == n) return false;
  for (; i < n; i++) {
    unsigned d;
    if (s[i] >= '0' && s[i] <= '9') d = (unsigned)(s[i] - '0');
    else if (s[i] >= 'a' && s[i] <= 'f') d = (unsigned)(s[i] - 'a' + 10);
    else if (s[i] >= 'A' && s[i] <= 'F') d = (unsigned)(s[i] - 'A' + 10);
    else return false;
    if (d >= base || d > max || v > (max - d) / base) return false;
    v = v * base + d;
  }
  *value = v;
  return true;
}
bool number_double(const char *s, double *value) {
  const char *p = s;
  size_t digits = 0;
  locale_t locale;
  char *end;
  double v;
  if (!s || !value) return false;
  if (*p == '+' || *p == '-') p++;
  while (*p >= '0' && *p <= '9') { p++; digits++; }
  if (*p == '.') { p++; while (*p >= '0' && *p <= '9') { p++; digits++; } }
  if (!digits) return false;
  if (*p == 'e' || *p == 'E') {
    p++; if (*p == '+' || *p == '-') p++;
    digits = 0;
    while (*p >= '0' && *p <= '9') { p++; digits++; }
    if (!digits) return false;
  }
  if (*p) return false;
  locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
  if (!locale) return false;
  errno = 0;
  v = strtod_l(s, &end, locale);
  freelocale(locale);
  /* ERANGE includes underflow: do not silently turn a coefficient into zero. */
  if (*end || errno == ERANGE || !isfinite(v)) return false;
  *value = v;
  return true;
}
bool number_timestamp(const char *s, size_t n, uint64_t *ns) {
  size_t point = 0, i;
  if (!s || !ns || !n) return false;
  uint64_t seconds, fraction = 0;
  while (point < n && s[point] != '.') point++;
  if (!number_unsigned(s, point, 10, UINT64_MAX / UINT64_C(1000000000), &seconds)) return false;
  if (point < n) {
    size_t digits = n - point - 1;
    if (!digits || digits > 9) return false;
    for (i = point + 1; i < n; i++) {
      if (s[i] < '0' || s[i] > '9') return false;
      fraction = fraction * 10 + (unsigned)(s[i] - '0');
    }
    for (i = digits; i < 9; i++) fraction *= 10;
  }
  if (seconds > (UINT64_MAX - fraction) / UINT64_C(1000000000)) return false;
  *ns = seconds * UINT64_C(1000000000) + fraction;
  return true;
}
bool number_name(const char *s, size_t max) {
  size_t n;
  if (!s || !(n = strlen(s)) || n > max) return false;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    if (!(c >= 'A' && c <= 'Z') && !(c >= 'a' && c <= 'z') &&
        !(c >= '0' && c <= '9') && c != '_') return false;
  }
  return true;
}
