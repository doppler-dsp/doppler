#!/usr/bin/env python3
"""Render ``docs/dev/issues.md`` from a committed tier map.

The backlog is triaged by **the kind of harm each issue does**, not by age
or label — 79 of 86 open issues carry no label at all, so a page grouped by
label would be one bucket. The tiers are a judgement, so they are committed
in ``docs/dev/issue-tiers.toml`` where they can be reviewed and argued with,
rather than recomputed from something that does not encode judgement.

Why a generator and not a hand-written page
-------------------------------------------
A hand-maintained list of eighty-odd issues is the shape this repo has had
to delete repeatedly: a list that goes stale silently in both directions —
missing what was filed, still naming what was closed. So the page is
rendered, and `--reconcile` checks the map against the live issue list and
**fails** when they disagree:

- an issue that is open and has no tier is untriaged, and says so;
- a tier entry whose issue is closed is a stale row, and says so.

Neither is a warning. A tracker that quietly drops a new issue is worse
than no tracker, because it reads as complete. `--write` fixes the second
kind itself and refuses on the first, because only the first needs
judgement.

The three modes, and where each one runs
----------------------------------------
``--check`` re-renders from the committed map alone and diffs the page, so
it is deterministic, offline, and safe on every pull request -- it catches a
hand-edit of the generated page (``docs-check``).

``--reconcile`` reads the live issue list through ``gh`` and fails on either
kind of drift, writing nothing. It is the half ``--check`` cannot do offline,
and it runs DAILY in ``.github/workflows/issues.yml`` (``make issues-check``)
rather than on pull requests: the drift it finds is made by filing or
closing an issue, not by any diff, so as a PR gate it would turn every open
PR red the moment anyone filed one. Until doppler#1716 nothing ran it at all
-- only ``make issues`` reconciled, and nothing ran that, so on 2026-09-30
the map had 66 open issues untiered and 3 closed ones still listed.

``--write`` (``make issues``) is the fix for a red reconcile. It refuses
while an open issue is untiered -- that is judgement, and only a person
supplies it -- and otherwise drops every row whose issue has closed (which
needs none, so it says so and does it), rewrites titles and statuses, and
renders the page. A red daily run therefore always has one fix: tier the
new issues, then ``make issues``. The reconcile still fails on both kinds,
so a closed row is reported the day it goes stale.

``--live FILE`` replaces the ``gh`` read with a JSON list of
``{"number", "title"}`` rows, so the reconcile can be driven over a seeded
list -- which is how its sabotage is proven.
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import pathlib
import subprocess
import sys

REPO_SLUG = "doppler-dsp/doppler"
ROOT = pathlib.Path(__file__).resolve().parent.parent
MAP = ROOT / "docs" / "dev" / "issue-tiers.toml"
PAGE = ROOT / "docs" / "dev" / "issues.md"


def pr_closes(prs: list[dict], repo_slug: str) -> dict[int, int]:
    """Map each issue an open PR will close to that PR's number.

    "In review" means a pull request will close the issue, so the answer is
    GitHub's own ``closingIssuesReferences`` -- the links its closing-keyword
    parser and the sidebar produce -- rather than a scan of the PR body.

    A body scan was what this used to do, and it read every ``#N`` token as
    "closes N": a PR that merely *mentioned* an issue, or named one in
    another repository (``just-buildit/just-makeit#1307``), marked this
    repository's issue of the same number as in review. Filtering on the
    issue URL keeps a cross-repository close out as well.

    Parameters
    ----------
    prs : list of dict
        ``gh pr list --json number,closingIssuesReferences`` rows.
    repo_slug : str
        ``owner/name`` whose issues the tracker covers.

    Returns
    -------
    dict of int to int
        Issue number -> the PR that closes it.

    Examples
    --------
    >>> ref = {"number": 7, "url": "https://github.com/o/r/issues/7"}
    >>> other = {"number": 9, "url": "https://github.com/x/y/issues/9"}
    >>> prs = [{"number": 12, "closingIssuesReferences": [ref, other]}]
    >>> pr_closes(prs, "o/r")
    {7: 12}
    """
    prefix = f"https://github.com/{repo_slug}/issues/"
    closes: dict[int, int] = {}
    for pr in prs:
        for ref in pr.get("closingIssuesReferences") or []:
            if str(ref.get("url", "")).startswith(prefix):
                closes[int(ref["number"])] = int(pr["number"])
    return closes


#: Tier -> (name, what belongs in it). The order here is the page's order and
#: the priority order: tier 0 is done first.
TIERS: dict[int, tuple[str, str]] = {
    0: (
        "Breaks for a user",
        "Reproducible through an interface someone actually uses — a crash, "
        "a race, a link failure, or a stub documenting a signature the "
        "extension does not have.",
    ),
    1: (
        "A gate that does not gate",
        "This repo's own doctrine turned on itself: *a claim nothing runs is "
        "prose*. Each is a check that reports green because it cannot see "
        "the thing it names — several confirmed by sabotage.",
    ),
    2: (
        "A claim nothing measures",
        "A header, report or design doc asserts a number or a behaviour that "
        "no test and no validator establishes. Not wrong — unestablished, "
        "which is a quieter problem.",
    ),
    3: (
        "Measured cost",
        "Performance findings with a number attached. Filed, not fixed; each "
        "names the measurement that produced it.",
    ),
    4: (
        "Cannot be reached",
        "The capability exists in C and no caller can get to it, or it "
        "exists and nothing tells anyone it does.",
    ),
    5: (
        "Convergence and hygiene",
        "Duplication, stale pins, harness drift, and the long tail. Real, "
        "none of it urgent — and the tier that grows when the ones above it "
        "are held.",
    ),
}

ISSUE_URL = f"https://github.com/{REPO_SLUG}/issues"


def drift(live: dict[int, str], issues: dict) -> tuple[list[int], list[int]]:
    """The two ways the tier map disagrees with the live issue list.

    Parameters
    ----------
    live : dict of int to str
        Every OPEN issue: number -> title.
    issues : dict
        The map's ``[issue.N]`` tables, keyed by the number as a string.

    Returns
    -------
    untiered : list of int
        Open issues the map has no tier for, ascending.
    stale : list of int
        Map rows whose issue is no longer open, ascending.

    Examples
    --------
    >>> drift({1: "a", 3: "c"}, {"1": {}, "2": {}})
    ([3], [2])
    """
    untiered = sorted(n for n in live if str(n) not in issues)
    stale = sorted(int(n) for n in issues if int(n) not in live)
    return untiered, stale


def report_drift(
    live: dict[int, str], untiered: list[int], stale: list[int]
) -> list[str]:
    """Each drift as a line to print, naming the fix for each kind."""
    lines: list[str] = []
    if untiered:
        lines.append(
            "gen_issue_tracker: open issue(s) with no tier -- triage them"
        )
        lines.append(
            "  in docs/dev/issue-tiers.toml before this page can render:"
        )
        lines += [f"    #{n}  {live[n]}" for n in untiered]
    if stale:
        lines.append(
            "gen_issue_tracker: tier entry/entries naming a CLOSED issue --"
        )
        lines.append("  delete them from docs/dev/issue-tiers.toml:")
        lines += [f"    #{n}" for n in stale]
    return lines


def _live_issues(path: pathlib.Path | None) -> dict[int, str]:
    """Every open issue, from ``gh`` or from a seeded JSON list."""
    if path is not None:
        rows = json.loads(path.read_text(encoding="utf-8"))
    else:
        rows = _gh(
            "issue",
            "list",
            "--repo",
            REPO_SLUG,
            "--state",
            "open",
            "--limit",
            "1000",
            "--json",
            "number,title",
        )
    return {int(d["number"]): d["title"] for d in rows}


def _gh(*args: str) -> list[dict]:
    out = subprocess.run(
        ["gh", *args], capture_output=True, text=True, check=True
    ).stdout
    return json.loads(out)


def load_map() -> dict:
    # Imported here, not at the top: tomllib is 3.11+ and the project floor is
    # 3.9, so a module-level import made pr_closes() unimportable -- and its
    # test red -- on every CI Python below 3.11. The reconcile's tests read
    # the map on every leg, so below 3.11 it is the `tomli` backport (a dev
    # dependency there), as in scripts/python_versions.py.
    if sys.version_info >= (3, 11):
        import tomllib
    else:  # pragma: no cover - the 3.9/3.10 CI matrix legs
        import tomli as tomllib

    if not MAP.is_file():
        return {"meta": {}, "issue": {}}
    return tomllib.loads(MAP.read_text(encoding="utf-8"))


def _mdformat(text: str) -> str:
    """Return *text* as mdformat would leave it.

    The page is judged by `make lint`'s mdformat hook like every other
    markdown file, and mdformat pads table columns. A generator emitting
    unpadded tables would be rewritten by the hook on every commit and then
    fail its own `--check` -- the two would fight forever, and the winner
    would be whichever ran last. Formatting here makes them agree by
    construction.

    Routed through the project's PINNED mdformat and its gfm/mkdocs plugins
    rather than a bare import, so this cannot format differently from the
    hook that judges the result.
    """
    proc = subprocess.run(
        ["uv", "run", "--group", "dev", "mdformat", "-"],
        input=text,
        capture_output=True,
        text=True,
        cwd=ROOT,
    )
    if proc.returncode != 0:
        print("gen_issue_tracker: mdformat failed:", proc.stderr.strip())
        raise SystemExit(1)
    return proc.stdout


def render(data: dict) -> str:
    issues = data["issue"]
    meta = data.get("meta", {})
    by_tier: dict[int, list[tuple[int, dict]]] = {t: [] for t in TIERS}
    for num, rec in issues.items():
        by_tier[int(rec["tier"])].append((int(num), rec))
    for rows in by_tier.values():
        rows.sort(key=lambda r: r[0])

    total = len(issues)
    in_review = sum(1 for r in issues.values() if r.get("status") != "open")

    L: list[str] = []
    L.append("# Open issues, by the harm they do")
    L.append("")
    L.append(
        "<!-- GENERATED by scripts/gen_issue_tracker.py — do not hand-edit. "
        "Run `make issues`. -->"
    )
    L.append("")
    L.append(
        f"**{total} open**, sorted into six tiers by the kind of harm each "
        "one does rather than by age or label. Most carry no label at all, "
        "so this ordering *is* the triage rather than a view onto one that "
        "already existed."
    )
    L.append("")
    L.append(
        f"Derived {meta.get('generated', 'unknown')} by "
        "`make issues`, which reads the live issue list; titles and statuses "
        "are as of that date. Whether every open issue still has a tier is "
        "checked daily against the live list (`make issues-check`, in "
        "`.github/workflows/issues.yml`). The tier "
        "assignments below are committed in "
        "[`issue-tiers.toml`](issue-tiers.toml) and reviewed like code."
    )
    L.append("")
    L.append("| Tier | What it means | Open |")
    L.append("| ---- | ------------- | ---- |")
    for t, (name, _why) in TIERS.items():
        L.append(
            f"| [{t}](#tier-{t}-{name.lower().replace(' ', '-')}) "
            f"| {name} | {len(by_tier[t])} |"
        )
    L.append("")
    if in_review:
        L.append(
            f"{in_review} of them "
            f"{'has' if in_review == 1 else 'have'} a pull request open "
            f"against {'it' if in_review == 1 else 'them'}; the **Status** "
            "column says which."
        )
        L.append("")

    for t, (name, why) in TIERS.items():
        rows = by_tier[t]
        L.append(f"## Tier {t} — {name}")
        L.append("")
        L.append(why)
        L.append("")
        if not rows:
            L.append("*Empty.*")
            L.append("")
            continue
        L.append("| Issue | Summary | Status |")
        L.append("| ----- | ------- | ------ |")
        for num, rec in rows:
            title = rec["title"].replace("|", "\\|")
            L.append(
                f"| [#{num}]({ISSUE_URL}/{num}) | {title} "
                f"| {rec.get('status', 'open')} |"
            )
        L.append("")

    L.append("## How an issue gets its tier")
    L.append("")
    L.append(
        "By asking what goes wrong if it is never fixed, and nothing else. "
        "Age does not raise a tier and neither does effort — a one-line fix "
        "that stops a crash outranks a week of hygiene. The tiers are "
        "deliberately about *harm* rather than *cost*, so that the order to "
        "work in falls out of the table instead of being argued each time."
    )
    L.append("")
    L.append(
        "The daily `make issues-check` run goes red if an open issue has no "
        "tier or a tier names an issue that is closed — so this page cannot "
        "rot in either direction without saying so. `make issues` refuses "
        "while an issue is untiered, and drops a closed issue's row itself."
    )
    L.append("")
    return _mdformat("\n".join(L))


def refresh(
    issues: dict, live: dict[int, str], closes: dict[int, int]
) -> list[int]:
    """Bring the map's rows up to date with the live list, in place.

    Drops every row whose issue is no longer open -- that needs no
    judgement, so ``make issues`` does it rather than refusing -- and
    rewrites each open row's title and status. Tier and ``why`` are left
    alone: they are the judgement, and only a person edits them. The caller
    must already have refused an untiered open issue.

    Parameters
    ----------
    issues : dict
        The map's ``[issue.N]`` tables, keyed by the number as a string.
    live : dict of int to str
        Every open issue: number -> title.
    closes : dict of int to int
        Open issue -> the open pull request that closes it.

    Returns
    -------
    list of int
        The numbers of the rows dropped, ascending.

    Examples
    --------
    >>> rows = {
    ...     "1": {"tier": 2, "title": "old", "status": "open", "why": "w"},
    ...     "2": {"tier": 5, "title": "x"},
    ... }
    >>> refresh(rows, {1: "new"}, {})
    [2]
    >>> rows["1"]["title"], rows["1"]["why"], sorted(rows)
    ('new', 'w', ['1'])
    """
    _, stale = drift(live, issues)
    for n in stale:
        del issues[str(n)]
    for n, title in live.items():
        rec = issues[str(n)]
        rec["title"] = title
        rec["status"] = (
            f"in review ([#{closes[n]}]"
            f"(https://github.com/{REPO_SLUG}/pull/{closes[n]}))"
            if n in closes
            else "open"
        )
    return stale


def do_reconcile(live_path: pathlib.Path | None) -> int:
    """Fail on drift between the map and the live list; write nothing.

    The drift also goes to ``$GITHUB_STEP_SUMMARY`` when Actions sets it,
    so a red scheduled run says what to triage on its summary page.
    """
    live = _live_issues(live_path)
    if not live:
        # An empty read is a failed read, not an empty backlog: a gate that
        # compared against nothing would report every row stale, or -- worse
        # -- pass a map that names nothing.
        print("gen_issue_tracker: the live issue list came back empty")
        return 1
    untiered, stale = drift(live, load_map().get("issue", {}))
    lines = report_drift(live, untiered, stale)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as f:
            f.write("### Issue tier map vs the live issue list\n\n```\n")
            f.write("\n".join(lines) if lines else "no drift")
            f.write(
                "\n```\n\nFix: tier the new issues in "
                "docs/dev/issue-tiers.toml, then `make issues`.\n"
            )
    for line in lines:
        print(line)
    if lines:
        return 1
    print(
        f"gen_issue_tracker: OK -- {len(live)} open issue(s), every one "
        "tiered, no closed issue listed"
    )
    return 0


def do_write() -> int:
    live = _live_issues(None)
    prs = _gh(
        "pr",
        "list",
        "--repo",
        REPO_SLUG,
        "--state",
        "open",
        "--limit",
        "100",
        "--json",
        "number,closingIssuesReferences",
    )
    closes = pr_closes(prs, REPO_SLUG)

    data = load_map()
    issues = data.setdefault("issue", {})

    untiered, _ = drift(live, issues)
    if untiered:
        # Tiering is the judgement half; nothing is written until it is done,
        # so the page and the map never disagree.
        for line in report_drift(live, untiered, []):
            print(line)
        return 1

    dropped = refresh(issues, live, closes)
    for n in dropped:
        print(f"gen_issue_tracker: dropped #{n}, closed since it was tiered")

    today = datetime.date.today().isoformat()
    data.setdefault("meta", {})["generated"] = today
    _write_map(data)
    PAGE.write_text(render(data), encoding="utf-8")
    print(
        f"gen_issue_tracker: {len(live)} open issue(s) rendered to "
        f"{PAGE.relative_to(ROOT)} ({today})"
    )
    return 0


def _write_map(data: dict) -> None:
    L = [
        "# The backlog's tier assignments — the judgement half of",
        "# docs/dev/issues.md, committed so it can be reviewed and argued",
        "# with. `make issues` renders the page from this file, refuses while",
        "# an open issue is missing here, and drops a closed issue's row.",
        "#",
        "# tier 0 breaks for a user          3 measured cost",
        "# tier 1 a gate that does not gate  4 cannot be reached",
        "# tier 2 a claim nothing measures   5 convergence and hygiene",
        "",
        "[meta]",
        f'generated = "{data["meta"]["generated"]}"',
        "",
    ]
    for num in sorted(data["issue"], key=int):
        rec = data["issue"][num]
        L.append(f"[issue.{num}]")
        L.append(f"tier = {rec['tier']}")
        L.append(f"title = {json.dumps(rec['title'])}")
        L.append(f"status = {json.dumps(rec['status'])}")
        # Optional: the one-line reason for the tier, so the judgement can be
        # argued with in review. Not rendered on the page.
        if rec.get("why"):
            L.append(f"why = {json.dumps(rec['why'])}")
        L.append("")
    MAP.write_text("\n".join(L), encoding="utf-8")


def do_check() -> int:
    if not MAP.is_file():
        print("gen_issue_tracker: docs/dev/issue-tiers.toml is missing")
        return 1
    want = render(load_map())
    have = PAGE.read_text(encoding="utf-8") if PAGE.is_file() else ""
    if want != have:
        print("gen_issue_tracker: docs/dev/issues.md does not match what")
        print("  docs/dev/issue-tiers.toml renders. Run `make issues`.")
        return 1
    print("gen_issue_tracker: OK — issues.md matches its tier map")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument(
        "--write",
        action="store_true",
        help="refresh from the live issue list (needs network)",
    )
    g.add_argument(
        "--check",
        action="store_true",
        help="re-render from the committed map and diff (offline)",
    )
    g.add_argument(
        "--reconcile",
        action="store_true",
        help="fail on drift from the live issue list; write nothing",
    )
    ap.add_argument(
        "--live",
        type=pathlib.Path,
        help="with --reconcile: a JSON list of {number, title} rows to use "
        "instead of `gh issue list`",
    )
    a = ap.parse_args()
    if a.live is not None and not a.reconcile:
        ap.error("--live goes with --reconcile")
    if a.reconcile:
        return do_reconcile(a.live)
    return do_write() if a.write else do_check()


if __name__ == "__main__":
    sys.exit(main())
