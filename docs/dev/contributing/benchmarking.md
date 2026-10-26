# Benchmarking

doppler benchmarks run through a single command — `just-makeit bench` —
which builds the project, runs the C and Python benchmark suites, trims
the raw per-iteration data, and writes dated JSON snapshots to
`benchmarks/history/`.

______________________________________________________________________

## Quick start

```sh
make bench                       # C + Python (delegates to just-makeit bench)
just-makeit bench                # same thing, run directly
just-makeit bench --c-only       # C benchmarks only
just-makeit bench --python-only  # Python benchmarks only
just-makeit bench --tag v1.2.3   # label the snapshot (default: UTC timestamp)
```

Each run prints a stats table per side — with a Δ column versus the most
recent earlier snapshot. The Python side also gets a live `throughput`
summary from pytest's own terminal output (one `module::case: N MSa/s`
line per benchmark that sets `extra_info["MSa_s"]`, see below) — useful
for a quick eyeball scan before waiting on the full trimmed-snapshot diff.
Each run also writes:

```text
benchmarks/history/<tag>.json     # Python (pytest-benchmark schema)
benchmarks/history/<tag>-c.json   # C (jm_bench, merged across components)
```

`<tag>` defaults to a sortable UTC timestamp (e.g. `20260519T120000Z`);
pass `--tag` to use a version or label instead.

### Trimmed snapshots

pytest-benchmark records every individual timing sample in `stats.data`
— left in, a single run bloats the JSON to 100+ MB. `just-makeit bench`
drops those raw arrays (`stats.data` / `runtimes`) before writing,
keeping only the summary statistics. A snapshot is then a few tens of
KB — small enough to commit.

______________________________________________________________________

## Python benchmarks

