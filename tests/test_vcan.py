#!/usr/bin/env python3
"""Linux-only vcan integration; exit 77 is an environment skip, never a pass."""
import json
import os
from pathlib import Path
import selectors
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
BIN = os.environ.get("CANFRAME_BIN", "./canframe")
IFACE = f"cft{os.getpid()}"
processes = []
passed = 0

def skip(reason):
    print(f"SKIPPED — vcan unavailable: {reason}", file=sys.stderr)
    raise SystemExit(77)

if sys.platform != "linux": skip("requires Linux SocketCAN kernel")
if not shutil.which("ip") or not hasattr(socket, "AF_CAN"): skip("iproute2/Python AF_CAN missing")
p = subprocess.run(["ip", "link", "add", IFACE, "type", "vcan"], capture_output=True, text=True)
if p.returncode: skip(p.stderr.strip()+" (vcan kernel support/CAP_NET_ADMIN required)")

def listen(args):
    proc = subprocess.Popen([BIN, "--interface", IFACE, *args], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    processes.append(proc)
    selector = selectors.DefaultSelector()
    selector.register(proc.stderr, selectors.EVENT_READ)
    ready = selector.select(3)
    selector.close()
    assert ready, "receiver never signalled readiness"
    marker = proc.stderr.readline()
    assert b"listening on" in marker, marker
    return proc

def finish(proc, timeout=3):
    out, err = proc.communicate(timeout=timeout)
    assert proc.returncode == 0, (proc.returncode, err)
    return [json.loads(line) for line in out.splitlines()]

def send(sock, id_, data=b"", extended=False, rtr=False, dlc=None):
    can_id = id_ | (0x80000000 if extended else 0) | (0x40000000 if rtr else 0)
    length = len(data) if dlc is None else dlc
    assert sock.send(struct.pack("=IB3x8s", can_id, length, data.ljust(8,b"\0"))) == 16

def check(name, fn):
    global passed
    fn(); passed += 1; print(f"{name}: ok", flush=True)

try:
    subprocess.run(["ip", "link", "set", IFACE, "up"], check=True)
    with socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW) as sender:
        sender.bind((IFACE,))
        def normal():
            proc = listen(["--json", "--count", "5", "--duration-ms", "2000"])
            send(sender,0x123,b"\xAA\x55")
            send(sender,0x123,b"\x11",extended=True)
            send(sender,0x123,rtr=True,dlc=2)
            send(sender,0x18daf110,b"\x01\x02\x03",extended=True)
            send(sender,0x100)
            rows = finish(proc)
            assert len(rows) == 5
            assert [(r["id"],r["extended"],r["rtr"],r["dlc"],r["data"]) for r in rows] == [
                ("0x123",False,False,2,[170,85]),("0x123",True,False,1,[17]),("0x123",False,True,2,[]),
                ("0x18DAF110",True,False,3,[1,2,3]),("0x100",False,False,0,[])]
            assert all(r["interface"] == IFACE and r["time_source"] == "userspace_observed" for r in rows)
            assert [int(r["timestamp_ns"]) for r in rows] == sorted(int(r["timestamp_ns"]) for r in rows)
        check("standard/extended-low-ID/RTR/empty normalization", normal)
        def filters():
            for value in ["123", "123,456", "120-12F"]:
                proc = listen(["--json", "--filter", value, "--count", "2", "--duration-ms", "1500"])
                send(sender,0x777,b"\xFF")
                send(sender,0x123,b"\x01")
                send(sender,0x123,b"\x02",extended=True)
                rows = finish(proc)
                assert len(rows) == 2 and [r["extended"] for r in rows] == [False,True] and all(r["id"] == "0x123" for r in rows)
        check("single/list/range filters", filters)
        def idle():
            start = time.monotonic()
            proc = listen(["--json", "--idle-timeout-ms", "150", "--duration-ms", "1500"])
            assert finish(proc) == []
            assert .12 <= time.monotonic()-start < 1.5
        check("bounded idle timeout", idle)
        def cancel():
            for sig in [signal.SIGINT,signal.SIGTERM]:
                proc = listen(["--json"])
                start = time.monotonic(); proc.send_signal(sig)
                assert finish(proc,1.5) == [] and time.monotonic()-start < 1.5
        check("SIGINT and SIGTERM cancellation", cancel)
        def diagnostic():
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory)/"timing.cfd"
                path.write_text("CFD 1\nMESSAGE 0x100 STANDARD 1 Seen\nPERIOD 20 15\nTIMEOUT 80\nEND\nMESSAGE 0x101 STANDARD 1 Unseen\nTIMEOUT 80\nEND\n")
                proc = listen(["--json", "--defs", str(path), "--diagnostics", "--duration-ms", "450"])
                time.sleep(.12); send(sender,0x100,b"\x01")
                time.sleep(.02); send(sender,0x100,b"\x02")
                time.sleep(.12); send(sender,0x100,b"\x03")
                rows = finish(proc)
                events = [r for r in rows if r.get("type") == "diagnostic"]
                assert sum("missing" in r["events"] for r in events if r["id"] == "0x101") == 1
                seen = [r for r in events if r["id"] == "0x100"]
                assert any("stale" in r["events"] for r in seen) and sum("recovery" in r["events"] for r in seen) == 2
                assert all(r["time_source"] == "userspace_observed" for r in events)
                summaries = [r for r in rows if r.get("type") == "diagnostic_summary"]
                assert len(summaries) == 2 and next(r for r in summaries if r["id"] == "0x100")["frames"] == 3
                # A filtered-out configured message must not generate missing events.
                proc = listen(["--json", "--defs", str(path), "--diagnostics", "--filter", "100", "--duration-ms", "120"])
                filtered = finish(proc)
                assert not any(r.get("id") == "0x101" for r in filtered)
        check("idle missing/stale/recovery and diagnostic filter", diagnostic)
    print(f"ok: {passed} Linux vcan integration cases")
finally:
    for proc in processes:
        if proc.poll() is None:
            proc.kill(); proc.communicate()
    subprocess.run(["ip", "link", "del", IFACE], check=False)
