#!/usr/bin/env python3
"""Run each validation report's ``--check`` and classify what came back.

``make validate-check`` used to be a shell loop that treated *any* non-zero
exit as STALE and then printed ``run 'make validate'``. That conflates two
unrelated failures, and the wrong one was the common one: in a fresh
worktree ``make build`` does not build the Python extensions, so every
validator dies on ``ModuleNotFoundError`` and all 21 reports read "stale".
Re-rendering a report cannot fix an import, so the advice sent the reader
the wrong way -- and the diff replay (``sed -n '/STALE:/,$p'``) found no
marker in a traceback, discarding the traceback that *was* the diagnosis.

Three outcomes, three messages (doppler#1074):

===================  ==================================================
exit status          meaning
===================  ==================================================
``0``                the committed report matches a fresh run
``EXIT_STALE`` (3)   it does not -- re-run ``make validate``
anything else        the validator never decided; its output is the
                     diagnosis, so it is replayed verbatim
===================  ==================================================

The validator list is passed in by the Makefile rather than rediscovered
here, so ``VALIDATORS`` stays the one declaration of what gets checked.

Not ``check_validation_reports.py``
-----------------------------------
That is a different gate on the same artifacts, and the names are close
enough to confuse: it asks whether a rendered ``results.md`` *says what it
claims* -- tables that parse, findings actually shown, figures that exist --
and it hangs off ``make lint``. This one asks only whether the committed
report still matches its generator, and it is ``make validate-check``.
Staleness is not correctness; a generator emitting a broken table agrees
with itself perfectly.

Usage
-----
    python scripts/validate_check.py <validate.py> [<validate.py> ...]
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

# Imported rather than restated: the harness decides what staleness exits
# with, and a second copy of the number here could drift from it silently.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "src"))
from doppler.tests._validation_common import EXIT_STALE

#: Lines of a crashed validator's output to replay. A traceback is the whole
#: diagnosis, so this is generous -- but not unbounded, because a validator
#: that fails midway can print a report first.
_ERROR_LINES = 40


def _run(validator: str) -> tuple[int, str]:
    """Run one validator's ``--check``, returning ``(status, output)``."""
    p = subprocess.run(
        [sys.executable, validator, "--check"],
        capture_output=True,
        text=True,
    )
    return p.returncode, p.stdout + p.stderr


def main(argv: list[str]) -> int:
    validators = argv[1:]
    if not validators:
        print("check_validation_reports: no validators given", file=sys.stderr)
        return 2

    stale: list[str] = []
    broken: list[str] = []

    for v in validators:
        status, out = _run(v)
        if status == 0:
            continue
        if status == EXIT_STALE:
            stale.append(v)
            print(f"validate-check: STALE — {v}")
            # From the marker onward, so the per-limit PASS lines stay out.
            tail = out.split("STALE:", 1)
            if len(tail) == 2:
                print("  STALE:" + tail[1].rstrip())
        else:
            broken.append(v)
            print(f"validate-check: ERROR — {v} could not run (exit {status})")
            lines = out.rstrip().splitlines()
            for line in lines[-_ERROR_LINES:]:
                print(f"    {line}")
            if len(lines) > _ERROR_LINES:
                print(f"    … {len(lines) - _ERROR_LINES} earlier line(s)")

    if not stale and not broken:
        print(f"validate-check: OK — {len(validators)} report(s) up to date")
        return 0

    # Two different remedies. Naming both when only one applies is how the
    # old message sent a reader to `make validate` for an import error.
    if stale:
        print(f"validate-check: {len(stale)} stale — run 'make validate'")
    if broken:
        print(
            f"validate-check: {len(broken)} could not run — read the output "
            "above. If it is an import error, the extensions are missing: "
            "'make build' does not build them, 'make pyext' does."
        )
    return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
