#ifndef DECODE_H
#define DECODE_H

#include "canframe.h"

canframe_status_t decode_signals(const can_message_def_t *definition,
                                 const can_frame_t *frame);

#endif

