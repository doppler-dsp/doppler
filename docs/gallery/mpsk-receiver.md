# M-PSK Receiver — Pull-in, Lock, and BER

![MpskReceiver constellation pull-in, carrier acquisition, and BER](../assets/mpsk_receiver_demo.png)

[`track.MpskReceiver`](../api/python-track.md) is a complete M-PSK modem, and it
owns no filter, no NCO and no interpolator of its own. It is a **matched
down-converter with two loops closed around its two control ports**: a
[`MatchedDDC`](ddc-fn.md) mixes, decimates and matched-filters in the dot
products it was already doing, a carrier loop steers its LO (`freq_ctrl`), and
[RateSync](ratesync.md)'s own timing loop steers its terminal accumulator
(`rate_ctrl`). The terminal polyphase stage's **bank is the matched filter**
(integrate-and-dump by default, or root-raised-cosine for band-limited links) and
the **arm it selects is the fractional symbol-timing delay**. Carrier recovery
follows the project rule, now structurally rather than by convention:
**predetection de-rotation** in the LO at the front of the chain, and
**postdetection discrimination** on the matched-filtered symbols at the end of
it. See the [design note](../design/mpsk.md).

Because the front end plans its own cascade, `sps` is a **double** and an
irrational samples-per-symbol — a free-running ADC clock against the symbol clock
— is no harder than an integer one. The matched filter costs ~34 taps/arm at
`sps = 8` and at `sps = 256` alike, against the ~4225 taps/arm a single-stage
design would need at the top of that range.

[`track.MpskReceiverR`](../api/python-track.md) is the **real-input twin**: same
loops, same handover, same demapper — literally the same implementation — with a
`MatchedDdcr` front end so it takes a real IF (`f32`) instead of complex
baseband. It is a separate type rather than a flavor because `steps()` takes a
different dtype, and a shared class would have to name the dtype in a method.

**Left — Constellation pull-in.** A QPSK signal with a `0.0015` cycles/sample
carrier offset, with the loop seeded at zero. During acquisition (red) the
de-rotated symbols sweep a ring — the residual carrier is still rotating them;
once the non-data-aided loop pulls the LO onto the offset, the symbols (blue)
collapse onto the four QPSK clusters. **No data aiding** is required to acquire —
that is what the M-th-power discriminator buys. Note that the old engine needed no
*symbol timing* either; the rebuilt one does, because its discriminator runs on
the on-time strobe.

**Middle — Carrier acquisition + lock.** The tracked frequency (green) snaps from
zero onto the true offset (black dashed) within tens of symbols, and the lock
metric (purple) rises and holds. The metric is `Re(z^M)` on the de-rotated
symbols, normalised so that **it reads ≈ 1.0 at lock for every M** (measured
1.00 / 1.02 / 1.05) — one threshold means one thing at every order, and
`lock_thresh` is a plain fraction of what a locked constellation reads. That is
what the opt-in `acq_to_track` switch thresholds on.

**Right — BER vs Es/N0.** Bit error rate against the coherent M-PSK bound, using
NDA acquisition followed by decision-directed tracking (`acq_to_track=1`). BPSK
and QPSK track the bound within ~1–2 dB. **8PSK shows an acquisition threshold**:
the 8th-power discriminator's phase noise is large at low SNR, so the loop does
not pull in until ~13–14 dB — above which decision-directed tracking takes over
and it falls to the bound. This is the fundamental cost of non-data-aided 8PSK
acquisition; a known preamble or external frequency aid removes the threshold.

## Acquisition-to-tracking switch — acquire blind, track clean

By default (`acq_to_track=0`) the receiver stays in robust NDA tracking the
whole time. Enabling the switch hands the **shared LO** from the M-th-power
discriminator to a lower-jitter **decision-directed** error `e = Im(y·conj(â))/|y|`
on the recovered symbols once the loop has locked and a warmup has elapsed —
essential for 8PSK, whose M-th-power phase noise would otherwise cross the ±π/8
decision margins. Both discriminators run on the same on-time strobe at the
symbol rate, so the handover is a plain discriminator swap: the shared loop
filter carries the frequency estimate across it in both directions, which makes a
drop-back a swap rather than a cold re-acquisition.

