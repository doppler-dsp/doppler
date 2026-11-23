---
description: "the library's decision vocabulary: the signal and the search's grid, decision and detection, tracking loops and lock detection, the retired words and the names the fields take"
---

# Vocabulary

The words the library's design pages, headers and reviews use for a search, a
decision and a loop. A term names its own axis or role; a bare general word
(`block`, `look`, `tile`, `bin`, `row`, `cell`, `surface`, `span`) is not a
term. A name in backticks is the C field; `py:` gives the Python name where it
differs. Numbers in brackets are the correlator's UNO Q case ([correlator
§9](correlator.md#9-cost-and-parallelism)). An entry that is a draft names the
issue it waits on.

## 1. The signal and the search's grid

### The signal

- **burst**: a waveform type: short and time-limited, as opposed to
    continuous. Not the thing a detector finds. It is also the name of the
    correlator's door that searches one (`dp_correlator_create_burst`,
    `framing="burst"`), which keeps it.

- **preamble**: the part of a burst or continuous
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
    possibly dilated, is one epoch of the signal. For a spreading code the
    epoch may be called the **code period**.

- **epoch rate**: `fs`/N [1,280 Hz]. Also the Doppler range the slow-time
    transform covers on its own.

- **Doppler**: the signal's carrier frequency offset,
    Hz; it varies over the acquisition sequence. **Reported Doppler**: the
    value a hit carries, the offset at the middle of the coherent dwell it
    was decided on.

- **Doppler rate**: the Doppler's derivative, Hz/s
    (`doppler_rate`).

- **Doppler uncertainty**, ±U: the half-range of the
    Doppler the caller asks the search to cover (`doppler_uncertainty`)
    [±50 kHz].

- **delay** (τ): the offset between the signal's
    epoch boundaries and the engine's; continuous, 0 to N samples.
    **Sub-sample delay**: its fractional part only.

- **dilation**: the received epoch's departure from N
    samples [about 23 ppm at ±50 kHz on S-band].

- **Doppler ambiguity**: an estimate displaced from the true Doppler by a
    whole number of ambiguity intervals. An M-th power estimate over a
    coherent sum of length T measures the Doppler only modulo 1/(M T), as a
    pulse train measures it modulo its repetition frequency. **Ambiguity
    interval**: 1/(M T) \[at the 415.1-B-1 point, M = 2 and T one code
    period: 1,571 Hz at 2.2 GHz and 2,000 Hz at 2.8 GHz\].

### The engine's grid

- **sample**: one complex sample at the search rate
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
    W·D (`py: doppler_bins`).
- **Doppler interpolation**: the factor by which the
    engine evaluates the Doppler axis more finely than one bin (`interp`;
    2 when D > 1). A factor, not a second kind of bin.
- **Doppler roll**: one frequency shift of the search by a multiple of the
    epoch rate. **Roll count**, W (`window_bins`): how many shifts the
    search makes; W = 1 is no rolling. Each roll contributes D Doppler
    bins [79 rolls for ±50 kHz as the continuous door sizes it].
- **lag**: one point of the delay axis; **lag bin
    count** (`lag_bins`). A hit's `col` is a delay of col·N/`lag_bins`
    samples.
- **rate hypothesis**: one trial Doppler rate the
    coherent dwell is de-chirped by (`rate_hyps`; `py: rate_hypotheses`).
- **delay-Doppler cell**: one (Doppler bin at one
    interpolation point, lag) under one rate hypothesis.
- **delay-Doppler surface**: the cells of one total
    dwell. The stored surface holds the winning rate hypothesis's cells
    only.
