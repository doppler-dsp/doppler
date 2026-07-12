"""receiver_lock_demo.py — a real closed-loop async DSSS receiver, watching
all three loops' lock decisions on one shared timeline.

Drives the same three-object composition proven end-to-end in
``src/doppler/track/tests/test_async_dsss_receiver.py`` —
``Dll(segments=K) -> Costas -> SymbolSync`` — but with a
``doppler.telemetry.Telemetry`` context attached to all three, so the
``.locked`` decision each loop declares (code, then carrier, then symbol
timing) can be read back and plotted together. This is the "tying it all
together" payoff of the lock-detector consistency pass: one shared
``lockdet_core.h`` decision rule, one shared telemetry bus, three
independent loops.

The signal starts on cold (no code, carrier, or timing knowledge) and the
receiver pulls all three in from scratch — the plot shows the real
acquisition cascade order: code locks first (it has the most processing
gain), then carrier, then symbol timing (which needs the carrier
de-rotated and the code removed before its own statistic is meaningful).

Run:  python -m doppler.examples.receiver_lock_demo  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:chain]
import numpy as np

from doppler.telemetry import Telemetry
from doppler.track import Costas, Dll, SymbolSync

SF, SPS, K = 127, 2, 8  # code chips, samples/chip, partials/epoch
TE = SF * SPS  # code-epoch length, samples
DSYM = 4e-3  # symbol-vs-code rate offset (independent, async clock)
F0 = 3e-4  # residual carrier after acquisition, cycles/sample
NSYM = 600


def make_signal(seed=7):
    """Async-data DSSS-BPSK, cold start: no code/carrier/timing knowledge."""
    rng = np.random.default_rng(seed)
    code = rng.integers(0, 2, SF).astype(np.uint8)
    csign = np.where(code & 1, -1.0, 1.0)
    tsym = TE * (1.0 + DSYM)
    n = int(NSYM * tsym) + 2 * TE
    data = (rng.integers(0, 2, NSYM + 6) * 2 - 1).astype(float)
    idx = np.arange(n)
    si = np.clip(
        np.floor((idx - 0.37 * TE) / tsym).astype(int), 0, len(data) - 1
    )
    cph = (idx // SPS) % SF
    rx = (data[si] * csign[cph] * np.exp(2j * np.pi * F0 * idx)).astype(
        np.complex64
    )
    return code, rx


def run_chain(code, rx, tlm=None, block=2 * TE):
    """Dll(segments=K) -> Costas -> boxcar MF -> SymbolSync, block-wise.

    Telemetry (if attached) is stamped with the RAW input sample index at
    the start of each block, so all three loops' probes -- despite running
    at different internal rates (epochs, partials, symbols) -- land on one
    shared timeline.
    """
    d = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=K)
    cos = Costas(bn=0.02, zeta=0.707, tsamps=1)
    ss = SymbolSync(sps=round(K * (1.0 + DSYM)), bn=0.02, zeta=0.707)
    if tlm is not None:
        d.set_telemetry(tlm, "code")
        cos.set_telemetry(tlm, "car")
        ss.set_telemetry(tlm, "sync")
    for i in range(0, rx.size, block):
        if tlm is not None:
            tlm.set_now(i)
        part = d.steps(rx[i : i + block]).astype(np.complex64)
        wiped = cos.steps(part).astype(np.complex64)
        mf = np.convolve(wiped, np.ones(K), mode="same").astype(np.complex64)
        ss.steps(mf)
    return d, cos, ss


# --8<-- [end:chain]


def main(out_path="receiver_lock_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    code, rx = make_signal()
    tlm = Telemetry(1 << 16)
    d, cos, ss = run_chain(code, rx, tlm)
    assert d.locked and cos.locked and ss.locked

    recs = tlm.read()
    assert tlm.dropped == 0

    fig, ax = plt.subplots(figsize=(9, 3.6))
    colors = {"code": "#1f77b4", "car": "#ff7f0e", "sync": "#2ca02c"}
    for i, prefix in enumerate(["code", "car", "sync"]):
        pid = tlm.probe_id(f"{prefix}.locked")
        r = recs[recs["probe"] == pid]
        y = r["value"].astype(float) + i * 1.15  # stack the three traces
        ax.step(r["n"], y, where="post", color=colors[prefix], lw=1.6)
        ax.text(
            r["n"][0],
            i * 1.15 + 0.5,
            f"{prefix}.locked",
            color=colors[prefix],
            fontsize=9,
            va="center",
            ha="right",
        )
    ax.set_xlabel("raw input sample index")
    ax.set_yticks([])
    ax.set_title(
        "Cold-start async DSSS receiver: Dll -> Costas -> SymbolSync\n"
        "the real acquisition cascade (code, then carrier, then timing)",
        fontsize=10,
    )
    ax.grid(alpha=0.25, axis="x")
    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "receiver_lock_demo.png")
