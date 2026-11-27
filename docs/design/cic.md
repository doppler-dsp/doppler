---
description: "the fixed-point input budget: gain, headroom and precision, the three terms of a 64-bit register"
---

# CIC Decimator — the fixed-point input budget

The CIC is doppler's cheap bulk decimator: no multipliers, four integrators and
four combs, decimation by any power of two up to 2048. It is **fixed point by
construction** — that is the whole reason it is cheap — and a fixed-point
pipeline has an input budget that must be spent explicitly. This page is that
budget.

Fixed design parameters: `CIC_N = 4` stages, one-sample combs, `R` a power of
two from 2 to `CIC_R_MAX` = 2048, registers of `CIC_REG_BITS` = 64 bits.

______________________________________________________________________

## The budget is the register, and it has three terms

A 64-bit register holds three things, and they are all the budget there is:

| term          | what it is                                      | bits                               |
| ------------- | ----------------------------------------------- | ---------------------------------- |
| **gain**      | the pipeline's own growth, `R^N`                | `N · log2(R)`                      |
| **headroom**  | the signal's peak above its unit amplitude      | `headroom_bits`, the top of a word |
| **precision** | how small a sample can be and still be a number | the rest of the word               |

The first is the filter's; the other two are the signal's, and together they
are the **input word**. Budgeting only the gain leaves a caller silently
responsible for the peaks; budgeting gain and peaks leaves the smallest
sample unstated, and a stream quieter than one step is read as zeros.

______________________________________________________________________

## Term 1 — gain, and what it leaves for the word

Each integrator accumulates, each comb differences. With one-sample combs
the composite response is

$$H(z) = \left(\frac{1 - z^{-R}}{1 - z^{-1}}\right)^{N}$$

whose DC gain is $R^N$, removed on output by a right shift of `N·log2(R)`
bits, so the filter's **DC gain is exactly one** — `dp_cic_dc_gain()` computes
`R^N / 2^shift` from the stored `R` and `shift` rather than asserting it,
which is what lets a gate catch the two drifting apart.

The largest sum the output forms is `(2^word − 1) · R^N`, so the word can be
as wide as `64 − N·log2(R)` bits and no wider:

| R    | gain, bits | widest word, bits |
| ---- | ---------- | ----------------- |
| 2    | 4          | 60                |
| 16   | 16         | 48                |
| 64   | 24         | 40                |
| 256  | 32         | 32                |
| 512  | 36         | 28                |
| 1024 | 40         | 24                |
| 2048 | 44         | 20                |

`CIC_R_MAX` is where the widest word is still wider than 16 bits: one
doubling further it is 16 exactly.

!!! note "Every intermediate overflow is harmless, and that is not luck"

    All arithmetic is unsigned, so wrapping is defined: C99 6.2.5p9 reduces
    an unsigned result modulo 2⁶⁴, for a 64-bit type as for any other. The
    integrator/comb pair is exactly invertible under modular arithmetic:
    every overflow in an integrator cancels in the corresponding comb,
    **provided the true result fits in 64 bits**, and the word is sized so
    that it does, with nothing over. So the pipeline needs no saturation,
    no range checks and no floating point. The only saturation in the block
    is at the encoder, and it is a signal-level bound, not an arithmetic
    guard.

______________________________________________________________________

## The word has a precision, and three are named

A filter is built for one of three words (`dp_cic_set_precision()`):

| precision        | word                                  | full scale as built | what it is for                                       |
| ---------------- | ------------------------------------- | ------------------- | ---------------------------------------------------- |
| 16               | 16 bits, at every ratio               | 2.0 (6 dB)          | the filter as it was, output for output              |
| 32               | 32 bits, or the widest word when less | 4.0 (12 dB)         | a fixed word across the ratios that have room for it |
| 64 (the default) | the widest word                       | 4.0 (12 dB)         | everything the register has                          |

Past `R = 256` the register has under 32 bits left and the two wide
precisions are the same word.

## Term 2 — headroom, and why full scale is not the symbol amplitude

The top of the word is headroom: full scale is a power of two above unit
amplitude, `2^headroom_bits` (`dp_cic_set_headroom_bits()`,
`dp_cic_get_full_scale()`). The tempting full scale — 1.0 — is wrong for
every shaped signal, because **a stream's peak is not its amplitude**.

Measured on a root-raised-cosine BPSK stream, roll-off 0.35:

$$\frac{\max_n |x[n]|}{A_{\text{symbol}}} = 1.582$$

and it is **identical at every samples-per-symbol** — 4, 8, 16, 32, 64 — because
it is a property of the pulse and the symbol alphabet, not of the sampling
grid. A multi-carrier signal's peaks sit 10 to 12 dB over its average.

