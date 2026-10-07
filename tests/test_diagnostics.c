#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "diagnostics.h"
static can_message_def_t def(void) {
  can_message_def_t d = {0}; d.id = 0x100; strcpy(d.name,"M"); d.expected_dlc = 2;
  d.period_ms = 100; d.tolerance_ms = 10; d.timeout_ms = 500; return d;
}
static can_frame_t frame(uint64_t ms) {
  can_frame_t f = {0}; f.id = 0x100; f.dlc = 2; strcpy(f.ifname,"vcan0");
  f.has_timestamp = true; f.time_source = CANFRAME_TIME_SOURCE; f.timestamp_ns = ms * 1000000; return f;
}
void test_diagnostics(void) {
  can_message_def_t d = def(); diagnostics_t map; diagnostic_event_t event, events[CANFRAME_DIAGNOSTIC_CAPACITY];
  const uint64_t deltas[] = {89,90,110,111}; const unsigned flags[] = {DIAG_EARLY,DIAG_NORMAL,DIAG_NORMAL,DIAG_LATE};
  for (size_t i = 0; i < 4; i++) {
    assert(diagnostics_init(&map,&d,1,NULL,0) == CANFRAME_OK);
    can_frame_t f = frame(1000); assert(diagnostics_observe(&map,&f,&event) == CANFRAME_OK && event.flags == DIAG_BASELINE);
    f = frame(1000 + deltas[i]); assert(diagnostics_observe(&map,&f,&event) == CANFRAME_OK && event.flags == flags[i]);
    assert(event.entry->intervals == 1 && event.entry->min_ns == deltas[i]*1000000 && event.entry->max_ns == deltas[i]*1000000);
    assert(diagnostics_poll(&map, UINT64_MAX, events) == 0); /* EOF has no timeout inference */
  }
  assert(diagnostics_init(&map,&d,1,NULL,0) == CANFRAME_OK);
  can_frame_t f = frame(1000); diagnostics_observe(&map,&f,&event);
  f = frame(900); diagnostics_observe(&map,&f,&event); assert(event.flags == DIAG_OUT_OF_ORDER && event.entry->last_ns == 1000000000);
  f = frame(1000); diagnostics_observe(&map,&f,&event); assert(event.flags == DIAG_EARLY && event.entry->min_ns == 0);
  f = frame(1100); diagnostics_observe(&map,&f,&event); assert(event.flags == DIAG_NORMAL && event.entry->mean_ns == 50000000 && event.entry->frames == 4);
  strcpy(f.ifname,"vcan1"); diagnostics_observe(&map,&f,&event); assert(event.flags == DIAG_BASELINE && map.count == 2);
  assert(diagnostics_init(&map,&d,1,NULL,0) == CANFRAME_OK);
  f = frame(0); f.has_timestamp = false; f.time_source = CANFRAME_TIME_NONE;
  assert(diagnostics_observe(&map,&f,&event) == CANFRAME_OK && event.flags == DIAG_TIMESTAMP_UNAVAILABLE && !event.entry->received);
  f.is_rtr = true; assert(diagnostics_observe(&map,&f,&event) == CANFRAME_ERR_NOT_APPLICABLE); f.is_rtr = false;
  f.dlc = 1; assert(diagnostics_observe(&map,&f,&event) == CANFRAME_ERR_INVALID_DLC);
  assert(diagnostics_init(&map,&d,1,"vcan0",0) == CANFRAME_OK && map.entries[0].state == DIAG_WAITING);
  assert(diagnostics_poll(&map,499000000,events) == 0);
  assert(diagnostics_poll(&map,500000000,events) == 1 && events[0].flags == DIAG_MISSING);
  assert(diagnostics_poll(&map,900000000,events) == 0);
  f = frame(1000); f.time_source = CANFRAME_TIME_USERSPACE;
  diagnostics_observe(&map,&f,&event); assert(event.flags == (DIAG_BASELINE|DIAG_RECOVERY));
  assert(diagnostics_poll(&map,1499000000,events) == 0);
  assert(diagnostics_poll(&map,1500000000,events) == 1 && events[0].flags == DIAG_STALE);
  assert(diagnostics_poll(&map,1900000000,events) == 0);
  f = frame(2000); f.time_source = CANFRAME_TIME_USERSPACE;
  diagnostics_observe(&map,&f,&event); assert(event.flags == (DIAG_LATE|DIAG_RECOVERY));
  f.time_source = CANFRAME_TIME_SOURCE; assert(diagnostics_observe(&map,&f,&event) == CANFRAME_ERR_INVALID_FORMAT);
  assert(diagnostics_init(&map,&d,1,NULL,0) == CANFRAME_OK);
  for (unsigned i = 0; i < CANFRAME_DIAGNOSTIC_CAPACITY; i++) {
    f = frame(i); snprintf(f.ifname,sizeof(f.ifname),"i%u",i); assert(diagnostics_observe(&map,&f,&event) == CANFRAME_OK);
  }
  strcpy(f.ifname,"overflow"); assert(diagnostics_observe(&map,&f,&event) == CANFRAME_ERR_CAPACITY && map.count == CANFRAME_DIAGNOSTIC_CAPACITY);
}
