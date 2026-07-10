"""telemetry_fanin_demo.py — many emitters, one ring, one consumer.

Attaches three independent DSP objects to a **single** telemetry
context — an M-PSK receiver (``rx.*``), a DSSS code loop (``code0.*``)
and an AGC (``agc.*``) — and drives them block by block on one producer
thread while a single consumer drains the shared ring, demuxes by probe
name, and writes the capture to disk. This is the in-process model for
"take it all in": one ring, distinct prefixes, one ``read()`` loop.

The SPSC contract makes this safe by construction: *any number of
emitters* may share a context as long as they all step on one producer
thread (multiple threads or processes get a ring each — see the gallery
page for the NATS fan-in pattern that unifies those).

The scenario is built so every stream has a story:

  * ``rx``   — QPSK with a mid-stream outage: the carrier lock EMA
    (``rx.lock``) collapses, the two-way handover (``rx.tracking``)
    drops back to NDA acquisition, and re-declares when the signal
    returns (the carrier is re-seeded on drop-back, as a real outer
    acquisition would).
  * ``code0`` — PN-31 DSSS BPSK seeded half a chip off into a
    partial-correlation Dll: the code-lock CFAR statistic
    (``code0.lock``) climbs through pull-in and the verify-counted
    decision (``code0.locked``) declares once, two consecutive 20-look
    windows in.
  * ``agc``  — a -12 dB input level step: ``agc.gain_db`` ramps to
    re-level the output.

Run:  python -m doppler.examples.telemetry_fanin_demo  [out.png]
"""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

import numpy as np

from doppler.agc import AGC
from doppler.detection import det_threshold_noncoherent
from doppler.telemetry import Telemetry
from doppler.track import Dll, MpskReceiver

SPS = 8  # rx samples/symbol
FOFF = 8e-4  # rx carrier offset, cycles/sample
CODE_LEN, CHIP_SPS = 31, 4  # ch0 spreading code / samples per chip
N_CLEAN = 3000 * SPS  # rx segment lengths, samples
N_OUT = 2000 * SPS
N = 2 * N_CLEAN + N_OUT  # common stream length for all three
BLK = 512  # producer block size (one set_now per block)


def _qpsk(n_samples, snr_db, seed):
    """Rectangular-pulse QPSK at SPS samples/symbol with AWGN + FOFF."""
    rng = np.random.default_rng(seed)
    nsym = n_samples // SPS
    idx = rng.integers(0, 4, nsym)
    syms = np.exp(1j * (2 * np.pi * idx / 4 + np.pi / 4))
    tx = np.repeat(syms, SPS)
    tx *= np.exp(2j * np.pi * FOFF * np.arange(tx.size))
    sigma = np.sqrt(0.5 / 10 ** (snr_db / 10))
    tx += rng.normal(0, sigma, tx.size) + 1j * rng.normal(0, sigma, tx.size)
    return tx.astype(np.complex64)


def _mseq31():
    """Length-31 m-sequence (x^5 + x^2 + 1) — a low-sidelobe PN code.

    The code-lock detector's CFAR reference correlates at a random
    off-peak chip offset, which is only signal-free for a code with low
    periodic autocorrelation sidelobes (an m-sequence's are -1/31); a
    degenerate code (e.g. alternating 0/1, whose autocorrelation is
    2-periodic) would put signal in the noise tap and kill the ratio.
    """
    reg = np.ones(5, dtype=int)
    out = np.empty(31, dtype=np.uint8)
    for i in range(31):
        out[i] = reg[-1]
        fb = reg[4] ^ reg[1]
        reg[1:] = reg[:-1]
        reg[0] = fb
    return out


