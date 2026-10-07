#ifndef DECODE_H
#define DECODE_H
#include "canframe.h"
typedef enum { CANFRAME_SIGNAL_VALID, CANFRAME_SIGNAL_OUT_OF_RANGE, CANFRAME_SIGNAL_NUMERIC_ERROR } canframe_signal_quality_t;
typedef struct {
  const can_signal_t *definition;
  uint64_t raw_unsigned; /* exact extracted bits, including signed two's-complement */
  int64_t raw_signed;
  double physical;
  bool precision_warning;
  canframe_signal_quality_t quality;
} canframe_signal_result_t;
canframe_status_t validate_definition(const can_message_def_t *definition);
canframe_status_t decode_signals(const can_message_def_t *definition, const can_frame_t *frame,
                                canframe_signal_result_t *results, size_t capacity, size_t *count);
const can_message_def_t *find_definition(const can_message_def_t *definitions, size_t count,
                                        uint32_t id, bool extended);
#endif
