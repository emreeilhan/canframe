#include <assert.h>
#include <math.h>
#include <string.h>
#include "decode.h"
static can_message_def_t definition(unsigned start, unsigned width, bool big, bool sign, unsigned dlc) {
  can_message_def_t d = {0}; d.id = 0x123; d.expected_dlc = (uint8_t)dlc;
  strcpy(d.name, "M"); d.signal_count = 1;
  strcpy(d.signals[0].name, "s"); strcpy(d.signals[0].unit, "u");
  d.signals[0].start_bit = (uint8_t)start; d.signals[0].bit_length = (uint8_t)width;
  d.signals[0].endian = big ? CANFRAME_ENDIAN_BIG : CANFRAME_ENDIAN_LITTLE;
  d.signals[0].is_signed = sign; d.signals[0].scale = 1;
  return d;
}
static canframe_signal_result_t extract(can_message_def_t *d, can_frame_t *f) {
  canframe_signal_result_t r[16]; size_t count = 99;
  assert(decode_signals(d, f, r, 16, &count) == CANFRAME_OK && count == 1); return r[0];
}
void test_decode(void) {
  can_frame_t f = {0}; f.id = 0x123; f.dlc = 2; f.data[0] = 0x12; f.data[1] = 0x34;
  can_message_def_t d = definition(0,16,true,false,2);
  assert(extract(&d,&f).raw_unsigned == 0x1234);
  d.signals[0].endian = CANFRAME_ENDIAN_LITTLE; assert(extract(&d,&f).raw_unsigned == 0x3412);
  d = definition(3,7,false,false,2); f.data[0] = 0xD6; f.data[1] = 3; assert(extract(&d,&f).raw_unsigned == 122);
  d.signals[0].endian = CANFRAME_ENDIAN_BIG; f.data[0] = 0xB2; f.data[1] = 0x60; assert(extract(&d,&f).raw_unsigned == 73);
  const uint8_t eight[] = {0xff,0x80,0x7f}; const int64_t signed_eight[] = {-1,-128,127};
  d = definition(0,8,true,true,1); f.dlc = 1;
  for (size_t i = 0; i < 3; i++) { f.data[0] = eight[i]; assert(extract(&d,&f).raw_signed == signed_eight[i]); }
  const uint16_t twelve[] = {0xfff,0x800,0x7ff}; const int64_t signed_twelve[] = {-1,-2048,2047};
  d = definition(0,12,true,true,2); f.dlc = 2;
  for (size_t i = 0; i < 3; i++) { f.data[0] = (uint8_t)(twelve[i] >> 4); f.data[1] = (uint8_t)(twelve[i] << 4); assert(extract(&d,&f).raw_signed == signed_twelve[i]); }
  d = definition(0,1,true,true,1); f.dlc = 1; f.data[0] = 0x80; assert(extract(&d,&f).raw_signed == -1);
  d = definition(0,63,true,true,8); f.dlc = 8; memset(f.data, 0xff, 8); assert(extract(&d,&f).raw_signed == -1);
  d = definition(0,64,true,false,8); canframe_signal_result_t r = extract(&d,&f); assert(r.raw_unsigned == UINT64_MAX && r.precision_warning);
  d.signals[0].is_signed = true; memset(f.data,0,8); f.data[0] = 0x80; r = extract(&d,&f); assert(r.raw_signed == INT64_MIN && r.precision_warning);
  d.signals[0].is_signed = false;
  memset(f.data,0,8); f.data[1] = 0x20; assert(!extract(&d,&f).precision_warning); /* exactly 2^53 */
  f.data[7] = 1; assert(extract(&d,&f).precision_warning);
  d = definition(0,16,true,false,2); f.dlc = 2; f.data[0] = 3; f.data[1] = 0x89; d.signals[0].scale = .1;
  assert(fabs(extract(&d,&f).physical - 90.5) < 1e-12);
  d.signals[0].scale = -1; d.signals[0].offset = 2; assert(extract(&d,&f).physical == -903);
  d.signals[0].has_range = true; d.signals[0].range_min = -903; d.signals[0].range_max = 0;
  assert(extract(&d,&f).quality == CANFRAME_SIGNAL_VALID);
  d.signals[0].range_min = -902; assert(extract(&d,&f).quality == CANFRAME_SIGNAL_OUT_OF_RANGE);
  d.signals[0].scale = 1e308; assert(extract(&d,&f).quality == CANFRAME_SIGNAL_NUMERIC_ERROR);
  canframe_signal_result_t out[16], before[16]; size_t count;
  memset(out,0x5a,sizeof(out)); memcpy(before,out,sizeof(out));
  f.dlc = 1; count = 99; assert(decode_signals(&d,&f,out,16,&count) == CANFRAME_ERR_INVALID_DLC && count == 0 && !memcmp(out,before,sizeof(out)));
  f.dlc = 3; assert(decode_signals(&d,&f,out,16,&count) == CANFRAME_ERR_INVALID_DLC && count == 0);
  f.dlc = 2; assert(decode_signals(&d,&f,out,0,&count) == CANFRAME_ERR_CAPACITY && count == 0);
  f.is_rtr = true; assert(decode_signals(&d,&f,out,16,&count) == CANFRAME_ERR_NOT_APPLICABLE && count == 0); f.is_rtr = false;
  f.is_error = true; assert(decode_signals(&d,&f,out,16,&count) == CANFRAME_ERR_NOT_APPLICABLE); f.is_error = false;
  f.is_extended = true; assert(decode_signals(&d,&f,out,16,&count) == CANFRAME_ERR_NO_DEFINITION); f.is_extended = false;
  d.signals[0].scale = NAN; assert(decode_signals(&d,&f,out,16,&count) == CANFRAME_ERR_INVALID_FORMAT && count == 0);
  d.signals[0].scale = 1; d.signals[0].bit_length = 0; assert(validate_definition(&d) == CANFRAME_ERR_BOUNDS);
  d.signals[0].bit_length = 16; d.signal_count = 17; assert(validate_definition(&d) == CANFRAME_ERR_CAPACITY);
  assert(!memcmp(out,before,sizeof(out)));
}
