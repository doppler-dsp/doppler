---
description: "the one correlator: seven flavors of one C object and one Python class, Correlator; its constructor and knobs, who serves each member, push's record, the state blob, the map (per-row, 2-D, padded inverse, delay-Doppler, batches), detection, and its cost and parallelism"
---

# The correlator

One C object, `native/{inc/doppler,src}/correlator/`, and one Python class,
`doppler.correlation.Correlator`, compute one thing: correlate input against
a reference by FFT, then perhaps detect. Its seven flavors are one state
type with one constructor each; the Python class takes every knob and the C
picks the flavor from them.

The search's mathematics (the epoch matrix, the tiles, the gate, the
surface) is [the acquisition surface](acq-surface.md). How to use it is
[the correlator guide](../guide/correlator.md). §11 is how it came to be one
object.

## Vocabulary

A term names its own axis or role; a bare general word (block, look,
tile, bin, row, cell, surface, span) is not a term. A name in backticks is
today's C field; `py:` gives the Python name where it differs. Numbers in
brackets are §9's UNO Q case. Entries marked *(not yet reviewed)* are a
draft, and the rest of this page keeps its older words until they are
ruled.

**Still to rule:**

1. **Doppler bin: native or interpolated spacing?** Below it is the native
    spacing, with interpolation as a factor.
1. **Field renames.** Each term maps to today's field. The renames (the
    last list here) are an API change for their own issue; the new names
    are not settled.
1. The entries from "Losses" down.

### The signal

- **burst**: a waveform type: short and time-limited, as opposed to
    continuous. Not the thing a detector finds.
- **preamble** *(not yet reviewed)*: the part of a burst or continuous
    waveform ahead of the data; it may hold several elements. Not a
    correlator term.
- **acquisition sequence**: the element of a preamble the detector
    searches for: R consecutive repetitions of the template \[64
    repetitions; 32,768 samples; 50 ms\]. **Repetition count**, R (`reps`;
    C documents it as the coherence ceiling, and `pd_burst` assumes the
    sequence is exactly R epochs).
- **template**: one period of the known waveform, as handed to the
    constructor (`tmpl` on the burst door; chips `code` on the continuous
    door; `py: ref`). Not always a code.
- **template length**, N: its length in samples at the search rate
    (`code_bins`) [512].
- **epoch**: one template period: N samples at the search rate, nominally.
    The engine processes the stream in N-sample epochs; the received
    signal's epochs are offset from them by the delay and dilated by
    Doppler. One full template, received at an unknown time offset and
    possibly dilated, is one epoch of the signal.
- **epoch rate**: `fs`/N [1,280 Hz]. Also the Doppler range the slow-time
    transform covers on its own.
- **Doppler** *(not yet reviewed)*: the signal's carrier frequency offset,
    Hz; it varies over the acquisition sequence. **Reported Doppler**: the
    value a hit carries, the offset at the middle of the coherent dwell it
    was decided on.
- **Doppler rate** *(not yet reviewed)*: the Doppler's derivative, Hz/s
    (`doppler_rate`).
- **Doppler uncertainty**, ±U *(not yet reviewed)*: the half-range of the
    Doppler the caller asks the search to cover (`doppler_uncertainty`)
    [±50 kHz].
- **delay** (τ) *(not yet reviewed)*: the offset between the signal's
    epoch boundaries and the engine's; continuous, 0 to N samples.
    **Sub-sample delay**: its fractional part only.
- **dilation** *(not yet reviewed)*: the received epoch's departure from N
    samples [about 23 ppm at ±50 kHz on S-band].

### The engine's grid

- **sample** *(not yet reviewed)*: one complex sample at the search rate
    `fs` (`py: sample_rate`), after any down-conversion [655.36 kSa/s].
    Never the capture rate [5 MSa/s].
- **dwell**: an integration interval. It can span many hypotheses, and it
    always takes a qualifier:
    - **coherent dwell**: D epochs summed coherently, D·N samples. D
        (`coherent_bins`) is its length in epochs. The map flavors' `dwell`
        parameter is the same thing counted in frames.
    - **non-coherent count**, K (`n_noncoh`): how many coherent dwells have
        their power summed.
    - **total dwell**: K coherent dwells, K·D epochs. One threshold
        decision is made per total dwell. It equals the coherent dwell when
        K = 1. **Total integration time**: its duration in seconds \[64 × 1:
        50 ms; 32 × 2: 25 ms coherent, 50 ms total\].
- **Doppler bin**: the spacing of the Doppler grid, `fs`/(N·D)
    (`doppler_res_hz`), whether the grid comes from rolling, from the
    slow-time transform, or both [20 Hz at D = 64]. **Doppler bin count**:
    W·D (`py: doppler_bins`). See "Still to rule", item 1.
- **Doppler interpolation** *(not yet reviewed)*: the factor by which the
    engine evaluates the Doppler axis more finely than one bin (`interp`;
    2 when D > 1). A factor, not a second kind of bin.
- **Doppler roll**: one frequency shift of the search by a multiple of the
    epoch rate. **Roll count**, W (`window_bins`): how many shifts the
    search makes; W = 1 is no rolling. Each roll contributes D Doppler
    bins [79 rolls for ±50 kHz as the door sizes it today].
- **lag** *(not yet reviewed)*: one point of the delay axis; **lag bin
    count** (`lag_bins`). A hit's `col` is a delay of col·N/`lag_bins`
    samples.
- **rate hypothesis** *(not yet reviewed)*: one trial Doppler rate the
    coherent dwell is de-chirped by (`rate_hyps`; `py: rate_hypotheses`).
- **delay-Doppler cell** *(not yet reviewed)*: one (Doppler bin at one
    interpolation point, lag) under one rate hypothesis.
- **delay-Doppler surface** *(not yet reviewed)*: the cells of one total
    dwell. The stored surface holds the winning rate hypothesis's cells
    only.
