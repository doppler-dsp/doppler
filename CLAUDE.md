# doppler — codebase guide for Claude

## Project

C-first DSP library. Every algorithm lives in C exactly once; Python, Rust, and
other language bindings are thin wrappers over the C ABI. The Python layer is a
CPython C extension generated and managed by **just-makeit** (jm).

See `~/.claude/skills/just-makeit.md` for the full jm reference.

______________________________________________________________________

## Repository layout

```
native/inc/<obj>/<obj>_core.h   — public C API + state struct (hand-written)
native/src/<obj>/<obj>_core.c   — algorithm implementation (hand-written)
native/src/<module>/<mod>_ext.c — CPython glue (jm-generated, patch sparingly)
native/src/<module>/CMakeLists.txt
native/tests/test_<obj>_core.c  — C-level unit tests
native/benchmarks/bench_<obj>_core.c

objects/<obj>.toml              — jm fragment: the interface declaration (source)
templates/                      — starter fragments for the two non-default patterns
scaffold/                       — legacy CLI scripts (historical; do not re-run)

src/doppler/<module>/__init__.py    — re-export only; no logic
src/doppler/<module>/tests/         — Python integration tests
src/doppler/<module>/benchmarks/
```

______________________________________________________________________

## Adding a new module or object

### Step 0 — characterise the algorithm

Ask one question:

> Can a single input sample be processed independently with small fixed state?

- **Yes** → use the CLI default (`jm object name --state ...`). No template needed.
- **No** → the C API owns block I/O. Pick a template:
    - Block processor / decimator / resampler / FFT / correlator → `templates/block_nostate.toml`
    - Signal generator (void input, array output) → `templates/void_source.toml`

### Step 1 — declare the interface

**Entry point A: CLI (exploratory, step/steps objects)**

```sh
source /tmp/jm-venv/bin/activate       # or install: bash ~/just-makeit/install.sh
jm object myobj --module mymodule --state gain:double:1.0 --mutable
jm property myobj gain --module mymodule --type double --writable --field
# CLI mutations route to objects/myobj.toml automatically (split layout)
```

**Entry point B: template (block I/O objects)**

```sh
cp templates/block_nostate.toml objects/myobj.toml  # or void_source.toml
# Edit objects/myobj.toml — replace <<OBJ>> and <<CLASS>>, tune init_params,
# methods, and properties for the algorithm.
```

### Step 2 — register in the module manifest

```toml
# just-makeit.toml
[module.mymodule]
objects = ["existing_obj", "myobj"]
```

### Step 3 — apply

```sh
jm apply objects/myobj.toml
# Materialises: _core.h stub, _core.c stub, _ext.c (regenerated),
# CMakeLists.txt, __init__.py, .pyi, C test, C bench, Python test, Python bench.
```

### Step 4 — implement the C core (the only phase requiring algorithm knowledge)

1. `native/inc/<obj>/<obj>_core.h` — add state struct fields; declare extra
    lifecycle functions (`reconfigure`, secondary constructors, etc.)
1. `native/src/<obj>/<obj>_core.c` — implement:
    - `<obj>_create()`: allocate, validate args, compute derived values
    - `<obj>_destroy()`: free heap members then struct
    - `<obj>_reset()`: zero state, reset counters
    - Method bodies: hot loops, format conversions, modular arithmetic
1. `native/src/<module>/<mod>_ext.c` — patch **only** for:
    - Dtype dispatch (float32 vs complex64 → different create function)
    - Non-trivial argument validation the generated code cannot infer
    - Lazy-alloc grow-on-demand when block size may change across calls

### Step 5 — build and test

```sh
cmake --build build --target <module>   # rebuild just this .so
pytest src/doppler/<module>/tests/      # Python integration tests
ctest --test-dir build -R <obj>         # C-level tests
```

### Step 6 — reconcile (if TOML changed after apply)

```sh
jm apply   # idempotent; reconciles CMakeLists, __init__.py, .pyi
```

### Step 7 — commit the fragment, not the commands

```sh
git add objects/<obj>.toml native/inc/<obj>/ native/src/<obj>/
git add src/doppler/<module>/tests/ src/doppler/<module>/<module>.pyi
```

______________________________________________________________________

