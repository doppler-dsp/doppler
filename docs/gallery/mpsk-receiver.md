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

!!! warning "The figure below predates the cascade rebuild"

    It shows cold pull-in (`init_norm_freq=0`), which the rebuilt engine no longer
    does reliably — see the two boxes at the end of this page. The plots are kept
    because the *shape* of acquisition, lock and the BER curve is unchanged, but
    the demo that produced them currently fails its own assertion and has
    deliberately not been retuned to pass.

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
metric (purple) rises and holds. The lock metric is orientation-normalised, so it
reads `~+` at lock for every M (BPSK ≈ 1, QPSK ≈ 0.62, 8PSK ≈ 0.41) and is what
the opt-in `acq_to_track` switch thresholds on.

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

<!-- docs-snippet: skip=gh#536 cold carrier pull-in regressed by the cascade rebuild; this block asserts rx.tracking == 1 and fails for ~1/3 of data seeds -->

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
assert len(bits) == 3 * (len(iq) // 8 - 3)           # log2(8) = 3 bits/symbol
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
assert rx.lock > 0.4              # QPSK locks toward the 0.62 ceiling
```

Read `rx.lock`'s **sign** as well as its magnitude: a steady negative lock is the
signature of an inverted carrier error, not a weak one, and the per-M ceilings
(BPSK ≈ 1, QPSK ≈ 0.62, 8PSK ≈ 0.41) are ceilings — a lock statistic *above* its
ceiling means the loop gain is wrong, not that the lock is unusually good.

!!! bug "Cold pull-in is currently unreliable — seed `init_norm_freq` if you can"

    Since the cascade rebuild the carrier loop **acquires reliably when it is
    seeded** (`init_norm_freq` set at or near the true offset, so the loop only
    has to hold it) but **not from a cold start** (`init_norm_freq=0`, loop must
    find the offset). Measured on a clean, noiseless QPSK signal at `sps=8`, cold
    acquisition succeeds in at best 4 of 6 data seeds, and widening `bn_carrier`
    past 0.02 makes it worse rather than better — so this is not simply a pull-in
    range limit. It fails even at offsets small enough that pull-in range cannot
    be the explanation.

    Until it is fixed: **seed `init_norm_freq`** from a coarse frequency estimate
    if you have one, keep `bn_carrier` modest (`0.002` is a good starting point
    for a seeded loop), and **verify acquisition with a truth-free metric** (see
    below) rather than assuming it. Note the example above seeds `init_norm_freq`
    for exactly this reason.

    Tracked as
    [doppler-dsp/doppler#536](https://github.com/doppler-dsp/doppler/issues/536),
    which also records a related `sps`-dependent instability: because the cascade
    plan changes discontinuously with the rate, the largest stable `bn_carrier` is
    not a smooth function of `sps`.

!!! danger "The pull-in range is now below `Rs`, and `Δf = k·Rs/M` false-locks"

    The discriminator moved from the sample rate to the symbol rate, so the
    frequency error it can even *observe* shrank by a factor of `sps`: an
    M-th-power detector is unambiguous only for `|Δf| < F/(2M)` at its update rate
    `F`, which was `fs/(2M)` and is now `Rs/(2M)`. **Carrier pull-in is now well
    below the symbol rate, where before it was above it** — measured ~0.01·`Rs`.

    And `Rs/M` is exactly where the symbol-rate M-th power aliases onto zero, so
    the M-fold ambiguity is now a **frequency** ambiguity too. At `Δf = k·Rs/M`
    the loop sits still and still reports a healthy lock (+0.546 against the 0.62
    QPSK ceiling, measured), with a stationary constellation — so EVM and M2M4
    both look clean as well. **No self-referenced metric can catch this**; it
    takes an external frequency reference or a sync word.

    For a wider acquisition range, put a coarse frequency estimate in front (an
    FFT sweep, or `dsss.Ppe`) and pass it as
    `init_norm_freq`. That is what the parameter is for — no loop bandwidth
    reaches it.

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
