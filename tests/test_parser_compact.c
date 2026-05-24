#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "parser.h"

void test_parser_compact(void) {
  can_frame_t frame;
  canframe_status_t status;

  status = parse_compact_frame("123#1122334455667788", &frame);
  assert(status == CANFRAME_OK);
  assert(frame.id == 0x123U);
  assert(frame.dlc == 8U);
  assert(frame.data[0] == 0x11U);
  assert(frame.data[7] == 0x88U);
  assert(frame.is_extended == false);
  assert(frame.is_rtr == false);

  status = parse_compact_frame("18DAF110#0322010000000000", &frame);
  assert(status == CANFRAME_OK);
  assert(frame.id == 0x18DAF110U);
  assert(frame.is_extended == true);

  status = parse_compact_frame("123#R", &frame);
  assert(status == CANFRAME_OK);
  assert(frame.is_rtr == true);
  assert(frame.dlc == 0U);

  status = parse_compact_frame("XYZ#11", &frame);
  assert(status == CANFRAME_ERR_INVALID_ID);

  status = parse_compact_frame("123#001122334455667788", &frame);
  assert(status == CANFRAME_ERR_INVALID_DLC);

  status = parse_compact_frame("1230011", &frame);
  assert(status == CANFRAME_ERR_INVALID_FORMAT);

  status = parse_line("  123#AA", &frame);
  assert(status == CANFRAME_OK);
  assert(frame.data[0] == 0xAAU);
  assert(strcmp(canframe_status_string(CANFRAME_ERR_UNSUPPORTED),
                "format not supported yet") == 0);
}

