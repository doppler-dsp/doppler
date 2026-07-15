# Continuous Async DSSS Receiver

![Constellation, running BER, segments comparison, and carrier pull-in](../assets/async_dsss_receiver_demo.png)

Stage 3 of the multi-part story that began with
[DSSS Acquisition: Continuous Async-Data Modulation](dsss-acq-async-data.md)
(Stage 1 — does `Acquisition` land the right code phase/Doppler bin) and
[DSSS Despread: Continuous Async-Data Hand-off](dsss-despread-async-data.md)
(Stage 2 — does that hit correctly seed `Dll`, and is `segments=4` robust
enough for the DLL's *own* tracking loop). This page asks the last
question the story set up: carrier and symbol-timing recovery
([`MpskReceiver`](../api/python-track.md)) sit downstream of `Dll` too —
does Stage 2's own `segments=4` sweet spot suffice there as well, or does
a coherent-combining matched filter need something `Dll`'s own loop
never needed?

`MpskReceiver`'s own docstring states the intended composition: *"A
DSSS-MPSK receiver is `Dll(segments) -> MpskReceiver`: despread to
symbol-rate soft chips, then this modem."* `Dll.steps()`'s partial stream
feeds `MpskReceiver.steps()` directly — no intermediate carrier wipe or
matched filter, since `MpskReceiver` owns both internally.

Not to be confused with [`dsss.Despreader`](despreader.md), which composes
`Costas`+`Dll` for the **synchronous** case (symbol clock = code-epoch
clock, `segments=1`). This page's signal needs the asynchronous
`segments=K` architecture throughout.

## The central finding

**Measured fresh on this page's CCSDS Gold-code signal, not assumed from
an older run: `segments=4` — `Dll`'s own robust tracking sweet spot from
Stage 2 — never decodes.** Streamed through `MpskReceiver` unchanged, its
BER sits at ~0.47, indistinguishable from a coin flip, for the entire
run. `segments=34` (chosen so `round(segments * T_sym/T_epoch)` lands on
an `MpskReceiver`-friendly `sps` with a small divisor for the carrier-arm
count `n`) decodes perfectly, converging to 100% symbol correctness
within about 150 symbols and staying there.

The reason: each `Dll` partial is `segments`-times weaker than a full
coherent code epoch. That's fine for the DLL's own non-coherent
early/late discriminator (Stage 2), but it starves `MpskReceiver`'s
coherent matched filter (length `sps`) of the samples-per-symbol it needs
to rebuild real coherent gain before its carrier/timing loops can
converge at all. **The DLL's own optimum is downstream-insufficient** —
tuning `Dll` in isolation and assuming that setting carries over to a
composed receiver is exactly the trap this page catches.

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
    `symbol_rate=` the same robust-default way Stage 1/2 use — no
    `reset()`-hopping sweep needed (the code is always present).
1. **Hand-off.** `dll_init_chip_from_acq` (Stage 2's own helper, reused
    verbatim — the phase-inversion finding isn't re-litigated here) seeds
    `Dll`'s code phase; `MpskReceiver`'s `init_norm_freq` is cycles per
    *its own input sample* (the `Dll` partial stream, at rate
    `FS*segments/TE`), not cycles per raw ADC sample.
1. **Despread + demod.** `Dll(segments).steps()`'s partial stream feeds
    directly into `MpskReceiver.steps()` — matched filter (boxcar), NDA
    carrier acquisition, Gardner+Farrow symbol timing, and (once locked)
    decision-directed tracking, all in one call.

## What you're seeing

All four panels run at one operating point (CN0=97 dB-Hz) — chosen, as in
the pre-story version of this demo, to unambiguously validate the
pipeline mechanics rather than run a margin sensitivity study (see Stage
2 for a page that studies margin sensitivity instead).

**Top-left — decoded BPSK constellation** (settled window, `segments=34`).
Two tight clusters at ±1, not a smeared ring: the residual carrier has
been fully removed by `MpskReceiver`'s NDA carrier loop, and symbol
timing has converged tightly enough that every recovered sample lands
on-constellation.

**Top-right — running BER** (settled window, `segments=34`). Flat at
zero across the settled window, confirming the lock isn't a lucky
momentary alignment.

**Bottom-left — windowed decode correctness, `segments=4` vs.
`segments=34`** (50-symbol windows, the whole run). This is the panel
that actually demonstrates the central finding: `segments=34`'s windowed
BER drops to zero almost immediately and stays there; `segments=4`'s
oscillates around the 0.5 chance line the entire run, never converging.

**Bottom-right — `MpskReceiver.norm_freq` vs. epoch** (`segments=34`).
`Acquisition`'s Doppler bins are sized wide enough that this page's hit
resolves to a single ~kHz-scale bin at Doppler=0 — the carrier-frequency
seed handed to `MpskReceiver` starts off by the *entire* 50 Hz injected
residual, not close to it. The NDA carrier loop visibly pulls `norm_freq`
in over the first ~100 epochs and holds it at the true value afterward.
Nothing downstream needs the Acquisition estimate to be exact — only
close enough for the carrier loop's own capture range.

## Two gotchas worth knowing before reusing this pattern

**`MpskReceiver`'s own `tracking`/`lock` flags are not reliable
stand-ins for "decoding correctly" at a failing `segments`.** In this
page's run, `segments=4` frequently reports `tracking=1` with a
healthy-looking `lock` value despite decoding pure noise — the carrier
loop can lock to *something* (a wrong absolute phase, or a timing point
that isn't actually sampling the symbol) without ever producing a
correct bit. Don't gate success on these flags for an unfamiliar
`segments`; check measured BER against known data instead. This extends
Stage 2's own caution about `Dll.locked` not being reliable at large
`segments` — the analogous flag one layer up shows the same pattern, and
Stage 2's resolution (gate on measured performance, not the lock flag)
applies here too.

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
