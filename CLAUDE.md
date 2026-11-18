# doppler — codebase guide for Claude

## Project

C-first DSP library. Every algorithm lives in C exactly once; Python, Rust, and
other language bindings are thin wrappers over the C ABI. The Python layer is a
CPython C extension generated and managed by **just-makeit** (jm).

See `~/.claude/skills/just-makeit.md` for the full jm reference.

______________________________________________________________________

## The card — before you start, and before you say "ready"

The lifecycle page opens with one card,
[`docs/dev/contributing/adding-algorithms.md`](docs/dev/contributing/adding-algorithms.md#the-card):
the principles and the phases every object goes through. Its
principles, copied from between the page's markers:

<!-- spine-principles:start -->

**The principles.** They are what the phases exist to keep true:

1. **A claim names its evidence.** Every number says what prints it, and
    every claim says what test, validator or benchmark holds it.
1. **A test shows its failing case.** An assertion with no case it rejects
    is a comment.
1. **A page states what is.** The record of how it got there lives beside
    it, not in it.
1. **A constant is derived or measured.** Never fitted, ported or matched.
1. **Test what the use case can produce.** Not what will never happen.
1. **Reuse first.** Add new only when nothing existing fits.
1. **Build the general object.** An application is a configuration of it:
    the standard's numbers arrive as arguments or a table, never as the
    object's shape. A one-off is the exception, and says why.
1. **It just works with the least input.** Every knob stays, behind that.
1. **A deviation is declared.** Skipping a row is allowed; skipping it
    silently is not.

<!-- spine-principles:end -->

Green CI is not "ready". Before handing work over, walk the card's ten rows
for each object the branch touches and say which are present; a row left
out is declared in the pull request, with its reason and an issue.

______________________________________________________________________

## Architecture

Moved here from `~/.claude/CLAUDE.md` on 2026-08-25: these are doppler's
rules, and that file loads into every session in every repo.

- **C-first**: every algorithm lives in C exactly once. Wrappers call C — they
    never reimplement logic. This holds *within* C too — don't let two C
    modules each grow their own copy of the same helper. Concrete instance
    (2026-07-17): `nco_core.c`, `lo_core.c`, and `dll_core.h` each carried
    their own private `norm_freq → phase_inc` conversion; `lo_core.c`'s had
    already been fixed to round instead of truncate, but `nco_core.c` — the
    primitive's actual owner — still truncated, and `dll_core.h` had grown a
    third private copy instead of calling either. One shared, public
    primitive; every caller reuses it.
- **Thin means thin**: a wrapper should be glue (type conversion, error
    translation, lifetime bridging). If it's growing past a few hundred lines,
    it's doing too much.
- **Shared wire formats**: one `dp_header_t`, one magic value, one framing
    convention. A C transmitter must talk to a Python subscriber.
- **Test at every layer**: C tests for C, integration tests for wrappers.
- **Build artifacts in `build/`**: never scatter cmake output into the repo
    root.

______________________________________________________________________

## Landing PRs — two or more ready ones go as ONE train

`protect-main` is strict, so merging ready PRs one at a time costs one full
CI run each, in series. Land them together instead:
`make train PRS="<n> <n>"` (dry run), then `PUSH=1`, then rebase-merge the
train. Never squash it. `docs/dev/workflow.md` §4 is the policy;
`skills://merge-set` is the method. A lone PR that is merely behind:
`git rebase origin/main && git push --force-with-lease`.

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

**Any new feature, object or reshaped public surface follows
[`docs/dev/contributing/adding-algorithms.md`](docs/dev/contributing/adding-algorithms.md)
phase by phase: design page, declare, implement, pin and sabotage, bind,
instrument, explore, certify, document, land.** Open it before planning,
not after; the steps below are phase 2's and 3's mechanics only. A
PreToolUse hook (`~/mcp-store/hooks/lifecycle-spine.sh`) refuses a write to
`native/inc/doppler/**`, `objects/*.toml` or `just-makeit.toml` until the
page has been read in the session: it went unread twice with a memory
saying to read it (2026-09-27, 2026-10-30).

### Step 0 — characterise the algorithm

Ask one question:

> Can a single input sample be processed independently with small fixed state?

- **Yes** → use the CLI default (`jm object name --state ...`). A plain step/steps object.
- **No** → the C API owns block I/O. Use a `jm object --preset` (jm ≥ 0.19):
    - Block processor / decimator / resampler / FFT / correlator → `--preset blockwise`
    - Signal generator (void input, array output) → `--preset generator`

### How to run jm — through `make`, never `uvx`

**jm is a dev-group dependency, pinned once in `pyproject.toml`** (keep it in
sync with `jm_version` in `just-makeit.toml`). The Makefile is the single
driver for every jm invocation — **`make jm-apply`**, `make drift-check`,
`make jm-upgrade`, `make bench`, `make bench-save`, `make bench-compare` — and
`~/mcp-store/hooks/make-ssot.sh` BLOCKS a raw one, because the target is what
runs `uv sync` first and pins the version.

**That includes `.venv/bin/just-makeit apply`**, which reads like the pinned
binary and is not the target. Every `Drive doppler with …` line in the dated
adoption notes (`docs/dev/archive/jm-adoption-notes.md`) — `uvx --from 'just-makeit==X'` in the older ones,
`.venv/bin/just-makeit` in the newer — records **which pin was current when
that note was written**. None of them is an instruction about how to invoke
jm; `make jm-apply` is, and it is the only spelling that survives a pin bump.

**`make jm-apply` also re-renders every adopted fragment** (one with
`fragment = "generated"`; see "`_ext_<obj>.c` fragments" below), so a pin bump
can change `_ext_<obj>.c` files, not just `.pyi`; commit the diff, never
hand-patch it.
Adopting is its own driver, **`make jm-adopt JM_ADOPT_ARGS='…'`**, and the
raw `just-makeit adopt` is blocked like the rest. Both sync the dev group
*without* the project, which uninstalls the editable `doppler-dsp` from the
venv: run `make pyext` before the next test.

**Never `uvx just-makeit`.** An unpinned `uvx <tool>` does NOT fetch the latest
release: it silently reuses whatever version is installed as a uv *tool*, which
drifts per machine and never updates. That is not hypothetical — `make bench`
was the one unpinned call site and had been resolving to **0.19.28** while the
repo pinned 0.33.12, so it did not understand the `[codec.blue_keyword]` table
(jm 0.33.9), derived a bogus `codec` component, and died on
`No rule to make target 'bench_codec_core'`. The local benchmark path was
unrunnable from then on, which is why `benchmarks/published` stalled at v0.35.0.
(This is why the adoption notes are a record of pins, not a menu of commands.)

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

Each module has its own manifest, `modules/<module>.toml`, which
`just-makeit.toml` includes (`include = ["objects/*.toml", "modules/*.toml"]`):

```toml
# modules/mymodule.toml
[module.mymodule]
objects = ["existing_obj", "myobj"]
```

A script that reads module tables goes through jm's public loader,
`just_makeit.load_manifest(ROOT)` (and `just_makeit.components()` for the
component list), never a `tomllib` parse of the root file, which finds no
modules at all.

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

### Step 4b — make it serializable (REQUIRED for every stateful object)

**If the object carries any running state that survives between calls** (a phase,
a delay line, an accumulator, an integrator, a ring, an RNG), it MUST implement
the standard state triplet. This is not optional: elastic resume (checkpoint /
migrate / scale across threads, processes, pods) depends on *every* stateful
object speaking the one bytes interface. Only genuinely stateless objects (pure
converters, FFT plans, by-value analyzers) are exempt. See
`docs/design/state-serialization.md` and `native/inc/doppler/dp_state.h`.

1. **C core** — `#include "doppler/dp_state.h"` in `<obj>_core.h`, declare a per-object
    `#define <OBJ>_STATE_MAGIC DP_FOURCC(...)` + `<OBJ>_STATE_VERSION 1u`, and
    the triplet:

    ```c
    size_t <obj>_state_bytes (const <obj>_state_t *s);          /* envelope + payload   */
    void   <obj>_get_state   (const <obj>_state_t *s, void *blob);
    int    <obj>_set_state   (<obj>_state_t *s, const void *blob); /* DP_OK / DP_ERR_INVALID */
    ```

    Implement them in `<obj>_core.c` (sibling to `<obj>_reset`) with the cursor
    helpers: `dp_w_hdr` then pack the running fields **and every mutator's
    value** -- a setter's, a writable property's, a `configure`/`retune`'s
    (#2022). `create()` restores only the config no mutator reaches, and one
    that sizes the blob or changes its meaning (`sf`, a mode, a rate) is a
    reject key. The rule lives in `docs/design/state-serialization.md`, "What
    goes in the blob"; `src/doppler/tests/test_mutator_state.py` enforces it.
    `set_state` opens with `dp_state_validate(...)`, decodes into a temporary,
    checks it whole, then commits. A pointer-free POD struct can snapshot whole (`dp_w_bytes(&w, s,  sizeof *s)`); a struct with pointers packs field-wise and skips them; a
    composition delegates to its children's `*_state_bytes`/`get`/`set`. Add
    `DP_DEFINE_RUN(<obj>, <obj>_state_t, IN_T, OUT_T)` for a single-`execute`
    object to get the pure `<obj>_run(state_in, state_out, …)` transducer.

1. **Declare it** — set `serializable = "true"` in `objects/<obj>.toml`, then
    `jm apply`: jm generates the Python triplet (`state_bytes()` / `get_state()  -> bytes` / `set_state(bytes)`) **and** the `.pyi` stubs. As of **jm 0.20.0**
    this is fully hands-off for every object kind: a **sacred** `_ext_<obj>.c`
    fragment gets the triplet **transplanted** in by `jm apply` (gh-404,
    idempotent — delete any hand-added triplet and let jm own it), and a
    `kind="handle"` module generates it over the handle when the flag is on
    `[module.<name>]` (gh-403; doppler's example was `ddc_fn`'s `Ddcr`, since
    migrated to a module object — see the `ddc` section below). No
    hand-written triplet glue.
    (The legacy `DP_PY_STATE_METHODS` macro still works and is left in place on
    fragments that have it; new objects need only the flag.)

1. **Test it in both harnesses** (REQUIRED — see Step 5).

### Step 5 — build and test

```sh
make build BUILD_TARGET=<module>                          # rebuild just this .so
make test-python TEST_PATHS=src/doppler/<module>/tests/   # Python integration tests
make test                                                 # C-level tests (ctest)
```

For a **serializable** object the round-trip must be tested on **both** sides
(the standard's "both harnesses" rule):

- **C** — in `native/tests/test_<obj>_core.c`, a mid-stream split that resumes
    bit-for-bit from `get_state`/`set_state` **plus** an envelope reject (clobber
    `blob[0]`, assert `<obj>_set_state(...) == DP_ERR_INVALID`). Leaves can use
    the `DP_STATE_ROUNDTRIP_TEST` macro (`native/tests/dp_state_test.h`).
- **Python** — add the type to the parametrized matrix
    `src/doppler/tests/test_state_serialization.py` (one `(make, feed)` entry):
    it auto-checks bit-exact resume + size/clobber/non-bytes rejects. A
    frame/push object that doesn't fit the block-`execute` harness (e.g. acq)
    gets a bespoke round-trip in its own module test instead.

### Step 6 — reconcile (if TOML changed after apply)

```sh
jm apply   # idempotent; reconciles CMakeLists, __init__.py, .pyi
```

### Step 7 — commit the fragment, not the commands

```sh
git add objects/<obj>.toml native/inc/<obj>/ native/src/<obj>/
git add src/doppler/<module>/tests/ src/doppler/<module>/<module>.pyi
# serializable object: also the fragment hand-add, the C test, and the matrix
git add native/src/<module>/<mod>_ext_<obj>.c native/tests/test_<obj>_core.c
git add src/doppler/tests/test_state_serialization.py
```

______________________________________________________________________

## CI gates — current state

The jm adoption notes
([`docs/dev/archive/jm-adoption-notes.md`](docs/dev/archive/jm-adoption-notes.md))
are **dated notes**, kept as a record. Many of them say the jm drift gate is pinned in `ci.yml` + `perf-regression.yml`; that
was true when each was written.

**As of 2026-07-30 there is no `perf-regression.yml`.** It was removed
([#543](https://github.com/doppler-dsp/doppler/issues/543)) — it reported
regressions whose sign reversed locally, and being `continue-on-error` it
could not be trusted either way. The jm pin therefore lives in `ci.yml` alone
(plus `just-makeit.toml`'s `jm_version` and `pyproject.toml`'s dev-group pin).

Benchmarking did not go away. To ask whether a change is slower, run
`make bench-ab A=<commit> B=<commit>`: it alternates K passes of two
commits' builds and gives each row a verdict (#2770). `make bench-interleaved`
answers a different question: portable against native builds of ONE commit,
for a release's published set. `make bench-save` / `make bench-compare`
remain for the Python microbenchmarks. See
`docs/dev/contributing/benchmarking.md`.

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

**`_ext_<obj>.c` fragments are hand-owned only until they are adopted.**
doppler splits each module binding into per-object
`native/src/<mod>/<mod>_ext_<obj>.c` fragments that the generated aggregator
`<mod>_ext.c` `#include`s. A fragment starts out sacred, like `_core.c`:
`jm status`/`apply` regenerate **only the aggregator**, and bespoke binding
logic (e.g. HalfbandDecimatorR2C's float64→float32 input cast) lives there by
design and is **not** drift.

**An adopted fragment is the opposite: jm owns it.** `fragment = "generated"`
in `objects/<obj>.toml` stamps its fragments `/* jm:generated */`, after which
`jm apply` re-renders it from the manifest like a `.pyi`, a hand edit turns
`make drift-check` red (`STALE`), and its line comes off
`scripts/.warnings-exempt`. The #1446 migration moved most fragments over, so
**check the banner before assuming a fragment is yours to patch**; a
behaviour jm cannot render is declared in the manifest, not hand-edited in.

What is left is derived, never restated here (a count in this file goes stale
with the next adoption):

```sh
# per object: REFUSES, or what needs acknowledging
make jm-adopt JM_ADOPT_ARGS='--check --all'
# the meter: entries remaining, a ratchet that only shrinks
grep -vc '^#' scripts/.warnings-exempt
```

Read the `REFUSES` and `needs acknowledgement` entries, not the closing
`N of M object(s) could flip` total, which also counts objects already
adopted. The method — adopt, sort, what a re-render changes, which `differs`
units are removed behaviour — is `skills://just-makeit`, "Adopting fragments in
bulk".

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

### `ddc` — two down-converters, each with a matched FLAVOR

`[module.ddc]` carries two objects and two views: `DDC`/`Ddcr` (complex- and
real-input) plus `MatchedDDC`/`MatchedDdcr`, built by `<obj>_create_matched`
over the same state. **This is the model to copy when an object needs a second
constructor:** a `[[<obj>.views]]` entry (gh-504) gives a second Python class
over one core — its own `class_name`, `create_fn` and `init_params`, the
parent's methods shared verbatim (`exclude_properties`/`exclude_methods` trim),
and jm injects the view's prototype itself. There is no hand-written CPython
anywhere in the module and no `status_allow` entry.

Two axes decide the mechanism: a difference in CONSTRUCTOR is a flavor (view);
a difference in METHOD SIGNATURE is a separate type. That is why the input
dtype (`cf32` vs `f32`) stays two objects — a view shares methods verbatim, so
one class would have to name the dtype in a method.

`Ddcr` was a `kind = "handle"` module (`ddc_fn`) until Layer 3; **handles
cannot have views**, so it moved to a module object and its core split out to
`native/{inc,src}/ddcr/`. Two behaviours changed with the kind: `execute()`'s
`out=` buffer is optional now, and a wrong-dtype `out=` buffer is **refused**
rather than silently cast into a temp — fixed in jm 0.33.13, pinned by
`test_ddcr.py::test_rejects_wrong_out_dtype` and the library-wide
`src/doppler/tests/test_out_param_dtype.py`. See the gallery walkthrough
(`docs/gallery/ddc-fn.md`) for the streaming/threading model.

**Diagnostics are declarative too.** `create_error`/`create_error_message`
(gh-482, what `jm error` writes) turns a NULL `create()` into the exception the
component actually meant instead of a blanket `MemoryError`; `[[<obj>.warnings]]`
(gh-481, per-view via gh-509) emits a post-construction `PyErr_WarnEx` gated on
a **declared** bool field — `narrow_pulse` on the matched flavors is the
example. jm prints these diagnostics BEFORE its file list, so never read
`jm apply` output through a `tail`.

______________________________________________________________________

## Object validation — the certification evidence

**The process has one home: [`docs/dev/contributing/validation.md`](docs/dev/contributing/validation.md).**
Follow it there — the inventory step, the sabotage rule, the report's five
sections, the two gates and the per-object checklist. It is a contributor
page rather than a note here precisely so it is followed rather than
reconstructed.

The two things worth repeating at this altitude, because getting either
wrong wastes a day:

- **The order is not negotiable.** The C header is the SSOT: enumerate its
    prose claims, map each onto `test_<obj>_core.c` as pinned /
    pinned-only-at-literals / absent, write C tests for the uncovered ones
    and **prove each by sabotage** — and only then build `validate.py`.
    Python-first produces evidence in the wrong language for the gate, and
    measures whatever the binding happens to expose rather than what
    matters. NCO cost a rewrite this way.
- **A claim nothing runs is prose.** The tree used to live at
    `src/doppler/tests/validation/` and was executed by **nothing** — no make
    target, no CI job, `pytest --collect-only` found zero tests in it — so 44
    claims across the first two objects were asserted by nobody and two
    regressions went through them in one afternoon. If you add a phase, ask
    which of the two gates runs it.

______________________________________________________________________

## State serialization

Every stateful object resumes bit-for-bit from a serialized blob (thread /
process / pod hand-off). The rule: **serialization is module-specific; the
bytes interface is not.** The universal layer lives once in
`native/inc/doppler/dp_state.h`; each module packs only its own fields. See
`docs/design/state-serialization.md` for the full design.

- **What goes in the blob** (#2022): running state, and **every mutator's
    value** (a setter, a writable property, `configure`/`retune`/`reseed`),
    checked by that mutator's own predicate. `create()` restores only the
    config no mutator reaches; config that sizes the blob or changes its
    meaning is a reject key. The rule's one home is the design page's "What
    goes in the blob"; the gate is `src/doppler/tests/test_mutator_state.py`
    with its shrink-only list `scripts/.mutator-state-exempt`.

- **ABI triplet** (sibling to `reset`, hand-written in `<obj>_core.c`):
    `size_t <obj>_state_bytes(const T*)`, `void <obj>_get_state(const T*, void*)`,
    `int <obj>_set_state(T*, const void*)` → `DP_OK` / `DP_ERR_INVALID`. Optional
    `DP_DEFINE_RUN(pfx, STATE_T, IN_T, OUT_T)` emits the pure-transducer
    `<obj>_run` (frame/push shapes like `acq` keep a hand-written `run`).

- **Envelope**: every blob is `[dp_state_hdr_t][payload]`; compositions nest
    self-validating child sub-blobs (`[hdr][extra?][child]…`, `state_bytes = hdr + extra + Σ child_state_bytes`). Pack via the `dp_writer_t`/`dp_reader_t`
    cursors; **every `set_state` opens with `dp_state_validate`** (magic / version
    / endian / size) so a wrong blob is rejected, never reinterpreted. Per-object
    `<OBJ>_STATE_MAGIC = DP_FOURCC(...)` + `<OBJ>_STATE_VERSION`.

- **Python**: set `serializable = "true"` in `objects/<obj>.toml` → `jm apply`
    generates the `state_bytes`/`get_state`/`set_state` binding + `.pyi`. For a
    **sacred** `_ext_<obj>.c` fragment (DDC, RateConverter) the flag still
    generates the `.pyi`, but hand-add the three methods to match jm's form (jm
    #404 will transplant them). `kind="handle"` modules (`Ddcr`) can't auto-bind
    yet — jm #403.

- **Tests**: C uses `DP_STATE_ROUNDTRIP_TEST` (`native/tests/dp_state_test.h`);
    Python uses the parametrized matrix `src/doppler/tests/test_state_serialization.py`.
    Both assert bit-exact resume + envelope rejects. Add new types to both.

Blobs are native-endian POD (no cross-endian swap; endian byte rejected on
mismatch). The format is unreleased — layouts can still change freely.

______________________________________________________________________

## Build

```sh
# First time (from repo root):
cmake -B build -DBUILD_PYTHON=ON
cmake --build build

# Rebuild a single module:
make build BUILD_TARGET=filter

# Tests -- through make, never a raw pytest/ctest (the make-SSOT hook stops
# those, and the bypass skips mem-guard and the leak check):
make test-python TEST_PATHS=src/doppler/filter/tests/   # one module
make test-python                                        # the full Python suite
make test                                               # the C suite (ctest)
```

______________________________________________________________________

## Doc examples are tested (fail-closed)

Every fenced code block under `docs/` is checked in CI — **discovered,
not registered**, so a new page is gated the moment it exists. Three
fence gates (all `make test-snippets`, in
`src/doppler/tests/`): **python/pycon** (`test_doc_snippets.py` —
exec'd or `>>>`-output-checked, page = one shared-namespace notebook),
**c** (`test_c_doc_snippets.py` — compiled `-Werror` against
`build/libdoppler.a`, run, exit 0), **sh/bash/console**
(`test_sh_doc_snippets.py` — `doppler`/`doppler-specan` lines parse
against the CLIs' real `build_parser()`; safe fences execute under
`bash -e` in a per-page cwd with ```` ```json title="f.json" ````
fences materialized as files). Markers, reasons mandatory:
`skip=`, `raises=` (py), `broker=` (py+C: runs iff a NATS broker is on
:4222 — CI has one; C compiles regardless), `no-run=` (C:
compile-`-Werror` only) and `no-exec=` (sh: parse-validate only).

**Examples**: every `src/doppler/examples/*.py` runs via
`test_examples.py` (glob-discovered; skips in
`src/doppler/examples/.examples-skip`) and must **self-validate** with
physical asserts — exit 0 means demonstrated AND checked. Gallery pages
`--8<--`-include regions of these scripts, so page == tested script ==
committed PNG.

When adding/editing docs: **runnable-first** — prefer a `--8<--`
include from a tested example (the gates resolve includes), else
exec-with-real-setup; pseudocode/templates are ```` ```text ````, not
`python`. The docs build is `--strict` (zero warnings) and
`scripts/check_site_links.py` fails CI on any broken internal
link/anchor in the built site. Full policy + docs-build gotchas:
`docs/dev/contributing/doc-examples.md`; generated-vs-hand-owned map + all drift
gates: `docs/dev/contributing/docs-conventions.md`.

______________________________________________________________________

## Docs are generated in several places — never hand-edit those

`docs/c-api/**` (except `index.md`) is mkdoxy output (`make gen-c-api`);
the Python reference (`reference/doppler/<module>/`) is generated from each
package's `__all__` by mkdocs-api-autonav and mkdocstrings, so a public name
reaches it by existing (`check_surface.py` holds the built site, and lint's
`surface-check` holds `__all__` and `docs/.surface-unrendered`, #3000) --
its prose lives in the C header, the module's `[module.X] doc`, or a guide,
never in a hand page;
`README.md`'s entire body below the badges is generated from
`docs/index.md`'s readme-sync region by `scripts/gen_readme.py`
(`make docs-relink`; never hand-edit between `<!-- readme-sync:start -->`/
`:end`);
`docs/example-projects/<name>.md` is generated by `scripts/gen_example_projects.py` from the project folder (README body between its `[start:body]`/`[end:body]` markers, a Get the code block, one tab per tracked file; a new file reaches its page on `make docs-relink`);
`tests/install/build-*-deps.sh` are generated from `bootstrap.toml`'s `[dev.*]`
lists by `scripts/gen_install_scripts.py` (same `make docs-relink`).
**The nav is discovered, not listed**: `docs/.nav.yml` sets the top level,
and each section directory's own `.nav.yml` ends in a glob, so a new page
appears in the nav by existing. A page's one-line summary is its
`description:` front matter. Full table of what's generated vs. hand-owned:
`docs/dev/contributing/docs-conventions.md`.

______________________________________________________________________

## Code style

- **C**: `clang-format` with `.clang-format` at repo root (GNU, 79 cols — copied
    from just-makeit so generated glue is byte-identical to jm's output)
- **Python**: `uvx ruff format --line-length=79`
- Line width: 79 characters in all languages
- Docstrings: numpy-style, verbose — explain how it works, not just what
- Inline comments: explain intent and non-obvious mechanics only

### pre-commit (`.pre-commit-config.yaml`)

`pre-commit run --all-files` lints + formats **hand-written** code and docs; a
CI `pre-commit` job enforces it.

**Every hook dispatches to a make target.** `entry: make -s lint-<tool>`,
`language: system`, `pass_filenames: false` — no hook carries its own `rev:`,
no hook names a bare binary, and no hook inlines a command. A hook that did
any of those would be a SECOND source of truth for the same tool, which is
exactly how clang-format came to be pinned at `v22.1.8` by
`mirrors-clang-format` while `make`, a bare shell and jm each resolved
whatever the box had. just-makeit gh-745 was filed from this repo on the
consequence: 21.1.8 locally against 22.1.8 in CI, identical input giving
different output. clang-tidy was worse — a bare `entry: clang-tidy`, no pin
anywhere, absent from at least one dev box, so a gate that could not run.

**There is now no clang-tidy hook at all** (gh-737, removed 2026-08-15). Fixing
the pin left a second defect untouched: it sat at `stages: [pre-push]`, `make setup` runs a plain `pre-commit install` (which installs only the pre-commit
hook type absent a `default_install_hook_types` declaration, and this repo has
none), and CI's `make lint` runs pre-commit at the default stage — so it
executed on no machine and in no pipeline, reporting zero findings because it
never looked. Deleting it costs no coverage, because there was none; what it
buys is that the tree no longer advertises a gate it does not have.
`make lint-clang-tidy` still exists and is still how to run it by hand. Putting
it back needs an execution home (a CI job, or `default_install_hook_types`) and
the backlog cleared enough to pass — see gh-720/gh-737.

Versions live in `pyproject.toml`'s dev group (exact pins — these rewrite
source, so an upgrade is a deliberate commit), the Makefile says how they
run, and `uv.lock` is the one pin. **This supersedes the old carve-out** that
kept clang-format and cmake-format on upstream mirrors as "not Python project
dependencies": both ship as PyPI wheels, so that was never an obstacle.

File lists and exclusions live in the Makefile too — `C_EXCLUDE_RE` beside
`MD_EXCLUDE_RE` — so `make format` and the hook cannot disagree about which
files are ours.

The hard rule on generated code is unchanged: **jm-generated glue is NOT
formatted by pre-commit** — it is owned by `jm apply` and guarded by the
`manifest-drift` gate. As of jm 0.55.2 doppler sets `c_style = "clang-format"`
with `c_format_command` routed through uv, so **jm formats its own C with the
project's pinned binary and the committed `.clang-format`** — the render and
the tree agree without a manual pass. Excluded from pre-commit's own
clang-format: `native/inc/**` headers (jm injects public decls), per-module
`*_ext.c` aggregators, and `vendor/**`. Also excluded elsewhere: `*.pyi`
stubs, per-module `CMakeLists.txt`, the re-export `__init__.py` shims, the
pep723 `wavegen.py`, `docs/c-api/**`, and
`src/doppler/source/tests/validation/**/results.md` (generated by each object's own
`validate.py`).

**just-makeit#917 is FIXED in 0.55.2** — the manual `make lint-clang-format`
pass after every `jm apply` is no longer needed, and this file said it was
until the bump. `c_style` now reaches members reconciled in place: jm's
formatter glob had missed the per-object `<module>_ext_<obj>.c` fragments
gh-729 introduced, *and* `apply` formatted only the throwaway scaffold it
compares against, so a reconciled member landed in jm's K&R inside a GNU
file. Verified on adoption: after `jm apply` on 0.55.2,
`make lint-clang-format` changes nothing.

After any change that could touch generated files, re-run `jm status --check`.
