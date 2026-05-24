#include <assert.h>

#include "parser.h"

void test_parser_candump(void) {
  can_frame_t frame;
  canframe_status_t status;

  status = parse_candump_line("(1700000000.123456) can0 123#11223344", &frame);
  assert(status == CANFRAME_ERR_UNSUPPORTED);

  status = parse_line("can0  123  [8]  11 22 33 44 55 66 77 88", &frame);
  assert(status == CANFRAME_ERR_UNSUPPORTED);
}

