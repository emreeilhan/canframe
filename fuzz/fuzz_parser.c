#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "parser.h"
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  can_frame_t frame, before;
  memset(&frame, 0x5a, sizeof(frame)); memcpy(&before,&frame,sizeof(frame));
  canframe_status_t s = parse_bytes(data,size,&frame);
  if (s == CANFRAME_OK) {
    assert(frame.id <= (frame.is_extended ? 0x1fffffffU : 0x7ffU));
    assert(frame.dlc <= 8 && memchr(frame.ifname,0,sizeof(frame.ifname)));
    assert(!frame.has_timestamp || frame.time_source == CANFRAME_TIME_SOURCE);
  } else assert(!memcmp(&frame,&before,sizeof(frame)));
  return 0;
}
