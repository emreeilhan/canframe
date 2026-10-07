#include "cli.h"
#include "canframe.h"
#include "numbers.h"
#include <stdio.h>
#include <string.h>
static bool hex(const char *s, size_t n, uint32_t *out) {
  uint64_t v;
  if (!number_unsigned(s, n, 16, 0x1fffffff, &v)) return false;
  *out = (uint32_t)v; return true;
}
static int filter(const char *arg, canframe_cli_options_t *out) {
  const char *dash = strchr(arg, '-'), *comma = strchr(arg, ',');
  if (!*arg || (dash && comma)) return -1;
  if (dash) {
    out->filter_mode = CANFRAME_FILTER_RANGE;
    if (!hex(arg, (size_t)(dash - arg), &out->filter_range_lo) || !hex(dash + 1, strlen(dash + 1), &out->filter_range_hi) || out->filter_range_lo > out->filter_range_hi) return -1;
    return 0;
  }
  out->filter_mode = CANFRAME_FILTER_LIST;
  do {
    comma = strchr(arg, ',');
    size_t n = comma ? (size_t)(comma - arg) : strlen(arg);
    if (out->filter_id_count == CANFRAME_FILTER_MAX_IDS || !hex(arg, n, &out->filter_ids[out->filter_id_count])) return -1;
    out->filter_id_count++;
    if (!comma) break;
    arg = comma + 1;
    if (!*arg) return -1;
  } while (true);
  return 0;
}
int canframe_parse_args(int argc, char **argv, canframe_cli_options_t *out) {
  if (!out || !argv || argc < 1) return -1;
  memset(out, 0, sizeof(*out)); out->raw_mode = true;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    if (!strcmp(a, "--help") || !strcmp(a, "-h")) return 1;
    if (!strcmp(a, "--raw")) { out->raw_mode = true; out->json_mode = false; continue; }
    if (!strcmp(a, "--json")) { out->raw_mode = false; out->json_mode = true; continue; }
    if (!strcmp(a, "--signals")) { out->signals = true; continue; }
    if (!strcmp(a, "--diagnostics")) { out->diagnostics = true; continue; }
    if (!strcmp(a, "--input") || !strcmp(a, "--decode") || !strcmp(a, "--interface") || !strcmp(a, "--definitions") || !strcmp(a, "--defs")) {
      const char **slot = (!strcmp(a, "--input") || !strcmp(a, "--decode")) ? &out->input_path : !strcmp(a, "--interface") ? &out->interface : &out->definitions_path;
      if (*slot || ++i == argc || !*argv[i]) return -1;
      *slot = argv[i]; if (!strcmp(a, "--decode")) out->signals = true; continue;
    }
    if (!strcmp(a, "--filter")) {
      if (out->filter_mode != CANFRAME_FILTER_NONE || ++i == argc || filter(argv[i], out)) return -1;
      continue;
    }
    if (!strcmp(a, "--count") || !strcmp(a, "--duration-ms") || !strcmp(a, "--idle-timeout-ms")) {
      uint64_t v;
      if (++i == argc || !number_unsigned(argv[i], strlen(argv[i]), 10, !strcmp(a, "--count") ? UINT64_MAX : UINT32_MAX, &v) || !v) return -1;
      if (!strcmp(a, "--count")) { if (out->count) return -1; out->count = v; }
      else if (!strcmp(a, "--duration-ms")) { if (out->duration_ms) return -1; out->duration_ms = (uint32_t)v; }
      else { if (out->idle_timeout_ms) return -1; out->idle_timeout_ms = (uint32_t)v; }
      continue;
    }
    if (*a == '-' || out->inline_frame) return -1;
    out->inline_frame = a;
  }
  if ((out->signals || out->diagnostics) && !out->definitions_path) return -1;
  if ((out->input_path && out->inline_frame) || (out->interface && (out->input_path || out->inline_frame))) return -1;
  if (!out->interface && (out->count || out->duration_ms || out->idle_timeout_ms)) return -1;
  if (out->interface && strlen(out->interface) >= CANFRAME_IFNAME_MAX) return -1;
  return 0;
}
bool canframe_filter_matches(const canframe_cli_options_t *opts, uint32_t id) {
  if (!opts) return false;
  if (opts->filter_mode == CANFRAME_FILTER_NONE) return true;
  if (opts->filter_mode == CANFRAME_FILTER_RANGE) return id >= opts->filter_range_lo && id <= opts->filter_range_hi;
  for (size_t i = 0; i < opts->filter_id_count; i++) if (opts->filter_ids[i] == id) return true;
  return false;
}
void canframe_print_usage(const char *name) {
  fprintf(stderr, "Usage: %s [--raw|--json] [--filter ID|ID,ID|LO-HI]\n"
          "  [--definitions|--defs file.cfd] [--signals] [--diagnostics]\n"
          "  [--input path | FRAME | --interface can0]\n"
          "  Alias: --decode path means --signals --input path\n"
          "  Live Linux only: [--count N] [--duration-ms N] [--idle-timeout-ms N]\n", name);
}
