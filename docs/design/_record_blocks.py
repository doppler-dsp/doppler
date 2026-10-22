"""Render a measurement record's tables from committed data, and check them.

A record under ``docs/design/`` that quotes a run keeps the run's data
beside it and a script that renders every one of its tables between
markers::

    <!-- <prefix>:<block>:start -->

    | ... |

    <!-- <prefix>:<block>:end -->

A run may keep blocks on more than one page (``main_pages``): a design
page that quotes a record's numbers holds them as blocks of the same run.
``--write`` rewrites the blocks; ``--check`` (a line in the Makefile's
``DOCS_CHECK_PRE_CMDS``, so in ``make docs-invariants`` and
``make docs-check``) fails when one differs from what the data renders,
cell by cell, so mdformat's column padding is not a difference. A cell is
the data's output, never an edit.

Three records render this way: the Spectrogram's
(``spectrogram-measurements/``, its runs through ``_record.py``), the
parallel receivers' (``async-dsss-parallel-measurements/record.py``) and
the acquisition surface's (``acq-surface-measurements/record.py``). This
module is what they share, so they cannot disagree on what a block is, how
one is checked, or how a run is stamped (``run_stamp``, ``toolchain``,
``require_toolchain``).
"""

from __future__ import annotations

import argparse
import datetime
import os
import platform
import re
import subprocess
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    from collections.abc import Callable, Sequence
    from pathlib import Path


# ── a run's stamp ─────────────────────────────────────────────────────────
#
# What every record writes beside its data, and refuses to render without:
# where and when a run was taken, from which commit, and with which
# toolchain. One home, so two records cannot stamp a run two ways.


def git(root: Path, *args: str) -> str:
    """`git -C root args`, its output stripped."""
    return subprocess.run(
        ["git", "-C", str(root), *args],
        capture_output=True,
        text=True,
        check=True,
    ).stdout.strip()


def cpu_model() -> str:
    """The CPU's model name, from /proc/cpuinfo where there is one."""
    try:
        with open("/proc/cpuinfo", encoding="utf-8") as f:
            for line in f:
                if line.startswith("model name"):
                    return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor() or "unknown"


def load_avg() -> list[float]:
    """The 1, 5 and 15 minute load averages, to two places."""
    return [round(v, 2) for v in os.getloadavg()]


#: The stamp schema a record writes. Schema 2 stamps carry the toolchain; a
#: schema 2 stamp without one is refused (load), so a stamp cannot silently
#: lose the field the validator's numbers depend on. Schema 1 stamps (the
#: committed data before this field) render the toolchain as "—".
STAMP_SCHEMA = 2


#: The fields a schema-2 toolchain stamp carries, each non-empty.
TOOLCHAIN_FIELDS = ("compiler", "libc", "build_type", "c_flags", "native")


def cmake_cache(build: Path) -> dict[str, str]:
    """`build/CMakeCache.txt` as name -> value, the `:TYPE` suffix dropped."""
    cache: dict[str, str] = {}
    text = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    for line in text.splitlines():
        if line and not line.startswith(("#", "//")) and "=" in line:
            key, _, value = line.partition("=")
            cache[key.split(":")[0]] = value
    return cache


def libc() -> str:
    """The running C library, e.g. "glibc 2.41"; "unknown" where none is
    named.

    `os.confstr` is POSIX-only: Windows has no `os.confstr`, and macOS has it
    but refuses the GNU name (ValueError). It reports the C library running
    now, unlike `platform.libc_ver()`, which reads the version the
    interpreter was linked against.
    """
    try:
        return os.confstr("CS_GNU_LIBC_VERSION") or "unknown"
    except (AttributeError, ValueError, OSError):
        return "unknown"