- **total-dwell offset**: where a sequence of total
    dwells starts in the stream (not built;
    [#3054](https://github.com/doppler-dsp/doppler/issues/3054)). P offsets
    are spaced one total dwell / P apart. **Overlapped dwells**: total
    dwells from more than one offset.
- **alignment** (s₀): where the acquisition sequence
    starts inside a total dwell; continuous. **Aligned**: s₀ = 0.

### Losses

- **straddle**: loss from a signal parameter falling between grid points,
    and nothing else: **Doppler straddle** between Doppler grid points,
    **delay straddle** between lags. `straddle_loss` is the model's mean
    amplitude factor for these plus rotation within an epoch (dB =
    20·log10); diagnostic only.
- **alignment loss**: loss from the acquisition sequence being split
    across total dwells. Not a straddle. No field; it exists only inside
    `pd_burst`'s model.

### Cost

- **ns a sample**: wall-clock ns per search-rate sample, one thread unless
    a thread count is given.
- **core time**: ns a sample times the threads in use.
- **real time**: 1/`fs` per sample per core [1,526 ns].
- **budget**: real time times the cores available, minus what the front
    end takes.

## 2. Decision and detection

- **decision**: one threshold test of one total dwell's statistic.
- **verify count**: how many consecutive decisions declare a lock (up) or
    drop it (down): LockDet's `n_up` and `n_down`, sized by
    `dp_det_verify_count()`.

### Statistics

- **hit**: one listed peak of a total dwell whose maximum cleared the
    threshold (`dp_correlator_hit_t`); a total dwell can list several.
- **detection**: at least one hit from the total dwells the acquisition
    sequence touches.
- **`pfa`** (the constructor's): the probability that one noise-only total
    dwell lists any hit. Per total dwell: not per cell, not per second.
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

## 3. Tracking loops and lock detection

- **update**: one step of a tracking loop's filter. **Update interval**:
    the time between updates \[the async receiver's carrier loop: one code
    period\]. For an averaged statistic, the total dwell is the update
    interval; the average's length is its memory.
- **memory**: an averaged statistic's effective length, (2 − α)/α
    updates for an EMA of weight α.
- **acquisition bound**: the frequency offset inside which a second-order
    loop locks in its normal lock time, about 5/bn updates: 0.5 bn per
    update, `bn / (2T)` Hz at an update interval T. Past it the lock time
    is the pull-in's and grows with the offset. Empirical, not derived
    from the loop equations: held for `carrier_nda` by its validation
    report (`src/doppler/track/tests/validation/carrier_nda/results.md`,
    §2.5) and, at the async receiver's configuration, by the sweep
    committed with that receiver,
    `docs/design/async-dsss-receiver-measurements/sweep-1b7312b09/`: its
    `read_sweep.py`, section "Lock time by hand-over error", prints the
    median, p90 and worst lock time at each step in `B_L`, with 0.50 `B_L`
    the bound.
- **false lock**: a tracking loop sitting on a Doppler ambiguity. It is one
    definition for both uses: the receiver's carrier loop on an estimate
    whole ambiguity intervals off, and `carrier_nda`'s own stable locks at
    k/M cycles a sample, its ambiguity at an update of one sample.
- **no-slip edge**: the largest frequency step from which no start phase
    crosses two lock points on the mean loop. It is the detector's own,
    printed by `validate_costas_pullin --design` (`edge_worst`).

## 4. Retired words

A word below is not used in prose for the meaning given. The tables are
the one declaration a check reads: one row per retired word or phrase;
`retired` lists the phrases, each in backticks, matched as whole words
without regard to case; `allowed` lists phrases, each in backticks, inside
which a match is not a use, or `—` for none. A row is matched by a script
only where its phrase has one sense in this tree; a common word with other
senses is `review only`, which review holds and a check skips. Code spans,
fenced blocks, generated regions and the measurement records are not
prose.

| retired                                                                                  | use instead                                                                    | allowed     |
| ---------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------ | ----------- |
| `block lock`                                                                             | ratio-of-sums lock                                                             | —           |
| `detection dwell`                                                                        | total dwell                                                                    | —           |
| `dwell straddle`                                                                         | alignment loss                                                                 | —           |
| `look count`, `n looks`                                                                  | non-coherent count                                                             | —           |
| `tile`, `tiles`, `Doppler window`, `native span`                                         | Doppler roll; its width is the epoch rate                                      | —           |
| `lattice`, `lattices`, `lattice step`, `lattice image`, `carrier image`, `image spacing` | Doppler ambiguity, ambiguity interval                                          | —           |
| `reliable pull-in`, `lock-in`                                                            | acquisition bound, or no-slip edge                                             | —           |
| `block`, `depth` (for the search's coherent sum)                                         | coherent dwell, and D its length in epochs                                     | review only |
| `block` (for a lock's decision interval)                                                 | non-coherent count                                                             | review only |
| `look` (one element of a non-coherent sum)                                               | coherent dwell; the Dll's `look-back` keeps its name                           | review only |
| `looks` (the count)                                                                      | non-coherent count                                                             | review only |
| `term` (one element of a non-coherent sum)                                               | coherent dwell                                                                 | review only |
| `dwell` (bare, for a decision interval)                                                  | total dwell                                                                    | review only |
| `dwell` (for an average's length)                                                        | memory                                                                         | review only |
| `span` (for a Doppler roll's width)                                                      | the epoch rate                                                                 | review only |
| `bin`, `row` (for a Doppler bin)                                                         | Doppler bin; Doppler interpolation                                             | review only |
| `cell`, `surface` (of the search)                                                        | delay-Doppler cell, delay-Doppler surface                                      | review only |
| `preamble`, `burst` (for the thing detected)                                             | acquisition sequence                                                           | review only |
| `repetition` (for the interval)                                                          | epoch                                                                          | review only |
| `sliding`, `phase` (for total-dwell offsets)                                             | total-dwell offset; phase is carrier phase                                     | review only |
| `code phase` (for lag, of a non-code template)                                           | lag                                                                            | review only |
| `image` (for a Doppler ambiguity)                                                        | Doppler ambiguity; image means a spectral, I/Q-mismatch or interpolation image | review only |

On design pages only, narration: a page states what is, so its prose does
not use these.

| retired                                                   | use instead                                       | allowed     |
| --------------------------------------------------------- | ------------------------------------------------- | ----------- |
| `as built`                                                | the present tense                                 | —           |
| `today`, `today's`                                        | the present tense                                 | review only |
| `now` (against a before)                                  | the present tense                                 | review only |
| `proposed` (for what is built)                            | what is                                           | review only |
| `used to`, `no longer`, `previously` (narrating a change) | the present tense; the history goes to the record | review only |

## 5. Names

A count is `n_` and what it counts; a length carries its unit as a suffix.
`n_noncoh` is the non-coherent count's name and keeps it. The table is the
whole list of renames agreed on 2026-11-23 and not yet merged; a row leaves
when its rename merges.

**In the async receiver PR**, the names as the branch spells them. Shipped
in a release, so retired by name and declared breaking:

| name         | becomes                 | where                                                                                                                                                                                                  |
| ------------ | ----------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `n_looks`    | `n_noncoh`              | the Dll's field and its `configure_lock` and `configure_lock_raw` parameter; the Despreader's, DsssReceiver's and AsyncDsssReceiver's `configure_lock` or `configure_lock_raw` parameter; C and Python |
| `p_look`     | `p_decision`            | the first parameter of `det_verify_count` and `det_verify_delay`, C and Python                                                                                                                         |
| `lattice_hz` | `ambiguity_interval_hz` | AsyncDsssPool's state field                                                                                                                                                                            |

New on that branch:

| name                                   | becomes                                                  | where                                                                  |
| -------------------------------------- | -------------------------------------------------------- | ---------------------------------------------------------------------- |
| `set_block_lock`                       | `set_ratio_lock`                                         | CarrierNda, C and Python                                               |
| `get_block_thresh`, `block_thresh`     | `get_ratio_thresh`, `ratio_thresh`                       | CarrierNda, C and the Python property                                  |
| `blk_len`                              | `n_noncoh`                                               | CarrierNda's field                                                     |
| `blk_thr`, `blk_pfa`                   | `ratio_thr`, `ratio_pfa`                                 | CarrierNda's fields                                                    |
| `blk_n`, `blk_num`, `blk_den`          | `ratio_n`, `ratio_num`, `ratio_den`                      | CarrierNda's running sums                                              |
| `CARRIER_NDA_BLOCK_MAX_LEN`            | `CARRIER_NDA_LOCK_MAX_N_NONCOH`                          | CarrierNda's macro                                                     |
| `det_ratio_block_len`                  | `det_ratio_n_noncoh`                                     | detection, C and Python                                                |
| `car_blk`                              | `car_n_noncoh`                                           | AsyncDsssReceiver's field; `carrier_lock_n_noncoh` keeps its name      |
| `blocks`                               | `n_noncoh`                                               | the fine design's output field                                         |
| `block_s`                              | `coherent_dwell_s`                                       | the fine design's parameter                                            |
| `max_blocks`                           | `max_n_noncoh`                                           | the fine design's parameter                                            |
| `look_prompt_amp`                      | `lock_prompt_amp`                                        | the Dll's field; its probe stays `prompt_amp`                          |
| `look_offset_pwr`                      | `lock_offset_pwr`                                        | the Dll's field; its probe stays `offset_pwr`                          |
| `lock_look`                            | `lock_accumulate`                                        | the Dll's internal function                                            |
| `df_end_lat`, `end_b_lat`, `end_a_lat` | `df_end_intervals`, `end_b_intervals`, `end_a_intervals` | the pool's ghost validator; its committed CSV columns stay as recorded |
| `gh_on_image`, `GH_IMAGE_TOL_HZ`       | `gh_on_ambiguity`, `GH_AMBIGUITY_TOL_HZ`                 | the pool's ghost validator                                             |
| `block_ms`                             | `coherent_dwell_ms`                                      | the pull-in validator's `# search` column                              |

**In the correlator's own PR, after the release**, all shipped:

| name                                                  | becomes                         | where                                                                   |
| ----------------------------------------------------- | ------------------------------- | ----------------------------------------------------------------------- |
| `coherent_bins` (D)                                   | `n_coherent`                    | the correlator, and the async receiver's matching argument and property |
| `window_bins` (W)                                     | `n_rolls`                       | the correlator                                                          |
| `code_bins` (N)                                       | `template_len`                  | the correlator                                                          |
| `doppler_bins`, the property (W·D)                    | `n_doppler_bins`                | the correlator                                                          |
| `doppler_bins`, `configure_search_raw`'s argument (D) | `n_coherent`                    | the correlator                                                          |
| `doppler_span_hz` (half the epoch rate)               | `epoch_rate_hz` (the full rate) | the correlator                                                          |
| `pd_burst`                                            | `pd_acquisition`                | the correlator                                                          |
| `n_blocks`, `max_n_blocks`, `refine_max_n_blocks`     | not yet named                   | CarrierAcquisition, and the receiver's refine argument                  |
