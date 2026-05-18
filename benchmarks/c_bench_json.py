#!/usr/bin/env python3
"""Run C benchmark binaries and emit a combined JSON report.

Each binary writes bench_<component>_core.json to its working directory
via jm_bench_write_json().  This script collects those per-component
files and merges them into a single top-level JSON whose schema matches
pytest-benchmark output (same machine_info / commit_info / benchmarks
keys) so C and Python results can be compared directly.

Usage:
    python benchmarks/c_bench_json.py [--build-type Release] <binary> ...
    make benchmark-c
"""

import json
import os
import re
import subprocess
import sys
import tempfile
from datetime import datetime, timezone


def _machine_info():
    import platform

    info = {
        "node": platform.node(),
        "system": platform.system(),
        "release": platform.release(),
        "machine": platform.machine(),
        "cpu": {},
    }
    try:
        with open("/proc/cpuinfo") as f:
            for line in f:
                if line.startswith("model name"):
                    info["cpu"]["brand_raw"] = line.split(":", 1)[1].strip()
                    break
    except OSError:
        pass
    info["cpu"]["count"] = os.cpu_count()
    return info


def _commit_info():
    try:
        sha = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], stderr=subprocess.DEVNULL
        ).decode().strip()
        branch = subprocess.check_output(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            stderr=subprocess.DEVNULL,
        ).decode().strip()
        dirty = bool(
            subprocess.check_output(
                ["git", "status", "--porcelain"],
                stderr=subprocess.DEVNULL,
            ).decode().strip()
        )
        return {"id": sha, "branch": branch, "dirty": dirty}
    except Exception:
        return {}


def _component_from_binary(path):
    """'acc_cf64' from '.../bench_acc_cf64_core'."""
    n = os.path.basename(path)
    n = re.sub(r"^bench_", "", n)
    n = re.sub(r"_core$", "", n)
    return n


def _run_binary(binary, tmpdir):
    """Run binary in tmpdir; return list of benchmark entry dicts."""
    subprocess.run(
        [os.path.abspath(binary)],
        cwd=tmpdir,
        capture_output=True,
        text=True,
        timeout=300,
    )

    # Collect every bench_*_core.json the binary wrote.
    entries = []
    for fname in os.listdir(tmpdir):
        if not (fname.startswith("bench_") and fname.endswith(".json")):
            continue
        path = os.path.join(tmpdir, fname)
        try:
            with open(path) as f:
                data = json.load(f)
        except (OSError, json.JSONDecodeError):
            continue
        for entry in data.get("benchmarks", []):
            # Prefix name with component so entries from different
            # binaries are unambiguous in history comparisons.
            # fullname = "bench_acc_f32_core::step" → "acc_f32::step"
            fn = entry.get("fullname", "")
            m = re.match(r"bench_(.+)_core::(.+)", fn)
            if m:
                entry["name"] = f"{m.group(1)}::{m.group(2)}"
            entries.append(entry)

    return entries


def main(argv=None):
    argv = argv or sys.argv[1:]

    build_type = "Release"
    binaries = []
    i = 0
    while i < len(argv):
        if argv[i] == "--build-type" and i + 1 < len(argv):
            build_type = argv[i + 1]
            i += 2
        else:
            binaries.append(argv[i])
            i += 1

    if not binaries:
        print(
            "Usage: c_bench_json.py [--build-type TYPE] <binary> ...",
            file=sys.stderr,
        )
        sys.exit(1)

    all_benchmarks = []
    with tempfile.TemporaryDirectory() as tmpdir:
        for binary in binaries:
            entries = _run_binary(binary, tmpdir)
            all_benchmarks.extend(entries)
            # Clear JSON files between runs so they don't bleed across.
            for fname in os.listdir(tmpdir):
                if fname.endswith(".json"):
                    os.unlink(os.path.join(tmpdir, fname))

    report = {
        "datetime": datetime.now(timezone.utc).isoformat(),
        "machine_info": _machine_info(),
        "commit_info": _commit_info(),
        "build": build_type,
        "benchmarks": all_benchmarks,
    }
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
