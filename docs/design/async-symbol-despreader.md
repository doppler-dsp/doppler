# Asynchronous symbol/code despreading

**Status:** draft / validated architecture
**Scope:** the receive-side despreader when the **data-symbol rate is on the
order of the code-epoch rate but asynchronous** to it. This is theory, the
failure mechanism, and a validated robust architecture that composes existing
`doppler.track` primitives. The reproducible study is
`src/doppler/examples/async_despreader_study.py`
(`python -m doppler.examples.async_despreader_study`).

______________________________________________________________________

## 1. The two-clock problem

A DSSS receiver despreads by integrating early/prompt/late correlations over one
**code epoch** (`TE = sf·sps` samples) — an integrate-and-dump locked to the
*code* clock. The data symbols are a separate stream; the despread prompt per
epoch carries the data.

That works when the symbol clock is locked to the code clock at an integer ratio
(GPS C/A: 20 code epochs per data bit, bit edges on epoch edges). It **breaks**
when the symbol clock is *independent*:

```
T_sym = TE · (1 + delta)        # symbol period, samples
                                # delta = symbol-vs-code rate offset
phi_sym                         # independent symbol phase
```

with `T_sym ≈ TE` (symbol ≈ one epoch). This is the hard regime: ~one symbol per
epoch, a transition roughly every epoch, and — crucially — `delta ≠ 0` makes the
symbol boundary **slide continuously** through the epoch at the beat rate
`delta / TE`.

______________________________________________________________________

## 2. Why per-epoch despreading fails

The coherent prompt over an epoch whose data flips at fraction `f ∈ [0,1]`:

```
P(f) = A·[ f·d1 + (1−f)·d2 ]  =  A·d1·(2f−1)        (d2 = −d1)
```

- `f → 0, 1` (flip at an epoch edge): `|P| = A` (full despread).
- `f → 0.5` (flip mid-epoch): **`|P| = 0`** — total coherent cancellation.

Because `delta ≠ 0`, `f` sweeps through every value, so ~half of all epochs
straddle a transition and their prompts collapse. The consequences:

1. **Data**: per-epoch decisions floor — the BER plateaus regardless of `Es/N0`
    (the straddle epochs carry no usable energy). Measured floor ≈ 1e-1 even when
    the bound is < 1e-5.
1. **Code**: the early/late discriminator `(|E|−|L|)/(|E|+|L|)` collapses to
    `0/0` on straddle epochs → the DLL is starved → the code loop wanders.

**Root cause:** at one prompt per epoch the symbol clock is **unobservable** (a
single sample per symbol cannot drive a timing loop), and the integration window
is forced to straddle transitions.

### Diagnostic fingerprint

The straddle modulation is periodic at the symbol↔epoch beat. The spectrum of
the prompt-magnitude stream `|P[n]|` shows a **tone at `|delta|` cycles/epoch**
(centre panel of the figure). This is the signature to look for when a DSSS link
shows unexplained despread fades — it identifies this failure class directly.

______________________________________________________________________

## 3. Robust architecture

![Async despreader study](../assets/async_despreader_study.png)

The fix gives the symbol clock its own observability and its own matched filter,
and makes code tracking insensitive to data sign — composing primitives that
already exist.

### 3.1 Data path — partial correlations + symbol matched filter + SymbolSync

1. **Partial correlations.** Split each code epoch into `K` sub-epoch partial
    prompt correlations (each `TE/K` samples, known code phase). This yields `K`
    despread samples per epoch ≈ `K` samples per symbol — the symbol clock is now
    **observable**.
1. **Symbol matched filter.** A length-`K` **boxcar** over the partial stream.
    This is a *sliding, symbol-aligned* coherent re-integration of the partials —
    the full-symbol despread the epoch-locked window could not form. It is
    essential: without it, the rectangular symbol pulse is sampled at one point
    and only ~1/`K` of the symbol energy is captured (the BER floors at ~2e-2).
1. **SymbolSync.** [`track.SymbolSync`](../api/python-track.md) (Gardner TED +
    Farrow interpolator) recovers the independent symbol clock (`delta`, `phi`)
    from the matched-filtered stream and decimates at the symbol-aligned peak.

**Result (left panel):** the BER follows the BPSK matched-filter bound within
~1–2 dB. A **genie** reference (coherent symbol-aligned despread with *known*
timing) hits the bound exactly — the loss was only window misalignment, never
SNR. The broken per-epoch path floors.

| Es/N0  | bound  | genie (known timing) | partial+MF+SymbolSync | broken epoch |
| ------ | ------ | -------------------- | --------------------- | ------------ |
| 6 dB   | 2.4e-3 | 2.5e-3               | 4.5e-3                | ~7e-2        |
| 8 dB   | 1.9e-4 | 1.5e-4               | 5.8e-4                | ~6e-2        |
| 9.6 dB | 9.7e-6 | 0                    | 0                     | ~5e-2        |

