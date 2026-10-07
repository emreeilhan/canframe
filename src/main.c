#define _POSIX_C_SOURCE 200809L
#include "cli.h"
#include "deffile.h"
#include "output.h"
#include "parser.h"
#include "socketcan.h"
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
static volatile sig_atomic_t stopped;
static void stop(int signal_number) { (void)signal_number; stopped = 1; }
typedef struct {
  canframe_cli_options_t options;
  can_message_def_t definitions[CANFRAME_MAX_DEFINITIONS];
  size_t definition_count;
  diagnostics_t diagnostics;
  bool decode_failed;
} context_t;
static int process_frame(context_t *ctx, const can_frame_t *frame) {
  if (!canframe_filter_matches(&ctx->options, frame->id)) return 0;
  if (ctx->options.signals) {
    const can_message_def_t *d = find_definition(ctx->definitions, ctx->definition_count, frame->id, frame->is_extended);
    canframe_signal_result_t results[CANFRAME_MAX_SIGNALS];
    size_t count = 0;
    canframe_status_t status = frame->is_rtr || frame->is_error ? CANFRAME_ERR_NOT_APPLICABLE : !d ? CANFRAME_ERR_NO_DEFINITION :
      decode_signals(d, frame, results, CANFRAME_MAX_SIGNALS, &count);
    output_signals(stdout, frame, ctx->options.json_mode, status, results, count);
    if (status != CANFRAME_OK && status != CANFRAME_ERR_NO_DEFINITION && status != CANFRAME_ERR_NOT_APPLICABLE) ctx->decode_failed = true;
  } else if (ctx->options.json_mode) output_json(stdout, frame);
  else output_raw(stdout, frame);
  if (ctx->options.diagnostics) {
    diagnostic_event_t event;
    canframe_status_t status = diagnostics_observe(&ctx->diagnostics, frame, &event);
    if (status == CANFRAME_OK) output_diagnostic(stdout, &event);
    else if (status == CANFRAME_ERR_CAPACITY || status == CANFRAME_ERR_INVALID_FORMAT) {
      fprintf(stderr, "canframe: diagnostics: %s\n", canframe_status_string(status)); return -1;
    }
  }
  return 1;
}
static int process_stream(FILE *f, context_t *ctx) {
  uint8_t line[CANFRAME_MAX_LINE + 1];
  size_t length = 0, line_number = 1;
  int c;
  do {
    c = fgetc(f);
    if (c != EOF && c != '\n') {
      if (length == CANFRAME_MAX_LINE) { fprintf(stderr, "canframe: line %zu exceeds %d bytes\n", line_number, CANFRAME_MAX_LINE); return 1; }
      line[length++] = (uint8_t)c; continue;
    }
    bool blank = true;
    for (size_t i = 0; i < length; i++) if (!isspace(line[i])) { blank = false; break; }
    if (!blank) {
      can_frame_t frame;
      canframe_status_t status = parse_bytes(line, length, &frame);
      if (status != CANFRAME_OK) { fprintf(stderr, "canframe: line %zu: %s\n", line_number, canframe_status_string(status)); return 1; }
      if (process_frame(ctx, &frame) < 0) return 1;
    }
    length = 0; line_number++;
  } while (c != EOF);
  if (ferror(f)) { perror("canframe: read"); return 1; }
  return ctx->decode_failed ? 2 : 0;
}
static int process_live(context_t *ctx) {
  char reason[160];
  int fd = socketcan_open(ctx->options.interface, reason, sizeof(reason));
  uint64_t start = socketcan_monotonic_ns(), last = start, accepted = 0;
  struct sigaction action;
  if (fd < 0) { fprintf(stderr, "canframe: %s\n", reason); return 1; }
  if (ctx->options.diagnostics && diagnostics_init(&ctx->diagnostics, ctx->definitions, ctx->definition_count, ctx->options.interface, start) != CANFRAME_OK) { socketcan_close(fd); return 1; }
  if (ctx->options.diagnostics) {
    size_t kept = 0;
    for (size_t i = 0; i < ctx->diagnostics.count; i++) if (canframe_filter_matches(&ctx->options, ctx->diagnostics.entries[i].definition->id)) ctx->diagnostics.entries[kept++] = ctx->diagnostics.entries[i];
    ctx->diagnostics.count = kept;
  }
  memset(&action, 0, sizeof(action)); action.sa_handler = stop; sigemptyset(&action.sa_mask);
  if (sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL)) { perror("canframe: signals"); socketcan_close(fd); return 1; }
  fprintf(stderr, "canframe: listening on %s (Classical CAN; userspace_observed)\n", ctx->options.interface); fflush(stderr);
  int result = 0;
  while (!stopped) {
    can_frame_t frame;
    int rc = socketcan_receive(fd, ctx->options.interface, 25, &frame, reason, sizeof(reason));
    if (stopped) break;
    if (rc == -2) continue;
    if (rc < 0) { fprintf(stderr, "canframe: %s\n", reason); result = 1; break; }
    uint64_t now = socketcan_monotonic_ns();
    if (rc == 1) {
      int matched = process_frame(ctx, &frame);
      if (matched < 0) { result = 1; break; }
      if (matched) { accepted++; last = now; }
    }
    if (ctx->options.diagnostics) {
      diagnostic_event_t events[CANFRAME_DIAGNOSTIC_CAPACITY];
      size_t count = diagnostics_poll(&ctx->diagnostics, now, events);
      for (size_t i = 0; i < count; i++) output_diagnostic(stdout, &events[i]);
    }
    fflush(stdout);
    if ((ctx->options.count && accepted >= ctx->options.count) ||
        (ctx->options.duration_ms && now - start >= (uint64_t)ctx->options.duration_ms * 1000000) ||
        (ctx->options.idle_timeout_ms && now - last >= (uint64_t)ctx->options.idle_timeout_ms * 1000000)) break;
  }
  socketcan_close(fd); return result ? result : ctx->decode_failed ? 2 : 0;
}
int main(int argc, char **argv) {
  context_t ctx = {0};
  int rc = canframe_parse_args(argc, argv, &ctx.options);
  if (rc) { canframe_print_usage(argv[0]); return rc == 1 ? 0 : 1; }
  if (ctx.options.definitions_path) {
    cfd_error_t error;
    canframe_status_t status = deffile_load_ex(ctx.options.definitions_path, ctx.definitions, CANFRAME_MAX_DEFINITIONS, &ctx.definition_count, &error);
    if (status != CANFRAME_OK) { fprintf(stderr, "canframe: definitions line %zu: %s (%s)\n", error.line, error.reason, canframe_status_string(status)); return 1; }
  }
  if (ctx.options.diagnostics && diagnostics_init(&ctx.diagnostics, ctx.definitions, ctx.definition_count, NULL, 0) != CANFRAME_OK) return 1;
  if (ctx.options.interface) rc = process_live(&ctx);
  else if (ctx.options.inline_frame) {
    can_frame_t frame;
    canframe_status_t status = parse_line(ctx.options.inline_frame, &frame);
    if (status != CANFRAME_OK) { fprintf(stderr, "canframe: %s\n", canframe_status_string(status)); return 1; }
    rc = process_frame(&ctx, &frame) < 0 ? 1 : ctx.decode_failed ? 2 : 0;
  } else {
    FILE *f = ctx.options.input_path ? fopen(ctx.options.input_path, "rb") : stdin;
    if (!f) { perror("canframe: input"); return 1; }
    rc = process_stream(f, &ctx);
    if (f != stdin) fclose(f);
  }
  if (ctx.options.diagnostics) for (size_t i = 0; i < ctx.diagnostics.count; i++) output_diagnostic_summary(stdout, &ctx.diagnostics.entries[i]);
  if (ferror(stdout)) { fprintf(stderr, "canframe: output error\n"); return 1; }
  return rc;
}
