#ifndef PARSER_H
#define PARSER_H
#include "canframe.h"
canframe_status_t parse_compact_frame(const char *input, can_frame_t *out);
canframe_status_t parse_candump_line(const char *input, can_frame_t *out);
canframe_status_t parse_line(const char *input, can_frame_t *out);
canframe_status_t parse_bytes(const uint8_t *input, size_t size, can_frame_t *out);
const char *canframe_status_string(canframe_status_t status);
#endif