The drop-back is real, not decorative — the discriminator and the lock metric
keep running while tracking, so a receiver that loses its signal falls back to
NDA acquisition instead of sitting in `tracking` forever on stale data.

```python
--8<-- "src/doppler/examples/mpsk_receiver_demo.py:receiver"
```

## Resolving the M-fold ambiguity — differential bits

The carrier loop locks to one of `m` phases, so the absolute constellation
orientation is ambiguous. `bits(..., differential=1)` decodes each symbol from
the phase **difference** between consecutive symbols, which is invariant to an
unknown constant carrier phase:

```python
import numpy as np
from doppler.track import MpskReceiver

rng = np.random.default_rng(0)
syms = np.exp(2j * np.pi * rng.integers(0, 8, 2000) / 8)
iq = np.repeat(syms, 8).astype(np.complex64) * 0.5   # 8PSK, 8 samples/symbol

rx = MpskReceiver(m=8, sps=8, differential=1)
bits = rx.bits(iq)           # rotation-invariant; survives any fixed phase slip
assert len(bits) % 3 == 0                            # log2(8) = 3 bits/symbol
# The cascade eats the first couple of symbols as group delay -- how many
# depends on m_out, so bound it rather than pinning it.
assert 3 * (len(iq) // 8 - 4) <= len(bits) <= 3 * (len(iq) // 8)
```

## A real IF instead of complex baseband

`MpskReceiverR` takes `float32` samples of a real bandpass signal and tunes it
down itself. Its one extra constraint is `sps > 2·m_out`: the cascade behind the
R2C halfband runs at twice the overall rate.

```python
import numpy as np
from doppler.track import MpskReceiverR

m, sps, fc = 4, 24.0, 0.10          # QPSK, real IF at 0.10 cycles/sample
rng = np.random.default_rng(3)
syms = np.exp(2j * np.pi * rng.integers(0, m, 4000) / m)
bb = np.repeat(syms, int(sps))                      # I&D (rectangular) pulses
n = np.arange(len(bb))
rf = (bb * np.exp(2j * np.pi * fc * n)).real.astype(np.float32) * 0.5

rx = MpskReceiverR(m=m, sps=sps, m_out=4, init_norm_freq=fc, bn_carrier=0.002)
out = rx.steps(rf)

print(f"{len(out)} symbols, lock {rx.lock:.2f}")
assert len(out) > 3000            # ~ one symbol per sps input samples
assert rx.lock > 0.4              # normalised: ~1.0 at lock, every M
```

Read `rx.lock`'s **sign** as well as its magnitude: a steady negative lock is the
signature of an inverted carrier error, not a weak one. The magnitude means the
same thing at every M — see how its threshold is derived below.

### Where `lock_thresh` comes from — a Pfa, not a guess

The lock statistic is `Re((z/|z|)^M)`: the M-th power of a **limited** sample,
smoothed by an EMA. The limiter is on this path only — the phase error keeps its
raw `|z|^M` weighting, which is the natural matched weighting on a pulse-shaped
signal. Limiting the *lock* signal is what makes it a detector you can put a
number on, because under H0 (no carrier) the phase is uniform and so

```text
Var[Re(e^{jMtheta})] = 1/2   for EVERY M
```

One threshold is therefore one false-alarm probability at every constellation
order. The whole chain is derived, none of it picked:

| quantity  | value  | from                                              |
| --------- | ------ | ------------------------------------------------- |
| `α` (EMA) | 0.05   | `det_ema_alpha(0.0, 15.9)` → `N_eff = 39` looks   |
| `σ_H0`    | 0.1132 | `sqrt(½·α/(2−α))`, analytic — measured **0.1132** |
| `0.5`     | 4.42 σ | per-look Pfa = `Q(4.42)` = **5.0e-6**             |

Measured on noise only, 200 trials × 4000 symbols, against that analytic
`σ_H0`:

| M    | noise mean | noise σ    | max seen | over 0.5 |
| ---- | ---------- | ---------- | -------- | -------- |
| BPSK | +0.006     | **0.1133** | +0.342   | 0 / 200  |
| QPSK | +0.004     | **0.1071** | +0.292   | 0 / 200  |
| 8PSK | −0.009     | **0.1138** | +0.358   | 0 / 200  |

