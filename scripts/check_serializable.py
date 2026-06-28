#!/usr/bin/env python3
"""Fail when an object hasn't declared its state-serialization stance.

State serialization (``dp_state.h``) lets every stateful object checkpoint /
migrate / resume bit-for-bit. The risk is silent rot: someone adds a new
stateful object and forgets the serializer, so the elastic guarantee quietly
stops being universal.

This gate makes the stance **mandatory and explicit**. Every object declared in
``objects/*.toml`` must resolve to exactly one of:

* ``serializable = "true"`` in its TOML — it carries running state and
  implements the triplet (jm generates the Python binding; see
  ``docs/design/state-serialization.md``).
* listed in ``scripts/.serializable-stateless`` — it has no resumable state
  (pure converter, FFT plan, by-value analyzer). A reviewed, permanent opt-out.
  (Kept in a sidecar file, not the TOML, because the manifest dumper only
  round-trips keys it knows about.)

An object resolving to neither fails CI — unless it is still listed in the
burn-down file ``scripts/.serializable-ignore`` (stateful, not yet done). The
burn-down list shrinks as objects are completed; when it is empty the invariant
holds for good and no new object can skip the choice.

Both sidecar files are one object name per line, ``#`` comments allowed.

Usage
-----
    python scripts/check_serializable.py          # report + exit 1 on any gap
    python scripts/check_serializable.py --list   # print undeclared objects
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
OBJECTS = ROOT / "objects"
IGNORE = HERE / ".serializable-ignore"
STATELESS = HERE / ".serializable-stateless"

_SERIALIZABLE = re.compile(r'^\s*serializable\s*=\s*"true"', re.MULTILINE)


def _is_serializable(toml_text: str) -> bool:
    return bool(_SERIALIZABLE.search(toml_text))


def _load_list(path: Path) -> set[str]:
    if not path.exists():
        return set()
    out: set[str] = set()
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            out.add(line)
    return out


def undeclared() -> list[str]:
    """Objects that are not serializable=true in their TOML."""
    out: list[str] = []
    for toml in sorted(OBJECTS.glob("*.toml")):
        if not _is_serializable(toml.read_text(encoding="utf-8")):
            out.append(toml.stem)
    return out


def main(argv: list[str]) -> int:
    gaps = set(undeclared())
    if "--list" in argv:
        for g in sorted(gaps):
            print(g)
        return 0

    stateless = _load_list(STATELESS)
    ignore = _load_list(IGNORE)

    offenders = sorted(gaps - stateless - ignore)
    # Hygiene: a burn-down entry that is now serializable (left the gap set) or
    # has been opted out as stateless is stale and should be removed.
    stale_ignore = sorted((ignore - gaps) | (ignore & stateless))

    rc = 0
    if not offenders and not stale_ignore:
        total = len(list(OBJECTS.glob("*.toml")))
        ser = total - len(gaps)
        msg = (
            f"Serialization stance: OK — {ser} serializable, "
            f"{len(stateless & gaps)} stateless"
        )
        if ignore:
            msg += f", {len(ignore & gaps)} on the burn-down list"
        print(msg)
        return 0

    if offenders:
        rc = 1
        print(
            "Serialization stance: objects with no declared stance:\n",
            file=sys.stderr,
        )
        for o in offenders:
            print(f"  {o}", file=sys.stderr)
        print(
            '\nEither add serializable = "true" to objects/<name>.toml and '
            "implement the dp_state.h triplet, or add <name> to "
            "scripts/.serializable-stateless (no resumable state). During "
            "the rollout, scripts/.serializable-ignore holds the not-yet-done "
            "set.",
            file=sys.stderr,
        )
    if stale_ignore:
        rc = 1
        print(
            "\nStale scripts/.serializable-ignore entries (now resolved — "
            "remove them):",
            file=sys.stderr,
        )
        for s in stale_ignore:
            print(f"  {s}", file=sys.stderr)
    return rc


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
