# CFD v1

This is a small canframe-specific definition format, **not DBC**. Input is ASCII tokens separated by spaces/tabs; CRLF is accepted. Blank lines and full-line comments beginning with `#` after whitespace are ignored. Inline comments, quoting and names containing spaces are not supported. The first meaningful line must be exactly `CFD 1`. Unknown versions/keywords are errors.

```text
CFD 1
MESSAGE 0x100 STANDARD 2 RPM_STATUS
PERIOD 100 10
TIMEOUT 500
SIGNAL rpm 0 16 BIG UNSIGNED 1 0 rpm
END

MESSAGE 0x102 STANDARD 2 COOLANT_STATUS
SIGNAL coolant 0 16 BIG SIGNED 0.1 0 degC
RANGE coolant -40 150
END
```

Grammar:

```text
MESSAGE <0xHEX-ID> <STANDARD|EXTENDED> <expected-DLC> <message-name>
PERIOD <positive-period-ms> <tolerance-ms>
TIMEOUT <positive-timeout-ms>
SIGNAL <name> <start-bit> <width> <BIG|LITTLE> <SIGNED|UNSIGNED> <scale> <offset> <unit>
RANGE <earlier-signal-name> <minimum> <maximum>
END
```

A MESSAGE must end before the next begins. PERIOD and TIMEOUT may each appear once. Signals and timing directives occur only within a message. RANGE refers to an earlier signal in that message and may appear once per signal. SIGNAL cannot contain extra fields; END has no fields.

Limits and validation:

- Maximum 65536 input bytes, 1024 bytes per line excluding LF (CR counts toward the bound), 64 messages and 16 signals per message.
- Message/signal names: 1–31 ASCII letters, digits or underscores. Units: 1–15 of those characters; use `none` for dimensionless signals.
- ID syntax `0x` followed by 1–8 hex digits. STANDARD max `0x7FF`; EXTENDED max `0x1FFFFFFF`. Duplicate `(ID,type)` keys are rejected; standard and extended instances of the same numeric ID may coexist.
- Expected DLC 0–8. Start bit 0–63, width 1–64, with `start + width <= expected-DLC * 8`. A zero-length message cannot contain a signal.
- Duplicate signal names and overlapping physical payload bits are rejected, including overlaps between BIG and LITTLE layouts.
- Scale/offset/range numbers use a locale-independent decimal grammar: optional sign, decimal digits with an optional dot, optional `e`/`E` exponent with optional sign. At least one mantissa digit and one exponent digit if present are required. NaN, Inf, hex floats, trailing junk, overflow and underflow-to-unrepresentable coefficients are rejected. Zero and negative scale are allowed.
- Range minimum/maximum must be finite with minimum ≤ maximum. Both endpoints are inclusive.
- Timing fields are decimal integers through `UINT32_MAX` milliseconds. PERIOD and TIMEOUT must be positive; tolerance is 0 through `period-1`. If both period and timeout are present, `timeout > period + tolerance`. Missing directives are represented by zero in the model, without implying a default deadline.

## Bit numbering and signed conversion

LITTLE uses **linear LSB0**: for extracted bit `i`, `p = start + i`; payload byte `p/8`, bit `p%8` contributes to raw bit `i`.

BIG uses **linear MSB0**: `p = start + i`; payload byte `p/8`, bit `7-(p%8)` is appended to raw (`raw = (raw << 1) | bit`). This is not the DBC Motorola sawtooth convention.

Independent reference vectors:

| Payload | Start / width | Order | Raw |
| --- | --- | --- | --- |
| `12 34` | 0 / 16 | BIG | `0x1234` |
| `12 34` | 0 / 16 | LITTLE | `0x3412` |
| `D6 03` | 3 / 7 | LITTLE | 122 |
| `B2 60` | 3 / 7 | BIG | 73 |
| `FF` | 0 / 8 | BIG SIGNED | −1 |
| `80` | 0 / 8 | BIG SIGNED | −128 |
| `FF F0` | 0 / 12 | BIG SIGNED | −1 |
| `80 00` | 0 / 12 | BIG SIGNED | −2048 |

Signed values use two's-complement interpretation for the configured width. Widths 1, 63 and 64 are supported without shifting by 64 or converting an out-of-range unsigned integer directly to signed. Raw bit patterns remain exact even for `UINT64_MAX` and `INT64_MIN`.

`physical = interpreted_raw * scale + offset` uses double precision. Raw magnitudes above `2^53` carry a conservative precision warning. Out-of-range physical values retain the decoded raw result and `out_of_range`. A nonfinite physical calculation retains raw results, marks `numeric_error`, and renders JSON `physical:null`.

Decoder lookup requires ID **and type**. Data payload length must match expected DLC exactly. RTR/error frames are not signal data. Structural failure sets result count to zero and preserves the caller's result array; successful individual numeric/range quality flags do not erase other decoded signals.

## Parser API and errors

`deffile_parse_bytes(data,size,definitions,capacity,&count,&error)` is independent of filesystem, clock and network. `deffile_load_ex` bounds file reads and supplies the same contract. Success publishes the complete candidate array. Any failure sets count to zero and leaves definitions unchanged.

`cfd_error_t` contains a status, 1-based source line for grammar/bounds errors and a short reason. Whole-file/I/O/null/NUL errors may use line zero. Status distinguishes grammar, ID/DLC, bounds/overlap, capacity and I/O failure. Capacity may be lower than 64 when supplied by the caller; it is never silently exceeded.
