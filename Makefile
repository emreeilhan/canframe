CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic
CPPFLAGS ?= -Iinclude
LDFLAGS ?=
PYTHON ?= python3
FUZZ_CC ?= clang
COVERAGE_CC ?= clang
LLVM_PROFDATA ?= llvm-profdata
LLVM_COV ?= llvm-cov

BIN := canframe
TEST_BIN := test_runner
LIB_SRC := src/cli.c src/parser.c src/output.c src/decode.c src/deffile.c src/numbers.c src/diagnostics.c src/socketcan.c
SRC := src/main.c $(LIB_SRC)
OBJ := $(SRC:.c=.o)
TEST_SRC := tests/test_main.c tests/test_parser_compact.c tests/test_parser_candump.c tests/test_output.c tests/test_filter.c tests/test_hardening.c tests/test_deffile.c tests/test_decode.c tests/test_diagnostics.c tests/test_extended_output.c
TEST_OBJ := $(TEST_SRC:.c=.o)
TEST_LIB_OBJ := $(LIB_SRC:.c=.o)
FUZZ_SRC := src/parser.c src/deffile.c src/decode.c src/numbers.c
SAN_FLAGS := -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer
COV_FLAGS := -g -O0 -fprofile-instr-generate -fcoverage-mapping
# Source coverage includes all portable application modules except main.
COV_SRC := $(filter-out src/socketcan.c,$(LIB_SRC))
ifeq ($(shell uname -s),Linux)
COV_SRC += src/socketcan.c
LEAK_CHECK := 1
else
LEAK_CHECK := 0
endif

.PHONY: all clean test unit cli-test sanitize fuzz fuzz-smoke coverage integration
all: $(BIN)
%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)
$(TEST_BIN): $(TEST_LIB_OBJ) $(TEST_OBJ)
	$(CC) $(TEST_LIB_OBJ) $(TEST_OBJ) -o $@ $(LDFLAGS)
unit: $(TEST_BIN)
	./$(TEST_BIN)
cli-test: $(BIN)
	$(PYTHON) tests/test_cli.py
test: unit cli-test
sanitize:
	mkdir -p build/sanitize
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SAN_FLAGS) $(LIB_SRC) $(TEST_SRC) -o build/sanitize/test_runner $(LDFLAGS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SAN_FLAGS) $(SRC) -o build/sanitize/canframe $(LDFLAGS)
	ASAN_OPTIONS=detect_leaks=$(LEAK_CHECK) UBSAN_OPTIONS=halt_on_error=1 ./build/sanitize/test_runner
	ASAN_OPTIONS=detect_leaks=$(LEAK_CHECK) UBSAN_OPTIONS=halt_on_error=1 CANFRAME_BIN=./build/sanitize/canframe $(PYTHON) tests/test_cli.py
fuzz:
	mkdir -p build/fuzz
	$(FUZZ_CC) $(CPPFLAGS) $(CFLAGS) $(SAN_FLAGS) -fsanitize=fuzzer $(FUZZ_SRC) fuzz/fuzz_parser.c -o build/fuzz/fuzz_parser $(LDFLAGS)
	$(FUZZ_CC) $(CPPFLAGS) $(CFLAGS) $(SAN_FLAGS) -fsanitize=fuzzer $(FUZZ_SRC) fuzz/fuzz_deffile.c -o build/fuzz/fuzz_deffile $(LDFLAGS)
fuzz-smoke: fuzz
	ASAN_OPTIONS=detect_leaks=$(LEAK_CHECK) UBSAN_OPTIONS=halt_on_error=1 FUZZ_CC=$(FUZZ_CC) $(PYTHON) tools/run_fuzz.py
coverage:
	mkdir -p build/coverage
	rm -f build/coverage/*.profraw
	$(COVERAGE_CC) $(CPPFLAGS) $(CFLAGS) $(COV_FLAGS) $(LIB_SRC) $(TEST_SRC) -o build/coverage/test_runner $(LDFLAGS)
	$(COVERAGE_CC) $(CPPFLAGS) $(CFLAGS) $(COV_FLAGS) $(SRC) -o build/coverage/canframe $(LDFLAGS)
	LLVM_PROFILE_FILE='build/coverage/unit-%p.profraw' ./build/coverage/test_runner
	LLVM_PROFILE_FILE='build/coverage/cli-%p.profraw' CANFRAME_BIN=./build/coverage/canframe $(PYTHON) tests/test_cli.py
	$(LLVM_PROFDATA) merge -sparse build/coverage/*.profraw -o build/coverage/merged.profdata
	$(LLVM_COV) report build/coverage/canframe -object build/coverage/test_runner -instr-profile=build/coverage/merged.profdata $(COV_SRC) > build/coverage/report.txt
	$(LLVM_COV) export build/coverage/canframe -object build/coverage/test_runner -instr-profile=build/coverage/merged.profdata $(COV_SRC) > build/coverage/coverage.json
	$(LLVM_COV) show build/coverage/canframe -object build/coverage/test_runner -instr-profile=build/coverage/merged.profdata $(COV_SRC) -format=html -output-dir=build/coverage/html
	COVERAGE_CC=$(COVERAGE_CC) $(PYTHON) tools/coverage_metadata.py $(COV_SRC)
	cat build/coverage/report.txt
integration: $(BIN)
	$(PYTHON) tests/test_vcan.py
clean:
	rm -f $(OBJ) $(TEST_OBJ) $(OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(BIN) $(TEST_BIN)
-include $(OBJ:.o=.d) $(TEST_OBJ:.o=.d)
