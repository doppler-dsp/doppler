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

src/doppler/<module>/__init__.py    — re-export only; no logic
src/doppler/<module>/tests/         — Python integration tests
src/doppler/<module>/benchmarks/
```

______________________________________________________________________

## Adding a new module or object

### Step 0 — characterise the algorithm

Ask one question:

> Can a single input sample be processed independently with small fixed state?

- **Yes** → use the CLI default (`jm object name --state ...`). A plain step/steps object.
- **No** → the C API owns block I/O. Use a `jm object --preset` (jm ≥ 0.19):
    - Block processor / decimator / resampler / FFT / correlator → `--preset blockwise`
    - Signal generator (void input, array output) → `--preset generator`

### Step 1 — declare the interface

**Entry point A: CLI (exploratory, step/steps objects)**

```sh
source /tmp/jm-venv/bin/activate       # or install: bash ~/just-makeit/install.sh
jm object myobj --module mymodule --state gain:double:1.0 --mutable
jm property myobj gain --module mymodule --type double --writable --field
# CLI mutations route to objects/myobj.toml automatically (split layout)
```

**Entry point B: CLI presets (block I/O objects)**

A block-I/O object owns its output buffer (`no_state`/`no_step`, opaque heap
state jm can't infer), so it's a `--preset` for the object shape **plus** an
explicit `variable_output` method — `jm method … --variable-output` is what
adds the lazy-alloc, grow-on-demand output buffer. (Add `--pass-capacity` for
the 5-arg `(…, out, max_out)` form.)

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
# CLI mutations route to objects/<obj>.toml automatically (split layout).
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

These apply to all block-I/O objects (`--preset blockwise`/`generator`):

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

### 0.19.37 adoptions — `serializable` flag + LO/CIC/FIR adoption (gh-400, pin: 0.19.37)

The CI drift gate now pins **0.19.37** (`ci.yml` + `perf-regression.yml`);
`jm_version` is stamped 0.19.37. **Drive doppler with `uvx --from 'just-makeit==0.19.37' just-makeit …`.** The pin bump is pure tooling — fully additive, `jm status --check` clean across all 6495 files before any flag flip.

0.19.37 ships **gh-400** (doppler-filed, jm #401): a `serializable = "true"`
object key makes jm generate the Python binding triplet for a hand-written C
state ABI (sibling to `reset`, no struct knowledge by jm) — the elastic /
pure-transducer face:

```c
size_t <c>_state_bytes(const T *);            /* serialized size */
void   <c>_get_state(const T *, void *blob);  /* serialize       */
int    <c>_set_state(T *, const void *blob);  /* restore (0 ok)  */
```

↓ generates `state_bytes() -> int`, `get_state() -> bytes`,
`set_state(bytes) -> None` (size-mismatch / rejected-blob `ValueError`,
non-`bytes` `TypeError`).

**Adopted on `LO`, `CIC`, `FIR`** (the C triplets land via the #261 leaf
serializers): set `serializable = "true"` in `objects/<obj>.toml`, delete the
per-object `_ext_<obj>.c` fragment, `jm apply`, **clang-format the fragment**
(GNU 2-space — jm emits K&R 4-space). Verified: the leaf C round-trip tests
(`test_<obj>_core.c`) + a Python random-stream mid-sequence split → bit-exact
restore into a fresh instance (FIR delay line, CIC integ/comb, LO phase).
Regenerating each fragment also re-emits jm's current `variable_output`
machinery (LO/CIC pick up the gh-219 retired-list deferred-free + CIC gains the
`out=` kwarg on `decimate`); `test_lo.py`'s #116 large-n regressions confirm the
regenerated `steps()` is correct, so LO's bespoke independent-array `steps()` is
retired in favour of the declarative form. `ddcr` is NOT eligible (hand-owned
`ddc_fn`, `no_generate`); `hbdecim`/`resamp` are composed under
`HalfbandDecimator`/`Resampler` and stay C-level for now.

### 0.19.7 adoptions — additive collocated `link=true` (gh-254) (pin: 0.19.7)

The CI drift gate now pins **0.19.7** (`ci.yml` + `perf-regression.yml`);
`jm_version` is stamped 0.19.7. **Drive doppler with `uvx --from 'just-makeit==0.19.7' just-makeit …`.** The bump is pure tooling — `jm apply` produced **no
codegen drift** (only the hand-owned `measure.pyi`, restored as always).

0.19.7 ships **gh-254** (doppler-filed): `depends_on { link = true }` is now
**additive** for a *collocated* module-object (module name == object name, e.g.
`ddc`). Previously `link=true` moved the dep core onto the `.so` but stripped it
from the object's own `test_<obj>_core`/`bench_*` (which `apply` regenerates in the
same shared CMakeLists), breaking a composing object's C test link. Now the core
is linked onto the object's test/bench (+ PUBLIC on its `_core`) **and** the `.so`.
This unblocked the **#225 `ddc` migration** (see below).

### 0.19.6 adoptions — pin bump + the hand-rolled-glue migration campaign (pin: 0.19.7)

(Pin since bumped to 0.19.7 above.) The 0.19.6 `jm apply` reconciled 7 module
aggregators — module **functions are now keyword-capable** (gh-240;
`kaiser_window(w=…, beta=…)`), for free; `_core.c` + sacred fragments untouched.

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

**Migrated so far:** (1) FIR dtype dispatch → declarative `real_type` +
`real_create_fn` on the `taps` init-param (#224); the hand-coded probe/branch in
`filter_ext_fir.c` is gone. (2) **#225 link lines for `spectral`/`measure`/`wfm`**
\[PR #154\]: module `extra_link_libs` → per-object `depends_on = [{ name="…", link=true }]` (welch→acc_trace, tonemeas→fft/spectral, wfm_synth→lo/awgn/fir).
(3) **#225 link lines for `ddc`/`ddcr`** \[PR #155, after the 0.19.7 bump\]:
`ddc.toml` carries all 8 composed cores as `link=true`; `ddcr` keeps bare-string
`depends_on` (its own core/test/bench, no `.so` contribution → no dup); module
`extra_link_libs` reduced to the non-component libm `["m"]`. `.so` link
byte-identical; `jm status --check` covers it. (4) **#225 link lines for
`resample`** \[PR, stacked on #155\]: `RateConverter` → `resamp`/`fir`,
`HalfbandDecimator` → `hbdecim`/`hbdecim_r2c` (last for the `HalfbandDecimatorR2C`
extra type) as `link=true`; `cic` is in-module and `resample_core` is the module's
own core (auto-linked, so a redundant duplicate `resample_core` is dropped from
the `.so`); `extra_link_libs` → `["m"]`. **#225 link-line migration is now
COMPLETE** for every composing module.

**#225 MECHANIC — `link=true` semantics (post-gh-254 / 0.19.7):** `link=true`
links the dep `<name>_core` directly onto the *consuming target's* link line.
For a **non-collocated** object (per-component `native/src/<obj>/CMakeLists.txt`
is surgically managed by gh-174; the `.so` aggregator is a *separate*
`native/src/<module>/CMakeLists.txt`) it adds to the `.so` only. For a
**collocated** module-object (module name == object name, e.g. `ddc` — `.so` +
object-core + test/bench all in ONE regenerated CMakeLists) gh-254 made it
**additive**: the core lands on the object's test/bench (+ PUBLIC on `_core`)
**and** the `.so`, so a composing object's C test still links. (Pre-0.19.7 it
stripped them → `undefined reference` in `test_ddc_core`; that was the bug we
filed as gh-254.) The `link=true` table name is the **component** — jm appends
`_core`. To avoid `.so` dups when several module-objects share a dep, put
`link=true` on ONE object; jm does NOT dedup the `.so` list.

**#222 `out=` steps DONE** (11 `cvt` converters + `agc`): the hand-written
`steps(x, out)` dual-path in each `_ext_<obj>.c` is now jm-generated (gh-222 +
gh-240's keyword unification in 0.19.7) — delete the fragment, `jm apply`
recreates it with a **keyword** `out=` (was positional-only by hand). For the four
`F32To*` converters the hand-patched `clipped` getset became a declared
`[[<obj>.properties]]` (`type="bool"`, `field=true`, `doc="…"` to keep the rich
docstring). **Accumulators excluded** (`acc_f32`/`acc_cf64`/`acc_trace` expose
bespoke `madd`/`add2d`/`accumulate`/… methods, not a generated block-`steps`).
NB: after regenerating a fragment, **clang-format it** (jm emits 4-space; doppler
fragments are GNU 2-space) — `jm status --check` doesn't check fragment bodies
(sacred), but pre-commit does. And **build for the `.venv`'s Python**
(`-DPython3_EXECUTABLE=$PWD/.venv/bin/python` → cpython-313 `.so`); a plain
`cmake -B build` picks system 3.14 and the venv then imports a stale 313 `.so`.

**Roadmap** (see `~/.claude/plans`): #225 link lines DONE; #222 out= steps DONE;
#244 measure `--single` DONE (pin 0.19.9). Next: #224 Resampler `optional`
`bank`; #223 verify-only. #247 (group module functions into one TU) **shipped**
in jm 0.19.9 as `functions_in_core` — optional, **not adopted** (doppler's
module functions stay one-TU-per-function; adopt later if desired).

### 0.19.9 adoptions — measure `nogil` restored (pin: 0.19.9)

The CI drift gate now pins **0.19.9**; `jm_version` stamped 0.19.9. **Drive
doppler with `uvx --from 'just-makeit==0.19.9' just-makeit …`.** 0.19.9 ships
**gh-261** (#263): the single-record binding now honours `nogil` — wraps the
by-value kernel in `Py_BEGIN/END_ALLOW_THREADS`. So re-applying the measure
fragments **restores the GIL release** the `--single` migration had temporarily
lost (4/2/2 `ALLOW_THREADS` on tonemeas/nprmeas/imdmeas, matching the old hand
fragments). The remaining jm#261 sub-item — structseq `__module__` = component
name (`tonemeas` vs `doppler.measure`) — is cosmetic and left as-is (doppler's
measure tests assert the component-name `__module__`). Also in 0.19.9:
`functions_in_core` (gh-247, off by default — not adopted).

### 0.19.8 adoptions — measure `--single` migration unblocked (superseded by 0.19.9)

0.19.8 ships three doppler-driven fixes (issue
[#257](https://github.com/just-buildit/just-makeit/issues/257) / PRs #258, #259):

- **Manifest → `jm apply` round-trip for `single` + scalar param defaults**
    (#258) — authoring `single = true` / params with `default` in TOML now
    generates the same binding + `.pyi` as the CLI, including the
    `analyze(x, lo, hi, …, guard_hz=0.0) -> record` shape (the NPR case). gh-244
    had only wired the `jm method` CLI, not the apply path doppler uses.
- **`record_name`** (#259) — names the `PyStructSequence` (`ToneMetrics`/…)
    independently of the C struct (`tone_meas_t` would derive `ToneMeas`).
- **`_dump` preserves unknown method keys** — manifest-authored keys round-trip
    instead of being silently stripped (the root cause that blocked `record_name`).

**#244 measure migration** \[PR #160, merged\]: the 3 measure objects' `analyze`
family is declarative `single = true` (+ `record_name`, + NPR geometry params with
`guard_hz` default); `*_core.c` `analyze` reconciled to **return-by-value**;
`measure.pyi` jm-generated and **dropped from `status_allow`** (0 allowed); hand
structseq fragments gone. The `nogil` GIL release was briefly inactive on 0.19.8
and is **restored on the 0.19.9 bump** (gh-261, above).

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

### `measure` — ADC/spectral metric suite (declarative `single` structseq, jm#244)

`[module.measure]` (`ToneMeasure`/`NPRMeasure`/`IMDMeasure` + capture-planning
free functions) returns **named `PyStructSequence` results** (`r.enob`,
`r.sfdr_dbc`). As of the **0.19.x migration** this is fully declarative: the
`analyze`/`analyze_complex`/`time_stats` methods set `single = true` (by-value
record binding) + `record_name = "ToneMetrics"`/… (public type name); NPR's 5
geometry doubles are declared `params` (with `guard_hz` defaulting to 0.0);
`nogil = true` releases the GIL across the kernel (jm gh-261, 0.19.9); the
`*_core.c` kernels **return the record by value**. `measure.pyi` is jm-generated
(no longer `status_allow`-listed). The fragments are sacred-but-jm-recreatable
(delete + `jm apply`). The module composes `fft_core` + `spectral_core`;
module-level helpers are one TU each. See `docs/design/measurement-suite.md`.

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
