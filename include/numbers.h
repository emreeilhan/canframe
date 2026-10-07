#ifndef NUMBERS_H
#define NUMBERS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool number_unsigned(const char *s, size_t n, unsigned base, uint64_t max, uint64_t *value);
bool number_double(const char *s, double *value);
bool number_timestamp(const char *s, size_t n, uint64_t *ns);
bool number_name(const char *s, size_t max);
#endif
