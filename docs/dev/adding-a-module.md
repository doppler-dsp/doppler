# Adding a New C Extension Module

This guide walks through every step required to add a new DSP module to
doppler using [just-makeit](https://just-buildit.github.io/just-makeit/)
(`jm`). The scaffold handles boilerplate; you fill in the DSP logic.

For the layout rules that govern each generated file, see
[Module Layout](module-layout.md).

______________________________________________________________________

## Prerequisites

Drive doppler with the **exact pinned jm version** — a stale local install
warns on every command (version skew), and an unpinned `uvx just-makeit`
can silently resolve to a newer release than the one doppler's manifest
was written against:

```sh
uvx --from 'just-makeit==0.28.11' jm --help
```

(`0.28.11` is the current pin — check
[`just-makeit.toml`](https://github.com/doppler-dsp/doppler/blob/main/just-makeit.toml)'s
`jm_version` for the live value.) The scaffold writes into the doppler
source tree, so run all commands from the repo root.

**Always diff after any mutating `jm` command** (`object`, `method`,
`property`, `add`, `remove`, and — see Step 11 — a bare `apply`). Confirmed
on jm 0.29.1: these commands can rewrite files well outside the component
you touched —

- Every `jm object` / `jm method` / `jm remove object` call reformats
    **every** `objects/*.toml` fragment (reordering keys, collapsing
    multi-line arrays, stripping hand-written comments) even though only
    one object changed semantically. Cosmetic, but don't commit the noise:
    `git checkout -- objects/ just-makeit.toml` after the command, then
    reapply just your own manifest edit by hand.
- A **bare** `jm apply` (no fragment path) can regenerate a sibling
    object's sacred `<module>_ext_<component>.c` fragment in the same
    module — silently discarding hand-patches (a corrected constructor
    default, an explanatory comment, a UAF fix) even though that object
    was never touched. This is not cosmetic — it reintroduces real bugs.
    Prefer the **scoped** form, `jm apply objects/<component>.toml`, and
    if you do run a bare `jm apply`, check `git diff` for every
    `*_ext_*.c` file, not just the one you meant to change.

These are being filed upstream as just-makeit issues; this note stays
until they're fixed and the pin bumped past the fix.

______________________________________________________________________

## Step 0 — Characterize the algorithm

Ask one question: **can a single input sample be processed independently,
with small fixed state?**

- **Yes** → a plain step/steps object. Use the CLI default (Step 1,
    Entry point A).
- **No** — the algorithm owns block I/O (it needs a window, a whole
    buffer, or produces output whose length isn't known until runtime) →
    use a `--preset` (Step 1, Entry point B):
    - Block processor / decimator / resampler / FFT / correlator →
        `--preset blockwise`
    - Signal generator (void input, array output) → `--preset generator`

______________________________________________________________________

## Step 1 — Declare the interface

**Entry point A: plain step/steps object**

```sh
jm object myobj --module mymodule --state gain:double:1.0 --mutable
jm property myobj gain --module mymodule --type double --writable --field
```

`object` is for stateful DSP types with a create/destroy lifecycle. The
default scaffold generates `step()`/`steps()` — single-sample and
block-processing methods.

**Entry point B: `--preset` for block-I/O objects**

A block-I/O object owns its output buffer (`--no-state --no-step`, opaque
heap state jm can't infer), so it's a `--preset` for the object shape
**plus** an explicit `variable_output` method — `jm method … --variable-output`
is what adds the lazy-alloc, grow-on-demand output buffer (add
`--pass-capacity` for the 5-arg `(…, out, max_out)` form):

```sh
# Block processor (decimator / resampler / FFT / correlator):
jm object myobj --preset blockwise --module mymodule \
  --no-state --no-step --class-name MyObj --init-param n:size_t:1024
jm method myobj execute --module mymodule \
  --arg-type "float _Complex" --return-type "float _Complex" --variable-output
jm property myobj n --module mymodule --type size_t --field

# Signal generator (void input, array output: NCO, LO, tone/PN gen):
jm object mysrc --preset generator --module mymodule \
  --no-state --no-step --class-name MySrc --init-param norm_freq:double:0.0
jm method mysrc steps --module mymodule \
  --arg-type void --return-type "float _Complex" --variable-output
```

Both entry points route their CLI mutations to `objects/<component>.toml`
automatically (jm's split manifest layout) — you don't hand-edit TOML for
a plain `object`/`method`/`property` call.

**`function`** is for module-level operations with no persistent state —
window functions, unit conversions, design helpers, anything that takes
inputs and returns a result without a lifecycle:

```sh
jm function <fn_name> --module <name> \
    --param "x:float _Complex[]" --return-type "float _Complex"
```

A module can have any mix of objects and functions.

______________________________________________________________________

## Step 2 — Register in the module manifest

New module, new object: add it to `just-makeit.toml`'s module block (a
brand-new module needs this; adding an object to an *existing* module is
already handled by `--module <name>` in Step 1):

```toml
# just-makeit.toml
[module.mymodule]
objects = ["myobj"]
```

______________________________________________________________________

## Step 3 — Apply

```sh
jm apply objects/myobj.toml
```

Materializes: `<component>_core.h` stub, `<component>_core.c` stub,
`<module>_ext.c` (regenerated), `CMakeLists.txt`, `__init__.py`, `.pyi`,
C test, C benchmark, Python test skeleton, Python benchmark skeleton.
After this:

```text
native/inc/<module>/
├── <module>_core.h             # module-level function declarations
└── <component>_core.h          # per-object C header

native/src/<module>/
├── <module>_core.c             # module-level function implementations
├── <module>_ext.c              # aggregator — jm-regenerated, do not edit
├── <module>_ext_<component>.c  # per-object binding fragment — hand-owned
├── <component>_core.c          # algorithm skeleton (fill in)
└── CMakeLists.txt              # auto-managed

native/tests/
└── test_<component>_core.c     # C test skeleton

native/benchmarks/
└── bench_<component>_core.c    # C benchmark skeleton

src/doppler/<module>/
├── __init__.py                 # re-export stub
└── <module>.pyi                # type stub skeleton
```

`tests/` and `benchmarks/` under `src/doppler/<module>/` are not
scaffolded — you create them in Steps 9 and 10.

______________________________________________________________________

## Step 4 — Implement the C core

Open `native/src/<module>/<component>_core.c`. The primary thing to
implement is `step()` (for a Step-1-Entry-A object — the scaffold
generates `steps()` as an inline loop over `step()`, so it comes for
free) or the `execute`/`steps` method body you declared (for a
Step-1-Entry-B object). Any other methods you declared via `jm method`
also need their bodies filled in.

<!-- docs-snippet: skip=template scaffold (placeholder <module>/<component> tokens), not compilable -->

```c
/* native/src/<module>/<component>_core.c */
#include "<module>/<component>_core.h"

float _Complex
dp_<component>_step(dp_<component>_t *s, float _Complex x)
{
    /* DSP logic for one sample */
    return x * s->gain;
}
```

The lifecycle functions (`create`, `destroy`, `reset`) are scaffolded and
only need changes if your object allocates extra memory or has
non-trivial initialization beyond what the generated struct assignment
already does.

Keep this file algorithm-only. No Python headers, no NumPy, no `Py_*`
calls — those belong exclusively in `<module>_ext_<component>.c`, and
even there only for genuinely bespoke binding logic (dtype dispatch,
non-trivial argument validation) that jm can't generate; ordinary
methods need no hand-written binding code at all.

______________________________________________________________________

## Step 4b — Make it serializable (REQUIRED for every stateful object)

**If the object carries any running state that survives between calls**
(a phase, a delay line, an accumulator, an integrator, a ring, an RNG) it
**must** implement the standard state triplet. This is not optional:
elastic resume (checkpoint / migrate / scale across threads, processes,
pods) depends on *every* stateful object speaking the one bytes
interface. Only genuinely stateless objects (pure converters, FFT plans,
by-value analyzers) are exempt. See
[State Serialization](../design/state-serialization.md) for the full
design.

1. **C core** — `#include "dp_state.h"` in `<component>_core.h`, declare
    a per-object `#define <COMPONENT>_STATE_MAGIC DP_FOURCC(...)` +
    `<COMPONENT>_STATE_VERSION 1u`, and the triplet:

    <!-- docs-snippet: skip=template scaffold (placeholder <component> tokens), not compilable -->

    ```c
    size_t <component>_state_bytes (const <component>_state_t *s);
    void   <component>_get_state   (const <component>_state_t *s, void *blob);
    int    <component>_set_state   (<component>_state_t *s, const void *blob);
    ```

    Implement them in `<component>_core.c` (sibling to `<component>_reset`)
    with the cursor helpers: `dp_w_hdr` then pack the **running** fields
    (config is restored by `create()`); `set_state` opens with
    `dp_state_validate(...)` and returns its result.

1. **Declare it** — set `serializable = "true"` in
    `objects/<component>.toml` (or pass `--serializable` to `jm object` in
    Step 1), then `jm apply`: jm generates the Python triplet
    (`state_bytes()` / `get_state() -> bytes` / `set_state(bytes)`) **and**
    the `.pyi` stubs — no hand-written binding glue needed.

1. **Test it in both harnesses** (C: a mid-stream split that resumes
    bit-for-bit from `get_state`/`set_state`, plus an envelope reject; the
    `DP_STATE_ROUNDTRIP_TEST` macro in `native/tests/dp_state_test.h`
    covers the common shape. Python: add the type to the parametrized
    matrix `src/doppler/tests/test_state_serialization.py`) — see Step 9.

______________________________________________________________________

## Step 5 — Add extra methods or properties *(optional)*

Only run these if you need additional methods (e.g. dtype-specific
execute paths) or properties beyond what Step 1 already declared.

```sh
jm method <component> execute_cf32 --module <module> \
    --arg-type "float _Complex[]" \
    --out-type "float _Complex"
```

For read-only C struct fields exposed as Python properties:

```sh
jm property <component> n --module <module> --type size_t --field
```

______________________________________________________________________

## Step 6 — Write C tests

Open `native/tests/test_<component>_core.c` (generated skeleton) and add
test cases using the [Unity](https://github.com/ThrowTheSwitch/Unity) test
framework that the project ships:

<!-- docs-snippet: skip=template scaffold (placeholder dp_mytype_* names), not compilable -->

```c
void test_execute_passthrough(void) {
    dp_mytype_t *s = dp_mytype_create(256);
    TEST_ASSERT_NOT_NULL(s);

    float _Complex in[256] = { [0] = 1.0f + 0.0f * _Complex_I };
    float _Complex out[256];
    dp_mytype_execute_cf32(s, in, 256, out);

    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, crealf(out[0]));
    dp_mytype_destroy(s);
}
```

If Step 4b applies, add the state round-trip test here too (see
[State Serialization](../design/state-serialization.md)).

Run the C suite:

```sh
make test
```

All C tests must pass before moving to the Python layer.

______________________________________________________________________

## Step 7 — Write `<module>.pyi`

Open `src/doppler/<module>/<module>.pyi` (generated skeleton) and flesh
out every public type following the
[module layout rules](module-layout.md). Docstrings use
[numpy-style format](https://numpydoc.readthedocs.io/en/latest/format.html#docstring-standard):

```text
# mymod/mymod.pyi — type stubs for the mymod C extension.
import numpy as np
from numpy.typing import NDArray

class <component>:
    """One-line summary.

    Parameters
    ----------
    n:
        Block size.  Must be a power of two.

    Examples
    --------
    Construct and inspect an instance:

    >>> import numpy as np
    >>> from doppler.mymod import <component>
    >>> obj = <component>(256)
    >>> obj.n
    256

    Process a block of CF32 samples:

    >>> x = np.ones(256, dtype=np.complex64)
    >>> y = obj.execute_cf32(x)
    >>> y.dtype
    dtype('complex64')
    >>> y.shape
    (256,)

    """

    n: int
    """Block size passed to the constructor."""

    def execute_cf32(
        self, x: NDArray[np.complex64]
    ) -> NDArray[np.complex64]:
        """Process one block of CF32 samples."""
        ...
```

Verify the examples pass:

```sh
python -m doctest -v src/doppler/<module>/<module>.pyi
```

Fix any failures before continuing.

______________________________________________________________________

## Step 8 — Write `__init__.py`

Open `src/doppler/<module>/__init__.py` and update the re-export:

```text
from .<module> import <component>

__all__ = ["<component>"]
```

`__all__` **is** the public API. It controls what
`from doppler.<module> import *` exposes, what IDEs surface in
autocompletion, and what users can reasonably rely on. Make sure every
symbol a user should be able to reach is listed — if it isn't here, it
isn't public. Conversely, every name in `__all__` must have a
corresponding stub in `<module>.pyi`; the two lists must stay in sync.

Nothing else belongs here. See [Module Layout](module-layout.md).

______________________________________________________________________

## Step 9 — Write Python tests

Open `src/doppler/<module>/tests/test_<module>.py` and write `pytest`
tests that exercise the Python API end-to-end through the C extension.

At minimum, cover these categories:

- **Construction** — valid arguments create the object; invalid arguments
    raise the expected exception (`TypeError`, `ValueError`, etc.)
- **Output shape and dtype** — every execute path returns an array of the
    correct shape and dtype for each supported input type
- **Correctness** — known input produces known output; verify numerically
    against a reference (e.g. `np.allclose`)
- **DSP design requirements** — DSP algorithms carry quantitative targets
    (filter attenuation, SFDR, passband ripple, decimation accuracy, etc.) that
    must be verified, not assumed. Test these thoroughly over repeatable
    pseudo-random inputs and/or swept parameter ranges so regressions are caught
    automatically. Use a fixed `np.random.default_rng(seed)` for
    reproducibility.
- **`step` / `steps` consistency** — a block processed via `steps()` matches
    the same samples processed one-at-a-time via `step()`
- **Properties** — read-only properties return the values passed at construction
- **State round-trip** *(if Step 4b applies)* — add the type to the
    parametrized matrix in `src/doppler/tests/test_state_serialization.py`;
    it auto-checks bit-exact resume plus size/clobber/non-bytes rejects.

```text
import numpy as np
import pytest
from doppler.<module> import <component>


def test_construction_invalid():
    with pytest.raises(ValueError):
        <component>(-1)


def test_output_shape_and_dtype():
    obj = <component>(256)
    x = np.ones(256, dtype=np.complex64)
    y = obj.execute_cf32(x)
    assert y.shape == (256,)
    assert y.dtype == np.complex64


def test_correctness():
    obj = <component>(256)
    x = np.ones(256, dtype=np.complex64)
    y = obj.execute_cf32(x)
    expected = ...  # compute reference result
    assert np.allclose(y, expected, atol=1e-5)


@pytest.mark.parametrize("seed", [0, 1, 2, 3, 4])
def test_dsp_design_requirements(seed):
    # Example: verify stopband attenuation meets the design target.
    # Use a fixed-seed RNG so failures are reproducible.
    rng = np.random.default_rng(seed)
    obj = <component>(256)
    x = (rng.standard_normal(4096) +
         1j * rng.standard_normal(4096)).astype(np.complex64)
    y = obj.execute_cf32(x)
    # Measure stopband power and assert it meets the spec (example: -60 dB).
    stopband = np.abs(np.fft.fft(y)[128:384]) ** 2
    attenuation_db = 10 * np.log10(stopband.mean())
    assert attenuation_db < -60.0


def test_step_steps_consistency():
    obj_a = <component>(256)
    obj_b = <component>(256)
    x = np.random.randn(256).astype(np.float32) + \
        1j * np.random.randn(256).astype(np.float32)
    via_steps = obj_a.steps(x.astype(np.complex64))
    via_step  = np.array([obj_b.step(s) for s in x.astype(np.complex64)])
    assert np.allclose(via_steps, via_step, atol=1e-6)
```

Run the Python suite:

```sh
make python-test
```

______________________________________________________________________

## Step 10 — Write the Python benchmark

Fill in `src/doppler/<module>/benchmarks/bench_<module>.py` (scaffolded
empty by `jm apply` in Step 3). Benchmarks use
[pytest-benchmark](https://pytest-benchmark.readthedocs.io/) so results are
collected into versioned JSON snapshots in `benchmarks/history/` and tracked
in git for regression detection.

For a full description of both the Python and C benchmark pipelines, history
file format, and how to compare snapshots, see [Benchmarking](benchmarking.md).

```text
"""Benchmark for <component>."""
import numpy as np
import pytest
from doppler.<module> import <component>

BLOCK = 1_048_576  # samples per benchmark round


@pytest.fixture(scope="module")
def obj():
    return <component>(BLOCK)


@pytest.fixture(scope="module")
def x_cf32():
    return np.ones(BLOCK, dtype=np.complex64)


def test_bench_execute_cf32(benchmark, obj, x_cf32):
    benchmark(obj.execute_cf32, x_cf32)
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = BLOCK / benchmark.stats["mean"] / 1e6
```

pytest-benchmark handles warmup and repetition automatically. Name each
test `test_bench_<case>` — **not** module-qualified; every `bench_*.py`
file draws from the same small case vocabulary, so identifiability across
the full suite comes from the *filename* (`conftest.py`'s
`pytest_terminal_summary` hook and `scripts/bench_report.py` both derive a
`module::case` label from it). See [Benchmarking](benchmarking.md) for
the full naming convention.

CI commits a snapshot automatically on every push to `main` and on every
release tag — no manual step required. Run locally when you want an
immediate result during development:

```sh
make bench                       # C + Python (delegates to just-makeit bench)
just-makeit bench --python-only  # Python only
just-makeit bench --tag v1.2.3   # version-tagged snapshot (matches CI on tag push)
```

Compare two snapshots:

```sh
uv run pytest-benchmark compare benchmarks/history/2026-05-01-abc1234.json \
                                 benchmarks/history/2026-05-15-def5678.json
```

______________________________________________________________________

## Step 11 — Rebuild, reconcile, and verify

```sh
cmake --build build --target <module>   # rebuild just this .so
make python-test                        # full pytest suite
```

If you changed `objects/<component>.toml` (or `just-makeit.toml`) after
the initial `jm apply` — e.g. added a property, tweaked a param default —
reconcile the generated glue. **Prefer the scoped form** —
`jm apply objects/<component>.toml` — over a bare `jm apply`: in a module
with sibling objects, a bare `jm apply` has been seen (jm 0.29.1) to
regenerate a *sibling's* sacred `_ext_<sibling>.c` fragment too, discarding
its hand-patches. Whichever form you run, check `git diff` before trusting
it:

```sh
jm apply objects/<component>.toml   # scoped; reconciles CMakeLists, __init__.py, .pyi
jm status --check                   # confirms zero drift; this is what CI's manifest-drift gate runs
git diff --stat                     # confirm only your component's files changed
```

______________________________________________________________________

## Checklist

Before opening a PR:

- [ ] `make test` — all C tests pass
- [ ] `python -m doctest -v src/doppler/<module>/<module>.pyi` — all examples pass
- [ ] `make python-test` — all Python tests pass
- [ ] `make bench` — C and Python benchmarks run and JSON snapshots are saved
- [ ] `jm status --check` — zero manifest drift
- [ ] `__init__.py` contains only re-exports and `__all__`
- [ ] No Python wrapper classes — C extension types are the public API
- [ ] `<module>.pyi` has stubs for every exported symbol
- [ ] If stateful: `serializable = "true"` set, C triplet implemented, both
    the C round-trip test and the Python
    `test_state_serialization.py` matrix entry pass
- [ ] `make docs` — docs build clean
- [ ] `uv run pytest -m docs_snippets` — every Python fence you added to the
    docs runs (or is `skip=`-marked with a reason). New pages are gated
    automatically; see [Doc Examples (tested)](doc-examples.md)

______________________________________________________________________

## See also

- [Module Layout](module-layout.md) — file layout rules and rationale
- [State Serialization](../design/state-serialization.md) — the bit-exact
    checkpoint/resume design behind Step 4b
- [Benchmarking](benchmarking.md) — C and Python benchmark pipelines, history files, comparisons
- [just-makeit docs](https://just-buildit.github.io/just-makeit/) — full
    command reference
- [Build from Source](../install/source.md) — cmake flags and make targets
