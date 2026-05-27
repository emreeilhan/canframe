#include <stdio.h>
#include <stdlib.h>

void test_parser_compact(void);
void test_parser_candump(void);
void test_output(void);
void test_filter(void);

int main(void) {
  test_parser_compact();
  test_parser_candump();
  test_output();
  test_filter();
  puts("ok");
  return EXIT_SUCCESS;
}

