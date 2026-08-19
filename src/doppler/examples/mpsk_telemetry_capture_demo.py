#!/usr/bin/env python3
"""Capture EVERY telemetry probe a BpskReceiver exposes, losslessly.

Attaches one `Telemetry` context to a `BpskReceiver` at ``decim=1`` (every
event on every probe), drives a BPSK carrier pull-in, and lets a
`MemoryCapture` own the drain. Nothing here reasons about ring size or drain
cadence: the capture sizes the ring from the probe count and the block, and
``set_now()`` drains at every boundary, so **a drop is impossible rather than
merely unlikely** — and `close()` raises if one happened anyway.

The receiver forwards to its child loops, so one attach registers every
probe it and they own — its own carrier set (``rx.lock``/``rx.car.*``),
the symbol-timing loop (``rx.sync.*``, including ``rx.sync.lock``, the
Gardner eye-opening ratio, and its de-chattered ``rx.sync.locked``), the
recovered symbol (``rx.sym.*``), and the front-end AGC (``rx.agc.*``). The
count is deliberately not written down here — it was stated as 13 while the
real figure was 16, and nothing read it back. What pins the set is the
assert below, ``set(series) == set(tlm.probe_names)``, which fails if a
probe appears or disappears.
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
from doppler.wfm import Composer, SampleClock, Segment

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

# The stimulus is built by `wfmgen`'s composer, not by numpy. Source
# generation is C-first, and this is the same path the CLI and the JSON
# record drive -- so the demo exercises the shipped transmitter instead of a
# second one written to resemble it.
#
# What that replaces is worth naming, because each line was a copy of
# something the library already owns: `np.repeat` for a rectangular pulse,
# `exp(2j*pi*OFFSET*k)` for the carrier offset, and
# `sqrt(8 / (2 * 10**(20/10)))` for the noise level -- an Es/N0 conversion
# that is `snr_mode="esno"` here and `wfm_snr_over_fs()` in C. A demo that
# re-derives the transmitter cannot catch a transmitter bug.
NSYM = 4000
SPS = int(FS / RS)
iq = np.asarray(
    Composer(
        [
            Segment(
                type="pn",  # maximal-length payload; no numpy RNG
                pn_length=15,  # period 32767 >> NSYM, so it never repeats
                seed=1,
                modulation="bpsk",
                pulse="rect",  # NRZ, matching the I&D the receiver derives
                sps=SPS,
                snr=20.0,
                snr_mode="esno",  # 20 dB MATCHED Es/N0, by the library's law
                freq=OFFSET * FS,  # cycles/sample -> Hz, converted once
                fs=FS,
                num_samples=NSYM * SPS,
            )
        ]
    ).compose()
)

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
# There is no handover: one M-th-power NDA discriminator steers the LO from
# the first output to the last, which is Mode 1 in `docs/design/mpsk.md` --
# nothing here waits for anything, and the transient is simply the cost of
# starting cold. `acq_to_track` was retired in doppler#877 along with the
# second discriminator it selected.
rx = BpskReceiver(
    sample_rate_hz=FS,
    symbol_rate_hz=RS,
    bn_carrier=BN_CARRIER,
    bn_timing=0.01,
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

assert rx.lock_time >= 0  # it declared, and lock_time dates the first one
assert set(series) == set(tlm.probe_names)  # every probe came back by name
assert sum(v.size for _, v in series.values()) == len(recs)  # nothing lost
assert store.stat().st_size == recs.nbytes == 16 * len(recs)
assert np.array_equal(np.fromfile(store, dtype=recs.dtype), recs)

# A BPSK decision is REAL, so a locked carrier loop leaves essentially
# nothing in Q -- measured over the settled half, mean|I| is ~17x mean|Q|.
# That is the claim the shared I/Q panel exists to make visible, and it is
# asserted here rather than left to the eye.
settled = len(series["rx.sym.i"][1]) // 2
mean_i = np.abs(series["rx.sym.i"][1][settled:]).mean()
mean_q = np.abs(series["rx.sym.q"][1][settled:]).mean()
assert mean_i > 8 * mean_q, f"carrier unresolved: {mean_i=:.3f} {mean_q=:.3f}"
# --8<-- [end:capture]


def main(out_path: str = "mpsk_telemetry_capture_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    # One panel per probe, EXCEPT the recovered symbol. `sym.i` and `sym.q`
    # are two halves of one complex number and the thing worth seeing is
    # their RELATIVE size: on a locked BPSK receiver the decision is all in
    # I and Q sits at zero, which is a claim about the carrier loop, not
    # about the symbol. On separate panels each axis autoscales to its own
    # range, so a Q of pure noise renders exactly like a Q carrying signal
    # -- the one reading the pair is there to distinguish. Sharing an axis
    # is what makes the comparison honest.
    panels = []
    for name in sorted(series):
        if name.endswith(".sym.q"):
            continue  # drawn beside its .sym.i partner, below
        if name.endswith(".sym.i"):
            stem = name[: -len(".i")]
            panels.append((f"{stem}.i / {stem}.q", [name, f"{stem}.q"]))
        else:
            panels.append((name, [name]))

    # What each trace is meant to be READ AGAINST. Two kinds, kept visually
    # distinct because they answer different questions:
    #
    #   * a DECISION threshold (red) -- read off the receiver, never retyped,
    #     so the line and the decision it drives cannot drift apart; and
    #   * the ACTUAL value an estimator is estimating (green), which this
    #     script knows because it generated the stimulus.
    #
    # Without them a panel shows a trace settling onto some number and leaves
    # the reader to decide whether it is the RIGHT number -- which is the one
    # thing the panel is there to answer.
    THRESH, ACTUAL = "#c62828", "#2e7d32"
    refs = {
        "rx.lock": [
            (rx.lock_thresh, "declare", THRESH),
            (rx.lock_drop_thresh, "drop", THRESH),
        ],
        "rx.sync.lock": [
            (rx.sync_lock_thresh, "declare", THRESH),
            (rx.sync_lock_drop_thresh, "drop", THRESH),
        ],
        "rx.car.freq": [(OFFSET, "actual", ACTUAL)],
        "rx.car.nco": [(OFFSET, "actual", ACTUAL)],
        "rx.sync.rate": [(FS / RS, "actual", ACTUAL)],
        "rx.agc.level_db": [(0.0, "reference", ACTUAL)],
        "rx.sym.i": [(1.0, "ideal", ACTUAL), (-1.0, "ideal", ACTUAL)],
        "rx.sym.q": [(0.0, "ideal", ACTUAL)],
    }

    ncol = 3
    nrow = -(-(len(panels) + 1) // ncol)  # +1 cell for the capture summary
    fig, axes = plt.subplots(
        nrow, ncol, figsize=(12.0, 2.2 * nrow), sharex=True
    )
    axes = axes.ravel()

    # `n` is the sample index the record was stamped with, so the x-axis is
    # real time — not an event ordinal standing in for one.
    for ax, (title, members) in zip(axes, panels):
        for member, color in zip(members, ("#1565c0", "#ef6c00")):
            n, v = series[member]
            # Symbols are DISCRETE samples, not a trajectory: one decision
            # per symbol period, with nothing in between to interpolate
            # through. A line through them draws a path the receiver never
            # took, and on a BPSK stream toggling +-1 it fills the panel
            # solid and shows nothing. Loop state is the opposite -- a
            # continuous quantity sampled per symbol -- so it stays a line.
            if ".sym." in member:
                ax.plot(
                    n / FS * 1e3,
                    v,
                    ".",
                    ms=1.5,
                    color=color,
                    label=member.rsplit(".", 1)[1].upper(),
                )
            else:
                ax.plot(n / FS * 1e3, v, lw=0.8, color=color)
        # Drawn after the traces so a reference sits ON the data, and
        # de-duplicated by VALUE: the timing loop's declare and drop levels
        # are equal (its hysteresis is in the verify counts, not the
        # levels), and two identical lines would imply a gap that is not
        # there. One line, labelled with both names, says the true thing.
        drawn = {}
        for member in members:
            for value, label, color in refs.get(member, ()):
                drawn.setdefault((round(value, 12), color), []).append(label)
        for (value, color), labels in drawn.items():
            ax.axhline(value, color=color, lw=0.8, ls="--", alpha=0.75)
            ax.annotate(
                " = ".join(dict.fromkeys(labels)),
                xy=(1.0, value),
                xycoords=("axes fraction", "data"),
                xytext=(2, 0),
                textcoords="offset points",
                va="center",
                fontsize=6,
                color=color,
            )

        if len(members) > 1:
            # Above the axes, not on them: I fills the +-1 bands edge to
            # edge, so any in-axes corner sits on data.
            ax.legend(
                loc="lower right",
                bbox_to_anchor=(1.0, 1.0),
                ncol=2,
                fontsize=7,
                markerscale=6,
                frameon=False,
                handletextpad=0.2,
                columnspacing=0.8,
            )
        # A 0/1 decision gets a 0/1 axis. Autoscale gives a probe that sits
        # at one level a +-0.05 range, which draws pure axis noise as though
        # it were signal -- reading a flat decision as flat rather than as a
        # wandering trace is the whole point of the panel.
        if title.endswith(".locked"):
            ax.set_ylim(-0.05, 1.05)
        ax.set_title(title, fontsize=9, family="monospace")
        ax.grid(alpha=0.3)
        ax.margins(x=0)

    # The counts are printed to stdout and asserted above; a panel restating
    # them is a caption for the figure's own axes, which the panel titles
    # already are.
    for ax in axes[len(panels) :]:
        ax.axis("off")

    fig.suptitle(
        "Every BpskReceiver probe, one ring, nothing dropped", fontsize=13
    )
    fig.supxlabel("time (ms)")
    fig.tight_layout()
    fig.savefig(out_path, dpi=110)
    print(
        f"captured {len(recs)} records over {len(series)} probes "
        f"({recs.nbytes:,} bytes, nothing dropped) -> {out_path}"
    )


if __name__ == "__main__":
    main(*sys.argv[1:2])
