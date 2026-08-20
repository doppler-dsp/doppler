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
src/doppler/accumulator/benchmarks/bench_acc.py
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

Read that word "component" strictly — it means an object declared in
`objects/*.toml`, and everything else in the tree is invisible to it. See
[Benchmarks jm cannot see](#benchmarks-jm-cannot-see) directly below,
which is not a footnote: it is how four of doppler's benchmarks came to
be compiled by every build and run by nothing.

### Benchmarks jm cannot see

`jm bench` derives its work list from jm's component list, so a benchmark
for anything that is **not an object** is never built and never run:

- a **`c_deps` entry** — hand-owned C a module links (`conv`, `rs`,
    `ccsds_tm`, `hbdecim`, `resamp`, `timing`);
- a **function-only module** — one whose surface is free functions, with
    no object of its own (`mpsk`, `ber`, `snr`, `util`, `detection`).

This is silent in every direction. The `.c` file exists, CMake builds the
target, `jm status --check` is clean (benchmarks are not manifest-owned),
and a snapshot missing a component looks exactly like one that includes
it. Four benchmarks — `util`, `timing`, `hbdecim`, `resamp` — sat in that
state for months, appearing in no published snapshot. Proof, in one
command:

```console
$ just-makeit bench util
error: unknown component(s): util
```

**Nothing runs them here yet, and that is tracked rather than patched.**
Making `jm bench` run a non-component benchmark is
[just-makeit#1023](https://github.com/just-buildit/just-makeit/issues/1023);
doppler deliberately does not carry a local runner that duplicates jm's
collector and would be retired the day the fix ships. Ten benchmarks are
in that state — `conv`, `rs`, `ccsds_tm`, `mpsk`, `ber`, `snr`, `util`,
`timing`, `hbdecim`, `resamp`. Run one by hand:

```sh
cmake --build build --target bench_conv_core
./build/native/src/conv/bench_conv_core
```

What `scripts/check_bench_coverage.py` (on `make lint`) holds meanwhile is
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

Thirty of them are in that state today
([#891](https://github.com/doppler-dsp/doppler/issues/891)).

**This costs a C-level row, not always the measurement.** Eleven of the
thirty — `fir`, `nco`, `fft`, `fft2d`, `ddc`, `ddcr`, `corr`, `detector`,
`detector2d`, `hbdecim_q15`, `HalfbandDecimator` — have Python benchmarks
that run and publish, so those kernels *are* on `docs/benchmarks.md`
(`fir::…execute[819200]` at 74.0 → 261.4 MSa/s, `nco::…steps_u32_64k` at
3.44 GSa/s). What their empty C file costs is the face where per-call
overhead is not folded into the number — which matters most for small
blocks and per-sample methods, and least for the large-block rows already
published.

The other **nineteen are measured in no language at all**, and that is the
real gap: `mpsk_receiver`, `psd`, `specan`, `ratesync`, `pn`, `gold`,
`carrier_mpsk`, `carrier_acq`, the burst family, `acc_trace`, `ber_meter`,
`doppler_channel`, `imdmeas`, `nprmeas`, `tonemeas`, `interp_table`,
`async_dsss_receiver`. Start there.

`jm bench` does say so —

```
bench_fir_core: recorded 0 measurements -- this target measures nothing.
  EMPTY   bench_fir_core: no measurements recorded
```

— but it warns rather than fails, in a long log, from a target that runs
occasionally by design. So `check_bench_coverage.py` fails `make lint` on
a benchmark that calls `jm_bench_write_json` and never `jm_bench_add`,
with the existing thirty carried as a ratchet that may only shrink.

**Filling one in is a good first contribution**: pick a component from
#891 — one of the nineteen first — write the timing loop against its real
`_create()`, and delete its line from the ratchet.

### How they work

Each `bench_<component>_core.c` uses the header-only `jm_bench.h` library
(at `native/benchmarks/jm_bench.h`). The pattern is:

1. **Allocate** input/output buffers and create the DSP object.
1. **Warm up** — run a few iterations before timing starts.
1. **Time** — outer loop of `ITERATIONS` independent rounds; inner loop
    of `BENCH_N` calls; `clock_gettime(CLOCK_MONOTONIC)` around the inner
    loop.
1. **Record** — `jm_bench_add()` stores the per-round elapsed times.
1. **Write JSON** — `jm_bench_write_json()` computes stats and writes
    `bench_<component>_core.json` to the current working directory.

<!-- docs-snippet: skip=template scaffold (placeholder <component> token), not compilable -->

```c
#include "<component>/<component>_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N    65536
#define ITERATIONS 200

static double elapsed_sec(struct timespec a, struct timespec b) {
    return (double)(b.tv_sec - a.tv_sec)
           + (double)(b.tv_nsec - a.tv_nsec) * 1e-9;
}

int main(void) {
    comp_state_t *obj = comp_create(/* defaults */);
    float _Complex in[BENCH_N];
    /* ... fill in ... */

    jm_bench_t bench = {0};
    double times[ITERATIONS];
    struct timespec t0, t1;

    /* warmup */
    for (int i = 0; i < 16; i++) comp_step(obj, in[0]);

    for (int r = 0; r < ITERATIONS; r++) {
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < BENCH_N; i++)
            comp_step(obj, in[i]);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        times[r] = elapsed_sec(t0, t1);
    }
    jm_bench_add(&bench, "step", times, ITERATIONS, BENCH_N);

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
 *   iters  — inner calls per round (BENCH_N); ops = iters / mean
 */
void jm_bench_add(jm_bench_t *b, const char *name,
                  const double *times, int rounds, int iters);

/* Compute stats and write bench_<component>_core.json.
 * JSON schema matches pytest-benchmark output (machine_info,
 * benchmarks[], stats{min,max,mean,stddev,median,iqr,ops,...}).
 */
void jm_bench_write_json(const jm_bench_t *b, const char *component);
```

`ops = iters / mean` gives the natural throughput unit: for a scalar
`step()` bench with `BENCH_N = 65536`, ops is samples per second. For a
block bench parameterised by block size, set `iters = TOTAL_PER_ROUND`
so ops still reflects input samples per second.

### Parametrised block-size / rate sweeps

For algorithms where block size or rate affects throughput (resampler,
decimator), the convention is to call `jm_bench_add` once per
configuration with a bracketed name:

<!-- docs-snippet: skip=illustrative excerpt (bench/times/rate/block undeclared here), not standalone -->

```c
/* "execute[rate=1.0001,block=1024]", "execute[rate=0.5000,block=65536]", … */
char name[64];
snprintf(name, sizeof(name), "execute[rate=%.4f,block=%zu]", rate, block);
jm_bench_add(&bench, name, times, ITERATIONS, TOTAL_PER_ROUND);
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
clock_gettime(CLOCK_MONOTONIC, &w0);
do { run(0); clock_gettime(CLOCK_MONOTONIC, &w1); }
while (elapsed_sec(&w0, &w1) < WARMUP_S);

/* Rounds on the OUTSIDE, configurations on the inside. */
for (int r = 0; r < ITERATIONS; r++)
  for (int d = 0; d < NCONFIG; d++)
    {
      clock_gettime(CLOCK_MONOTONIC, &t0);
      run(d);
      clock_gettime(CLOCK_MONOTONIC, &t1);
      t[d][r] = elapsed_sec(&t0, &t1);
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
edits. Version-over-version comparison (`bench-check`, `bench-interleaved`)
is largely unaffected, since a consistent first-config penalty cancels
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

______________________________________________________________________

## Saving history — when and how

Benchmark history exists to catch performance regressions across
releases. That only works if every snapshot is measured the same way, so
the policy is deliberately strict about *which* snapshots are kept.

**CI owns the committed history.** The
[`benchmark.yml`](https://github.com/doppler-dsp/doppler/blob/main/.github/workflows/benchmark.yml) workflow runs
the full `just-makeit bench` (C **and** Python) on a pinned runner (fixed
OS and Python version) on **every release tag (`v*`)** and on **manual
`workflow_dispatch`**, then commits **both** trimmed snapshots —
`<tag>.json` (Python) and `<tag>-c.json` (C) — to the dedicated
`benchmarks` branch. Because the hardware and toolchain are constant,
those snapshots are directly comparable over time — that is the canonical
record.

It does **not** run on ordinary pushes to `main`: a full benchmark on
every merge would add cost and per-commit noise without a comparable
anchor. To snapshot a specific non-release commit, trigger it manually:

```sh
gh workflow run benchmark.yml -f tag=<label>
```

**There is no per-PR regression gate.** There was one — a
`perf-regression.yml` workflow that benchmarked the PR base and head on one
runner and flagged anything past 30% — and it was removed, because it reported
regressions whose sign reversed when the same comparison was run locally
([#543](https://github.com/doppler-dsp/doppler/issues/543)). An advisory gate
nobody trusts costs attention and buys nothing.

What replaced it is the local path, which is sound: **`make bench-interleaved VERSION=X.Y.Z`** builds both sides in worktrees and runs them *alternately*
K times (K=5), keeping the per-benchmark best, so drift on the machine cannot
be charged to one side. That alternation is exactly what the CI gate lacked —
it always ran base first and head second. `make bench-save` and
`make bench-compare` are still here for a two-point comparison you drive
yourself.

**Local runs are throwaway.** Run `just-makeit bench` locally to
spot-check a change before you push, but **do not commit the result**.
Developer machines differ in CPU, turbo behaviour, and background load,
so a local snapshot is not comparable to the CI baseline and would only
pollute the history. Local snapshots are written into
`benchmarks/history/` and are git-ignored — only `.gitkeep` is tracked
on `main`. Delete them freely.

**Tagging.** CI tags release snapshots with the version (`v1.2.3`); a
manual `workflow_dispatch` uses its `tag` input, or defaults to
`<date>-<sha>` when none is given. Locally the default tag is a UTC
timestamp; pass `--tag` only when you want a labelled local comparison
between two of your own runs.

In short: never commit a snapshot from your own machine — let CI record
the canonical history on the `benchmarks` branch.

______________________________________________________________________

## Comparing snapshots

```sh
# Compare two Python snapshots with pytest-benchmark's built-in tool
uv run pytest-benchmark compare \
    benchmarks/history/20260501T000000Z.json \
    benchmarks/history/20260519T120000Z.json

# Quick diff — ops (samples/sec) for every entry shared by two snapshots
python3 - <<'EOF'
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
EOF \
benchmarks/history/20260501T000000Z-c.json \
benchmarks/history/20260519T120000Z-c.json
```

The `-c.json` files share the same top-level schema as the Python files
(`machine_info`, `commit_info`, `benchmarks`, `datetime`) so the same
tooling works on both.

______________________________________________________________________

## See also

- [Adding a Module](adding-a-module.md) — full workflow for new components
- [jm_bench.h source](https://github.com/doppler-dsp/doppler/blob/main/native/benchmarks/jm_bench.h) — header-only implementation
- [just-makeit docs](https://just-buildit.github.io/just-makeit/) — scaffold, upgrade, and bench commands
