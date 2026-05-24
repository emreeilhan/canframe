#include "cli.h"

#include <stdio.h>
#include <string.h>

int canframe_parse_args(int argc, char **argv, canframe_cli_options_t *out) {
  int i;

  if (out == NULL) {
    return -1;
  }

  out->raw_mode = true;
  out->json_mode = false;
  out->inline_frame = NULL;
  out->input_path = NULL;

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

void canframe_print_usage(const char *argv0) {
  fprintf(stderr,
          "Usage: %s [--raw|--json] [--input path] [FRAME]\n"
          "\n"
          "Examples:\n"
          "  %s 123#1122334455667788\n"
          "  cat samples/compact.log | %s --raw\n",
          argv0, argv0, argv0);
}

