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
- each skip permission agrees with itself (``SKIP_LISTS``): every job
  gated on ``needs.changes.outputs.src`` (or ``.code``) needs ``changes``,
  ``changes`` declares that output, and the aggregator's ``SKIPPABLE`` (or
  ``CODE_ONLY``) is exactly the set of gated jobs (see scripts/ci_passed.py).
  ``changes`` is the canonical workflow, vendored (#1809): a job that
  ``uses:`` a local workflow declares the outputs that FILE declares under
  ``on.workflow_call.outputs``, so that is where they are read;
- the scope permission (``scripts/ci_scope.py``, ``ci_passed.py``): a push
  to a branch with no PR runs the minimal set only, and the rest says so.
  The ``scope`` job exists and declares a ``scope`` output; every other job
  (``changes`` and the aggregator aside) needs it and reads it in its ``if``,
  so a ``skip`` run skips them; and the aggregator's ``PR_ONLY`` is exactly
  the set of jobs gated on ``needs.scope.outputs.scope == 'full'``. A job
  gated that way and missing from ``PR_ONLY`` turns every push red; one in
  ``PR_ONLY`` that is not gated would be a green skip nothing takes;
- there is no second skip permission: the aggregator declares no ``HEAVY``
  and ``changes`` no ``full``/``heavy``/``primary_full`` output. Those were
  the merge queue's pull_request split, retired with the queue on
  2026-10-01; one left behind would grant a skip nothing takes;
- a step that runs on one Python leg names the leg by ROLE: its ``if`` is
  exactly one of the ``LEG_SELECTORS`` below (after the no-fail-fast guards,
  ``!cancelled()`` and ``steps.<id>.outcome == 'success'``, are taken off:
  they say WHETHER the step runs after a failure, not WHERE), which compare
  ``matrix.python-version`` against the ``pythons`` job's ``primary`` output
  -- never a version literal, which silently matches no leg once that version
  leaves the classifiers (doppler#1714). The job the selectors name
  (``LEG_SOURCE``, read from them) must declare the output and derive it with
  ``make -s print-python-primary``, whose recipe must run
  ``scripts/python_versions.py --primary``, and a selecting job must need it.
  It was ``changes`` until that became the vendored workflow (#1809), which
  knows nothing of doppler's Python matrix;
- docs/dev/ci.md's table of those steps (between the ``python-legs``
  markers) names exactly the steps, targets and lanes the workflow runs;
- docs/dev/ci.md's "What runs where" table (between the ``ci-jobs``
  markers) names every job in the workflow exactly once, and no job it
  lacks, by the backticked job ids in its first column (#2599). A hand-kept
  job list drifted with every workflow edit; one row may group jobs that
  share an environment.

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
#: The aggregator's name, as written: only the FULL scope carries the required
#: name (the PR run's gate, and main's, which release-verify-ci reads). Minimal
#: and skip pushes are named apart, so a ruleset resolving the required name to
#: the newest check cannot read a head green while its full run is pending.
AGGREGATOR_NAME_EXPR = (
    "${{ needs.scope.outputs.scope == 'full' && 'CI passed' || "
    "needs.scope.outputs.scope == 'minimal' "
    "&& 'CI passed (push, minimal set)' || "
    "'CI passed (skipped: the PR run is the gate)' }}"
)

#: The ONE declaration of where a single-leg step runs: the only ``if:``
#: forms a step may use to pick a Python leg, each mapped to its lane.
LEG_SELECTORS = {
    "matrix.python-version == needs.pythons.outputs.primary": "primary",
    "matrix.python-version != needs.pythons.outputs.primary": "rest",
}
#: The job whose ``primary`` output the selectors read -- taken from them, so
#: it is declared once, there.
LEG_SOURCE = {
    m.group(1)
    for m in (
        re.search(r"needs\.([\w-]+)\.outputs\.primary", s)
        for s in LEG_SELECTORS
    )
    if m
}.pop()

#: What each lane means, for the messages and docs/dev/ci.md's table.
LANES = {
    "primary": "the primary leg alone",
    "rest": "every leg but the primary",
}
#: What the merge queue's pull_request split declared. None may come back:
#: with every run full, each would be a skip permission nothing takes.
RETIRED_OUTPUTS = ("full", "heavy", "primary_full")
#: A version literal anywhere in a selector, e.g. ``'3.12'``.
_LITERAL = re.compile(r"""['"]\d+\.\d+['"]""")
#: The markers around docs/dev/ci.md's table of single-leg steps.
#: The one spelling of the primary leg in a workflow: this make target,
#: whose recipe runs ``scripts/python_versions.py --primary`` (checked).
PRIMARY_TARGET = "print-python-primary"

DOC_START = "<!-- python-legs:start -->"
DOC_END = "<!-- python-legs:end -->"
#: The markers around docs/dev/ci.md's table of every job.
JOBS_START = "<!-- ci-jobs:start -->"
JOBS_END = "<!-- ci-jobs:end -->"
#: A job id as the jobs table writes it: backticked, in the first column.
_JOB_ID = re.compile(r"`([\w-]+)`")


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
    if agg.get("name") != AGGREGATOR_NAME_EXPR:
        problems.append(
            f"{path}: `{AGGREGATOR}` is named {agg.get('name')!r}; it must "
            f"be {AGGREGATOR_NAME_EXPR!r}: only the full scope's aggregator "
            f"carries the required name {REQUIRED_NAME!r}"
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
    problems += _check_retired(path, jobs, agg)
    problems += _check_scope(path, jobs, agg)
    problems += _check_legs(path, jobs)[0]
    return problems


def _outputs(path: pathlib.Path, jobs: dict, name: str) -> dict:
    """The outputs job ``name`` declares, wherever it declares them.

    A job that ``uses:`` a local reusable workflow (``./.github/...``) has
    no ``outputs`` of its own: they are the called file's
    ``on.workflow_call.outputs``. Reading only the job would report every
    output of the vendored ``changes`` missing.
    """
    job = jobs.get(name) or {}
    uses = str(job.get("uses", ""))
    if not uses.startswith("./"):
        return job.get("outputs") or {}
    called = path.resolve().parents[2] / uses[2:]
    try:
        doc = yaml.safe_load(called.read_text(encoding="utf-8")) or {}
    except (OSError, yaml.YAMLError):
        return {}
    on = doc.get(True, doc.get("on")) or {}
    call = on.get("workflow_call") if isinstance(on, dict) else None
    return (call or {}).get("outputs") or {}


def _needs(job: dict) -> set[str]:
    n = job.get("needs") or []
    return {n} if isinstance(n, str) else set(n)


#: The ONE declaration of the skip permissions: a ``changes`` output, the
#: aggregator env list whose jobs may skip when that output is ``false``, and
#: what a gated job missing from the list does. scripts/ci_passed.py grants
#: exactly these two; any other is refused (_check_retired).
SKIP_LISTS = (
    ("src", "SKIPPABLE", "every version bump goes red"),
    ("code", "CODE_ONLY", "every docs-only PR goes red"),
)


def _check_fast_path(path: pathlib.Path, jobs: dict, agg: dict) -> list[str]:
    """Each skip permission: the jobs gated on its output equal its list.

    A job whose ``if`` reads ``needs.changes.outputs.src`` (or ``.code``)
    is skipped when that output is false. Three ways that goes wrong, all
    silent:

    - the job does not ``need`` ``changes``: the expression reads an empty
      output, and the job is skipped on EVERY diff, forever;
    - ``changes`` declares no such output: the same, for every job on it;
    - it is gated but missing from the aggregator's list (every such diff
      turns red), or listed there without being gated (the aggregator
      grants a skip the job never takes -- the permission outlives the
      reason, and the next edit to that job inherits it).
    """
    problems: list[str] = []
    outputs = _outputs(path, jobs, "changes")
    for key, env_name, cost in SKIP_LISTS:
        gated = {
            name
            for name, job in jobs.items()
            if f"needs.changes.outputs.{key}" in str(job.get("if", ""))
        }
        problems += [
            f"{path}: job `{name}` is gated on `changes` but does not need "
            "it, so its condition reads nothing and it is skipped on every "
            "diff"
            for name in sorted(gated)
            if "changes" not in _needs(jobs[name])
        ]
        if gated and key not in outputs:
            problems.append(
                f"{path}: jobs are gated on `{key}` but `changes` declares no "
                f"`{key}` output, so they are skipped on every diff"
            )
        declared = _declared(agg, env_name)
        if gated and declared is None:
            problems.append(
                f"{path}: jobs are gated on `{key}` but `{AGGREGATOR}` "
                f"declares no {env_name}, so {cost[6:]}"
            )
            continue
        for name in sorted(gated - (declared or set())):
            problems.append(
                f"{path}: `{name}` is gated on `{key}` but not in "
                f"`{AGGREGATOR}`'s {env_name} -- {cost}"
            )
        for name in sorted((declared or set()) - gated):
            problems.append(
                f"{path}: `{AGGREGATOR}`'s {env_name} lists `{name}`, which "
                f"is not gated on `{key}` -- a skip it should never take "
                "would be green"
            )
    return problems


#: The job that decides how much a run owes, and the value that means "all".
SCOPE_JOB = "scope"
FULL = "needs.scope.outputs.scope == 'full'"
#: Jobs that do not read the scope: it IS the scope; `changes` is the
#: vendored classifier (a `uses:` job, which carries no `if` of ours); and
#: the aggregator runs always.
SCOPE_EXEMPT = {SCOPE_JOB, "changes", AGGREGATOR}


def _check_scope(path: pathlib.Path, jobs: dict, agg: dict) -> list[str]:
    """The scope permission: ``PR_ONLY`` equals the jobs gated on ``full``.

    A push without a pull request runs the minimal set (scripts/ci_scope.py).
    What it does not run is skipped, and a skip is green only through the
    aggregator's ``PR_ONLY``. So, as for ``SKIPPABLE`` and ``CODE_ONLY``:
    a job gated on ``full`` and absent from it turns every such push red;
    one listed but not gated is a skip granted for nothing. And every job
    must read the scope at all, or a ``skip`` run (the pushed commit heads
    an open PR, whose own run is the gate) would run it a second time.
    """
    scope_job = jobs.get(SCOPE_JOB)
    reads = any(
        "needs.scope.outputs.scope" in str((j or {}).get("if", ""))
        for j in jobs.values()
    )
    if scope_job is None:
        if not reads and _declared(agg, "PR_ONLY") is None:
            return []  # a workflow with no scope policy at all (a seed)
        return [
            f"{path}: no `{SCOPE_JOB}` job -- nothing says what a push "
            "without a PR runs"
        ]
    problems: list[str] = []
    if "scope" not in (scope_job.get("outputs") or {}):
        problems.append(f"{path}: `{SCOPE_JOB}` declares no `scope` output")
    gated: set[str] = set()
    for name, job in jobs.items():
        if name in SCOPE_EXEMPT:
            continue
        cond = _norm(str((job or {}).get("if", "")))
        if "needs.scope.outputs.scope" not in cond:
            problems.append(
                f"{path}: job `{name}` does not read "
                "`needs.scope.outputs.scope` in its `if`, so a run that "
                "another PR run already covers would run it again"
            )
        if FULL in cond:
            gated.add(name)
        if "needs.scope.outputs.scope" in cond and SCOPE_JOB not in _needs(
            job
        ):
            problems.append(
                f"{path}: job `{name}` reads the scope but does not need "
                f"`{SCOPE_JOB}`, so its condition reads nothing and it is "
                "skipped on every run"
            )
    declared = _declared(agg, "PR_ONLY")
    if gated and declared is None:
        problems.append(
            f"{path}: jobs are gated on `{FULL}` but `{AGGREGATOR}` "
            "declares no PR_ONLY, so every push without a PR goes red"
        )
        return problems
    for name in sorted(gated - (declared or set())):
        problems.append(
            f"{path}: `{name}` is gated on `{FULL}` but not in "
            f"`{AGGREGATOR}`'s PR_ONLY -- every push without a PR goes red"
        )
    for name in sorted((declared or set()) - gated):
        problems.append(
            f"{path}: `{AGGREGATOR}`'s PR_ONLY lists `{name}`, which is not "
            f"gated on `{FULL}` -- a skip it should never take would be "
            "green"
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


def _check_retired(path: pathlib.Path, jobs: dict, agg: dict) -> list[str]:
    """No trace of the merge queue's pull_request split survives.

    Until 2026-10-01 a pull_request ran the fast gates and the merge queue ran
    the rest, so ``changes`` declared ``full``/``heavy``/``primary_full`` and
    the aggregator a ``HEAVY`` list it might skip. With the queue retired every
    run is full; any of those coming back is a second skip permission.
    """
    problems: list[str] = []
    outputs = _outputs(path, jobs, "changes")
    for key in RETIRED_OUTPUTS:
        if key in outputs:
            problems.append(
                f"{path}: `changes` declares `{key}`, the retired merge "
                "queue's pull_request split -- every run is full now"
            )
    if _declared(agg, "HEAVY") is not None:
        problems.append(
            f"{path}: `{AGGREGATOR}` declares HEAVY, a skip permission from "
            "the retired merge queue -- every run is full now"
        )
    for name, job in jobs.items():
        cond = str((job or {}).get("if", ""))
        if any(f"needs.changes.outputs.{k}" in cond for k in RETIRED_OUTPUTS):
            problems.append(
                f"{path}: job `{name}` is gated on a retired `changes` output"
            )
    return problems


def _norm(expr: str) -> str:
    """An ``if:`` with its ``${{ }}`` wrapper and spacing normalized."""
    e = expr.strip()
    if e.startswith("${{") and e.endswith("}}"):
        e = e[3:-2]
    return " ".join(e.split())


#: A guard term of the no-fail-fast form: it decides whether a step runs
#: after an earlier failure, never which Python leg it runs on.
_GUARD = re.compile(r"!cancelled\(\)|steps\.[\w-]+\.outcome == 'success'")


def _leg_cond(expr: str) -> str:
    """``_norm(expr)`` with the no-fail-fast guards taken off its ``&&``
    terms, so a step's LEG is judged by what is left."""
    terms = [t.strip() for t in _norm(expr).split("&&")]
    return " && ".join(t for t in terms if not _GUARD.fullmatch(t))


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
            cond = _leg_cond(str(step.get("if", "")))
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
            if LEG_SOURCE not in _needs(job):
                problems.append(
                    f"{path}: job `{jname}` step `{name}` reads a "
                    f"`{LEG_SOURCE}` output but the job does not need "
                    f"`{LEG_SOURCE}`, so it compares against nothing"
                )
            used.add(cond.rsplit(".", 1)[1])
            rows.append((str(name), _target(step), lane))
    if used:
        source = jobs.get(LEG_SOURCE) or {}
        outputs = _outputs(path, jobs, LEG_SOURCE)
        for key in sorted({"primary"} - set(outputs)):
            problems.append(
                f"{path}: `{LEG_SOURCE}` declares no `{key}` output, so "
                "every step selecting on it matches no leg and skips"
            )
        text = " ".join(
            str(s.get("run", "")) for s in source.get("steps") or []
        )
        if f"make -s {PRIMARY_TARGET}" not in text:
            problems.append(
                f"{path}: `{LEG_SOURCE}` does not derive the primary leg "
                f"with `make -s {PRIMARY_TARGET}`, so the leg is written "
                "down here, or spelled a second way, instead of read from "
                "the classifiers through the one target"
            )
        problems += _primary_target_problems(ROOT / "Makefile")
    return problems, rows


def _primary_target_problems(makefile: pathlib.Path) -> list[str]:
    """The target a workflow derives the primary leg through must run the
    script that reads the classifiers, or the one spelling is a literal
    with extra steps."""
    lines = makefile.read_text(encoding="utf-8").splitlines()
    for i, line in enumerate(lines):
        if line.startswith(f"{PRIMARY_TARGET}:"):
            recipe = []
            for r in lines[i + 1 :]:
                if not r.startswith("\t"):
                    break
                recipe.append(r)
            if any("python_versions.py --primary" in r for r in recipe):
                return []
            return [
                f"{makefile.name}: `{PRIMARY_TARGET}` does not run "
                "`scripts/python_versions.py --primary`"
            ]
    return [f"{makefile.name}: no `{PRIMARY_TARGET}` target"]


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


def _doc_jobs(doc: pathlib.Path) -> list[str] | None:
    """Every backticked job id in the first column of the doc's jobs table,
    in order and with repeats, or None when the table is absent."""
    text = doc.read_text(encoding="utf-8")
    if JOBS_START not in text or JOBS_END not in text:
        return None
    body = text.split(JOBS_START, 1)[1].split(JOBS_END, 1)[0]
    named: list[str] = []
    for line in body.splitlines()[1:]:
        cells = line.strip().split("|")[1:-1]
        if not cells or set(cells[0].strip()) <= set("-: "):
            continue
        named += _JOB_ID.findall(cells[0])
    return named


def check_jobs_doc(path: pathlib.Path, doc: pathlib.Path) -> list[str]:
    """docs/dev/ci.md's jobs table names each workflow job exactly once."""
    try:
        jobs = (yaml.safe_load(path.read_text(encoding="utf-8")) or {}).get(
            "jobs"
        ) or {}
    except yaml.YAMLError:
        return []  # check() already reports it
    named = _doc_jobs(doc)
    if named is None:
        return [
            f"{doc}: no `{JOBS_START}` ... `{JOBS_END}` table of the jobs "
            f"{path.name} runs"
        ]
    problems = []
    for job in jobs:
        if job not in named:
            problems.append(
                f"{doc}: the jobs table lacks `{job}`, which {path.name} runs"
            )
    for job in sorted(set(named)):
        if job not in jobs:
            problems.append(
                f"{doc}: the jobs table names `{job}`, which {path.name} "
                "has no job called"
            )
        elif named.count(job) > 1:
            problems.append(f"{doc}: the jobs table names `{job}` twice")
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
        problems += check_jobs_doc(paths[0], doc)
    for p in problems:
        print(f"ci-aggregator-check: {p}")
    if problems:
        return 1
    doc = yaml.safe_load(paths[0].read_text(encoding="utf-8")) or {}
    jobs = doc.get("jobs") or {}
    gated = len(jobs) - 1  # every job but the aggregator
    legs = _check_legs(paths[0], jobs)[1]
    print(
        f"ci-aggregator-check: OK -- all {gated} job(s) gate `{REQUIRED_NAME}`"
        f" on every run; {len(legs)} single-leg step(s), named by role"
    )
    for name, target, lane in legs:
        print(f"  {lane:7}  {target:28}  {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
