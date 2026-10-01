#!/usr/bin/env python3
"""Fail when a CI job does not feed the one required check, ``CI passed``.

``protect-main`` requires exactly one status check: ``CI passed``, the
``ci-passed`` job at the bottom of ``ci.yml``. That job is green only when
every job in its ``needs`` succeeded, so its ``needs`` list IS the merge gate.
A job that is not in it can fail on every pull request and block nothing.

That is not hypothetical as a shape. Until 2026-09-14 the ruleset required
four named checks, and a job's name was what made it binding; relaxing it to
the single aggregator moved that responsibility into a hand-kept YAML list
that nothing read. Adding a job and forgetting the list is exactly the kind
of mistake a green PR does not reveal -- the new job still shows red on the
PR page, it just stops mattering.

What is checked
---------------
- the aggregator job exists, is named ``CI passed`` (the string the ruleset
  requires), and runs ``if: always()`` -- without it a failed dependency
  SKIPS the aggregator, and a skipped required check does not block;
- every other job in the workflow is in its ``needs``;
- every entry in its ``needs`` names a job that exists;
- the bump-only fast path agrees with itself: every job gated on
  ``needs.changes.outputs.src`` or ``.heavy`` needs ``changes``, and the
  aggregator's ``SKIPPABLE`` is exactly the set of gated jobs (see
  scripts/ci_passed.py);
- the pull_request split agrees with itself: the aggregator's ``HEAVY`` is
  exactly the jobs gated on ``needs.changes.outputs.heavy``, and ``changes``
  declares both ``full`` (which ci_passed.py reads to tell a skip by design
  from a skip that should not have happened) and ``heavy``. Without the
  equality a heavy job missing from HEAVY turns every PR red, and a job in
  HEAVY that is not gated is granted a skip it never takes;
- a step that runs on one Python leg names the leg by ROLE: its ``if`` is
  exactly one of the ``LEG_SELECTORS`` below, which compare
  ``matrix.python-version`` against ``changes``' ``primary`` (every run) or
  ``primary_full`` (full runs only) output -- never a version literal, which
  silently matches no leg once that version leaves the classifiers, and
  never a leg that a pull_request's matrix lacks (doppler#1714). ``changes``
  must declare both outputs and derive them with
  ``scripts/python_versions.py --primary``;
- docs/dev/ci.md's table of those steps (between the ``python-legs``
  markers) names exactly the steps, targets and lanes the workflow runs.

Usage
-----
::

    python scripts/check_ci_aggregator.py            # ci.yml + docs/dev/ci.md
    python scripts/check_ci_aggregator.py FILE       # a named workflow
    python scripts/check_ci_aggregator.py FILE --doc DOC
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

import yaml

ROOT = pathlib.Path(__file__).resolve().parent.parent
WORKFLOW = ROOT / ".github" / "workflows" / "ci.yml"
DOC = ROOT / "docs" / "dev" / "ci.md"
AGGREGATOR = "ci-passed"
REQUIRED_NAME = "CI passed"

#: The ONE declaration of where a single-leg step runs: the only ``if:``
#: forms a step may use to pick a Python leg, each mapped to its lane. The
#: lane is decided by which ``changes`` output the step compares against, so
#: moving a step between lanes is a one-token edit this table already knows.
LEG_SELECTORS = {
    "matrix.python-version == needs.changes.outputs.primary": "fast",
    "matrix.python-version == needs.changes.outputs.primary_full": "heavy",
    "matrix.python-version != needs.changes.outputs.primary_full": "rest",
}
#: What each lane means, for the messages and docs/dev/ci.md's table.
LANES = {
    "fast": "the primary leg, every run (a pull_request included)",
    "heavy": "the primary leg, full runs only (merge_group, push)",
    "rest": "every leg but the heavy lane's",
}
#: A version literal anywhere in a selector, e.g. ``'3.12'``.
_LITERAL = re.compile(r"""['"]\d+\.\d+['"]""")
#: The markers around docs/dev/ci.md's table of single-leg steps.
DOC_START = "<!-- python-legs:start -->"
DOC_END = "<!-- python-legs:end -->"