### 3.2 Code path — non-coherent partial combining

The DLL keeps tracking through data flips by combining the partial correlations
**non-coherently**: `|E| = Σ_k |E_k|`, `|L| = Σ_k |L_k|`. A data flip changes a
partial's *sign*, not its *magnitude*, so only the one straddling segment
degrades (~`1/K`). This roughly **halves the discriminator variance** versus the
coherent-epoch form (right panel) — keeping the (already validated, smooth
sub-chip) code loop locked. It needs no symbol timing, so it works from cold
start; the bootstrap order stays sequential: DLL (non-coherent) → SymbolSync →
data.

### 3.3 Choosing K

`K` trades observability and straddle-robustness against the non-coherent
squaring/Rician bias (which erodes the discriminator gain as `K` grows). The
study shows **`K = 4` as the sweet spot** for `T_sym ≈ TE` (best discriminator
SNR; `K = 8` loses more gain than variance). `K` must divide `TE`.

______________________________________________________________________

## 4. Scope: the despreader removes the code and outputs samples

The despreader's one job is to **remove the PN code and output samples**. The
asynchronous symbol clock is merely *why* it despreads in `K` partial
correlations (§3) — it is not a reason to recover symbols here. **Carrier
recovery and symbol extraction are downstream problems**, handled by separate
objects fed from the despreader's output:

```
              ┌──────────────── the despreader ───────────────┐
acq seed →    Dll(segments=K):  E/P/L correlate · partial dump · non-coherent
   (code phase)                 (|E|−|L|) code loop
              └───────────────── partial stream out ──────────┘
                         │  K oversampled async BPSK samples/symbol
                         │  (PN removed; residual carrier + data still on them)
                         ▼
   downstream:  Costas (carrier recovery)  →  SymbolSync (symbol timing) → bits
```

This is **`track.Dll(..., segments=K)`** — no new object. `segments=1` is the
classic coherent full-epoch DLL; `segments=K>1` is the streaming async
despreader. It composes downstream with `Costas` and `SymbolSync`, which already
exist (the data path of §3 is exactly that composition).

### Why the carrier belongs downstream

The DLL's `|E|−|L|` discriminator is **non-coherent**, so code tracking is
**carrier-blind** — it locks with a residual carrier still on the samples. And
because each output is a *partial* (a `TE/K`-sample integrate-and-dump, not a
full epoch), a residual carrier barely dents it. For a ½-Doppler-bin residual
after acquisition the I&D loss is `sinc(Δφ/2)` with `Δφ = π/segments`:

| segments | window | Δφ at ½-bin residual | despread loss |
| -------- | ------ | -------------------- | ------------- |
| 1        | `TE`   | `π`                  | **−3.9 dB**   |
| 4        | `TE/4` | `π/4`                | **−0.2 dB**   |

So short partials make the despread carrier-tolerant: the small residual just
rides out on the output (a ring in the constellation; see the gallery demo), and
a downstream `Costas` loop removes it at full symbol SNR. Putting a carrier loop
*inside* the despreader would only matter for long coherent integration — which
partials deliberately avoid.

## 5. Code-lock detection (always on)

A tracking channel must always answer one question: *am I locked?* The DLL
carries an **always-on** lock detector that reuses **acquisition's** non-coherent
test statistic, so acquire and track agree on what "detected" means.

**Statistic.** Each emitted look (a partial in `segments` mode, the full-epoch
prompt when `segments=1`) contributes its prompt power `|P_k|²`. The detector
sums `N = n_looks` consecutive looks and forms

```
R = sqrt( 2 · Σ_{k=1}^{N} |P_k|²  /  E|O|² )
```

which under H0 (noise only) has `P(R > η) = marcum_q(N, 0, η)` — exactly the
acquisition tail. So a caller sizes the threshold `η = det_threshold_noncoherent(pfa, N)` and the depth `N = det_n_noncoh(snr, …)` to
meet a target `(Pfa, Pd)`; `configure_lock(pfa, n_looks)` does the conversion
(default `pfa=1e-3`, `N=20`).

**The noise reference `E|O|²`.** Instead of a separate noise channel, the loop
correlates each look a second time at a **random off-peak code phase** — a whole
chip offset re-drawn every epoch and kept clear of the prompt/early/late lobe by
`noise_guard` chips. For a low-sidelobe code (Gold, long PN) that offset
correlation is signal-free, so `|O_k|²` is a sample of the per-look noise power.
Cycling the offset and averaging recovers the same noise estimate a bank of
fixed off-peak taps would, with O(1) state.

