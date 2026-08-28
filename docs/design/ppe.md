# The Polynomial-Phase Estimator — the reasoning

**Scope:** why `PolynomialPhaseEstimator` (`ppe`) has the shape it does — a
coherent two-dimensional search rather than a loop, one knob spanning
near-static Doppler through severe LEO chirp, and a transform four times
larger than its input. Not how to use it: that is the
[API reference](../api/python-dsss.md) and the object's own certification,
`src/doppler/dsss/tests/validation/ppe/results.md`, which is where every
number quoted below was measured.

______________________________________________________________________

## 1. The problem: one shot, no second chance

A tracking loop earns its estimate over time. It can start wrong, because the
error it makes on the first sample is fed back and walked out over the next
thousand. That trade — accept a transient, buy accuracy — is the whole reason
a loop exists, and it is unavailable to a burst receiver.

A burst is over before a loop has converged. `BurstDemod` gets **one**
frequency estimate per burst and there is no residual-walking stage behind it:
whatever the estimate is wrong by, the whole burst is demodulated wrong by. So
the estimator has to be right the first time, from the segment in hand, which
makes it feedforward and by-value — stateless, with `reset()` a documented
no-op, because an estimate depends only on the samples handed to that call.

That framing sets the bar. The question is not "does it converge" but "how
close is a single shot, and at what SNR does it stop being close".

______________________________________________________________________

## 2. Why two dimensions

A signal with a constant frequency offset has linear phase. A signal seen
across a satellite pass does not: the range rate itself changes, so the phase
is quadratic and the frequency **ramps**. Fitting a constant frequency to a
ramping one smears the energy across bins, and the smear is what destroys the
estimate — not noise.

So the model is polynomial phase to second order: a frequency `f`
(cycles/sample) and a chirp rate `r` (cycles/sample²). The estimator searches
both:

> For each chirp-rate hypothesis `r_i`, dechirp the segment by
> `exp(-j·π·r_i·m²)` and take its FFT. The true `(r, f)` is the peak of the
> resulting (chirp-rate × frequency) surface.

Dechirping by the *correct* rate makes the phase linear again, which is
exactly the condition under which an FFT concentrates the energy into one bin.
Every wrong rate leaves residual curvature and a lower, wider peak. The search
is therefore a **matched filter** over the two-parameter family, and it is
fully coherent — which is what makes it optimal in the estimator sense, and
what makes it hold at low SNR rather than falling off a cliff (§5).

**`max_rate = 0` is not a special case, it is the degenerate one.** The rate
axis collapses to a single hypothesis, the search becomes a single FFT, and
the returned rate is forced to *exactly* `0.0` — a caller may test it for
equality. One knob therefore spans the whole range from near-static Doppler to
a severe LEO chirp, with the cost paid only where the dynamics demand it: the
work is linear in the number of rate hypotheses.

______________________________________________________________________

## 3. The grid is coarse; the refinement is what makes it accurate

Both axes are searched on a grid and then refined sub-grid by parabolic
interpolation through the peak and its two neighbours. This is the decision
that makes the object affordable, and it is worth being explicit about why,
because the two axes get there differently.

**The rate axis** is genuinely coarse. At the certification's geometry, 53
hypotheses span ±5·10⁻⁵, a step of 1.92·10⁻⁶ — and the measured worst error
across five true rates is **0.000 of a step**. The refinement is doing the
work; the grid only has to be fine enough to put the true rate inside the
parabola's basin. The hypothesis count is forced **odd**, so a zero rate lands
on a node rather than straddling two.

**The frequency axis** is refined the same way, but it is also **zero-padded
4×** before the transform — `nfft = 4 · next_pow2(max_len)`. Padding buys no
information, so the reason is not resolution but *conditioning*: parabolic
interpolation assumes the peak is locally parabolic, and it is a better
assumption on a finely sampled main lobe than on a coarse one. The inputs here
are short — preamble partials, symbol streams — which is precisely where a
next-pow2 transform samples the main lobe with too few points for the parabola
to be a good fit.

Measured against the segment's own FFT bin (`1/L` — what a caller gets with no
refinement at all), the error across a full bin is **0.00e+00 bins**,
noiseless, at every one of eight offsets.

