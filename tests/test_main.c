#include <stdio.h>
#include <stdlib.h>
void test_parser_compact(void); void test_parser_candump(void); void test_output(void); void test_filter(void);
void test_hardening(void); void test_deffile(void); void test_decode(void); void test_diagnostics(void); void test_extended_output(void);
#define RUN(fn) do { fn(); puts(#fn ": ok"); } while (0)
int main(void) {
  RUN(test_parser_compact); RUN(test_parser_candump); RUN(test_output); RUN(test_filter);
  RUN(test_hardening); RUN(test_deffile); RUN(test_decode); RUN(test_diagnostics); RUN(test_extended_output);
  puts("ok: 9 C test groups"); return EXIT_SUCCESS;
}
