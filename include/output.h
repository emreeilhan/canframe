#ifndef OUTPUT_H
#define OUTPUT_H
#include <stdio.h>
#include "canframe.h"
#include "decode.h"
#include "diagnostics.h"
void output_raw(FILE *stream, const can_frame_t *frame);
void output_json(FILE *stream, const can_frame_t *frame);
void output_signals(FILE *stream, const can_frame_t *frame, bool json, canframe_status_t status,
                    const canframe_signal_result_t *signals, size_t count);
void output_diagnostic(FILE *stream, const diagnostic_event_t *event);
void output_diagnostic_summary(FILE *stream, const diagnostic_entry_t *entry);
#endif
