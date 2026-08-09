# CIC Decimator — the fixed-point input budget

The CIC is doppler's cheap bulk decimator: no multipliers, four integrators and
four combs, decimation by any power of two up to 4096. It is **fixed point by
construction** — that is the whole reason it is cheap — and a fixed-point
pipeline has an input budget that must be spent explicitly. This page is that
budget.

Fixed design parameters: `N = 4` stages, `M = 1` (one-sample comb),
`R` a power of two in `[2, 4096]`.

______________________________________________________________________

## The budget has two terms, and they are not the same term

A caller presenting a signal to the CIC is spending two different things at
once, and conflating them is how a chain ends up quietly wrong:

| term              | what it is                                    | where it is budgeted   |
| ----------------- | --------------------------------------------- | ---------------------- |
| **DC gain**       | the pipeline's own growth, `R^N`              | the 64-bit accumulator |
| **PAPR headroom** | the signal's peak above its nominal amplitude | the input encoder      |

The first is the filter's; the second is the signal's. Budgeting only the
first — which is what this implementation did until the headroom was added —
leaves the caller silently responsible for the second, and nothing downstream
restores what they gave away.

______________________________________________________________________

## Term 1 — DC gain, and why the accumulator is 64 bits

Each integrator accumulates, each comb differences. For `M = 1` the
composite response is

$$H(z) = \left(\frac{1 - z^{-R}}{1 - z^{-1}}\right)^{N}$$

whose DC gain is $R^N$ — at `R = 4096`, `N = 4` that is $2^{48}$.

The input is encoded to **UQ16** (16 bits, offset binary), so the largest value
any accumulator can hold is

$$(2^{16} - 1)\cdot R^N \;=\; (2^{16}-1)\cdot 2^{48} \;=\; 2^{64} - 2^{48} \;<\; 2^{64}$$

which is why the accumulators are `uint64_t` and why `R` is capped at 4096: the
budget is consumed **exactly** at the top of the range. Below it there is
slack, and a lot of it:

| R    | pipeline gain `R^4` | max accumulation | spare   |
| ---- | ------------------- | ---------------- | ------- |
| 8    | 2¹²                 | 2²⁸              | **2³⁶** |
| 32   | 2²⁰                 | 2³⁶              | **2²⁸** |
| 128  | 2²⁸                 | 2⁴⁴              | 2²⁰     |
| 512  | 2³⁶                 | 2⁵²              | 2¹²     |
| 2048 | 2⁴⁴                 | 2⁶⁰              | 2⁴      |

The gain is removed on output by a right shift of `N·log2(R)` bits, so the
filter's **DC gain is exactly one** — `cic_dc_gain()` computes `R^N / 2^shift`
from the stored `R` and `shift` rather than asserting it, which is what lets a
gate catch the two drifting apart.

!!! note "Every intermediate overflow is harmless, and that is not luck"

    All arithmetic is unsigned, so wrapping is defined (mod 2⁶⁴). The
    integrator/comb pair is exactly invertible under modular arithmetic: every
    overflow in an integrator cancels in the corresponding comb, **provided the
    true result fits in 64 bits**. So the pipeline needs no saturation, no
    range checks and no floating point. The only saturation in the block is at
    the encoder, and it is a signal-level bound, not an arithmetic guard.

______________________________________________________________________

## Term 2 — PAPR headroom, and why full scale is not the symbol amplitude

The encoder maps CF32 to UQ16. The question it has to answer is *what input
amplitude corresponds to full scale*, and the tempting answer — 1.0 — is wrong
for every pulse-shaped signal, because **a symbol stream's peak is not its
symbol amplitude**.

Measured on a root-raised-cosine BPSK stream, roll-off 0.35:

$$\frac{\max_n |x[n]|}{A_{\text{symbol}}} = 1.582$$

and it is **identical at every samples-per-symbol** — 4, 8, 16, 32, 64 — because
it is a property of the pulse and the symbol alphabet, not of the sampling
grid.

So with full scale at 1.0, the largest symbol amplitude that survives is
`1/1.582 = 0.632`. A caller who does not know that clips; a caller who does
know it backs off by 4 dB. Either way the amplitude arriving at whatever
consumes the CIC's output is 0.632 of what was sent, and **nothing puts it
back** — the cascade around it is unity gain by design
([`RateConverter_gain()`](../c-api/index.md)), so it faithfully preserves the
shortfall. Downstream of a timing loop that costs the square of it: a Gardner
detector's slope goes as $A^2$, so 0.632 amplitude is a loop running at
**2.5×** below the bandwidth its `bn` names.

