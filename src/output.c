#include "output.h"

#include <inttypes.h>

void output_raw(FILE *stream, const can_frame_t *frame) {
  uint8_t i;

  fprintf(stream, "[0x%03" PRIX32 "] [%u]", frame->id, frame->dlc);
  if (frame->is_extended) {
    fprintf(stream, " [ext]");
  }
  if (frame->is_rtr) {
    fprintf(stream, " RTR");
  }
  for (i = 0; i < frame->dlc; ++i) {
    fprintf(stream, " %02X", frame->data[i]);
  }
  fputc('\n', stream);
}

void output_json(FILE *stream, const can_frame_t *frame) {
  uint8_t i;

  fprintf(stream, "{\"id\":\"0x%03" PRIX32 "\",\"extended\":%s,\"dlc\":%u,\"data\":[",
          frame->id, frame->is_extended ? "true" : "false", frame->dlc);
  for (i = 0; i < frame->dlc; ++i) {
    fprintf(stream, "%s%u", i == 0 ? "" : ",", frame->data[i]);
  }
  fprintf(stream, "]}\n");
}

