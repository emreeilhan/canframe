#include "deffile.h"
#include "decode.h"
#include "numbers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static canframe_status_t fail(cfd_error_t *e, canframe_status_t s, size_t line, const char *why) {
  if (e) { e->status = s; e->line = line; snprintf(e->reason, sizeof(e->reason), "%s", why); }
  return s;
}
static bool whitespace(char c) { return c == ' ' || c == '\t' || c == '\r'; }
static bool decimal(const char *s, uint64_t max, uint64_t *v) { return number_unsigned(s, strlen(s), 10, max, v); }
canframe_status_t deffile_parse_bytes(const uint8_t *data, size_t size, can_message_def_t *definitions,
                                     size_t capacity, size_t *count, cfd_error_t *error) {
  can_message_def_t *candidate, *current = NULL;
  size_t pos = 0, line_no = 0, loaded = 0;
  bool header = false;
  canframe_status_t status = CANFRAME_OK;
  if (count) *count = 0;
  if (error) memset(error, 0, sizeof(*error));
  if (!data || !definitions || !count) return fail(error, CANFRAME_ERR_INVALID_FORMAT, 0, "null argument");
  if (size > CFD_MAX_BYTES) return fail(error, CANFRAME_ERR_BOUNDS, 0, "file exceeds 64 KiB");
  if (memchr(data, 0, size)) return fail(error, CANFRAME_ERR_INVALID_FORMAT, 0, "embedded NUL");
  candidate = calloc(CANFRAME_MAX_DEFINITIONS, sizeof(*candidate));
  if (!candidate) return fail(error, CANFRAME_ERR_IO, 0, "allocation failed");
#define BAD(s, why) do { status = fail(error, s, line_no, why); goto done; } while (0)
  while (pos < size) {
    char line[CFD_MAX_LINE + 1], *words[10], *p;
    size_t begin = pos, len, n = 0;
    uint64_t a, b;
    while (pos < size && data[pos] != '\n') pos++;
    len = pos - begin; if (pos < size) pos++;
    line_no++;
    if (len > CFD_MAX_LINE) BAD(CANFRAME_ERR_BOUNDS, "line exceeds 1024 bytes");
    memcpy(line, data + begin, len); line[len] = 0;
    p = line; while (whitespace(*p)) p++;
    if (!*p || *p == '#') continue;
    while (*p) {
      if (n == 10) BAD(CANFRAME_ERR_INVALID_FORMAT, "too many fields");
      words[n++] = p;
      while (*p && !whitespace(*p)) p++;
      if (*p) *p++ = 0;
      while (whitespace(*p)) p++;
    }
    if (!header) {
      if (n != 2 || strcmp(words[0], "CFD") || strcmp(words[1], "1")) BAD(CANFRAME_ERR_INVALID_FORMAT, "expected CFD 1 header");
      header = true; continue;
    }
    if (!strcmp(words[0], "MESSAGE")) {
      if (current || n != 5) BAD(CANFRAME_ERR_INVALID_FORMAT, "invalid MESSAGE or missing END");
      if (loaded >= CANFRAME_MAX_DEFINITIONS || loaded >= capacity) BAD(CANFRAME_ERR_CAPACITY, "too many messages");
      if (strlen(words[1]) > 10 || strncmp(words[1], "0x", 2) || !number_unsigned(words[1], strlen(words[1]), 16, 0x1fffffff, &a)) BAD(CANFRAME_ERR_INVALID_ID, "invalid message ID");
      if (strcmp(words[2], "STANDARD") && strcmp(words[2], "EXTENDED")) BAD(CANFRAME_ERR_INVALID_FORMAT, "expected STANDARD or EXTENDED");
      if (!decimal(words[3], 8, &b)) BAD(CANFRAME_ERR_INVALID_DLC, "expected DLC 0..8");
      if (!number_name(words[4], 31)) BAD(CANFRAME_ERR_INVALID_FORMAT, "invalid message name");
      current = &candidate[loaded]; current->id = (uint32_t)a;
      current->is_extended = !strcmp(words[2], "EXTENDED"); current->expected_dlc = (uint8_t)b;
      if (!current->is_extended && a > 0x7ff) BAD(CANFRAME_ERR_INVALID_ID, "standard ID exceeds 0x7FF");
      for (size_t i = 0; i < loaded; i++) if (candidate[i].id == current->id && candidate[i].is_extended == current->is_extended) BAD(CANFRAME_ERR_INVALID_FORMAT, "duplicate message key");
      strcpy(current->name, words[4]); continue;
    }
    if (!current) BAD(CANFRAME_ERR_INVALID_FORMAT, "directive outside MESSAGE");
    if (!strcmp(words[0], "END")) {
      if (n != 1) BAD(CANFRAME_ERR_INVALID_FORMAT, "END takes no fields");
      status = validate_definition(current);
      if (status != CANFRAME_OK) BAD(status, "invalid message bounds, overlaps or timeout");
      loaded++; current = NULL; continue;
    }
    if (!strcmp(words[0], "PERIOD")) {
      if (n != 3 || current->period_ms || !decimal(words[1], UINT32_MAX, &a) || !a || !decimal(words[2], UINT32_MAX, &b)) BAD(CANFRAME_ERR_INVALID_FORMAT, "invalid or duplicate PERIOD");
      if (b >= a) BAD(CANFRAME_ERR_BOUNDS, "tolerance must be smaller than period");
      current->period_ms = (uint32_t)a; current->tolerance_ms = (uint32_t)b; continue;
    }
    if (!strcmp(words[0], "TIMEOUT")) {
      if (n != 2 || current->timeout_ms || !decimal(words[1], UINT32_MAX, &a) || !a) BAD(CANFRAME_ERR_INVALID_FORMAT, "invalid or duplicate TIMEOUT");
      current->timeout_ms = (uint32_t)a; continue;
    }
    if (!strcmp(words[0], "SIGNAL")) {
      can_signal_t *s;
      if (n != 9) BAD(CANFRAME_ERR_INVALID_FORMAT, "SIGNAL requires eight fields");
      if (current->signal_count >= CANFRAME_MAX_SIGNALS) BAD(CANFRAME_ERR_CAPACITY, "too many signals");
      if (!number_name(words[1], 31) || !number_name(words[8], 15)) BAD(CANFRAME_ERR_INVALID_FORMAT, "invalid signal name or unit");
      if (!decimal(words[2], 63, &a) || !decimal(words[3], 64, &b) || !b) BAD(CANFRAME_ERR_BOUNDS, "invalid bit offset or width");
      s = &current->signals[current->signal_count];
      strcpy(s->name, words[1]); strcpy(s->unit, words[8]); s->start_bit = (uint8_t)a; s->bit_length = (uint8_t)b;
      if (strcmp(words[4], "BIG") && strcmp(words[4], "LITTLE")) BAD(CANFRAME_ERR_INVALID_FORMAT, "expected BIG or LITTLE");
      s->endian = !strcmp(words[4], "BIG") ? CANFRAME_ENDIAN_BIG : CANFRAME_ENDIAN_LITTLE;
      if (strcmp(words[5], "SIGNED") && strcmp(words[5], "UNSIGNED")) BAD(CANFRAME_ERR_INVALID_FORMAT, "expected SIGNED or UNSIGNED");
      s->is_signed = !strcmp(words[5], "SIGNED");
      if (!number_double(words[6], &s->scale) || !number_double(words[7], &s->offset)) BAD(CANFRAME_ERR_INVALID_FORMAT, "scale and offset must be finite decimals");
      current->signal_count++; continue;
    }
    if (!strcmp(words[0], "RANGE")) {
      can_signal_t *s = NULL;
      if (n != 4) BAD(CANFRAME_ERR_INVALID_FORMAT, "RANGE requires three fields");
      for (size_t i = 0; i < current->signal_count; i++) if (!strcmp(words[1], current->signals[i].name)) s = &current->signals[i];
      if (!s || s->has_range) BAD(CANFRAME_ERR_INVALID_FORMAT, "RANGE needs an earlier signal, once");
      if (!number_double(words[2], &s->range_min) || !number_double(words[3], &s->range_max) || s->range_min > s->range_max) BAD(CANFRAME_ERR_BOUNDS, "invalid RANGE");
      s->has_range = true; continue;
    }
    BAD(CANFRAME_ERR_INVALID_FORMAT, "unknown directive");
  }
  if (!header || current) BAD(CANFRAME_ERR_INVALID_FORMAT, "missing header or END");
  memcpy(definitions, candidate, loaded * sizeof(*definitions)); *count = loaded;
done:
  free(candidate);
  return status;
#undef BAD
}
canframe_status_t deffile_load_ex(const char *path, can_message_def_t *definitions,
                                size_t capacity, size_t *count, cfd_error_t *error) {
  FILE *f;
  uint8_t *data;
  size_t size;
  canframe_status_t status;
  if (count) *count = 0;
  if (!path) return fail(error, CANFRAME_ERR_IO, 0, "missing path");
  f = fopen(path, "rb");
  if (!f) return fail(error, CANFRAME_ERR_IO, 0, "cannot open definitions");
  data = malloc(CFD_MAX_BYTES + 1U);
  if (!data) { fclose(f); return fail(error, CANFRAME_ERR_IO, 0, "allocation failed"); }
  size = fread(data, 1, CFD_MAX_BYTES + 1U, f);
  if (ferror(f)) status = fail(error, CANFRAME_ERR_IO, 0, "cannot read definitions");
  else status = deffile_parse_bytes(data, size, definitions, capacity, count, error);
  fclose(f); free(data); return status;
}
canframe_status_t deffile_load(const char *path, can_message_def_t *definitions, size_t capacity, size_t *count) {
  return deffile_load_ex(path, definitions, capacity, count, NULL);
}
