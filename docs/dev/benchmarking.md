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
recent earlier snapshot — and writes:

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

### Writing a Python benchmark

```text
"""bench_mymod.py — throughput benchmarks for doppler.mymod."""
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


def test_execute_cf32(benchmark, obj, x):
    benchmark(obj.execute_cf32, x)
```

Name each test `test_<module>_<method>` so results are identifiable
across runs.

______________________________________________________________________

## C benchmarks

C benchmarks are standalone executables that write JSON directly to
disk. `just-makeit bench` builds every `bench_<component>_core`
executable, runs each one, collects the JSON it writes, and merges them
into one combined `-c.json` snapshot.

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

```c
/* "execute[rate=1.0001,block=1024]", "execute[rate=0.5000,block=65536]", … */
char name[64];
snprintf(name, sizeof(name), "execute[rate=%.4f,block=%zu]", rate, block);
jm_bench_add(&bench, name, times, ITERATIONS, TOTAL_PER_ROUND);
```

The combined JSON then has one entry per (rate, block) pair, all
namespaced under the component.

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

**Per-PR regression gate.** A second workflow,
[`perf-regression.yml`](https://github.com/doppler-dsp/doppler/blob/main/.github/workflows/perf-regression.yml),
benchmarks the PR base (`main`) and the PR head on the *same* runner and
flags any entry that regresses past its threshold (30%) via
`just-makeit bench --check`. It is **advisory** (`continue-on-error`):
microbenchmark wall-times are too noisy in shared CI to block a merge, so
it surfaces regressions for a human to judge rather than failing the
build.

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
