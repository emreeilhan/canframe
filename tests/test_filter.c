#include <assert.h>

#include "cli.h"

static canframe_cli_options_t make_opts(void) {
  canframe_cli_options_t opts;
  opts.raw_mode = true;
  opts.json_mode = false;
  opts.inline_frame = NULL;
  opts.input_path = NULL;
  opts.filter_mode = CANFRAME_FILTER_NONE;
  opts.filter_id_count = 0;
  opts.filter_range_lo = 0;
  opts.filter_range_hi = 0;
  return opts;
}

void test_filter(void) {
  canframe_cli_options_t opts;
  char *argv_single[]  = { "canframe", "--filter", "0x123", NULL };
  char *argv_list[]    = { "canframe", "--filter", "0x123,0x456,0x18DAF110", NULL };
  char *argv_range[]   = { "canframe", "--filter", "0x100-0x1FF", NULL };
  char *argv_nopfx[]   = { "canframe", "--filter", "123", NULL };
  char *argv_bad[]     = { "canframe", "--filter", "GGGG", NULL };
  char *argv_badrange[]= { "canframe", "--filter", "0x200-0x100", NULL };

  /* no filter — every ID passes */
  opts = make_opts();
  assert(canframe_filter_matches(&opts, 0x000U) == true);
  assert(canframe_filter_matches(&opts, 0x7FFU) == true);
  assert(canframe_filter_matches(&opts, 0x18DAF110U) == true);

  /* single ID */
  opts = make_opts();
  assert(canframe_parse_args(3, argv_single, &opts) == 0);
  assert(opts.filter_mode == CANFRAME_FILTER_LIST);
  assert(opts.filter_id_count == 1);
  assert(canframe_filter_matches(&opts, 0x123U) == true);
  assert(canframe_filter_matches(&opts, 0x124U) == false);
  assert(canframe_filter_matches(&opts, 0x000U) == false);

  /* single ID without 0x prefix */
  opts = make_opts();
  assert(canframe_parse_args(3, argv_nopfx, &opts) == 0);
  assert(opts.filter_id_count == 1);
  assert(canframe_filter_matches(&opts, 0x123U) == true);

  /* comma-separated list */
  opts = make_opts();
  assert(canframe_parse_args(3, argv_list, &opts) == 0);
  assert(opts.filter_mode == CANFRAME_FILTER_LIST);
  assert(opts.filter_id_count == 3);
  assert(canframe_filter_matches(&opts, 0x123U) == true);
  assert(canframe_filter_matches(&opts, 0x456U) == true);
  assert(canframe_filter_matches(&opts, 0x18DAF110U) == true);
  assert(canframe_filter_matches(&opts, 0x100U) == false);

  /* range */
  opts = make_opts();
  assert(canframe_parse_args(3, argv_range, &opts) == 0);
  assert(opts.filter_mode == CANFRAME_FILTER_RANGE);
  assert(opts.filter_range_lo == 0x100U);
  assert(opts.filter_range_hi == 0x1FFU);
  assert(canframe_filter_matches(&opts, 0x100U) == true);
  assert(canframe_filter_matches(&opts, 0x150U) == true);
  assert(canframe_filter_matches(&opts, 0x1FFU) == true);
  assert(canframe_filter_matches(&opts, 0x0FFU) == false);
  assert(canframe_filter_matches(&opts, 0x200U) == false);

  /* invalid: non-hex ID */
  opts = make_opts();
  assert(canframe_parse_args(3, argv_bad, &opts) != 0);

  /* invalid: reversed range (lo > hi) */
  opts = make_opts();
  assert(canframe_parse_args(3, argv_badrange, &opts) != 0);
}
