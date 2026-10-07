# canframe

C11 tools for **Classical CAN** text logs and Linux SocketCAN receive input. The library parses frames, loads a small versioned signal definition format, decodes up to 64-bit signed/unsigned signals, and reports observed message timing. It does not transmit control commands.

Implemented:

- Compact, timestamped candump and verbose candump text input; standard/extended IDs and RTR.
- Strict ID/DLC/data/timestamp validation with bounded byte input and atomic parser output.
- `.cfd` v1 definitions, linear LSB0/MSB0 bit extraction, scaling, range quality and exact raw integers.
- Raw/NDJSON output; numeric ID list/range filters; definition/decode options.
- Bounded timing state keyed by interface + ID + standard/extended type.
- Linux-only, receive-only SocketCAN backend; bounded waits and signal cancellation.
- C unit groups, CLI golden tests, Linux vcan integration, real libFuzzer targets, sanitizer and LLVM source coverage targets.

DBC import, Motorola sawtooth numbering, multiplexing, CAN FD, error-frame reception and physical bus measurements are outside this version. A vcan test verifies local Linux socket behavior; it does not verify controllers, transceivers, arbitration or wiring.

## Build and portable tests

```sh
make
make test                 # C groups + CLI golden/semantic cases
make sanitize CC=clang    # ASan + UBSan, separate binaries under build/
```

Use GCC or Clang with C11 support. Python 3 is required for CLI/integration tooling. On macOS with Xcode:

```sh
SDKROOT="$(xcrun --show-sdk-path)" make clean all test CC=clang
```

`make sanitize` enables leak detection on Linux and disables it on macOS; the macOS run is not a leak-check result. Sanitizers and bounded fuzz runs supplement deterministic tests, rather than proving correctness for every input.

## Parse and decode

```sh
./canframe 123#1122334455667788
./canframe --json --input samples/candump-timestamped.log
./canframe --filter 0x100-0x1FF --input samples/compact.log
./canframe --json --definitions samples/demo.cfd --signals 100#1234
./canframe --json --defs samples/demo.cfd --decode tests/golden/frames.log
./canframe --json --defs samples/demo.cfd --diagnostics --input tests/golden/timing.log
```

`--defs` aliases `--definitions`. `--decode path` means `--signals --input path`; definitions are required. Loading definitions without `--signals` or `--diagnostics` does not change legacy frame output.

A frame matching the example RPM definition decodes `12 34` as **4660 rpm**. An extended low-valued text ID is written with eight hex digits (`00000100#1234`); it selects the extended definition and decodes the same payload as LITTLE value **13330**. An ID greater than `0x7FF` is also extended. Numeric filters match both ID types; definition lookup keeps them separate.

Signal JSON includes `raw_hex` and `raw_unsigned` as strings, plus `raw_signed` for signed signals. These preserve exact 64-bit values for readers such as JavaScript. `physical` is a double. `precision_warning` is conservative for raw magnitudes greater than `2^53`, even when a particular large integer is exactly representable. A configured range violation is `out_of_range`; overflow of the physical calculation is `numeric_error` with `physical:null`, while the raw result remains available.

Unmatched IDs retain their frame with `decode_status:"no_definition"`. RTR/error records are `not_applicable`. A matching payload length error retains raw data with an empty signal array and a reason. Parser/input/definition errors exit **1**; structural decode errors produce records and exit **2** at the end; successful parsing, unmatched definitions, RTR and per-signal quality flags exit **0**. Earlier successfully emitted records are not rolled back when a later input line fails.

## Text format contract

See [format notes](docs/format-notes.md) and [CFD v1 specification](docs/cfd-spec.md).

- Payloads are 0–8 bytes. `123#` is an explicit zero-length data frame; this version intentionally accepts empty compact payloads. `123#R` is RTR with requested DLC zero; text `R8` and CAN FD `##` forms are rejected.
- IDs are 1–8 hex digits, optionally prefixed `0x`; values may not exceed `0x1FFFFFFF`. Eight hex digits explicitly identify an extended frame, including a low-valued ID.
- Verbose input requires exactly the declared number of two-digit byte tokens. Extra tokens, signed DLC, overflow and trailing junk are rejected.
- Text timestamps are nonnegative decimal seconds with at most nine fractional digits, no exponent, up to `18446744073.709551615`. Diagnostics use the exact `uint64_t` nanoseconds. The legacy frame JSON timestamp remains a six-decimal compatibility display.
- `parse_bytes` accepts at most 4096 bytes and rejects embedded NUL. `parse_line` requires a NUL-terminated string; leading/trailing whitespace is removed. Direct compact/candump APIs require their exact form. Streaming input rejects oversized lines instead of interpreting chunks as separate frames.
- Parser failures leave the caller's frame unchanged.

## Timing diagnostics

Use `--diagnostics --definitions file.cfd`. Frame output remains raw or JSON as selected; diagnostic events and final summaries are NDJSON objects with a separate `type`. Prefer `--json` for one machine-readable stream.

`PERIOD` and `TIMEOUT` are optional per-message metadata. First timestamped observation establishes a baseline. Intervals inside `period ± tolerance` are inclusive; smaller/larger intervals are early/late events. Backward timestamps increment `out_of_order_events` and preserve the last accepted timestamp. Equal timestamps are zero intervals. Summary `frames` counts matching data records; `intervals` counts accepted inter-arrival observations, and min/max/mean describe those intervals. A late gap does **not** establish the exact number of lost frames.

