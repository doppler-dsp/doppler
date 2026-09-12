#!/usr/bin/env python3
"""Gate: a validator's --check SPOT CHECK has a recorded cost, and it ratchets.

Every `sweep`-labelled validator runs twice in this repo: a full sweep, which
is characterisation and runs on demand (`make validate-c`), and a downselected
`--check` spot check, which runs on every push in the ordinary C suite. The
split exists because ratesync_scurve's full sweep was 81s and 78% of
`make test-fast`.

The split was never gated. Measured 2026-09-12: three validators added in the
preceding six days had spot checks of 87.0s, 58.7s and 53.5s -- together 60%
of the whole C suite, and the largest of them longer than the full sweep that
motivated the split. Each passed review because nothing put a number in front
of a reviewer.

This reads the timings out of a ctest run and compares them against
`scripts/.validation-time-budget`:

  * a sweep validator with NO entry fails -- a new spot check must state its
    cost, which is the case the reviewer could not see;
  * an entry exceeded by more than TOLERANCE fails;
  * an entry far BELOW its recorded value fails as a stale waiver, the same
    way the alloc-helper ratchet treats one -- a number that stayed high
    after its validator got cheaper has stopped meaning anything;
  * RAISING a recorded number is allowed with `# <reason>` on that line,
    because a spot check that genuinely needs the work is a real case a
    timer cannot see -- and the raise is detected against the BASE REF, the
    way the alloc-helper ratchet detects one. The reason governs the EDIT,
    not the measurement: an entry raised without one fails even though the
    run is inside it. Getting this wrong is how the two rules in the
    alloc-helper file came to contradict each other, and the first draft of
    this gate repeated it -- the header promised a reason would permit a
    raise while the code only printed it.

WALL-CLOCK, AND WHAT THAT COSTS. doppler#543 removed a CI wall-clock gate
because its verdicts reversed sign between CI and a dev box. That one compared
two builds across runs; this compares one run against a recorded number, which
is the easier case -- but it is the same hazard. So: TOLERANCE is 2.0x rather
than something tight, and the gate only FAILS where the machine is pinned
(the `sweep validators` CI job, or --strict). Everywhere else it reports.
The deterministic successor -- gate each validator's WORK COUNT, which is a
property of the source and not of the machine -- is doppler#1328.

Usage:
    python3 scripts/check_validation_times.py --ctest-log <file>
    python3 scripts/check_validation_times.py --ctest-log <file> --strict
    python3 scripts/check_validation_times.py --ctest-log <file> --update

Exit 0 when every spot check is within its recorded budget.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUDGET = Path(__file__).parent / ".validation-time-budget"

# ctest's per-test line: "  1/175 Test  #3: name .......   Passed    1.23 sec"
TIMED = re.compile(
    r"Test\s+#\d+:\s+(\S+)\s+\.+\s+\**(?:Passed|Failed|Timeout)\**\s+"
    r"([0-9.]+)\s+sec"
)

# Generous on purpose -- see the module docstring and doppler#543.
TOLERANCE = 2.0
# Below this, a recorded budget is a waiver outliving its reason. Only applied
# to entries big enough for the ratio to mean anything: a 0.1s floor entry
# measuring 0.02s is noise, not an improvement worth re-recording.
STALE_FACTOR = 0.4
STALE_FLOOR_S = 2.0


def parse_budget(text: str) -> dict[str, tuple[float, str]]:
    """`<test> <seconds>[  # reason]` per line -> {test: (seconds, reason)}.

    The reason is kept rather than stripped: it is what makes a RAISE
    reviewable, so it has to survive parsing.
    """
    out: dict[str, tuple[float, str]] = {}
    for raw in text.splitlines():
        if not raw.strip() or raw.lstrip().startswith("#"):
            continue
        body, _, reason = raw.partition("#")
        parts = body.split()
        if len(parts) != 2:
            continue
        try:
            out[parts[0]] = (float(parts[1]), reason.strip())
        except ValueError:
            continue
    return out


def parse_ctest(text: str) -> dict[str, float]:
    """Per-test wall seconds from a ctest run's output."""
    return {m.group(1): float(m.group(2)) for m in TIMED.finditer(text)}


def sweep_tests(build: Path) -> set[str]:
    """The `sweep`-labelled test names, asked of ctest rather than listed.

    Enumerated the same way native/validation/CMakeLists.txt labels them, so
    a validator added there is in scope here without a second edit.
    """
    r = subprocess.run(
        ["ctest", "--test-dir", str(build), "-L", "sweep", "-N"],
        capture_output=True,
        text=True,
    )
    if r.returncode != 0:
        return set()
    return {m.group(1) for m in re.finditer(r"Test\s+#\d+:\s+(\S+)", r.stdout)}


def base_budget(root: Path, ref: str) -> dict[str, tuple[float, str]] | None:
    """The budget table as of the merge base, or None if it cannot be read.

    None means "cannot tell" -- a shallow clone, a new file, a detached
    checkout -- and the caller treats that as no raise to check rather than
    as a failure, because a gate that fires on a clone's shape teaches
    people to ignore it.
    """
    rel = BUDGET.relative_to(root).as_posix()
    if (
        subprocess.run(
            ["git", "rev-parse", "--git-dir"],
            cwd=root,
            capture_output=True,
        ).returncode
        != 0
    ):
        return None
    mb = subprocess.run(
        ["git", "merge-base", "HEAD", ref],
        cwd=root,
        capture_output=True,
        text=True,
    )
    ok = mb.returncode == 0 and mb.stdout.strip()
    base = mb.stdout.strip() if ok else ref
    show = subprocess.run(
        ["git", "show", f"{base}:{rel}"],
        cwd=root,
        capture_output=True,
        text=True,
    )
    if show.returncode != 0:
        return None
    return parse_budget(show.stdout)


