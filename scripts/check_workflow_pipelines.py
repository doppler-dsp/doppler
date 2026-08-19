#!/usr/bin/env python3
"""Gate: a CI step may not throw away the exit code it just produced.

GitHub Actions runs a `run:` block under `bash -e {0}` by default, and in
that shell a PIPELINE reports the status of its LAST command. So

    run: make coverage | tee coverage.txt

is green whenever `tee` is green -- which is always. doppler shipped exactly
that line: `make coverage` died on a missing directory, the step went green,
no report was written, and the failure surfaced one step later as the patch
gate opening a `coverage.lcov` that had never existed. The diagnosis cost a
day, and the number the whole job exists to produce was absent rather than
wrong, which is the harder thing to notice.

That is the same shape as a leading `-` on a recipe line: the exit code is
computed correctly and then discarded. The remedy is `pipefail`, and on
Actions the cheapest form is `shell: bash` -- GitHub's own alias for
`bash --noprofile --norc -eo pipefail {0}`.

So: **every step whose script contains a shell pipeline must run with
pipefail in effect.** In effect means any of

  * `shell: bash` on the step (or a `defaults.run.shell: bash` above it),
  * a shell string that names `pipefail` itself, or
  * `set -o pipefail` inside the script.

Registration-free: it walks every workflow and every composite action in
the tree, so a new file or a new step is covered the moment it exists.
Pipes inside quotes are not pipelines -- a `jq` filter or an `awk` program
carries `|` as data -- and only `|` found outside quoting counts.

Usage:  python3 scripts/check_workflow_pipelines.py [FILE...]
With no argument it checks the whole tree; explicit files are how
`src/doppler/tests/test_ci_pipefail_gate.py` drives it over seeded YAML,
so the gate is proven against a known-bad step instead of only ever being
run against a tree that happens to pass.
Exit 0 when every pipeline in every step runs under pipefail.
"""

from __future__ import annotations

import sys
from pathlib import Path
from typing import TYPE_CHECKING

import yaml

if TYPE_CHECKING:
    from collections.abc import Iterator

ROOT = Path(__file__).resolve().parent.parent
GLOBS = (".github/workflows/*.yml", ".github/actions/*/*.yml")

# `shell: bash` is GitHub's alias for `bash --noprofile --norc -eo pipefail`.
# `sh` is NOT: it maps to `sh -e`, no pipefail, and POSIX sh has no such
# option to add -- a script needing one there must restructure instead.
PIPEFAIL_SHELLS = frozenset({"bash", "pwsh", "powershell"})


def _pipes(script: str) -> Iterator[tuple[int, str]]:
    """Yield `(line number, line)` for each line holding a shell pipeline.

    A `|` only pipes when it is unquoted, so quoting is tracked: a `jq`
    filter (`--jq '.runs[] | .name'`) and an `awk` program both carry one as
    data. `||` is the or-list operator, `#` outside quotes opens a comment,
    and quotes span newlines exactly as the shell says they do.

    Command substitution gets its own frame, because quoting RESTARTS inside
    it: `line="$(gh api "$url" | head -n1)"` is a pipeline, and a scanner
    that merely toggles on every `"` reads the inner quotes as closing the
    outer one and loses track. That is not a hypothetical -- release.yml
    holds exactly that line, and the flat version of this scanner reported
    it clean. A pipeline inside `$( )` discards its status the same way, so
    it counts.
    """
    frames = [""]
    line_no, hits = 1, set()
    i = 0
    while i < len(script):
        ch = script[i]
        if ch == "\n":
            line_no += 1
            i += 1
            continue
        quote = frames[-1]
        if quote == "'":
            if ch == "'":
                frames[-1] = ""
            i += 1
            continue
        if ch == "\\":
            # A backslash-newline is a line continuation; count the newline
            # it swallows or every report after it is off by one.
            if script[i + 1 : i + 2] == "\n":
                line_no += 1
            i += 2
            continue
        if quote == '"':
            if ch == '"':
                frames[-1] = ""
                i += 1
                continue
            if script[i : i + 2] == "$(":
                frames.append("")
                i += 2
                continue
            i += 1
            continue
        # Unquoted, in whatever frame we are in.
        if script[i : i + 2] == "$(":
            frames.append("")
            i += 2
        elif ch == ")" and len(frames) > 1:
            frames.pop()
            i += 1
        elif ch in "'\"":
            frames[-1] = ch
            i += 1
        elif ch == "#" and (i == 0 or script[i - 1] in " \t\n"):
            while i < len(script) and script[i] != "\n":
                i += 1
        elif ch == "|":
            if script[i + 1 : i + 2] == "|":
                i += 2
            else:
                hits.add(line_no)
                i += 1
        else:
            i += 1

    lines = script.splitlines()
    for n in sorted(hits):
        yield n, lines[n - 1].strip()


def _steps(doc: dict) -> Iterator[tuple[str, dict, str]]:
    """Yield `(job name, step, inherited shell)` for a parsed YAML file.

    Covers both shapes the tree uses: a workflow's `jobs.<id>.steps` and a
    composite action's `runs.steps`. The inherited shell is the nearest
    `defaults.run.shell` above the step, workflow-level or job-level.
    """
    top = (doc.get("defaults") or {}).get("run", {}).get("shell", "")
    for name, job in (doc.get("jobs") or {}).items():
        if not isinstance(job, dict):
            continue
        shell = (job.get("defaults") or {}).get("run", {}).get("shell", top)
        for step in job.get("steps") or []:
            yield name, step, shell
    for step in (doc.get("runs") or {}).get("steps") or []:
        yield "runs", step, top


def _targets(argv: list[str]) -> list[Path]:
    """The YAML files to check: the arguments, else the whole tree."""
    if argv:
        return [Path(a) for a in argv]
    return [p for g in GLOBS for p in sorted(ROOT.glob(g))]


def main(argv: list[str] | None = None) -> int:
    findings: list[str] = []
    checked = 0
    for path in _targets(argv if argv is not None else sys.argv[1:]):
        doc = yaml.safe_load(path.read_text(encoding="utf-8")) or {}
        if not isinstance(doc, dict):
            continue
        try:
            rel = path.relative_to(ROOT).as_posix()
        except ValueError:
            rel = path.as_posix()
        for job, step, inherited in _steps(doc):
            script = step.get("run")
            if not isinstance(script, str):
                continue
            checked += 1
            shell = str(step.get("shell") or inherited or "")
            safe = (
                shell in PIPEFAIL_SHELLS
                or "pipefail" in shell
                or "pipefail" in script
            )
            if safe:
                continue
            label = step.get("name") or step.get("uses") or "(unnamed)"
            for n, line in _pipes(script):
                findings.append(
                    f"  {rel} [{job}] {label!r} line {n}:\n      {line}"
                )

    if findings:
        print(
            f"ci-pipefail: {len(findings)} pipeline(s) whose exit code is"
            " discarded."
        )
        print("\n".join(findings))
        print(
            "\nThe default Actions shell is `bash -e`, where a pipeline"
            " reports the LAST\ncommand's status -- so a failure on the left"
            " of the `|` is silently green.\nAdd `shell: bash` to the step"
            " (GitHub's alias for `bash -eo pipefail`), or\n`set -o pipefail`"
            " to the script."
        )
        return 1

    print(f"ci-pipefail: OK — {checked} run step(s), every pipeline gated")
    return 0


if __name__ == "__main__":
    sys.exit(main())
