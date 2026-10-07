#define _GNU_SOURCE
#include "output.h"
#include "parser.h"
#include <inttypes.h>
#include <locale.h>
#include <math.h>
#ifdef __APPLE__
#include <xlocale.h>
#endif
/* Preserve valid UTF-8; malformed log bytes must not invalidate JSON output. */
static size_t utf8_length(const unsigned char *s) {
  unsigned char first = s[0];
  size_t length;
  if (first >= 0xc2 && first <= 0xdf) length = 2;
  else if (first >= 0xe0 && first <= 0xef) length = 3;
  else if (first >= 0xf0 && first <= 0xf4) length = 4;
  else return 0;
  for (size_t i = 1; i < length; i++) if (s[i] < 0x80 || s[i] > 0xbf) return 0;
  if ((first == 0xe0 && s[1] < 0xa0) || (first == 0xed && s[1] > 0x9f) ||
      (first == 0xf0 && s[1] < 0x90) || (first == 0xf4 && s[1] > 0x8f)) return 0;
  return length;
}
static void string(FILE *f, const char *s) {
  fputc('"', f);
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') { fputc('\\', f); fputc(c, f); }
    else if (c < 32) fprintf(f, "\\u%04x", c);
    else if (c >= 0x80) {
      size_t length = utf8_length((const unsigned char *)s);
      if (length) { fwrite(s, 1, length, f); s += length - 1; }
      else fprintf(f, "\\u%04x", c);
    }
    else fputc(c, f);
  }
  fputc('"', f);
}
static void number(FILE *f, double v, bool timestamp) {
  locale_t c, old;
  if (!isfinite(v)) { fputs("null", f); return; }
  c = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
  if (!c) { fputs("null", f); return; }
  old = uselocale(c);
  if (timestamp) fprintf(f, "%.6f", v); else fprintf(f, "%.17g", v);
  uselocale(old); freelocale(c);
}
void output_raw(FILE *f, const can_frame_t *frame) {
  fprintf(f, "[0x%03" PRIX32 "] [%u]", frame->id, frame->dlc);
  if (frame->is_extended) fputs(" [ext]", f);
  if (frame->is_rtr) fputs(" RTR", f);
  if (!frame->is_rtr) for (unsigned i = 0; i < frame->dlc && i < 8; i++) fprintf(f, " %02X", frame->data[i]);
  fputc('\n', f);
}
static void body(FILE *f, const can_frame_t *frame) {
  fputc('{', f);
  if (frame->has_timestamp) { fputs("\"timestamp\":", f); number(f, frame->timestamp, true); fputc(',', f); }
  if (frame->ifname[0]) { fputs("\"interface\":", f); string(f, frame->ifname); fputc(',', f); }
  fprintf(f, "\"id\":\"0x%03" PRIX32 "\",\"extended\":%s,\"dlc\":%u,\"data\":[", frame->id, frame->is_extended ? "true" : "false", frame->dlc);
  if (!frame->is_rtr) for (unsigned i = 0; i < frame->dlc && i < 8; i++) fprintf(f, "%s%u", i ? "," : "", frame->data[i]);
  fputc(']', f);
  if (frame->time_source == CANFRAME_TIME_USERSPACE) fprintf(f, ",\"time_source\":\"userspace_observed\",\"timestamp_ns\":\"%" PRIu64 "\"", frame->timestamp_ns);
}
void output_json(FILE *f, const can_frame_t *frame) {
  body(f, frame);
  if (frame->time_source == CANFRAME_TIME_USERSPACE) fprintf(f, ",\"rtr\":%s", frame->is_rtr ? "true" : "false");
  fputs("}\n", f);
}
static const char *quality(canframe_signal_quality_t q) {
  return q == CANFRAME_SIGNAL_NUMERIC_ERROR ? "numeric_error" : q == CANFRAME_SIGNAL_OUT_OF_RANGE ? "out_of_range" : "valid";
}
void output_signals(FILE *f, const can_frame_t *frame, bool json, canframe_status_t status,
                    const canframe_signal_result_t *signals, size_t count) {
  if (!json) {
    output_raw(f, frame); fprintf(f, "  decode: %s\n", canframe_status_string(status));
    for (size_t i = 0; i < count; i++) {
      const canframe_signal_result_t *r = &signals[i];
      fprintf(f, "  %s raw=0x%016" PRIX64 " physical=", r->definition->name, r->raw_unsigned);
      number(f, r->quality == CANFRAME_SIGNAL_NUMERIC_ERROR ? NAN : r->physical, false);
      fprintf(f, " %s %s%s\n", r->definition->unit, quality(r->quality), r->precision_warning ? " precision_warning" : "");
    }
    return;
  }
  body(f, frame); fprintf(f, ",\"rtr\":%s,\"decode_status\":", frame->is_rtr ? "true" : "false");
  string(f, canframe_status_string(status)); fputs(",\"signals\":[", f);
  for (size_t i = 0; i < count; i++) {
    const canframe_signal_result_t *r = &signals[i];
    if (i) fputc(',', f);
    fputs("{\"name\":", f); string(f, r->definition->name);
    fprintf(f, ",\"raw_hex\":\"0x%016" PRIX64 "\",\"raw_unsigned\":\"%" PRIu64 "\"", r->raw_unsigned, r->raw_unsigned);
    if (r->definition->is_signed) fprintf(f, ",\"raw_signed\":\"%" PRId64 "\"", r->raw_signed);
    fputs(",\"physical\":", f); number(f, r->quality == CANFRAME_SIGNAL_NUMERIC_ERROR ? NAN : r->physical, false);
    fputs(",\"unit\":", f); string(f, r->definition->unit);
    fputs(",\"quality\":", f); string(f, quality(r->quality));
    fprintf(f, ",\"precision_warning\":%s}", r->precision_warning ? "true" : "false");
  }
  fputs("]}\n", f);
}
static void key(FILE *f, const diagnostic_entry_t *e) {
  fputs(",\"interface\":", f); string(f, e->ifname);
  fprintf(f, ",\"id\":\"0x%03" PRIX32 "\",\"extended\":%s,\"time_source\":", e->definition->id, e->definition->is_extended ? "true" : "false");
  string(f, diagnostic_source_name(e->source)); fputs(",\"state\":", f); string(f, diagnostic_state_name(e->state));
}
void output_diagnostic(FILE *f, const diagnostic_event_t *event) {
  static const struct { unsigned flag; const char *name; } names[] = {
    {DIAG_BASELINE,"baseline"},{DIAG_NORMAL,"normal"},{DIAG_EARLY,"early"},{DIAG_LATE,"late"},
    {DIAG_OUT_OF_ORDER,"out_of_order"},{DIAG_STALE,"stale"},{DIAG_MISSING,"missing"},
    {DIAG_RECOVERY,"recovery"},{DIAG_TIMESTAMP_UNAVAILABLE,"timestamp_unavailable"}};
  bool comma = false;
  fputs("{\"type\":\"diagnostic\"", f); key(f, event->entry);
  fprintf(f, ",\"at_ns\":\"%" PRIu64 "\",\"delta_ns\":\"%" PRIu64 "\",\"events\":[", event->at_ns, event->delta_ns);
  for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); i++) if (event->flags & names[i].flag) {
    if (comma) fputc(',', f);
    string(f, names[i].name); comma = true;
  }
  fputs("]}\n", f);
}
void output_diagnostic_summary(FILE *f, const diagnostic_entry_t *e) {
  fputs("{\"type\":\"diagnostic_summary\"", f); key(f, e);
  fprintf(f, ",\"frames\":%" PRIu64 ",\"intervals\":%" PRIu64 ",\"min_ns\":\"%" PRIu64 "\",\"max_ns\":\"%" PRIu64 "\",\"mean_ns\":", e->frames, e->intervals, e->min_ns, e->max_ns);
  if (e->intervals) number(f, (double)e->mean_ns, false); else fputs("null", f);
  fprintf(f, ",\"early_events\":%" PRIu64 ",\"late_events\":%" PRIu64 ",\"out_of_order_events\":%" PRIu64 ",\"stale_events\":%" PRIu64 ",\"missing_events\":%" PRIu64 ",\"recovery_events\":%" PRIu64 "}\n", e->early, e->late, e->out_of_order, e->stale, e->missing, e->recovery);
}
