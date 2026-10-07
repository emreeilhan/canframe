#!/usr/bin/env python3
"""Run deterministic bounded smoke campaigns; corpus mutation stays under build/."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
from datetime import datetime, timezone

root = Path(__file__).resolve().parents[1]
os.chdir(root)
seed = int(os.environ.get("FUZZ_SEED", "20261007"))
runs = int(os.environ.get("FUZZ_RUNS", "30000"))
limit = int(os.environ.get("FUZZ_SECONDS", "60"))
source_files = sorted([*root.glob("src/*.c"), *root.glob("include/*.h"), *root.glob("fuzz/*.c")])
manifest = {"recorded_at_utc": datetime.now(timezone.utc).isoformat(), "source_sha256": {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files}, "seed": seed, "requested_runs": runs, "max_total_time_seconds": limit,
            "base_sha": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
            "compiler": subprocess.check_output([os.environ.get("FUZZ_CC", "clang"), "--version"], text=True),
            "campaigns": []}
for target, maximum in [("parser", 4096), ("deffile", 65536)]:
    directory = root/"build"/"fuzz"/target
    directory.mkdir(parents=True, exist_ok=True)
    corpus = directory/"corpus"
    if corpus.exists(): shutil.rmtree(corpus)
    shutil.copytree(root/"tests"/"corpus"/target, corpus)
    command = [str(root/"build"/"fuzz"/("fuzz_"+target)), str(corpus), f"-seed={seed}", f"-runs={runs}", f"-max_total_time={limit}", f"-max_len={maximum}", f"-artifact_prefix={directory}/"]
    if target == "deffile": command.append("-dict=fuzz/cfd.dict")
    start = time.monotonic()
    with (directory/"run.log").open("w") as log:
        p = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=limit+30)
    elapsed = time.monotonic()-start
    manifest["campaigns"].append({"target": target, "command": command, "elapsed_seconds": elapsed, "exit_code": p.returncode, "log": str(directory/"run.log"), "binary_sha256": hashlib.sha256(Path(command[0]).read_bytes()).hexdigest()})
    (root/"build"/"fuzz"/"manifest.json").write_text(json.dumps(manifest, indent=2)+"\n")
    print(f"{target}: exit={p.returncode}, elapsed={elapsed:.3f}s; {directory/'run.log'}", flush=True)
    if p.returncode: raise SystemExit(p.returncode)