- **total-dwell offset** *(not yet reviewed)*: where a sequence of total
    dwells starts in the stream (proposed in
    [#3054](https://github.com/doppler-dsp/doppler/issues/3054)). P offsets
    are spaced one total dwell / P apart. **Overlapped dwells**: total
    dwells from more than one offset.
- **alignment** (s₀) *(not yet reviewed)*: where the acquisition sequence
    starts inside a total dwell; continuous. **Aligned**: s₀ = 0.

### Losses *(not yet reviewed)*

- **straddle**: loss from a signal parameter falling between grid points,
    and nothing else: **Doppler straddle** between Doppler grid points,
    **delay straddle** between lags. `straddle_loss` is the model's mean
    amplitude factor for these plus rotation within an epoch (dB =
    20·log10); diagnostic only.
- **alignment loss**: loss from the acquisition sequence being split
    across total dwells. Not a straddle. No field; it exists only inside
    `pd_burst`'s model.

### Statistics *(not yet reviewed)*

- **hit**: one listed peak of a total dwell whose maximum cleared the
    threshold (`dp_correlator_hit_t`); a total dwell can list several.
- **detection**: at least one hit from the total dwells the acquisition
    sequence touches.
- **`pfa`** (the constructor's): the probability that one noise-only total
    dwell lists any hit. Per total dwell: not per cell, not per second (C:
    "target system (max-of-N) false-alarm probability").
- **`pfa_cell`**: `pfa` split (Šidák) over the cells the total dwell's
    maximum ranges over: in-band Doppler grid points × `lag_bins` × rate
    hypotheses.
- **false-alarm rate**: hits per unit time on noise: `pfa` / total
    integration time, times the number of total-dwell offsets. Always
    carries a time unit \[1.4e-5 a 50 ms total dwell is about one an hour
    at one offset\]. No field computes it.
- **acquisition Pd**: the probability an acquisition sequence is detected,
    at a stated alignment prior. "Pd" unqualified means this.
- **`pd_predicted`**: the model's Pd of one total dwell lying wholly
    inside the signal. Never called "Pd" for an acquisition sequence.
- **`pd_burst`**: the model's acquisition Pd at uniform alignment; NaN
    when K > 1.
- **measured Pd**: the fraction of trial acquisition sequences detected.
- **underpowered**: the engine's flag: `pd_burst` < `pd` on a burst
    search where `pd_burst` is defined, else `pd_predicted` < `pd`; never
    set without a design C/N0.
- **emulated**: a harness arranging the engine to imitate something it does
    not do [re-pushing the stream at P offsets].

### Cost *(not yet reviewed)*

- **ns a sample**: wall-clock ns per search-rate sample, one thread unless
    a thread count is given.
- **core time**: ns a sample times the threads in use.
- **real time**: 1/`fs` per sample per core [1,526 ns].
- **budget**: real time times the cores available, minus what the front
    end takes.

### Retired words

- **block, depth** → coherent dwell (and D, its length in epochs).
- **look** → non-coherent count; **detection dwell**, bare **dwell** →
    total dwell.
- **tile, Doppler window, span, native span** → Doppler roll; the width is
    the epoch rate.
- **bin, row** unqualified → Doppler bin; Doppler interpolation.
- **cell, surface** unqualified → delay-Doppler cell, delay-Doppler
    surface.
- **preamble** for the thing detected → acquisition sequence; **burst** for
    it likewise.
- **repetition** as a second word for the interval → epoch (it survives
    only in "repetition count").
- **sliding**, **phase** for total-dwell offsets: phase means carrier
    phase only. **Code phase** for lag in prose about non-code templates.
- **dwell straddle** → alignment loss.

### Fields carrying retired words (the rename list)

`coherent_bins` (D), `n_noncoh` (K), `window_bins` (W), `code_bins` (N),
`doppler_span_hz` (half the epoch rate), `pd_burst`, and
`dp_correlator_create_burst` / `framing="burst"` themselves, if "burst" is
to mean only the waveform type. Also `doppler_bins`, which is W·D as a
property but D, the coherent dwell, as `configure_search_raw`'s argument.

## 1. Who calls it

| caller                                                   | flavor                     | does with the answer                                 |
| -------------------------------------------------------- | -------------------------- | ---------------------------------------------------- |
| a user correlating a block against a template            | 1-D, 2-D                   | maps: one per call, or a batch reduced in C          |
| a user searching a code over delay × Doppler             | delay-Doppler              | maps, a hypothesis bank, the peak per map            |
| a user detecting a reference in a stream                 | 1-D or 2-D detector        | detections from `push`, the gate a multiple of noise |
| a user acquiring a DSSS signal or a burst                | continuous or burst search | detections from `push`, the gate from Pfa and Pd     |
| `CarrierAcquisition` (C)                                 | 1-D detector               | a sub-bin carrier estimate                           |
| `BurstCapture` (C)                                       | burst search               | resolves the period, refines, emits a burst          |
| `DsssReceiver`, `AsyncDsssReceiver`, `AsyncDsssPool` (C) | continuous search          | hands the hit to the tracking loops                  |
| `doppler.dsss.orchestrator` (Python)                     | burst search               | one search per channel                               |

Nothing else in `native/` correlates by FFT: `ppe_core.c` is a periodogram.

## 2. Seven flavors, one state

| `framing`    | `ref`     | `doppler_rows` | `detect` | flavor (`DP_CORRELATOR_*`) | C constructor                        |
| ------------ | --------- | -------------- | -------- | -------------------------- | ------------------------------------ |
| `none`       | 1-D       | 0              | no       | `1D`                       | `dp_correlator_create_1d`            |
| `none`       | 2-D       | 0              | no       | `2D`                       | `dp_correlator_create_2d`            |
| `none`       | 1-D       | ny > 0         | no       | `DELAY_DOPPLER`            | `dp_correlator_create_delay_doppler` |
| `none`       | 1-D       | 0              | yes      | `DETECTOR_1D`              | `dp_correlator_create_detector_1d`   |
| `none`       | 2-D       | 0              | yes      | `DETECTOR_2D`              | `dp_correlator_create_detector_2d`   |
| `continuous` | 1-D chips | -              | implied  | `SEARCH_CONTINUOUS`        | `dp_correlator_create_continuous`    |
| `burst`      | 1-D       | -              | implied  | `SEARCH_BURST`             | `dp_correlator_create_burst`         |

The 1-D flavors are the 2-D ones at one row: the same plans, products, pad
and scale, held byte for byte by `test_corr_equiv.c` and
`test_detector_equiv.c`.

**One state type, `dp_correlator_state_t`, in layers.** Each flavor uses the
layers it needs, and none exists twice:

| layer         | holds                                                                                                         |
| ------------- | ------------------------------------------------------------------------------------------------------------- |
| **map**       | the reference spectrum and the transforms: the 2-D shape, the single-row shape and the delay-Doppler map (§7) |
| **dwell**     | the coherent accumulator and its count; the non-coherent sum                                                  |
| **stream**    | the ring and framer; a search's block-coherent epochs held raw                                                |
| **detection** | magnitude, noise estimate, peak list, the gate, twins, the decision (§8)                                      |
| **search**    | the search record: sizing, the grid, rates, the carrier, the sidelobe cache, Pfa and Pd, telemetry, hand-off  |

The search record is embedded in the state below every field the other
flavors' benches measure, and `search` points at it on a search flavor and
is NULL elsewhere. Every flavor carries it.

**The sources.** `correlator_core.c` holds every flavor;
`correlator_map.h` is the delay-Doppler map's stages and schedule rule;
`correlator_private.h` is the detection layer's helpers (the ring, the
noise estimate, the peak list, the framed push).

**One hit record**, `dp_correlator_hit_t`, 64 bytes, for every flavor that
detects: `row`, `col`, `peak_mag`, `noise_est`, `test_stat`,
`cn0_dbhz_est`, `samples_consumed`, `chip_phase`, `doppler_rate_hz_s`,
`test_stat_peak`. A detector writes the search-only fields as 0, and at one
row `row` is 0.

## 3. One constructor

A jm object has one constructor, so "one class whose knobs are its
arguments" is one C entry point, `dp_correlator_create`, that takes every
knob and calls the flavor's own constructor (§2's table). The dispatch is in
C, once. C callers name a flavor and call its constructor; the dispatcher's
doc in `correlator_core.h` says it is the binding's entry point.

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

`ref` is 1-D or 2-D (`rank = [1, 2]`); a 1-D array arrives as one row. The
continuous search's chips arrive as cf32 NRZ, and the dispatcher refuses
any chip that is not exactly +1 or -1, so a shaped or noisy reference
cannot quantize silently; `DsssReceiver`'s C path calls the `uint8`
constructor directly.

**A value the flavor would not honor is refused**, never ignored: a
combination outside §2's table, a knob the flavor does not read set away
from its default, or a value the flavor itself refuses (`dwell = 0`,
`pfa = 1`, a fractional code period). The dispatcher writes one sentence
naming the knob through its trailing `why`, and the binding raises it as
`ValueError` (`create_why`, just-makeit#2552; C callers pass NULL).

**The defaults, and why each:**

| knob                           | default                  | why                                                                                                                                                     |
| ------------------------------ | ------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| a one-row `ref`                | the 1-D flavor           | C cannot tell `ref` from `ref[None, :]`; `nx_out` pads it, and `ny_out` and `col_out` are refused at one row                                            |
| `noise_hi`                     | the `SIZE_MAX` sentinel  | clamped to the last bin; `0` would make the floor one bin                                                                                               |
| `reps`                         | 1                        |                                                                                                                                                         |
| `sample_rate`, `chip_rate`     | 0.0, not given           | **a rate is never a default:** a search sized by a wrong one detects at the wrong Doppler without a word, so a search refuses a rate it reads that is 0 |
| `symbol_rate`                  | 0.0, no known data clock | the continuous search reads it only for a diagnostic (`epochs_per_symbol`), never to size, so its 0 is honored                                          |
| `cn0_dbhz`                     | NaN, no design point     | the burst search honors it; the continuous search sizes its dwell from a finite design point and refuses NaN                                            |
| `detect` on a search framing   | accepted                 | a search always detects, so `True` is honored                                                                                                           |
| `nthreads` on a search framing | refused away from 1      | a search's pool is `set_threads()`'s; this knob's 1 would mean another request                                                                          |

Normalized units are one stated argument away, `sample_rate=1.0`.

One warning for both searches, on `underpowered`, whose meaning is each
search's own (`pd_predicted < pd` continuous; `pd_burst < pd` burst, or
`pd_predicted < pd` where `pd_burst` is NaN), with
one message that names both remedies.

## 4. The members, and who serves each

One class can call any member on any flavor. Which flavors serve each is
decided in one place for methods and one for properties, both in C; the
binding adds no refusal of its own.

- **A method is served where the flavor honors it**, not merely where it
    does not crash; the C refuses it everywhere else. The batch methods
    serve the 1-D flavor, which is the 2-D one at one row. On the burst
    search a continuous-search method is served only where a test shows it
    does what its doc says: `set_threads` (`test_correlator_regrid_failure.c`)
    and `surface_complex` (`test_correlator_search.c`'s map-equivalence
    check). The carrier, telemetry and the other read-outs are refused there:
    the carrier feeds a chip arithmetic a sample template lacks.
- **A property is served by the flavors in the table below**, through a C
    getter that returns `DP_ERR_UNSUPPORTED` elsewhere (jm's
    `getter_status`), which raises `AttributeError` naming the flavors.
    `DP_ERR_INVALID` keeps meaning a bad argument to a flavor that serves
    the call, so the binding can say which happened. A property that reads
    a method's setting (`threads`, `carrier_freq_hz`, `keep_surface`,
    `surface_rows`, `surface_at`) is served wherever that method is.
