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
rendered, and `--write` reconciles the map against the live issue list and
**fails** when they disagree:

- an issue that is open and has no tier is untriaged, and says so;
- a tier entry whose issue is closed is a stale row, and says so.

Neither is a warning. A tracker that quietly drops a new issue is worse than
no tracker, because it reads as complete.

The two halves, and why only one is in CI
------------------------------------------
``--write`` reads the live issue list through ``gh`` and needs the network.
``--check`` re-renders from the committed map alone and diffs the page, so
it is deterministic, offline, and safe in CI — it catches a hand-edit of the
generated page, which is the drift that happens without anybody noticing.

What ``--check`` deliberately does NOT verify is freshness against GitHub.
That cannot be done offline, so the page carries the date it was derived and
the command that derives it, in the same shape this repo requires of any
recorded live value. Run ``make issues`` to refresh it.
"""

from __future__ import annotations

import argparse
import datetime
import json
import pathlib
import subprocess
import sys

import tomllib

REPO_SLUG = "doppler-dsp/doppler"
ROOT = pathlib.Path(__file__).resolve().parent.parent
MAP = ROOT / "docs" / "dev" / "issue-tiers.toml"
PAGE = ROOT / "docs" / "dev" / "issues.md"

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


def _gh(*args: str) -> list[dict]:
    out = subprocess.run(
        ["gh", *args], capture_output=True, text=True, check=True
    ).stdout
    return json.loads(out)


def load_map() -> dict:
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
        "`make issues`, which reads the live issue list. Nothing re-reads it "
        "for you, so treat the date as the age of this page — the tier "
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
        "`make issues` fails if an open issue has no tier, or if a tier names "
        "an issue that is closed — so this page cannot rot in either "
        "direction without saying so."
    )
    L.append("")
    return _mdformat("\n".join(L))


def do_write() -> int:
    live = {
        d["number"]: d["title"]
        for d in _gh(
            "issue",
            "list",
            "--repo",
            REPO_SLUG,
            "--state",
            "open",
            "--limit",
            "300",
            "--json",
            "number,title",
        )
    }
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
        "number,body",
    )
    closes: dict[int, int] = {}
    for pr in prs:
        for tok in (pr.get("body") or "").replace("#", " #").split():
            if tok.startswith("#") and tok[1:].rstrip(".,").isdigit():
                closes[int(tok[1:].rstrip(".,"))] = pr["number"]

    data = load_map()
    issues = data.get("issue", {})

    stale = sorted(int(n) for n in issues if int(n) not in live)
    untiered = sorted(n for n in live if str(n) not in issues)
    if untiered:
        print("gen_issue_tracker: open issue(s) with no tier — triage them")
        print("  in docs/dev/issue-tiers.toml before this page can render:")
        for n in untiered:
            print(f"    #{n}  {live[n]}")
        return 1
    if stale:
        print("gen_issue_tracker: tier entry/entries naming a CLOSED issue —")
        print("  delete them from docs/dev/issue-tiers.toml:")
        for n in stale:
            print(f"    #{n}")
        return 1

    for n, title in live.items():
        rec = issues[str(n)]
        rec["title"] = title
        rec["status"] = (
            f"in review ([#{closes[n]}]"
            f"(https://github.com/{REPO_SLUG}/pull/{closes[n]}))"
            if n in closes
            else "open"
        )

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
        "# with. `make issues` renders the page from this file and fails if",
        "# an open issue is missing here or a closed one is still listed.",
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
    a = ap.parse_args()
    return do_write() if a.write else do_check()


if __name__ == "__main__":
    sys.exit(main())
