#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CANFRAME_FILTER_MAX_IDS 16

typedef enum {
  CANFRAME_FILTER_NONE = 0,
  CANFRAME_FILTER_LIST,
  CANFRAME_FILTER_RANGE,
} canframe_filter_mode_t;

typedef struct {
  bool raw_mode;
  bool json_mode;
  const char *inline_frame;
  const char *input_path;
  canframe_filter_mode_t filter_mode;
  uint32_t filter_ids[CANFRAME_FILTER_MAX_IDS];
  size_t filter_id_count;
  uint32_t filter_range_lo;
  uint32_t filter_range_hi;
} canframe_cli_options_t;

int canframe_parse_args(int argc, char **argv, canframe_cli_options_t *out);
bool canframe_filter_matches(const canframe_cli_options_t *opts, uint32_t id);
void canframe_print_usage(const char *argv0);

#endif