With full scale at 1.0 a caller who does not know the first number clips; one
who does backs off by 4 dB, and **nothing puts it back** — the cascade around
the CIC is unity gain by design
([`dp_RateConverter_gain()`](../c-api/index.md)), so it faithfully preserves
the shortfall. Downstream of a timing loop that costs the square of it: a
Gardner detector's slope goes as $A^2$, so 0.632 amplitude is a loop running
at **2.5×** below the bandwidth its `bn` names.

So the wide precisions start with 2 bits of headroom, full scale 4.0, which
holds a multi-carrier signal's peaks, and the 16-bit one keeps the 1 bit it
always had, which holds a single carrier's. Either is a setting: each bit of
headroom is 6 dB more room and one bit less precision.

## Term 3 — precision, and the step at the bottom

What the headroom does not take is precision. The input is an integer, so a
component smaller than one **step** reads zero (`dp_cic_get_step()`):
`full_scale / 2^(word − 1)`.

| R    | step at 64, full scale 4.0 | step at 32, full scale 4.0 | step at 16, full scale 2.0 |
| ---- | -------------------------- | -------------------------- | -------------------------- |
| 2    | 6.9e-18                    | 1.9e-09                    | 6.1e-05                    |
| 16   | 2.8e-14                    | 1.9e-09                    | 6.1e-05                    |
| 64   | 7.3e-12                    | 1.9e-09                    | 6.1e-05                    |
| 256  | 1.9e-09                    | 1.9e-09                    | 6.1e-05                    |
| 512  | 3.0e-08                    | 3.0e-08                    | 6.1e-05                    |
| 1024 | 4.8e-07                    | 4.8e-07                    | 6.1e-05                    |
| 2048 | 7.6e-06                    | 7.6e-06                    | 6.1e-05                    |

The step is a floor on what the filter can pass, whatever the signal's
quality. A stream whose noise is about one step comes out as zeros with an
occasional single step, and a statistic that is a ratio over those few
samples is large by construction. At 16 bits that floor is 90 dB under full
scale; at the default it is under a single-precision float's own resolution
of a unit-amplitude sample for every ratio up to 512.

`test_cic_core.c` (`test_the_word`) holds the word at both ends: at each
precision every output equals a model of the definition that uses no modular
arithmetic (480 rows of tones and noise, to full scale and past it), a slow
tone at a quarter of full scale is off its exact value by 1.5 steps at 16
bits and by 3e-8 of itself at 64, and both rails pass as DC at every ratio
and precision.

______________________________________________________________________

## The offset goes around the pipeline, not through it

Every register in the pipeline is unsigned, because the pipeline depends on
its registers wrapping and in C only an unsigned type promises that. A
sample is signed, so it is made non-negative with the word's offset-binary
midpoint, `2^(word − 1)`, and that offset is handled at the two ends:

- **In:** the offset is added to make the conversion's result non-negative,
    and taken straight off again in `uint64_t`. The register holds the
    sample modulo 2⁶⁴, and a zero sample is a zero word.
- **Out:** the offset is added back at the pipeline's gain,
    `offset << shift`, before the normalizing shift. The sum is the
    offset-binary result, in `[0, 2^word · R^N)`, so the shift is of a value
    that is never negative.

Two things follow.

**A filter that has only seen silence holds zeros.** Its first `N − 1`
outputs after create or reset are the boxcars filling with the input and
nothing else: silence in is exact zeros out from the first output. Carried
through the pipeline instead, the offset is a full-scale step the empty
boxcars have to fill with: the filter as it was read −1.88, −0.88 and
−0.06 for silence in its first three outputs at `R = 16`.

**The steady state does not change.** Once the boxcars are full the offset
through the pipeline is `offset · R^N`, which is the term added at the
output. `test_the_word` holds it against that earlier filter: at 16 bits,
160 rows equal it bit for bit on every output past the fill, and differ
from it inside the fill on every row.

A probe for the normalizing shift still wants a tone: the gate uses one at
1/512 of the output rate, low enough that the droop is below 1e-4.

______________________________________________________________________

## Response, for sizing

Independent of `R`:

| property         | value                         |
| ---------------- | ----------------------------- |
| alias rejection  | ~77 dB at `f_p = 0.1·f_out`   |
| passband droop   | ~0.57 dB at `f_p = 0.1·f_out` |
| output precision | the input word's              |

The droop is real and is why `RateConverter` folds a compensator into the
terminal stage's polyphase bank when a CIC precedes it — same tap grid, so the
fold is exact and costs no extra pass over the data. Measured, the fold is
worth ~28 dB of matched-filter EVM, which is why `compensate` is effectively
mandatory on that path.

______________________________________________________________________

## Related

- [Quantization](QUANTIZATION.md) — the encoding and headroom conventions
    doppler's fixed-point boundaries share
- [Continuously Variable Resampler](RESAMPLER.md) — the polyphase stage that
    usually follows a CIC, and the arm/accumulator contract
- [MPSK Receiver](mpsk.md) §4–5.1 — the unity-gain and level contracts this
    budget has to satisfy