**Why an EMA, and why it must be long.** The reference is an EMA of `|O_k|²`
(`E|O|² += α(|O_k|² − E|O|²)`), which is adaptive (tracks a drifting noise floor)
and O(1) — matching the `Costas` lock-metric pattern. The subtlety, found by
Monte-Carlo: the *detection* integrates a fixed `N` looks (that sets the χ²(2N)
threshold), but the *noise estimate* must average **many more** cells than `N`,
or its own variance inflates Pfa. One offset cell per look (`L=N`) drives Pfa
~400× high; `1/α = max(1024, 32·N)` (`L_eff ≫ N`) holds Pfa at target with
`Pd ≈ 0.98`. So the integration depth and the noise-averaging length are
**decoupled**: `N` is the test, `1/α` is the reference. The reference uses a
**cumulative-mean bootstrap** — it is the running average until `1/α` looks have
accrued, then relaxes to the fixed-α EMA — so the noise floor is unbiased from
the first look instead of seed-dominated for the ~`1/α`-look warm-up (otherwise
Pfa runs ~10× high until the EMA settles, ~hundreds of epochs in). Verified
end-to-end: empirical Pfa ≈ `9e-4` against the `1e-3` target right from the
start of a noise stream.

**Readouts.** `Dll.locked` (bool, latched each `N`-look decision), `Dll.lock_stat`
(the last `R`), `Dll.noise_est` (`E|O|²`). The detector runs inside the normal
`steps()` — no separate method, no opt-in. The threshold conversion (the one
`detection`-module call) lives in the binding so `dll_core` links only `-lm`.

## 6. Status

- **Shipped — the despreader.** `Dll(..., segments=K)` (the §3 code+symbol path;
    `segments=1` = the classic coherent DLL). Validated **carrier-present**: code
    lock holds with a residual carrier on the samples, and the partial output is
    losslessly recoverable by a downstream carrier wipe + symbol despread
    (`test_dll.py::test_segments_carrier_present_*`). The streaming binding returns
    an independent array per call (block-size invariant).
- **Shipped — the inline symbol-loop primitive.** `symsync_step()` (the
    per-sample SymbolSync composition API); `symsync_steps()` is it in a loop.
- **Shipped — the always-on code-lock detector** (§5). `Dll.locked` /
    `lock_stat` / `noise_est`, tuned by `configure_lock(pfa, n_looks)`; reuses
    acquisition's non-coherent statistic with a random off-peak EMA noise
    reference. Validated signal-vs-noise in `test_dll.py` / `test_dll_core.c`.
- **Downstream, already available:** `Costas` (carrier recovery) and
    `SymbolSync` (Gardner + Farrow symbol timing). A receiver is the pipeline
    `Dll(segments) → Costas → SymbolSync`; the §3 study and the
    `async_despread_demo` gallery example show the composition.
- **End-to-end validated with a real acquisition front end.**
    `Dll(segments=K) → MpskReceiver` (`MpskReceiver` already fuses matched
    filter + NDA carrier acquisition + Gardner/Farrow timing + acq↔track
    handover into one object — its own docstring names this exact
    composition) is now proven at real physical parameters — a continuous
    1023-chip code at 3 Mchips/s, async 2100 sym/s BPSK data, with a genuine
    `Acquisition` search in front (see the
    [Continuous Async DSSS Receiver](../gallery/async-dsss-receiver.md)
    gallery page, `src/doppler/examples/async_dsss_receiver_demo.py`).
    One correction to this section's own guidance surfaced doing so: `K=4`
    (§3.3's sweet spot) is tuned for the DLL's own code-discriminator
    variance, not for feeding a downstream matched filter — each partial is
    `K`-times weaker than a full coherent epoch, so a downstream receiver
    needs a much larger `K` (34, in the validated example) to reconstruct
    real coherent gain before its own carrier/timing loops can converge.
    The acquisition hand-off also needs two non-obvious unit conversions
    (`Dll`'s `init_chip` is phase-*inverted* relative to `Acquisition`'s
    `code_phase`; `MpskReceiver`'s `init_norm_freq` is cycles per its own
    *partial-rate* input, not per raw ADC sample) — see the example's
    docstring for the exact formulas.

### Possible refinements

- **Symbol MF length.** A downstream length-`K` boxcar matched filter follows the
    BPSK bound within ~1–2 dB; matching it to the *tracked* symbol period closes
    the gap.
- **Closed-loop code-jitter asset.** Drive the non-coherent partial code loop
    under async data + code Doppler; confirm lock retention and the low-SNR
    threshold (`bn≈1e-5` held to 4 dB Es/N0; `bn≈0.002` lost lock at 6 dB).
