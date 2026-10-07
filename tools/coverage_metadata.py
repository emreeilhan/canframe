#!/usr/bin/env python3
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from datetime import datetime, timezone
root = Path(__file__).resolve().parents[1]
metadata = {
    "recorded_at_utc": datetime.now(timezone.utc).isoformat(),
    "base_sha": subprocess.check_output(["git","rev-parse","HEAD"],cwd=root,text=True).strip(),
    "platform": sys.platform,
    "compiler": subprocess.check_output([os.environ.get("COVERAGE_CC","clang"),"--version"],text=True),
    "included_sources": sys.argv[1:],
    "excluded_sources": {"src/main.c":"entry point; unit plus CLI execution is run, but entry point excluded from application-library denominator"},
    "measurement_type":"LLVM source line, region and branch coverage from unit and CLI golden tests; no fuzz profiles included",
    "source_sha256": {str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted([*root.glob("src/*.c"),*root.glob("include/*.h")])},
}
if sys.platform != "linux": metadata["excluded_sources"]["src/socketcan.c"] = "Linux implementation unavailable on this host; portable unsupported stub is exercised but excluded"
(root/"build/coverage/scope.json").write_text(json.dumps(metadata,indent=2)+"\n")
