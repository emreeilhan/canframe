#include "cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parse_hex_id(const char *start, const char *end, uint32_t *out) {
  char buf[12];
  size_t len;
  char *endptr;
  unsigned long val;

  len = (size_t)(end - start);
  if (len == 0 || len >= sizeof(buf)) {
    return false;
  }

  memcpy(buf, start, len);
  buf[len] = '\0';

  val = strtoul(buf, &endptr, 16);
  if (*endptr != '\0') {
    return false;
  }

  if (val > 0x1FFFFFFFU) {
    return false;
  }

  *out = (uint32_t)val;
  return true;
}

static int parse_filter_arg(const char *arg, canframe_cli_options_t *out) {
  const char *p;
  const char *comma;
  const char *dash;
  const char *next;
  const char *end;

  comma = strchr(arg, ',');

  dash = NULL;
  for (p = arg + 1; *p != '\0'; ++p) {
    if (*p == '-') {
      dash = p;
      break;
    }
  }

  if (comma != NULL) {
    out->filter_mode = CANFRAME_FILTER_LIST;
    out->filter_id_count = 0;
    p = arg;
    while (*p != '\0') {
      next = strchr(p, ',');
      end = (next != NULL) ? next : (p + strlen(p));
      if (out->filter_id_count >= CANFRAME_FILTER_MAX_IDS) {
        return -1;
      }
      if (!parse_hex_id(p, end, &out->filter_ids[out->filter_id_count])) {
        return -1;
      }
      out->filter_id_count++;
      p = (next != NULL) ? (next + 1) : end;
    }
    return 0;
  }

  if (dash != NULL) {
    out->filter_mode = CANFRAME_FILTER_RANGE;
    if (!parse_hex_id(arg, dash, &out->filter_range_lo)) {
      return -1;
    }
    if (!parse_hex_id(dash + 1, arg + strlen(arg), &out->filter_range_hi)) {
      return -1;
    }
    if (out->filter_range_lo > out->filter_range_hi) {
      return -1;
    }
    return 0;
  }

  out->filter_mode = CANFRAME_FILTER_LIST;
  out->filter_id_count = 1;
  if (!parse_hex_id(arg, arg + strlen(arg), &out->filter_ids[0])) {
    return -1;
  }
  return 0;
}

int canframe_parse_args(int argc, char **argv, canframe_cli_options_t *out) {
  int i;

  if (out == NULL) {
    return -1;
  }

  out->raw_mode = true;
  out->json_mode = false;
  out->inline_frame = NULL;
  out->input_path = NULL;
  out->filter_mode = CANFRAME_FILTER_NONE;
  out->filter_id_count = 0;
  out->filter_range_lo = 0;
  out->filter_range_hi = 0;

  for (i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--raw") == 0) {
      out->raw_mode = true;
      out->json_mode = false;
      continue;
    }

    if (strcmp(argv[i], "--json") == 0) {
      out->json_mode = true;
      out->raw_mode = false;
      continue;
    }

    if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
      out->input_path = argv[++i];
      continue;
    }

    if (strcmp(argv[i], "--filter") == 0 && i + 1 < argc) {
      if (parse_filter_arg(argv[++i], out) != 0) {
        return -1;
      }
      continue;
    }

    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      return 1;
    }

    if (out->inline_frame == NULL) {
      out->inline_frame = argv[i];
      continue;
    }

    return -1;
  }

  return 0;
}

bool canframe_filter_matches(const canframe_cli_options_t *opts, uint32_t id) {
  size_t i;

  if (opts->filter_mode == CANFRAME_FILTER_NONE) {
    return true;
  }

  if (opts->filter_mode == CANFRAME_FILTER_RANGE) {
    return id >= opts->filter_range_lo && id <= opts->filter_range_hi;
  }

  for (i = 0; i < opts->filter_id_count; ++i) {
    if (opts->filter_ids[i] == id) {
      return true;
    }
  }

  return false;
}

void canframe_print_usage(const char *argv0) {
  fprintf(stderr,
          "Usage: %s [--raw|--json] [--filter ID|ID,ID|LO-HI] [--input path] [FRAME]\n"
          "\n"
          "Examples:\n"
          "  %s 123#1122334455667788\n"
          "  cat samples/compact.log | %s --raw\n"
          "  cat samples/candump-timestamped.log | %s --json\n"
          "  cat log | %s --filter 0x123 --json\n"
          "  cat log | %s --filter 0x100-0x1FF --raw\n",
          argv0, argv0, argv0, argv0, argv0, argv0);
}
