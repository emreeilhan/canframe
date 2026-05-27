#include "parser.h"

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void zero_frame(can_frame_t *frame) {
  memset(frame, 0, sizeof(*frame));
}

static int hex_value(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  return -1;
}

static bool parse_hex_u32(const char *start, size_t len, uint32_t *out) {
  char buffer[16];
  char *endptr;
  unsigned long value;

  if (len == 0 || len >= sizeof(buffer) || out == NULL) {
    return false;
  }

  memcpy(buffer, start, len);
  buffer[len] = '\0';

  value = strtoul(buffer, &endptr, 16);
  if (*endptr != '\0') {
    return false;
  }

  *out = (uint32_t)value;
  return true;
}

canframe_status_t parse_compact_frame(const char *input, can_frame_t *out) {
  const char *hash;
  size_t id_len;
  const char *data_ptr;
  size_t data_len;
  uint32_t parsed_id;
  size_t i;

  if (input == NULL || out == NULL) {
    return CANFRAME_ERR_INVALID_FORMAT;
  }

  hash = strchr(input, '#');
  if (hash == NULL) {
    return CANFRAME_ERR_INVALID_FORMAT;
  }

  id_len = (size_t)(hash - input);
  if (!parse_hex_u32(input, id_len, &parsed_id)) {
    return CANFRAME_ERR_INVALID_ID;
  }

  if (parsed_id > 0x1FFFFFFFU) {
    return CANFRAME_ERR_INVALID_ID;
  }

  zero_frame(out);
  out->id = parsed_id;
  out->is_extended = parsed_id > 0x7FFU;

  data_ptr = hash + 1;
  if (*data_ptr == 'R' && data_ptr[1] == '\0') {
    out->is_rtr = true;
    out->dlc = 0;
    return CANFRAME_OK;
  }

  data_len = strlen(data_ptr);
  if (data_len == 0 || (data_len % 2U) != 0U) {
    return CANFRAME_ERR_INVALID_DATA;
  }

  out->dlc = (uint8_t)(data_len / 2U);
  if (out->dlc > CANFRAME_MAX_DLC) {
    return CANFRAME_ERR_INVALID_DLC;
  }

  for (i = 0; i < data_len; i += 2U) {
    int high = hex_value(data_ptr[i]);
    int low = hex_value(data_ptr[i + 1U]);

    if (high < 0 || low < 0) {
      return CANFRAME_ERR_INVALID_DATA;
    }

    out->data[i / 2U] = (uint8_t)((high << 4) | low);
  }

  return CANFRAME_OK;
}

/* Parse: (1700000000.123456) can0 123#1122334455667788 */
static canframe_status_t parse_timestamped(const char *input, can_frame_t *out) {
  const char *p;
  const char *end;
  char ts_buf[32];
  size_t ts_len;
  char *ts_end;
  double timestamp;
  char ifname[CANFRAME_IFNAME_MAX];
  size_t ifname_len;
  canframe_status_t status;

  p = input;

  if (*p != '(') {
    return CANFRAME_ERR_INVALID_FORMAT;
  }
  ++p;

  end = strchr(p, ')');
  if (end == NULL) {
    return CANFRAME_ERR_INVALID_FORMAT;
  }

  ts_len = (size_t)(end - p);
  if (ts_len == 0 || ts_len >= sizeof(ts_buf)) {
    return CANFRAME_ERR_INVALID_FORMAT;
  }

  memcpy(ts_buf, p, ts_len);
  ts_buf[ts_len] = '\0';

  timestamp = strtod(ts_buf, &ts_end);
  if (ts_end == ts_buf || *ts_end != '\0') {
    return CANFRAME_ERR_INVALID_FORMAT;
  }

  p = end + 1;
  while (isspace((unsigned char)*p)) {
    ++p;
  }

  end = p;
  while (*end != '\0' && !isspace((unsigned char)*end)) {
    ++end;
  }

  ifname_len = (size_t)(end - p);
  if (ifname_len == 0 || ifname_len >= CANFRAME_IFNAME_MAX) {
    return CANFRAME_ERR_INVALID_FORMAT;
  }

  memcpy(ifname, p, ifname_len);
  ifname[ifname_len] = '\0';

  p = end;
  while (isspace((unsigned char)*p)) {
    ++p;
  }

  status = parse_compact_frame(p, out);
  if (status != CANFRAME_OK) {
    return status;
  }

  out->has_timestamp = true;
  out->timestamp = timestamp;
  memcpy(out->ifname, ifname, ifname_len + 1U);

  return CANFRAME_OK;
}

