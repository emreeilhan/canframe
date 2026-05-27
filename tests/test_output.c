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

  /* plain frame: no timestamp, no interface */
  json = capture_output(output_json, &frame);
  assert(strcmp(json, "{\"id\":\"0x123\",\"extended\":false,\"dlc\":2,\"data\":[170,85]}\n") == 0);
  free(json);

  /* timestamped frame: both timestamp and interface fields present */
  {
    can_frame_t ts = {0};
    ts.id = 0x456U;
    ts.dlc = 2U;
    ts.data[0] = 0xCAU;
    ts.data[1] = 0xFEU;
    ts.has_timestamp = true;
    ts.timestamp = 1700000001.0;
    memcpy(ts.ifname, "can0", 5);

    json = capture_output(output_json, &ts);
    assert(strcmp(json, "{\"timestamp\":1700000001.000000,\"interface\":\"can0\","
                        "\"id\":\"0x456\",\"extended\":false,\"dlc\":2,\"data\":[202,254]}\n") == 0);
    free(json);
  }

  /* frame with interface but no timestamp (verbose format) */
  {
    can_frame_t vb = {0};
    vb.id = 0x7DFU;
    vb.dlc = 1U;
    vb.data[0] = 0x02U;
    memcpy(vb.ifname, "vcan0", 6);

    json = capture_output(output_json, &vb);
    assert(strcmp(json, "{\"interface\":\"vcan0\","
                        "\"id\":\"0x7DF\",\"extended\":false,\"dlc\":1,\"data\":[2]}\n") == 0);
    free(json);
  }
}