def render(budget: dict[str, tuple[float, str]]) -> str:
    """Rewrite the table, preserving the header and every reason."""
    header = []
    for raw in BUDGET.read_text().splitlines():
        if raw.strip() and not raw.lstrip().startswith("#"):
            break
        header.append(raw)
    width = max((len(k) for k in budget), default=0) + 2
    rows = [
        f"{k:<{width}}{v:>5.1f}" + (f"  # {r}" if r else "")
        for k, (v, r) in sorted(
            budget.items(), key=lambda kv: (-kv[1][0], kv[0])
        )
    ]
    return "\n".join(header + rows) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ctest-log", required=True, type=Path)
    ap.add_argument("--build", default="build", type=Path)
    ap.add_argument(
        "--strict",
        action="store_true",
        help="fail rather than report (the pinned CI job sets this)",
    )
    ap.add_argument(
        "--update",
        action="store_true",
        help="re-record the table from this run (the ratchet's shrink path)",
    )
    ap.add_argument(
        "--base-ref",
        default="origin/main",
        help="ref the raise check compares the table against",
    )
    a = ap.parse_args()

    if not a.ctest_log.exists():
        print(f"validation-times: no ctest log at {a.ctest_log}")
        return 1
    measured = parse_ctest(a.ctest_log.read_text(errors="replace"))
    if not measured:
        # Absent output is not a pass.
        print(
            f"validation-times: {a.ctest_log} has no per-test timings in it.\n"
            "  ctest prints them only when it actually ran tests -- an empty\n"
            "  selection would otherwise read as a clean sweep."
        )
        return 1

    budget = parse_budget(BUDGET.read_text())
    sweep = sweep_tests(ROOT / a.build)
    if not sweep:
        print(
            "validation-times: ctest reported no `sweep` tests. Either the\n"
            "  build directory is stale or the label is gone -- both mean\n"
            "  this gate is measuring nothing, so it fails rather than pass."
        )
        return 1

    ran = {t: s for t, s in measured.items() if t in sweep}
    if a.update:
        keep = {
            t: (round(max(s, 0.1), 1), budget.get(t, (0.0, ""))[1])
            for t, s in ran.items()
        }
        BUDGET.write_text(render(keep))
        print(f"validation-times: re-recorded {len(keep)} entries")
        return 0

    missing = sorted(t for t in ran if t not in budget)
    over = [
        (t, ran[t], budget[t][0], budget[t][1])
        for t in sorted(ran)
        if t in budget and ran[t] > budget[t][0] * TOLERANCE
    ]
    # Strict-only: the recorded numbers are the pinned image's, and a dev
    # box runs these 2.35x faster (measured 2026-09-12), so off CI every
    # entry would read as stale and the report would be noise.
    stale = (
        [
            (t, ran[t], budget[t][0])
            for t in sorted(ran)
            if t in budget
            and budget[t][0] >= STALE_FLOOR_S
            and ran[t] < budget[t][0] * STALE_FACTOR
        ]
        if a.strict
        else []
    )

    # A RAISED entry needs a reason on its line. Compared against the base
    # ref rather than the measurement: the reason justifies the EDIT.
    raised: list[tuple[str, float, float]] = []
    base = base_budget(ROOT, a.base_ref)
    if base is not None:
        for t, (now, reason) in budget.items():
            was = base.get(t, (None, ""))[0]
            if was is not None and now > was and not reason:
                raised.append((t, was, now))

    total = sum(ran.values())
    print(
        f"validation-times: {len(ran)} sweep spot check(s), {total:.1f}s total"
    )
    if not (missing or over or stale or raised):
        print("validation-times: OK -- every spot check within its budget")
        return 0

    for t in missing:
        print(
            f"  NOT RECORDED  {t}: {ran[t]:.1f}s, and no entry in "
            f"{BUDGET.relative_to(ROOT)}"
        )
    for t, got, want, reason in over:
        why = f"  (recorded reason: {reason})" if reason else ""
        print(
            f"  OVER BUDGET   {t}: {got:.1f}s against {want:.1f}s "
            f"recorded (x{got / want:.1f}){why}"
        )
    for t, was, now in raised:
        print(
            f"  RAISED, NO REASON  {t}: {was:.1f}s -> {now:.1f}s in "
            f"{BUDGET.relative_to(ROOT)}. A spot check that genuinely needs "
            "the work is fine, but put `# <reason>` on the line -- a commit "
            "message is read once, by someone already convinced."
        )
    for t, got, want in stale:
        print(
            f"  STALE WAIVER  {t}: {got:.1f}s against {want:.1f}s recorded "
            "-- it got cheaper; re-record so the number still means something"
        )
    print(
        "\n  A spot check is the DOWNSELECTED run, not the sweep. If it\n"
        "  needs this much work, cut what it measures (fewer dwells, one\n"
        "  C/N0, one gain) -- the full tables are `make validate-c`, where\n"
        "  the characterisation belongs and nobody pays for it per push.\n"
        "  Re-record a real improvement: make validation-time-baseline.\n"
        "  To RAISE one, put `# <reason>` on its line: a timer cannot see\n"
        "  a spot check that legitimately needs the samples."
    )
    if not a.strict:
        print(
            "\n  Reporting only: wall-clock is meaningful on the pinned CI\n"
            "  image, not on an arbitrary dev box (doppler#543). The `sweep\n"
            "  validators` job runs this with --strict."
        )
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
