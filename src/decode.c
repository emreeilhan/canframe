#include "decode.h"

canframe_status_t decode_signals(const can_message_def_t *definition,
                                 const can_frame_t *frame) {
  (void)definition;
  (void)frame;
  return CANFRAME_ERR_UNSUPPORTED;
}

