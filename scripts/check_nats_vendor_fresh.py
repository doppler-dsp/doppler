#!/usr/bin/env python3
"""Gate: the vendored nats.c keeps doppler's options after a compiler change.

CMakeLists.txt builds `vendor/nats.c` with a custom command that configures
`<build>/libnats-vendor` with `_NATS_CONFIGURE_ARGS`: the parent's compiler
plus doppler's own options (`NATS_BUILD_STREAMING=OFF`, ...). When that
directory already holds a cache naming a DIFFERENT compiler, CMake deletes
the cache and re-runs configure WITHOUT the other `-D` values. Then nats.c's
own defaults win: streaming ON (wants protobuf-c), TLS ON (wants OpenSSL),
examples ON. The first build after a compiler change failed, and the second
passed (#1936). The fix configures from an empty directory.

**Why this needs its own gate.** Every CI leg configures a fresh tree, so
the vendor directory never exists beforehand and the fix is a no-op there:
a green build proves nothing. This gate makes the stale state on purpose:

1. Configure `vendor/nats.c` alone into `<dir>/libnats-vendor`, with the
   SAME compiler reached through a symlink, so the path CMake records
   differs from the one the parent passes.
2. **Positive control.** Seed a second, identical directory and
   reconfigure it with the real compiler path plus a sentinel `-D`. The
   sentinel must be ABSENT afterwards, so this CMake really drops options
   on a compiler change. Otherwise the gate fails as "premise not
   reproduced": it would be testing nothing.
3. Run `make build BUILD_DIR=<dir> BUILD_TARGET=libnats_vendor`.
4. Read `<dir>/libnats-vendor/CMakeCache.txt` and require every literal
   `-DNAME=VALUE` in `_NATS_CONFIGURE_ARGS`, parsed from CMakeLists.txt.
   Every `-D` there must be accounted for (parsed, or `${...}`-valued), so
   a spelling the parser does not know fails rather than drops out.

**The cache, not the exit code.** With protobuf-c and OpenSSL installed,
the old bug builds fine, with streaming and TLS quietly ON.

**What it may delete.** Only a directory it CREATED, which it marks, that
is strictly inside the repository and is not the main build tree. A
`--build-dir` of `.`, `build`, a path outside the checkout, or any existing
unmarked directory is refused before anything is touched.

Linux only: CI runs it on the Linux legs, and the symlinked-compiler seed
is a POSIX construction. It skips elsewhere, saying so.

Usage:  python3 scripts/check_nats_vendor_fresh.py --build-dir DIR
            [--protect DIR] [--root DIR] [--cmake CMAKE]
"""

from __future__ import annotations

import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VENDOR = "vendor/nats.c"
MARKER = ".nats-vendor-fresh"
SENTINEL = "DOPPLER_PREMISE_SENTINEL"
LAUNCHERS = frozenset({"ccache", "sccache", "distcc", "icecc"})
ARGS = re.compile(r"set\(_NATS_CONFIGURE_ARGS(?P<body>.*?)\)\n", re.S)
# A literal -DNAME=VALUE; a ${...} value is the parent's and varies.
LITERAL = re.compile(r"-D(?P<name>\w+)=(?P<value>[^\s${}()]+)")
FROM_PARENT = re.compile(r"-D\w+=\$\{")
ANY_D = re.compile(r"(?<![\w-])-D")


class RefusedError(Exception):
    """The gate will not run; nothing has been touched."""


def expected_options(cmakelists: str) -> dict[str, str]:
    """NAME -> VALUE for each literal -D in `_NATS_CONFIGURE_ARGS`.

    Every -D must be a literal one or a ${...}-valued one. Any other
    spelling (`-DNAME:BOOL=OFF`, `-D NAME=...`) raises rather than drop out
    of the check.
    """
    m = ARGS.search(cmakelists)
    if m is None:
        raise RefusedError(
            "no set(_NATS_CONFIGURE_ARGS ...) in CMakeLists.txt"
        )
    body = "\n".join(ln.split("#", 1)[0] for ln in m["body"].splitlines())
    literal = {d["name"]: d["value"] for d in LITERAL.finditer(body)}
    parent = len(FROM_PARENT.findall(body))
    total = len(ANY_D.findall(body))
    if not literal or len(literal) + parent != total:
        raise RefusedError(
            f"_NATS_CONFIGURE_ARGS has {total} -D option(s) but {len(literal)}"
            f" literal and {parent} ${{...}}-valued were understood -- a -D "
            "spelled another way would drop out of the check"
        )
    return literal


def cache_values(cache: str) -> dict[str, str]:
    """NAME -> VALUE from a CMakeCache.txt (`NAME:TYPE=VALUE` lines)."""
    out: dict[str, str] = {}
    for line in cache.splitlines():
        m = re.match(r"([A-Za-z_]\w*):\w+=(.*)$", line)
        if m:
            out[m[1]] = m[2]
    return out


