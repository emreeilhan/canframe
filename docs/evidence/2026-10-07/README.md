# Portable validation record

Measured **8 October 2026 (Europe/Istanbul)**; raw metadata uses UTC
(`2026-10-07T21:14:18Z`). The directory retains the implementation's 7 October
start date. These measurements cover working-tree sources based on
`59156ab4d0d8f1edf02256708c9ef056426c0588`, before the implementation commit.
The JSON records preserve exact source SHA-256 values rather than treating the
baseline commit as the tested implementation.

| Check | Observed result |
|---|---|
| Portable build and tests | 9 C groups and 17 CLI golden/semantic groups passed |
| Apple Clang ASan/UBSan | The same 9 + 17 groups passed; leak detection disabled on macOS |
| Parser libFuzzer smoke | 30,000 executions, seed 20261007, exit 0; elapsed 0.503 seconds |
| CFD libFuzzer smoke | 30,000 executions, seed 20261007, exit 0; elapsed 0.946 seconds |
| LLVM source lines | 646 / 659, **98.03%** |
| LLVM source branches | 815 / 1059, **76.96%** |
| LLVM source regions | 1427 / 1585, **90.03%** |

The fuzz and coverage compiler was Homebrew LLVM **23.1.1**, targeting native
`arm64-apple-darwin27.0.0`. Exact flags and commands are in the build logs;
compiler strings, source hashes and binary hashes are in the JSON context files.
Each fuzz campaign had both a 30,000-run bound and a 60-second safety cap. Neither
campaign ran for 60 seconds, and neither is the proposed 10-minute campaign.
Fuzzer coverage counters are not the source coverage figures above.

The source coverage denominator includes `cli.c`, `parser.c`, `output.c`,
`decode.c`, `deffile.c`, `numbers.c` and `diagnostics.c`. It combines C unit and
CLI execution profiles. It excludes `main.c`, tests, fuzz profiles and
`socketcan.c` on this non-Linux host. Per-file totals, raw LLVM export,
merged profile and source hashes are retained here. Linux builds may have a
different denominator, so their percentages should not be compared without
checking scope.

## Independent review and regression

The review covered bounded/atomic text parsing; CFD grammar, budgets and mixed
bit-order overlap; signed 64-bit extraction; timing source separation, idle
polling and EOF behavior; CLI errors; and SocketCAN descriptor cleanup and
bounded receive control.

One reproducible output defect was fixed: `(1) x\xFF 123#AA` parsed successfully
but emitted invalid UTF-8 JSON. Output now preserves valid UTF-8 and escapes each
malformed byte as `\u00XX`. Regression cases cover a lone high byte, truncated
sequence, overlong encoding, surrogate encoding, out-of-range code point and
valid UTF-8. The recorded ASan/UBSan and CLI runs include this fix. No other
concrete defect was established by this review.

## Validation boundary

Linux SocketCAN/vcan runtime has **not** been exercised in this macOS record.
The committed revision must pass the separate Linux integration job before that
behavior is called verified. No physical CAN controller, transceiver, wiring,
arbitration or hardware timestamp was measured. Bounded fuzzing and sanitizer
success do not establish correctness for all inputs.

`validation-summary.json` provides totals and artifact/source hashes. The
portable logs, sanitizer logs, fuzz manifests/logs and LLVM coverage files are
the underlying observations.

## Native Linux follow-up — 8 October

[Run 37688890032](https://github.com/emreeilhan/canframe/actions/runs/37688890032)
passed for `b07313eb80fbff0a2a88c7db019be7ad7b4958d0`: GCC/Clang portable tests,
ASan/UBSan, parser/CFD fuzz campaigns, source coverage and five privileged
vcan integration groups. The first attempt failed because the runner lacked
its vcan module; the required job now installs the official extra-module package
matching the running kernel, rather than counting that skip as a pass.

The [Linux records](../2026-10-08/linux-ci/) include module environment, vcan
results, exact revision, LLVM export and fuzz logs. Linux Clang18 portable-test
coverage includes all eight library modules: 661/726 lines (91.05%) and
791/1042 branch outcomes (75.91%). The SocketCAN receive functions are largely
uncovered by this coverage suite; the separate vcan job exercises them without
merging its profiles into that denominator. The earlier seven-module macOS
percentages measure a different scope/toolchain.

The five vcan groups cover standard/extended-low-ID/RTR/empty normalization,
ID filters, bounded idle timeout, SIGINT/SIGTERM cancellation, and live
missing/stale/recovery plus diagnostic filtering. This is kernel-local socket
validation; physical CAN controller/transceiver/wiring/arbitration remains untested.