def check(path: pathlib.Path) -> list[str]:
    """Return every way ``path``'s aggregator fails to gate its jobs.

    Parameters
    ----------
    path : pathlib.Path
        A GitHub Actions workflow file.

    Returns
    -------
    list of str
        One human-readable problem per line; empty when the gate is sound.
    """
    try:
        doc = yaml.safe_load(path.read_text(encoding="utf-8"))
    except yaml.YAMLError as exc:
        return [f"{path}: not valid YAML: {exc}"]
    jobs = (doc or {}).get("jobs") or {}
    agg = jobs.get(AGGREGATOR)
    if agg is None:
        return [
            f"{path}: no `{AGGREGATOR}` job -- `{REQUIRED_NAME}` is the only "
            "check protect-main requires, so without it nothing gates a merge"
        ]

    problems: list[str] = []
    if agg.get("name") != REQUIRED_NAME:
        problems.append(
            f"{path}: `{AGGREGATOR}` is named {agg.get('name')!r}, but the "
            f"ruleset requires the check {REQUIRED_NAME!r} by that string"
        )
    if str(agg.get("if", "")).replace(" ", "") != "always()":
        problems.append(
            f"{path}: `{AGGREGATOR}` must run `if: always()` -- otherwise a "
            "failed dependency skips it, and a skipped check does not block"
        )

    needs = agg.get("needs") or []
    if isinstance(needs, str):
        needs = [needs]
    needs_set = set(needs)
    for job in sorted(set(jobs) - {AGGREGATOR} - needs_set):
        problems.append(
            f"{path}: job `{job}` is not in `{AGGREGATOR}`'s needs, so it "
            "gates no merge however red it goes"
        )
    for job in sorted(needs_set - set(jobs)):
        problems.append(
            f"{path}: `{AGGREGATOR}` needs `{job}`, which is not a job here"
        )
    problems += _check_fast_path(path, jobs, agg)
    problems += _check_heavy(path, jobs, agg)
    problems += _check_legs(path, jobs)[0]
    return problems


def _needs(job: dict) -> set[str]:
    n = job.get("needs") or []
    return {n} if isinstance(n, str) else set(n)


def _check_fast_path(path: pathlib.Path, jobs: dict, agg: dict) -> list[str]:
    """The bump-only fast path: the gated jobs and SKIPPABLE agree.

    A job whose ``if`` reads ``needs.changes.outputs.src`` is skipped for a
    version bump alone. Two ways that goes wrong, both silent:

    - the job does not ``need`` ``changes``: the expression reads an empty
      output, and the job is skipped on EVERY diff, forever;
    - it is gated but missing from the aggregator's ``SKIPPABLE`` (every
      bump turns red), or listed there without being gated (the aggregator
      grants a skip the job never takes -- the permission outlives the
      reason, and the next edit to that job inherits it).
    """
    gated = {
        name
        for name, job in jobs.items()
        if "needs.changes.outputs.src" in str(job.get("if", ""))
        or "needs.changes.outputs.heavy" in str(job.get("if", ""))
    }
    problems = [
        f"{path}: job `{name}` is gated on `changes` but does not need it, "
        "so its condition reads nothing and it is skipped on every diff"
        for name in sorted(gated)
        if "changes" not in _needs(jobs[name])
    ]
    declared: set[str] | None = None
    for step in agg.get("steps") or []:
        env = step.get("env") or {}
        if "SKIPPABLE" in env:
            declared = set(str(env["SKIPPABLE"]).split())
    if gated and declared is None:
        problems.append(
            f"{path}: jobs are gated on `changes` but `{AGGREGATOR}` declares "
            "no SKIPPABLE, so every version bump would read as a failure"
        )
    elif declared is not None:
        for name in sorted(gated - declared):
            problems.append(
                f"{path}: `{name}` is gated on `changes` but not in "
                f"`{AGGREGATOR}`'s SKIPPABLE -- every version bump goes red"
            )
        for name in sorted(declared - gated):
            problems.append(
                f"{path}: `{AGGREGATOR}`'s SKIPPABLE lists `{name}`, which is "
                "not gated on `changes` -- a skip it should never take would "
                "be green"
            )
    return problems


