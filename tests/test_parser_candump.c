#include <assert.h>
#include <string.h>

#include "parser.h"

void test_parser_candump(void) {
  can_frame_t frame;
  canframe_status_t status;

  /* timestamped: (ts) iface compact_frame */
  status = parse_candump_line("(1700000000.123456) can0 123#11223344", &frame);
  assert(status == CANFRAME_OK);
  assert(frame.id == 0x123U);
  assert(frame.dlc == 4U);
  assert(frame.data[0] == 0x11U);
  assert(frame.data[3] == 0x44U);
  assert(frame.has_timestamp == true);
  assert(frame.timestamp > 0.0);
  assert(strcmp(frame.ifname, "can0") == 0);
  assert(frame.is_extended == false);

  /* timestamped via parse_line */
  status = parse_line("  (1700000000.123456) vcan0 18DAF110#0102030405060708", &frame);
  assert(status == CANFRAME_OK);
  assert(frame.id == 0x18DAF110U);
  assert(frame.is_extended == true);
  assert(frame.dlc == 8U);
  assert(strcmp(frame.ifname, "vcan0") == 0);

  /* timestamped: RTR inside */
  status = parse_candump_line("(1700000000.000001) can0 123#R", &frame);
  assert(status == CANFRAME_OK);
  assert(frame.is_rtr == true);

  /* timestamped: malformed timestamp */
  status = parse_candump_line("(notanumber) can0 123#11", &frame);
  assert(status != CANFRAME_OK);

  /* timestamped: missing closing paren */
  status = parse_candump_line("(1700000000.123456 can0 123#11", &frame);
  assert(status != CANFRAME_OK);

  /* timestamped: bad frame after interface */
  status = parse_candump_line("(1700000000.123456) can0 ZZZZ#11", &frame);
  assert(status != CANFRAME_OK);

  /* verbose: can0  id  [dlc]  bytes */
  status = parse_line("can0  123   [8]  11 22 33 44 55 66 77 88", &frame);
  assert(status == CANFRAME_OK);
  assert(frame.id == 0x123U);
  assert(frame.dlc == 8U);
  assert(frame.data[0] == 0x11U);
  assert(frame.data[7] == 0x88U);
  assert(frame.is_extended == false);
  assert(strcmp(frame.ifname, "can0") == 0);

  /* verbose: extended ID */
  status = parse_line("vcan0  18DAF110  [3]  01 02 03", &frame);
  assert(status == CANFRAME_OK);
  assert(frame.id == 0x18DAF110U);
  assert(frame.is_extended == true);
  assert(frame.dlc == 3U);
  assert(frame.data[2] == 0x03U);

  /* verbose: zero DLC */
  status = parse_line("can0  7FF  [0]", &frame);
  assert(status == CANFRAME_OK);
  assert(frame.id == 0x7FFU);
  assert(frame.dlc == 0U);

  /* verbose: DLC mismatch — declares 4 bytes but provides 3 */
  status = parse_line("can0  123  [4]  11 22 33", &frame);
  assert(status != CANFRAME_OK);

  /* verbose: invalid hex byte */
  status = parse_line("can0  123  [2]  ZZ 00", &frame);
  assert(status != CANFRAME_OK);

  /* verbose: DLC over maximum */
  status = parse_line("can0  123  [9]  11 22 33 44 55 66 77 88 99", &frame);
  assert(status != CANFRAME_OK);
}
