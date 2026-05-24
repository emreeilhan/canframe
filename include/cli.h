#ifndef CLI_H
#define CLI_H

#include <stdbool.h>

typedef struct {
  bool raw_mode;
  bool json_mode;
  const char *inline_frame;
  const char *input_path;
} canframe_cli_options_t;

int canframe_parse_args(int argc, char **argv, canframe_cli_options_t *out);
void canframe_print_usage(const char *argv0);

#endif

