#!/usr/bin/env python3
"""The supported Python versions, read from pyproject.toml, and their gate.

pyproject.toml states the supported set twice: the ``Programming Language ::
Python :: 3.N`` classifiers list every version, and ``requires-python`` gives
the floor. CI's Python matrix is a third place that could restate it, and did:
``ci.yml`` carried the list as a literal. This script is the one reader, so
the matrix is derived from the classifiers instead of copied from them.

Modes
-----
``--matrix``
    Print every classifier version, sorted by version, as a JSON list. This is
    CI's Python matrix, on every run.
``--primary``
    Print the PRIMARY leg, as a bare version string: the one leg of the
    ``python`` job that runs the steps worth running once rather than on
    every interpreter (the doc-fence gates, the validation-report check, the
    coverage run). CI's ``classify`` step emits it as an output and every
    such step compares ``matrix.python-version`` against that output, so no
    step names a version.
(no argument)
    The gate. Exit 1 unless the lowest classifier equals the
    ``requires-python`` floor and every classifier satisfies every
    ``requires-python`` clause. Either drift means the matrix tests a set the
    package does not declare: a raised floor with the classifiers unchanged
    tests a version pip will refuse to install on, and a classifier below the
    floor advertises one it will not.

The primary leg
---------------
The rule is: **the primary is the floor**, the lowest classifier. That is the
reason the single-leg steps were first put on 3.12 -- it was the first leg of
the original ``['3.12', '3.13', '3.14']`` matrix -- kept when the floor moved.
The literal stayed at 3.12 when 3.9 was added below it, and while a
pull_request ran the floor alone, no PR ran those steps (doppler#1714). The
floor is the version the package promises first, so it is the one that
answers for the version-independent checks.

The script imports only what the floor Python has: ``tomllib`` is 3.11, so
3.9 and 3.10 fall back to the ``tomli`` backport. CI's ``classify`` step runs
it on the runner's own ``python3``.

Examples
--------
>>> _version("3.10") > _version("3.9")
True
>>> _satisfies((3, 8), ">=3.9")
False
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

if sys.version_info >= (3, 11):
    import tomllib
else:  # pragma: no cover - the 3.9/3.10 CI matrix jobs
    import tomli as tomllib

ROOT = Path(__file__).resolve().parent.parent

#: A classifier naming one minor version. The bare ``Python :: 3`` and the
#: ``3 :: Only`` forms name no version, so they do not match.
_CLASSIFIER = re.compile(r"^Programming Language :: Python :: (\d+\.\d+)$")

#: One ``requires-python`` clause: an operator and a dotted version.
_CLAUSE = re.compile(r"^\s*(>=|<=|==|!=|>|<)\s*(\d+(?:\.\d+)*)\s*$")


def _version(text: str) -> tuple[int, ...]:
    """Parse ``"3.10"`` into ``(3, 10)``, so versions sort numerically."""
    return tuple(int(p) for p in text.split("."))


def _satisfies(v: tuple[int, ...], clause: str) -> bool:
    """Whether version ``v`` satisfies one ``requires-python`` clause.

    Both sides are padded to equal length with zeros, so ``3.9`` compared
    against ``>=3.9.0`` is equal rather than shorter.
    """
    m = _CLAUSE.match(clause)
    if m is None:
        raise ValueError(f"unsupported requires-python clause: {clause!r}")
    op, bound = m.group(1), _version(m.group(2))
    n = max(len(v), len(bound))
    a = v + (0,) * (n - len(v))
    b = bound + (0,) * (n - len(bound))
    return {
        ">=": a >= b,
        "<=": a <= b,
        "==": a == b,
        "!=": a != b,
        ">": a > b,
        "<": a < b,
    }[op]


def classifier_versions(project: dict) -> list[str]:
    """The classifier versions, sorted by version (not as strings)."""
    found = []
    for c in project.get("classifiers", []):
        m = _CLASSIFIER.match(c)
        if m:
            found.append(m.group(1))
    return sorted(found, key=_version)


def primary(project: dict) -> str | None:
    """The primary leg: the floor, so it is in every matrix (see module doc).

    Returns ``None`` when there is no classifier; the gate reports that case.

    Examples
    --------
    >>> primary(
    ...     {
    ...         "classifiers": [
    ...             "Programming Language :: Python :: 3.12",
    ...             "Programming Language :: Python :: 3.9",
    ...         ]
    ...     }
    ... )
    '3.9'
    """
    versions = classifier_versions(project)
    return versions[0] if versions else None


def check(project: dict) -> list[str]:
    """Every way the classifiers disagree with ``requires-python``."""
    versions = classifier_versions(project)
    spec = project.get("requires-python")
    if not versions:
        return ["no `Programming Language :: Python :: 3.N` classifier"]
    if not spec:
        return ["no `requires-python`"]
    clauses = [c for c in spec.split(",") if c.strip()]
    floors = [
        _version(m.group(2))
        for m in map(_CLAUSE.match, clauses)
        if m and m.group(1) == ">="
    ]
    errors = []
    if len(floors) != 1:
        errors.append(
            f"requires-python {spec!r} must have exactly one `>=` floor"
        )
    elif _version(versions[0]) != floors[0]:
        errors.append(
            f"lowest classifier {versions[0]} is not the requires-python "
            f"floor {'.'.join(map(str, floors[0]))}"
        )
    for v in versions:
        bad = [c.strip() for c in clauses if not _satisfies(_version(v), c)]
        if bad:
            errors.append(
                f"classifier {v} is outside requires-python ({', '.join(bad)})"
            )
    return errors


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--matrix", action="store_true")
    mode.add_argument("--primary", action="store_true")
    ap.add_argument("--pyproject", type=Path, default=ROOT / "pyproject.toml")
    args = ap.parse_args(argv)
    project = tomllib.loads(args.pyproject.read_text(encoding="utf-8"))[
        "project"
    ]
    if args.matrix:
        versions = classifier_versions(project)
        print(json.dumps(versions))
        return 0 if versions else 1
    if args.primary:
        lead = primary(project)
        if lead is None:
            return 1
        print(lead)
        return 0
    errors = check(project)
    for e in errors:
        print(f"python-versions-check: {e}", file=sys.stderr)
    if errors:
        return 1
    versions = classifier_versions(project)
    print(
        f"python-versions-check: OK -- {len(versions)} classifier(s), "
        f"{versions[0]}..{versions[-1]}, floor matches requires-python, "
        f"primary leg {primary(project)}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
