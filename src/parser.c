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

canframe_status_t parse_candump_line(const char *input, can_frame_t *out) {
  (void)input;
  (void)out;
  return CANFRAME_ERR_UNSUPPORTED;
}

canframe_status_t parse_line(const char *input, can_frame_t *out) {
  const char *trimmed = input;

  while (*trimmed != '\0' && isspace((unsigned char)*trimmed)) {
    ++trimmed;
  }

  if (strchr(trimmed, '#') != NULL) {
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

