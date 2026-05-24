#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdio.h>

#include "canframe.h"

void output_raw(FILE *stream, const can_frame_t *frame);
void output_json(FILE *stream, const can_frame_t *frame);

#endif