def safe_dir(root: Path, build: Path, protect: list[Path]) -> Path:
    """The build dir, resolved, if the gate may own it; else RefusedError."""
    root = root.resolve()
    build = (build if build.is_absolute() else root / build).resolve()
    if root not in build.parents:
        raise RefusedError(f"{build} is not strictly inside {root}")
    for p in protect:
        p = (p if p.is_absolute() else root / p).resolve()
        if build == p or build in p.parents or p in build.parents:
            raise RefusedError(
                f"{build} is, holds or is in a protected tree ({p})"
            )
    if build.exists() and not (build / MARKER).exists():
        raise RefusedError(f"{build} exists and was not made by this gate")
    return build


def compiler() -> Path:
    """The C compiler CMake will pick, past any launcher in $CC."""
    words = shlex.split(os.environ.get("CC", "cc"))
    while words and Path(words[0]).name in LAUNCHERS:
        words = words[1:]
    found = shutil.which(words[0]) if words else None
    if found is None:
        raise RefusedError(
            f"no C compiler from CC={os.environ.get('CC', 'cc')!r}"
        )
    return Path(found)


def seed(cmake: str, root: Path, into: Path, cc: Path) -> None:
    """nats.c configured alone, into `into`, by the compiler at `cc`."""
    run = subprocess.run(
        [
            cmake,
            "-S",
            str(root / VENDOR),
            "-B",
            str(into),
            f"-DCMAKE_C_COMPILER={cc}",
            # off, so this configure needs neither protobuf-c nor OpenSSL
            "-DNATS_BUILD_STREAMING=OFF",
            "-DNATS_BUILD_WITH_TLS=OFF",
        ],
        capture_output=True,
        text=True,
    )
    if run.returncode != 0:
        print(run.stdout[-2000:] + run.stderr[-2000:])
        raise SystemExit("nats-vendor-fresh: could not seed a vendor cache")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build-dir", type=Path, required=True)
    ap.add_argument("--protect", type=Path, action="append", default=[])
    # --root exists for this gate's OWN test: it points the path checks at
    # a throwaway tree, so a refusal can be proven without risking this one.
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--cmake", default="cmake")
    a = ap.parse_args()

    if not sys.platform.startswith("linux"):
        print(
            f"nats-vendor-fresh: skipped on {sys.platform}: the seed is a "
            "POSIX construction, and CI runs this gate on its Linux legs"
        )
        return 0
    try:
        build = safe_dir(a.root, a.build_dir, a.protect)
        want = expected_options(
            (a.root / "CMakeLists.txt").read_text(encoding="utf-8")
        )
        cc = compiler()
    except RefusedError as e:
        print(f"nats-vendor-fresh: refused, nothing touched -- {e}")
        return 2

    if build.exists():
        shutil.rmtree(build)  # ours: marked, inside the repo, unprotected
    (build / "alias").mkdir(parents=True)
    (build / MARKER).write_text("made by check_nats_vendor_fresh.py\n")
    alias = build / "alias" / cc.name
    alias.symlink_to(cc.resolve())

    # 2. The positive control: does THIS cmake drop options on a compiler
    # change? Same seed, then the real path plus a sentinel.
    seed(a.cmake, a.root, build / "control", alias)
    subprocess.run(
        [
            a.cmake,
            "-S",
            str(a.root / VENDOR),
            "-B",
            str(build / "control"),
            f"-DCMAKE_C_COMPILER={cc}",
            f"-D{SENTINEL}=1",
            "-DNATS_BUILD_STREAMING=OFF",
            "-DNATS_BUILD_WITH_TLS=OFF",
        ],
        capture_output=True,
        text=True,
    )
    control = build / "control" / "CMakeCache.txt"
    kept = SENTINEL in cache_values(
        control.read_text(encoding="utf-8") if control.exists() else ""
    )
    if kept:
        print(
            "nats-vendor-fresh: premise not reproduced -- this cmake kept a "
            "-D across a compiler change, so the stale state #1936 needs "
            "was not made and the check below would prove nothing"
        )
        return 1

    # 1 and 3. The stale vendor cache, then the real build. `-o
    # compile_commands.json`: the root link is not this gate's to re-point.
    seed(a.cmake, a.root, build / "libnats-vendor", alias)
    run = subprocess.run(
        [
            "make",
            "--no-print-directory",
            "-o",
            "compile_commands.json",
            "build",
            f"BUILD_DIR={build}",
            "BUILD_TARGET=libnats_vendor",
        ],
        cwd=a.root,
        capture_output=True,
        text=True,
    )

    # 4. What the vendor configure actually saw.
    cache_file = build / "libnats-vendor" / "CMakeCache.txt"
    have = cache_values(
        cache_file.read_text(encoding="utf-8") if cache_file.exists() else ""
    )
    wrong = {
        k: (v, have.get(k, "(absent)"))
        for k, v in want.items()
        if have.get(k) != v
    }
    if run.returncode == 0 and not wrong:
        print(
            "nats-vendor-fresh: after a compiler change the vendor kept all "
            f"{len(want)} of doppler's options (premise reproduced first)"
        )
        shutil.rmtree(build)
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
