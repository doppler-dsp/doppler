# Continuous Async DSSS Receiver

![Constellation, running BER, before/after resample comparison, and carrier pull-in](../assets/async_dsss_receiver_demo.png)

Stage 3 of the multi-part story that began with
[DSSS Acquisition: Continuous Async-Data Modulation](dsss-acq-async-data.md)
(Stage 1 — does `Acquisition` land the right code phase/Doppler bin) and
[DSSS Despread: Continuous Async-Data Hand-off](dsss-despread-async-data.md)
(Stage 2 — does that hit correctly seed `Dll`, and is `segments=4` robust
enough for the DLL's *own* tracking loop). This page asks the last
question the story set up: carrier and symbol-timing recovery
([`MpskReceiver`](../api/python-track.md)) sit downstream of `Dll` too —
does Stage 2's own `segments=4` sweet spot suffice there as well?

## The despreader's only job is to remove the code

`Dll(segments=K)` emits its partial-correlation dumps at a fixed, uniform
rate `K*chip_rate/SF` — a sub-multiple of the chip rate, nothing more.
Turning that into a clean `N` samples/symbol grid for the demodulator is
a *separate* problem, solved by an explicit resampler
([`RateConverter`](../api/python-resample.md), arbitrary output/input
ratio, already in this codebase), not by contorting `Dll`'s own tuning
parameter to fake the right output rate.

**An earlier version of this page got this wrong**: it wired `segments`
directly into `MpskReceiver`'s `sps` (picking `segments=34` purely
because `round(segments*T_sym/T_epoch)` landed on an integer), which made
`segments=4` — Stage 2's own tracking-optimal choice — look
downstream-broken. It wasn't. At `segments=4` the partial rate is
~11.7 kHz, already ~5.6x the 2100 sym/s symbol rate — comfortably past
the 2x Nyquist floor for symbol timing recovery. The failure was
architectural, not a property of non-coherent partial correlation.

**The corrected chain**: `Dll(segments=4)` (Stage 2's own choice, kept
for its own tracking-robustness reasons and nothing else) feeds
`RateConverter`, which converts the partial stream to a clean `N=8`
samples/symbol — `MpskReceiver`'s own constructor default, the same
shape used everywhere else in this codebase. A "normal"
`MpskReceiver(m=2, sps=8, n=4, ...)` takes over from there, with none of
the previous `sps=47` weirdness. On the *same* signal, *same*
`segments=4`, the only thing that changed is whether a resample stage
sits in between: without it, BER sits at ~0.44 (chance) the entire run;
with it, BER is 0.0 and stays there — panel 3 below is that direct
before/after comparison. `init_norm_freq`'s unit conversion also
simplifies and becomes independent of `segments`: it's cycles per
`RateConverter`'s *output* sample rate (`N*symbol_rate`, fixed), not per
the despreader's own partial rate.

**A caveat found while re-measuring, not chased further**: this specific
loop tuning (`bn_carrier=bn_timing=0.01`, `sps=8`) stays perfectly locked
through the ~3500-symbol run plotted here, but a longer run (past ~4000
symbols, at this same margin) shows the symbol-timing loop starting to
jitter and occasionally slip. That's a separate, further loop-tuning
question — how finely Gardner timing resolution scales with `sps` under
this margin — not resolved on this page, the same way Stage 2 found (and
didn't chase) `segments=1`'s eventual long-run divergence.

## How it works

```python
--8<-- "src/doppler/examples/async_dsss_receiver_demo.py:signal"
```

```python
--8<-- "src/doppler/examples/async_dsss_receiver_demo.py:acq_symbol_rate"
```

```python
--8<-- "src/doppler/examples/async_dsss_receiver_demo.py:handoff"
```

Three stages, each seeded from the previous:

1. **Acquisition** streams raw samples until it reports a hit, sized by
    `symbol_rate=` the same robust-default way Stage 1/2 use.
1. **Hand-off.** `dll_init_chip_from_acq` (Stage 2's own helper, reused
    verbatim) seeds `Dll`'s code phase.
1. **Despread -> resample -> demod.** `Dll(segments=4).steps()`'s
    partial stream feeds `RateConverter`, which produces a clean `sps=8`
    grid for a "normal" `MpskReceiver` — matched filter (boxcar), NDA
    carrier acquisition, Gardner+Farrow symbol timing, and (once locked)
    decision-directed tracking.

## What you're seeing

All four panels run at one operating point (CN0=97 dB-Hz) — chosen, as in
the pre-story version of this demo, to unambiguously validate the
pipeline mechanics rather than run a margin sensitivity study (see Stage
2 for a page that studies margin sensitivity instead).

**Top-left — decoded BPSK constellation** (settled window). Two tight
clusters at ±1, not a smeared ring: the residual carrier has been fully
removed by `MpskReceiver`'s NDA carrier loop, and symbol timing has
converged tightly enough that every recovered sample lands
on-constellation.

**Top-right — running BER** (settled window). Flat at zero across the
settled window, confirming the lock isn't a lucky momentary alignment.

**Bottom-left — windowed decode correctness, with vs. without the
resample stage** (50-symbol windows, the whole run, both at
`segments=4`). This is the panel that demonstrates the central finding:
with `RateConverter` in the chain, correctness is 100% from the first
window on; without it (the earlier bug), it oscillates around the 0.5
chance line the entire run, never converging. Same `Dll` configuration
both times — the only variable is whether the stream got resampled to a
standard grid before reaching `MpskReceiver`.

**Bottom-right — `MpskReceiver.norm_freq` vs. epoch**. `Acquisition`'s
Doppler bins are sized wide enough that this page's hit resolves to a
single ~kHz-scale bin at Doppler=0 — the carrier-frequency seed handed to
`MpskReceiver` starts off by the *entire* 50 Hz injected residual, not
close to it. The NDA carrier loop visibly pulls `norm_freq` in over the
first ~50 epochs and holds it at the true value afterward.

## Two gotchas worth knowing before reusing this pattern

**`MpskReceiver`'s own `tracking`/`lock` flags are not reliable
stand-ins for "decoding correctly."** The broken (un-resampled) variant
frequently reports `tracking=1` with a healthy-looking `lock` value
despite decoding pure noise — the carrier loop can lock to *something*
without ever producing a correct bit. Check measured BER against known
data instead, same as this page does. This extends Stage 2's own caution
about `Dll.locked` not being reliable at large `segments` — the analogous
flag one layer up shows the same pattern.

**`init_norm_freq` starts from a coarse, quantized estimate, not the true
residual carrier** — see the bottom-right panel above.

Source: `src/doppler/examples/async_dsss_receiver_demo.py`. See also
[DSSS Acquisition: Continuous Async-Data Modulation](dsss-acq-async-data.md)
(Stage 1), [DSSS Despread: Continuous Async-Data Hand-off](dsss-despread-async-data.md)
(Stage 2), [Streaming Async Despreader](async-despread.md) (the
despread-only half, toy parameters, genie code phase), [Full-Chain
Lock-Up](receiver-lock.md) (a converged `Dll -> Costas -> SymbolSync`
chain with `.locked` observed via telemetry on all three loops), and
`docs/design/async-symbol-despreader.md` for the underlying theory.
