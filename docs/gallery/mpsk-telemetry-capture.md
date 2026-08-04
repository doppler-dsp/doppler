# Capturing All Receiver Telemetry

![Every MpskReceiver probe captured from one ring](../assets/mpsk_telemetry_capture_demo.png)

## What you're seeing

Every panel is one telemetry probe an `track.MpskReceiver` exposes, and
every trace came out of a **single** `read()` loop over a **single**
`telemetry.Telemetry` ring. One `set_telemetry` attach registers the
receiver's own carrier probes **and** forwards to its symbol-timing child
loop, so a cold-start QPSK pull-in leaves a complete record of the whole
receiver:

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
recovered symbol.

## How it works

The 16-byte record layout **is** the capture format. `Telemetry.read()`
returns the exact C `dp_tlm_rec_t` as a structured array
(`n:u8, value:f4, probe:u2, flags:u2`), so `np.save` persists it with no
transformation and `recs.tobytes()` is byte-for-byte the `TLM16` payload
that `dp_tlm_sink` frames onto the NATS wire. File storage and streaming
fan-out share one format — the capture below verifies both round-trip
bit-exact:

```python
--8<-- "src/doppler/examples/mpsk_telemetry_capture_demo.py:capture"
```

Two rules make "capture everything" safe by construction: the ring is
SPSC, so the producer (`steps`) and consumer (`read`) run on one thread
together while `set_now(n)` stamps a shared sample clock; and `read()` is
non-blocking, so a slow reader costs *records* (`dropped` counts them),
never *throughput*. Drain at least as fast as the aggregate emit rate and
`dropped` stays 0.

For a long run, append each `read()` chunk to an open file handle and
rotate on size — every chunk is already wire-format bytes. To take the same
records across processes, publish them as `TLM16` frames instead of saving
them; see [Many Emitters, One Consumer](telemetry-fanin.md).

## Run it

```bash
python src/doppler/examples/mpsk_telemetry_capture_demo.py   # → mpsk_telemetry_capture_demo.png  (~2 s)
```

See the [telemetry API](../api/python-telemetry.md) for the probe tables
and record layout, [M-PSK Receiver](mpsk-receiver.md) for the receiver
itself, and [Lock Detection](lockdet.md) for the verify-counted decisions
the `*.locked` / `rx.tracking` traces come from.
