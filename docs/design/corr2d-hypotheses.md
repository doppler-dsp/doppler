# Corr2D hypothesis bank: operation order

A bank of hypotheses on one frame, many carrier offsets each with several
Doppler-rate weightings, shares every stage that does not depend on the
hypothesis. This page sets the order the rest runs in, and how a carrier
offset between whole bins is applied. It serves
[#2683](https://github.com/doppler-dsp/doppler/issues/2683) (batch maps and
reductions on `Corr2D`/`DelayDoppler`) and
[#2670](https://github.com/doppler-dsp/doppler/issues/2670) (acq's rate
search on DelayDoppler's primitive). §1–§4 are pinned by a prototype that
`make docs-check` runs. §5 times them in C, from preamble-short codes to
1023, and shows that the order of the stages, not the method, is what sets
the cost. The bank mixes per fraction. §6 splits the grid in two: it detects
on $L_d = 2$ and refines each candidate to $L_r = 4$, which matches
$L = 4$'s sensitivity for 44% less work.

______________________________________________________________________

## 1. The question

A hypothesis bank runs one frame through many hypotheses: carrier offsets,
each with several Doppler-rate weightings. Run every hypothesis through the
whole pipeline and the cost is the pipeline times the hypothesis count. But
every stage before the power is linear, so the stages commute, and the order
they run in is free. This page fixes that order, and how a carrier offset
between whole bins is applied.

DelayDoppler's pipeline for one frame of $n_y$ periods of $n_x$ samples, with
code $c$, a carrier correction $f$ and a slow-time weight $w_r$ per period
$r$:

$$
M[\nu, \tau] = \sum_{r=0}^{n_y - 1} e^{-j 2\pi \nu r / n_{y,\text{out}}}
\, w_r \, \mathrm{IDFT}_{n_x}\!\left\{ \mathrm{DFT}_{n_x}\!\left\{ x_r[n]\,
e^{-j 2\pi f n} \right\} \overline{\mathrm{DFT}_{n_x}\{c\}} \right\}[\tau]
$$

where $x_r[n]$ is period $r$ and $n$ counts within it, $0 \le n < n_x$.

## 2. Ordering rules

1. **Non-linear last.** Power, max and any statistic come after every linear
    stage.
1. **Share the prefix.** What does not depend on the hypothesis runs once per
    frame: the row FFT always, and the row correlation when only $w$ varies.
1. **Carrier outer, rate inner.** $f$ acts on the row spectrum before the
    reference multiply, so each carrier hypothesis pays the row inverses.
    $w_r$ is a per-period scalar after them, so each rate hypothesis pays only
    the slow-time FFT.
1. **The carrier is corrected within a period only.** The mix restarts at
    every period ($n$ counts within the period, above). The phase a carrier
    adds *between* periods is slow-time Doppler, which the slow-time FFT
    measures. Leaving it alone puts a signal on the same, absolute Doppler row
    under every carrier hypothesis, so a pointwise reduction across carrier
    hypotheses compares like cells.

## 3. Carrier offsets on a $1/(L\,n_x)$ grid

A whole-bin offset $k/n_x$ is a circular roll of each row's spectrum. An offset
between whole bins is reached exactly in one of two ways. Both are pinned
equal to the full pipeline in §4.

**Zero-pad, then roll.** Pad each row to $M = L\,n_x$ and transform once. The
offset $g/M$ is then a stride-$L$ gather, and its whole-bin part is just where
the gather starts:

$$
\mathrm{DFT}_{n_x}\!\left\{ x[n]\, e^{-j 2\pi g n / M} \right\}[q]
= X_M\big[(L q + g) \bmod M\big]
$$

**Mix per fraction.** Write $g = L k + j$. Mix each row by $e^{-j 2\pi j n / M}$
for each of the $L$ fractions $j$, take an $n_x$-point FFT of each, and roll by
$k$.

Mixing per fraction does fewer operations, by $\log_2 n_x / \log_2 (L\,n_x)$.
Zero-pad then roll makes one pass and one plan. §5 times both; the bank
mixes per fraction.

An offset between grid points takes the nearest one, and loses the Dirichlet
factor of the residual within a period. The worst case is half a grid step,
which is $\mathrm{sinc}(1/(2L))$ to within $10^{-4}$. At $n_x = 512$:

<!-- corr2d-hypotheses:loss:start -->

| $L$ | grid step (cycles/sample) | worst loss |
| --- | ------------------------- | ---------- |
| 1   | $1/n_x$                   | 3.92 dB    |
| 2   | $1/(2\,n_x)$              | 0.91 dB    |
| 4   | $1/(4\,n_x)$              | 0.22 dB    |
| 8   | $1/(8\,n_x)$              | 0.06 dB    |

<!-- corr2d-hypotheses:loss:end -->

## 4. What the prototype pins

[`corr2d-hypotheses-measurements/prototype.py`](corr2d-hypotheses-measurements/prototype.py)
states the full pipeline in numpy and checks it against the shipped
`DelayDoppler` first, so every other comparison is against what ships. It
then runs both orderings over a grid of signed whole and fractional offsets.
The stimuli use continuous offsets and delays, because a signal placed on the
grid hides the droop and the straddle. `make docs-check` runs it with
`--check`. A claim that fails, or a table here that differs from what the
prototype renders, fails the gate.

<!-- corr2d-hypotheses:claims:start -->

| claim                                                                      | held to                                                   |
| -------------------------------------------------------------------------- | --------------------------------------------------------- |
| the numpy reference equals the shipped `DelayDoppler`                      | 2e-5 (float32 kernel), `test_corr2d_hypotheses_oracle.py` |
| zero-pad then roll equals the reference, all 57 maps                       | 1e-12                                                     |
| mix per fraction equals the reference, all 57 maps                         | 1e-12                                                     |
| the peak sits on the absolute Doppler row under every carrier hypothesis   | half a Doppler bin; delay within one lag                  |
| the loss between grid points is the Dirichlet factor, worst at half a step | 1e-9 to Dirichlet; 1e-3 to sinc(1/(2L))                   |
| the op counts are the plan's                                               | exact, to two places                                      |

<!-- corr2d-hypotheses:claims:end -->

## 5. Cost

Op counts per frame at the request's geometry: $n_y = 64$, $n_x = 512$,
$n_{y,\text{out}} = 64$ (assumed; #2683 does not state it), 4 carrier
hypotheses ($L = 4$) by 12 rate hypotheses. Units are millions of
$N \log_2 N$ per FFT.

<!-- corr2d-hypotheses:ops:start -->

| stage           | every hypothesis, full pipeline | ordered, zero-pad then roll | ordered, mix per fraction |
| --------------- | ------------------------------- | --------------------------- | ------------------------- |
| row forward FFT | 14.16 (48 x)                    | 1.44 (one per row)          | 1.18 (4 x)                |
| row inverse FFT | 14.16 (48 x)                    | 1.18 (4 x)                  | 1.18 (4 x)                |
| slow-time FFT   | 9.44 (48 x)                     | 9.44 (48 x)                 | 9.44 (48 x)               |
| total           | 37.75                           | 12.06                       | 11.80                     |

<!-- corr2d-hypotheses:ops:end -->

Counted, ordering cuts the work about 3x, and about 78% of what is left is
the slow-time FFT.

Op counts are not times. Real code lengths are $2^n - 1$, where an FFT is
slowest; $L\,n_x$ changes the length's factors; the gathers and mixes are
memory-bound; and a pool splits each stage its own way. So the harness
`native/validation/corr2d_hypothesis_order.c`
times one whole frame through three pipelines. The first is the full
pipeline: every hypothesis mixed, weighted and run through the shipped
`DelayDoppler`. The other two are the ordered pipelines, one per method of §3.
Every pipeline folds all its maps into one accumulator, and CTest holds the
three equal to $10^{-4}$. The carriers are the $L$ fractions of one whole
bin; whole-bin offsets cost both methods the same roll. One run, rendered by
[`corr2d-hypotheses-measurements/record.py`](corr2d-hypotheses-measurements/record.py):

<!-- corr2d-hypotheses:stamp:start -->

| run                                       | when                       | machine                               | commit             | toolchain                                                             | geometry                                                                | command                         |
| ----------------------------------------- | -------------------------- | ------------------------------------- | ------------------ | --------------------------------------------------------------------- | ----------------------------------------------------------------------- | ------------------------------- |
| `validate_corr2d_hypothesis_order --emit` | 2026-10-30 21:57 UTC, 19 s | `zen-x2elite-x`: unknown (18 threads) | `bf41ccc93`, clean | GNU 15.2.0; glibc 2.43; build Release, flags -O3 -DNDEBUG, native OFF | $n_y = 64$, $n_{y,\text{out}} = 64$, 12 rates x $L$ carriers, best of 7 | `make corr2d-hypotheses-record` |

<!-- corr2d-hypotheses:stamp:end -->

On one thread:

<!-- corr2d-hypotheses:serial:start -->

| $n_x$ | $L$ | maps | full (ms) | zero-pad then roll (ms) | mix per fraction (ms) | ordering buys | mix vs zero-pad | slow-time share (mix) |
| ----- | --- | ---- | --------- | ----------------------- | --------------------- | ------------- | --------------- | --------------------- |
| 32    | 2   | 24   | 0.20      | 0.08                    | 0.08                  | 2.6x          | +2%             | 87%                   |
| 32    | 4   | 48   | 0.40      | 0.16                    | 0.16                  | 2.6x          | +2%             | 87%                   |
| 32    | 8   | 96   | 0.80      | 0.31                    | 0.32                  | 2.6x          | +1%             | 87%                   |
| 64    | 2   | 24   | 0.33      | 0.15                    | 0.15                  | 2.2x          | +1%             | 89%                   |
| 64    | 4   | 48   | 0.66      | 0.30                    | 0.31                  | 2.2x          | +1%             | 89%                   |
| 64    | 8   | 96   | 1.33      | 0.62                    | 0.62                  | 2.2x          | -0%             | 89%                   |
| 127   | 2   | 24   | 3.50      | 0.55                    | 0.54                  | 6.5x          | -3%             | 51%                   |
| 127   | 4   | 48   | 6.67      | 1.07                    | 1.03                  | 6.5x          | -4%             | 50%                   |
| 127   | 8   | 96   | 14.19     | 2.30                    | 2.17                  | 6.5x          | -6%             | 51%                   |
| 128   | 2   | 24   | 0.66      | 0.31                    | 0.31                  | 2.2x          | +1%             | 89%                   |
| 128   | 4   | 48   | 1.32      | 0.61                    | 0.61                  | 2.2x          | +0%             | 90%                   |
| 128   | 8   | 96   | 2.66      | 1.23                    | 1.20                  | 2.2x          | -2%             | 89%                   |
| 255   | 2   | 24   | 3.11      | 0.76                    | 0.76                  | 4.1x          | -0%             | 72%                   |
| 255   | 4   | 48   | 6.02      | 1.45                    | 1.44                  | 4.2x          | -1%             | 72%                   |
| 255   | 8   | 96   | 11.98     | 2.95                    | 2.88                  | 4.2x          | -3%             | 72%                   |
| 256   | 2   | 24   | 1.31      | 0.61                    | 0.61                  | 2.2x          | +0%             | 90%                   |
| 256   | 4   | 48   | 2.61      | 1.23                    | 1.22                  | 2.1x          | -1%             | 90%                   |
| 256   | 8   | 96   | 5.15      | 2.36                    | 2.40                  | 2.2x          | +2%             | 90%                   |
| 512   | 2   | 24   | 2.79      | 1.84                    | 1.84                  | 1.5x          | +0%             | 92%                   |
| 512   | 4   | 48   | 5.53      | 3.31                    | 3.31                  | 1.7x          | +0%             | 93%                   |
| 512   | 8   | 96   | 10.70     | 6.67                    | 6.73                  | 1.6x          | +1%             | 93%                   |
| 1023  | 2   | 24   | 16.87     | 3.32                    | 3.29                  | 5.1x          | -1%             | 64%                   |
| 1023  | 4   | 48   | 34.27     | 6.87                    | 6.60                  | 5.2x          | -4%             | 64%                   |
| 1023  | 8   | 96   | 70.11     | 14.33                   | 13.26                 | 5.3x          | -7%             | 64%                   |

<!-- corr2d-hypotheses:serial:end -->

On a pool, at the default $L = 4$, each time beside the same pipeline's
time on one thread:

<!-- corr2d-hypotheses:pool:start -->

| $n_x$ | workers | full, ms (vs serial) | zero-pad then roll, ms (vs serial) | mix per fraction, ms (vs serial) |
| ----- | ------- | -------------------- | ---------------------------------- | -------------------------------- |
| 32    | 4       | 9.78 (24.46x)        | 1.01 (6.52x)                       | 1.28 (8.12x)                     |
| 32    | 18      | 36.28 (90.72x)       | 3.44 (22.14x)                      | 4.79 (30.31x)                    |
| 64    | 4       | 9.91 (14.96x)        | 1.02 (3.35x)                       | 1.38 (4.50x)                     |
| 64    | 18      | 37.87 (57.16x)       | 3.40 (11.15x)                      | 4.45 (14.48x)                    |
| 127   | 4       | 13.82 (2.07x)        | 1.68 (1.58x)                       | 1.82 (1.77x)                     |
| 127   | 18      | 34.34 (5.15x)        | 3.39 (3.18x)                       | 4.70 (4.58x)                     |
| 128   | 4       | 9.78 (7.39x)         | 1.28 (2.08x)                       | 1.56 (2.53x)                     |
| 128   | 18      | 33.84 (25.58x)       | 3.24 (5.28x)                       | 4.42 (7.18x)                     |
| 255   | 4       | 14.25 (2.37x)        | 2.22 (1.53x)                       | 2.10 (1.46x)                     |
| 255   | 18      | 38.76 (6.44x)        | 3.80 (2.62x)                       | 5.07 (3.51x)                     |
| 256   | 4       | 11.05 (4.23x)        | 1.95 (1.58x)                       | 2.04 (1.67x)                     |
| 256   | 18      | 39.02 (14.93x)       | 3.83 (3.12x)                       | 4.86 (3.97x)                     |
| 512   | 4       | 13.59 (2.46x)        | 3.11 (0.94x)                       | 3.51 (1.06x)                     |
| 512   | 18      | 38.43 (6.95x)        | 4.62 (1.40x)                       | 5.42 (1.63x)                     |
| 1023  | 4       | 37.91 (1.11x)        | 4.11 (0.60x)                       | 5.21 (0.79x)                     |
| 1023  | 18      | 44.49 (1.30x)        | 5.29 (0.77x)                       | 6.21 (0.94x)                     |

<!-- corr2d-hypotheses:pool:end -->

What the run shows:

1. **Ordering is the lever; the method is not.** On one thread the two
    methods of §3 are within 7% of each other in every row, and within 3%
    at the powers of two. The full pipeline is 1.5–6.5x slower. The gap is
    widest at the $2^n - 1$ lengths, where it pays a slow row FFT twice per
    map and an ordered pipeline pays once per carrier.
1. **The bank mixes per fraction.** Mixing needs only $n_x$-point row plans,
    which `Corr2D` already keeps per worker, and holds one fraction's spectra
    at a time. Zero-pad then roll needs an $L\,n_x$-point plan per worker and
    a row buffer $L$ times the frame, for no gain on one thread. On a pool it
    is ahead here only because it makes one forward dispatch where this
    harness's mix makes $L$. The bank's row stage dispatches once for all
    fractions.
1. **Dispatch once per carrier, never once per map.** A `dp_pool_run` wakes
    every helper and waits for each, whatever the work
    ([#2696](https://github.com/doppler-dsp/doppler/issues/2696)). Its cost
    therefore grows with the pool, and at these map sizes it is larger than
    the work of most stages. That is why the shipped full pipeline, at two
    dispatches per map, is slower on every pool than on one thread. The
    ordered pipelines' slow-time stage dispatches once per carrier, with the
    rates split across workers and innermost in the tile loop. Each
    correlated tile is then read once per carrier.
1. **Short frames run on one thread.** Even dispatched once per
    carrier, an ordered frame is slower on any pool than on one thread at
    every length up to 256: 1.5–2.5x on 4 workers from 127 to 256, and 3–8x
    at 32 and 64. Preambles are that short. At 512 it breaks even on 4
    workers and loses on 18; only at 1023 does it gain (0.6–0.94x). So the
    bank splits one frame across workers only at long codes, and a batch
    spreads whole frames across workers instead.
1. **The slow-time FFT is the next target.** It is most of an ordered frame:
    87–93% at the powers of two, and 50–72% at the $2^n - 1$ lengths.
1. **$L$ is a loss budget, not a speed choice; the default is 4.** An ordered
    frame's time grows in proportion to $L$ at every length, since every
    carrier costs the same. $L = 4$ holds the worst loss to 0.22 dB (§3) for
    twice the time of $L = 2$, which at $n_x = 64$ is 0.31 ms a frame
    against 0.15.

## 6. Detect coarse, refine the candidate

$L$ buys sensitivity only at the moment of detection. After a detection,
the fraction is resolved at one cell, where it costs almost nothing. So the
bank detects on a coarse grid and refines each candidate on the fine grid:

1. **Detect** on $L_d = 2$: half the carrier hypotheses of $L = 4$, so half
    the bank. Its threshold is set for a candidate rate (here $10^{-2}$ per
    search), not for the final $P_{fa}$.
1. **Refine** each candidate at its cell only: the two fine hypotheses
    ($L_r = 4$) on either side of the coarse one that found it, at the same
    delay and Doppler row. That is a time-domain correlation at one lag and a
    slow-time DFT at one row: $2\,n_y\,n_x$ multiply-adds a candidate, with no
    FFT.
1. **Confirm** on the refined value, against a threshold that sets the final
    $P_{fa}$.

The fraction a caller gets back is $L_r$'s, so what $L = 4$ resolved, the
refine still resolves. What changes is the cost.

**Measured** by the bank's characterization
(`spectral/tests/characterization/corr2d_bank/characterize.py`, through
`bank_power_max` and `bank_detect`): a repeated 511-chip PN m-sequence, 64
periods, at a uniform delay, whole bins $k \in \{-1, 0, 1\}$, in the shipped
`AWGN`. Every detector's thresholds come from its own 20,000 noise-only
frames at $P_{fa} = 10^{-3}$ per search, and the oracle corrects the exact
carrier at L = 4's threshold. Pd is at the SNR where the oracle detects
about 0.9, 1,000 trials a row, with Wilson 95% intervals:

| carrier                         | oracle               | $L = 4$              | $L = 2$              | detect 2 -> 4        |
| ------------------------------- | -------------------- | -------------------- | -------------------- | -------------------- |
| uniform fraction, −29.0 dB      | 0.895 [0.874, 0.913] | 0.899 [0.879, 0.916] | 0.889 [0.868, 0.907] | 0.901 [0.881, 0.918] |
| a quarter bin (worst), −30.7 dB | 0.897 [0.877, 0.914] | 0.903 [0.883, 0.920] | 0.861 [0.838, 0.881] | 0.898 [0.878, 0.915] |

On fresh noise each held its rate: $1.00 \times 10^{-3}$, $0.95 \times 10^{-3}$
and $1.00 \times 10^{-3}$ (intervals $[0.6, 1.5] \times 10^{-3}$). **At the
worst fraction $L = 2$ alone loses detections and detect-and-refine does not:
it matches $L = 4$.** Noiseless, the worst case loses the full sinc(1/4),
0.912 dB, exactly as §3 says; in noise $L = 2$ alone loses less (the
de-risk prototype put it at 0.37 dB of SNR), because two coarse hypotheses
straddle the signal and their max claws some back, and half the cells
lower its threshold. The refine recovers the rest.

**Cost.** By §5's run at $n_x = 512$ with 12 rates on one thread, an $L = 2$
frame is 1.84 ms against $L = 4$'s 3.31 ms: **44% less**. The shipped bank
measures the same at 64 x 512 with no rates (`bench_corr2d_core.c`,
`bench_bank`, one thread, aarch64): 12 carriers in 1.11 ms, 6 in 0.54 ms,
48%. The refine adds 0.17 ms a candidate, three fine carriers at
$2\,n_y\,n_x$ multiply-adds each, so `bank_detect` with one candidate runs
in 0.71 ms. Against the frame batch of 12 hand-corrected copies a caller
would otherwise build, 1.35 ms before the copies, the bank is 1.21x: the
row FFT is shared per fraction, and the slow-time stage is not.

**The bank, as shipped.** $L$ splits into $L_d$ (the search, default 2) and
$L_r$ (the resolution, default 4). §5's "the default is 4" stays true of
the resolution. A caller who wants one stage sets $L_d = L_r$.
`dp_corr2d_bank_fold` hands every hypothesis map to a C reducer;
`dp_corr2d_bank_power_max` and `dp_corr2d_bank_detect` are bound as
`bank_power_max` and `bank_detect` on `Corr2D` and `DelayDoppler`. On the
fast path and DelayDoppler with a native delay axis, the refine is a
time-domain correlation at the cell's lag and one Doppler DFT; anywhere
else it makes the fine map and reads the cell. Every path runs a bank, and
only those two share work. A bank runs on the calling thread
([#2716](https://github.com/doppler-dsp/doppler/issues/2716) is splitting
hypotheses across workers).

**Where this is certified.** The Corr2D report's §2.11 holds five limits:
the bank equals the corrected batch on every path; it leaves the dwell
alone and refuses what it should; the loss between grid points is at most
sinc(1/(2L)), and exactly that half a step out; `bank_detect` names the
fine carrier, and its refine is the fine bank's own value; and every
detector holds its $P_{fa}$, with detect-and-refine matching $L = 4$, at a
small scale. The full-geometry Monte Carlo is
`src/doppler/spectral/tests/characterization/corr2d_bank/characterize.py`,
run by `make characterize`, with its fast twin per push.