## Known post-apply patches

These apply to all block-I/O objects (template users):

| Issue                                                      | Fix                                                                                                      |
| ---------------------------------------------------------- | -------------------------------------------------------------------------------------------------------- |
| Init params all optional (`\|kkk` format string)           | Expected — `create(0,0,0)` → NULL → `MemoryError`. Add validation in `_core.c` for human-readable errors |
| Reconfigure method doctest silently no-ops on invalid args | Hand-fix the docstring; the C API intentionally ignores invalid params                                   |

### `variable_output` block objects — jm now generates the machinery

As of **jm 0.14.3**, a `variable_output` method generates the full execute
buffer machinery itself; the five hand-patches this file used to list are
**obsolete**. Verified against a fresh scaffold, jm emits:

- `float complex *_execute_buf;` **and** `size_t _execute_buf_cap;` struct fields
- lazy first-call `malloc` plus grow-on-demand `realloc`
    (`if (!_execute_buf || _execute_buf_cap < _need) { realloc(...) }`)
- `free(self->_execute_buf)` in the dealloc path

Because grow-on-demand re-sizes on the next `execute`, an explicit
buffer-invalidation in a rate/​config setter is no longer required (it is at
most defensive; RateConverter keeps one, harmlessly).

**`_ext_<obj>.c` fragments are hand-owned.** doppler splits each module
binding into per-object `native/src/<mod>/<mod>_ext_<obj>.c` fragments that the
generated aggregator `<mod>_ext.c` `#include`s. `jm status`/`apply` regenerate
**only the aggregator** — the fragments are sacred, like `_core.c`. Bespoke
binding logic (e.g. HalfbandDecimatorR2C's float64→float32 input cast) lives
there by design and is **not** drift.

### jm gaps — all resolved in **jm 0.14.4** (pin bumped)

doppler drove five jm fixes/features; all shipped in 0.14.4 and are adopted
here, so `jm apply` is now fully idempotent with **no allowlist**:

| Was                                                                                                                                | Resolution in 0.14.4                                                                                                                                                                                           |
| ---------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `jm apply` re-injected a conflicting 4-arg `variable_output` prototype for the multi-line 5-arg `ddc_execute`/`ddcr_execute` decls | [jm#137](https://github.com/just-buildit/just-makeit/issues/137) — multi-line decl recognition in `_inject_decls_into_core_h`; replay no longer preserves                                                      |
| `variable_output` could not declare an explicit output-capacity param                                                              | [jm#138](https://github.com/just-buildit/just-makeit/issues/138) — **`pass_capacity = true`** generates the 5-arg `(..., out, max_out)` form. ddc, ddcr, RateConverter now set it (see their `objects/*.toml`) |
| `--arg-type "T[]"` rendered malformed `const T[] *in`                                                                              | [jm#139](https://github.com/just-buildit/just-makeit/issues/139) — element type used for the block input                                                                                                       |
| `jm status` had no CI-gate surface — drift gate was hand-rolled in shell                                                           | [jm#140](https://github.com/just-buildit/just-makeit/issues/140) — `jm status --allow/--json/--diff/--check`; the `manifest-drift` CI job is now just `jm status --check`                                      |
| no perf-regression gate                                                                                                            | [jm#141](https://github.com/just-buildit/just-makeit/issues/141) — `jm bench --check` (available; not yet wired into doppler CI)                                                                               |

`spectral` window out-param fix (`{name="w", type="float[]", out = true}`)
from the prior round also stands. The only remaining non-jm surface is
`ffi/rust/` (jm emits CPython only) — maintained by hand against the C ABI.

### 0.15.x adoptions — formerly-manual patterns are now declarative (pin: 0.15.4)

doppler drove a second round of jm features; with the pin at **0.15.4**, three
things that used to be hand-patches are now manifest-driven glue — never edit
the generated file, set the key:

| Pattern                                                                | Old (hand-patch)                                         | Now (declarative, jm ≥ 0.15.x)                                                                                                           |
| ---------------------------------------------------------------------- | -------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| **Release the GIL** in an execute binding for thread-per-shard scaling | hand-add `Py_BEGIN_ALLOW_THREADS` to `<mod>_ext_<obj>.c` | `nogil = true` on the method (`objects/ddc.toml`, `ddcr.toml`); jm generates the hoist + allow-threads (0.15.2)                          |
| **Re-export a sibling's symbols** from a package `__init__.py`         | hand-edit `__all__` + `from .ddc_fn import …`            | `reexports = { ddc_fn = [...] }` on `[module.ddc]`; regenerates single-line (0.15.1)                                                     |
| **Runtime `__doc__`** parity with the `.pyi`                           | (was stale fallback)                                     | `jm apply` transplants header-derived docstrings into the sacred fragments' `PyMethodDef`/`tp_doc`/getset slots, scaffold-only (0.14.11) |

Also adopted: `depends_on` auto-includes a dependency's header **only when the
header exists** (0.15.4 — 0.15.3 wrongly injected `lo_core/lo_core_core.h` for
doppler's link-target deps and was skipped); `jm apply` is idempotent against a
hand-tuned `JM_RESTRICT`/non-const prototype (0.15.2). `jm apply` /
`jm status --check` remain fully idempotent with **no allowlist**.

### 0.16.0 adoptions — `jm app` for real tools + version-skew (pin: 0.16.0)

The CI drift gate now pins **0.16.0** (`ci.yml`); `[project].jm_version` is
stamped in `just-makeit.toml` and **every jm command warns on version skew**
(gh-183) — so a stale CLI (e.g. the `/tmp/jm-venv` 0.14.12) is caught
immediately. **Always drive doppler with `uvx --from 'just-makeit==0.16.0' just-makeit …`**, never the stale console script.

`jm app` (gh-184) now generates a complete CLI tool from one object:
init_params → ctor `--flags`, a `string_enum:` init param → a `--flag a|b`
**choice flag**, a cf32 generator/blockwise object → a built-in
`--sample_type cf32|cf64|ci32|ci16|ci8` (interleaved-I/Q convert-on-write,
byte-identical across the c/console/pep723 faces), and `--help`. This is what
the `wfmgen`/`wavegen` tool is being built on (see `~/.claude/plans`). The
string-enum binding parses a Python string and maps it to the C enum index, so
`Obj(kind="tone")` works directly.

### 0.18.0 adoptions — header-derived docstrings + @code doctests

(Pinned 0.18.0 at the time; superseded by the 0.19.0 pin below.)

jm 0.18.0 synthesizes comprehensive `.pyi` docstrings from the sacred header
Doxygen: a `@code … @endcode` block becomes a runnable numpy **`Examples`**
doctest, multi-line `@brief` prose renders as flowing paragraphs (continuation
lines, no blank `*` between them — a blank line double-spaces), and built-in
`reset`/`step`/`steps` derive their docstring from the header `@brief` (scaffold
briefs are filtered for idempotency). doppler's headers carry verified `@code`
doctests on every public method; the CI doctest gate (`pytest --doctest-glob`)
runs them. NB: a method's `@code` block needs nothing special, but jm leaves a
blank line before the closing `"""` so the text-mode doctest doesn't swallow it.

### 0.19.6 adoptions — pin bump + the hand-rolled-glue migration campaign (pin: 0.19.6)

The CI drift gate pins **0.19.6** (`ci.yml` + `perf-regression.yml`); `jm_version`
is stamped 0.19.6. **Drive doppler with `uvx --from 'just-makeit==0.19.6' just-makeit …`.** The bump's `jm apply` reconciled 7 module aggregators — module
**functions are now keyword-capable** (gh-240; `kaiser_window(w=…, beta=…)`), for
free; `_core.c` + sacred fragments untouched.

doppler's filed feature issues all shipped (#222 `out=` steps, #223 multi-return,
#224 ctor dispatch, #225 `depends_on link=true`, #244 `--single` structseq +
size_t default fix), so the **hand-rolled binding glue is being migrated back to
declarative jm**, one object per PR. **The migration mechanic** (non-destructive —
do NOT use `jm regenerate`, which rebuilds `_core.c`): edit the manifest → **delete
the per-object `native/src/<mod>/<mod>_ext_<obj>.c` fragment** → `jm apply`
recreates it from the manifest. `apply` only creates missing files + reconciles
glue; `_core.c`/tests/benches are untouched. (Restore `measure.pyi` after every
apply — apply regenerates it; it stays `status_allow`-listed until the measure
`--single` migration.) Regenerating against 0.19.6 also picks up codegen fixes
free (the pilot fixed a latent gh-219 UAF in `FIR.execute`).

**Migrated so far:** FIR dtype dispatch → declarative `real_type` +
`real_create_fn` on the `taps` init-param (#224); the hand-coded probe/branch in
`filter_ext_fir.c` is gone. **Roadmap** (see `~/.claude/plans`): #225 link lines;
#222 `out=` on the ~8 cvt/agc/accumulator `steps`; #224 Resampler `optional`
`bank`; #244 measure structseq `--single` (the one with a return-by-value
`_core.c` signature reconcile — its own PR, un-allowlists `measure.pyi`); #223
verify-only. #247 (group module functions into one TU) still open upstream.

### 0.19.3 adoptions — gh-197 window fix + the gh-219 UAF (pin: 0.19.3)

The CI drift gate now pins **0.19.3** (`ci.yml` + `perf-regression.yml`);
`jm_version` is stamped 0.19.3. **Always drive doppler with
`uvx --from 'just-makeit==0.19.3' just-makeit …`.**

`jm apply` under 0.19.3 regenerated one aggregator — `spectral_ext.c` — adopting
**gh-197**: the `kaiser_window`/`hann_window` out-params are now writable
(`float *w` + `NPY_ARRAY_WRITEABLE`) instead of `const float *`.

Separately, doppler **hand-adopted gh-219's use-after-free fix** in the three
`variable_output` execute fragments that still grew their output buffer in place
(`ddc_ext_ddc.c`, `ddc_ext_ddcr.c`, `filter_ext_hbdecim_q15.c`): a `realloc`-grow
moved the buffer while a previously returned view (which pins `self`, not the
buffer) dangled. They now return an **independent numpy-owned array per call**
(`PyArray_SimpleNew` + copy), matching the `lo`/`nco`/`awgn` source objects — the
fragments are sacred so `jm regenerate` (which would also nuke `_core.c`) was not
used. Regression test: `test_ddc_execute_result_survives_buffer_grow`.

**Not adopted:** gh-208 `step_delegates_to_steps` — the deferred synth #76 parity
target is `arg_type="void"` / `variable_output` (gh-208-ineligible) and is already
byte-identical by hand; the eligible scalar objects show no parity bug, and
adopting would refactor each hand-written `step()`. gh-225 (`depends_on` link
line, doppler-filed) is implemented upstream but unreleased — adopt when it ships.

### 0.19.0 adoptions — Windows boilerplate is opt-in (pin: 0.19.0)

The CI drift gate previously pinned **0.19.0** (`ci.yml` + `perf-regression.yml`);
`jm_version` was stamped 0.19.0.

jm 0.19.0 resolves the doppler-driven [jm#213](https://github.com/just-buildit/just-makeit/issues/213):
the per-component MinGW runtime-DLL `if(WIN32 …)` block is now gated on
`[project] platforms` (default `["linux", "macos"]`) instead of emitted
unconditionally. doppler sets `platforms = ["linux", "macos"]` in
`just-makeit.toml`; `jm apply` stripped the block from all 13 generated module
`CMakeLists.txt`, and `jm status --check` treats its absence as correct — so the
Windows fluff doppler couldn't remove (frozen by the drift gate) is finally
gone. The lone remaining block was `native/src/buffer/CMakeLists.txt` (a
`no_generate`, hand-owned module); removed by hand. jm itself also dropped
Windows CI/tooling in 0.19.0 (the "jm doesn't really support Windows" point from
the issue).

### 0.17.0 adoptions — `jm app` output axes (pin: 0.17.1)

The CI drift gate previously pinned **0.17.0** (`ci.yml`); `jm_version` stamped 0.17.0.

`jm app` (gh-193) adds three output axes to any cf32 generator/blockwise app
(the same stream that gets `--sample_type`), byte-identical across all three
faces: **`--file-type raw|csv`** (raw interleaved I/Q, or text `I,Q` lines),
**`--endian le|be`** (big-endian reverses each element), and **`--record FILE`**
(a JSON record of the fully-resolved run — every flag after defaulting, choice
flags as their chosen string — for reproducible captures). `wavegen` gets all
three for free on regeneration. Richer containers (**BLUE type-1000**, **SigMF**,
**zmq**) deliberately stay application-side in the `wfmcompose` c_dep
(`wfm_writer.c`, `wfm_sink.c`) — they need sample-rate / segment / transport
context a generic generator can't know.

### `measure` — ADC/spectral metric suite (structseq returns, jm#244)

`[module.measure]` (`ToneMeasure`/`NPRMeasure`/`IMDMeasure` + capture-planning
free functions) returns **named `PyStructSequence` results** (`r.enob`,
`r.sfdr_dbc`) instead of jm's `result_fields` default `list[tuple]`. jm has no
single-named-record shape, so each `analyze()` body is hand-written in its sacred
`measure_ext_<obj>.c` fragment to build a lazily-cached structseq; the metric
kernels live in `*_core.c`. Two jm gaps drove **jm#244** and are hand-patched in
the fragments until it lands: (1) jm renders the return as `list[tuple]`, so
`src/doppler/measure/measure.pyi` is **hand-maintained and allowlisted**
(`status_allow`) — `jm apply` regenerates it, so **re-apply the .pyi edits after
any apply**; (2) jm drops `size_t` init-param **defaults** from the generated
`__init__` (only float/double), so `n`/`pad`/`n_harmonics` defaults are set by
hand in each fragment's init. NPR's `analyze` also hand-parses its 5 geometry
doubles (jm's `result_fields` codegen ignores extra scalar params). The module
composes `fft_core` + `spectral_core`; module-level helpers are one TU each. See
`docs/design/measurement-suite.md`.

### `ddc_fn` — the functional DDCR API (`no_generate`)

`[module.ddc_fn]` is `no_generate`: a fully hand-written CPython extension
exposing the DDCR down-converter as free functions over an opaque PyCapsule
state (`ddcr_create`/`execute`/`reset`/`destroy`/`get_/set_*`) instead of a
type. jm only wires its CMake `add_subdirectory`; `ddc_fn_ext.c` and
`ddc_fn.pyi` are hand-owned (including their GIL release — a no_generate module
is hand-owned end to end, so `nogil` does not apply). `doppler.ddc` re-exports
the `ddcr_*` names via the `reexports` key above. See the gallery walkthrough
(`docs/gallery/ddc-fn.md`) for the streaming/threading model.

______________________________________________________________________

## Build

```sh
# First time (from repo root):
cmake -B build -DBUILD_PYTHON=ON
cmake --build build

# Rebuild a single module:
cmake --build build --target filter

# Full test suite:
pytest src/doppler/
ctest --test-dir build
```

______________________________________________________________________

## Code style

- **C**: `clang-format` with `.clang-format` at repo root (GNU, 79 cols — copied
    from just-makeit so generated glue is byte-identical to jm's output)
- **Python**: `uvx ruff format --line-length=79`
- Line width: 79 characters in all languages
- Docstrings: numpy-style, verbose — explain how it works, not just what
- Inline comments: explain intent and non-obvious mechanics only

### pre-commit (`.pre-commit-config.yaml`)

`pre-commit run --all-files` lints + formats **hand-written** code and docs
(ruff, clang-format tracking the **latest** release via `pre-commit autoupdate`,
mdformat, hygiene hooks); a CI `pre-commit` job enforces it. clang-format only
ever sees hand-owned C (jm glue is excluded), so its version need not match jm's. The hard rule: **jm-generated glue is
NOT formatted here** — it is owned by `jm apply` and guarded by the
`manifest-drift` gate, so a formatter touching it would drift CI. Excluded
accordingly: `*.pyi` stubs, `native/inc/**` headers (jm injects the public
decls — clang-format reflows its multi-line prototypes → drift), per-module
`*_ext.c` aggregators (the `*_ext_<obj>.c` fragments ARE formatted — they're
hand-owned), per-module `CMakeLists.txt`, the re-export `__init__.py` shims, the
pep723 `wavegen.py`, `docs/c-api/**` (mkdoxy-generated), and `vendor/**`. After
any change that could touch generated files, re-run `jm status --check`.
