# Adding a New C Extension Module

This guide walks through adding a new DSP module to doppler using
[just-makeit](https://just-buildit.github.io/just-makeit/) (`jm`). The
scaffold handles boilerplate; you fill in the DSP logic.

**This page is not the whole lifecycle.**
[Adding an algorithm](adding-algorithms.md) is the spine and owns the phase
*order*; this page is the *how* for its declare, implement and bind phases.
The phases after them are somebody else's page and are not optional:

| after you finish here                         | go to                                                                          | gate                                               |
| --------------------------------------------- | ------------------------------------------------------------------------------ | -------------------------------------------------- |
| explore the envelope, characterize            | [Object Validation](validation.md)                                             | `make validate-c`, `make characterize`             |
| **certify** — the evidence a caller relies on | [Object Validation](validation.md)                                             | `make validate-check`, `test_validation_limits.py` |
| document — header `@code`, guides, examples   | [Docstring Authoring](docstring-authoring.md), [Doc Examples](doc-examples.md) | `make test-stubs`, `make test-examples-c`          |
| land — changelog, issues for what is left     | [Release](../release.md)                                                       | `make changelog-check`                             |

A module that builds and passes its own tests is **not** finished: an object
ships with certified limits, and the order is not negotiable — the C header
is the SSOT, so the claims get pinned in C first and the Python evidence
layer is built on top of that, never the reverse.

For the layout rules that govern each generated file, see
[Module Layout](module-layout.md).

______________________________________________________________________

## Prerequisites

Drive doppler through **`make`**, and jm through the repo's own pinned
install — never `uvx`. An unpinned `uvx just-makeit` does not fetch the
latest release: it silently reuses whatever version is installed as a uv
*tool*, which drifts per machine and never updates. That is not
hypothetical — `make bench` was the one unpinned call site and resolved to a
version far behind the manifest, so it could not parse the manifest at all
and the local benchmark path was unrunnable for months.

The pin is declared **once**, in
[`just-makeit.toml`](https://github.com/doppler-dsp/doppler/blob/main/just-makeit.toml)'s
`[project] jm_version`, and `scripts/gen_jm_pin.py` propagates it to every
other site. Read it from there rather than from any prose — including this
page, which deliberately does not name a version:

```sh
grep '^jm_version' just-makeit.toml     # the SSOT
.venv/bin/just-makeit --version         # what you are actually running
make drift-check                        # what CI runs
```

Every jm command warns on version skew, so a stale CLI announces itself.

The scaffold writes into the doppler source tree, so run all commands from
the repo root.

**Diff after any mutating `jm` command** (`object`, `method`, `property`,
`add`, `remove`) — not because jm is untrustworthy, but because a manifest
edit legitimately regenerates every fragment in the module, and reading that
diff is how you confirm it changed what you meant.

> **Two hazards this page used to warn about are fixed** and the warnings
> are gone. A bare `jm apply` no longer regenerates a sibling's sacred
> `<module>_ext_<component>.c` and discards its hand-patches, and a mutating
> command no longer reformats every `objects/*.toml`. Verified on the
> current pin: a bare `jm apply` against a clean tree writes **nothing**,
> and `jm property` touches only the component it names and that module's
> generated glue. If you are reading an older copy of this page telling you
> to `git checkout -- objects/` after every command, that advice is stale.

______________________________________________________________________

## Step 0 — Characterize the algorithm

Two questions, and they pick the method verb as well as the scaffold.

**1. Is the output rate tied to the input rate?**

- **Yes** — N samples in, N (or N/R) samples out. A transducer.
- **No** — samples go in and *events* come out, zero or more per call, at
    whatever rate the signal produces them. A detector, an acquisition
    engine, a burst receiver.

**2. If the rate is tied: can a single sample be processed independently,
with small fixed state?**

Each shape has its own verb, so pick the shape first and the name
follows:

| shape                                                                                        | verb             | scaffold                                      | examples                                             |
| -------------------------------------------------------------------------------------------- | ---------------- | --------------------------------------------- | ---------------------------------------------------- |
| sample-independent, small fixed state — **or usable inside a feedback loop**                 | `step` + `steps` | CLI default (Step 1, Entry point A)           | AGC, LoopFilter, LockDet, converters, accumulators   |
| owns block I/O — needs a window, a whole buffer, or an output length not known until runtime | `execute`        | `--preset blockwise`                          | decimator, resampler, FFT, correlator                |
| **event-driven — output decoupled from input**                                               | `push`           | `--preset blockwise`, returning a record list | `acq`, `detector`, `detector2d`, `DsssBurstReceiver` |
| void input, array output                                                                     | `steps`          | `--preset generator`                          | NCO, LO, tone/PN generators                          |

**`push` is not a deviation from `steps`; it is the third category.** The
distinction is real rather than cosmetic: a `steps` caller knows how much
output a call produces before making it, and a `push` caller does not — which
is exactly why `push` returns a **list** of records (`acq_result_t`,
`det_result_t`) or a variable-length payload with a parallel `events()`, and
why its `*_max_out()` takes the input length. Naming an event-driven object
`steps` would promise a rate it cannot honour.

> **`step` and `steps` are jm BUILT-INS**, generated from the object's
> `arg_type`/`return_type` — they are not `[[<obj>.methods]]` entries, so
> grepping the manifest for them finds nothing. A sample-wise object gets
> both, sharing one state: `steps(x)` for a block, `step(x)` for a single
> sample. Verified across the shipped stubs: AGC, LoopFilter, LockDet,
> MovingAverage, the converters and the accumulators all carry the pair.
>
> **`step` is what makes an object usable inside a RECURSIVE loop, and that
> is why it is not optional.** When a caller closes feedback around your
> object — a Costas loop driving an NCO through a `LoopFilter`, a DLL
> correcting its own timing, an AGC adjusting the gain that produced the
> sample it just measured — **the next input depends on the previous
> output**, so there is no block to hand over. `steps` cannot express that
> at all; only `step` can. Offering `steps` alone quietly excludes your
> object from every tracking loop in the library.

______________________________________________________________________

## Step 1 — Declare the interface

**Entry point A: plain `steps` object**

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
[State Serialization](../../design/state-serialization.md) for the full
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
[State Serialization](../../design/state-serialization.md)).

Run the C suite:

```sh
make test
```

All C tests must pass before moving to the Python layer.

______________________________________________________________________

## Step 7 — Author the docstrings in the **C header**

**Do not write `src/doppler/<module>/<module>.pyi` by hand.** jm generates it,
and it is derived from your `_core.h` — the header's Doxygen *is* the source
of the Python docstrings. Editing the stub means writing something the next
`jm apply` deletes.

So the authoring you actually do happens in Step 4, in the header:

| write this in `<component>_core.h` | becomes this in `<module>.pyi`      |
| ---------------------------------- | ----------------------------------- |
| `@brief` prose                     | the summary line and body           |
| `@param name  text`                | the `Parameters` entry              |
| `@return text`                     | the `Returns` entry                 |
| a `@code … @endcode` block         | a runnable numpy `Examples` doctest |
| `@code` on `create()`              | the **class**'s `Examples` section  |

That last row matters more than it looks: a class whose constructor takes a
required array (a code, a tap vector) gets its Examples only from `create()`'s
`@code`, so without one the class ships with no example at all.

**The `@code` blocks are executed**, by `make test-stubs` — they are doctests,
not decoration. Two traps worth knowing before you write one:

- **Wrap at 71 columns.** jm indents the block into the stub, and a line that
    lands past 79 fails the formatter. `jm apply` names any line that will
    overflow, with its budget.
- **Leave a blank line before the closing `@endcode`.** A text-mode doctest
    otherwise swallows the terminator into the last example's expected output.

```sh
make test-stubs      # doctest every generated .pyi
```

______________________________________________________________________

## Step 8 — Declare the public surface in the manifest

**Do not write `src/doppler/<module>/__init__.py` by hand either.** jm
generates it, and `__all__` follows from what the manifest declares.

- The **module docstring** comes from `[module.<name>] doc`.
- **Re-exporting a sibling module's symbols** is
    `reexports = { <mod> = [...] }` on `[module.<name>]`, not a hand-written
    `from .x import y`.

`__all__` is still the public API — it controls `import *`, what IDEs
surface, and what users may rely on — but you edit it by declaring the
surface, not by typing the list. A symbol missing from `__all__` is a symbol
that is not public.

Nothing else belongs in `__init__.py`: no wrapper classes, no logic. See
[Module Layout](module-layout.md).

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
make test-python
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

## Step 10b — The C benchmark, and when you must write it yourself

For an **object**, this step is free: `jm apply` scaffolds
`native/benchmarks/bench_<component>_core.c` in Step 3, generates its CMake
target, and `jm bench` builds and runs it because the object is one of jm's
components.

For anything that is **not** an object, none of that happens and nothing
says so. Two shapes in this repo:

- a **`c_deps` entry** — hand-owned C a module links (`conv`, `rs`,
    `ccsds_tm`);
- a **function-only module** — one whose surface is free functions
    (`mpsk`, `ber`, `snr`, `util`, `detection`).

For those you write the benchmark, and you register the target by hand in
the root `CMakeLists.txt` beside `bench_util_core`:

```cmake
add_executable(bench_mycomp_core native/benchmarks/bench_mycomp_core.c)
target_link_libraries(bench_mycomp_core PRIVATE mycomp_core m)
target_include_directories(bench_mycomp_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
```

`make lint` then holds it: a component with C tests must have a benchmark,
it must record a measurement, and it must write its JSON under the name a
collector opens (`jm_bench_write_json(&b, "mycomp")`, not `"mycomp_core"` —
a real and repeated mistake: the wrong name writes a file no collector opens,
so the benchmark runs and its measurement is silently thrown away).

**It will not run under `make bench` yet.** That needs
[just-makeit#1023](https://github.com/just-buildit/just-makeit/issues/1023);
run it by hand meanwhile:

```sh
cmake --build build --target bench_mycomp_core
./build/native/src/mycomp/bench_mycomp_core
```

Background, and the four benchmarks that were compiled by every build and
run by nothing:
[Benchmarks jm cannot see](benchmarking.md#benchmarks-jm-cannot-see).

______________________________________________________________________

## Step 11 — Rebuild, reconcile, and verify

```sh
cmake --build build --target <module>   # rebuild just this .so
make test-python                        # full pytest suite
```

If you changed `objects/<component>.toml` (or `just-makeit.toml`) after
the initial `jm apply` — e.g. added a property, tweaked a param default —
reconcile the generated glue:

```sh
jm apply                            # reconciles CMakeLists, __init__.py, .pyi
jm status --check                   # confirms zero drift; this is what CI's manifest-drift gate runs
git diff --stat                     # confirm only your component's files changed
```

______________________________________________________________________

## Checklist

Before opening a PR:

- [ ] `make test` — all C tests pass
- [ ] `make test-stubs` — every `@code` doctest in your header runs
- [ ] `make test-python` — all Python tests pass
- [ ] `make bench` — C and Python benchmarks run and JSON snapshots are saved
- [ ] The C benchmark exists **and runs**: an object's is scaffolded, a
    `c_deps` entry's or a function-only module's is hand-written and
    hand-registered (Step 10b). `make lint` gates both halves
- [ ] `jm status --check` — zero manifest drift
- [ ] `__init__.py` and `<module>.pyi` are **generated**, not hand-edited
    — the surface is declared in the manifest and documented in the header
- [ ] No Python wrapper classes — C extension types are the public API
- [ ] If stateful: `serializable = "true"` set, C triplet implemented, both
    the C round-trip test and the Python
    `test_state_serialization.py` matrix entry pass
- [ ] `make docs` — docs build clean
- [ ] `make test-snippets` — every fence you added to the docs runs (or is
    `skip=`-marked with a reason). New pages are gated automatically; see
    [Doc Examples (tested)](doc-examples.md)
- [ ] `make lint` — every gate CI runs, including the drift and report gates

______________________________________________________________________

## See also

- [Module Layout](module-layout.md) — file layout rules and rationale
- [State Serialization](../../design/state-serialization.md) — the bit-exact
    checkpoint/resume design behind Step 4b
- [Benchmarking](benchmarking.md) — C and Python benchmark pipelines, history files, comparisons
- [just-makeit docs](https://just-buildit.github.io/just-makeit/) — full
    command reference
- [Build from Source](../../install/source.md) — cmake flags and make targets
