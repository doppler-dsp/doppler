# Corr2D hypothesis bank: operation order

A bank of hypotheses on one frame, many carrier offsets each with several
Doppler-rate weightings, shares every stage that does not depend on the
hypothesis. This page sets the order the rest runs in, and how a carrier
offset between whole bins is applied. It serves
[#2683](https://github.com/doppler-dsp/doppler/issues/2683) (batch maps and
reductions on `Corr2D`/`DelayDoppler`) and
[#2670](https://github.com/doppler-dsp/doppler/issues/2670) (acq's rate
search on DelayDoppler's primitive). §1–§4 are pinned by a prototype that
`make docs-check` runs. The choice between the two methods in §3 rests on
the C timings §5 lists.

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

## 5. Cost, and what is left to measure

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

Ordering cuts the work about 3x. What is left is about 78% slow-time FFT, so
that is the next target. Two options to size there: computing only the rows
or columns a caller asks for, and sharing work across rate hypotheses.

Op counts are not times. Real code lengths are $2^n - 1$, where an FFT is
slowest, and $M = L\,n_x$ changes the length's factors. The gathers and mixes
are memory-bound, and the pool splits each stage differently. So the choice
between the two methods in §3, and the default $L$, wait for these timings in
C:

- the row FFT at $n_x$ and at $L\,n_x$, the row inverse, the slow-time FFT,
    and the gather and mix passes;
- at $n_x = 512$ and $n_x = 1023$, with $L \in \{2, 4, 8\}$;
- single-threaded and on the pool.
