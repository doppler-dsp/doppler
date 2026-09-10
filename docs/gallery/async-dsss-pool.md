# AsyncDsssPool: the population's lifecycle

![Who holds which slot when: two emitters, one leaves and returns, the searcher's false alarms coloured grey; each tracked receiver's live Doppler below](../assets/async_dsss_pool_demo.png)

Where [AsyncDsssReceiver: the SPEC Waveform](async-dsss-receiver-spec.md)
drives one packaged receiver against one emitter, this page drives the
object that holds the *population*: [`dsss.AsyncDsssPool`](../api/python-dsss.md)
— one searcher, a pool of cell receivers created idle, the assigned table,
and the event log by attachment, all behind a single `push()`.

Two emitters on **one Gold code**, told apart by Doppler and code phase
alone, arrive on one channel. One leaves the air and is released by the
receiver's own rule — both lock flags down, without a break, for longer
than the confirm interval; it returns and is a *new* detection into
whichever slot is free, because the pool remembers nothing about an
emitter it has released. The searcher's false alarms are part of that
lifecycle: at pfa 1e-3 a noise peak occasionally seeds a free slot, pulls
in to nothing, and is released one interval later. That is why the figure
colours them, and why a slot is an emitter's **by both coordinates** — its
seed within the searcher's row of the emitter's Doppler *and* within a chip
of its code phase — never by a count. The C twin,
`native/examples/async_dsss_pool_demo.c`, manages what the binding hides:
the symbol buffer sized after the first push, the borrowed log the caller
closes.

## The geometry

The design's operating point at a demo's depth, `D = 16`: a 1023-chip Gold
code at 5 Mcps, two samples per chip, asynchronous BPSK at 2700 sym/s. The
searcher runs epoch by epoch over ±6 kHz, and coherence across `D = 16`
epochs makes its Doppler row 305 Hz wide, so the two emitters — 5 kHz apart
— are sixteen rows apart and two unmistakable peaks. Four slots and a
release interval of 300 ms keep the whole lifecycle inside one figure.

```python
--8<-- "src/doppler/examples/async_dsss_pool_demo.py:geometry"
```

## The stimulus is the shipped waveform

Each emitter is the shipped continuous-DSSS `Synth` at its own carrier
offset, clean; the second starts 900 chips into its code (two emitters at
one code phase are one peak to the searcher, and one row to the pool's
zone); the noise is a `type="noise"` Synth scaled by
`wfm_awgn_amplitude` from the C/N0, added once at the sum. B's synth keeps
running while it is off the air — coming back is not restarting.

```python
--8<-- "src/doppler/examples/async_dsss_pool_demo.py:stimulus"
```

## The pool, and reading it back

One `push()` per epoch; after each, every slot's record by value
(`status(slot)`), the symbols it decided on this push (`symbols(slot)`),
and the owner of its seed against the truth.

```python
--8<-- "src/doppler/examples/async_dsss_pool_demo.py:pool"
```

## What the figure shows

The upper panel is occupancy — one horizontal band per slot, coloured by
whose seed the slot holds. The lower panel is each tracked receiver's live
Doppler, against thin lines at the two emitters' true offsets. The dashed
verticals are the moments B leaves the air (1.0 s) and returns (1.9 s).

- **A** (blue) is seeded in the run's first epochs and holds slot 0 for the
    whole 2.9 s; in the lower panel its live Doppler sits flat on the
    +1500 Hz truth line.
- **B** (orange) holds slot 1 and keeps it for roughly a third of a second
    after it leaves — the release rule cannot fire before the 300 ms
    confirm interval, and here it fires just past it. When B returns, the
    pool has no memory of it: it is a fresh detection, seeded into whatever
    slot is free, which on this run is slot 1 again.
- The **grey** stint near 2.5 s is one of the searcher's false alarms. It
    takes the free slot 2, its receiver "tracks" noise at about −4000 Hz in
    the lower panel — nowhere near either truth line — and one release
    interval later the slot is free again. This is what the pool's slot
    headroom is for: the design's default of twelve slots carries ten
    emitters *and* the false alarms passing through them.
- The **event log** carries every transition — seeded, tracking, lost,
    released, seeded again, in order — and its line count equals what the
    pool counted.

The pool is certified at the full operating point, rather than this demo's,
by the lifecycle soak `validate_async_dsss_pool_soak`: ten emitters through
the channel at their own Dopplers, the block-coherent searcher at
`D = 154`, two minutes at 45 and 40 dB-Hz.

## See also

- [The async DSSS receiver design](../design/async-dsss-receiver.md) — the
    searcher, the cell receivers and the assignment rules the pool composes.
