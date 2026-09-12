#!/usr/bin/env python3
"""Gate: ffi/rust/Cargo.lock's own version matches ffi/rust/Cargo.toml.

`Cargo.lock` records the resolved dependency graph, and its `[[package]]`
entry for doppler restates doppler's version. That copy is GENERATED -- cargo
rewrites it on the next build from `Cargo.toml` -- so it is not a version
site to be written, it is a derived file to be kept fresh.

Nothing kept it fresh. `bump-version` wrote the five declared sites and ran
`uv lock` for uv's lockfile, with no cargo equivalent, so `Cargo.lock` stated
**0.46.0** against a 0.47.0 project: one whole release behind, through a
release that shipped. It had no symptom because nothing READS it -- cargo just
regenerates it, which is how it surfaced at all (a `cargo metadata` inside a
`make gates` run rewrote the file and dirtied the tree mid-rebase).

This is the same shape the repo already fixed twice: `bootstrap.toml` frozen
at 0.3.7 and `just-makeit.toml` at 0.1.0, both "nothing read either, which is
why nothing noticed". The fix there was to declare them; the fix HERE is not,
because a lockfile must not be hand-edited -- `bump-version` now regenerates
it and this gate proves it happened.

Usage
-----
    python3 scripts/check_cargo_lock.py

Exit 0 when the two agree. The remedy it prints is the regeneration, never an
edit.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOML = ROOT / "ffi" / "rust" / "Cargo.toml"
LOCK = ROOT / "ffi" / "rust" / "Cargo.lock"


def _toml_version() -> str:
    """doppler's declared version, from the [package] table."""
    lines = TOML.read_text(encoding="utf-8").splitlines()
    start = next(
        (i for i, ln in enumerate(lines) if ln.strip() == "[package]"), None
    )
    if start is None:
        raise SystemExit(f"check_cargo_lock: {TOML.name}: no [package] table")
    for ln in lines[start + 1 :]:
        if ln.lstrip().startswith("["):
            break
        m = re.match(r'^version = "([^"]+)"', ln)
        if m:
            return m.group(1)
    raise SystemExit(f"check_cargo_lock: {TOML.name}: no version in [package]")


def _lock_version() -> str:
    """doppler's version as the lockfile records it.

    Scoped to the [[package]] block naming doppler: the file has one block
    per crate and every one of them states a version, so the header alone is
    not enough to find ours.
    """
    lines = LOCK.read_text(encoding="utf-8").splitlines()
    for i, ln in enumerate(lines):
        if ln.strip() != 'name = "doppler"':
            continue
        for nxt in lines[i + 1 :]:
            if nxt.lstrip().startswith("["):
                break
            m = re.match(r'^version = "([^"]+)"', nxt)
            if m:
                return m.group(1)
    raise SystemExit(
        f"check_cargo_lock: {LOCK.name}: no [[package]] block for doppler. "
        f"An absent entry is NOT a pass -- it would report OK while the "
        f"lockfile said nothing about our own crate."
    )


def main() -> int:
    for p in (TOML, LOCK):
        if not p.is_file():
            print(f"check_cargo_lock: {p} is missing", file=sys.stderr)
            return 2

    declared, locked = _toml_version(), _lock_version()
    if declared != locked:
        print(
            f"check_cargo_lock: Cargo.lock is STALE — it records doppler "
            f"{locked}, Cargo.toml declares {declared}.\n\n"
            f"  Do NOT edit the lockfile. Regenerate it:\n\n"
            f"      cd ffi/rust && cargo metadata --offline "
            f"--format-version 1 >/dev/null\n\n"
            f"  `make bump-version` now does this for you; a drift here "
            f"means it was bumped some other way."
        )
        return 1

    print(
        f"check_cargo_lock: OK — Cargo.lock and Cargo.toml agree ({declared})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
