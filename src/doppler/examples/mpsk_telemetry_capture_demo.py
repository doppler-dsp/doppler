#!/usr/bin/env python3
"""Capture and store EVERY telemetry probe an MpskReceiver exposes.

Attaches one `Telemetry` context to an `MpskReceiver` at ``decim=1`` (every
event on every probe), drives a QPSK carrier pull-in, drains the lock-free
ring block by block, and stores the complete capture. The stored bytes ARE
the on-wire format: a `Telemetry.read()` row is the 16-byte C
``dp_tlm_rec_t`` (``n:u8, value:f4, probe:u2, flags:u2``), so ``np.save``
persists it with zero conversion and ``recs.tobytes()`` is exactly the
``TLM16`` payload that ``dp_tlm_sink`` frames onto NATS.

The receiver forwards to its child loops, so one attach registers all 11
probes — its own carrier set (``rx.lock``/``rx.tracking``/``rx.car.*``) plus
the symbol-timing loop (``rx.sync.*``). The figure plots every one of them,
all pulled from the same `read()` loop.

Generates ``docs/assets/mpsk_telemetry_capture_demo.png``.

Run:  uv run python src/doppler/examples/mpsk_telemetry_capture_demo.py
"""

from __future__ import annotations

import sys

# --8<-- [start:capture]
import tempfile
from pathlib import Path

import numpy as np

from doppler.telemetry import Telemetry
from doppler.track import MpskReceiver

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
# so probe_names below is the full set of "all available telemetry".
tlm = Telemetry(1 << 14)
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
probes = tlm.probe_names  # name -> id: the full registered set

# Produce block by block on one thread; the single consumer drains every block.
chunks = []
for i in range(0, iq.size, 256):
    tlm.set_now(i)  # stamp this block's records with sample index i
    rx.steps(iq[i : i + 256])  # producer: emits from the hot loops
    chunks.append(tlm.read())  # consumer: drain what this block emitted
recs = np.concatenate(chunks)

# The 16-byte record layout IS the capture format. np.save writes the array
# verbatim; recs.tobytes() is byte-for-byte the TLM16 payload tlm_sink frames.
cap = Path(tempfile.mkdtemp())
np.save(cap / "mpsk_tlm.npy", recs)
(cap / "mpsk_tlm.tlm16").write_bytes(recs.tobytes())

assert rx.tracking == 1  # the receiver handed over to decision-directed track
assert tlm.dropped == 0  # the drain cadence kept the ring ahead
assert {int(p) for p in np.unique(recs["probe"])} == set(probes.values())
assert np.array_equal(np.load(cap / "mpsk_tlm.npy"), recs)  # bit-exact reload
assert (cap / "mpsk_tlm.tlm16").stat().st_size == recs.nbytes == 16 * len(recs)
# --8<-- [end:capture]


def main(out_path: str = "mpsk_telemetry_capture_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    id2name = {v: kk for kk, v in probes.items()}
    order = sorted(probes.values())

    ncol = 3
    nrow = -(-(len(order) + 1) // ncol)  # +1 cell for the capture summary
    fig, axes = plt.subplots(
        nrow, ncol, figsize=(12.0, 2.0 * nrow), sharex=True
    )
    axes = axes.ravel()

    # Every probe emits once per recovered symbol, so the event ordinal is the
    # symbol index; one small panel each — the whole capture, nothing dropped.
    for ax, pid in zip(axes, order):
        v = recs[recs["probe"] == pid]["value"]
        ax.plot(np.arange(v.size), v, lw=0.8, color="#1565c0")
        ax.set_title(id2name[pid], fontsize=9, family="monospace")
        ax.grid(alpha=0.3)
        ax.margins(x=0)

    # Summary in the trailing empty cell.
    sm = axes[len(order)]
    sm.axis("off")
    sm.text(
        0.5,
        0.5,
        f"{len(order)} probes\n{len(recs)} records\n"
        f"{recs.nbytes:,} bytes  (16 B/record)\n{tlm.dropped} dropped",
        ha="center",
        va="center",
        fontsize=11,
        family="monospace",
    )
    for ax in axes[len(order) + 1 :]:
        ax.axis("off")

    fig.suptitle(
        "Every MpskReceiver probe, one ring, one read() loop", fontsize=13
    )
    fig.supxlabel("symbol index (one event per recovered symbol)")
    fig.tight_layout()
    fig.savefig(out_path, dpi=110)
    print(
        f"captured {len(recs)} records over {len(order)} probes "
        f"({recs.nbytes:,} bytes, {tlm.dropped} dropped) -> {out_path}"
    )


if __name__ == "__main__":
    main(*sys.argv[1:2])
