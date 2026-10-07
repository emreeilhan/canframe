#include "parser.h"
#include "numbers.h"
#include <ctype.h>
#include <string.h>
static void space(const char **p) { while (isspace((unsigned char)**p)) (*p)++; }
static bool byte(const char *p, uint8_t *v) {
  uint64_t n;
  if (!number_unsigned(p, 2, 16, 255, &n)) return false;
  *v = (uint8_t)n;
  return true;
}
canframe_status_t parse_compact_frame(const char *input, can_frame_t *out) {
  const char *hash;
  uint64_t id;
  size_t n;
  can_frame_t frame = {0};
  if (!input || !out) return CANFRAME_ERR_INVALID_FORMAT;
  hash = strchr(input, '#');
  if (!hash) return CANFRAME_ERR_INVALID_FORMAT;
  if ((size_t)(hash - input) > (input[0] == '0' && (input[1] == 'x' || input[1] == 'X') ? 10U : 8U) || !number_unsigned(input, (size_t)(hash - input), 16, 0x1fffffff, &id)) return CANFRAME_ERR_INVALID_ID;
  frame.id = (uint32_t)id;
  frame.is_extended = id > 0x7ff || (size_t)(hash - input) == 8 || ((size_t)(hash - input) == 10 && input[0] == '0' && (input[1] == 'x' || input[1] == 'X'));
  hash++;
  if (strcmp(hash, "R") == 0) { frame.is_rtr = true; *out = frame; return CANFRAME_OK; }
  n = strlen(hash);
  if (n > 2U * CANFRAME_MAX_DLC) return CANFRAME_ERR_INVALID_DLC;
  if (n % 2) return CANFRAME_ERR_INVALID_DATA;
  /* Empty payload represents a valid zero-length Classical CAN data frame. */
  for (size_t i = 0; i < n / 2; i++) if (!byte(hash + i * 2, &frame.data[i])) return CANFRAME_ERR_INVALID_DATA;
  frame.dlc = (uint8_t)(n / 2);
  *out = frame;
  return CANFRAME_OK;
}
static canframe_status_t timestamped(const char *p, can_frame_t *out) {
  const char *end;
  can_frame_t frame;
  uint64_t ns;
  char ifname[CANFRAME_IFNAME_MAX];
  size_t n;
  canframe_status_t status;
  end = strchr(++p, ')');
  if (!end || !number_timestamp(p, (size_t)(end - p), &ns)) return CANFRAME_ERR_INVALID_FORMAT;
  p = end + 1;
  if (!isspace((unsigned char)*p)) return CANFRAME_ERR_INVALID_FORMAT;
  space(&p); end = p;
  while (*end && !isspace((unsigned char)*end)) end++;
  n = (size_t)(end - p);
  if (!n || n >= sizeof(ifname) || !*end) return CANFRAME_ERR_INVALID_FORMAT;
  memcpy(ifname, p, n); ifname[n] = 0;
  p = end; space(&p);
  status = parse_compact_frame(p, &frame);
  if (status != CANFRAME_OK) return status;
  frame.has_timestamp = true;
  frame.timestamp_ns = ns;
  frame.timestamp = (double)ns / 1e9;
  frame.time_source = CANFRAME_TIME_SOURCE;
  memcpy(frame.ifname, ifname, n + 1);
  *out = frame;
  return CANFRAME_OK;
}
static canframe_status_t verbose(const char *p, can_frame_t *out) {
  const char *end = p;
  can_frame_t frame = {0};
  uint64_t id, dlc;
  size_t n, id_width;
  while (*end && !isspace((unsigned char)*end)) end++;
  n = (size_t)(end - p);
  if (!n || n >= sizeof(frame.ifname) || !*end) return CANFRAME_ERR_INVALID_FORMAT;
  memcpy(frame.ifname, p, n);
  p = end; space(&p); end = p;
  while (*end && !isspace((unsigned char)*end)) end++;
  id_width = (size_t)(end - p);
  if (id_width > (p[0] == '0' && (p[1] == 'x' || p[1] == 'X') ? 10U : 8U) || !number_unsigned(p, id_width, 16, 0x1fffffff, &id)) return CANFRAME_ERR_INVALID_ID;
  if (id_width >= 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) id_width -= 2;
  p = end; space(&p);
  if (*p++ != '[') return CANFRAME_ERR_INVALID_FORMAT;
  end = strchr(p, ']');
  if (!end || !number_unsigned(p, (size_t)(end - p), 10, 8, &dlc)) return CANFRAME_ERR_INVALID_DLC;
  p = end + 1;
  for (size_t i = 0; i < dlc; i++) {
    if (!isspace((unsigned char)*p)) return CANFRAME_ERR_INVALID_DATA;
    space(&p);
    if (strlen(p) < 2 || !byte(p, &frame.data[i])) return CANFRAME_ERR_INVALID_DATA;
    p += 2;
    if (*p && !isspace((unsigned char)*p)) return CANFRAME_ERR_INVALID_DATA;
  }
  space(&p);
  if (*p) return CANFRAME_ERR_INVALID_DATA;
  frame.id = (uint32_t)id; frame.is_extended = id > 0x7ff || id_width == 8; frame.dlc = (uint8_t)dlc;
  *out = frame;
  return CANFRAME_OK;
}
canframe_status_t parse_candump_line(const char *input, can_frame_t *out) {
  if (!input || !out) return CANFRAME_ERR_INVALID_FORMAT;
  return *input == '(' ? timestamped(input, out) : verbose(input, out);
}
canframe_status_t parse_bytes(const uint8_t *input, size_t size, can_frame_t *out) {
  char line[CANFRAME_MAX_LINE + 1];
  const char *p, *hash;
  if (!input || !out) return CANFRAME_ERR_INVALID_FORMAT;
  if (size > CANFRAME_MAX_LINE) return CANFRAME_ERR_BOUNDS;
  if (memchr(input, 0, size)) return CANFRAME_ERR_INVALID_FORMAT;
  memcpy(line, input, size); line[size] = 0;
  while (size && isspace((unsigned char)line[size - 1])) line[--size] = 0;
  p = line; space(&p);
  if (!*p) return CANFRAME_ERR_INVALID_FORMAT;
  if (*p == '(') return timestamped(p, out);
  hash = strchr(p, '#');
  if (hash) {
    const char *q;
    for (q = p; q < hash; q++) if (isspace((unsigned char)*q)) return CANFRAME_ERR_INVALID_FORMAT;
    return parse_compact_frame(p, out);
  }
  return verbose(p, out);
}
canframe_status_t parse_line(const char *input, can_frame_t *out) {
  if (!input) return CANFRAME_ERR_INVALID_FORMAT;
  size_t n = 0;
  while (n <= CANFRAME_MAX_LINE && input[n]) n++;
  return parse_bytes((const uint8_t *)input, n, out);
}
const char *canframe_status_string(canframe_status_t s) {
  switch (s) {
    case CANFRAME_OK: return "ok";
    case CANFRAME_ERR_INVALID_ID: return "invalid CAN ID";
    case CANFRAME_ERR_INVALID_DLC: return "invalid DLC";
    case CANFRAME_ERR_INVALID_DATA: return "invalid data";
    case CANFRAME_ERR_INVALID_FORMAT: return "invalid format";
    case CANFRAME_ERR_UNSUPPORTED: return "format not supported yet";
    case CANFRAME_ERR_IO: return "I/O error";
    case CANFRAME_ERR_BOUNDS: return "out of bounds";
    case CANFRAME_ERR_CAPACITY: return "capacity exceeded";
    case CANFRAME_ERR_NO_DEFINITION: return "no_definition";
    case CANFRAME_ERR_NOT_APPLICABLE: return "not_applicable";
  }
  return "unknown error";
}
