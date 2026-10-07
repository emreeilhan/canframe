# Text parser formats

Supported forms:

```text
123#AABBCC
123#
123#R
(1700000000.123456789) can0 123#11223344
can0  123  [4]  11 22 33 44
```

The compact payload is uninterrupted pairs of hex digits, at most eight bytes. Empty payload is valid zero-DLC data. Only `R` is supported for text RTR (DLC zero); `R8` and CAN FD `##` are not accepted. IDs are optional-`0x` 1–8 hex digits, bounded before narrowing to `uint32_t`; eight digits mark extended type, even if the value is ≤`0x7FF`. Otherwise values above `0x7FF` are extended.

Timestamped input requires whitespace after `)` and between interface and frame. Timestamp is nonnegative decimal seconds, with 0–9 fractional digits (a decimal point requires at least one following digit), no sign/exponent/NaN/Inf, fitting `uint64_t` nanoseconds. The interface token must fit 15 bytes plus NUL. Exact source nanoseconds are kept alongside a compatibility display double.

JSON string output preserves valid UTF-8 interface names. A malformed UTF-8 byte is emitted as `\u00XX`, representing that byte value, so a byte-oriented log cannot produce an invalid JSON encoding. Quotes, backslashes and control bytes are escaped separately.

Verbose input requires whitespace between interface, ID, bracketed decimal DLC and each two-digit byte. The number of byte tokens must match DLC exactly, including zero. Signs, overflows, joined/trailing bytes and extra tokens are rejected.

`parse_bytes` supports at most 4096 bytes, rejects embedded NUL, removes leading/trailing whitespace and selects a format. `parse_line` bounds its scan of a NUL-terminated string. Direct format parsers accept their exact form; callers use `parse_bytes` for arbitrary byte input. All parser failures preserve the output frame.

`parse_line` dispatch:

1. Leading `(` → timestamped.
2. `#` without whitespace before it → compact.
3. Otherwise → verbose.

The CLI reads bounded complete lines (not `fgets` chunks), retains embedded NUL for validation, ignores blank lines, accepts CRLF and rejects an overlong line. A failed record stops parsing with a line number and reason.