- **`flavor`** (a string) says which flavor the knobs built, so a caller can
    ask before reading a member instead of catching the error.

<!-- section-4-table: test_correlator.py reads these two tables; every
flavor-only member must be a row, and each row is checked against the code
on every flavor. -->

| properties                                                                                                                                                                                                          | served by                  |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------- |
| `flavor`                                                                                                                                                                                                            | every flavor               |
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
| `set_max_peaks`, `set_peak_zone`, `configure_search_raw`, `set_threads`, `surface_complex`                                               | both searches              |
| `set_carrier_freq_hz`, `set_telemetry`, `surface`, `surface_doppler_hz`, `surface_chip_phase`, `block_prompt`, `block_raw`               | the continuous search      |
| `execute`, `execute_all`, `execute_coherent`, `execute_power_sum`, `execute_power_max`, `execute_peaks`, `bank_power_max`, `bank_detect` | the maps                   |
| `push`                                                                                                                                   | the detectors and searches |

`reset` is one method on every flavor, with one doc that says what it
clears for each.

**How a refusal reaches the caller.**

- A method that refuses a value takes a trailing `const char **why` and
    writes one sentence naming the argument; the binding raises it verbatim
    (just-makeit#2574). Off the flavors that serve it the C writes nothing,
    and the binding raises the method's declared "only" sentence, so each
    off-flavor sentence has one home, the manifest. The header holds every
    method's sentences.
- The two banks also write their cause to an `int *status` ahead of `why`
    (just-makeit#2584), so memory that cannot be allocated raises
    `MemoryError` and every other refusal `ValueError`.
- The six read-outs (`surface*`, `block_*`) return `SIZE_MAX` off their
    flavors, which the binding raises as `ValueError` (`error_sentinel`); 0
    stays "nothing yet".
- `execute_all_max_out`, `execute_peaks_max_out` and `execute_max_out`
    return 0 off a map, the same as "nothing yet"; the methods they size
    refuse there.
- `keep_surface` is writable, so its read names its own exception
    (`getter_error`); `last_corr` is a buffer view, asked through
    `dp_correlator_last_corr_status()` before the view is built.
- `dp_correlator_set_surface_sink()` (C only) returns `DP_ERR_UNSUPPORTED`
    off a search.

Each guard has a C test that calls it on a flavor it refuses
(`test_correlator_create.c`), each proven red by sabotage, and so is each
flavor-level knob sentence (`test_a_flavor_names_the_knob_it_refuses`). The
Python face of the refusals is `test_correlator.py`.

## 5. What `push` returns

One record for every flavor that detects: `dp_correlator_hit_t` (§2),
returned as a NumPy structured array (`record_dtype`) whose fields keep
their C names. Off a search flavor the search-only fields read 0.

`push` sizes the array from `push_max_out(n_in)`: the frames the input
completes, counting the carry, times the hits one frame may make (1 for a
detector, `max_peaks` for a search). It is a bound, not a count; the binding
trims to what `push` wrote. There is no stack cap, so a push never drops
input past the last hit it had room for.

## 6. State

One magic, `CORL`, and one version (`CORRELATOR_STATE_VERSION`). The flavor
is the blob's first payload field, a u64 and a reject key, so a blob from one
flavor is refused by every other; then each layer packs its own fields once.

- **Map and dwell flavors:** the accumulator and its count.
- **Detectors:** append `noise_lo`, `noise_hi` and `noise_mode` (reject
    keys: they decide what the stored statistic means), `threshold` (a
    mutator's value, #2022), the framer's child blob and the last dump's
    result.
- **Searches:** the search's extra block (the counts that are reject keys,
    and the stream position), the framer's snapshot, the non-coherent
    accumulator when `n_noncoh > 1`, the twins, a tiled block's raw epochs,
    and `carrier_freq_hz`.

`set_state` checks the whole blob, children included, before it commits
any of it. A search restores the carrier through the same rebuild
`set_carrier_freq_hz` runs (§7.5), never by writing the field, so a blob
taken at a carrier and restored into a carrier-0 engine gets the tables it
needs. That rebuild can fail to allocate, which is the one `DP_ERR_MEMORY`
a `set_state` returns (`MemoryError` in Python).

The composing objects nest the blob: `CarrierAcquisition` (`CAQR`),
`BurstCapture` (`BCAP`), `DsssReceiver` (`DSRX`), `AsyncDsssReceiver`
(`ADRX`), and through a receiver `AsyncDsssPool` and `DsssBurstReceiver`.
`make state-versions-check` holds each version against every release tag.

## 7. The map

### 7.1 Per-row and 2-D correlation

Per frame the 2-D flavor computes $S = \mathrm{FFT2}(x)$, then
$P = S \cdot \overline{\mathrm{FFT2}(\mathrm{ref})}$, then
$R = \mathrm{IFFT2}(P)/(n_y n_x)$. The forward transform stays at the
reference's size $(n_y, n_x)$, because the correlation is circular.

**The single-row fast path.** A reference with energy in row 0 only has a
row spectrum flat in the row frequency, so the row axis of the
forward-accumulate-inverse round trip is an exact identity for any input.
The 2-D correlation is then $n_y$ independent length-$n_x$ circular
correlations:

$$R(i, j) = \frac{1}{n_x}\,\mathrm{IFFT}_{n_x}\big(\mathrm{FFT}_{n_x}(x_i)\cdot\overline{\mathrm{FFT}_{n_x}(\mathrm{ref}_0)}\big)(j)$$

It is decided once, at create: the reference is nonzero only in row 0 and
`ny_out == ny` (a row-axis pad needs the row axis's content). Its plans are
length-$n_x$ and length-$n_x^{out}$ 1-D plans and a length-$n_x$ reference
spectrum. `dp_correlator_set_ref` refuses a multi-row reference on a
fast-path object rather than truncating it. The accumulator keeps its
$(n_y, n_x)$ layout either way, so the blob does not depend on the path.
The row stage fans across the pool; the general 2-D path runs serially.

**The dwell sums spectra.** A coherent dwell accumulates $P$ over `dwell`
frames and inverts once at the dump, since the inverse is linear. It differs
from inverting every frame only by accumulation-order rounding (about 1e-5
relative in cf32). A non-coherent sum of magnitudes is not linear and
inverts every frame.

### 7.2 The padded inverse and interpolation

The inverse length is free. Zero-padding the product $P$ to
$(n_y^{out}, n_x^{out}) \ge (n_y, n_x)$ and inverting there is the
band-limited (Dirichlet) interpolation of the same circular correlation:
the peak and its value kept, on a finer grid.

$$\mathrm{out} = \mathrm{IFFT2}_{n_y^{out}, n_x^{out}}\big(\mathrm{zeropad}(P)\big) / (n_y n_x)$$

- **The scale is the native $1/(n_y n_x)$,** not the padded one, so an
    interpolated peak equals the native peak.
- **The pad** keeps each axis's low half $[0, n/2]$, inserts
    $n^{out} - n$ zeros at the high frequencies, then the high half. For an
    even $n$ it splits the Nyquist bin, half on each side, which minimum-error
    interpolation needs.
- **Pick a fast inverse size** (a multiple of 16 whose only factors are 2, 3
    and 5), or the pad only makes a bigger slow transform. The forward stays
    at the native size.
- **Index:** a peak at $(r, c)$ on the padded grid is Doppler
    $r\, n_y / n_y^{out}$ and code phase $c\, n_x / n_x^{out}$.
- `ny_out < ny` or `nx_out < nx` is refused: downsizing is not
    interpolation. `col_out >= 0` emits one lag a row instead of the map.

### 7.3 The delay-Doppler flavor

`Correlator(code, doppler_rows=ny)` takes the raw stream, `ny` code periods
back to back, and returns the delay-Doppler map: §7.1's per-row correlation,
then on the dump the `ny_out`-point FFT down every delay column, centered.

```text
map = fftshift(fft(ifft(fft(x.reshape(ny, nx), axis=1) * conj(fft(code)),
                        axis=1), n=ny_out, axis=0), axes=0)
```

- **Rows are Doppler bins,** zero Doppler on row `ny_out // 2`; row $r$ is
    $(r - n_y^{out}/2)\cdot$`doppler_step` cycles a sample, with
    `doppler_step` $= 1/(n_y^{out} n_x)$. Columns are lags.
- **The row inverse carries the only $1/n_x$;** the slow-time FFT is
    unnormalized, as numpy's is. A unit-amplitude signal against a
    unit-modulus code peaks at `dwell * ny * nx`.
- **`ny_out > ny`** pads the slow-time samples after the `ny` rows, sampling
    the same response on a finer grid; a peak at a bin center keeps its
    height.
- **Columns are a stride of `nx` apart,** so the column stage gathers 16
    neighboring columns a row into contiguous scratch (a tiled transpose).

A streaming caller hands the same preallocated map to every dump:

```python
import numpy as np
from doppler.correlation import Correlator

ny, nx, delay, bins = 32, 256, 41, -4
rng = np.random.default_rng(7)
code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
dd = Correlator(code, doppler_rows=ny, dwell=2, ny_out=4 * ny)
f = bins * dd.doppler_step * 4  # -4 native bins, in cycles/sample
n = np.arange(ny * nx)
x = (np.tile(np.roll(code, delay), ny) * np.exp(2j * np.pi * f * n)).astype(
    np.complex64
)

out = np.empty(dd.execute_max_out(), np.complex64)  # (ny_out * nx,)
dumps = 0
for frame in range(6):  # three dwells
    if dd.execute(x, out=out) is not None:  # writes `out` on a dump
        dumps += 1
        m = out.reshape(dd.ny_out, nx)
        row, col = np.unravel_index(np.argmax(np.abs(m)), m.shape)
        assert (int(col), int(row) - dd.ny_out // 2) == (delay, 4 * bins)
assert dumps == 3
```

### 7.4 Batches

A batch takes H frames in one call and returns every map, or one reduction
of them, from one C loop.

| method              | returns                                        | reduces                                                                  |
| ------------------- | ---------------------------------------------- | ------------------------------------------------------------------------ |
| `execute_all`       | H × n_out complex                              | nothing: map h at row h                                                  |
| `execute_coherent`  | n_out complex                                  | the product spectra summed, inverted once: a fresh dwell-H object's dump |
| `execute_power_sum` | n_out float                                    | $\sum_h \lvert m_h \rvert^2$, cell by cell                               |
| `execute_power_max` | n_out float, n_out uint32                      | $\max_h \lvert m_h \rvert^2$ and that h; a tie keeps the lowest h        |
| `execute_peaks`     | H records: peak power, row, column, mean power | each map to its own peak and mean                                        |

`execute_peaks` reports each map's own statistics because the peak-to-mean
of the pointwise maximum over maps is a different number, and the wrong one
to rank hypotheses by. `bank_power_max` and `bank_detect` run a hypothesis
bank (carriers and rates) of one frame through the same loop.

**What a batch promises:**

- **Map h is a fresh dwell-1 object's map for frame h,** bit for bit, on
    every path and at every thread count: `execute()` and the batch run the
    same row, tile and frame helpers.
- **The dwell is untouched.** A batch never reads or writes the accumulator,
    its count or the blob, so it may run mid-dwell.
- **Every reduction folds in h order,** so a float sum is the same at every
    thread count. The power stays in double, so a cell's power is one value
    on every compiler, which the tie rule needs. The max and peak folds are
    selects with unconditional stores, which vectorize.
- **A refused batch writes nothing:** an empty batch, a length that is not
    whole frames, or an output too small. A batch never truncates, because a
    truncated reduction has no meaning.
- **The fixed reducers are the Python contract.** C callers also get
    `dp_correlator_execute_fold` and `bank_fold`, which take a C reducer
    (`dp_correlator_fold_t`); a Python callable there would be a callback
    in the inner loop. A statistic the fixed reducers lack is `execute_all`
    and a NumPy reduction, or a reducer in C.

**Threads.** Two frames or more: each worker makes whole maps on its own
plans and scratch, folded in h order. One frame: split as `execute()` splits
it. The coherent sum: each worker owns a fixed range of rows through every
frame. A short frame split across a pool costs more than it saves, which is
why whole maps go to workers.

A carrier-offset search as one batch is
`native/examples/delay_doppler_batch_demo.c`, and its Python twin
`src/doppler/examples/delay_doppler_batch_demo.py`.

### 7.5 The search's block map

The two searches run one block-coherent delay-Doppler map a block of $D$
epochs: the forward transform of each epoch, slow time over chunks of
capture bins, then the rows over (tile, slow row) with each tile's
code-rate ramp. Its definition, its two schedules and its one rule
(`dp__dd_schedule`), and the carrier's rebuild are
[the acquisition surface §18](acq-surface.md#18-one-delay-doppler-map).
`DelayDoppler`'s map and the hypothesis bank run on the same stages.

## 8. Detection

- **The peak list** (`det_peak_list`) is the one peak picker for every
    detecting flavor: one peak for a detector, up to `max_peaks` for a
    search, zoned by span. On equal cells it keeps the lowest index.
- **The peak zone.** By default a search's listed peak masks one native
    Doppler row by the template's mainlobe across the lags, so one strong
    emitter can fill the list with its own lag: its slow-time sidelobes,
    its alias in the neighbouring rolls, and inside a coherent dwell its
    data's spread. A weaker emitter elsewhere is then never listed
    ([#3067](https://github.com/doppler-dsp/doppler/issues/3067)).
    `set_peak_zone(all_doppler, half_chips)` widens the zone to every
    Doppler bin of every roll and sets its half-width in chips: the most
    whole lag columns within it. The `AsyncDsssReceiver` asks for
    `(1, DP_CORRELATOR_SAME_PHASE_CHIPS / 2)`, half the width its pick
    treats as one signal: with a carrier set the pick's key moves each hit
    off the lag grid by its own Doppler's dilation, so only a zone inside
    the key never masks a pair the key keeps. Its searcher lists about one
    peak an emitter. A caller who never sets it gets the default list. The
    two-epoch twin rule keeps the mainlobe's width either way, so a peak
    past a narrower zone but inside the mainlobe is still held as a twin.
- **NaN is never a peak.** The scan's seed skips NaN cells, so a NaN in
    cell 0 does not hide a finite peak elsewhere, and masked and unmasked
    scans pick alike. At `threshold = 0` an all-NaN frame reports
    `(0, 0, 0, NaN, 0)`; at a positive threshold a NaN statistic fails the
    gate. With the `mean` noise estimate, a NaN in the noise window makes
    `noise_est` NaN and `test_stat` 0; `min` and `max` skip a NaN anywhere
    but the window's first bin, and `median`'s order with a NaN is not
    defined. The C pin covers an all-NaN frame only.
- **The noise estimate** aggregates the bins in `[noise_lo, noise_hi]` by
    `noise_mode` (`mean`, `median`, `min`, `max`); the test statistic is the
    peak over it. A search uses the mode for its CFAR reference.
- **The gate:** a detector's threshold is a multiple of the noise; a
    search's comes from Pfa and Pd at its design point
    ([detection](detection.md): `det_threshold`, `det_pd`).
- **A burst search's total dwells are aligned to the stream, not to the
    acquisition sequence** (`search_burst_pd_at`), so the sequence starts
    anywhere in a total dwell, and there are no overlapped dwells
    ([#3054](https://github.com/doppler-dsp/doppler/issues/3054)). At a
    marginal design point that alignment loss is the largest single loss.
    On §9's UNO Q case, with a 64-epoch coherent dwell on one roll and
    Doppler uniform over ±50 kHz and rate over ±1 kHz/s, an aligned
    acquisition sequence has an acquisition Pd of 0.999 at zero delay and
    0.975 with the delay uniform over an epoch, and a randomly placed one
    0.705.
    On one roll nothing exceeds about 0.81: the sizer's 41-epoch coherent
    dwell with four rate hypotheses gets 0.807, and a 32-epoch coherent dwell with a non-coherent
    count of 2, pinned through `configure_search_raw`, 0.805.
    `pd_burst` is conservative there (0.635 against 0.705) and NaN when the
    non-coherent count exceeds 1.
- **A burst search's Doppler rolls come from the Doppler uncertainty and
    the epoch rate alone** (`search_cover_window_bins`); the template's own
    Doppler tolerance is not measured. A chirp told ±50 kHz gets 79 rolls,
    whose acquisition Pd is 0.887 on that case and which run 9× over real
    time on the UNO Q's four cores; one roll gets 0.807. One roll is
    reached only by passing `doppler_uncertainty = 0`, and then a hit's
    delay and reported Doppler are one coupled number (1.37 samples per
    1,280 Hz for that chirp) with nothing saying so
    ([#3056](https://github.com/doppler-dsp/doppler/issues/3056)).

## 9. Cost and parallelism

The search serves two applications, each with its own design point:

| application                                 | rate                                                                                                                                       | grid                                           | where it runs                                  |
| ------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------------------- | ---------------------------------------------- |
| #2457's point, where §9.1–9.4 were measured | 3.069 Mcps at 2 samples a chip, 6.138 MSa/s                                                                                                | ±3 kHz, 3 tiles, $D = 90$, $N = 2046$          | the dev and bench boxes                        |
| a burst preamble on the UNO Q               | 5 MSa/s captured, down-converted to 655.36 kSa/s                                                                                           | ±50 kHz, ±1 kHz/s, 1 tile, $D = 64$, $N = 512$ | four Cortex-A53 cores (Arduino UNO Q, QRB2210) |
| `AsyncDsssPool`                             | 5 Mcps, 11 MSa/s resampled to 2,048 a period ([async-dsss-receiver §1.2](async-dsss-receiver.md#12-the-sample-rate-is-derived-not-chosen)) | ±50 kHz, 21 tiles, $D = 150$                   | the dev and bench boxes                        |

The burst's preamble is a 512-sample linear chirp (±240 kHz) repeated 64
times, 50 ms, at a C/N0 of 32 dB-Hz
([#3054](https://github.com/doppler-dsp/doppler/issues/3054)). Its budget is real time at the
down-converted rate: 1,526 ns a sample a core, 6,104 across the four cores,
before the down-converter. Its target acquisition Pd is 0.999, at about
one false alarm an hour (that rate is an assumption, not yet the owner's).
The committed search needs 35 dB-Hz for it, and 34 with four emulated
total-dwell offsets; at 32 dB-Hz overlapped dwells reach 0.957 to 0.971
([#3054](https://github.com/doppler-dsp/doppler/issues/3054#issuecomment-6886875009)).
Unless it names another box, everything below
was measured at #2457's point on the x86 dev box, and §9.1's profile also on
a Snapdragon X2. Every number here is a run in
[the measurement record](correlator-measurements.md#9-cost-and-parallelism),
or, for the UNO Q case (here and in §8), a run recorded in the issue it
names, whose harness lives there; the record says which runs have their
harness and raw output committed beside it.

The search's signals lie below the noise before correlation gain, so
anything above the noise is interference that a front end may clip or
excise ([#3047](https://github.com/doppler-dsp/doppler/issues/3047)). That
assumption is what makes a narrower sample type worth considering at all;
§9.5 lists what it does not settle.

### 9.1 Where a sample's time goes

On one thread a sample costs about 38 ns on x86 and 25 ns on arm64, and the
FFT is 59–60% of it on both:

| stage                                                          | x86 share | arm64 share |
| -------------------------------------------------------------- | --------: | ----------: |
| the inverse of each correlation row (pffft)                    |     23.6% |       15.3% |
| the forward of each 2,046-sample epoch (pocketfft)             |     20.0% |       23.7% |
| ↳ its generic pass for the prime factor 31                     |      8.3% |       13.2% |
| slow time down each column, 180 points ($F\,D\,I$, no carrier) |     15.8% |       20.1% |
| everything else (the row gather, decide, peak scan, statistic) |      ~40% |        ~41% |

A lower-precision FFT could buy at most about 2.5× of this (Amdahl), since
about 40% is not FFT.

**The forward length is the code period.** 2,046 = 2·3·11·31 takes pocketfft's
generic passes; a 2,048-point transform is 5.2× faster. No integer samples a
chip gives the 1023-chip code a smooth period
([acq-surface §14](acq-surface.md#14-compute-cost)), so this is a front-end
sample-rate question, not the correlator's. The pool's front end answers it
by resampling to a 2,048-sample period; #2457's point does not.

**The FFT backend's fast sizes.** The `fft` object (`dp_fft_create`) takes its float SIMD path (pffft)
only for multiples of 16 whose only factors are 2, 3 and 5; every other
length runs in double through pocketfft. The search's slow-time depth is
rounded to 7-smooth (`dp_fft_smooth_floor`), which is not that set: 180
points take 0.57 µs, where 160 take 0.17 µs and 192 take 0.21 µs. The rule
that would fix it floors $D\,I$ to the backend's fast set rather than to
7-smooth, and its price is each point's own: at $D = 90$ the nearest fast
depth that keeps 2 rows a bin is 80 (0.5 dB), while the pool's $D = 150$
(300 points) has 144 (288 points) for 0.18 dB.

### 9.2 Bytes against bandwidth

On the x86 dev box the search is not bandwidth-bound. Counting each map
stage's buffers read and written once (a floor, with no cache):

| tiles | logical bytes a sample | map stage at 16 threads | triad ceiling, all threads |
| ----: | ---------------------: | ----------------------: | -------------------------: |
|     3 |                    136 |                 25 GB/s |                    77 GB/s |
|    11 |                    456 |                 28 GB/s |                    77 GB/s |

The other boxes' triad ceilings are 117 GB/s (Snapdragon X2), 128 GB/s
(GB10) and about 58 GB/s (Ryzen AI 9 465, possibly loaded when measured);
the search's own bandwidth has been measured on the x86 box only.

So a narrower type pays two different ways. On a CPU that is not
bandwidth-bound it pays by lanes: Q15 or fp16 arithmetic fits twice the
lanes of cf32 in a vector, bounded by the FFT's share (§9.1). Narrower
storage of the surface pays by bytes moved, which is the lever on a
bandwidth-bound target such as a GPU on unified memory.

### 9.3 How each stage splits across workers

Every stage of the search's map runs once a block and fans once on its
pool:

| stage                                    | split by                                                                                                                   |
| ---------------------------------------- | -------------------------------------------------------------------------------------------------------------------------- |
| forward transforms                       | epoch                                                                                                                      |
| slow time                                | chunks of capture bins                                                                                                     |
| rows                                     | (tile, slow row)                                                                                                           |
| decide (magnitude, reference, peak scan) | chunks of whole rows: each tile's rows cut equally, aiming at 24 chunks in all and at most 8 a tile; one a tile at $D = 1$ |

- **Results never depend on the thread count.** Each output row is one
    task's arithmetic, and the decide pass's chunk count is fixed by the grid,
    not the pool, so its float sums fold in one order. The surface and every
    hit are bit-identical at any thread count.
- **The pool is not capped at one worker a tile when $D > 1$,** since
    every stage fans finer than tiles. That holds at one tile too: its map
    fans by epoch, capture bin and row, and only its decide pass stays one
    chunk
    ([#3055](https://github.com/doppler-dsp/doppler/issues/3055)). At
    $D = 1$ a block is one epoch and the cap stays: there the fans run
    every epoch, and the caller's `epoch_fan` decides; one tile at $D = 1$
    runs serial at any thread count.
- **Each map stage (forward, slow time, rows) cuts 4 chunks a worker,** and a chunk claims whichever
    per-worker slot (plans and scratch) is free, so a fast worker takes more
    of a stage than a slow one.
- **Memory grows with the thread count** at $D > 1$: plans and scratch are
    per worker, not per tile. They and the workers start at the engine's
    first push, or at `set_threads()`, never at create: a pool of receivers
    sets each to one worker before any is pushed, and none builds them.

What it buys at the design point and at ±15 kHz (11 tiles), ns a sample:

| threads               |     1 |    2 |    4 |    8 |   16 |
| --------------------- | ----: | ---: | ---: | ---: | ---: |
| design point, 3 tiles |  38.7 | 21.2 | 13.2 |  8.5 |  6.5 |
| ±15 kHz, 11 tiles     | 100.2 | 54.7 | 33.7 | 22.0 | 16.0 |

Each is the best of five pushes, except the design point at 16 threads:
the best of four runs, three of them best of fifteen pushes, which spread
6.5–8.4. The map alone scales 7–9× to 16 threads.

### 9.4 Measured and not taken

| lever                                                                          | result                                                                                                                   |
| ------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------ |
| Bluestein over pffft for the 2,046-point forward                               | 0.85× today at 2,046 points, 0.39× at 90                                                                                 |
| slow-time depth 80 instead of 90 at #2457's point (§9.1's rule)                | about 10% of the search for 0.5 dB of coherent gain; not taken there. The pool's 150 → 144 is not measured               |
| pad slow time to 192 rows                                                      | breaks the rule that each native Doppler bin spans exactly `interp` rows, which the row map, zone fold and hand-off read |
| gather the row product in pffft's internal order and run the unordered inverse | bit-identical, and a tie: the gather costs what the reorder did                                                          |
| `-march=native`                                                                | 7% at one thread, 0 at 16                                                                                                |

### 9.5 Open

Each is a lever or a question of
[#3047](https://github.com/doppler-dsp/doppler/issues/3047) unless it names
its own issue.

- **Fuse the decide pass into the row stage** while each row is in cache:
    a timing prototype was 10% faster at 16 threads and no faster at one,
    where the pass is bound by `cabsf`.
- **#2457's 2,046-point forward:** batch SIMD across
    epochs, or a front end that resamples to a 2,048-point period as the
    pool's does (§9.1).
- **Zero-copy block input.** Input reaches the block through two copies
    (the framer's ring, then `blk_raw`), 13% of the 16-thread critical path.
    Removing one needs the framer to view the caller's buffer when it holds
    no carry, a framer feature for every user.
- **Batched SIMD across transforms, at full precision.** The default x86
    build is `x86-64-v2`, pffft runs 4-lane SSE, and every length outside
    pffft's set (the 2,046-point forward, the 180-point slow time) runs
    scalar in double. One kernel that runs many same-length transforms side
    by side, a lane each (epochs for the forward, columns for slow time,
    which the column stage already gathers 16 at a time), would run any
    radix in float at the full vector width. That would also remove the
    fast-size constraint behind §9.4's depth trade. It needs runtime ISA
    dispatch, and it is the shape a Q15 kernel would take. Not measured.
- **The decide pass's magnitude** calls libm `cabsf` per cell
    (`search_tile_decide`), and the non-coherent sum squares the result.
    `sqrtf(re*re + im*im)` vectorizes and the sum needs no root; both move
    results by ulps, so they are not bit-identical to today.
- **What a clip does not bound.** A time-domain clip bounds the input and
    the product, not the spectra: a clipped CW still lands in one bin, about
    $33 + 20\log_{10} k$ dB above the noise bin at $N = 2046$. So a narrow
    forward transform may need cf32 or a narrowband excision between the
    forward and the product, where the spectrum already exists. The clip
    also needs a noise level that an interferer does not drag up (a robust
    estimate rather than total power) and a clip rate a caller can check
    against the Gaussian's $2Q(k)$.
- **Precision is configuration.** Results are bit-identical at any thread
    count and a blob resumes bit for bit on another machine (§6, §9.3), so a
    narrower type must be chosen deterministically from the caller's
    request and the ISA, and recorded in the blob or refused on restore,
    never picked from a per-machine timing cache.
- **Measure on the A53.** The UNO Q burst search measured 152 ns a sample
    at one tile, $D = 64$, one thread (10% of one core), and 657 with five
    rate hypotheses
    ([#3054](https://github.com/doppler-dsp/doppler/issues/3054)); that
    harness is not in the record. #2457's point is unmeasured on the A53. The UNO Q's own bench
    ([its README](../example-projects/uno-q.md)) shows the A53 cache-bound:
    `fft` runs 34 Mbin/s at 4,096 points against 6.7 at 65,536. So §9.2's
    conclusion that bandwidth is not the limit is the x86 box's only. On
    that board the one-roll burst search has compute to spare and is short on
    acquisition Pd, 3 dB with the committed engine and 1 to 2 with overlapped dwells (§9), so
    narrower arithmetic is not on that application's critical path.
- **The arm64 two-thread case** gained nothing over one thread at the design
    point before §9.3's split, unexplained, and has not been measured on
    that box since.

## 10. Validation and benches

**The certified report** is `src/doppler/correlation/tests/validation/correlator/`:
four parts (the map, the 2-D detector, the search, the burst door), its
limits, and its open findings. `make validate-check` holds it current.

**The C tests** are in `native/tests/correlator/`; the equivalence tests
(`test_corr_equiv.c`, `test_detector_equiv.c`, `test_search_equiv.c`) pin
the flavors to each other and to their predecessors' scenes. The Python
suites are `src/doppler/correlation/tests/`.

**The benches** are one C binary, `bench_correlator_core`, and the
`correlation` module's Python benches. Rows keep their IDs across the
renames that made them one: `benchmarks/renames.toml` maps `c:corr`,
`c:corr2d`, `c:detector`, `c:detector2d`, `c:acq` and `c:burst_acq` to
`c:correlator`, and `make bench-ab` and the bench report pair rows through
it. A change is judged by `make bench-ab A=<main> B=<head>`, with no row
flagged SLOWER twice.

## 11. How it came to be one object

Six C directories and seven Python classes (`Corr`, `Corr2D`,
`DelayDoppler`, `CorrDetector`, `CorrDetector2D`, `Acquisition`,
`BurstAcquisition`) became this one object through
[#2752](https://github.com/doppler-dsp/doppler/issues/2752):
one delay-Doppler map (step 2,
[acq-surface §18](acq-surface.md#18-one-delay-doppler-map)), one C object
(step 3), one Python class (step 4). The plans as they stood when each
landed are archived:

- [step 3, one C object](archive/correlator-step3.md);
- [step 4, one Python class](archive/correlator-step4.md);
- [the batch calls](archive/corr2d-batch.md) and
    [the padded inverse and delay-Doppler flavor](archive/corr2d-interpolated-inverse.md)
    as first built.

## Related

- [The acquisition surface](acq-surface.md): the search's mathematics.
- [Detection](detection.md): the gate's primitives.
- [The correlator guide](../guide/correlator.md) and
    [the DSSS acquisition guide](../guide/dsss-acquisition.md).
- [The async DSSS receiver](async-dsss-receiver.md): the receiver that runs
    the continuous search.
