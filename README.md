# canframe

Command-line CAN frame parser written in C11. Reads raw frames from stdin or a file, outputs them as formatted text or NDJSON, and can filter by CAN ID.

No external dependencies. Build with any C11 compiler.

## What it does

- Parses three input formats: compact (`123#AABBCC`), candump timestamped, and candump verbose
- Outputs frames as human-readable text (`--raw`) or NDJSON (`--json`)
- Filters by CAN ID: single ID, comma-separated list, or hex range (`--filter`)
- Handles standard 11-bit IDs, extended 29-bit IDs, and RTR frames
- Preserves timestamp and interface name in JSON output when the source format carries them

## Build

```
make
make test
```

## Usage

```bash
# parse a single inline frame
./canframe 123#1122334455667788

# read from stdin
./canframe --raw < samples/compact.log
./canframe --json < samples/candump-timestamped.log

# filter by CAN ID
./canframe --filter 0x123 --json < samples/compact.log
./canframe --filter 0x100-0x1FF --raw < samples/compact.log
./canframe --filter 0x123,0x456 --raw < samples/compact.log

# pipe to jq
./canframe --json < samples/candump-timestamped.log | jq '.id'
```

## Input formats

**Compact** — one frame per line, socketcan wire format:
```
123#1122334455667788
18DAF110#0322010000000000
200#R
```

**candump timestamped** — output of `candump -ta`:
```
(1700000001.000000) can0 7DF#0201050000000000
```

**candump verbose** — output of `candump -l`:
```
can0   7DF   [8]  02 01 05 00 00 00 00 00
```

The input format is auto-detected per line, so mixed logs work fine.

## Planned

- `.cfd` signal definition files — a lightweight text format for naming CAN messages and their signals (spec stub in `docs/cfd-spec.md`)
- Signal decoding — extract named values from raw frame bytes using a loaded definition file
