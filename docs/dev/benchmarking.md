# Benchmarking

doppler runs two parallel benchmark pipelines — one Python, one C — that
both write pytest-benchmark-compatible JSON into `benchmarks/history/`.
History files are committed to git so regressions are visible in version
control.

---

## Quick start

```sh
make benchmark     # Python: benchmarks/history/<tag>.json
make benchmark-c   # C:      benchmarks/history/<tag>-c.json
```

Both targets accept a `BENCH_TAG` override for version-tagged snapshots:

```sh
make benchmark     BENCH_TAG=v1.2.3
make benchmark-c   BENCH_TAG=v1.2.3
```

---

## Python benchmarks

Python benchmarks use [pytest-benchmark](https://pytest-benchmark.readthedocs.io/).
Each module keeps its benchmarks under `src/doppler/<module>/benchmarks/`:

```text
src/doppler/accumulator/benchmarks/bench_acc.py
src/doppler/filter/benchmarks/bench_fir.py
...
```

`make benchmark` discovers all bench files under `src/doppler/`, runs them
via `pytest --benchmark-only`, and saves a dated JSON snapshot:

```sh
make benchmark    # → benchmarks/history/20260518T071221Z.json
```

Each `test_*` function is one entry in the JSON with full stats (min, max,
mean, stddev, median, IQR, ops).

### Writing a Python benchmark

```python
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

Name each test `test_<module>_<method>` so results are identifiable across
runs.

---

## C benchmarks

C benchmarks are standalone executables that write JSON directly to disk.
The `make benchmark-c` target runs all bench executables, collects the JSON
each one writes, and merges them into a combined history file.

### How they work

Each `bench_<component>_core.c` uses the header-only `jm_bench.h` library
(at `native/benchmarks/jm_bench.h`).  The pattern is:

1. **Allocate** input/output buffers and create the DSP object.
2. **Warm up** — run a few iterations before timing starts.
3. **Time** — outer loop of `ITERATIONS` independent rounds; inner loop of
   `BENCH_N` calls; `clock_gettime(CLOCK_MONOTONIC)` around the inner loop.
4. **Record** — `jm_bench_add()` stores the per-round elapsed times.
5. **Write JSON** — `jm_bench_write_json()` computes stats and writes
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
    printf("  step()  %.1f MSa/s\n",
           (double)BENCH_N / (times[0]) / 1e6);  /* approx */

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
`step()` bench with `BENCH_N = 65536`, ops is samples per second.
For a block bench parameterised by block size, set `iters =
TOTAL_PER_ROUND` so ops still reflects input samples per second.

### Parametrised block-size / rate sweeps

For algorithms where block size or rate affects throughput (resampler,
decimator), the convention is to call `jm_bench_add` once per configuration
with a bracketed name:

```c
/* "execute[rate=1.0001,block=1024]", "execute[rate=0.5000,block=65536]", … */
char name[64];
snprintf(name, sizeof(name), "execute[rate=%.4f,block=%zu]", rate, block);
jm_bench_add(&bench, name, times, ITERATIONS, TOTAL_PER_ROUND);
```

The combined JSON then has one entry per (rate, block) pair, all namespaced
under the component.

### Auto-generated bench files

`just-makeit upgrade` (schema 3→4) regenerates `bench_<component>_core.c`
for every component in `just-makeit.toml`.  The generated file already
includes `jm_bench.h`, times `step()` and `steps()` if the component has
them, and adds a timing block for every method that is not `variable_output`
and not flagged `--no-bench`.

Methods marked `variable_output = true` (those that return a
dynamically-sized array) are excluded because the bench harness cannot
determine the output buffer size at generation time.  Add timing for these
by hand if you need them.

Components with no `init_params` in the TOML (e.g. `fir`, `HalfbandDecimator`,
whose `_create()` takes filter tap arrays) emit a `/* TODO: _create(...) */`
placeholder rather than an invalid zero-arg call.  Fill this in by hand
using real filter coefficients before running those benches.

### Entry naming in history files

`c_bench_json.py` prefixes every benchmark name with its component:

```
acc_f32::step
acc_f32::steps
acc_f32::madd
acc_cf64::step
hbdecim_core::execute[block=1024]
resamp_core::execute[rate=1.0001,block=1024]
```

This prevents collisions when multiple components export methods with the
same name (`step`, `get`, `dump`, etc.).

---

## History files

Both pipelines write into `benchmarks/history/`:

```text
benchmarks/history/
├── 20260518T071221Z.json      # Python pytest-benchmark output
├── 20260518T071221Z-c.json    # C jm_bench merged output
└── ...
```

Files are committed to git.  CI generates a snapshot on every push to
`main` and on every release tag.

### Comparing snapshots

```sh
# Compare two Python snapshots with pytest-benchmark's built-in tool
uv run pytest-benchmark compare \
    benchmarks/history/20260501T000000Z.json \
    benchmarks/history/20260518T071221Z.json

# Quick Python diff — ops (samples/sec) for every entry
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
benchmarks/history/20260501T000000Z.json \
benchmarks/history/20260518T071221Z-c.json
```

The `-c.json` files share the same top-level schema as the Python files
(`machine_info`, `commit_info`, `benchmarks`, `datetime`) so the same
tooling works on both.

---

## See also

- [Adding a Module](adding-a-module.md) — full workflow for new components
- [jm_bench.h source](../../native/benchmarks/jm_bench.h) — header-only implementation
- [c_bench_json.py](../../benchmarks/c_bench_json.py) — collector script
- [just-makeit docs](https://just-buildit.github.io/just-makeit/) — scaffold and upgrade commands
