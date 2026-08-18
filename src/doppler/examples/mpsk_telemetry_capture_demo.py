#!/usr/bin/env python3
"""Capture EVERY telemetry probe a BpskReceiver exposes, losslessly.

Attaches one `Telemetry` context to a `BpskReceiver` at ``decim=1`` (every
event on every probe), drives a BPSK carrier pull-in, and lets a
`MemoryCapture` own the drain. Nothing here reasons about ring size or drain
cadence: the capture sizes the ring from the probe count and the block, and
``set_now()`` drains at every boundary, so **a drop is impossible rather than
merely unlikely** — and `close()` raises if one happened anyway.

The receiver forwards to its child loops, so one attach registers all 13
probes — its own carrier set (``rx.lock``/``rx.tracking``/``rx.car.*``), the
symbol-timing loop (``rx.sync.*``), and the front-end AGC (``rx.agc.*``).
The AGC pair does not share the others' grid: it is tapped pre-terminal,
ahead of the stage the timing loop steers, and emits once per gain update
rather than once per recovered symbol. That is what makes `read_dict`
handing back sample **indices** load-bearing here rather than a
convenience — the figure plots real seconds, so probes counted on two
different grids still share one axis, and no line of this file filters by
probe id or inverts an id-to-name map.

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
from doppler.track import BpskReceiver
from doppler.wfm import SampleClock

FS = 1e6  # sample rate, and therefore the figure's time axis
RS = 125e3  # symbol rate — 8 samples/symbol, but nothing here says "8"
BLOCK = 256  # the step of our own loop — and the capture's whole contract
BN_CARRIER = 0.02

# The carrier offset, seeded AT the loop's acquisition bound and not past it.
# The bound is `bn_carrier / m` cycles per SYMBOL (the m because the NDA
# discriminator is an M-th power), so it is stated in those units and
# converted to the cycles per sample the stimulus advances in exactly once.
# This demo used to seed 0.0015 cyc/sample against a QPSK bound of 0.005
# cyc/symbol — 2.4x outside it, where acquisition is a coin flip rather than
# a demonstration (doppler#843).
OFFSET_SYM = BN_CARRIER / 2  # cycles per symbol, at the bound
OFFSET = OFFSET_SYM * RS / FS  # cycles per sample, for the stimulus

# A BPSK signal with that residual offset, cold-started so the pull-in is
# real; 20 dB matched Es/N0.
rng = np.random.default_rng(1)
idx = rng.integers(0, 2, 4000)
tx = np.exp(1j * np.pi * idx).astype(np.complex64)
tx = np.repeat(tx, int(FS / RS)).astype(np.complex64)
k = np.arange(tx.size)
sigma = np.sqrt(8 / (2 * 10 ** (20.0 / 10)))
iq = (
    tx * np.exp(2j * np.pi * OFFSET * k)
    + rng.normal(0, sigma, tx.size)
    + 1j * rng.normal(0, sigma, tx.size)
).astype(np.complex64)

# ONE ring; attach the receiver at decim=1 = EVERY event on EVERY probe. The
# attach registers the receiver's own probes AND forwards to its child loops,
# so this is the full set of "all available telemetry". Probes must be
# attached BEFORE the capture opens: the ring is sized from the probe table.
tlm = Telemetry()
# Stated in the units a capture comes with: two rates, in Hz. `sps` is
# `FS / RS` and the receiver computes it; `m` is carried by the type. Neither
# appears here, and `m_out` is DERIVED rather than pinned — which is not
# cosmetic: this demo pinned `m_out=4` against the default I&D pulse, and
# that pairing is measured at 3.11 dB off the coherent bound where the
# derived 8 is 0.41 dB off. A parameter nobody needed was costing the demo
# most of its margin.
rx = BpskReceiver(
    sample_rate_hz=FS,
    symbol_rate_hz=RS,
    bn_carrier=BN_CARRIER,
    bn_timing=0.01,
    acq_to_track=1,
)
rx.set_telemetry(tlm, "rx", 1)

# The capture owns the drain. `set_now(i)` marks the boundary and drains the
# block just finished; no ring size to guess, no read()/concatenate loop, and
# no post-hoc assert standing in for a guarantee. Leaving the block finalizes
# and RAISES if a record was lost — so reaching the next line is itself the
# losslessness proof — but it does not free, so the capture is still readable.
with MemoryCapture(tlm, BLOCK, SampleClock(FS)) as cap:
    for i in range(0, iq.size, BLOCK):
        tlm.set_now(i)
        rx.steps(iq[i : i + BLOCK])

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
        # Symbols are DISCRETE samples, not a trajectory: one decision per
        # symbol period, with nothing in between to interpolate through. A
        # line through them draws a path the receiver never took, and on a
        # BPSK stream toggling +-1 it fills the panel solid and shows
        # nothing. Loop state is the opposite -- a continuous quantity
        # sampled per symbol -- so it stays a line.
        if ".sym." in name:
            ax.plot(n / FS * 1e3, v, ".", ms=1.5, color="#1565c0")
        else:
            ax.plot(n / FS * 1e3, v, lw=0.8, color="#1565c0")
        ax.set_title(name, fontsize=9, family="monospace")
        ax.grid(alpha=0.3)
        ax.margins(x=0)

    # The counts are printed to stdout and asserted above; a panel restating
    # them is a caption for the figure's own axes, which the panel titles
    # already are.
    for ax in axes[len(names) :]:
        ax.axis("off")

    fig.suptitle(
        "Every BpskReceiver probe, one ring, nothing dropped", fontsize=13
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
