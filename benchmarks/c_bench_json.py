#!/usr/bin/env python3
"""
Run C benchmark binaries and emit a JSON report.

Top-level keys match pytest-benchmark output (datetime, machine_info,
commit_info, benchmarks).  Per-benchmark keys also match: name,
extra_info, stats.  stats contains MSa_s instead of timing aggregates,
but the key structure is otherwise identical.

Usage:
    python benchmarks/c_bench_json.py [--build-type Release] <binary> ...
"""

import json
import os
import platform
import re
import subprocess
import sys
from datetime import datetime, timezone


# ---------------------------------------------------------------------------
# Environment
# ---------------------------------------------------------------------------

def _machine_info():
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
                ["git", "status", "--porcelain"], stderr=subprocess.DEVNULL
            ).decode().strip()
        )
        return {"id": sha, "branch": branch, "dirty": dirty}
    except Exception:
        return {}


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------

def _bench_name(path):
    """acc_cf64 from .../bench_acc_cf64_core."""
    n = os.path.basename(path)
    n = re.sub(r'^bench_', '', n)
    n = re.sub(r'_core$', '', n)
    return n


def _parse_output(name, text):
    """Return a list of benchmark entry dicts from one binary's stdout."""
    if "(no step() generated" in text:
        return []

    entries = []

    # Collect any metadata from header lines (taps, filter description, etc.)
    header_meta = {}
    for line in text.splitlines():
        m = re.match(r'taps\s*=\s*(\d+)', line)
        if m:
            header_meta["taps"] = int(m.group(1))

    # Split into rate sub-sections if the binary uses "rate = X.XXXX" headings
    rate_pattern = re.compile(r'^rate\s*=\s*(\S+)', re.MULTILINE)
    rates = rate_pattern.findall(text)
    if rates:
        chunks = re.split(r'^rate\s*=\s*\S+\n', text, flags=re.MULTILINE)
        sections = list(zip(rates, chunks[1:]))
    else:
        sections = [(None, text)]

    for rate, section in sections:
        # Find the column-header line (must contain "block" and "iters")
        col_line = None
        for line in section.splitlines():
            if re.search(r'block\s+iters', line):
                col_line = line
                break
        if col_line is None:
            continue

        # Derive data-column names from the header (everything after iters)
        raw_cols = re.split(r'\s{2,}', col_line.strip())
        # raw_cols[0]='block', raw_cols[1]='iters', raw_cols[2:]= data cols
        data_cols = raw_cols[2:]

        # Find the separator line then collect data rows after it
        lines = section.splitlines()
        data_start = None
        for i, line in enumerate(lines):
            if re.match(r'\s*-+\s+-+', line):
                data_start = i + 1
                break
        if data_start is None:
            continue

        for line in lines[data_start:]:
            # Strip annotation comments like (sink=N)
            line = re.sub(r'\(\w+=\d+\)', '', line).strip()
            if not line:
                continue

            msa_vals = re.findall(r'(\d+(?:\.\d+)?)\s+MSa\b', line)
            all_nums = re.findall(r'\d+(?:\.\d+)?', line)
            if len(all_nums) < 2 or not msa_vals:
                continue

            try:
                block = int(all_nums[0])
                iters = int(all_nums[1])
            except ValueError:
                continue

            extra = {**header_meta, "block": block, "iters": iters}
            if rate is not None:
                extra["rate"] = float(rate)

            # One unnamed column → single entry; named columns → one entry each
            if len(msa_vals) == 1:
                param = f"block={block}"
                if rate is not None:
                    param = f"rate={rate},block={block}"
                entries.append({
                    "name": f"{name}[{param}]",
                    "extra_info": extra,
                    "stats": {"MSa_s": float(msa_vals[0])},
                })
            else:
                for col, msa in zip(data_cols, msa_vals):
                    col = col.strip()
                    entries.append({
                        "name": f"{name}[variant={col},block={block}]",
                        "extra_info": extra,
                        "stats": {"MSa_s": float(msa)},
                    })

    return entries


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

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
    for binary in binaries:
        result = subprocess.run(
            [binary], capture_output=True, text=True, timeout=300
        )
        entries = _parse_output(_bench_name(binary), result.stdout)
        all_benchmarks.extend(entries)

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
