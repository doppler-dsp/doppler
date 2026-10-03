#!/usr/bin/env python3
"""framed_link_demo.py -- a framed BPSK / QPSK link, checked by its frame.

wfmgen sends frames built from ONE ``FrameDesc`` (a sync word, a payload, a
CRC-16); ``MpskReceiver`` recovers the symbols; the SAME description then
finds and checks every frame. Nothing about the frame is written twice: the
sync word comes from the description's field 0, the frame length from its
layout, and the verdict from ``FrameDesc.check``.

What it shows, in order:

1. Transmit: 200 frames of 48 payload bits at Es/N0 = 14 dB, once as BPSK
   and once as QPSK, with a small carrier offset.
2. Receive: ``MpskReceiver`` locks and hands back matched-filter symbols.
   The carrier loop settles on one of ``M`` equivalent phases, so each phase
   is tried and the one whose bits repeat the sync word at the frame period
   is kept (the sync word resolves the ambiguity, as it does on air).
3. Check: every sliced frame is judged by ``FrameDesc.check``. Every frame
   after the lock transient passes, and every passed frame carries exactly
   the payload that was sent.
4. The check is a real detector: one flipped bit fails it, and at a low
   Es/N0 the damaged frames fail while no damaged frame passes.

Every claim is an explicit check that exits non-zero on failure.

Run: python src/doppler/examples/framed_link_demo.py
"""

# --8<-- [start:link]
import numpy as np

from doppler.mpsk import mpsk_demap
from doppler.track import MpskReceiver
from doppler.wfm import (
    STAGE_CRC16,
    Composer,
    FrameDesc,
    Segment,
    field_bits,
)

SYNC = field_bits("0x1ACFFC1D")  # 32 bits: field 0 of the description
PAYLOAD = 48
SPS = 8
SETTLE = 12  # frames the carrier and timing loops may take to settle

d = FrameDesc()
d.add_field("sync", SYNC)
d.add_data("payload", PAYLOAD)
d.add_derived("crc", 16)
d.add_stage_over(STAGE_CRC16, "payload", "crc")
d.build()
NB = d.nbits  # the frame's length, from the layout
POFF = d.field_off(d.field_index("payload"))  # where the payload starts


def transmit(kind, esn0_db, frames, seed=4):
    """Frames of random payload -> (truth chunks, complex baseband)."""
    rng = np.random.default_rng(seed)
    data = rng.integers(0, 2, frames * PAYLOAD, dtype=np.uint8)
    seg = Segment(
        type=kind,
        sps=SPS,
        fs=1.0,
        freq=0.0002,  # a carrier offset the receiver must remove
        pulse="rrc",
        level=-6.0,
        snr=esn0_db,
        snr_mode="esno",
        seed=seed,
        frame=d,
        data=data,
    )
    return data.reshape(-1, PAYLOAD), np.asarray(Composer([seg]).compose())


def receive(m, iq):
    """Matched-filter symbols from MpskReceiver."""
    rx = MpskReceiver(
        m=m,
        sps=SPS,
        m_out=SPS,  # one matched-filter output per symbol
        pulse="rrc",
        rrc_beta=0.35,
        rrc_span=8,
        bn_carrier=0.005,
        bn_timing=0.01,
        lock_thresh=0.65,
    )
    return rx, np.asarray(rx.steps(iq))


def to_bits(sym, m, k):
    """Symbols -> bits for carrier-phase ambiguity ``k`` (MSB first)."""
    bps = {2: 1, 4: 2}[m]
    rot = (sym * np.exp(-2j * np.pi * k / m)).astype(np.complex64)
    lab = mpsk_demap(rot, m)
    shifts = np.arange(bps - 1, -1, -1)
    return ((lab[:, None] >> shifts) & 1).astype(np.uint8).ravel()


# Retires when dp_syncword_search gets a Python face (#1803).
def sync_starts(bits):
    """Offsets where the description's sync word matches exactly."""
    win = np.lib.stride_tricks.sliding_window_view(bits, SYNC.size)
    return np.flatnonzero((win == SYNC).all(axis=1))


def slice_frames(sym, m):
    """Resolve the phase ambiguity by the sync word, then cut frames."""
    best = None
    for k in range(m):
        bits = to_bits(sym, m, k)
        st = sync_starts(bits)
        ok = np.intersect1d(st, st + NB)  # repeats at the frame period
        if best is None or ok.size > best[2].size:
            best = (k, bits, ok)
    k, bits, ok = best
    s0 = int(ok[0]) % NB if ok.size else 0
    n = (bits.size - s0) // NB
    return k, bits[s0 : s0 + n * NB].reshape(n, NB)


def link(kind, m, esn0_db, frames=200):
    chunks, iq = transmit(kind, esn0_db, frames)
    rx, sym = receive(m, iq)
    k, got = slice_frames(sym, m)
    verdicts = [d.check(f) for f in got]
    # Which sent frame is got[0]? Frames are unique, so match a passing one.
    first = next(
        (
            i
            for i, v in enumerate(verdicts)
            if v.passed
            and any(
                np.array_equal(got[i, POFF : POFF + PAYLOAD], c)
                for c in chunks
            )
        ),
        None,
    )
    return rx, chunks, got, verdicts, k, first


# --8<-- [end:link]


def main():
    for kind, m in (("bpsk", 2), ("qpsk", 4)):
        rx, chunks, got, verdicts, k, first = link(kind, m, 14.0)
        passed = [v.passed for v in verdicts]
        assert rx.locked, f"{kind}: receiver did not lock"
        assert first is not None, f"{kind}: no frame passed the check"
        # The loops settle within SETTLE frames (QPSK, with half the
        # symbols per bit, takes longest); after that nothing may fail.
        assert first < SETTLE, f"{kind}: first good frame {first}"
        tail = passed[SETTLE:]
        assert all(tail), f"{kind}: a frame failed after lock"
        n_ok = sum(passed)
        assert n_ok >= len(chunks) - SETTLE, f"{kind}: only {n_ok} frames"
        sent = {c.tobytes() for c in chunks}
        for i, v in enumerate(verdicts):
            if v.passed:
                assert got[i, POFF : POFF + PAYLOAD].tobytes() in sent, (
                    f"{kind}: frame {i} passed with a payload never sent"
                )
        # One flipped payload bit must fail the description's check.
        good = got[first].copy()
        good[POFF + 4] ^= 1
        assert not d.check(good).passed, f"{kind}: damage went undetected"
        print(
            f"{kind}: phase k={k}, {n_ok}/{len(chunks)} frames passed, "
            f"payloads exact, corrupted frame rejected"
        )

    # Low Es/N0: damaged frames fail; none passes with a wrong payload.
    rx, chunks, got, verdicts, k, first = link("bpsk", 2, 4.0)
    n_fail = sum(not v.passed for v in verdicts)
    assert 2 * n_fail > len(verdicts), "expected most frames to fail at 4 dB"
    sent = {c.tobytes() for c in chunks}
    wrong = sum(
        v.passed and got[i, POFF : POFF + PAYLOAD].tobytes() not in sent
        for i, v in enumerate(verdicts)
    )
    assert wrong == 0, f"{wrong} damaged frames passed the check"
    print(f"bpsk @ 4 dB: {n_fail} frames failed the check, 0 passed wrong")
    print("framed_link_demo: OK")


if __name__ == "__main__":
    main()
