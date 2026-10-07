#include "diagnostics.h"
#include "decode.h"
#include <string.h>
static void increment(uint64_t *n) { if (*n != UINT64_MAX) (*n)++; }
static diagnostic_entry_t *add(diagnostics_t *map, const can_message_def_t *d, const char *iface,
                               canframe_time_source_t source, uint64_t start) {
  diagnostic_entry_t *e;
  if (map->count == CANFRAME_DIAGNOSTIC_CAPACITY) return NULL;
  e = &map->entries[map->count++]; memset(e, 0, sizeof(*e));
  e->definition = d; strcpy(e->ifname, iface); e->source = source; e->start_ns = start;
  e->state = DIAG_WAITING;
  return e;
}
canframe_status_t diagnostics_init(diagnostics_t *map, const can_message_def_t *defs, size_t count,
                                  const char *live_interface, uint64_t start_ns) {
  if (!map || !defs || count > CANFRAME_MAX_DEFINITIONS ||
      (live_interface && strlen(live_interface) >= CANFRAME_IFNAME_MAX)) return CANFRAME_ERR_INVALID_FORMAT;
  for (size_t i = 0; i < count; i++) {
    canframe_status_t s = validate_definition(&defs[i]);
    if (s != CANFRAME_OK) return s;
  }
  memset(map, 0, sizeof(*map)); map->definitions = defs; map->definition_count = count;
  if (live_interface) for (size_t i = 0; i < count; i++) {
    if (defs[i].period_ms || defs[i].timeout_ms) add(map, &defs[i], live_interface, CANFRAME_TIME_USERSPACE, start_ns);
  }
  return CANFRAME_OK;
}
canframe_status_t diagnostics_observe(diagnostics_t *map, const can_frame_t *frame, diagnostic_event_t *event) {
  const can_message_def_t *d;
  diagnostic_entry_t *e = NULL;
  uint64_t delta;
  if (!map || !frame || !event || !memchr(frame->ifname, 0, sizeof(frame->ifname))) return CANFRAME_ERR_INVALID_FORMAT;
  memset(event, 0, sizeof(*event));
  d = find_definition(map->definitions, map->definition_count, frame->id, frame->is_extended);
  if (!d || (!d->period_ms && !d->timeout_ms)) return CANFRAME_ERR_NO_DEFINITION;
  if (frame->is_rtr || frame->is_error) return CANFRAME_ERR_NOT_APPLICABLE;
  if (frame->dlc != d->expected_dlc) return CANFRAME_ERR_INVALID_DLC;
  for (size_t i = 0; i < map->count; i++) {
    if (map->entries[i].definition == d && !strcmp(map->entries[i].ifname, frame->ifname)) e = &map->entries[i];
  }
  if (!e) e = add(map, d, frame->ifname, frame->time_source, frame->timestamp_ns);
  if (!e) return CANFRAME_ERR_CAPACITY;
  increment(&e->frames);
  event->entry = e; event->at_ns = frame->timestamp_ns;
  if (!frame->has_timestamp || frame->time_source == CANFRAME_TIME_NONE) {
    event->flags = DIAG_TIMESTAMP_UNAVAILABLE; return CANFRAME_OK;
  }
  if (e->source == CANFRAME_TIME_NONE) e->source = frame->time_source;
  if (e->source != frame->time_source) return CANFRAME_ERR_INVALID_FORMAT;
  if (e->received && frame->timestamp_ns < e->last_ns) {
    increment(&e->out_of_order); event->flags = DIAG_OUT_OF_ORDER; return CANFRAME_OK;
  }
  if (e->state == DIAG_TIMED_OUT) { increment(&e->recovery); event->flags |= DIAG_RECOVERY; }
  if (!e->received) event->flags |= DIAG_BASELINE;
  else {
    delta = frame->timestamp_ns - e->last_ns; event->delta_ns = delta;
    if (!e->intervals || delta < e->min_ns) e->min_ns = delta;
    if (!e->intervals || delta > e->max_ns) e->max_ns = delta;
    increment(&e->intervals);
    e->mean_ns += ((long double)delta - e->mean_ns) / (long double)e->intervals;
    if (d->period_ms && delta < (uint64_t)(d->period_ms - d->tolerance_ms) * 1000000) {
      increment(&e->early); event->flags |= DIAG_EARLY;
    } else if (d->period_ms && delta > ((uint64_t)d->period_ms + d->tolerance_ms) * 1000000) {
      increment(&e->late); event->flags |= DIAG_LATE;
    } else event->flags |= DIAG_NORMAL;
  }
  e->received = true; e->state = DIAG_FRESH; e->last_ns = frame->timestamp_ns;
  return CANFRAME_OK;
}
size_t diagnostics_poll(diagnostics_t *map, uint64_t now, diagnostic_event_t events[CANFRAME_DIAGNOSTIC_CAPACITY]) {
  size_t count = 0;
  if (!map || !events) return 0;
  for (size_t i = 0; i < map->count; i++) {
    diagnostic_entry_t *e = &map->entries[i];
    uint64_t base = e->received ? e->last_ns : e->start_ns;
    if (e->source != CANFRAME_TIME_USERSPACE || !e->definition->timeout_ms || e->state == DIAG_TIMED_OUT || now < base ||
        now - base < (uint64_t)e->definition->timeout_ms * 1000000) continue;
    e->state = DIAG_TIMED_OUT;
    if (e->received) increment(&e->stale); else increment(&e->missing);
    events[count++] = (diagnostic_event_t){e, e->received ? DIAG_STALE : DIAG_MISSING, now, 0};
  }
  return count;
}
const char *diagnostic_source_name(canframe_time_source_t source) {
  return source == CANFRAME_TIME_USERSPACE ? "userspace_observed" : source == CANFRAME_TIME_SOURCE ? "source_timestamp" : "timestamp_unavailable";
}
const char *diagnostic_state_name(diagnostic_state_t state) {
  return state == DIAG_FRESH ? "fresh" : state == DIAG_TIMED_OUT ? "timed_out" : "waiting";
}
