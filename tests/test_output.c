#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "output.h"

static char *capture_output(void (*fn)(FILE *, const can_frame_t *),
                            const can_frame_t *frame) {
  char *buffer = NULL;
  size_t size = 0;
  FILE *stream = open_memstream(&buffer, &size);

  assert(stream != NULL);
  fn(stream, frame);
  fclose(stream);
  return buffer;
}

void test_output(void) {
  can_frame_t frame = {0};
  char *raw;
  char *json;

  frame.id = 0x123U;
  frame.dlc = 2U;
  frame.data[0] = 0xAAU;
  frame.data[1] = 0x55U;

  raw = capture_output(output_raw, &frame);
  assert(strcmp(raw, "[0x123] [2] AA 55\n") == 0);
  free(raw);

  json = capture_output(output_json, &frame);
  assert(strcmp(json, "{\"id\":\"0x123\",\"extended\":false,\"dlc\":2,\"data\":[170,85]}\n") == 0);
  free(json);
}