Offline logs use `source_timestamp`, never file-read speed. Input without timestamps yields `timestamp_unavailable`. Unseen offline messages and EOF do not establish missing/stale events.

Live input uses `CLOCK_MONOTONIC` sampled when userspace reads the frame, labeled `userspace_observed`. Queueing and scheduler delay affect it; this is not a hardware bus timestamp. Configured messages begin `waiting`; at `age >= timeout` an unseen message emits one `missing`, or a seen one emits one `stale`. A later valid frame emits `recovery`. Idle polling occurs at most every 25 ms plus scheduler delay, so event delivery is not a hard real-time deadline. Filtered-out definitions do not produce missing events.

The map holds at most **128 interface/message keys**, without unbounded growth. A further key returns an explicit capacity error. Counts saturate at `UINT64_MAX`; interval means use an online average. There is no invented lost-frame count.

## Linux live receive and vcan

```sh
./canframe --interface vcan0 --json --count 10 --duration-ms 5000
./canframe --interface vcan0 --json --filter 0x100 --idle-timeout-ms 1000
./canframe --interface vcan0 --json --defs samples/demo.cfd --signals --diagnostics
```

`--interface` is mutually exclusive with file/inline input. `--count`, `--duration-ms` and `--idle-timeout-ms` are positive live-only bounds. Count and idle time refer to records accepted by the numeric filter; unrelated traffic does not keep the filtered session alive. SIGINT/SIGTERM cancel a waiting receiver. A readiness marker is flushed to stderr after successful bind. Live JSON includes RTR, exact `timestamp_ns`, and the userspace time-source label. Standard/extended and RTR flags are read separately from the kernel record. Error frames are disabled, and the CAN FD socket option is not enabled.

On macOS, `--interface` exits with an explicit Linux-only error; it does not fall back to stdin.

To run the separate integration test on a Linux kernel with vcan and `CAP_NET_ADMIN`:

```sh
sudo modprobe vcan               # if the kernel requires a module
make
sudo env CANFRAME_BIN="$PWD/canframe" python3 tests/test_vcan.py
```

The test creates and cleans up only its own `cft<PID>` interface. It uses another socket to send standard, extended-low-ID, RTR and zero-length vectors; checks filters, idle timeout, SIGINT/SIGTERM cancellation and missing/stale/recovery. Startup and finish waits are bounded. Exit **77** means `SKIPPED — vcan unavailable`, not a pass. The required CI integration job treats this as an environment failure. The portable tests and workflow definition alone do not validate Linux/vcan behavior.

## API

Public headers are under `include/`:

- `parser.h`: `parse_bytes`, `parse_line`, direct format parsers and status strings.
- `deffile.h`: pure `deffile_parse_bytes`; file wrapper `deffile_load_ex` with line/reason errors; four-argument `deffile_load` wrapper.
- `decode.h`: `validate_definition`, `(ID,type)` lookup, `decode_signals(def,frame,results,capacity,&count)`.
- `diagnostics.h`: initialization, observation and explicit live-clock polling; no hidden file I/O or clock in the timing model.
- `socketcan.h`: caller-owned Linux descriptor and normalized receive records; portable unsupported stub.
- `output.h` and `cli.h`: rendering and argument/filter helpers.

Definition/decode failure sets count to zero and leaves output arrays unchanged. Counts/capacities are explicit. Successfully decoded result pointers reference the caller-owned definition, which must remain alive. Zero-signal definitions are allowed, but decoder callers still provide a result buffer and count pointer. Model objects passed directly to output functions must be initialized, bounded and NUL-terminated; parsed objects meet those conditions.

## Fuzz and source coverage

```sh
make fuzz-smoke FUZZ_CC=clang
FUZZ_RUNS=100000 FUZZ_SECONDS=60 make fuzz-smoke FUZZ_CC=clang
make coverage COVERAGE_CC=clang LLVM_PROFDATA=llvm-profdata LLVM_COV=llvm-cov
```

Use matching LLVM compiler/profile/coverage tools. On macOS a Homebrew LLVM installation can provide libFuzzer and coverage tools, with `SDKROOT` set as above. Each real target calls the byte parser; the CFD target also validates successful definitions and decodes generated frame values. Both verify failure count/output invariants. Tracked seed corpora stay unchanged; mutation/crash artifacts go under `build/fuzz/`.

The smoke default is seed **20261007**, at most **30000 runs per target**, and a **60-second safety cap**, not a claimed 10-minute campaign. Logs and `manifest.json` preserve actual elapsed time, exit status, source hashes, toolchain and exact commands. A short successful campaign does not prove absence of bugs.

Coverage produces `build/coverage/report.txt`, `coverage.json`, `merged.profdata`, `scope.json` and HTML. It measures source lines/regions/branches from the C and CLI tests, excluding `main.c` and test code. On a non-Linux host the unsupported SocketCAN module is explicitly excluded from the application denominator. Linux coverage includes that module; privileged vcan validation is a separate job. Fuzzer edge counts are not source-line coverage percentages. There is no fixed coverage percentage or performance claim in this README.

CI separates GCC/Clang portable tests, LLVM sanitizer/fuzz/coverage, and Linux vcan integration. Review actual runner results for the exact revision before calling those jobs verified.

The [8 October portable validation record](docs/evidence/2026-10-07/README.md)
contains 9 C groups, 17 CLI groups, ASan/UBSan results, two real 30,000-run fuzz
smokes and raw LLVM source coverage: 98.03% lines and 76.96% branches over the
seven portable library modules. Exact source hashes, commands and exclusions
are retained. That macOS record does not verify Linux/vcan or physical CAN.
