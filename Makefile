CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude
LDFLAGS ?=

BIN := canframe
TEST_BIN := test_runner

SRC := src/main.c src/cli.c src/parser.c src/output.c src/decode.c src/deffile.c
OBJ := $(SRC:.c=.o)

TEST_SRC := tests/test_main.c tests/test_parser_compact.c tests/test_parser_candump.c tests/test_output.c tests/test_filter.c
TEST_OBJ := $(TEST_SRC:.c=.o)
TEST_FILTERED_OBJ := $(filter-out src/main.o,$(OBJ))

.PHONY: all clean test

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(TEST_BIN): $(TEST_FILTERED_OBJ) $(TEST_OBJ)
	$(CC) $(TEST_FILTERED_OBJ) $(TEST_OBJ) -o $@ $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(OBJ) $(TEST_OBJ) $(BIN) $(TEST_BIN)