Python benchmarks use [pytest-benchmark](https://pytest-benchmark.readthedocs.io/).
Each module keeps its benchmarks under `src/doppler/<module>/benchmarks/`:

```text
src/doppler/accumulator/benchmarks/bench_acc_f32.py
src/doppler/accumulator/benchmarks/bench_acc_cf64.py
src/doppler/filter/benchmarks/bench_fir.py
...
```

`just-makeit bench` discovers all bench files under `src/`, runs them
via `pytest --benchmark-only` in the project's own virtualenv, and saves
a dated JSON snapshot. Each `test_*` function is one entry in the JSON
with full summary stats (min, max, mean, stddev, median, IQR, ops).

### Testing them vs timing them

These files are exercised by **both** targets, answering different questions:

| target              | what it does with `src/doppler/*/benchmarks/`                                         |
| ------------------- | ------------------------------------------------------------------------------------- |
| `make test-python`  | runs them as **tests** — `--benchmark-disable`, under xdist, no timing                |
| `make bench-python` | **times** them — serially, because a timing taken on eight busy cores is not a timing |

So a broken benchmark script fails in the test run, where it should, and a
number is only ever produced when you asked for one.

`--benchmark-disable` is explicit rather than relied upon: pytest-benchmark
*also* disables itself whenever xdist is active, which would make the test
run's behaviour depend on `-n auto` — and `PYTEST_ARGS="-n 0"` is a supported
override, under which timing would quietly switch back on. (`-p no:benchmark`
is the wrong knob: it removes the fixture, and every benchmark test errors
with "fixture 'benchmark' not found".)

This used to be one target doing both — a second, serial pass inside
`make test-python`. In CI that was **139 s of a 268 s step on all six Python
versions**, timing code on a shared runner and discarding the numbers.
Runner timings are not trustworthy anyway; that is what
[#543](https://github.com/doppler-dsp/doppler/issues/543) deleted
`perf-regression.yml` over, and why `make bench-interleaved` exists.

### Writing a Python benchmark

```text
"""Benchmark for MyType."""
import numpy as np
import pytest
from doppler.mymod import MyType

BLOCK = 1_048_576


@pytest.fixture(scope="module")
def obj():
    return MyType(BLOCK)


@pytest.fixture(scope="module")
def x():
    return np.ones(BLOCK, dtype=np.complex64)


def test_bench_execute_cf32(benchmark, obj, x):
    benchmark(obj.execute_cf32, x)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = BLOCK / benchmark.stats["mean"] / 1e6
```

Name each test `test_bench_<case>` — `test_bench_execute_cf32`,
`test_bench_steps_64k`, etc. Case names are **not** module-qualified (every
`bench_*.py` draws from the same small vocabulary), so identifiability
across the full suite comes from the *filename*, not the test name:
`conftest.py`'s `pytest_terminal_summary` hook and `scripts/bench_report.py`
both derive a `module::case` label from the pytest fullname (stripping
`bench_`/`test_bench_` from the file/function names) when printing results,
so `bench_fir.py::test_bench_execute` shows up as `fir::execute`. Set
`benchmark.extra_info["MSa_s"]` (samples processed / mean time, in millions)
when throughput is the metric worth summarizing — it feeds both that
`module::case` terminal summary and the `docs/benchmarks.md` table.

______________________________________________________________________

## C benchmarks

C benchmarks are standalone executables that write JSON directly to
disk. `just-makeit bench` builds every **component's**
`bench_<component>_core` executable, runs each one, collects the JSON it
writes, and merges them into one combined `-c.json` snapshot.

"Component" here means every `bench_<x>_core` target CMake builds,
whether or not `objects/*.toml` declares it — see
[Benchmarks that are not jm components](#benchmarks-that-are-not-jm-components)
directly below.

### Benchmarks that are not jm components

Three kinds of benchmark get no bench target from jm, so theirs is
registered by hand in a CMakeLists:

- a **`c_deps` entry** — hand-owned C a module links (`conv`, `rs`,
    `ccsds_tm`, `resamp`, `timing`);
- a **function-only module** — one whose surface is free functions, with
    no object of its own (`mpsk`, `ber`, `snr`, `util`, `detection`);
- an **object whose component CMakeLists is hand-written** — `spectrogram`,
    which composes PSD's per-frame kernel and the ring's framed face.

`jm bench` runs them anyway: since
[just-makeit#1023](https://github.com/just-buildit/just-makeit/issues/1023)
it discovers every built bench target by scan, and doppler carries no
runner of its own. Before that shipped, four of them — `util`, `timing`,
`hbdecim`, `resamp` — were compiled by every build and run by nothing for
months, appearing in no published snapshot. Name the ones you want with
`BENCH_ARGS`, and add `--c-only`: component names filter the C side only,
and the whole Python suite would otherwise run beside them.

```sh
make bench BENCH_ARGS="--c-only conv rs"
```

A filtered run cannot become a release's numbers by either publish path.
`make bench-publish` and `make bench-interleaved` both refuse a C snapshot
that lacks any of the tree's `native/benchmarks/bench_*_core.c` (a bench
that crashed records nothing, so this catches that too), and
`make bench-interleaved` refuses a set `BENCH_ARGS` before it builds,
because its inner `make bench` runs would inherit it. It also refuses
while the clone holds a result origin/main lacks, after clearing the
outputs main already has: `make bench-preflight`, which the
[release procedure](../release.md#2b-refresh-the-benchmarks-page) runs
before checking out the commit to measure.

**How many are not jm components is printed by the gate, not written here.**
`make bench-coverage-check` counts them from the tree on every run and says
so in its OK line; a tally copied into this page would go stale the first
time a benchmark is added, which is exactly how the numbers in the section
below came to be wrong.

What `scripts/check_bench_coverage.py` (on `make lint`) holds is
everything checkable without running them: each has a CMake target, records
a measurement, and writes its JSON under the name a collector opens. That
last one is not a formality — see below.

### A benchmark writing under the wrong name

`jm_bench_write_json(&b, "X")` writes `bench_X_core.json`, and jm's
collector opens `bench_<component>_core.json`. Pass the wrong `X` and the
binary builds, runs, prints a perfectly good table, and its JSON is never
found — `_collect_c` does `if not jf.exists(): continue`.

Four files had this. Two were in the never-run set above, so it had never
had a chance to surface. **The other two were live**: `bench_awgn_core.c`
passed `"bench_awgn_core"` and `bench_wfm_synth_core.c` passed `"synth"`,
so `awgn` and `wfm_synth` were measured on every `make bench` and appear in
no published snapshot. Both are fixed, and rule 4 of the gate is what keeps
them fixed.

### A benchmark that records nothing

The reciprocal failure, and the more common one. `jm apply` scaffolds
`bench_<component>_core.c` for every object, and the scaffold is a
`main()` with a `TODO` and no `jm_bench_add` call — for a component whose
`_create()` takes arrays (`fir`, `HalfbandDecimator`) jm cannot generate
the timing loop and leaves a `/* TODO: _create(...) */` placeholder
instead. **A scaffold that is never filled in writes
`"benchmarks": []`**, so its component silently vanishes from the
snapshot while the file, the target and the `jm bench` run all exist.

Thirty were in that state when
[#891](https://github.com/doppler-dsp/doppler/issues/891) was filed.
**`HOLLOW_ALLOW` is now empty**, so every benchmark in the tree records
something. The ratchet stays, and stays enforced, because an empty set is
what keeps it that way: the gate fails on a benchmark that stops
recording, and adding a name back has to be argued for.

The last ten off it were the library's hottest kernels — `fir`, `fft`,
`nco`, `ddc`, `ddcr`, `corr`, `fft2d`, `detector`, `detector2d`,
`hbdecim_q15` — and they had survived because their kernels already reach
`docs/benchmarks.md` through a Python benchmark, so the empty C file cost
a row rather than the measurement. What that row is worth is the face
where per-call overhead is not folded into the number, which is where the
questions a signature cannot answer live: what a complex tap costs over a
real one at the same length, what the one-sample-at-a-time form a closed
loop is obliged to use costs over a block call, whether a 2-D transform is
priced by its bin count or its shape.

`jm bench` does say when a benchmark records nothing —

```
bench_fir_core: recorded 0 measurements -- this target measures nothing.
  EMPTY   bench_fir_core: no measurements recorded
```

— but it warns rather than fails, in a long log, from a target that runs
occasionally by design. So `check_bench_coverage.py` fails `make lint` on
a benchmark that calls `jm_bench_write_json` and never records.

**Write the question down, not just the loop.** A row that reports one
number for one configuration is a fact nobody can act on. Every benchmark
here opens by naming something the API does not tell a caller and closes
by answering it in a printed comparison, which is also why the
configurations are interleaved rather than run back to back: the output is
a ratio, and a ratio is only worth reading when both halves saw the same
machine. Some of those answers are *no* — the detector's chunk size and
the ring buffer's wrap position both turn out to cost nothing, and a
measured "this is free" is a result a caller can design against. Write the
prediction into the file, and if the measurement refutes it, correct the
file rather than the reading.

### How they work

Each `bench_<component>_core.c` uses the header-only `jm_bench.h` library
(at `native/benchmarks/jm_bench.h`). The pattern is:

1. **Allocate** input/output buffers and create the DSP object.
1. **Warm up** — run a few iterations before timing starts.
1. **Time** — outer loop of `ITERATIONS` independent rounds; inner loop
    of `BENCH_N` calls; `jm_bench_now_ns()` around the inner loop, and
    `jm_bench_elapsed_sec()` between the two reads. The clock lives in
    `jm_bench.h` once, so it ports once: the `bench-timer` gate refuses a
    private `clock_gettime` in `native/benchmarks`.
1. **Record** — `jm_bench_add_unit()` stores the per-round elapsed times
    and names what one round counted: its `iters` units of `unit`.
1. **Write JSON** — `jm_bench_write_json()` computes stats, writes
    `bench_<component>_core.json` to the current working directory, and
    frees the rows.

<!-- docs-snippet: skip=template scaffold (placeholder <component> token), not compilable -->

```c
#include "<component>/<component>_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>

#define BENCH_N    65536
#define ITERATIONS 200

int main(void) {
    comp_state_t *obj = comp_create(/* defaults */);
    float _Complex in[BENCH_N];
    /* ... fill in ... */

    jm_bench_t bench = {0};
    double times[ITERATIONS];

    /* warmup */
    for (int i = 0; i < 16; i++) comp_step(obj, in[0]);

    for (int r = 0; r < ITERATIONS; r++) {
        uint64_t t0 = jm_bench_now_ns();
        for (int i = 0; i < BENCH_N; i++)
            comp_step(obj, in[i]);
        uint64_t t1 = jm_bench_now_ns();
        times[r] = jm_bench_elapsed_sec(t0, t1);
    }
    jm_bench_add_unit(&bench, "step", times, ITERATIONS, BENCH_N,
                      JM_BENCH_SAMPLE);

    jm_bench_write_json(&bench, "comp");
    comp_destroy(obj);
    return 0;
}
```

### jm_bench.h API

All functions are `static` (header-only, no link dependency).

<!-- docs-snippet: skip=API signature listing, not a compilable usage example -->

```c
/* Record ITERATIONS timing samples for one benchmark entry.
 *   name   — display name, e.g. "step" or "execute[block=1024]"
 *   times  — array of per-round elapsed seconds, length rounds
 *   rounds — outer iteration count (ITERATIONS)
 *   iters  — units of work per round (BENCH_N samples for a steps())
 *   unit   — what one of those is: JM_BENCH_SAMPLE, JM_BENCH_CALL,
 *            JM_BENCH_FRAME, or a word of your own ("bin", "bit")
 */
void jm_bench_add_unit(jm_bench_t *b, const char *name,
                       const double *times, int rounds, int iters,
                       const char *unit);

/* Compute stats, write bench_<component>_core.json, then free the
 * rows (the bench is empty afterwards). JSON schema matches
 * pytest-benchmark output (machine_info, benchmarks[],
 * stats{min,max,mean,...,ops, *_per_unit}, extra_info{unit,rate_unit}).
 */
void jm_bench_write_json(jm_bench_t *b, const char *component);
```

`ops = iters / mean` is units per second, in the unit the row names:
for a scalar `step()` bench with `BENCH_N = 65536` and `JM_BENCH_SAMPLE`,
samples per second. For a block bench parameterised by block size, set
`iters = TOTAL_PER_ROUND` input samples so the rate stays per sample. The
five-argument `jm_bench_add()` still compiles, but records every row as a
*call*; doppler's bench-timer gate refuses it, so a row always says what
it counted (#2314). `dp_bench_record()` (`native/benchmarks/dp_bench.h`)
takes the unit too and forwards it.

### Parametrised block-size / rate sweeps

For algorithms where block size or rate affects throughput (resampler,
decimator), the convention is to call `jm_bench_add_unit` once per
configuration with a bracketed name:

<!-- docs-snippet: skip=illustrative excerpt (bench/times/rate/block undeclared here), not standalone -->

```c
/* "execute[rate=1.0001,block=1024]", "execute[rate=0.5000,block=65536]", … */
char name[64];
snprintf(name, sizeof(name), "execute[rate=%.4f,block=%zu]", rate, block);
jm_bench_add_unit(&bench, name, times, ITERATIONS, TOTAL_PER_ROUND,
                  JM_BENCH_SAMPLE);
```

The combined JSON then has one entry per (rate, block) pair, all
namespaced under the component.

**Interleave the configurations. Do not measure them one after the other.**
The obvious loop — for each config, warm up, then take `ITERATIONS` timings —
charges the whole clock ramp to whichever config runs first, and a per-config
warm-up does not fix it: the *first* config is warming the machine up, and
0.25 s of it was not enough on at least one dev box. Measured on
`bench_viterbi_core.c`, which compares two traceback depths over identical
trellis work: back to back, the same binary reported depth=35 at 229 ns/bit
on a cold run and 162 ns/bit on the next two, turning a real 1.42x cost for
the longer traceback into 1.03x, and once into **0.99x** — a 95-step
traceback reading as cheaper than a 34-step one, which is not a thing that
can happen.

Settle once before any config is timed, then alternate:

<!-- docs-snippet: skip=illustrative excerpt (v/dec/cap/run undeclared here), not standalone -->

```c
/* One settle for the process, not one per configuration. */
uint64_t w0 = jm_bench_now_ns ();
do { run (0); }
while (jm_bench_elapsed_sec (w0, jm_bench_now_ns ()) < WARMUP_S);

/* Rounds on the OUTSIDE, configurations on the inside. */
for (int r = 0; r < ITERATIONS; r++)
  for (int d = 0; d < NCONFIG; d++)
    {
      uint64_t t0 = jm_bench_now_ns ();
      run (d);
      t[d][r] = jm_bench_elapsed_sec (t0, jm_bench_now_ns ());
    }
```

Each config still keeps its own **min** over rounds. This is the same
principle `make bench-interleaved` applies across worktrees, at the scale of
one process: when two numbers are going to be compared to each other, any
drift must land on both.

It matters most for a benchmark whose output is a *ratio*. An absolute figure
that comes out 40% slow is visibly a slow machine; a ratio that comes out
below 1.0 looks like a finding.

**Only `bench_viterbi_core.c` does this today.** 38 of the 40 multi-config
benchmarks still time their configurations one after another with no settle
at all — [#896](https://github.com/doppler-dsp/doppler/issues/896) has the
counts and argues the loop belongs in `jm_bench.h` rather than in 38 hand
edits. Comparison between snapshots (one release's published set against
the next, and `bench-save` / `bench-compare` for the Python
microbenchmarks) is largely unaffected, since a consistent first-config penalty cancels
between snapshots; what it corrupts is the comparison between rows inside
one snapshot.

### Auto-generated bench files

`just-makeit upgrade` (schema 3→4) regenerates `bench_<component>_core.c`
for every component in `just-makeit.toml`. The generated file already
includes `jm_bench.h`, times `step()` and `steps()` if the component has
them, and adds a timing block for every method that is not
`variable_output` and not flagged `--no-bench`.

Methods marked `variable_output = true` (those that return a
dynamically-sized array) are excluded because the bench harness cannot
determine the output buffer size at generation time. Add timing for
these by hand if you need them.

Components with no `init_params` in the TOML (e.g. `fir`,
`HalfbandDecimator`, whose `_create()` takes filter tap arrays) emit a
`/* TODO: _create(...) */` placeholder rather than an invalid zero-arg
call. Fill this in by hand using real filter coefficients before running
those benches.

### Entry naming

`just-makeit bench` prefixes every C benchmark name with its component:

```
acc_f32::step
acc_f32::steps
acc_f32::madd
acc_cf64::step
HalfbandDecimator::execute[block=1024]
Resampler::execute[rate=1.0001,block=1024]
```

This prevents collisions when multiple components export methods with
the same name (`step`, `get`, `dump`, etc.).

### The benchmarks page reads every row through a label

[The benchmarks page](../../benchmarks.md) shows each published row with a
plain name, its unit and a one-sentence definition from
`benchmarks/labels.toml`, keyed by the row's ID: `c:<component>::<row>`
for a C row, `py:<module>::<test>` for a Python one. The label also says
how the row reads: `samples = N` (the input samples one recorded unit
consumes, so the row reads in samples per second), `domain`/`count` for
input that is not samples (an RS codeword read as 2,040 coded bits), or
`domain = "call"` for setup and read-outs, read as time per call. A Python
row's own rate key names its unit where it records one: `MSa_s`,
`Mbit_s`, `Mchip_s`, `Msym_s`, `Mevent_s`, `Mframe_s`, `Mrecord_s`.

**The count is checked only against a unit the page knows.** For a
sample, bin or pair the gate refuses a `samples` that contradicts the
unit (a bin is 1, a pair 2). A unit word the page has no table entry for
(`blk`, `byte`) is read through the label alone, so its count is the
author's to get right: a bench recording per `blk` of 1,024 samples needs
`samples = 1024`, and `samples = 1` would print 1/1,024 of the true rate
with nothing to refuse it. For a unit many benches will share, add it to
`UNIT_SAMPLES` or `UNIT_DOMAIN` in `scripts/bench_report.py` instead, and
the count is checked.

**Label a new row in the PR that adds it.** `make lint`
(`bench-coverage-check`) refuses a published row with neither a label nor
a line in `benchmarks/.labels-exempt`, and that list only shrinks, so a
row a release publishes for the first time cannot be exempted then. A
label for a row no release has published yet is held as pending -- the
gate checks it was added after the newest release was measured -- and
the release that first measures the row makes it live.

**Nothing checks a PR's own rows yet.** The gate sees only the rows a
release has published, so a new row with no label, a unit change that
contradicts its label, or a pending label with a mistyped ID surfaces at
the next release's `make bench-docs`, not in the PR. A PR-time check
needs each row's name and unit without timing it; that is
[#2390](https://github.com/doppler-dsp/doppler/issues/2390).

______________________________________________________________________

## Saving history — when and how

Benchmark history exists to catch performance regressions across
releases. That only works if every snapshot is measured the same way, so
the policy is deliberately strict about *which* snapshots are kept.

**The committed history is the published per-release sets.** Each release
is measured by hand, on the same representative machine, and committed
under `benchmarks/published/v<ver>/` — `portable.json` / `portable-c.json`
(the wheel's build) and `native.json` / `native-c.json`
(`-DDOPPLER_NATIVE=ON`). The [Benchmarks](../../benchmarks.md) page is
rendered from them. Shared CI runners are not hardware-representative, so
no workflow measures or commits benchmarks; the procedure is
[release step 2b](../release.md#2b-refresh-the-benchmarks-page).

**`make bench-interleaved VERSION=X.Y.Z`** produces a set. It checks out
the current commit into two worktrees, builds one portable and one native,
and runs them *alternately* K times (K=5), keeping the per-benchmark best,
so drift on the machine cannot be charged to one build. What it compares is
the two builds of one commit, not two commits.

**There is no per-PR regression gate.** There was one — a
`perf-regression.yml` workflow that benchmarked the PR base and head on one
runner and flagged anything past 30% — and it was removed, because it reported
regressions whose sign reversed when the same comparison was run locally
([#543](https://github.com/doppler-dsp/doppler/issues/543)). An advisory gate
nobody trusts costs attention and buys nothing. To compare two commits, use
`make bench-save` and `make bench-compare`, a two-point comparison you drive
yourself.

**Local runs are throwaway.** Run `just-makeit bench` locally to
spot-check a change before you push, but **do not commit the result**.
Developer machines differ in CPU, turbo behaviour, and background load,
so a local snapshot is not comparable to the published sets and would only
pollute the history. Local snapshots are written into
`benchmarks/history/` and are git-ignored — only `.gitkeep` is tracked
on `main`. Delete them freely.

**Tagging.** Locally the default tag is a UTC timestamp; pass `--tag` only
when you want a labelled local comparison between two of your own runs.

In short: never commit a `benchmarks/history/` snapshot; the only committed
numbers are a release's set under `benchmarks/published/`.

______________________________________________________________________

## Comparing snapshots

```sh
# Compare two Python snapshots with pytest-benchmark's built-in tool
uv run pytest-benchmark compare \
    benchmarks/history/20260501T000000Z.json \
    benchmarks/history/20260519T120000Z.json

# Quick diff — ops (samples/sec) for every entry shared by two snapshots
python3 - \
    benchmarks/history/20260501T000000Z-c.json \
    benchmarks/history/20260519T120000Z-c.json <<'EOF'
import json, sys

def load(path):
    with open(path) as f:
        d = json.load(f)
    return {b["name"]: b["stats"]["ops"] for b in d["benchmarks"]}

a = load(sys.argv[1])
b = load(sys.argv[2])
for name in sorted(a.keys() & b.keys()):
    pct = (b[name] - a[name]) / a[name] * 100
    print(f"  {name:<52s}  {pct:+.1f}%")
EOF
```

The `-c.json` files share the same top-level schema as the Python files
(`machine_info`, `commit_info`, `benchmarks`, `datetime`) so the same
tooling works on both.

______________________________________________________________________

## See also

- [Adding a Module](adding-a-module.md) — full workflow for new components
- [jm_bench.h source](https://github.com/doppler-dsp/doppler/blob/main/native/benchmarks/jm_bench.h) — header-only implementation
- [just-makeit docs](https://just-buildit.github.io/just-makeit/) — scaffold, upgrade, and bench commands
