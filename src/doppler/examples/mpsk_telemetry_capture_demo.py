#!/usr/bin/env python3
"""Capture EVERY telemetry probe an MpskReceiver exposes, losslessly.

Attaches one `Telemetry` context to an `MpskReceiver` at ``decim=1`` (every
event on every probe), drives a QPSK carrier pull-in, and lets a
`MemoryCapture` own the drain. Nothing here reasons about ring size or drain
cadence: the capture sizes the ring from the probe count and the block, and
``set_now()`` drains at every boundary, so **a drop is impossible rather than
merely unlikely** — and `close()` raises if one happened anyway.

The receiver forwards to its child loops, so one attach registers all 11
probes — its own carrier set (``rx.lock``/``rx.tracking``/``rx.car.*``) plus
the symbol-timing loop (``rx.sync.*``). `read_dict` hands them back split by
name with their sample indices, so the figure plots real **seconds** and no
line of this file filters by probe id or inverts an id-to-name map.

The stored bytes are still the on-wire format: a record is the 16-byte C
``dp_tlm_rec_t`` (``n:u8, value:f4, probe:u2, flags:u2``), so ``.tofile()``
writes exactly the ``TLM16`` payload ``dp_tlm_sink`` frames onto NATS. For a
capture you never need back in-process, ``Capture(tlm, block, path, clock)``
writes those same bytes straight to disk plus a JSON sidecar carrying the
probe registry and time base.

Generates ``docs/assets/mpsk_telemetry_capture_demo.png``.

Run:  uv run python src/doppler/examples/mpsk_telemetry_capture_demo.py
"""

from __future__ import annotations

import sys

# --8<-- [start:capture]
import tempfile
from pathlib import Path

import numpy as np

from doppler.telemetry import MemoryCapture, Telemetry
from doppler.track import MpskReceiver
from doppler.wfm import SampleClock

FS = 1e6  # sample rate, and therefore the figure's time axis
BLOCK = 256  # the step of our own loop — and the capture's whole contract

# A QPSK signal at 8 samples/symbol with a residual carrier offset, cold-
# started (init_norm_freq=0) so the pull-in is real; 20 dB matched Es/N0.
rng = np.random.default_rng(1)
idx = rng.integers(0, 4, 4000)
tx = np.exp(1j * (2 * np.pi * idx / 4 + np.pi / 4)).astype(np.complex64)
tx = np.repeat(tx, 8).astype(np.complex64)
k = np.arange(tx.size)
sigma = np.sqrt(8 / (2 * 10 ** (20.0 / 10)))
iq = (
    tx * np.exp(2j * np.pi * 0.0015 * k)
    + rng.normal(0, sigma, tx.size)
    + 1j * rng.normal(0, sigma, tx.size)
).astype(np.complex64)

# ONE ring; attach the receiver at decim=1 = EVERY event on EVERY probe. The
# attach registers the receiver's own probes AND forwards to its child loops,
# so this is the full set of "all available telemetry". Probes must be
# attached BEFORE the capture opens: the ring is sized from the probe table.
tlm = Telemetry()
rx = MpskReceiver(
    m=4,
    sps=8,
    m_out=4,
    init_norm_freq=0.0,
    bn_carrier=0.02,
    bn_timing=0.01,
    acq_to_track=1,
    lock_thresh=0.65,
    warmup_syms=200,
)
rx.set_telemetry(tlm, "rx", 1)

# The capture owns the drain. `set_now(i)` marks the boundary and drains the
# block just finished; no ring size to guess, no read()/concatenate loop, and
# no post-hoc assert standing in for a guarantee.
with MemoryCapture(tlm, BLOCK, SampleClock(FS)) as cap:
    for i in range(0, iq.size, BLOCK):
        tlm.set_now(i)
        rx.steps(iq[i : i + BLOCK])
    cap.close()  # final drain — and it RAISES if a record was lost, so
    #              reaching the next line is itself the losslessness proof
    series = cap.read_dict(index=True)  # {name: (sample_index, values)}
    recs = cap.records()  # the same data, still 16-byte wire records

# The 16-byte record layout IS the capture format: .tofile() writes exactly
# the TLM16 payload tlm_sink frames onto the wire.
store = Path(tempfile.mkdtemp()) / "mpsk_tlm.tlm16"
recs.tofile(store)

assert rx.tracking == 1  # the receiver handed over to decision-directed track
assert set(series) == set(tlm.probe_names)  # every probe came back by name
assert sum(v.size for _, v in series.values()) == len(recs)  # nothing lost
assert store.stat().st_size == recs.nbytes == 16 * len(recs)
assert np.array_equal(np.fromfile(store, dtype=recs.dtype), recs)
# --8<-- [end:capture]


def main(out_path: str = "mpsk_telemetry_capture_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    names = sorted(series)
    ncol = 3
    nrow = -(-(len(names) + 1) // ncol)  # +1 cell for the capture summary
    fig, axes = plt.subplots(
        nrow, ncol, figsize=(12.0, 2.2 * nrow), sharex=True
    )
    axes = axes.ravel()

    # `n` is the sample index the record was stamped with, so the x-axis is
    # real time — not an event ordinal standing in for one.
    for ax, name in zip(axes, names):
        n, v = series[name]
        ax.plot(n / FS * 1e3, v, lw=0.8, color="#1565c0")
        ax.set_title(name, fontsize=9, family="monospace")
        ax.grid(alpha=0.3)
        ax.margins(x=0)

    sm = axes[len(names)]
    sm.axis("off")
    sm.text(
        0.5,
        0.5,
        f"{len(names)} probes\n{len(recs)} records\n"
        f"{recs.nbytes:,} bytes  (16 B/record)\nnothing dropped",
        ha="center",
        va="center",
        fontsize=11,
        family="monospace",
    )
    for ax in axes[len(names) + 1 :]:
        ax.axis("off")

    fig.suptitle(
        "Every MpskReceiver probe, one ring, nothing dropped", fontsize=13
    )
    fig.supxlabel("time (ms)")
    fig.tight_layout()
    fig.savefig(out_path, dpi=110)
    print(
        f"captured {len(recs)} records over {len(names)} probes "
        f"({recs.nbytes:,} bytes, nothing dropped) -> {out_path}"
    )


if __name__ == "__main__":
    main(*sys.argv[1:2])
