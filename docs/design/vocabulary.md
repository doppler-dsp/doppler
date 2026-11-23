---
description: "the library's decision vocabulary: the signal and the search's grid, decision and detection, tracking loops and lock detection, the retired words and the names the fields take"
---

# Vocabulary

The words the library's design pages, headers and reviews use for a
search, a decision and a loop. A term names its own axis or role; a bare
general word (block, look, tile, bin, row, cell, surface, span) is not a
term. A name in backticks is the C field; `py:` gives the Python name where
it differs. Numbers in brackets are the correlator's UNO Q case
([correlator §9](correlator.md#9-cost-and-parallelism)). An entry that is a
draft names the issue it waits on.

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
    period\].
- **memory**: an averaged statistic's effective length, (2 − α)/α
    updates for an EMA of weight α. An averaged statistic's total dwell is
    its update interval.
- **acquisition bound**: the frequency offset inside which a second-order
    loop locks in its normal lock time, about 5/bn updates: 0.5 bn per
    update, `bn / (2T)` Hz at an update interval T. Past it the lock time
    is the pull-in's and grows with the offset. Empirical, not derived
    from the loop equations: held for `carrier_nda` by its validation
    report (`src/doppler/track/tests/validation/carrier_nda/results.md`,
    §2.5) and, at the async receiver's configuration, by the committed
    sweep its design page cites
    ([async-dsss-receiver.md](async-dsss-receiver.md)).
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
which a match is not a use; `review only` marks a row whose sense a script
cannot tell, which review holds. Code spans, fenced blocks, generated
regions and the measurement records are not prose.

| retired                                                                      | use instead                                                                    | allowed                                                         |
| ---------------------------------------------------------------------------- | ------------------------------------------------------------------------------ | --------------------------------------------------------------- |
| `block`, `depth`                                                             | coherent dwell, and D its length in epochs                                     | `data-free block`, `refine block`, `refine blocks`, `blocks of` |
| `block lock`                                                                 | ratio-of-sums lock                                                             | —                                                               |
| `look` (one element of a non-coherent sum)                                   | coherent dwell                                                                 | `look-back`, `look for`, `looks like`                           |
| `looks`, `look count`, `n looks`                                             | non-coherent count                                                             | `looks like`                                                    |
| `term` (one element of a non-coherent sum)                                   | coherent dwell                                                                 | review only                                                     |
| `detection dwell`                                                            | total dwell                                                                    | —                                                               |
| `dwell` (bare, for a decision interval)                                      | total dwell                                                                    | `coherent dwell`, `total dwell`                                 |
| `dwell` (for an average's length)                                            | memory                                                                         | review only                                                     |
| `tile`, `Doppler window`, `span`, `native span`                              | Doppler roll; its width is the epoch rate                                      | —                                                               |
| `bin`, `row` (unqualified)                                                   | Doppler bin; Doppler interpolation                                             | `Doppler bin`, `lag bin`                                        |
| `cell`, `surface` (unqualified)                                              | delay-Doppler cell, delay-Doppler surface                                      | `delay-Doppler cell`, `delay-Doppler surface`                   |
| `preamble`, `burst` (for the thing detected)                                 | acquisition sequence                                                           | review only                                                     |
| `repetition` (for the interval)                                              | epoch                                                                          | `repetition count`                                              |
| `sliding`, `phase` (for total-dwell offsets)                                 | total-dwell offset; phase is carrier phase                                     | review only                                                     |
| `code phase` (for lag, of a non-code template)                               | lag                                                                            | review only                                                     |
| `dwell straddle`                                                             | alignment loss                                                                 | —                                                               |
| `lattice`, `lattice step`, `lattice image`, `carrier image`, `image spacing` | Doppler ambiguity, ambiguity interval                                          | —                                                               |
| `image` (for a Doppler ambiguity)                                            | Doppler ambiguity; image means a spectral, I/Q-mismatch or interpolation image | review only                                                     |
| `reliable pull-in`                                                           | acquisition bound, or no-slip edge                                             | —                                                               |
| `lock-in`                                                                    | acquisition bound, or no-slip edge                                             | —                                                               |

On design pages only, narration: a page states what is, so its prose does
not use these.

| retired                              | use instead                                       | allowed     |
| ------------------------------------ | ------------------------------------------------- | ----------- |
| `today`, `today's`                   | the present tense                                 | review only |
| `now` (against a before)             | the present tense                                 | review only |
| `as built`                           | the present tense                                 | —           |
| `proposed` (for what is built)       | what is                                           | review only |
| `used to`, `no longer`, `previously` | the present tense; the history goes to the record | —           |

## 5. Names

A count is `n_` and what it counts; a length carries its unit as a suffix.
`n_noncoh` is the non-coherent count's name and keeps it.

| name                                                   | becomes                         | where it is renamed                        |
| ------------------------------------------------------ | ------------------------------- | ------------------------------------------ |
| `coherent_bins` (D)                                    | `n_coherent`                    | the correlator's own PR, after the release |
| `window_bins` (W)                                      | `n_rolls`                       | the correlator's own PR, after the release |
| `code_bins` (N)                                        | `template_len`                  | the correlator's own PR, after the release |
| `doppler_bins`, the property (W·D)                     | `n_doppler_bins`                | the correlator's own PR, after the release |
| `doppler_bins`, `configure_search_raw`'s argument (D)  | `n_coherent`                    | the correlator's own PR, after the release |
| `doppler_span_hz` (half the epoch rate)                | `epoch_rate_hz` (the full rate) | the correlator's own PR, after the release |
| `pd_burst`                                             | `pd_acquisition`                | the correlator's own PR, after the release |
| `n_looks` (Dll, Despreader, both receivers)            | `n_noncoh`                      | the async receiver PR                      |
| `blk_len` (carrier_nda)                                | `n_noncoh`                      | the async receiver PR                      |
| `set_block_lock`                                       | `set_ratio_lock`                | the async receiver PR                      |
| `block_thresh`                                         | `ratio_thresh`                  | the async receiver PR                      |
| `CARRIER_NDA_BLOCK_MAX_LEN`                            | `CARRIER_NDA_LOCK_MAX_N_NONCOH` | the async receiver PR                      |
| `dp_det_ratio_block_len`                               | `dp_det_ratio_n_noncoh`         | the async receiver PR                      |
| `car_blk` (the async receiver)                         | `car_n_noncoh`                  | the async receiver PR                      |
| `lattice_hz` (the pool)                                | `ambiguity_interval_hz`         | the async receiver PR                      |
| `*_lat` (the pool's ghost validator)                   | `*_intervals`                   | the async receiver PR                      |
| `block_ms` (the pull-in validator's `# search` column) | `coherent_dwell_ms`             | the async receiver PR                      |