def main(out_path="telemetry_fanin_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    # ── the three emitters, one shared context ──────────────────────
    tlm = Telemetry(1 << 14)

    rx = MpskReceiver(
        m=4,
        sps=SPS,
        n=4,
        init_norm_freq=FOFF,
        acq_to_track=1,
        lock_thresh=0.4,
        warmup_syms=200,
        bn_carrier=0.03,
    )
    rx.set_telemetry(tlm, "rx")

    code = _mseq31()
    ch = Dll(code=code, sps=CHIP_SPS, init_chip=0.5, bn=0.005, segments=4)
    ch.set_telemetry(tlm, "code0")

    agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
    agc.set_telemetry(tlm, "agc")

    n_probes = len(tlm.probe_names())
    print(f"one ring, three emitters, {n_probes} probes:")
    for name, pid in sorted(tlm.probe_names().items()):
        print(f"  {pid:3d}  {name}")

    # ── per-emitter input streams (same length, shared sample clock) ─
    sig_rx = np.concatenate(
        [
            _qpsk(N_CLEAN, 25.0, seed=4),
            _qpsk(N_OUT, -10.0, seed=6),  # outage: noise-dominated
            _qpsk(N_CLEAN, 25.0, seed=5),
        ]
    )
    rng_ch = np.random.default_rng(11)
    chips = (1.0 - 2.0 * (code % 2)).astype(np.complex64)
    sig_ch = np.tile(
        np.repeat(chips, CHIP_SPS), N // (CODE_LEN * CHIP_SPS) + 1
    )[:N]
    sig_ch = sig_ch + 0.126 * (  # ~15 dB/sample AWGN
        rng_ch.standard_normal(N) + 1j * rng_ch.standard_normal(N)
    ).astype(np.complex64)
    sig_agc = np.full(N, 0.5, np.complex64)
    sig_agc[N // 2 :] = 0.125  # -12 dB level step mid-stream

    # ── produce block-wise; the single consumer drains per block ────
    chunks = []
    reseeded = False
    for i in range(0, N, BLK):
        if not reseeded and i >= N_CLEAN + N_OUT:
            rx.norm_freq = FOFF  # acq re-seed on drop-back (outage walked
            reseeded = True  # the NCO; see the lockdet gallery page)
        tlm.set_now(i)
        rx.steps(sig_rx[i : i + BLK])
        ch.steps(sig_ch[i : i + BLK])
        agc.steps(sig_agc[i : i + BLK])
        chunks.append(tlm.read())  # ONE consumer, keeping up
    recs = np.concatenate(chunks)
    assert tlm.dropped == 0  # the drain cadence kept the ring ahead
    assert np.all(np.diff(recs["n"].astype(np.int64)) >= 0)  # time-ordered

    # ── the disk model: the capture IS the wire format ───────────────
    cap = Path(tempfile.mkdtemp()) / "tlm_capture.npy"
    np.save(cap, recs)
    back = np.load(cap)
    assert np.array_equal(back, recs)
    print(f"\ncaptured {len(recs)} records ({recs.nbytes} bytes) -> {cap}")

    # ── demux by probe name (id -> per-source series) ────────────────
    def series(name):
        return recs[recs["probe"] == tlm.probe_id(name)]["value"]

    per_emitter = {
        p: int(
            np.sum(
                [len(series(n)) for n in tlm.probe_names() if n.startswith(p)]
            )
        )
        for p in ("rx.", "code0.", "agc.")
    }
    for p, cnt in per_emitter.items():
        print(f"  {p:5s} {cnt} records")

    # ── figure: three emitters on one shared sample axis ────────────
    # Each probe emits at a fixed event rate (per symbol / code period /
    # gain update), so the event ordinal maps back to sample time; the
    # block-level `n` stamp (set_now) keeps the sources aligned.
    lock = series("rx.lock")
    trk = series("rx.tracking")
    clk = series("code0.lock")
    cld = series("code0.locked")
    gdb = series("agc.gain_db")
    t_rx = np.arange(lock.size) * SPS
    t_ch = np.arange(clk.size) * CODE_LEN * CHIP_SPS
    t_ag = np.arange(gdb.size) * (N // max(gdb.size, 1))

    fig, (ax0, ax1, ax2) = plt.subplots(3, 1, figsize=(9.6, 8.0), sharex=True)
    ax0.plot(t_rx, lock, lw=0.7, color="#607d8b", label="rx.lock")
    ax0.plot(
        t_rx,
        trk * 0.9,
        lw=1.4,
        color="#1565c0",
        label="rx.tracking (scaled)",
    )
    ax0.axvspan(N_CLEAN, N_CLEAN + N_OUT, color="#b71c1c", alpha=0.08)
    ax0.text(N_CLEAN + N_OUT / 2, 0.98, "outage", ha="center", color="#b71c1c")
    ax0.set_ylim(-0.6, 1.1)  # outage spikes clip; the story is the flags
    ax0.set_ylabel("rx")
    ax0.legend(loc="lower right", fontsize=8)
    ax0.set_title("one ring, three emitters, one consumer")

    eta = det_threshold_noncoherent(1e-3, 20)  # the default CFAR eta
    ax1.plot(t_ch, clk, lw=0.9, color="#607d8b", label="code0.lock")
    ax1.plot(
        t_ch,
        cld * clk.max(),
        lw=1.4,
        color="#2e7d32",
        label="code0.locked (scaled)",
    )
    ax1.axhline(eta, color="#e65100", lw=1.0, ls="--", label="CFAR η")
    ax1.set_ylabel("code0")
    ax1.legend(loc="lower right", fontsize=8)

    ax2.plot(t_ag, gdb, lw=1.2, color="#e65100", label="agc.gain_db")
    ax2.axvline(N // 2, color="#b71c1c", lw=0.8, ls="--")
    ax2.text(
        N // 2,
        np.mean(gdb),
        "  -12 dB input step",
        color="#b71c1c",
        fontsize=8,
    )
    ax2.set_ylabel("agc, dB")
    ax2.set_xlabel("sample index")
    ax2.legend(loc="lower right", fontsize=8)

    fig.tight_layout()
    fig.savefig(out_path, dpi=110)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(*sys.argv[1:2])
