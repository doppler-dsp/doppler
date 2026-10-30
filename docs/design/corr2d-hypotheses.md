# Corr2D hypothesis bank: operation order

A bank of hypotheses on one frame, many carrier offsets each with several
Doppler-rate weightings, shares every stage that does not depend on the
hypothesis. This page sets the order the rest runs in, and how a carrier
offset between whole bins is applied. It serves
[#2683](https://github.com/doppler-dsp/doppler/issues/2683) (batch maps and
reductions on `Corr2D`/`DelayDoppler`) and
[#2670](https://github.com/doppler-dsp/doppler/issues/2670) (acq's rate
search on DelayDoppler's primitive). §1–§4 are pinned by a prototype that
`make docs-check` runs. §5 times them in C, and shows that the order of the stages, not the method,
is what sets the cost.

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
Zero-pad then roll makes one pass and one plan. §5 decides between them.

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

| claim                                                                      | held to                                  |
| -------------------------------------------------------------------------- | ---------------------------------------- |
| the numpy reference equals the shipped `DelayDoppler`                      | 2e-5 (float32 kernel)                    |
| zero-pad then roll equals the reference, all 57 maps                       | 1e-12                                    |
| mix per fraction equals the reference, all 57 maps                         | 1e-12                                    |
| the peak sits on the absolute Doppler row under every carrier hypothesis   | half a Doppler bin; delay within one lag |
| the loss between grid points is the Dirichlet factor, worst at half a step | 1e-9 to Dirichlet; 1e-3 to sinc(1/(2L))  |
| the op counts are the plan's                                               | exact, to two places                     |

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

| run                                       | when                      | machine                               | commit             | toolchain                                                             | geometry                                                                | command                         |
| ----------------------------------------- | ------------------------- | ------------------------------------- | ------------------ | --------------------------------------------------------------------- | ----------------------------------------------------------------------- | ------------------------------- |
| `validate_corr2d_hypothesis_order --emit` | 2026-10-30 19:56 UTC, 7 s | `zen-x2elite-x`: unknown (18 threads) | `3080bdd74`, clean | GNU 15.2.0; glibc 2.43; build Release, flags -O3 -DNDEBUG, native OFF | $n_y = 64$, $n_{y,\text{out}} = 64$, 12 rates x $L$ carriers, best of 7 | `make corr2d-hypotheses-record` |

<!-- corr2d-hypotheses:stamp:end -->

On one thread:

<!-- corr2d-hypotheses:serial:start -->

| $n_x$ | $L$ | maps | full (ms) | zero-pad then roll (ms) | mix per fraction (ms) | ordering buys | mix vs zero-pad | slow-time share (mix) |
| ----- | --- | ---- | --------- | ----------------------- | --------------------- | ------------- | --------------- | --------------------- |
| 512   | 2   | 24   | 2.84      | 1.76                    | 1.77                  | 1.6x          | +0%             | 92%                   |
| 512   | 4   | 48   | 5.77      | 3.58                    | 3.54                  | 1.6x          | -1%             | 93%                   |
| 512   | 8   | 96   | 11.21     | 7.01                    | 7.35                  | 1.6x          | +5%             | 92%                   |
| 1023  | 2   | 24   | 17.25     | 3.44                    | 3.29                  | 5.2x          | -4%             | 64%                   |
| 1023  | 4   | 48   | 34.00     | 6.89                    | 6.59                  | 5.2x          | -4%             | 64%                   |
| 1023  | 8   | 96   | 68.82     | 13.90                   | 13.23                 | 5.2x          | -5%             | 64%                   |

<!-- corr2d-hypotheses:serial:end -->

On a pool, each time beside the same pipeline's time on one thread:

<!-- corr2d-hypotheses:pool:start -->

| $n_x$ | $L$ | workers | full, ms (vs serial) | zero-pad then roll, ms (vs serial) | mix per fraction, ms (vs serial) |
| ----- | --- | ------- | -------------------- | ---------------------------------- | -------------------------------- |
| 512   | 2   | 4       | 6.30 (2.21x)         | 1.44 (0.82x)                       | 1.63 (0.92x)                     |
| 512   | 2   | 18      | 20.93 (7.36x)        | 2.38 (1.35x)                       | 2.87 (1.62x)                     |
| 512   | 4   | 4       | 13.25 (2.30x)        | 3.03 (0.85x)                       | 3.30 (0.93x)                     |
| 512   | 4   | 18      | 42.00 (7.28x)        | 4.29 (1.20x)                       | 6.01 (1.70x)                     |
| 512   | 8   | 4       | 27.49 (2.45x)        | 5.65 (0.81x)                       | 6.02 (0.82x)                     |
| 512   | 8   | 18      | 73.64 (6.57x)        | 8.59 (1.23x)                       | 11.03 (1.50x)                    |
| 1023  | 2   | 4       | 19.45 (1.13x)        | 2.92 (0.85x)                       | 2.79 (0.85x)                     |
| 1023  | 2   | 18      | 21.16 (1.23x)        | 2.91 (0.84x)                       | 3.40 (1.03x)                     |
| 1023  | 4   | 4       | 35.33 (1.04x)        | 5.34 (0.78x)                       | 5.04 (0.77x)                     |
| 1023  | 4   | 18      | 42.31 (1.24x)        | 5.01 (0.73x)                       | 5.99 (0.91x)                     |
| 1023  | 8   | 4       | 73.77 (1.07x)        | 11.99 (0.86x)                      | 11.15 (0.84x)                    |
| 1023  | 8   | 18      | 82.36 (1.20x)        | 8.87 (0.64x)                       | 10.67 (0.81x)                    |

<!-- corr2d-hypotheses:pool:end -->

What the run shows:

1. **Ordering is the lever; the method is not.** On one thread, the two
    methods of §3 are within 10% of each other in every row. The
    full pipeline is several times slower, most of all at $n_x = 1023$. There
    it pays a non-power-of-two row FFT twice per map, where an ordered
    pipeline pays once per carrier.
1. **The two methods differ in structure, not speed.** Mixing per fraction
    needs only $n_x$-point row plans, which `Corr2D` already keeps per
    worker, and it can hold one fraction's spectra at a time. Zero-pad then
    roll needs an $L\,n_x$-point plan per worker and a row buffer $L$ times
    the frame. On a pool neither wins every row. Zero-pad then roll makes one
    forward dispatch where mixing makes $L$, which counts for more on the
    larger pool.
1. **Dispatch once per carrier, never once per map.** A `dp_pool_run` wakes
    every helper and waits for each, whatever the work. Its cost therefore
    grows with the pool, and at these map sizes it is larger than the work
    of most stages. That is why the shipped full pipeline, at two dispatches
    per map, is slower on every pool than on one thread. The ordered
    pipelines' slow-time stage dispatches once per carrier, with the rates
    split across workers and innermost in the tile loop. Each correlated
    tile is then read once per carrier.
1. **The slow-time FFT is the next target.** It is most of an ordered frame:
    nearly all of it at $n_x = 512$, and still the larger part at 1023.
1. **$L$ is a loss budget, not a speed choice.** An ordered frame's time
    grows in proportion to $L$, since every carrier costs the same. The loss
    it buys is §3's table.