> **The 4× is a memory decision too, and it was mis-documented.** `nfft` sizes
> three buffers, so the footprint is four times what "next pow2" suggests. The
> header said next-pow2 for as long as the object existed; a caller budgeting
> from it was out by 4×. Corrected during certification (finding F2) — and the
> reason it is called out here rather than quietly fixed is that the same 4× is
> what buys §3's accuracy. It is a trade, not an implementation detail.

______________________________________________________________________

## 4. The caller strips the modulation

The estimator searches for a *tone* under a chirp. A modulated stream has no
tone, so something must remove the modulation first — and that something is
the caller, deliberately, because only the caller knows which case it is in.

| case               | what the caller does                                   | cost                                              |
| ------------------ | ------------------------------------------------------ | ------------------------------------------------- |
| **data-aided**     | multiply by the conjugate of the known symbols         | none — full SNR retained                          |
| **non-data-aided** | raise an M-PSK stream to the M-th power (BPSK: square) | squaring loss, and the estimate comes back scaled |

The M-th-power trick is the one with a trap in it. Raising to the M-th power
multiplies the phase by M, which multiplies **both** `f` and `r` by M — so a
BPSK caller must halve both results. Putting that halving inside the estimator
would be wrong: the estimator cannot know whether it was handed a squared
stream or a clean tone, and a function that silently halves a correct answer is
worse than one that documents the convention.

______________________________________________________________________

## 5. The envelope

The header's claim was "matched-filter optimal, holds at low SNR". Nothing
measured it until certification, and the envelope is what a caller sizing a
burst preamble actually needs — how long a preamble must be for the estimate to
be good enough is a question about this table, not about the algorithm.

| input SNR (dB) | median error (bins) | worst of 8 (bins) |
| -------------- | ------------------- | ----------------- |
| +30            | 0.0005              | 0.0008            |
| +20            | 0.0004              | 0.0062            |
| +10            | 0.0041              | 0.0183            |
| 0              | 0.0192              | 0.0502            |
| −10            | 0.0486              | 0.1382            |

It **degrades rather than breaks**: at 0 dB input SNR the estimate is still
inside 0.05 of a bin over eight noise draws, and at −10 dB it is inside 0.14.
The estimate stays on the right peak and loses precision, which is the
signature of a coherent search — the failure mode of an *incoherent* one is to
pick a different peak entirely, and there is no graceful version of that.

**`snr_db` is post-integration, and that matters more than it looks.** The
field is a peak-to-mean taken *after* the coherent transform, so it carries the
processing gain: on identical input it grows with segment length (23.9 dB at
L=256, 29.9 dB at L=1024 — quadrupling adds 6.03 dB against the ideal 6.02).
A caller thresholding on it as though it were an input-referred SNR is
comparing an integrated number against an un-integrated one, and will read the
same signal as stronger simply for having handed over a longer segment.

______________________________________________________________________

## 6. Where it sits

`ppe` is the **feedforward** branch of the burst chain. Its counterpart is
[`BurstDespreader`](dsss-burst-receiver.md), which closes carrier and code
loops across the burst — better, when the burst is long enough to converge, and
unavailable when it is not. `BurstDemod` takes the feedforward branch for
exactly that reason, and drives `ppe` once per burst.

The composition it serves, and the `search → refine → demod` shape that
surrounds it, are on the
[DsssBurstReceiver page](dsss-burst-receiver.md). The naming — why
`PolynomialPhaseEstimator` rather than the shorter `ChirpEstimator` — is
§4.2 of the [API Taxonomy](api-taxonomy.md).

______________________________________________________________________

## 7. The C surface

<!-- docs-snippet: skip=declarations quoted from ppe_core.h, not a compilable program -->

```c
typedef struct
{
  double freq_norm; /* frequency, cycles/sample, in [-0.5, 0.5)  */
  double rate_norm; /* chirp rate, cycles/sample^2               */
  double snr_db;    /* winning-row peak-to-mean (rough estimate) */
} ppe_result_t;

ppe_state_t *ppe_create (size_t max_len, double max_rate);
void         ppe_destroy (ppe_state_t *state);
void         ppe_reset (ppe_state_t *state);   /* documented no-op */
ppe_result_t ppe_estimate (ppe_state_t *state, const float _Complex *x,
                           size_t n);
```

`n` must lie in `[4, max_len]`; out of range zeroes every field rather than
returning an error, so a caller that ignores the bound reads zeros rather than
garbage. The object is stateless and by-value — the measure-suite pattern —
and composes `fft_core` plus the `spectral_core` window and peak-finding free
functions rather than carrying its own.
