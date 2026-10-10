#!/usr/bin/env python3
"""Gate: the vendored nats.c keeps doppler's options after a compiler change.

CMakeLists.txt builds `vendor/nats.c` with a custom command that configures
`<build>/libnats-vendor` with `-DCMAKE_C_COMPILER=...` plus doppler's own
options (`NATS_BUILD_STREAMING=OFF`, `NATS_BUILD_WITH_TLS=OFF`, ...). When
that directory already holds a cache naming a DIFFERENT compiler, CMake
deletes the cache and re-runs configure WITHOUT the other `-D` values. Then
nats.c's own defaults win: streaming ON (wants protobuf-c), TLS ON (wants
OpenSSL), examples ON. The first build after a compiler change failed, and
the second passed (#1936). The fix configures from an empty directory.

**Why this needs its own gate.** Every CI leg configures a fresh tree, so
the vendor directory never exists beforehand and the fix is a no-op there:
a green build proves nothing. This gate makes the stale state on purpose:

1. Configure `vendor/nats.c` alone into `<dir>/libnats-vendor`, with the
   SAME compiler reached through a symlink, so the path CMake records
   differs from the one the parent passes.
2. Run the real `make build BUILD_DIR=<dir> BUILD_TARGET=libnats_vendor`.
3. Read `<dir>/libnats-vendor/CMakeCache.txt` and require every
   `-DNAME=VALUE` the custom command passes. The expected values are
   parsed from CMakeLists.txt, not restated here.

**The cache, not the exit code.** With protobuf-c and OpenSSL installed,
the old bug builds fine, with streaming and TLS quietly ON. Only the cache
says which options the configure saw.

Usage:  python3 scripts/check_nats_vendor_fresh.py --build-dir DIR
            [--make MAKE] [--cmake CMAKE]
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VENDOR = "vendor/nats.c"
# The custom command, from its OUTPUT line to its COMMENT.
COMMAND = re.compile(
    r'add_custom_command\(OUTPUT "\$\{_NATS_LIB\}"(?P<body>.*?)COMMENT', re.S
)
# A -D with a literal value; ${...} values (the compiler, the generator) are
# the parent's and vary by machine, so they are not expectations.
DEFINE = re.compile(r"-D(?P<name>\w+)=(?P<value>[^\s${}]+)(?=\s)")


def expected_options(cmakelists: str) -> dict[str, str]:
    """NAME -> VALUE for each literal -D the vendor configure passes."""
    m = COMMAND.search(cmakelists)
    if m is None:
        raise SystemExit("check: no nats.c custom command in CMakeLists.txt")
    found = {d["name"]: d["value"] for d in DEFINE.finditer(m["body"])}
    if not found:
        raise SystemExit("check: the custom command passes no -D options")
    return found


def cache_values(cache: str) -> dict[str, str]:
    """NAME -> VALUE from a CMakeCache.txt (`NAME:TYPE=VALUE` lines)."""
    out: dict[str, str] = {}
    for line in cache.splitlines():
        m = re.match(r"([A-Za-z_]\w*):\w+=(.*)$", line)
        if m:
            out[m[1]] = m[2]
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build-dir", type=Path, required=True)
    ap.add_argument("--make", default="make")
    ap.add_argument("--cmake", default="cmake")
    a = ap.parse_args()
    build = a.build_dir if a.build_dir.is_absolute() else ROOT / a.build_dir

    shutil.rmtree(build, ignore_errors=True)
    (build / "alias").mkdir(parents=True)
    compiler = shutil.which(os.environ.get("CC", "cc"))
    if compiler is None:
        raise SystemExit("check: no C compiler (cc, or $CC) on PATH")
    alias = build / "alias" / Path(compiler).name
    alias.symlink_to(Path(compiler).resolve())

    # 1. The stale cache: nats.c configured alone, by another compiler path.
    # STREAMING and TLS off so this configure needs neither protobuf-c nor
    # OpenSSL; the gate is about what survives the NEXT configure.
    seed = subprocess.run(
        [
            a.cmake,
            "-S",
            str(ROOT / VENDOR),
            "-B",
            str(build / "libnats-vendor"),
            f"-DCMAKE_C_COMPILER={alias}",
            "-DNATS_BUILD_STREAMING=OFF",
            "-DNATS_BUILD_WITH_TLS=OFF",
        ],
        capture_output=True,
        text=True,
    )
    if seed.returncode != 0:
        print(seed.stdout[-2000:] + seed.stderr[-2000:])
        raise SystemExit("check: could not seed the stale vendor cache")

    # 2. The real build, through the target everyone runs.
    run = subprocess.run(
        [
            *a.make.split(),
            "--no-print-directory",
            "build",
            f"BUILD_DIR={build}",
            "BUILD_TARGET=libnats_vendor",
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )

    # 3. What the vendor configure actually saw.
    want = expected_options((ROOT / "CMakeLists.txt").read_text("utf-8"))
    cache_file = build / "libnats-vendor" / "CMakeCache.txt"
    have = cache_values(
        cache_file.read_text("utf-8") if cache_file.exists() else ""
    )
    wrong = {
        k: (v, have.get(k, "(absent)"))
        for k, v in want.items()
        if have.get(k) != v
    }
    if run.returncode == 0 and not wrong:
        print(
            "nats-vendor-fresh: after a compiler change the vendor kept all "
            f"{len(want)} of doppler's options"
        )
        shutil.rmtree(build, ignore_errors=True)
        return 0
    print(
        "nats-vendor-fresh: the vendored nats.c lost doppler's options "
        "after a compiler change (#1936) --"
    )
    if run.returncode != 0:
        print(f"  make build exited {run.returncode}:")
        print("  " + "\n  ".join((run.stdout + run.stderr).splitlines()[-12:]))
    for k, (v, got) in sorted(wrong.items()):
        print(f"  {k}: want {v}, the cache has {got}")
    print(
        "  CMake deletes a cache that names another compiler and re-runs\n"
        "  configure WITHOUT the custom command's other -D options. The\n"
        "  custom command must configure the vendor from an empty directory."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
