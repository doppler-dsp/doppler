---
description: "the record behind the feedforward timing estimator's design page: what was predicted before its validator first ran, what the first runs showed, and what each wrong guess was replaced by"
---

# The feedforward symbol-timing estimator — the measurement record

The dated record behind
[the estimator's design page](timing-estimator.md), under the same section
numbers. The design page states what is; this page keeps how it got there.
Every prediction below was written in the header comment of
`native/validation/timing_est_noise.c` and committed before that file was
first run (2026-11-25, commit `3b88401ea`); the runs are the same day's.

## 4.1 Noise: what was predicted, and what the first run showed

Predicted by hand, for a rectangle at 4 samples a symbol with the eye on a
sample: `k_ss = 0`, `k_sn = 0.064`, `k_nn = 0.103`, and 0.018 symbols rms
from 128 symbols at Es/N0 4 dB.

The first run measured 0.0180 against that, on a rectangle that was merely
sampled. The stimulus was wrong, not the arithmetic: a rectangle sampled
at a whole number of samples a symbol gives the same samples at 16
generator offsets, so it carries no timing finer than a sample, and the
mean error against the pulse's centroid read 0.070 symbols. The rectangle
was moved behind a boxcar (each sample the mean over its own interval,
the condition RateSync's bank was given), and the constants on the design
page, 0.095 and 0.148, are that pulse's.

Not predicted: that the mean depends on where the samples fall in the
symbol. The prediction said the bias is zero for a block of whole symbols.
It is up to 0.011 symbols at 4 samples a symbol.

One row of the full sweep (the rectangle at 7 samples a symbol, 4000
trials) sat 4.3 and 4.7 standard errors from its prediction in line
energy and power. Run again at its own seed with 16000 trials it was
inside four. The sweep's thousand assertions are held at five standard
errors and the gate's forty at four, for the same chance of one false
failure in a run.

## 4.2 Clock offset: the `sinc` was predicted for every pulse

Predicted: the line falls as `sinc(x)` for `x` cycles a block, 0.900 at a
quarter, and the rate's variance is two timing variances over `L^2`, each
over `sinc(x)^2`.

Measured on the rectangle at 4 samples a symbol, a quarter cycle a block:
0.985 of the line's energy where `sinc^2` is 0.814, and a rate variance
of 2.3e-8 where the prediction was 1.7e-8. The rate itself was right.
The cause is §4.1's: the line's size depends on the sampling phase, an
offset sweeps that phase, and the reference was the line at one phase.
The prediction was replaced by one computed on the block's own dilated
geometry, which gives 0.986 and 2.2e-8, and the `sinc` is held on the
root-raised cosine, where it measured 0.827 against 0.814.

The prediction's pulse was first measured with no channel in the chain.
Two rows with a dilation of 5 and 10 ppm then missed in line energy by
1.4% and 2.2%, and stayed there at four times the trials. The channel's
resampler is part of the pulse; measured through it, the same rows are
within 0.6%.

## 4.3 Doppler: "the rate reads the dilation" was predicted for every pulse

Predicted: with carrier and time base together at 20 ppm, the rate reads
20 ppm.

Measured on the rectangle at 4 samples a symbol over 512 symbols: 14.4
ppm, 10 standard errors off, with the carrier alone reading 0.8 +- 0.7.
A drift of a hundredth of a symbol over the block is read through the
slope of the sampling-phase bias, which is up to 0.28. The prediction
that carries that bias gives 13.2. On the root-raised cosine the rate
measured 20.5 +- 1.5.

The Doppler rate's cost was predicted as the integral on the design
page, 0.80 of the line's power and 0.082 symbols at `q = 2`. On the
rectangle the run measured 0.56 and 0.074; on the root-raised cosine,
0.80 and 0.083.

## 4.4 The quality figure: the threshold model over-corrects

Predicted: noise alone has mean quality 0.0277 at 512 samples of the
rectangle's filter, a Rayleigh threshold sits 22% over its false-alarm
target because it leaves out the spread of the block's power (7.3%
there), and a threshold that averages the tail over that spread holds
the target.

Measured over 200000 trials: mean quality 0.02776. The Rayleigh
threshold gave 1.05 of the target, not 1.22, and the threshold with the
spread gave 0.84, not 1.0. The model took the block's power as
independent of `|c|`; they rise and fall together. The two thresholds are
asserted as a bracket and the exact one is open
([#3367](https://github.com/doppler-dsp/doppler/issues/3367)).

The detection probability was predicted "near 0.6" for the rectangle at
Es/N0 0 dB from 128 symbols, and measured 0.633.

## 4.4 The derived threshold: predicted before its run, held to its last term

The derivation on the design page was committed before it was run
(commit `5b22bbff7`), with the prediction that the measured false-alarm
probability's interval would hold the target on every row. On 200000
trials a row it gave 0.99 to 1.02 of the target on three rows, 1.04 on a
fourth, 1.12 on the 0.001 target (inside its wider interval), and 1.06
and 1.10 on two (the rectangle at 3.2 samples a symbol and the
root-raised cosine from 128 symbols), outside their intervals.
The derived second and fourth moments matched the measured ones on all
seven, so what misses is the tail's expansion past its first term, which
is 0.095 of the tail on the worst row. The rows are held to the target
within that term.

## 6 The cascade: what the first runs showed

Predicted before the first run (commit `fa597df1b`): behind the
converter's cascade the statistics are the pulse's as with no cascade,
and the sampling-phase bias is smaller.

The first run refused: a CIC plan's output for silence is not zero
(#3365), of order 1 where the test level was 0.1, and it lasts 16
outputs for the rectangle and up to 128 for the root-raised cosine. The
pulse and the converter's impulse response are now taken against that
output, and every row's lead-in went from 24 to 40 symbols. With that
the prediction held on every whole decimation and the bias measured
0.0018 to 0.0030 symbols.

Lengthening the lead-in moved every row. Two then missed and stayed
missed at four times the trials: line energy 1.9% over its prediction
at 20 ppm of Doppler, and a rate 0.8% off at a Doppler rate of q 0.5,
both on the rectangle at 4 samples a symbol through `doppler_channel`.
Probing the pulse a second time, one symbol later, changes the
prediction by a tenth of that, so neither the eye center nor the
pulse's shape is the cause. It is not found; those rows are held within
3% and 1%, stated as such.

## 7 What the gate was shown to catch

Seven one-line breaks of `timing_est_core`, each built and run against
the validator's `--check` and reverted, with the number of assertions
that went red: `tau`'s sign (14), `quality` times 0.9 (15), the rate's
divisor times 1.5 (5), `power` over 1.05 (12), the clock's step off by
half a cycle a block (28), `|x|` accumulated for `|x|^2` (29), and a
constant added to the correlation kernel (35, the row with no
transitions among them). Every assertion of `--check` went red under at
least one.
