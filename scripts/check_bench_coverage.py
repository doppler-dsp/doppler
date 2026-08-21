#!/usr/bin/env python3
"""Gate: a component that is tested is benchmarked, and a benchmark RUNS.

Four failure modes, all of which had already happened here, and none of
which any existing gate could see.

**1. Tested but never measured.** Five components landed between v0.42.0 and
this branch with full C test suites and no benchmark at all: `conv`
(convolutional encode/Viterbi decode), `rs` (Reed-Solomon), `ccsds_tm`
(ASM/frame/randomiser), `mpsk` (the per-symbol map/demap/soft-demap kernels)
and `ber`. Viterbi decode is the most expensive kernel in the receiver chain
and nothing in the tree timed it. `snr` and `detection` had been in the same
state since July.

**2. Measured but never run.** The reciprocal, and the worse one, because it
looks fixed. `jm bench` walks jm's COMPONENT list -- the objects in
`objects/*.toml`. `util`, `timing`, `hbdecim` and `resamp` are not objects
(one module of free functions, three `c_deps`), so their benchmarks were
built by every CMake run and executed by nothing. Four files, reviewed and
committed, appearing in no published snapshot, for months. The proof is one
command:

    $ .venv/bin/just-makeit bench util
    error: unknown component(s): util

**This gate does not fix that half, deliberately.** Making jm run them is
just-buildit/just-makeit#1023, and a local runner here would duplicate jm's
own collector and be retired the day the fix ships. So ten benchmarks in
this tree still do not run under `make bench`, tracked rather than papered
over, and run by hand meanwhile:

    cmake --build build --target bench_conv_core
    ./build/native/src/conv/bench_conv_core

What this gate holds in the meantime is everything that is checkable
without running them, which is more than it sounds: they build, they record
a measurement (rule 3), and they write their JSON under the name a
collector opens (rule 4). Modes 3 and 4 are what actually made the never-run
set worthless -- two of the four wrote to a filename nothing reads, so they
would have produced nothing even once jm could run them.

**3. Run, but recording nothing.** An unfilled jm scaffold -- a `main()`
with a `TODO` and no `jm_bench_add` call -- writes `"benchmarks": []`, so
its component vanishes from the C snapshot while the file, the target and
the `jm bench` run all exist. Thirty of doppler's benchmarks were in that
state when this gate was written (doppler#891).

**The current set is `HOLLOW_ALLOW` below, and this paragraph does not
restate it.** It used to: it named eleven components and a tally of thirty,
and by the time anyone read it again the tally was ten, `HalfbandDecimator`
had been filled in and removed, and the "measured in no language at all"
group the text called *the actual hole* had been closed outright. Every one
of those numbers was true when written and none of them was kept true by
anything -- which is the same failure this file exists to gate, committed
in the file's own docstring. So the list lives in one place, the ratchet,
and the gate prints what is left rather than asserting it here.

Read a hollow entry as a missing C-LEVEL row, not a missing measurement:
what survives on the ratchet are components whose kernels DO reach
`docs/benchmarks.md` through a Python benchmark. What the empty C file
costs them is the face where per-call overhead is not folded into the
number -- which matters most for small blocks and per-sample methods, and
least for the large-block rows already published.

**4. Writing under a name nothing reads.** `jm_bench_write_json(&b, "X")`
writes `bench_X_core.json`, and both collectors open
`bench_<component>_core.json`. `bench_hbdecim_core.c` passed
`"hbdecim_core"` and `bench_resamp_core.c` passed `"resamp_core"`, so each
wrote a `bench_<name>_core_core.json` nothing opens. Both are in mode 2's
never-run set, which is why it had never surfaced: the bug needs the binary
to actually run before it can be seen, and running them is what found it.

jm is not silent about mode 3: it prints `EMPTY bench_fir_core: no
measurements recorded`. But it warns rather than fails, in a long log, from
a target that runs occasionally by design (`make bench` is the rare
activity; see `bench-python`). Thirty of those warnings printed on every run
for months and none was acted on. This gate is on `make lint` -- every PR,
and red rather than yellow. That is the whole difference.

All four rules are derived, not registered. The component set comes from
jm's own loader and the benchmark set from the tree, so adding a component
or a benchmark updates the gate by existing. There is no list of
what-is-benchmarked here to go stale -- which matters, because such a list
is the same shape as the four benchmarks nobody noticed were dead.

Rules
-----
1. COVERAGE -- every `native/inc/<c>/` with a C test has a C benchmark.
   A module umbrella header with no test of its own is exempt by
   construction: its objects carry the tests and the benchmarks.
2. BUILDABILITY -- every `native/benchmarks/bench_<x>_core.c` for a
   NON-component has a CMake target, since jm generates one only for
   components and nothing can run what CMake does not declare. Whether
   those targets are then RUN is just-makeit#1023's half; see below.
3. SUBSTANCE -- a benchmark that calls `jm_bench_write_json` also calls
   `jm_bench_add`, so the snapshot gets a row rather than an empty array.
4. NAMING -- the component a benchmark writes its JSON under matches the
   one it is collected as. `jm_bench_write_json(&b, "X")` writes
   `bench_X_core.json`; pass the wrong X and the binary runs, prints its
   table, and its results are never opened by anything.

Both allowlists are RATCHETS and may only shrink. An entry needs a reason,
and "it is only a few flops" is not one -- `bench_util_core.c` exists
precisely to measure three flops, because the question was whether a shared
inline cost anything, not whether an EMA is fast. The gate also fails on a
STALE entry: a component that starts measuring must lose its line.

Usage:  python3 scripts/check_bench_coverage.py
Exit 0 when every tested component is benchmarked and every benchmark runs.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INC = ROOT / "native" / "inc"
TESTS = ROOT / "native" / "tests"
BENCH = ROOT / "native" / "benchmarks"

#: component -> why it is legitimately unmeasurable. RATCHET: may only
#: shrink, and it is down to ONE. `buffer` and `wfm_compose` both came off
#: it once their own benchmarks existed, and both waivers had said the
#: quiet part out loud: the ring's push/pop "is a real hot path and should
#: be measured", and the composer was "benchmarked indirectly through
#: wfm_synth only". Indirectly is not a reason, it is the gap -- measuring
#: the engine underneath says nothing about the layer on top, and the
#: composer's whole contribution happens at a segment boundary that a
#: one-segment spec never crosses.
#:
#: What is left is the one case where "measured elsewhere" is true rather
#: than a euphemism: a component whose kernel really does run under another
#: component's benchmark, at the level where a caller constructs it.
ALLOW: dict[str, str] = {
    "hbdecim": "doppler#893 — measured at its composing object instead. "
    "bench_HalfbandDecimator_core.c carries the block sweep this component's "
    "own benchmark used to, capped at the object's real HBDECIM_MAX_OUT "
    "limit; the c_dep beneath has no such limit, so the old file measured a "
    "block size no caller can request. Not unmeasured — measured one level "
    "up, where a caller constructs.",
}

#: benchmarks that BUILD and RUN but record no measurement -- a `bench_*.c`
#: with no recording call writes `"benchmarks": []`, so the component is
#: silently absent from every C snapshot.
#:
#: RATCHET: may only shrink, and it has reached EMPTY. Every benchmark in
#: the tree now records something, so this set exists to stay empty rather
#: than to hold anything: an entry added here would have to be argued for,
#: and the gate still FAILS on an entry whose benchmark does record, so it
#: cannot rot in either direction.
#:
#: It got here in three passes. First the benchmarks measured in no
#: language at all -- the group doppler#891 called the actual hole. Then
#: `HalfbandDecimator`, measured one level up at its composing object.
#: Last, the ten hottest kernels in the library (`fir`, `fft`, `nco`,
#: `ddc`, `ddcr`, `corr`, `fft2d`, `detector`, `detector2d`,
#: `hbdecim_q15`), which had a Python face and so lost only the C-level
#: row -- the face where per-call overhead is not folded into the number.
#: Tracked as doppler#891.
HOLLOW_ALLOW: set[str] = set()

#: A recorded measurement, at the start of a line or after whitespace --
#: not inside a comment, which is how the scaffolds "mention" it while
#: calling nothing.
#:
#: TWO spellings, because there are two ways to record and the gate must
#: see both: `jm_bench_add` direct, or `dp_bench_record` from doppler's own
#: native/benchmarks/dp_bench.h, which records AND prints the min-derived
#: rate. The alias is only safe while it really does record, so
#: `_alias_records()` below re-derives that from the header on every run
#: rather than trusting this comment -- a helper that stopped calling
#: `jm_bench_add` would otherwise hand every caller a way to look measured
#: while writing an empty array, which is precisely the failure this whole
#: file exists to catch, reintroduced one level down.
_ADD_CALL = re.compile(r"^[^*/]*\b(?:jm_bench_add|dp_bench_record)\s*\(", re.M)

#: The helper the alias above trusts.
_ALIAS_HEADER = BENCH / "dp_bench.h"
_ALIAS_NAME = "dp_bench_record"


def _alias_records() -> bool:
    """Does `dp_bench_record` actually call `jm_bench_add`?

    Derived, not assumed. If the helper is ever refactored into something
    that prints without recording, every benchmark routed through it goes
    hollow at once and rule 3 would wave all of them through.
    """
    if not _ALIAS_HEADER.exists():
        return False
    text = _ALIAS_HEADER.read_text(encoding="utf-8")
    body = text.partition(_ALIAS_NAME + " (")[2]
    return bool(re.search(r"^[^*/]*\bjm_bench_add\s*\(", body, re.M))


#: The component name a benchmark writes its JSON under.
_WRITE_CALL = re.compile(
    r'^[^*/]*\bjm_bench_write_json\s*\([^,]+,\s*"([^"]+)"', re.M
)


def jm_components() -> set[str]:
    """The component names `jm bench` iterates -- objects, nothing else."""
    from just_makeit import _config as jm_config

    return set(jm_config.components(jm_config.load(ROOT)))


def _attribute(path: Path, prefix: str, names: set[str]) -> str | None:
    """Which component does `<prefix>_<stem>.c` belong to?

    LONGEST match, and that is the whole subtlety. A glob is wrong here:
    `bench_mpsk_*.c` matches `bench_mpsk_receiver_core.c`, so the first
    version of this gate credited the `mpsk` constellation kernels with
    `mpsk_receiver`'s benchmark and passed both `mpsk` and `ber` -- the
    two components the audit that prompted this file had started from.
    A gate that reports coverage from a NEIGHBOUR's artifact is worse
    than no gate, so attribution goes to the longest component name the
    filename actually starts with.
    """
    stem = path.name[len(prefix) + 1 : -len(".c")]
    cands = [n for n in names if stem == n or stem.startswith(n + "_")]
    return max(cands, key=len) if cands else None


def tested(names: set[str]) -> set[str]:
    """Components with at least one C test TU.

    A hand-owned component may split across TUs (`ccsds_tm` is
    `test_ccsds_tm_{asm,conv,frame,rand,rs}.c`) and those count -- a
    component tested in five files is more thoroughly exercised than one
    tested in a single file, not less.
    """
    got = {_attribute(p, "test", names) for p in TESTS.glob("test_*.c")}
    return got - {None}


def benched(names: set[str]) -> set[str]:
    """Components with at least one C benchmark TU.

    `bench_<c>.c` counts alongside `bench_<c>_core.c`: `stream`'s
    benchmark is `bench_stream.c`, run by its own `make bench-stream`
    target because it needs a live NATS broker, and that is measurement
    that happens -- which is the only thing this gate is asking about.
    """
    got = {_attribute(p, "bench", names) for p in BENCH.glob("bench_*.c")}
    return got - {None}


def cmake_targets() -> set[str]:
    """Every `add_executable(...)` name declared anywhere in the tree.

    Read across a newline: jm emits the target name on the line AFTER
    `add_executable(`, so a single-line regex silently finds none of the
    generated bench targets and this rule would pass by seeing nothing.
    """
    names: set[str] = set()
    pat = re.compile(r"add_executable\(\s*([A-Za-z0-9_]+)")
    for f in [ROOT / "CMakeLists.txt", *ROOT.glob("native/**/CMakeLists.txt")]:
        names |= set(pat.findall(f.read_text(encoding="utf-8")))
    return names


def main() -> int:
    comps = jm_components()
    targets = cmake_targets()
    failures: list[str] = []

    # Every name a test or benchmark TU could belong to. jm's components and
    # the hand-owned `native/inc/` directories are different sets and both
    # are needed: `ccsds_tm` has a directory and is no component, while a
    # composed object like `Resampler` is a component with no directory.
    names = comps | {p.name for p in INC.iterdir() if p.is_dir()}
    have_bench = benched(names)

    # Rule 1 -- tested implies benchmarked.
    for d in sorted(tested(names)):
        if d in have_bench or d in ALLOW:
            continue
        failures.append(
            f"{d}: has C tests (native/tests/test_{d}_*.c) and no benchmark. "
            f"Write native/benchmarks/bench_{d}_core.c, or add {d} to ALLOW "
            "in this file with the reason it cannot be measured."
        )

    # Rule 2 -- benchmarked implies run.
    for p in sorted(BENCH.glob("bench_*_core.c")):
        name = p.name[len("bench_") : -len("_core.c")]
        if name in comps:
            continue  # jm builds and runs it
        target = f"bench_{name}_core"
        if target not in targets:
            failures.append(
                f"{name}: {p.relative_to(ROOT)} is not a jm component and has "
                f"no CMake target ({target}), so nothing can build it. "
                "Register it in CMakeLists.txt beside bench_util_core."
            )

    # Rule 3 -- a benchmark records a measurement. A `bench_*.c` that never
    # calls `jm_bench_add` writes an empty array, so the component vanishes
    # from the snapshot while every other signal (the file exists, the target
    # builds, jm runs it, `jm status --check` is clean) says it is covered.
    for p in sorted(BENCH.glob("bench_*.c")):
        comp = _attribute(p, "bench", names)
        if comp is None or comp in HOLLOW_ALLOW:
            continue
        text = p.read_text(encoding="utf-8")
        if _ADD_CALL.search(text):
            continue
        # bench_stream.c reports through its own harness rather than jm's
        # JSON, and `make bench-stream` is what runs it.
        if not text.count("jm_bench_write_json"):
            continue
        failures.append(
            f"{comp}: {p.relative_to(ROOT)} calls jm_bench_write_json but "
            "never jm_bench_add, so it writes an empty benchmark array and "
            "the component is absent from every snapshot. Add a timing loop, "
            "or add it to HOLLOW_ALLOW (ratchet — it may only shrink)."
        )

    # Rule 4 -- the name a benchmark WRITES matches the one it is read under.
    # `jm_bench_write_json(&b, "X")` writes `bench_X_core.json`, and both
    # collectors look for `bench_<component>_core.json`. Pass "hbdecim_core"
    # from bench_hbdecim_core.c and it writes bench_hbdecim_core_core.json,
    # which nothing opens -- the binary runs, prints its table, and vanishes.
    # Two of the four never-run benchmarks had exactly this, undetectable
    # until something executed them.
    for p in sorted(BENCH.glob("bench_*_core.c")):
        comp = _attribute(p, "bench", names)
        if comp is None:
            continue
        m = _WRITE_CALL.search(p.read_text(encoding="utf-8"))
        if m and m.group(1) != comp:
            failures.append(
                f"{comp}: {p.relative_to(ROOT)} calls jm_bench_write_json "
                f'with "{m.group(1)}", so it writes '
                f"bench_{m.group(1)}_core.json — but it is collected as "
                f"bench_{comp}_core.json and the file is never found. "
                f'Pass "{comp}".'
            )

    # The alias rule 3 accepts has to keep its side of the bargain.
    if not _alias_records():
        failures.append(
            f"{_ALIAS_HEADER.relative_to(ROOT)}: rule 3 accepts "
            f"`{_ALIAS_NAME}` as a recorded measurement, but that function "
            "no longer calls jm_bench_add. Every benchmark routed through "
            "it now writes an empty array while still passing this gate. "
            "Restore the call, or drop the alias from _ADD_CALL."
        )

    # Same ratchet discipline for ALLOW, which is exempt from rule 1 rather
    # than from rule 3. The docstring above has claimed since this file was
    # written that the gate fails on a stale entry -- and for ALLOW it did
    # not, so the claim was prose and the list was free to rot in the one
    # direction that matters. A component that starts being measured must
    # lose its line, and a line naming something that is neither a
    # component nor a native/inc directory is a rename nobody finished.
    for comp in sorted(ALLOW):
        if comp in have_bench:
            failures.append(
                f"{comp}: is in ALLOW but native/benchmarks/bench_{comp}"
                "_*.c now exists. Delete its line — the ratchet may only "
                "shrink."
            )
        elif comp not in names:
            failures.append(
                f"{comp}: is in ALLOW but is neither a jm component nor a "
                "native/inc/ directory, so the entry exempts nothing. "
                "Delete it, or fix the name."
            )

    # A ratchet may only shrink: an entry for a component that now records a
    # measurement, or has no benchmark at all, is stale and must be deleted.
    for comp in sorted(HOLLOW_ALLOW):
        p = BENCH / f"bench_{comp}_core.c"
        if p.exists() and _ADD_CALL.search(p.read_text(encoding="utf-8")):
            failures.append(
                f"{comp}: is in HOLLOW_ALLOW but bench_{comp}_core.c now "
                "records a measurement. Delete its line — the ratchet may "
                "only shrink."
            )

    if failures:
        print("check_bench_coverage: FAIL\n", file=sys.stderr)
        for f in failures:
            print(f"  - {f}\n", file=sys.stderr)
        return 1

    n_extra = len(
        {
            p.name[len("bench_") : -len("_core.c")]
            for p in BENCH.glob("bench_*_core.c")
        }
        - comps
    )
    print(
        "check_bench_coverage: OK — every tested component is benchmarked "
        f"and every benchmark records under a collectable name ({n_extra} "
        "of them still await just-makeit#1023 to be RUN by `make bench`)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