and end to end, with `acq_to_track=1` over 100 noise-only runs of 20 000 symbols
each, `tracking` went high **0/100 times at every order** — peak lock 0.371 /
0.467 / 0.376 against the 0.5 threshold.

!!! note "What the limiter bought"

    Without it the statistic is unbounded and its H0 variance depends on M, so the
    *same* `lock_thresh = 0.5` was 4.4 σ at BPSK, 0.9 σ at QPSK and 0.02 σ at 8PSK
    — one number meaning three different Pfas, one of them meaningless.
    Detectability `d' = (μ_H1 − μ_H0)/σ_H0` at Es/N0 = 10 / 20 dB, raw → limited:

    | M    | raw         | limited         |
    | ---- | ----------- | --------------- |
    | BPSK | 5.70 / 6.21 | 7.95 / 8.75     |
    | QPSK | 1.50 / 1.78 | 5.81 / 8.47     |
    | 8PSK | 0.02 / 0.04 | 1.76 / **7.52** |

    The limiter *costs* H1 — it discards the `|z|^M` boost that helps at low SNR —
    and wins anyway at every M and every Es/N0, because it cuts H0's variance by
    far more than it cuts H1. With the raw form only BPSK ever cleared a 1e-3 Pfa,
    so for M ≥ 4 there was no Pfa-derived threshold to be had at all.

!!! warning "`0.5` is calibrated for the `strobe` tap"

    `σ_H0` is tap-independent, but the value at lock is not: a tap that averages
    badly-timed samples has small `|z|` on some of them, and limiting promotes
    those to full weight. Measured at Es/N0 20 dB, `m_out = 8`, with all nine
    combinations decoding at SER 0.0000, the *lock reading* is

    | tap      | BPSK   | QPSK   | 8PSK       |
    | -------- | ------ | ------ | ---------- |
    | `strobe` | +0.989 | +0.956 | +0.862     |
    | `mf_out` | +0.904 | +0.384 | **+0.209** |

    So on `mf_out` at higher M the receiver can decode perfectly and
    still not declare, because 0.5 sits above where that tap's statistic settles.
    Scale `lock_thresh` by the reading for your tap and order — the Pfa mapping
    (`thresh / 0.1132` in σ) is unchanged; you are only trading margin.

### `nda_tap` — buying pull-in range back

The carrier discriminator's tap point sets how much frequency error it can even
*see*: an M-th-power detector updating at rate `F` is unambiguous only for
`|Δf| < F/(2M)`. Reading the on-time strobe is the cleanest input and the
narrowest range; two wider taps trade signal quality for range, and both are
also **timing-independent**, which the strobe tap is not.

| `nda_tap`          | Update rate | Max acquired `Δf` (QPSK, `sps=8`) | Needs timing? |
| ------------------ | ----------- | --------------------------------- | ------------- |
| `strobe` (default) | `Rs`        | `0.01·Rs`                         | yes           |
| `mf_out`           | `m_out·Rs`  | `0.02·Rs`                         | no            |
| `mf_in`            | `bank_sps`  | not yet measured                  | no            |

The tap is fixed at construction, so nothing switches underneath you:

```python
rx = MpskReceiver(m=4, sps=8, m_out=4, bn_carrier=0.05, nda_tap="mf_in")
```

!!! note "`mf_in` is not characterised on this page yet"

    It reads the MFR's input and replaced `lo_arm`, which was removed with the
    Costas arm filter it depended on (gh-768). Its update rate is the
    cascade's `bank_sps` — a planner outcome rather than a fixed multiple of
    `Rs` — so its row cannot be filled in from the others by argument, and it
    is left blank rather than guessed. `native/validation/rx_nda_tap.c` gates
    that it acquires at every rate ratio; the pull-in **range** measurement
    that would fill this cell is gh-766.

`bn_carrier` keeps its meaning at every tap (symbol-rate normalised). The tap
does not widen the loop by itself — it widens what the discriminator can see and
improves the stability margin, which is what lets you then raise `bn_carrier`.

