#include <assert.h>
#include <math.h>
#include <string.h>
#include "parser.h"
#include "numbers.h"
void test_hardening(void) {
  can_frame_t before, frame;
  memset(&before, 0xA5, sizeof(before)); frame = before;
  char long_frame[520]; strcpy(long_frame, "123#"); memset(long_frame + 4, 'A', 512); long_frame[516] = 0;
  assert(parse_line(long_frame, &frame) == CANFRAME_ERR_INVALID_DLC);
  assert(!memcmp(&frame, &before, sizeof(frame)));
  const char *bad[] = {"100000123#AA", "+123#AA", "-1#AA", "20000000#AA", "FFFFFFFFFFFFFFFF#AA", "123#A", "123#GG", "123#R8", "123##11", "can0 123 [1] AA BB", "can0 123 [1] AABB", "can0 123 [-1]", "can0 123 [+1] AA", "(nan) can0 123#AA", "(inf) can0 123#AA", "(-1.0) can0 123#AA", "(1e3) can0 123#AA", "(1.1234567890) can0 123#AA", "(18446744073.709551616) can0 123#AA", "(1.0)can0 123#AA", "(1.0) can0 123#AA junk", "can0 123[1] AA"};
  for (size_t i = 0; i < sizeof(bad)/sizeof(bad[0]); i++) {
    frame = before; assert(parse_line(bad[i], &frame) != CANFRAME_OK); assert(!memcmp(&frame, &before, sizeof(frame)));
  }
  assert(parse_line(NULL, &frame) == CANFRAME_ERR_INVALID_FORMAT);
  assert(parse_line("123#AA", NULL) == CANFRAME_ERR_INVALID_FORMAT);
  const uint8_t nul[] = {'1','2','3','#','A','A',0,'X'};
  assert(parse_bytes(nul, sizeof(nul), &frame) == CANFRAME_ERR_INVALID_FORMAT);
  uint8_t huge[CANFRAME_MAX_LINE + 1] = {0};
  assert(parse_bytes(huge, sizeof(huge), &frame) == CANFRAME_ERR_BOUNDS);
  assert(parse_line("(18446744073.709551615) can0 123#", &frame) == CANFRAME_OK);
  assert(frame.timestamp_ns == UINT64_MAX && isfinite(frame.timestamp));
  assert(parse_line("(0.000000001) can0 123#AA", &frame) == CANFRAME_OK && frame.timestamp_ns == 1);
  assert(parse_line("00000123#AA", &frame) == CANFRAME_OK && frame.is_extended);
  assert(parse_line("can0 00000123 [1] AA", &frame) == CANFRAME_OK && frame.is_extended);
  assert(parse_line("123#", &frame) == CANFRAME_OK && frame.dlc == 0);
  uint64_t n;
  double value;
  assert(number_unsigned("0", 1, 10, 0, &n) && n == 0);
  assert(!number_unsigned("1", 1, 10, 0, &n));
  assert(!number_unsigned("1", 1, 2, 5, &n));
  assert(!number_unsigned("0x", 2, 16, UINT64_MAX, &n));
  assert(number_double("-1.25e+2", &value) && value == -125);
  assert(number_double(".5", &value) && value == 0.5);
  assert(!number_double("1e999", &value) && !number_double("1e-999", &value));
  assert(!number_double("nan", &value) && !number_double("0x1p2", &value) && !number_double("1junk", &value));
}