The fix is to budget it where it belongs:

```c
#define CIC_PAPR_HEADROOM 2.0f   /* 6 dB, voltage ratio */
```

The encoder scales by `32768 / CIC_PAPR_HEADROOM` and the decoder by
`CIC_PAPR_HEADROOM / 32768`. Two consequences worth stating explicitly:

- **The shift is untouched**, so the DC gain is still exactly one and
    `cic_dc_gain()` still reads 1.0. The headroom is an encode/decode scale
    *pair*, not a change to the normalisation.
- **The offset is not scaled by it.** `+32768` is the offset-binary midpoint —
    it is not signal, and scaling it would move the zero.

`2.0` covers the measured 1.582 with margin.

### How much headroom is available, given R

The headroom does not have to be bought by attenuating inside 16 bits. The
accumulator is 64 bits and the DC gain only consumes `N·log2(R)` of them, so
**the spare bits ARE headroom** — widening the input's integer range by `h`
bits costs no quantisation precision at all, subject to

$$2^{16+h}\cdot R^{N} \le 2^{64} \quad\Longrightarrow\quad h \le 48 - N\log_2 R$$

| R    | `N·log2(R)` | spare bits | max headroom (voltage) | **max headroom (dB)** |
| ---- | ----------- | ---------- | ---------------------- | --------------------- |
| 2    | 4           | 44         | 1.8e13                 | **264.9**             |
| 4    | 8           | 40         | 1.1e12                 | **240.8**             |
| 8    | 12          | 36         | 6.9e10                 | **216.7**             |
| 16   | 16          | 32         | 4.3e9                  | **192.7**             |
| 32   | 20          | 28         | 2.7e8                  | **168.6**             |
| 64   | 24          | 24         | 1.7e7                  | **144.5**             |
| 128  | 28          | 20         | 1.0e6                  | **120.4**             |
| 256  | 32          | 16         | 65536                  | **96.3**              |
| 512  | 36          | 12         | 4096                   | **72.2**              |
| 1024 | 40          | 8          | 256                    | **48.2**              |
| 2048 | 44          | 4          | 16                     | **24.1**              |
| 4096 | 48          | 0          | 1                      | **0.0**               |

Every ratio the planner actually produces has decades of room: even `R = 2048`
allows 24 dB, four times what a pulse-shaped signal needs. **`R = 4096` is the
sole exception** — the budget is consumed exactly there (16 + 48 = 64), so it
has no spare bit and any headroom at that ratio must be bought by attenuation.

The implementation currently buys the headroom the attenuating way at every
`R`, which costs **2 dB** of quantisation SNR relative to a caller performing a
perfect 1.582 backoff — which no caller did, because nothing told them the
number. Taking it from the spare bits instead would recover that 2 dB (and beat
the perfect-backoff caller by 4 dB) for every `R <= 2048`; the mechanics are
that the offset and clip bounds widen, the shift is unchanged, `cic_dc_gain()`
still reads 1, and the `uint16_t` decode cast widens.

______________________________________________________________________

## What a DC probe cannot see

Worth recording, because it was found by sabotaging a gate and watching it stay
green: **the CIC's DC output is insensitive to its own normalisation shift.**

The offset-binary bias travels through the same pipeline as the signal and is
removed after the same shift. Halve the shift and both the signal *and* the
restored bias double, and the decoded DC value comes back unchanged — while the
filter's gain on everything that actually varies has doubled.

Measured: with `shift` deliberately reduced by one bit, a DC probe through the
cascade still read **1.000**, and the matched-filter symbol amplitude read
**2×**. So a unity-gain gate that probes with DC is decorative on this filter.
The gate uses a tone at 1/512 of the output rate instead — low enough that CIC
droop is below 1e-4, so it reads the same as DC on a healthy filter, and high
enough that it is *signal* rather than bias.

______________________________________________________________________

## Response, for sizing

Independent of `R`:

| property         | value                         |
| ---------------- | ----------------------------- |
| alias rejection  | ~77 dB at `f_p = 0.1·f_out`   |
| passband droop   | ~0.57 dB at `f_p = 0.1·f_out` |
| output precision | 16-bit Q15                    |

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
