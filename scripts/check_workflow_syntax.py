#!/usr/bin/env python3
"""Fail when a workflow ``run:`` block is not valid shell.

A ``run:`` block is a shell script, and nothing parsed it. GitHub does not
validate it, and a syntax error does not announce itself as one -- the shell
runs what it managed to parse and the step fails somewhere else entirely.

Measured on the v0.43.0 release, which this gate exists because of. The C
library step passed its script to a container as a double-quoted argument::

    bash -euxc "
      ...
      # than assumed: without the line `git archive` prints "detected
      # dubious ownership" and writes nothing, and with it the export
      ...
      make package-starter-tarball VERSION=...
    "

The unbalanced ``"`` inside a COMMENT closed that argument early, so the lines
after it were parsed by the runner instead of the container -- and
``make package-starter-tarball`` ran as the runner user against a ``build/``
that cmake had just created as root inside the container. It surfaced as::

    mkdir: cannot create directory 'build/starter-pkg': Permission denied

a permissions error with no permissions bug behind it, on the job that builds
the release tarballs, after the wheels had already published to PyPI. The
backticks in those comments were command-substituted on the runner too, which
is where the log's ``safe.directory: command not found`` came from.

``bash -n`` on that block reports ``unexpected EOF while looking for matching
'"'`` in under a millisecond.

What is checked, and what is not
--------------------------------
Only the shell is parsed -- this says nothing about whether the commands are
right, which is what `check_workflow_pipelines.py` and the jobs themselves are
for. A ``shell:`` naming python/pwsh/cmd is skipped rather than guessed at.

``${{ ... }}`` expressions are substituted before the shell ever sees them, so
they are replaced here with a bare word. That is what GitHub does and it keeps
a legitimate ``${{ }}`` in argument position from reading as a syntax error.

Usage
-----
::

    python scripts/check_workflow_syntax.py            # every workflow
    python scripts/check_workflow_syntax.py FILE ...   # named files
"""

from __future__ import annotations

import pathlib
import re
import subprocess
import sys

import yaml

ROOT = pathlib.Path(__file__).resolve().parent.parent
WORKFLOWS = ROOT / ".github" / "workflows"

#: A `${{ ... }}` expression. GitHub substitutes these textually before the
#: shell runs, so for a syntax check they stand in as one bare word.
EXPR = re.compile(r"\$\{\{.*?\}\}", re.DOTALL)

#: Shells this gate can parse. Anything else is skipped, not guessed at.
BASH_SHELLS = {"bash", "sh", "bash -e", ""}


def _steps(doc: dict):
    """Yield ``(job, index, step)`` for every step in a workflow document."""
    for job_name, job in (doc.get("jobs") or {}).items():
        if not isinstance(job, dict):
            continue
        for n, step in enumerate(job.get("steps") or []):
            if isinstance(step, dict):
                yield job_name, n, step


def check(path: pathlib.Path) -> list[str]:
    """Return one message per ``run:`` block that does not parse."""
    try:
        doc = yaml.safe_load(path.read_text(encoding="utf-8"))
    except yaml.YAMLError as exc:  # a malformed workflow is its own failure
        return [f"{path.name}: not valid YAML — {exc}"]
    if not isinstance(doc, dict):
        return []

    problems: list[str] = []
    default_shell = ((doc.get("defaults") or {}).get("run") or {}).get(
        "shell", ""
    )
    for job, n, step in _steps(doc):
        run = step.get("run")
        if not isinstance(run, str):
            continue
        shell = str(step.get("shell", default_shell) or "").strip()
        # An absent `shell:` means bash on Linux runners, which is every
        # runner here. A named one is honoured, and anything this cannot
        # parse is skipped rather than guessed at.
        if shell and shell.split()[0] not in BASH_SHELLS:
            continue
        script = EXPR.sub("GHA_EXPR", run)
        proc = subprocess.run(
            ["bash", "-n"], input=script, capture_output=True, text=True
        )
        if proc.returncode != 0:
            name = step.get("name") or f"step {n}"
            err = proc.stderr.strip().splitlines()
            detail = err[-1] if err else "bash -n failed"
            detail = re.sub(r"^/dev/fd/\d+: ", "", detail)
            problems.append(f"{path.name}: {job} / {name}\n      {detail}")
    return problems


def main(argv: list[str]) -> int:
    files = (
        [pathlib.Path(a) for a in argv]
        if argv
        else sorted(WORKFLOWS.glob("*.yml")) + sorted(WORKFLOWS.glob("*.yaml"))
    )
    problems: list[str] = []
    for f in files:
        problems += check(f)

    if problems:
        print("check_workflow_syntax: a `run:` block is not valid shell.")
        print("  GitHub does not parse these, so the step fails somewhere")
        print("  else entirely — see the docstring for the release this cost.")
        print()
        for p in problems:
            print(f"  {p}")
        return 1

    print(
        f"check_workflow_syntax: {len(files)} workflow(s) — every `run:` "
        "block parses"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