def toolchain(build: Path) -> dict[str, str]:
    r"""The toolchain the build used, read from what CMake recorded.

    The validator's numbers are toolchain-bound (the same commit gives
    different rows under another compiler or C library), so a stamp records
    the compiler the build was configured with, the C library, the build
    type and the flags that type compiles with, and whether `DOPPLER_NATIVE`
    added `-march=native` (CMakeLists.txt; it moves the numbers). A re-run
    on another box then reads as "a different toolchain", not as drift.

    The compiler and its version come from the compiler CMake detected
    (`CMakeFiles/<cmake version>/CMakeCCompiler.cmake`), not from running
    `cc --version` at stamp time, which can name a compiler the build never
    used. The flags are `CMAKE_C_FLAGS` plus `CMAKE_C_FLAGS_<BUILD_TYPE>`:
    the release flags alone carry `-O3`.

    Examples
    --------
    >>> import tempfile
    >>> b = Path(tempfile.mkdtemp())
    >>> _ = (b / "CMakeCache.txt").write_text(
    ...     "CMAKE_CACHE_MAJOR_VERSION:INTERNAL=3\n"
    ...     "CMAKE_CACHE_MINOR_VERSION:INTERNAL=31\n"
    ...     "CMAKE_CACHE_PATCH_VERSION:INTERNAL=6\n"
    ...     "CMAKE_C_COMPILER:FILEPATH=/usr/bin/cc\n"
    ...     "CMAKE_BUILD_TYPE:STRING=Release\n"
    ...     "CMAKE_C_FLAGS:STRING=\n"
    ...     "CMAKE_C_FLAGS_RELEASE:STRING=-O3 -DNDEBUG\n"
    ...     "DOPPLER_NATIVE:BOOL=OFF\n",
    ...     encoding="utf-8",
    ... )
    >>> d = b / "CMakeFiles" / "3.31.6"
    >>> d.mkdir(parents=True)
    >>> ids = 'set(CMAKE_C_COMPILER_ID "GNU")\n'
    >>> ver = 'set(CMAKE_C_COMPILER_VERSION "14.2.0")\n'
    >>> f = d / "CMakeCCompiler.cmake"
    >>> _ = f.write_text(ids + ver, encoding="utf-8")
    >>> tc = toolchain(b)
    >>> tc["compiler"], tc["build_type"], tc["c_flags"], tc["native"]
    ('GNU 14.2.0', 'Release', '-O3 -DNDEBUG', 'OFF')
    """
    cache = cmake_cache(build)
    cmake = ".".join(
        cache[f"CMAKE_CACHE_{part}_VERSION"]
        for part in ("MAJOR", "MINOR", "PATCH")
    )
    detected = dict(
        re.findall(
            r'^set\((CMAKE_C_COMPILER_(?:ID|VERSION)) "([^"]*)"\)',
            (build / "CMakeFiles" / cmake / "CMakeCCompiler.cmake").read_text(
                encoding="utf-8"
            ),
            re.MULTILINE,
        )
    )
    build_type = cache.get("CMAKE_BUILD_TYPE") or "default"
    flags = " ".join(
        f
        for f in (
            cache.get("CMAKE_C_FLAGS", "").strip(),
            cache.get(f"CMAKE_C_FLAGS_{build_type.upper()}", "").strip(),
        )
        if f
    )
    return {
        "compiler": f"{detected['CMAKE_C_COMPILER_ID']} "
        f"{detected['CMAKE_C_COMPILER_VERSION']}",
        "compiler_path": cache.get("CMAKE_C_COMPILER", "cc"),
        "libc": libc(),
        "build_type": build_type,
        "c_flags": flags or "default",
        "native": cache.get("DOPPLER_NATIVE") or "OFF",
    }


def tc_cell(stamp: dict[str, Any]) -> str:
    """A stamp's toolchain as one cell; "—" for a stamp that predates it."""
    tc = stamp.get("toolchain")
    if tc is None:
        return "—"
    return (
        f"{tc['compiler']}; {tc['libc']}; build {tc['build_type']}, "
        f"flags {tc['c_flags']}, native {tc['native']}"
    )


def run_stamp(root: Path, validator: Path, source: Path) -> dict[str, Any]:
    """The stamp of a run of `validator` about to start, from `root`'s
    tree: refused when the binary is absent or older than `source`."""
    if not validator.exists():
        raise SystemExit(
            f"{validator.name} is not built: make build "
            f"BUILD_TARGET={validator.name}"
        )
    if validator.stat().st_mtime < source.stat().st_mtime:
        raise SystemExit(f"{validator.name} is older than its source: rebuild")
    # The tree as the run found it, before anything is written: the run's
    # outputs are themselves tracked, so read after the write it would call
    # its own run dirty.
    return {
        "stamp_schema": STAMP_SCHEMA,
        "date_utc": datetime.datetime.now(datetime.timezone.utc).strftime(
            "%Y-%m-%d %H:%M"
        ),
        "machine": platform.node(),
        "cpu_model": cpu_model(),
        "cpu_threads": os.cpu_count(),
        "commit": git(root, "rev-parse", "--short=9", "HEAD"),
        "dirty": bool(
            git(root, "status", "--porcelain", "--untracked-files=no")
        ),
        "load_before": load_avg(),
        "toolchain": toolchain(root / "build"),
    }


