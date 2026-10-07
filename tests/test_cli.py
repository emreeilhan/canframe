#!/usr/bin/env python3
"""CLI golden tests: explicit files and independently calculated expectations."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

BIN = os.environ.get("CANFRAME_BIN", "./canframe")
ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
passed = 0

def run(args, data=None, code=0):
    p = subprocess.run([BIN, *args], input=data, capture_output=True, timeout=10)
    assert p.returncode == code, (args, p.returncode, p.stderr.decode(errors="replace"))
    return p

def check(name, fn):
    global passed
    fn()
    passed += 1
    print(f"{name}: ok")

def exact(args, filename):
    assert run(args).stdout == Path(filename).read_bytes()

check("raw golden", lambda: exact(["--input", "tests/golden/frames.log"], "tests/golden/raw.txt"))
check("JSON golden", lambda: exact(["--json", "--input", "tests/golden/frames.log"], "tests/golden/raw.jsonl"))
check("signal golden", lambda: exact(["--json", "--definitions", "samples/demo.cfd", "--signals", "--input", "tests/golden/frames.log"], "tests/golden/signals.jsonl"))
check("defs/decode aliases", lambda: exact(["--json", "--defs", "samples/demo.cfd", "--decode", "tests/golden/frames.log"], "tests/golden/signals.jsonl"))
check("definitions alone preserve output", lambda: exact(["--json", "--definitions", "samples/demo.cfd", "--input", "tests/golden/frames.log"], "tests/golden/raw.jsonl"))

def filtered():
    expected = Path("tests/golden/raw.jsonl").read_bytes().splitlines(keepends=True)
    for value in ["100", "0x100", "100,FFF", "100-100"]:
        assert run(["--json", "--filter", value, "--input", "tests/golden/frames.log"]).stdout == expected[0]+expected[2]
check("numeric filter keeps standard and extended", filtered)

def invalid_options():
    for args in [["--unknown"], ["--signals"], ["--decode", "tests/golden/frames.log"], ["--diagnostics"], ["--input", "x", "123#AA"], ["--interface", "vcan0", "--input", "x"], ["--interface", "vcan0", "123#AA"], ["--filter", "123,"], ["--filter", "100000123"], ["--filter", "-1"], ["--filter", "123,,456"], ["--filter", "1-2,3"], ["--count", "1"], ["--interface", "vcan0", "--count", "0"], ["--interface", "vcan0", "--duration-ms", "4294967296"], ["--interface", "vcan0", "--count", "1", "--count", "2"], ["--input"], ["--definitions"]]:
        assert run(args, code=1).stdout == b""
check("invalid CLI combinations", invalid_options)

def invalid_input():
    for data in [b"123#"+b"AA"*256+b"\n", b"100000123#AA\n", b"(nan) can0 123#AA\n", b"(inf) can0 123#AA\n", b"123#AA\0ignored\n", b"A"*4097+b"\n", b"can0 123 [1] AA BB\n"]:
        assert run(["--json"], data, code=1).stdout == b""
check("parser hardening CLI", invalid_input)
check("blank and CRLF input", lambda: None if run(["--json"], b"\r\n  \n123#AA\r\n").stdout == b'{"id":"0x123","extended":false,"dlc":1,"data":[170]}\n' else (_ for _ in ()).throw(AssertionError()))

def escaped_interface():
    p = run(["--json"], b'(1) x"\\\x01 123#AA\n')
    assert json.loads(p.stdout)["interface"] == 'x"\\\x01'
    for token, expected in [(b"x\xff", "x\u00ff"), (b"x\xc3", "x\u00c3"),
                            (b"x\xc0\xaf", "x\u00c0\u00af"),
                            (b"x\xed\xa0\x80", "x\u00ed\u00a0\u0080"),
                            (b"x\xf4\x90\x80\x80", "x\u00f4\u0090\u0080\u0080"),
                            ("x\u00e7\U0001f680".encode(), "x\u00e7\U0001f680")]:
        p = run(["--json"], b'(1) '+token+b' 123#AA\n')
        assert json.loads(p.stdout.decode("utf-8"))["interface"] == expected
check("JSON escaping", escaped_interface)

def decode_failures():
    for value in ["100#01", "100#010203"]:
        p = run(["--json", "--defs", "samples/demo.cfd", "--signals", value], code=2)
        row = json.loads(p.stdout)
        assert row["decode_status"] == "invalid DLC" and row["signals"] == [] and row["data"]
    row = json.loads(run(["--json", "--defs", "samples/demo.cfd", "--signals", "102#7FFF"]).stdout)
    assert row["signals"][0]["quality"] == "out_of_range"
check("decode failure retains raw", decode_failures)

def definition_errors():
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)/"bad.cfd"
        path.write_text("CFD 1\nMESSAGE 0x100 STANDARD 1 M\nSIGNAL x 0 9 BIG SIGNED 1 0 u\nEND\n")
        p = run(["--defs", str(path), "123#AA"], code=1)
        assert p.stdout == b"" and b"definitions line 4:" in p.stderr
        path.write_bytes(b"#"*65537)
        assert b"64 KiB" in run(["--defs", str(path)], code=1).stderr
    assert run(["--defs", "/missing/cfd"], code=1).stdout == b""
check("definition startup errors", definition_errors)

def wide_numeric():
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)/"wide.cfd"
        path.write_text("CFD 1\nMESSAGE 0x123 STANDARD 8 Wide\nSIGNAL wide 0 64 BIG UNSIGNED 1e308 0 u\nEND\n")
        row = json.loads(run(["--json", "--defs", str(path), "--signals", "123#FFFFFFFFFFFFFFFF"]).stdout)
        signal = row["signals"][0]
        assert signal["raw_unsigned"] == "18446744073709551615" and signal["raw_hex"] == "0xFFFFFFFFFFFFFFFF"
        assert signal["physical"] is None and signal["quality"] == "numeric_error" and signal["precision_warning"]
check("wide raw stays exact and physical null", wide_numeric)

def timing():
    rows = [json.loads(line) for line in run(["--json", "--defs", "samples/demo.cfd", "--diagnostics", "--input", "tests/golden/timing.log"]).stdout.splitlines()]
    events = [r for r in rows if r.get("type") == "diagnostic"]
    assert [r["events"] for r in events] == [["baseline"],["early"],["normal"],["normal"],["late"],["out_of_order"]]
    summary = rows[-1]
    assert summary["frames"] == 6 and summary["intervals"] == 4 and summary["min_ns"] == "89000000" and summary["max_ns"] == "111000000" and summary["mean_ns"] == 100000000
    assert summary["early_events"] == 1 and summary["late_events"] == 1 and summary["out_of_order_events"] == 1
    assert summary["stale_events"] == 0 and summary["missing_events"] == 0 and summary["time_source"] == "source_timestamp"
check("offline source timestamps and EOF", timing)

def unavailable():
    rows = [json.loads(line) for line in run(["--json", "--defs", "samples/demo.cfd", "--diagnostics", "100#1234"]).stdout.splitlines()]
    assert rows[1]["events"] == ["timestamp_unavailable"] and rows[-1]["intervals"] == 0
check("timestamp unavailable", unavailable)

def sources():
    for source in ["samples/compact.log","samples/candump-timestamped.log","samples/candump-verbose.log"]:
        p = run(["--json", "--input", source])
        rows = [json.loads(line) for line in p.stdout.splitlines()]
        assert rows and all(0 <= row["dlc"] <= 8 for row in rows)
check("three original sample formats", sources)

if sys.platform != "linux":
    check("explicit unsupported live mode", lambda: None if b"only on Linux" in run(["--interface", "vcan0"], code=1).stderr else (_ for _ in ()).throw(AssertionError()))
print(f"ok: {passed} CLI golden/semantic cases")