!!! warning "`mf_out` needs `m_out = 8` — it is the tap that pays for a coarse strobe"

    Measured at Es/N0 20 dB, `sps = 8`, `bn_carrier = 0.005`, median SER / EVM over
    5 seeds, with and without the handover:

    | tap      | `m_out=8` (default), any M | `m_out=4`, QPSK | `m_out=4`, 8PSK         |
    | -------- | -------------------------- | --------------- | ----------------------- |
    | `strobe` | SER 0, −19.7 dB            | SER 0, −15.9 dB | SER 0.002, −15.9 dB     |
    | `mf_out` | SER 0, −19.7 dB            | SER 0, −16.0 dB | **SER 0.851, −11.9 dB** |

    **At the default `m_out = 8` all three taps decode every order cleanly.** Every
    failure in this table lives at `m_out = 4`, and it is `mf_out` that fails —
    which is what the `Σ g_k^M` gain-collapse argument actually predicts, because
    `mf_out` is the tap that averages the M-th power over **all `m_out`
    matched-filter outputs**, including the badly-timed ones. Halving `m_out`
    halves how much of each symbol those arms cover, and at 8th power that is
    fatal.

    The decode failure is `Σ g_k^M`; it used to come with a **false lock** on top,
    and that part is fixed. At `m_out = 4`, `mf_out`/8PSK reported +0.94 (+3.90 at
    `bn_carrier = 0.05`) while decoding at chance; since the lock statistic was
    limited it reports **−0.069** on the identical failure — correctly not locked.
    `mf_out` + `acq_to_track` at *QPSK* recovered with it, from 2/5 decodes (SER
    0.295) to **5/5** (SER 0.0000), because the handover is no longer triggered by
    a statistic that meant nothing.

    This box once named a different tap as the failing one, on numbers taken
    with a lag search clipped to ±30 and a window inside the settling transient
    — the two defects fixed in `30c76c6d`. Both report chance SER on a healthy
    decode, which is how a measurement bug comes to read as a DSP defect.

!!! danger "`Δf = k·F/M` is a stable false lock, at every tap"

    `F/M` is where the M-th power aliases onto zero, so the M-fold ambiguity is a
    **frequency** ambiguity as well as a phase one. Measured on QPSK with an
    initial error of `Rs/4`: the loop never moves (tracked frequency 2e-6 against
    a true 0.03125) and still reports a lock of **+0.83** against the ≈ 1.0 it
    reads at a real lock, with a stationary constellation — so EVM and blind M2M4
    both look clean too. **No self-referenced metric catches
    this**; it takes an external frequency reference or a sync word. A faster tap
    pushes the alias out proportionally.

    Beyond any tap's range, put a coarse frequency estimate in front (an FFT
    sweep, or `dsss.Ppe`) and pass it as `init_norm_freq`.

## Don't trust one metric — least of all a bit error rate

A bit error rate is truth-referenced: it needs the transmitted symbols *and* a
lag/polarity alignment search, which makes it fragile in both directions (a
too-narrow lag window reports chance on a perfectly good receiver; a wide one on
a short record can find a lucky alignment on garbage). Pair it with two
validators that need neither truth nor lag:

- **self-referenced EVM** — each symbol against its *own* hard decision. At lock
    this sits at `−(Es/N0)` dB; EVM is an I/Q-plane quantity, so there is no
    factor of two.
- **blind M2M4 SNR** — [`snr_m2m4_db`](../api/python-snr.md), moment-based.

Their **disagreement** is what carries the diagnosis. M2M4 uses only `|x|²` and
`|x|⁴`, so it is **rotation-blind**: a constellation that is cleanly shaped but
spinning still reads a healthy M2M4 while EVM collapses. A healthy M2M4 beside a
collapsed EVM therefore says *the amplitudes are fine, the phase is not* — which
no error rate could have told you. In the run above they agree within ~0.5 dB
(EVM −23.8 dB, M2M4 +24.1 dB), which is what a real lock looks like. In C, both
helpers are shared in `native/tests/dp_sym_test.h`.

## DSSS-MPSK — chain after a despreader

A spread-spectrum M-PSK receiver is just a despreader feeding this modem: the
[`Dll(segments)`](async-despread.md) streaming despreader collapses each PN epoch
to one symbol-rate soft chip, and `MpskReceiver` recovers carrier, timing, and
bits on that stream — `Dll(segments) → MpskReceiver`.

## Reproduce

```bash
python src/doppler/examples/mpsk_receiver_demo.py   # → mpsk_receiver_demo.png
```