def require_toolchain(name: str, stamp: dict[str, Any]) -> None:
    """Refuse a schema-2 stamp that lacks the toolchain it must carry.

    A stamp of schema 1 predates the field and renders "—" (_tc); a stamp
    that claims schema 2 without its toolchain is a writer that dropped it,
    and --check fails on it rather than printing a row no reader can date.
    """
    if stamp.get("stamp_schema", 1) < 2:
        return
    tc = stamp.get("toolchain")
    # Falsy, not absent: `"toolchain": null` or an empty field is the same
    # dropped field, and would render "—" or a blank cell silently.
    if not isinstance(tc, dict) or not all(
        tc.get(k) for k in TOOLCHAIN_FIELDS
    ):
        raise SystemExit(
            f"{name}: a schema-{stamp['stamp_schema']} stamp without its "
            f"full toolchain ({', '.join(TOOLCHAIN_FIELDS)}): the writer "
            "dropped a field; re-run the tool"
        )


def table(head: Sequence[str], rows: Sequence[Sequence[str]]) -> str:
    """A markdown table: the header, the rule, the rows."""
    out = ["| " + " | ".join(head) + " |"]
    out.append("| " + " | ".join("---" for _ in head) + " |")
    out += ["| " + " | ".join(r) + " |" for r in rows]
    return "\n".join(out)


def span(prefix: str, name: str) -> re.Pattern[str]:
    """The block ``name`` of a record's ``prefix`` markers."""
    return re.compile(
        rf"(<!-- {prefix}:{name}:start -->)(.*?)"
        rf"(<!-- {prefix}:{name}:end -->)",
        re.S,
    )


def cells(text: str) -> list[list[str]]:
    """A block's table rows as stripped cells; padding and dashes ignored."""
    rows = []
    for line in text.strip().splitlines():
        if not line.strip():
            continue
        row = [c.strip() for c in line.strip().strip("|").split("|")]
        rows.append([re.sub(r"^:?-+:?$", "---", c) for c in row])
    return rows


def _sync(
    record: Path, prefix: str, label: str, rendered: dict[str, str]
) -> tuple[str, list[str]]:
    """``record``'s text with every block replaced by its rendering, and the
    names of the blocks whose cells differed."""
    text = record.read_text(encoding="utf-8")
    stale = []
    for name, body in rendered.items():
        m = span(prefix, name).search(text)
        if m is None:
            raise SystemExit(f"{label}: no {name} markers in {record.name}")
        if cells(m.group(2)) != cells(body):
            stale.append(name)
        text = span(prefix, name).sub(
            lambda m, b=body: m.group(1) + "\n\n" + b + "\n\n" + m.group(3),
            text,
        )
    return text, stale


def main_pages(
    prefix: str,
    label: str,
    script: str,
    render: Callable[[], dict[Path, dict[str, str]]],
    doc: str,
    argv: Sequence[str] | None = None,
) -> int:
    """Print, ``--write`` or ``--check`` one run's blocks across pages.

    ``render`` is called after the arguments parse and returns, per page,
    every block's body by name: a run whose numbers a design page quotes
    keeps them there as blocks too, so a re-run cannot leave the page
    stale. ``label`` names the run in the messages and ``script`` is what a
    stale block is told to run.
    """
    ap = argparse.ArgumentParser(description=doc.splitlines()[0])
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    a = ap.parse_args(argv)
    pages = render()
    n = sum(len(r) for r in pages.values())
    if not (a.write or a.check):
        for page, rendered in pages.items():
            for name, body in rendered.items():
                print(f"== {page.name}: {name}\n{body}\n")
        return 0
    stale: list[str] = []
    texts = {}
    for page, rendered in pages.items():
        texts[page], bad = _sync(page, prefix, label, rendered)
        stale += [f"{page.name}'s {b}" for b in bad]
    if a.write:
        for page, text in texts.items():
            page.write_text(text, encoding="utf-8")
        names = ", ".join(p.name for p in pages)
        print(f"{label}: wrote {n} table block(s) in {names}")
        return 0
    if stale:
        print(
            f"{label}: {', '.join(stale)} table(s) are not what the run's "
            f"data renders. Run `python {script} --write`; a cell is the "
            f"data's output, never an edit."
        )
        return 1
    print(f"{label}: OK — {n} table block(s) match the run's data")
    return 0


def main(
    record: Path,
    prefix: str,
    label: str,
    script: str,
    render: Callable[[], dict[str, str]],
    doc: str,
    argv: Sequence[str] | None = None,
) -> int:
    """``main_pages`` for a run that keeps its blocks on one page."""
    return main_pages(
        prefix, label, script, lambda: {record: render()}, doc, argv
    )
