<!-- domain: acquire; summary: step 4 of the correlator consolidation (#2752): one Python class, doppler.correlation.Correlator, its constructor and knobs, what push returns, the classes and burst_acq forwarder it retires, its composers, and the #2683, #2900 and #2901 decisions -->

# One Python class: the step-4 design

Step 4 of the consolidation ([#2752](https://github.com/doppler-dsp/doppler/issues/2752),
[API taxonomy §6.4](api-taxonomy.md#64-consolidation-one-primitive-correlator)).
Step 3 left one C object with seven constructors
([correlator.md](correlator.md)). Python still sees seven classes in two
modules:

- `doppler.spectral`: `Corr`, `Corr2D`, `DelayDoppler`, `CorrDetector`,
    `CorrDetector2D`;
- `doppler.acquire`: `Acquisition`, `BurstAcquisition`.

Step 4 makes them one class, `doppler.correlation.Correlator`. The knobs are
its arguments, burst or continuous framing included. No `Acquisition` or
`BurstAcquisition` class survives (owner, 2026-10-31), and the `burst_acq`
C forwarder goes with them (step 1, folded here).

This page also settles three linked issues that step 4 needs:

- [#2683](https://github.com/doppler-dsp/doppler/issues/2683): the Python
    batch contract (§10);
- [#2900](https://github.com/doppler-dsp/doppler/issues/2900): bench rows
    that keep their history across a rename (§11);
- [#2901](https://github.com/doppler-dsp/doppler/issues/2901): the flavor
    recorded twice (§12).

The owner's two rules still hold: no private copy survives, and speed only
goes up. Step 4 is mostly binding and documentation. Its C changes are a
constructor dispatcher (§3), `BurstCapture` calling the engine directly
(§8), and #2901 (§12). This follows
[Adding an algorithm](../dev/contributing/adding-algorithms.md) from phase 1.

§16 lists the decisions, accepted on review, and §17 what they left open.

## 1. Who calls it, and with what

| caller                                                  | needs                                                                     |
| ------------------------------------------------------- | ------------------------------------------------------------------------- |
| A user correlating a block against a template           | maps: one per call, or a batch reduced in C                               |
| A user searching a code over delay x Doppler            | maps, a hypothesis bank, the peak per map                                 |
| A user detecting a reference in a stream                | detections from `push`, threshold set as k x noise                        |
| A user acquiring a DSSS signal or a burst               | detections from `push`, threshold set from Pfa/Pd and a C/N0 design point |
| `BurstCapture`, `DsssReceiver`, `AsyncDsssReceiver` (C) | the engine, composed in C; Python never sees it                           |
| `doppler.dsss.orchestrator` (Python)                    | a burst search per channel, built as `BurstAcquisition` today             |

The last row is a fourth consumer, which the taxonomy's table
([§6.4](api-taxonomy.md#64-consolidation-one-primitive-correlator)) now lists too. It
builds one `BurstAcquisition` per channel (`orchestrator.py:214`) and hands
it out through an `acquisition` property.

## 2. Today's seven surfaces

Measured from the stubs on main (0bb4824d0), excluding the context-manager
and state-blob members every class has:

| class              | module   | constructor                                                                                                                               | methods                                                                    | properties |
| ------------------ | -------- | ----------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------- | ---------- |
| `Corr`             | spectral | `ref, dwell, nthreads, n_out`                                                                                                             | `execute`, `reset`                                                         | 4          |
| `Corr2D`           | spectral | `ref` (2-D), `dwell, nthreads, ny_out, nx_out, col_out`                                                                                   | `execute*`, `bank_*` (2), `*_max_out`                                      | 8          |
| `DelayDoppler`     | spectral | `code, ny, dwell, nthreads, ny_out`                                                                                                       | as `Corr2D`                                                                | 7          |
| `CorrDetector`     | spectral | `ref, dwell, noise_lo, noise_hi, noise_mode, threshold, nthreads`                                                                         | `push`, `reset`                                                            | 8          |
| `CorrDetector2D`   | spectral | as `CorrDetector`, 2-D `ref`                                                                                                              | `push`, `reset`                                                            | 10         |
| `Acquisition`      | acquire  | `code` (uint8), `sample_rate, chip_rate, symbol_rate, cn0_dbhz, doppler_uncertainty, pfa, pd, noise_mode, code_only_epochs, doppler_rate` | `push`, `surface*` (4), `block_*` (2), setters (4), `configure_search_raw` | 33         |
| `BurstAcquisition` | acquire  | `preamble, reps, fs, cn0_dbhz, doppler_uncertainty, pfa, pd, noise_mode, doppler_rate, rate_hypotheses`                                   | `push`, `set_max_peaks`, `configure_search_raw`                            | 24         |

Six of them are the `correlator` object today: `Corr2D` its base class,
the other five its views. `BurstAcquisition`
is the seventh: its own object, `burst_acq`, a C forwarder onto the same
engine.

Two inconsistencies a reader meets today, which one class removes:

- **One quantity, several names.** The sample rate is `sample_rate`
    on `Acquisition` and `fs` on `BurstAcquisition`. The reference is `ref`,
    `code` or `preamble`.
- **`push` returns two shapes.** `CorrDetector*` return 5-field tuples;
    `Acquisition` and `BurstAcquisition` return 9-field tuples of the same
    C record, `dp_correlator_hit_t`.

## 3. One constructor

**The constraint.** A jm object has one constructor (`create_fn`). Views
give a second constructor, but each view is a separate class with its own
name. Alternate constructors (`factories`) exist only for `kind = "handle"`
modules, not objects (`just_makeit/_keys.py`). And a module's
`__init__.py` is re-export only, so a Python factory function is out.

So "one class whose knobs are its arguments" means one C constructor that
takes every knob and dispatches to the flavor's own constructor. The
dispatch lives in C, once. The seven flavor constructors stay public for C
callers, who name the flavor they want.

**Proposed C entry point** (`correlator_core.h`):

```text
dp_correlator_state_t *dp_correlator_create (
    const float _Complex *ref, size_t ny, size_t nx,
    dp_correlator_framing_t framing,  /* NONE, CONTINUOUS, BURST */
    size_t doppler_rows,              /* > 0: a code row, delay x Doppler */
    int detect,                       /* maps or detections (framing NONE) */
    size_t dwell, size_t ny_out, size_t nx_out, int col_out,
    float threshold, size_t noise_lo, size_t noise_hi, int noise_mode,
    double sample_rate, double chip_rate, double symbol_rate, size_t reps,
    double cn0_dbhz, double doppler_uncertainty, double pfa, double pd,
    double doppler_rate, size_t rate_hypotheses, size_t code_only_epochs,
    int nthreads, const char **why);
```

It is positional because jm's `init_params` map one to one onto the C
constructor's arguments; the Python face is the keyword call below. C
callers are pointed at the seven flavor constructors, and the
dispatcher's own doc in `correlator_core.h` says so: it is the binding's
entry point, not one a C caller should write by hand.

**How the knobs pick the flavor:**

| `framing`    | `ref`     | `doppler_rows` | `detect` | flavor              | replaces           |
| ------------ | --------- | -------------- | -------- | ------------------- | ------------------ |
| `none`       | 1-D       | 0              | no       | `1D`                | `Corr`             |
| `none`       | 1-D       | 0              | yes      | `DETECTOR_1D`       | `CorrDetector`     |
| `none`       | 2-D       | 0              | no       | `2D`                | `Corr2D`           |
| `none`       | 2-D       | 0              | yes      | `DETECTOR_2D`       | `CorrDetector2D`   |
| `none`       | 1-D       | ny > 0         | no       | `DELAY_DOPPLER`     | `DelayDoppler`     |
| `continuous` | 1-D chips | -              | implied  | `SEARCH_CONTINUOUS` | `Acquisition`      |
| `burst`      | 1-D       | -              | implied  | `SEARCH_BURST`      | `BurstAcquisition` |

Any other combination is refused with a `ValueError` naming the knob. For
example, a 2-D `ref` with `framing = "burst"`, or `doppler_rows` with
`detect`. **A value the flavor would not honor is refused**, never
silently ignored: a knob the flavor does not read, set away from its
default, is one. The dispatcher writes the sentence through its trailing
`why`, which the binding raises (`create_why`, just-makeit#2552; C callers
pass NULL). A value the flavor itself refuses (`dwell = 0`, `pfa = 1`, a
fractional code period) is named too: the dispatcher calls each flavor's
internal constructor with its `why`, and the public flavor constructors
pass NULL, so every refusal is one sentence that names one knob.

**Python spelling** (defaults as §17 rules them):

```text
Correlator(ref, *, framing="none", doppler_rows=0, detect=False,
           dwell=1, ny_out=0, nx_out=0, col_out=-1,
           threshold=0.0, noise_lo=0, noise_hi=<last bin>,
           noise_mode="mean",
           sample_rate=0.0, chip_rate=0.0, symbol_rate=0.0, reps=1,
           cn0_dbhz=nan, doppler_uncertainty=0.0, pfa=1e-3, pd=0.9,
           doppler_rate=0.0, rate_hypotheses=1, code_only_epochs=1,
           nthreads=1)
```

One name for each quantity (§2): `ref` for every reference,
`sample_rate` on both search flavors, and `nx_out` for the padded lag
axis at any row count. `fs`, `preamble` and `Corr`'s `n_out` retire.

**Three things jm must express, to confirm on a scratch project in phase 2
before any code:**

1. **One `ref` that is 1-D or 2-D.** Today `Corr2D`'s `ref` is
    `float _Complex[][]` and `Corr`'s is `float _Complex[]`. One init-param
    has one type. Fallback: `ref` is 2-D always, and a 1-D reference is
    `ref[None, :]` (ny = 1). That is a worse spelling for the commonest
    case, so it is the fallback, not the plan.
1. **The continuous search's chips.** `dp_correlator_create_continuous`
    takes `uint8_t` chips at the chip rate; every other flavor takes cf32
    samples. Recommended: the dispatcher takes the chips as cf32 NRZ
    (+1/-1, cvt's mapper), as the burst flavor already takes its preamble.
    Then "the reference is always samples" holds, and `DsssReceiver`'s C
    path keeps calling the uint8 constructor directly. The dispatcher
    refuses any chip that is not exactly +1 or -1 (a `create_error` naming
    `ref`), so a shaped or noisy reference cannot quantize silently.
1. **A string-enum `framing`.** jm has `string_enum:` init params
    (`noise_mode` uses one), so this is expected to work as-is.

If (1) needs a jm feature, the ask is filed and fixed upstream first
(owner: fix it where it is defined), never a hand-written fragment.

## 4. Members a flavor does not serve

One class carries every flavor's members: about 20 methods and 40
properties once duplicates merge. Most belong to some flavors only:
`execute*` and `bank_*` to the map flavors, `push` to detectors and
searches, `surface*`, `eta`, `pd_predicted` and the rest to searches.

- **Methods** already refuse: the C returns `SIZE_MAX` or an error code on
    a flavor that does not serve the call, and the binding raises. The
    docstring names the flavors that serve it.
- **Properties on the search record** read through `search`, which is
    `NULL` for a map or detector flavor. They **raise** there, an
    `AttributeError` naming the flavors that serve the property, through a
    C accessor that refuses off a search flavor. A NaN or a 0 would be a
    silent wrong answer: `Correlator(ref).eta` reading NaN looks like a
    measurement. Whether a jm property getter can raise is checked in phase
    2 with the items in §3. If it cannot, that is a jm ask, filed and
    fixed upstream before the class ships (the path just-makeit#2516
    took), never a NaN fallback.
- **A `flavor` property** (string) says which flavor the knobs built, so
    a caller can ask before reading a member, instead of catching the error.

## 5. What `push` returns

One record for every flavor: `dp_correlator_hit_t`, returned as a NumPy
**structured array** (`record_dtype`), the shape `BurstCapture.detections()`
already returns. Two reasons:

- One shape for one record, instead of 5-field tuples on detectors and
    9-field tuples on searches.
- It removes the stack cap. Today the binding calls into a stack array of
    `max_results = 1024` hits, and a push that would make more loses the
    input past the last one
    ([#1992](https://github.com/doppler-dsp/doppler/issues/1992),
    just-makeit#2184). A `variable_output` record array sizes from a
    `*_max_out` instead.

The fields keep their C names: `row`, `col`, `peak_mag`, `noise_est`,
`test_stat`, `cn0_dbhz_est`, `samples_consumed`, `chip_phase`,
`doppler_rate_hz_s`, `test_stat_peak`. Off a search flavor, the search-only
fields read 0, as the C fills them today.

To confirm in phase 2: jm renders `record_dtype` + `variable_output` on a
method that also takes a cf32 input array. `BurstCapture`'s record methods
take no input (`arg_type = "void"`).

## 6. State serialization

`correlator.toml` already declares `serializable = "true"` once for every
flavor, and each flavor's blob names its flavor, so a blob from one is
refused by another (step 3.4b). Step 4 keeps that one declaration; what
changes is the Python side.

- **The state matrix** (`src/doppler/tests/test_state_serialization.py`)
    gains one `(make, feed)` entry per flavor with running state, spelled
    `Correlator(...)`, replacing the retired classes' entries one for one:
    `Corr`, `Corr2D`, `DelayDoppler`, `CorrDetector`, `CorrDetector2D` and
    `BurstAcquisition (Zadoff-Chu)`. `Acquisition` has no entry today
    (#2078); the continuous flavor gets one, a split mid-stream through
    `push`, so it is held like the other six.
- **The #2022 mutator gate** (`test_mutator_state.py`) sees the merged
    members: every mutator is in the blob or on
    `scripts/.mutator-state-exempt`, and that list only shrinks. Its rows
    move from the retired names to `Correlator`'s
    (`Acquisition.keep_surface` becomes `Correlator.keep_surface`), and the
    seven `Acquisition.* NO_RECIPE` rows, which exist only because
    `Acquisition` had no matrix entry, are probed once the continuous
    flavor has one. Each then leaves the list or stays with its own reason.
- **Cross-flavor refusal through Python:** one test, a blob from each
    flavor offered to every other, each refused with `ValueError` and the
    target unchanged, the Python twin of the C
    `_search_flavor_cross_load_check`.

## 7. The module

- **New module `doppler.correlation`**: `modules/correlation.toml`, objects
    `["correlator"]`, and a `[domain.correlation]` entry in
    `docs/domains.toml`. The C object stays at
    `native/{inc/doppler,src}/correlator/`; only its Python module moves.
- **`doppler.spectral`** loses `Corr`, `Corr2D`, `DelayDoppler`,
    `CorrDetector`, `CorrDetector2D`, and its module docstring stops
    naming them.
- **`doppler.acquire`** loses `Acquisition` and `BurstAcquisition`. It
    keeps `CarrierAcquisition`, `BurstCapture`, `PersistentBurstCapture`
    and `bin_to_signed`, and its docstring points at `Correlator` for the
    search.
- **No aliases.** The old names are not re-exported, deprecated or kept
    as subclasses (owner: none survives). This is step 4's Python break,
    released once, with the changelog entry naming each old class and its
    `Correlator(...)` spelling.

`Correlator`'s docstring and guide say it is "also called acquisition when
it is part of a detector or receiver", so a search for the word lands on it.

## 8. Retiring `burst_acq`

What went, in step 4c: `burst_acq`'s object, its core, its binding
fragment, its C tests and its C bench. What it held moved:

- its Python tests, with the class, to
    `src/doppler/correlation/tests/test_correlator_burst.py`;
- its Python bench to `src/doppler/correlation/benchmarks/`, stem kept
    (§13);
- its C bench rows, under their names, into
    `native/benchmarks/correlator/bench_correlator_core.c`, with
    `c:burst_acq` mapped to `c:correlator` in `benchmarks/renames.toml`.

**`BurstCapture` calls the engine directly.** It makes eight `burst_acq_*`
calls (`create` x2, `destroy`, `reset`, `push`, `consumed`,
`configure_search_raw`, and the state type in its header). Each becomes the
`dp_correlator_*` call the forwarder made. `underpowered` already lives on
the search record. `burst_capture.toml`'s `depends_on` names `correlator`
instead of `burst_acq`.

Oracle: `BurstCapture`'s C and Python tests and its validator
(`src/doppler/acquire/tests/validation/burst_capture/`), unchanged and
green; and its state blob byte-identical, since the forwarder's blob
already was the engine's.

## 9. The objects that compose it

| consumer            | today                                        | step 4                                                                                |
| ------------------- | -------------------------------------------- | ------------------------------------------------------------------------------------- |
| `BurstCapture`      | C, through the `burst_acq` forwarder         | C, `dp_correlator_*` directly (§8)                                                    |
| `DsssReceiver`      | C, `dp_correlator_state_t`                   | unchanged                                                                             |
| `AsyncDsssReceiver` | C, `dp_correlator_state_t`                   | unchanged                                                                             |
| `dsss.orchestrator` | Python, a `BurstAcquisition` on the preamble | `Correlator(preamble, framing="burst", ...)`; its `acquisition` property returns that |

All three receivers forward `configure_search_raw` with their own
docstrings. Those docstrings name `BurstAcquisition.doppler_bins` and
`Acquisition`; they are rewritten to name `Correlator`.

## 10. #2683: the Python batch contract

What #2683 asked for and where it stands:

| ask                                                     | state                                                                                                                                               |
| ------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| a batch interface with documented axes                  | done in C and Python: `execute_all`, `execute_coherent`, `execute_power_sum`, `execute_power_max`, `execute_peaks`, `bank_power_max`, `bank_detect` |
| coherent / independent / power sum / power max          | done, each a fixed C reducer                                                                                                                        |
| a detector-specific statistic without a Python callback | C only: `dp_correlator_execute_fold` and `bank_fold` take a C reducer                                                                               |
| value tests for every reducer                           | partly: see below                                                                                                                                   |

**Recommended: the fixed reducers are the Python contract.**
`execute_fold`'s reducer is a C function pointer called once per map. A
Python callable there is the inner-loop callback #2683 forbids, and a
pointer from `ctypes` or `cffi` is a foreign object no other binding
accepts. A caller who needs a statistic the fixed reducers lack takes
`execute_all` (every map, no accumulation: #2683's "streaming
independent-map output") and reduces in NumPy, or writes the reducer in C.
The `Correlator` docstring says so, and #2683 closes on that.

**Value tests still missing**, each against a NumPy reference built from
`execute_all`'s maps:

- `execute_power_sum` and `execute_power_max` values, including the
    originating map index for power max and its tie rule;
- max of per-map statistics differs from the statistic of a pointwise-max
    map (the cancellation case #2683 names);
- a batch called mid-dwell, for each reducer beyond `all`, `coherent` and
    `peaks`: the temporal accumulator is unchanged after it;
- `reset()` before and after a batch;
- an `out=` buffer reused across calls, poisoned per run.

They move with the class into `src/doppler/correlation/tests/`. Each is
proved by sabotaging its reducer in a copy.

## 11. #2900: bench rows keep their history across a rename

Step 4 renames rows again: `py:corr2d::*`, `py:acq::*` and
`py:burst_acq::*` become `py:correlator::*`, and `c:burst_acq::*` becomes
`c:correlator::*`. Without #2900 each one starts a new trend line and its
labels are carried twice for a release, as `c:corr::` and `c:corr2d::` are
now.

**Proposed: one table, `benchmarks/renames.toml`.**

```toml
[[rename]]
old = "c:corr::"
new = "c:correlator::"
```

Three readers, one declaration (owner: one declaration, both directions):

- `bench_report` joins a row's history across the table, so the trend line
    continues.
- The label gate accepts a label under the new ID for a published row
    under the old one, so `labels.toml` holds one key per row.
- `make bench-ab` pairs renamed rows through the table; `ALIAS=` is gone.

Its gate: a rename whose `old` prefix names no published row, or whose
`new` names no current bench, fails `make lint`. Proved by sabotage like
every gate. Rows are prefixes, so a rename entry may also rename a single
row when the bench IDs change shape.

#2900 lands as its own PR before step 4's code, so step 4's renames are
entries in the table from the start. Its follow-up stays as #2900 names it:
the old `c:corr::`/`c:corr2d::` label keys go once the next release
publishes the new rows.

## 12. #2901: the flavor recorded twice

`dp_correlator_state_t` has `flavor` and the older `delay_doppler` flag,
which is 1 exactly when `flavor == DP_CORRELATOR_DELAY_DOPPLER`.

**Recommended: drop `delay_doppler` and branch on `flavor`.** The
dispatcher (§3) makes `flavor` the one fact every path reads, and a
compare of one enum against a constant costs the hot path nothing a
flag-read did not. The struct-layout risk #2901 names is measured, not
assumed: its own PR, `make bench-ab` against main on the pinned rows, and
the pool row that #2886 diagnosed among them. If any row is slower, the
other fix (keep the flag, set it only from `flavor` in one place) takes
its place.

## 13. Docs, examples, validation and benches

**Guides and gallery**, rewritten for `Correlator`:

- guides: `docs/guide/acquisition.md`, `delay-doppler.md`,
    `dsss-acquisition.md`; a new `Correlator` guide replaces the `Corr` and
    `CorrDetector` exemptions (`docs/.user-docs-exempt`, lines 90-91);
- gallery: `corr.md`, `delay-doppler.md`, `detection2d.md`,
    `dsss-acq-characterization.md`;
- API pages: a new page for the `correlation` module; the `spectral` and `acquire`
    pages lose the seven classes.

**Examples ported, not dropped** (13):

- `acq_surface_demo`, `burst_capture_demo`, `corr_demo`,
    `delay_doppler_bank_demo`, `delay_doppler_batch_demo`;
- `detection2d_demo`, `detector2d_acq_demo`, `dsss_burst_ber_demo`,
    `dsss_burst_demo`, `dsss_realtime_file_demod`;
- `dsss_receiver_demo`, `mpsk_doppler_profile_demo`,
    `mpsk_receiver_performance_demo`.

Each keeps its physical asserts. The C twins (`native/examples/corr_demo.c`,
`delay_doppler_bank_demo.c`, `delay_doppler_batch_demo.c`) already call the
C flavor constructors, which stay; they are unchanged. Phase 9 owes a C and
a Python example for the one class, and these are them. A gallery PNG it draws is regenerated and
must not change beyond its own tolerance. Every retired class's docstring
example is ported into `Correlator`'s, or into a guide whose fences are
tested.

**Validation.** The one report from step 3.5
(`src/doppler/spectral/tests/validation/correlator/`) moves to
`src/doppler/correlation/tests/validation/correlator/` and its Python face
switches to `Correlator`. The verdicts are the oracle (§14); the report is
not re-certified unless a verdict moves.

**Benches.** The seven Python benches (`bench_corr.py`,
`bench_correlator.py`, `bench_delay_doppler.py`, `bench_detector.py`,
`bench_detector2d.py`, `bench_acq.py`, `bench_burst_acq.py`) move to the
`correlation` module's benchmarks and keep their file stems. A Python
row's ID is `py:<stem>::<test>`, so every published row keeps its ID and
its history, and §11's table needs no entry. One merged file could not:
`bench_detector` and `bench_detector2d` both have `test_bench_push_64k`,
and the table maps one family to one family, never a single row (one
row, one provenance; ruled 2026-11-08).

## 14. Oracle and speed gate

- **Behaviour.** Every test that names a retired class runs against its
    `Correlator(...)` spelling with its expected values unchanged: the
    composing objects' tests and validators, the correlator's validation
    verdicts, every ported example and doc fence. A changed value is a
    finding, not an update.
- **Retirement.** No new gate: each retired class name and the
    `burst_acq_` prefix get a row in `scripts/.retired-names`
    (`check_retired_names.py`) in the commit that retires them, and the
    `burst_acq_*` symbols a row for `check_retired_symbols.py`.
- **Speed.** `make bench-ab A=main B=<branch>` on the pinned rows of
    [correlator.md §7.1](correlator.md), plus the renamed Python rows. Step
    4 changes no hot loop, so every row is expected to tie. The binding's
    record-array return (§5) is the one change on a measured path; its
    `push` rows are judged against the tuples they replace.

## 15. Order of work

Each item is its own PR. The release of step 3's C break comes first
(#2752's order).

1. **#2900**: `benchmarks/renames.toml` and its three readers (§11).
1. **#2901**: one flavor field, with its own `bench-ab` (§12).
1. **Phase 2 on a scratch project**: the three jm checks in §3, the
    property getter in §4 and the record array in §5. Any gap is filed and
    fixed upstream before 4.
1. **The class**: `dp_correlator_create`, `modules/correlation.toml`, the
    seven views removed, the record array, the #2683 tests.
1. **`burst_acq` retired**: `BurstCapture` on the engine directly, the
    orchestrator on `Correlator` (§8, §9).
1. **Docs and examples** ported (§13).
1. **Release** step 4's Python break.

Items 4 to 6 may land as one PR if splitting them would leave main with a
class whose examples do not run.

## 16. Decisions for review

| #   | decision                           | recommended                                                                               |
| --- | ---------------------------------- | ----------------------------------------------------------------------------------------- |
| D1  | how one class gets every flavor    | one C dispatcher, `dp_correlator_create`, with every knob (§3)                            |
| D2  | the continuous search's reference  | cf32 NRZ chips, anything not exactly +-1 refused; C keeps the uint8 constructor (§3)      |
| D3  | a member its flavor does not serve | methods and properties raise; a getter jm cannot raise from is a jm ask, fixed first (§4) |
| D4  | `push`'s return                    | one structured array of `dp_correlator_hit_t`, no stack cap (§5)                          |
| D5  | old class names                    | none kept, no aliases (§7)                                                                |
| D6  | #2683's extensibility              | fixed reducers + `execute_all` are the Python contract; fold stays C (§10)                |
| D7  | #2900's shape                      | `benchmarks/renames.toml`, read by report, label gate and bench-ab (§11)                  |
| D8  | #2901                              | drop `delay_doppler`, branch on `flavor`, if bench-ab ties (§12)                          |

D1 to D8 were accepted as recommended (reviewer, 2026-11-08); §17 lists
what they left open.

**The unknowns, checked on a scratch project with just-makeit 0.123.0**
(2026-11-08), each the shape step 4 uses:

| unknown                                      | answer                                                                                                                                                                 |
| -------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| a 1-D or 2-D `ref` in one init param (§3)    | yes: `rank = [1, 2]` on a `T[][]` init param (just-makeit#2537). A 1-D array arrives as one row, `(1, n)`, and a 3-D one is refused                                    |
| a property getter that raises (§4)           | yes: `getter_status = true` with `error = "AttributeError"` (just-makeit#2538). The C getter returns non-zero, the read raises with the message, `hasattr` reads False |
| a record array from a method with input (§5) | yes: `record_dtype` + `variable_output` on a cf32 `push`, sized by `push_max_out`; one call returned 10,000 records, with no stack cap                                 |
| `framing` as a string enum (§3)              | yes, as `noise_mode`                                                                                                                                                   |

## 17. What one constructor must choose

Merging seven constructors into one leaves one default per knob, and the
seven classes disagree on some of them today. A knob a flavor reads takes
the one default, and a value the flavor would not honor is refused (§3).
Measured from the manifests on main (84049024d); every row accepted on
review (2026-11-08):

| knob                           | today                                                                                    | recommended                                                                                                                                                |
| ------------------------------ | ---------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| a one-row `ref`                | `Corr` (1-D) and `Corr2D` at `ny = 1` are two flavors                                    | the 1-D flavor, given 1-D or `(1, n)`; `nx_out` pads it, `n_out` retires                                                                                   |
| `noise_hi`                     | the detectors: the `SIZE_MAX` sentinel, clamped to the last bin                          | that sentinel; §3's `0` would make the floor one bin                                                                                                       |
| `reps`                         | `BurstAcquisition`: 1                                                                    | 1                                                                                                                                                          |
| `sample_rate`                  | `BurstAcquisition`'s `fs`: 1.0; `Acquisition`: 4 MHz                                     | 0.0, not given: a search flavor refuses it                                                                                                                 |
| `chip_rate`                    | `Acquisition`: 1 MHz                                                                     | 0.0, not given: the continuous search refuses it                                                                                                           |
| `symbol_rate`                  | `Acquisition`: 1 kHz                                                                     | 0.0, no known data clock: the continuous search honors it (a diagnostic, never sizing)                                                                     |
| `cn0_dbhz`                     | `BurstAcquisition`: NaN, no design point; `Acquisition`: 50 dB-Hz, and a finite one only | NaN, no design point: the burst search's meaning today; the continuous search refuses it                                                                   |
| `detect` on a search framing   | a search always detects                                                                  | `True` accepted: a search honors it, so the rule accepts it                                                                                                |
| `nthreads` on a search framing | a search sizes its own pool at create; `set_threads()` sets it after                     | refused away from 1, with a sentence pointing at `set_threads()`: the search honors a count, but not through this knob, whose 1 would mean another request |

**A one-row reference is the 1-D flavor.** `rank = [1, 2]` hands C a 1-D
array as one row, so C cannot tell `ref` from `ref[None, :]`.
`dp_correlator_create_1d` is `dp_correlator_create_2d` at one row, its
`n_out` passed as `nx_out`, so the two differ only in their output's shape
(1-D against `(1, n)`) and in `col_out`. The one-row detector already is
the 2-D one at one row (correlator.md §2.1). So a one-row reference builds
the 1-D flavor. Zero-padding it stays reachable under the one padding
name, `nx_out`: `Corr`'s `n_out` was the same fact under a second name,
and it retires. `ny_out` and `col_out` are refused at one row. No Python caller on
main builds a one-row 2-D correlator: the `col_out` callers
(`test_corr2d_batch.py`, the correlator validator) use four rows.

**A rate is never a default.** `fs = 1.0` and the 4 MHz / 1 MHz / 1 kHz
triple are facts about one caller's signal, and a search sized by
omission with the wrong one detects at the wrong Doppler without a word.
A search flavor refuses a rate it reads that is still 0, naming the knob;
a flavor that does not read a rate is not asked for it. `symbol_rate` is
not such a rate: the continuous search reads it only for a diagnostic
(`epochs_per_symbol`), never to size the search, and its own 0 means no
known data clock, so 0 is honored there. Normalized units
stay one stated argument away, `sample_rate=1.0`. Every port states its
rates, and the changelog names both old defaults.

**`cn0_dbhz` defaults to no design point.** One default cannot be both
NaN and 50 dB-Hz. NaN keeps the burst search as it is. The continuous
search sizes its dwell from a finite design point and refuses NaN today,
so it still does: `Acquisition`'s 50 dB-Hz was a design point chosen by
omission, and a caller now names one.

## 18. The one class's members, and who serves each

One class can call any member on any flavor. Which flavors serve each is
decided in one place for methods and one for properties, never in the
binding (D3: the binding adds no refusal of its own):

- **A method is served where the flavor honors it**, not merely where it
    does not crash: §17's rule, applied to methods. The C refuses it
    everywhere else, never the binding. The batch methods, which `Corr`
    hid, serve the 1-D flavor, which is the 2-D one at one row
    (`test_corr_equiv` holds them byte for byte). On the burst search, a
    method the continuous search has is served only where a test shows it
    does what its doc says, and refused in C otherwise:

    | method on the burst search                                                                                                 | outcome                                                                                     |
    | -------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------- |
    | `push`, `configure_search_raw`, `set_max_peaks`                                                                            | served (`BurstAcquisition` had them)                                                        |
    | `set_threads`                                                                                                              | served: `test_correlator_regrid_failure.c` runs it on burst engines                         |
    | `surface_complex`                                                                                                          | served: `test_correlator_search.c`'s map-equivalence check reads it on burst engines        |
    | `set_carrier_freq_hz`, `set_telemetry`, `surface`, `surface_doppler_hz`, `surface_chip_phase`, `block_prompt`, `block_raw` | refused: no caller and no test; the carrier feeds a chip arithmetic a sample template lacks |

- **A property is served by the flavors whose class had it on main**,
    through a new C getter that returns `DP_ERR_UNSUPPORTED` elsewhere
    (`getter_status`, raising `AttributeError` that names the flavors).
    Two extensions follow the method rule: the 1-D flavor serves what the
    2-D one does, and a property that reads a method's setting (`threads`,
    `carrier_freq_hz`, `keep_surface`, `surface_rows`, `surface_at`) is
    served wherever that method is.

<!-- section-18-table: test_correlator.py reads these two tables; every
flavor-only member must be a row, and each row is checked against the code
on every flavor. -->

| properties                                                                                                                                                                                                          | served by                  |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------- |
| `flavor` (new)                                                                                                                                                                                                      | every flavor               |
| `ny`, `nx`, `dwell`, `count`                                                                                                                                                                                        | the maps and the detectors |
| `ny_out`                                                                                                                                                                                                            | 1-D, 2-D, delay-Doppler    |
| `nx_out`, `col_out`                                                                                                                                                                                                 | 1-D, 2-D                   |
| `doppler_step`                                                                                                                                                                                                      | delay-Doppler              |
| `n`                                                                                                                                                                                                                 | 1-D, both detectors        |
| `last_corr`                                                                                                                                                                                                         | both detectors             |
| `ring_cap`, `noise_lo`, `noise_hi`, `threshold`                                                                                                                                                                     | the detectors and searches |
| `max_peaks`, `code_bins`, `doppler_bins`, `n_noncoh`, `eta`, `eta_nc`, `pfa_cell`, `pd_predicted`, `straddle_loss`, `sample_rate`, `cn0_dbhz`, `doppler_span_hz`, `doppler_res_hz`, `pd`, `underpowered`, `threads` | both searches              |
| `lag_bins`, `coherent_bins`, `sf`, `chip_rate`, `symbol_rate`, `epochs_per_symbol`, `n_peaks`, `n_held`, `peak_conc`, `carrier_freq_hz`, `keep_surface`, `surface_rows`, `surface_at`                               | the continuous search      |
| `reps`, `pd_burst`, `psl_db`, `doppler_rate`, `rate_hypotheses`                                                                                                                                                     | the burst search           |

| methods                                                                                                                                  | served by                  |
| ---------------------------------------------------------------------------------------------------------------------------------------- | -------------------------- |
| `set_max_peaks`, `configure_search_raw`, `set_threads`, `surface_complex`                                                                | both searches              |
| `set_carrier_freq_hz`, `set_telemetry`, `surface`, `surface_doppler_hz`, `surface_chip_phase`, `block_prompt`, `block_raw`               | the continuous search      |
| `execute`, `execute_all`, `execute_coherent`, `execute_power_sum`, `execute_power_max`, `execute_peaks`, `bank_power_max`, `bank_detect` | the maps                   |
| `push`                                                                                                                                   | the detectors and searches |

**A method names its refusal** (just-makeit#2574, `why` on methods;
#2961). Each C function below takes a trailing `const char **why` and,
refusing a value, writes one sentence that names the argument; the binding
raises it verbatim. Off the flavors that serve the call it writes nothing,
and the binding raises the method's declared "only" sentence instead, so
each off-flavor sentence has one home (the manifest) and the mutator
gate's reading of a flavor refusal does not move. C callers pass NULL when
they do not want the sentence. An `out=` too small is the binding's own
refusal in Python; the C sentence for it is a C caller's.

| method                                                                                       | refusal (C code)          | sentence                                                                                                                                                                                                                                                                                                                                                                                                                        |
| -------------------------------------------------------------------------------------------- | ------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `set_telemetry`                                                                              | prefix (`INVALID`)        | `set_telemetry: the prefix is too long: prefix + "." + each probe name must fit DP_TLM_NAME_MAX bytes`                                                                                                                                                                                                                                                                                                                          |
|                                                                                              | decim (`INVALID`)         | `set_telemetry: decim must be at least 1`                                                                                                                                                                                                                                                                                                                                                                                       |
|                                                                                              | room (`INVALID`)          | `set_telemetry: the context has no room for this search's ten probes`                                                                                                                                                                                                                                                                                                                                                           |
| `configure_search_raw`                                                                       | `INVALID`                 | `configure_search_raw: doppler_bins must lie in 1..reps` / `n_noncoh must lie in 1..DP_CORRELATOR_N_NONCOH_SAFETY_CEILING` / `non-coherent looks need rate_hypotheses 1`                                                                                                                                                                                                                                                        |
| `set_max_peaks`                                                                              | `INVALID`                 | `set_max_peaks: n must lie in 1..64 (DP_CORRELATOR_MAX_PEAKS)`                                                                                                                                                                                                                                                                                                                                                                  |
| `execute`                                                                                    | `SIZE_MAX`                | off a map: its "only" sentence; else `execute: takes exactly one frame: the input length must be the frame length n (ny*nx)`                                                                                                                                                                                                                                                                                                    |
| `execute_all`, `execute_coherent`, `execute_power_sum`, `execute_power_max`, `execute_peaks` | `SIZE_MAX`                | off a map: the "only" sentence; else `<name>: the input must be a whole, nonzero number of frames` / `<name>: out= must hold …` (each its own) / `execute_power_max: at most 2**32 frames`                                                                                                                                                                                                                                      |
| `bank_power_max`, `bank_detect`                                                              | `SIZE_MAX`, and `*status` | off a map (`UNSUPPORTED`): the "only" sentence; else (`INVALID`) one each for one frame, `L`/`L_d >= 1`, `L_r` a positive multiple of `L_d`, `g_min <= g_max`, rates a whole number of ny-long vectors, the uint32 hypothesis index, a refined carrier past int32, `out=`, and a scratch (the bank's, or the refine's) too large to size; (`MEMORY`, `MemoryError`) a scratch, or the detector's work, that cannot be allocated |

`push` has one cause (a map) and `set_carrier_freq_hz` one per code;
neither takes a `why`. `set_telemetry` checks `decim` before its dry run,
so "decim" and "room" are told apart. The two banks also write their
cause to an `int *status` ahead of `why` (just-makeit#2584's
`status_out`), so memory that cannot be allocated raises `MemoryError`
with its sentence and every other refusal `ValueError`. Two
causes have no test, and neither can be reached: more than 2^32 frames to
`execute_power_max` (the input alone would be terabytes), and a scratch
too large to size from the rates or from `L_r` (on a 64-bit `size_t`, the
rates array and the int32 bound on a refined carrier both stop short of
it).

`reset` is one method with one doc that says what it clears for each
flavor; every flavor had one.

**Two names change, so each quantity has one.** The searches' `fs`
property is `sample_rate`, the constructor's knob. The maps' `n_out`
property goes: on the 1-D flavor it was `nx_out`, the one padding name
(§17), and on every map it is `execute_max_out()`.

**The C refuses what Python can now reach.** Until now each view excluded
the members its flavor does not serve, so some C entry points never
checked the flavor. One class can call any member on any flavor, so the
C must refuse first, measured on main (84049024d):

- `configure_search_raw`, `set_threads`, `set_carrier_freq_hz`,
    `set_telemetry`, the four `surface*`, `block_prompt`, `block_raw` and
    `psl_db` dereference `search`, which is NULL off a search: each returns
    its refusal (`DP_ERR_UNSUPPORTED`, or 0 / NaN for a count / value) first.
    `DP_ERR_UNSUPPORTED` (-11) means the flavor does not serve the call;
    `DP_ERR_INVALID` keeps meaning a bad argument to one it does serve, so
    the binding can say which happened.
    The continuous-only ones also refuse the burst search, per the table.
- `set_max_peaks` off a search allocates the peak list and never frees it
    (the map's `destroy` does not own it): it refuses.
- `execute_all_max_out` and `execute_peaks_max_out` divide by the frame
    length, 0 on a search: they refuse (`SIZE_MAX`) off a map.
    `execute_max_out` returns a detector's `n_out` although `execute`
    refuses it: it returns 0 off a map.
- Each property in the table above has a C getter that returns
    `DP_ERR_UNSUPPORTED` off its flavors (jm's `getter_status`). Two need
    more than a plain getter: `keep_surface` is writable, so its read
    names its own exception (`getter_error`), and `last_corr` is a buffer
    view, asked through `dp_correlator_last_corr_status()` before the view
    is built (both just-makeit#2543).
- The six read-outs (`surface*`, `block_*`) return `SIZE_MAX` off their
    flavors, which the binding raises as `ValueError` (`error_sentinel`,
    just-makeit#2559); 0 stays "nothing yet".
- `dp_correlator_set_surface_sink()` (C only) wrote through `search` on
    any flavor: it returns `DP_ERR_UNSUPPORTED` off a search. Both searches
    keep it, since `test_search_equiv.c` records every burst dwell through
    it.

Each guard has a C test that calls it on a flavor it refuses, where the
call would otherwise succeed (`test_correlator_create.c`), and each was
proved by sabotage: all 32 of the harness's sabotages (the guards,
`push_max_out` and `keep_surface`'s blob) turn it red, and so does
replacing any one of the 18 flavor-level knob sentences with a generic
one (`test_a_flavor_names_the_knob_it_refuses`). The Python face of
the refusals is `test_correlator.py`, red when a read-out's
sentinel or `last_corr`'s status is dropped from the manifest.

**`push` sizes its record array from `push_max_out(n_in)`** (§5), which C
does not have yet: the frames the input completes, counting the carry,
times the hits one frame may make (1 for a detector, `max_peaks` for a
search). It is a bound, not a count; the binding trims to what `push`
wrote. The test pushes every chunk length from 1 to three frames with a
result array exactly that long: push takes the whole chunk and never
reports more, for a detector and for a search listing three peaks a
dwell.

**One under-powered warning** for both searches, on `underpowered`, whose
meaning is each search's own (`pd_predicted < pd` continuous, `pd_burst < pd` burst), with one message that names both remedies.