def _declared(agg: dict, key: str) -> set[str] | None:
    """The space-separated job set a ci-passed step declares in ``key``."""
    out: set[str] | None = None
    for step in agg.get("steps") or []:
        env = step.get("env") or {}
        if key in env:
            out = set(str(env[key]).split())
    return out


def _check_heavy(path: pathlib.Path, jobs: dict, agg: dict) -> list[str]:
    """The pull_request split: HEAVY is exactly the jobs gated on ``heavy``.

    A heavy job is skipped on every pull_request by design and must run on
    merge_group and push. ci_passed.py grants the skip only to HEAVY, and only
    when ``changes.full`` is ``false``, so both directions of drift show up:

    - gated on ``heavy`` but missing from HEAVY: every PR goes red;
    - in HEAVY but not gated: the aggregator would pass a skip of a job that
      should never skip -- the permission outlives the reason.
    """
    gated = {
        name
        for name, job in jobs.items()
        if "needs.changes.outputs.heavy" in str(job.get("if", ""))
    }
    declared = _declared(agg, "HEAVY")
    problems: list[str] = []
    if gated or declared:
        outputs = (jobs.get("changes") or {}).get("outputs") or {}
        for key in ("full", "heavy"):
            if key not in outputs:
                problems.append(
                    f"{path}: `changes` declares no `{key}` output, so the "
                    "pull_request split has nothing to read"
                )
    if gated and declared is None:
        problems.append(
            f"{path}: jobs are gated on `heavy` but `{AGGREGATOR}` declares "
            "no HEAVY, so every pull_request would read as a failure"
        )
        return problems
    for name in sorted(gated - (declared or set())):
        problems.append(
            f"{path}: `{name}` is gated on `heavy` but not in "
            f"`{AGGREGATOR}`'s HEAVY -- every pull_request goes red"
        )
    for name in sorted((declared or set()) - gated):
        problems.append(
            f"{path}: `{AGGREGATOR}`'s HEAVY lists `{name}`, which is not "
            "gated on `heavy` -- a skip it should never take would be green"
        )
    return problems


def _norm(expr: str) -> str:
    """An ``if:`` with its ``${{ }}`` wrapper and spacing normalized."""
    e = expr.strip()
    if e.startswith("${{") and e.endswith("}}"):
        e = e[3:-2]
    return " ".join(e.split())


def _target(step: dict) -> str:
    """What a step runs, as docs/dev/ci.md's table names it."""
    m = re.search(r"\bmake\s+([\w-]+)", str(step.get("run", "")))
    if m:
        return f"make {m.group(1)}"
    return str(step.get("uses", "")).split("@")[0]


def _check_legs(
    path: pathlib.Path, jobs: dict
) -> tuple[list[str], list[tuple[str, str, str]]]:
    """Single-leg steps name their leg by role, and in a declared lane.

    Returns the problems and the ``(step, target, lane)`` rows of every
    leg-selecting step, in workflow order, for the docs/dev/ci.md table.
    """
    problems: list[str] = []
    rows: list[tuple[str, str, str]] = []
    used: set[str] = set()
    for jname, job in jobs.items():
        for step in (job or {}).get("steps") or []:
            cond = _norm(str(step.get("if", "")))
            if "matrix.python-version" not in cond:
                continue
            name = step.get("name") or step.get("uses") or "?"
            lane = LEG_SELECTORS.get(cond)
            if lane is None:
                why = (
                    "a version literal, which matches no leg once that "
                    "version leaves the classifiers"
                    if _LITERAL.search(cond)
                    else "an undeclared selector"
                )
                problems.append(
                    f"{path}: job `{jname}` step `{name}` picks a Python leg "
                    f"with `{cond}` -- {why}. Use one of: "
                    + "; ".join(
                        f"`{s}` ({LANES[ln]})"
                        for s, ln in LEG_SELECTORS.items()
                    )
                )
                continue
            if "changes" not in _needs(job):
                problems.append(
                    f"{path}: job `{jname}` step `{name}` reads a `changes` "
                    "output but the job does not need `changes`, so it "
                    "compares against nothing"
                )
            used.add(cond.rsplit(".", 1)[1])
            rows.append((str(name), _target(step), lane))
    if used:
        changes = jobs.get("changes") or {}
        outputs = changes.get("outputs") or {}
        for key in sorted({"primary", "primary_full"} - set(outputs)):
            problems.append(
                f"{path}: `changes` declares no `{key}` output, so every "
                "step selecting on it matches no leg and skips"
            )
        text = " ".join(
            str(s.get("run", "")) for s in changes.get("steps") or []
        )
        if "python_versions.py --primary" not in text:
            problems.append(
                f"{path}: `changes` does not derive the primary leg with "
                "`scripts/python_versions.py --primary`, so the leg is "
                "written down here instead of read from the classifiers"
            )
    return problems, rows


