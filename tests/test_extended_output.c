#define _GNU_SOURCE
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "output.h"
#include "socketcan.h"
void test_extended_output(void) {
  can_frame_t f = {0}; f.id = 1; strcpy(f.ifname,"x\"\\\001"); f.has_timestamp = true; f.timestamp = NAN;
  char *s = NULL; size_t n = 0; FILE *stream = open_memstream(&s,&n); assert(stream);
  output_json(stream,&f); fclose(stream);
  assert(strstr(s,"\"timestamp\":null") && strstr(s,"x\\\"\\\\\\u0001")); free(s);
  can_signal_t signal = {0}; strcpy(signal.name,"s"); strcpy(signal.unit,"u"); signal.is_signed = true;
  canframe_signal_result_t r = {0}; r.definition = &signal; r.raw_unsigned = UINT64_C(9223372036854775808); r.raw_signed = INT64_MIN;
  r.quality = CANFRAME_SIGNAL_NUMERIC_ERROR; r.precision_warning = true;
  s = NULL; n = 0; stream = open_memstream(&s,&n); assert(stream);
  output_signals(stream,&f,true,CANFRAME_OK,&r,1); fclose(stream);
  assert(strstr(s,"\"raw_signed\":\"-9223372036854775808\"") && strstr(s,"\"physical\":null") && strstr(s,"numeric_error")); free(s);
  if (!socketcan_supported()) {
    char reason[100]; assert(socketcan_open("vcan0",reason,sizeof(reason)) == -1 && strstr(reason,"only on Linux"));
    assert(socketcan_receive(-1,"vcan0",0,&f,reason,sizeof(reason)) == -1); socketcan_close(-1);
  }
}
