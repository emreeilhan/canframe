#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "deffile.h"
#include "decode.h"
static void bad(const char *text, canframe_status_t expected) {
  can_message_def_t defs[CANFRAME_MAX_DEFINITIONS], before[CANFRAME_MAX_DEFINITIONS];
  cfd_error_t error;
  size_t count = 17;
  memset(defs, 0x5A, sizeof(defs)); memcpy(before, defs, sizeof(defs));
  assert(deffile_parse_bytes((const uint8_t *)text, strlen(text), defs, CANFRAME_MAX_DEFINITIONS, &count, &error) == expected);
  assert(count == 0 && error.status == expected && error.reason[0]);
  assert(!memcmp(defs, before, sizeof(defs)));
}
void test_deffile(void) {
  const char *valid = "# comment\nCFD 1\nMESSAGE 0x100 STANDARD 2 RPM\nPERIOD 100 10\nTIMEOUT 500\nSIGNAL rpm 0 16 BIG UNSIGNED 1 0 rpm\nEND\nMESSAGE 0x100 EXTENDED 2 Temp\nSIGNAL temp 0 16 LITTLE SIGNED -0.1 2 degC\nRANGE temp -40 150\nEND\n";
  can_message_def_t defs[CANFRAME_MAX_DEFINITIONS]; cfd_error_t error; size_t count;
  assert(deffile_parse_bytes((const uint8_t *)valid, strlen(valid), defs, 64, &count, &error) == CANFRAME_OK);
  assert(count == 2 && defs[0].period_ms == 100 && defs[1].signals[0].has_range);
  assert(find_definition(defs, count, 0x100, true) == &defs[1]);
  assert(find_definition(defs, count, 0x100, false) == &defs[0]);
  assert(!find_definition(defs, count, 0x101, false));
  bad("CFD 2\n", CANFRAME_ERR_INVALID_FORMAT);
  bad("", CANFRAME_ERR_INVALID_FORMAT);
  bad("CFD 1\nMESSAGE 0x800 STANDARD 0 X\nEND\n", CANFRAME_ERR_INVALID_ID);
  bad("CFD 1\nMESSAGE 0x100000123 EXTENDED 0 X\nEND\n", CANFRAME_ERR_INVALID_ID);
  bad("CFD 1\nMESSAGE 0x1 STANDARD 9 X\nEND\n", CANFRAME_ERR_INVALID_DLC);
  bad("CFD 1\nMESSAGE 0x1 STANDARD 0 X\nEND\nMESSAGE 0x1 STANDARD 0 Y\nEND\n", CANFRAME_ERR_INVALID_FORMAT);
  bad("CFD 1\nMESSAGE 0x1 STANDARD 0 X\n", CANFRAME_ERR_INVALID_FORMAT);
  bad("CFD 1\nSIGNAL x 0 1 BIG SIGNED 1 0 u\n", CANFRAME_ERR_INVALID_FORMAT);
  bad("CFD 1\nMESSAGE 0x1 STANDARD 1 X\nSIGNAL x 0 0 BIG SIGNED 1 0 u\nEND\n", CANFRAME_ERR_BOUNDS);
  bad("CFD 1\nMESSAGE 0x1 STANDARD 1 X\nSIGNAL x 7 2 BIG SIGNED 1 0 u\nEND\n", CANFRAME_ERR_BOUNDS);
  bad("CFD 1\nMESSAGE 0x1 STANDARD 1 X\nSIGNAL x 0 1 BIG SIGNED nan 0 u\nEND\n", CANFRAME_ERR_INVALID_FORMAT);
  bad("CFD 1\nMESSAGE 0x1 STANDARD 1 X\nSIGNAL x 0 1 BIG SIGNED 1 1e999 u\nEND\n", CANFRAME_ERR_INVALID_FORMAT);
  bad("CFD 1\nMESSAGE 0x1 STANDARD 1 X\nSIGNAL x 0 1 BIG SIGNED 1 0 u\nSIGNAL x 1 1 BIG SIGNED 1 0 u\nEND\n", CANFRAME_ERR_INVALID_FORMAT);
  /* BIG bit0 and LITTLE bit7 refer to the same physical payload bit. */
  bad("CFD 1\nMESSAGE 0x1 STANDARD 1 X\nSIGNAL x 0 1 BIG SIGNED 1 0 u\nSIGNAL y 7 1 LITTLE SIGNED 1 0 u\nEND\n", CANFRAME_ERR_BOUNDS);
  bad("CFD 1\nMESSAGE 0x1 STANDARD 0 X\nPERIOD 100 100\nEND\n", CANFRAME_ERR_BOUNDS);
  bad("CFD 1\nMESSAGE 0x1 STANDARD 0 X\nPERIOD 100 10\nTIMEOUT 110\nEND\n", CANFRAME_ERR_BOUNDS);
  bad("CFD 1\nMESSAGE 0x1 STANDARD 0 X\nTIMEOUT 0\nEND\n", CANFRAME_ERR_INVALID_FORMAT);
  bad("CFD 1\nMESSAGE 0x1 STANDARD 0 X\nTIMEOUT 4294967296\nEND\n", CANFRAME_ERR_INVALID_FORMAT);
  bad("CFD 1\nMESSAGE 0x1 STANDARD 0 X\nBOGUS\nEND\n", CANFRAME_ERR_INVALID_FORMAT);
  bad("CFD 1\nMESSAGE 0x1 STANDARD 0 X\nRANGE x 0 1\nEND\n", CANFRAME_ERR_INVALID_FORMAT);
  bad("CFD 1\nMESSAGE 0x1 STANDARD 1 X\nSIGNAL x 0 1 BIG SIGNED 1 0 u\nRANGE x 2 1\nEND\n", CANFRAME_ERR_BOUNDS);
  bad("CFD 1\nMESSAGE 0x1 STANDARD 1 X\nSIGNAL x 0 1 BIG SIGNED 1 0 u\nRANGE x 0 1\nRANGE x 0 1\nEND\n", CANFRAME_ERR_INVALID_FORMAT);
  bad("CFD 1\nMESSAGE 0x1 STANDARD 0 name_with_a_space\nEND junk\n", CANFRAME_ERR_INVALID_FORMAT);
  bad("CFD 1\nMESSAGE 0x1 STANDARD 0 abcdefghijklmnopqrstuvwxyz0123456789\nEND\n", CANFRAME_ERR_INVALID_FORMAT);
  char many[8192]; size_t pos = (size_t)snprintf(many, sizeof(many), "CFD 1\n");
  for (size_t i = 0; i < 65; i++) pos += (size_t)snprintf(many + pos, sizeof(many) - pos, "MESSAGE 0x%zx STANDARD 0 M\nEND\n", i);
  bad(many, CANFRAME_ERR_CAPACITY);
  pos = (size_t)snprintf(many, sizeof(many), "CFD 1\nMESSAGE 0x1 STANDARD 8 X\n");
  for (size_t i = 0; i < 17; i++) pos += (size_t)snprintf(many + pos, sizeof(many) - pos, "SIGNAL s%zu %zu 1 LITTLE UNSIGNED 1 0 u\n", i, i);
  snprintf(many + pos, sizeof(many) - pos, "END\n"); bad(many, CANFRAME_ERR_CAPACITY);
  const uint8_t nul[] = {'C','F','D',' ','1',0}; count = 99;
  assert(deffile_parse_bytes(nul, sizeof(nul), defs, 64, &count, &error) == CANFRAME_ERR_INVALID_FORMAT && count == 0);
  uint8_t *huge = calloc(CFD_MAX_BYTES + 1, 1); assert(huge);
  assert(deffile_parse_bytes(huge, CFD_MAX_BYTES + 1, defs, 64, &count, &error) == CANFRAME_ERR_BOUNDS && count == 0); free(huge);
  char long_line[CFD_MAX_LINE + 2]; memset(long_line, '#', sizeof(long_line)); long_line[sizeof(long_line)-1] = 0; bad(long_line, CANFRAME_ERR_BOUNDS);
  assert(deffile_parse_bytes((const uint8_t *)valid, strlen(valid), defs, 1, &count, &error) == CANFRAME_ERR_CAPACITY && count == 0);
  assert(deffile_load("samples/demo.cfd", defs, 64, &count) == CANFRAME_OK && count == 3);
  assert(deffile_load_ex("/file/does/not/exist", defs, 64, &count, &error) == CANFRAME_ERR_IO && count == 0);
}
