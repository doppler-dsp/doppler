# Streaming Async Despreader

![Async despreader: oversampled BPSK out](../assets/async_despread_demo.png)

A [`track.Dll`](../api/python-track.md) in **segments** mode — the streaming
DSSS despreader. Its one job is to **remove the PN code and output samples**.
Two things make this the *asynchronous* case, and both are why segments mode
exists:

- the data symbols ride a clock **asynchronous** to the code epoch, so the DLL
    despreads in `K` **coherent** sub-epoch integrate-and-dump segments — a
    segment that straddles a mid-symbol data flip loses amplitude, not phase,
    where a single full-epoch I&D would be corrupted;
- the code Doppler dilates the chip clock, and the power discriminator can't
    pull that rate in on its own at low SNR, so the code rate is supplied by
    **carrier→code aiding** ([`Dll.set_rate_aid`](../api/python-track.md)).

The DLL runs on a **carrier-wiped** stream: the carrier loop is *upstream* (here
a genie de-rotate stands in for the [`Costas`](costas.md) loop that
[`AsyncDsssReceiver`](async-dsss-receiver-spec.md) runs before its DLL). This is
the SPEC geometry: **CCSDS Gold-1023**, 8 samples/chip, code Doppler 2e‑4, data
clock offset 4e‑3 from the code epoch. `K = 11` is
`dll_lookback_segments(1023, 0.5 dB)` — the number of transition-free coherent
segments the 1023 code splits into at the SPEC's tolerable correlation loss, the
same value the receiver's refine/track stages use.

## What you're seeing

**Left — Oversampled asynchronous BPSK out.** The despread segment stream at
`K = 11` samples/symbol, with the `|despread|` envelope (gray) drawn so the
amplitude is explicit. The symbol edges (red, dashed) **slide** through the
segment grid because the symbol clock is independent of the code clock — that is
the "async" in the name. The envelope sits at ≈ 1 except for the **per-symbol
dips**: each async symbol transition lands inside *exactly one* segment per
symbol, which then integrates two opposite-sign data halves and is scaled by
`|2f−1|`. The other `K−1` segments are full. This is the two-clock collapse,
**confined to 1-of-`K`** instead of wiping a whole epoch — exactly what
segmented coherent correlation buys you.

**Middle — Clean despread BPSK.** The same segments in the complex plane: two
tight BPSK clusters at ±1, *not* a smeared ring. The carrier was removed
upstream and the code rate is aided, so nothing rotates within the correlation —
the despread is coherent. (An uncorrected residual carrier is what would smear
these onto a ring; that is the carrier loop's job, done before the DLL, not
after it.)

**Right — Code rate comes from aiding, not the DLL's own loop.** `code_rate`
(the loop's *own* observable, `1 + integrator`) sits at ≈ 1.0 while the aid
supplies the true `1 + 2e-4` code-rate dilation. The DLL isn't rate-tracking the
Doppler — it is being *handed* the rate by the upstream carrier estimate, and
its loop only mops up the residual. (This is the same mechanism
`AsyncDsssReceiver` uses: the pre-despread Costas tracks the full carrier and
refreshes `set_rate_aid` every period.)

*(All three panels above are **noiseless** so the envelope and constellation
stay legible — they are one and the same signal.)*

## Always-on lock detector

![Async despreader: lock detector](../assets/async_despread_demo_lock.png)

A tracking channel must always know whether it is locked, so the DLL carries a
lock detector that reuses *acquisition's* non-coherent statistic. This is a
**separate, noisy experiment** (the despread figure above is noiseless; a lock
statistic is only meaningful against noise), so it gets its own figure on its own
signal — the same carrier-wiped, rate-aided DLL.

It forms `R = √(2·Σ|P|²/E|O|²)` over `N` looks — prompt power over a CFAR noise
reference taken from a **random off-peak correlation** (re-drawn each epoch,
EMA-averaged) — and latches `Dll.locked` when `R` crosses
`det_threshold_noncoherent(pfa, N)`. **Left:** `R` per epoch at several SNRs; with
Gold-1023's ~30 dB despread gain the signal traces sit far above the threshold
even when very weak, while the noise-only trace hugs `√(2N) ≈ 6.3` below it.
**Right:** the noisy despread output behind the "weak" trace — the BPSK is still
recoverable and the detector reports lock on it. Because the noise reference rides
an EMA much longer than the `N`-look test (and is cumulative-mean-bootstrapped so
it is unbiased from the first look), the false-alarm rate holds at the target
`pfa` (default `1e-3`) from the start. The statistic and threshold are the *same*
ones the FFT acquisition uses, so acquire and track agree on "detected".

## How it works

`Dll(segments=K)` splits each code epoch into `K` **coherent** segment
integrate-and-dumps. Each segment despreads `TE/K` samples against the local
code; the `K` segments per epoch are an oversampled view of the symbol (≈ `K`
samples/symbol when the symbol rate is near the code rate). Code tracking folds
each segment's early/late envelopes into a **non-coherent** epoch sum `Σ|E_k|`,
`Σ|L_k|` — a data flip changes a segment's *sign*, not its *magnitude*, so only
the one straddling segment degrades. The code-rate dilation, which the
discriminator alone can't pull in, arrives as a per-epoch `rate_aid` bias from
the upstream carrier loop.

```python
--8<-- "src/doppler/examples/async_despread_demo.py:signal"
```

```python
# CCSDS Gold-1023; sps samples/chip; segments = K coherent windows per epoch
code = np.asarray(Gold().generate(SF)).astype(np.uint8)
rxw = carrier_wipe(make_signal(code)[0])   # carrier removed upstream

d = Dll(code, sps=SPS, init_chip=0.0, bn=0.002, zeta=0.707, spacing=0.5,
        segments=K)
d.set_rate_aid(DCODE)     # carrier->code aiding supplies the code-rate dilation
part = d.steps(rxw)       # oversampled async BPSK out (PN removed), clean BPSK
rate = d.code_rate        # the loop's own observable (~1.0; the aid does the rate)

# always-on lock detector (acquisition's non-coherent test):
d.configure_lock(pfa=1e-3, n_looks=20)   # size n_looks via detection.det_n_noncoh
if d.locked:              # latched each n_looks-look decision
    print(d.lock_stat, d.noise_est)      # statistic R and the CFAR noise ref

# downstream — symbol-timing recovery on the already-carrier-clean segments:
from doppler.track import SymbolSync

ss = SymbolSync(sps=K, bn=0.02, zeta=0.707)
syms = ss.steps(part)            # -> one decision per recovered symbol
bits = np.where(syms.real >= 0, 1, -1)
assert bits.shape == syms.shape
```

The short coherent segment window is also what keeps the despread robust to the
residual the upstream carrier loop hasn't yet nulled: for a ½-Doppler-bin
residual the integrate-and-dump loss is small at `K = 11` (versus a full-epoch
`segments = 1` prompt, which a mid-epoch data flip corrupts outright).
`steps()` is block-size invariant and returns an independent array per call, so a
receiver can stream blocks and keep every one.

Source: `src/doppler/examples/async_despread_demo.py`.
See also the design note *Async Symbol Despreader*, the
[AsyncDsssReceiver — the SPEC Waveform](async-dsss-receiver-spec.md) gallery page
for the full carrier + code + aiding chain end to end, the
[Full-Chain Lock-Up](receiver-lock.md) page for the
`Dll -> Costas -> SymbolSync` chain converged and lock-observed via telemetry,
and the [lock-detection guide](../guide/lock-detection.md) for how `Dll`'s lock
detector here fits alongside every other loop's.
