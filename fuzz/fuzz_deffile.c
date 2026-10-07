#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "deffile.h"
#include "decode.h"
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  can_message_def_t *defs = malloc(CANFRAME_MAX_DEFINITIONS * sizeof(*defs));
  can_message_def_t *before = malloc(CANFRAME_MAX_DEFINITIONS * sizeof(*defs));
  size_t count = 99;
  cfd_error_t error;
  assert(defs && before);
  memset(defs,0x5a,CANFRAME_MAX_DEFINITIONS * sizeof(*defs));
  memcpy(before,defs,CANFRAME_MAX_DEFINITIONS * sizeof(*defs));
  canframe_status_t s = deffile_parse_bytes(data,size,defs,CANFRAME_MAX_DEFINITIONS,&count,&error);
  if (s == CANFRAME_OK) {
    assert(count <= CANFRAME_MAX_DEFINITIONS);
    for (size_t i = 0; i < count; i++) {
      assert(validate_definition(&defs[i]) == CANFRAME_OK);
      can_frame_t frame = {0}; canframe_signal_result_t results[CANFRAME_MAX_SIGNALS]; size_t result_count;
      frame.id = defs[i].id; frame.is_extended = defs[i].is_extended; frame.dlc = defs[i].expected_dlc;
      for (size_t j = 0; j < frame.dlc; j++) frame.data[j] = size ? data[(i + j) % size] : 0;
      assert(decode_signals(&defs[i],&frame,results,CANFRAME_MAX_SIGNALS,&result_count) == CANFRAME_OK);
      assert(result_count == defs[i].signal_count);
    }
  } else { assert(count == 0); assert(!memcmp(defs,before,CANFRAME_MAX_DEFINITIONS * sizeof(*defs))); }
  free(before); free(defs); return 0;
}
