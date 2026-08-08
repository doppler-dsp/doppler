# Capturing All Receiver Telemetry

![Every MpskReceiver probe captured from one ring](../assets/mpsk_telemetry_capture_demo.png)

## What you're seeing

Every panel is one telemetry probe an `track.MpskReceiver` exposes, and
every trace came out of a **single** `telemetry.MemoryCapture` over a
**single** `telemetry.Telemetry` ring. One `set_telemetry` attach registers
the receiver's own carrier probes **and** forwards to its symbol-timing
child loop, so a cold-start QPSK pull-in leaves a complete record of the
whole receiver:

- **`rx.lock` / `rx.tracking` / `rx.car.locked`** — the carrier lock EMA
    rises off its cold-start value, and the two verify-counted decisions
    flip 0→1 as the receiver hands over to decision-directed tracking.
- **`rx.car.freq`** — the tracked NCO frequency pulls in to the injected
    0.0015 cyc/sample offset.
- **`rx.car.e` / `rx.sync.e` / `rx.sync.ctrl`** — the carrier discriminator
    and the timing TED / loop-filter control settle out of the acquisition
    transient.
- **`rx.sync.rate` / `rx.sync.mu`** — the tracked samples/symbol settles on
    ~8.0 and the fractional interpolation phase sweeps its `[0, 1)` range.
- **`rx.sync.lock` / `rx.sync.locked`** — the timing lock statistic climbs
    and its verify-counted decision declares.

Nothing is decimated (`decim=1`) and nothing is dropped — the summary cell
reports the full capture: 11 probes, one 16-byte record per probe per
recovered symbol. The x-axis is real time, because each record carries the
sample index it was stamped with.

## How it works

Two things the caller no longer does. **The drain**: a `MemoryCapture`
sizes the ring from the probe count and the block, and `set_now(i)` drains
at every boundary — so losslessness is arithmetic rather than a cadence you
have to get right, and `close()` raises if a record was lost anyway.
**The split**: `read_dict(index=True)` returns `{name: (n, values)}`, so
nothing below filters by probe id, inverts an id-to-name map, or plots an
event ordinal in place of a time axis.

```python
--8<-- "src/doppler/examples/mpsk_telemetry_capture_demo.py:capture"
```

The 16-byte record layout **is** the capture format. `records()` returns
the exact C `dp_tlm_rec_t` as a structured array
(`n:u8, value:f4, probe:u2, flags:u2`), so `.tofile()` writes it with no
transformation and those bytes are byte-for-byte the `TLM16` payload that
`dp_tlm_sink` frames onto the NATS wire. File storage and streaming fan-out
share one format — the capture above verifies the round-trip bit-exact.

The ring stays SPSC: producer (`steps`) and consumer (the capture's drain)
run on one thread together, which is exactly why the boundary drain is both
the fastest option and the provable one.

For a capture you never need back in-process, `Capture(tlm, block, path, clock)` writes those same bytes straight to disk plus a `<path>-meta` JSON
sidecar carrying the probe registry and time base — self-describing, so a
reader needs nothing from the process that wrote it. To take the same
records across processes live, publish them as `TLM16` frames instead; see
[Many Emitters, One Consumer](telemetry-fanin.md).

## Run it

```bash
python src/doppler/examples/mpsk_telemetry_capture_demo.py   # → mpsk_telemetry_capture_demo.png  (~2 s)
```

See the [telemetry API](../api/python-telemetry.md) for the probe tables
and record layout, [M-PSK Receiver](mpsk-receiver.md) for the receiver
itself, and [Lock Detection](lockdet.md) for the verify-counted decisions
the `*.locked` / `rx.tracking` traces come from.