/* Parse: can0  123  [8]  11 22 33 44 55 66 77 88 */
static canframe_status_t parse_verbose(const char *input, can_frame_t *out) {
  const char *p;
  const char *end;
  char ifname[CANFRAME_IFNAME_MAX];
  size_t ifname_len;
  uint32_t id;
  char dlc_buf[8];
  size_t dlc_str_len;
  char *dlc_end_ptr;
  unsigned long dlc_val;
  uint8_t dlc;
  uint8_t bytes[CANFRAME_MAX_DLC];
  uint8_t count;
  int high;
  int low;

  p = input;

  end = p;
  while (*end != '\0' && !isspace((unsigned char)*end)) {
    ++end;
  }

  ifname_len = (size_t)(end - p);
  if (ifname_len == 0 || ifname_len >= CANFRAME_IFNAME_MAX) {
    return CANFRAME_ERR_INVALID_FORMAT;
  }

  memcpy(ifname, p, ifname_len);
  ifname[ifname_len] = '\0';

  p = end;
  while (isspace((unsigned char)*p)) {
    ++p;
  }

  end = p;
  while (*end != '\0' && !isspace((unsigned char)*end) && *end != '[') {
    ++end;
  }

  if (!parse_hex_u32(p, (size_t)(end - p), &id)) {
    return CANFRAME_ERR_INVALID_ID;
  }

  if (id > 0x1FFFFFFFU) {
    return CANFRAME_ERR_INVALID_ID;
  }

  p = end;
  while (isspace((unsigned char)*p)) {
    ++p;
  }

  if (*p != '[') {
    return CANFRAME_ERR_INVALID_FORMAT;
  }
  ++p;

  end = strchr(p, ']');
  if (end == NULL) {
    return CANFRAME_ERR_INVALID_FORMAT;
  }

  dlc_str_len = (size_t)(end - p);
  if (dlc_str_len == 0 || dlc_str_len >= sizeof(dlc_buf)) {
    return CANFRAME_ERR_INVALID_FORMAT;
  }

  memcpy(dlc_buf, p, dlc_str_len);
  dlc_buf[dlc_str_len] = '\0';

  dlc_val = strtoul(dlc_buf, &dlc_end_ptr, 10);
  if (*dlc_end_ptr != '\0' || dlc_val > CANFRAME_MAX_DLC) {
    return CANFRAME_ERR_INVALID_DLC;
  }

  dlc = (uint8_t)dlc_val;

  p = end + 1;
  while (isspace((unsigned char)*p)) {
    ++p;
  }

  count = 0;
  while (*p != '\0' && count < dlc) {
    if (!isxdigit((unsigned char)*p) || !isxdigit((unsigned char)*(p + 1U))) {
      return CANFRAME_ERR_INVALID_DATA;
    }

    high = hex_value(*p);
    low = hex_value(*(p + 1U));

    if (high < 0 || low < 0) {
      return CANFRAME_ERR_INVALID_DATA;
    }

    bytes[count] = (uint8_t)((high << 4) | low);
    ++count;
    p += 2U;

    while (isspace((unsigned char)*p)) {
      ++p;
    }
  }

  if (count != dlc) {
    return CANFRAME_ERR_INVALID_DATA;
  }

  zero_frame(out);
  out->id = id;
  out->is_extended = id > 0x7FFU;
  out->dlc = dlc;
  memcpy(out->data, bytes, dlc);
  memcpy(out->ifname, ifname, ifname_len + 1U);

  return CANFRAME_OK;
}

canframe_status_t parse_candump_line(const char *input, can_frame_t *out) {
  if (input == NULL || out == NULL) {
    return CANFRAME_ERR_INVALID_FORMAT;
  }

  if (*input == '(') {
    return parse_timestamped(input, out);
  }

  return parse_verbose(input, out);
}

canframe_status_t parse_line(const char *input, can_frame_t *out) {
  const char *trimmed;
  const char *hash;
  const char *p;

  trimmed = input;
  while (*trimmed != '\0' && isspace((unsigned char)*trimmed)) {
    ++trimmed;
  }

  if (*trimmed == '(') {
    return parse_candump_line(trimmed, out);
  }

  hash = strchr(trimmed, '#');
  if (hash != NULL) {
    for (p = trimmed; p < hash; ++p) {
      if (isspace((unsigned char)*p)) {
        return parse_candump_line(trimmed, out);
      }
    }
    return parse_compact_frame(trimmed, out);
  }

  return parse_candump_line(trimmed, out);
}

const char *canframe_status_string(canframe_status_t status) {
  switch (status) {
    case CANFRAME_OK:
      return "ok";
    case CANFRAME_ERR_INVALID_ID:
      return "invalid CAN ID";
    case CANFRAME_ERR_INVALID_DLC:
      return "invalid DLC";
    case CANFRAME_ERR_INVALID_DATA:
      return "invalid frame data";
    case CANFRAME_ERR_INVALID_FORMAT:
      return "invalid input format";
    case CANFRAME_ERR_UNSUPPORTED:
      return "format not supported yet";
  }

  return "unknown error";
}