def _doc_rows(doc: pathlib.Path) -> list[tuple[str, str, str]] | None:
    """The ``(step, target, lane)`` rows of the doc's python-legs table."""
    text = doc.read_text(encoding="utf-8")
    if DOC_START not in text or DOC_END not in text:
        return None
    body = text.split(DOC_START, 1)[1].split(DOC_END, 1)[0]
    rows = []
    for line in body.splitlines():
        cells = [c.strip().strip("`") for c in line.strip().split("|")[1:-1]]
        if len(cells) < 3 or set(cells[0]) <= set("-: "):
            continue
        rows.append((cells[0], cells[1], cells[2]))
    return rows[1:]  # the header row


def check_doc(path: pathlib.Path, doc: pathlib.Path) -> list[str]:
    """docs/dev/ci.md's table states exactly the workflow's lanes."""
    try:
        jobs = (yaml.safe_load(path.read_text(encoding="utf-8")) or {}).get(
            "jobs"
        ) or {}
    except yaml.YAMLError:
        return []  # check() already reports it
    want = _check_legs(path, jobs)[1]
    have = _doc_rows(doc)
    if have is None:
        return [
            f"{doc}: no `{DOC_START}` ... `{DOC_END}` table of the Python "
            "job's single-leg steps"
        ]
    problems = []
    for row in want:
        if row not in have:
            problems.append(
                f"{doc}: the table lacks `{row[0]}` | `{row[1]}` | {row[2]} "
                f"-- {path.name} runs it in that lane"
            )
    for row in have:
        if row not in want:
            problems.append(
                f"{doc}: the table states `{row[0]}` | `{row[1]}` | {row[2]}"
                f", which {path.name} does not run that way"
            )
    return problems


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("paths", nargs="*", type=pathlib.Path)
    ap.add_argument("--doc", type=pathlib.Path)
    args = ap.parse_args(argv)
    paths = args.paths or [WORKFLOW]
    doc = args.doc or (None if args.paths else DOC)
    problems = [p for path in paths for p in check(path)]
    if doc is not None:
        problems += check_doc(paths[0], doc)
    for p in problems:
        print(f"ci-aggregator-check: {p}")
    if problems:
        return 1
    doc = yaml.safe_load(paths[0].read_text(encoding="utf-8")) or {}
    jobs = doc.get("jobs") or {}
    gated = len(jobs) - 1  # every job but the aggregator
    heavy = len(_declared(jobs.get(AGGREGATOR) or {}, "HEAVY") or ())
    legs = _check_legs(paths[0], jobs)[1]
    print(
        f"ci-aggregator-check: OK -- all {gated} job(s) gate `{REQUIRED_NAME}`"
        f"; {heavy} heavy, skipped on pull_request, required on merge_group "
        f"and push; {len(legs)} single-leg step(s), named by role"
    )
    for name, target, lane in legs:
        print(f"  {lane:5}  {target:28}  {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
