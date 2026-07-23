# Continuous Async DSSS Receiver

![Decoded BPSK constellation and running BER](../assets/async_dsss_receiver_demo.png)

The full receive chain — `Acquisition -> Dll(segments=K) -> MpskReceiver`
— for a **continuous**, non-bursty spreading code whose data-symbol clock
is **asynchronous** to the code-epoch clock: a 1023-chip PN code at
3 Mchips/s carrying BPSK data at 2100 sym/s (`chips/symbol ~= 1428.6`, not
an integer). This is the same two-clock problem
[Streaming Async Despreader](async-despread.md) and
`docs/design/async-symbol-despreader.md` study, but with a real
[`Acquisition`](../api/python-dsss.md) search in front — neither existing
example constructs `Dll` from anything but a genie-known phase.

Not to be confused with [`dsss.Despreader`](despreader.md), which composes
`Costas`+`Dll` for the **synchronous** case (symbol clock = code-epoch
clock, `segments=1`). This page's signal needs the asynchronous
`segments=K` architecture throughout.

## What you're seeing

**Left — decoded BPSK constellation.** Two tight clusters at ±1, not a
smeared ring: the residual carrier has been fully removed by
[`MpskReceiver`](../api/python-track.md)'s NDA carrier loop by the settled
window shown, and the symbol timing has converged tightly enough that
every recovered sample lands on-constellation. BER over this window is
0 — every symbol decodes correctly against the known transmitted data.

**Right — running BER.** Flat at zero across the settled window,
confirming the lock isn't a lucky momentary alignment.

## How it works

```python
--8<-- "src/doppler/examples/async_dsss_receiver_demo.py:signal"
```

Three stages, each seeded from the previous:

1. **Acquisition** streams raw samples until it reports a hit — no
    `reset()`-hopping sweep needed (unlike a sparse burst, the code is
    always present, so a single continuous `push()` stream suffices).
1. **Hand-off.** The hit's `doppler_bin`/`code_phase` seed `Dll` and
    `MpskReceiver`. Two non-obvious conversions, both load-bearing:
    - `Dll`'s `init_chip` is the code's *actual* phase, while
        `Acquisition`'s `code_phase` is a correlation *lag* — they're
        inverted: `chip_phase = (sf - code_phase/spc) % sf`.
    - `MpskReceiver`'s `init_norm_freq` is cycles per *its own input
        sample* (the `Dll` partial stream), not cycles per raw ADC sample:
        divide `doppler_hz` by the partial rate `fs*K/T_epoch`, not `fs`.
1. **Despread + demod.** `Dll(segments=K).steps()`'s partial stream feeds
    directly into `MpskReceiver.steps()` — matched filter (boxcar), NDA
    carrier acquisition, Gardner+Farrow symbol timing, and (once locked)
    decision-directed tracking, all in one call.

### Tuning `K`

`docs/design/async-symbol-despreader.md` finds `K=4` optimal — but that
result is tuned for the DLL's *own* non-coherent code-discriminator
variance, not for feeding a downstream matched filter. Each partial is
`K`-times weaker than a full coherent epoch; `MpskReceiver`'s own boxcar
(length `sps`) needs enough partials to rebuild real coherent gain before
its carrier/timing loops can converge. `K=4` never locks here; `K=34`
(chosen so `round(K * T_sym/T_epoch)` lands on an `MpskReceiver`-friendly
`sps` with a small divisor for the carrier-arm count `n`) locks robustly.
If you reuse this pattern at different rates, expect to re-sweep `K`.

Source: `src/doppler/examples/async_dsss_receiver_demo.py`. See also
[Streaming Async Despreader](async-despread.md) (the despread-only half,
toy parameters, genie code phase), [Full-Chain Lock-Up](receiver-lock.md)
(a converged `Dll -> Costas -> SymbolSync` chain with `.locked` observed
via telemetry on all three loops — worth comparing against this page's
`Dll.locked` staying unlatched despite a clean decode, discussed in the
example's own docstring), and `docs/design/async-symbol-despreader.md` for
the underlying theory.
