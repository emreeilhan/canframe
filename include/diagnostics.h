#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H
#include "canframe.h"
#define CANFRAME_DIAGNOSTIC_CAPACITY 128
enum { DIAG_BASELINE=1, DIAG_NORMAL=2, DIAG_EARLY=4, DIAG_LATE=8,
       DIAG_OUT_OF_ORDER=16, DIAG_STALE=32, DIAG_MISSING=64, DIAG_RECOVERY=128,
       DIAG_TIMESTAMP_UNAVAILABLE=256 };
typedef enum { DIAG_WAITING, DIAG_FRESH, DIAG_TIMED_OUT } diagnostic_state_t;
typedef struct {
  const can_message_def_t *definition;
  char ifname[CANFRAME_IFNAME_MAX];
  canframe_time_source_t source;
  diagnostic_state_t state;
  bool received;
  uint64_t start_ns, last_ns, frames, intervals, min_ns, max_ns;
  long double mean_ns;
  uint64_t early, late, out_of_order, stale, missing, recovery;
} diagnostic_entry_t;
typedef struct { diagnostic_entry_t *entry; unsigned flags; uint64_t at_ns, delta_ns; } diagnostic_event_t;
typedef struct {
  const can_message_def_t *definitions;
  size_t definition_count, count;
  diagnostic_entry_t entries[CANFRAME_DIAGNOSTIC_CAPACITY];
} diagnostics_t;
canframe_status_t diagnostics_init(diagnostics_t *map, const can_message_def_t *defs, size_t count,
                                  const char *live_interface, uint64_t start_ns);
canframe_status_t diagnostics_observe(diagnostics_t *map, const can_frame_t *frame, diagnostic_event_t *event);
size_t diagnostics_poll(diagnostics_t *map, uint64_t now_ns, diagnostic_event_t events[CANFRAME_DIAGNOSTIC_CAPACITY]);
const char *diagnostic_source_name(canframe_time_source_t source);
const char *diagnostic_state_name(diagnostic_state_t state);
#endif
