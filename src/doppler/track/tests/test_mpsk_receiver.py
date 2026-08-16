"""Integration tests for track.MpskReceiver (pulse-shaped M-PSK modem)."""

import numpy as np
import pytest

from doppler.track import ContinuousMpskReceiver, MpskReceiver

PHI0 = {2: 0.0, 4: np.pi / 4, 8: 0.0}


def _signal(m, sps=8, foff=0.0, snr_db=30.0, nsym=5000, seed=0):
    """Rectangular (I&D-matched) M-PSK with a carrier offset + AWGN."""
    rng = np.random.default_rng(seed)
    idx = rng.integers(0, m, nsym)
    syms = np.exp(1j * (2 * np.pi * idx / m + PHI0[m])).astype(np.complex64)
    tx = np.repeat(syms, sps).astype(np.complex64)
    n = np.arange(tx.size)
    tx = tx * np.exp(1j * 2 * np.pi * foff * n)
    sigma = np.sqrt(0.5 / 10 ** (snr_db / 10))
    tx = tx + (
        rng.normal(0, sigma, tx.size) + 1j * rng.normal(0, sigma, tx.size)
    )
    return tx.astype(np.complex64), idx


def _ser(out, idx, m):
    """Genie SER: best over the M-fold rotation and a small symbol lag."""
    th = np.angle(out) - PHI0[m]
    oi = np.round(th * m / (2 * np.pi)).astype(int) % m
    lo, hi = out.size // 3, 2 * out.size // 3
    best = 1.0
    for lag in range(-30, 31):
        base = np.arange(lo, hi) + lag
        if base.min() < 0 or base.max() >= idx.size:
            continue
        a, b = oi[lo:hi], idx[base]
        for r in range(m):
            best = min(best, float(np.mean(((a - b - r) % m) != 0)))
    return best


@pytest.mark.parametrize("m", [2, 4, 8])
def test_steps_recovers_symbols(m):
    """Acquires the carrier (NDA) + timing and recovers symbols, every M."""
    tx, idx = _signal(m, foff=0.0008, snr_db=30, seed=m)
    rx = MpskReceiver(
        m=m,
        sps=8,
        m_out=4,
        bn_carrier=0.005,
        bn_timing=0.01,
        init_norm_freq=0.0008,
    )
    out = rx.steps(tx)
    assert out.dtype == np.complex64
    assert _ser(out, idx, m) < 0.02
    assert rx.lock > 0.15  # orientation-normalised lock is positive at lock


def test_defaults_and_keywords():
    """All ctor params default; keyword construction (no forced positional)."""
    rx = MpskReceiver()  # QPSK, sps=8, m_out=8, I&D
    assert rx.m == 4 and rx.sps == 8 and rx.m_out == 8 and rx.tracking == 0
    rx2 = MpskReceiver(m=2, sps=4, m_out=2, pulse="iandd")
    assert rx2.m == 2 and rx2.sps == 4 and rx2.m_out == 2


def test_invalid_args_raise():
    # A bad parameter is a ValueError naming the constraint, not the blanket
    # MemoryError a NULL create() used to surface (jm gh-482 create_error).
    with pytest.raises(ValueError):
        MpskReceiver(m=3)  # M not in {2,4,8}
    with pytest.raises(ValueError):
        MpskReceiver(m=4, sps=8, m_out=3)  # m_out must be even
    with pytest.raises(ValueError):
        MpskReceiver(m=4, sps=2, m_out=4)  # sps < m_out: would interpolate
    with pytest.raises(ValueError):
        MpskReceiver(pulse="rrc", rrc_span=0)  # invalid RRC geometry


def test_properties():
    """Read-only metrics and the writable norm_freq round-trip."""
    rx = MpskReceiver(m=8, sps=8, m_out=4)
    assert rx.m == 8 and rx.m_out == 4 and rx.sps == 8
    assert rx.lock == 0.0 and rx.tracking == 0
    assert rx.timing_rate == pytest.approx(8.0)  # seeded at nominal sps
    rx.norm_freq = 0.01
    assert rx.norm_freq == pytest.approx(0.01)


def test_bits_coherent():
    """Coherent bits() (default, non-differential) recovers a known stream."""
    tx, idx = _signal(2, foff=0.0, snr_db=30, nsym=3000, seed=3)
    rx = MpskReceiver(m=2, sps=8, m_out=4, bn_carrier=0.005)  # differential=0
    b = rx.bits(tx)
    assert b.dtype == np.uint8
    assert set(np.unique(b)).issubset({0, 1})
    # BPSK bit == symbol index up to the 2-fold (inversion) ambiguity and a
    # small loop/filter lag.
    best = 1.0
    for lag in range(-3, 4):
        base = np.arange(500, 1500) + lag
        if base.min() < 0 or base.max() >= idx.size:
            continue
        err = float(np.mean(b[500:1500] != idx[base]))
        best = min(best, err, 1 - err)
    assert best < 0.02


def test_rrc_pulse_recovers():
    """RRC matched filter on a true RRC-shaped TX recovers QPSK.

    Previously xfail: the original integrate-and-dump CarrierNda arm pulled in
    too slowly/jitterily on a pulse-shaped (RRC) arm for the downstream matched
    filter + Gardner timing to recover the symbols. The CarrierNda rework (raw
    M-th-power discriminator on a per-sample boxcar moving-average arm + arm
    AGC) fixed the pull-in; QPSK now recovers at SER 0 (verified across seeds).
    """
    from doppler.filter import FIR

    m, sps, beta, span, nsym = 4, 8, 0.35, 8, 6000
    rng = np.random.default_rng(11)
    idx = rng.integers(0, m, nsym)
    syms = np.exp(1j * (2 * np.pi * idx / m + PHI0[m])).astype(np.complex64)
    up = np.zeros(nsym * sps, np.complex64)
    up[::sps] = syms
    N = 2 * span * sps + 1
    t = (np.arange(N) - span * sps) / sps
    taps = np.zeros(N)
    for i, ti in enumerate(t):
        if abs(ti) < 1e-8:
            taps[i] = 1 - beta + 4 * beta / np.pi
        elif abs(abs(ti) - 1 / (4 * beta)) < 1e-8:
            taps[i] = (beta / np.sqrt(2)) * (
                (1 + 2 / np.pi) * np.sin(np.pi / (4 * beta))
                + (1 - 2 / np.pi) * np.cos(np.pi / (4 * beta))
            )
        else:
            num = np.sin(np.pi * ti * (1 - beta)) + 4 * beta * ti * np.cos(
                np.pi * ti * (1 + beta)
            )
            taps[i] = num / (np.pi * ti * (1 - (4 * beta * ti) ** 2))
    taps = (taps / np.sqrt(np.sum(taps**2))).astype(np.complex64)
    tx = FIR(taps).execute(up).astype(np.complex64)
    sigma = np.sqrt(0.5 / 10 ** (28 / 10)) * np.sqrt(np.mean(np.abs(tx) ** 2))
    tx = (
        tx + rng.normal(0, sigma, tx.size) + 1j * rng.normal(0, sigma, tx.size)
    ).astype(np.complex64)
    rx = MpskReceiver(
        m=4,
        sps=8,
        m_out=4,
        pulse="rrc",
        rrc_beta=beta,
        rrc_span=span,
        bn_carrier=0.005,
        bn_timing=0.005,
    )
    out = rx.steps(tx)
    assert _ser(out, idx, m) < 0.02


def test_acq_to_track_engages():
    """acq_to_track flips the loop from NDA acquisition to DD tracking."""
    tx, idx = _signal(4, foff=0.0008, snr_db=25, seed=4)
    rx = MpskReceiver(
        m=4,
        sps=8,
        m_out=4,
        init_norm_freq=0.0008,
        acq_to_track=1,
        lock_thresh=0.4,
        bn_carrier=0.005,
    )
    out = rx.steps(tx)
    assert rx.tracking == 1
    assert _ser(out, idx, 4) < 0.02


def test_acq_to_track_off_by_default():
    tx, _ = _signal(4, snr_db=30, seed=5)
    rx = MpskReceiver(m=4, sps=8, m_out=4)
    rx.steps(tx)
    assert rx.tracking == 0  # opt-in: stays in NDA tracking


def test_acq_to_track_two_way():
    """A sustained lock loss drops back to the NDA acquisition steer.

    The handover is verify-counted both ways (8 symbols up, 32 down with a
    0.8x drop threshold), so a noise-dominated stretch drops tracking; a
    returning signal re-declares after the carrier is re-seeded (on a real
    drop-back the outer acquisition supplies that seed — during the outage
    the discriminators see only noise and random-walk the shared NCO).

    **The drop-back is asserted as an EVENT, not as the state at the end of
    the noise.** What the design promises is that a sustained loss drops
    tracking; it does not promise the receiver then *stays* dropped while
    being fed noise, and it cannot: with nothing but noise in, the M-th-power
    lock metric random-walks, and 8 consecutive samples above `lock_thresh`
    is a bar noise clears from time to time. Measured over this stretch it
    reaches +0.52 against a 0.4 declare threshold. So the final state after
    5000 noise symbols is a sample of that walk — it was asserted as 0 here
    and held only until the arm-AGC seeding changed by a few LSBs, after
    which it still held on one machine's libm and flipped on CI's. Asserting
    the transition itself tests the documented behaviour and is immune to
    where the walk happens to end.
    """
    foff = 0.0008
    tx, _ = _signal(4, foff=foff, snr_db=25, seed=4)
    noise, _ = _signal(4, foff=foff, snr_db=-10, seed=6)
    rx = MpskReceiver(
        m=4,
        sps=8,
        m_out=4,
        init_norm_freq=foff,
        acq_to_track=1,
        lock_thresh=0.4,
        bn_carrier=0.005,
    )
    rx.steps(tx)
    assert rx.tracking == 1

    # Feed the outage in blocks and watch for the drop-back.
    blk = len(noise) // 10
    dropped = False
    for i in range(10):
        rx.steps(noise[i * blk : (i + 1) * blk])
        dropped = dropped or rx.tracking == 0
    assert dropped, "a sustained lock loss never dropped back to the NDA steer"

    rx.norm_freq = foff  # acquisition re-seed
    rx.steps(tx)
    assert rx.tracking == 1  # re-declared


def test_configure_lock_unreachable_threshold_never_engages():
    tx, _ = _signal(4, foff=0.0008, snr_db=25, seed=4)
    rx = MpskReceiver(
        m=4,
        sps=8,
        m_out=4,
        init_norm_freq=0.0008,
        acq_to_track=1,
        bn_carrier=0.005,
    )
    rx.configure_lock(up_thresh=2.0, down_thresh=1.9, n_up=1, n_down=1)
    rx.steps(tx)
    assert rx.tracking == 0


def test_configure_lock_low_threshold_engages_fast():
    # n_up=1 with an easily-reachable threshold hands over on the first
    # above-threshold symbol once warmup has elapsed.
    tx, idx = _signal(4, foff=0.0008, snr_db=25, seed=4)
    rx = MpskReceiver(
        m=4,
        sps=8,
        m_out=4,
        init_norm_freq=0.0008,
        acq_to_track=1,
        bn_carrier=0.005,
    )
    rx.configure_lock(up_thresh=0.1, down_thresh=0.05, n_up=1, n_down=32)
    out = rx.steps(tx)
    assert rx.tracking == 1
    assert _ser(out, idx, 4) < 0.02


@pytest.mark.parametrize("m", [2, 4, 8])
def test_bits_differential_rotation_invariant(m):
    """Differential bits survive an arbitrary fixed carrier-phase rotation."""
    from doppler.mpsk import mpsk_diff_map

    bps = {2: 1, 4: 2, 8: 3}[m]
    rng = np.random.default_rng(20 + m)
    nsym = 4000
    txbits = rng.integers(0, 2, nsym * bps).astype(np.uint8)
    labels = np.array(
        [
            sum(int(txbits[i * bps + b]) << b for b in range(bps))
            for i in range(nsym)
        ],
        np.uint8,
    )
    pts = mpsk_diff_map(labels, m).astype(np.complex64)
    tx = np.repeat(pts, 8).astype(np.complex64) * np.exp(
        1j * 0.7
    )  # phase slip
    sigma = np.sqrt(0.5 / 10 ** (30 / 10))
    tx = (
        tx + rng.normal(0, sigma, tx.size) + 1j * rng.normal(0, sigma, tx.size)
    ).astype(np.complex64)
    # acq_to_track for 8PSK: differential demapping roughly doubles the
    # symbol-error rate, and 8PSK's decision margin is only +-pi/8, so the
    # NDA M-th-power discriminator's own phase jitter is the dominant error
    # term. Handing the carrier to the low-jitter decision-directed loop is
    # the documented remedy (mpsk_receiver_ber.c makes the same call), and
    # it is free for the lower orders.
    rx = MpskReceiver(
        m=m,
        sps=8,
        m_out=4,
        bn_carrier=0.005,
        bn_timing=0.01,
        differential=1,
        acq_to_track=1,
        lock_thresh=0.3,
    )
    rb = rx.bits(tx)
    assert set(np.unique(rb)).issubset({0, 1})
    best = 1.0
    # NEGATIVE lags are in the sweep because the interpolator's rule (emit
    # every tick, load on u(k) <= u(k-1)) made the block path emit before it
    # loads, so the recovered stream LEADS the payload by a symbol. The
    # sweep was range(0, 6) and could not express that, which read as a
    # coin-flip BER rather than as an alignment miss.
    for lag in range(-3, 6):
        a = rb[1000 * bps : 2000 * bps]
        b = txbits[(1000 + lag) * bps : (2000 + lag) * bps]
        if a.size == b.size:
            best = min(best, float(np.mean(a != b)))
    # 8PSK differential sits on a genuinely higher floor than the lower
    # orders: differential demapping roughly doubles the symbol-error rate
    # and 8PSK's decision margin is only +-pi/8, so what is left is the
    # carrier loop's own phase jitter. Measured 0.036 on the rebuilt engine
    # against 0.005 for BPSK/QPSK; this is a tolerance, not a pin, and it is
    # one of the numbers the cascade rebuild moved.
    assert best < (0.02 if m < 8 else 0.05)


def test_block_size_invariance():
    """Streaming over chunks == one block (state carries across calls)."""
    tx, _ = _signal(4, foff=0.0005, snr_db=30, nsym=3000, seed=7)
    whole = MpskReceiver(m=4, sps=8, m_out=4, init_norm_freq=0.0005).steps(tx)
    rx = MpskReceiver(m=4, sps=8, m_out=4, init_norm_freq=0.0005)
    parts = [rx.steps(tx[i : i + 1000]) for i in range(0, tx.size, 1000)]
    chunked = np.concatenate(parts)
    assert chunked.size == whole.size
    assert np.allclose(chunked, whole, atol=1e-4)


def test_empty_input():
    rx = MpskReceiver(m=4, sps=8, m_out=4)
    out = rx.steps(np.zeros(0, np.complex64))
    assert out.size == 0


def test_reset_reproducible():
    tx, _ = _signal(4, foff=0.0008, snr_db=30, seed=9)
    rx = MpskReceiver(m=4, sps=8, m_out=4, init_norm_freq=0.0008)
    first = rx.steps(tx)
    rx.reset()
    assert rx.tracking == 0
    second = rx.steps(tx)
    assert np.array_equal(first, second)


# ── ContinuousMpskReceiver — the continuous flavor (design/mpsk.md §2.1) ──


def test_continuous_view_pins_the_gating():
    """The flavor's whole claim: no handover, no gate, nothing to wait for.

    It is a jm VIEW over the same core, so the surface must be identical and
    only the constructor may differ -- that is what separates a flavor from a
    second type in this project. Checked both ways round: every knob the view
    pins must be REFUSED by its constructor, and every method/property the
    base has must still be reachable.
    """
    rx = ContinuousMpskReceiver(m=2, sps=8.0)

    # The pinned knobs are gone from the constructor, not merely defaulted.
    for kw in (
        "lock_thresh",
        "acq_to_track",
        "nda_tap",
        "agc",
        "m_out",
        "zeta",
        "num_phases",
        "bn_agc_ratio",
    ):
        with pytest.raises(TypeError):
            ContinuousMpskReceiver(m=2, sps=8.0, **{kw: 1})

    # ... but every one is still READABLE, which is what makes a pinned value
    # checkable rather than hidden. These are the derived §8.1 answers.
    assert rx.m_out == 8
    assert rx.num_phases == 64
    assert rx.zeta == pytest.approx(0.70710678118654752, abs=1e-15)
    assert rx.lock_thresh == pytest.approx(0.4999, abs=1e-15)
    assert rx.bn_agc_ratio == pytest.approx(0.05, abs=1e-15)

    # Same object, same surface. A view shares its parent's methods verbatim.
    base = {d for d in dir(MpskReceiver) if not d.startswith("_")}
    view = {d for d in dir(ContinuousMpskReceiver) if not d.startswith("_")}
    assert base == view, f"surface differs: {base ^ view}"


def test_continuous_view_never_hands_over():
    """`tracking` is 0 from the first output to the last.

    The handover-enabled receiver on the SAME stimulus is the control. Without
    it this asserts nothing: `tracking == 0` is equally satisfied by a signal
    that never locked at all, or by a handover that is simply broken.
    """
    tx, idx = _signal(2, foff=0.0008, snr_db=30, seed=21)

    rx = ContinuousMpskReceiver(m=2, sps=8.0, bn_carrier=0.02, bn_timing=0.01)
    out = rx.steps(tx)
    assert out.size > 0
    assert rx.lock > 0.5  # it genuinely locked
    assert rx.tracking == 0  # and never flipped
    assert _ser(out, idx, 2) < 0.01

    # The control: this stimulus DOES drive a handover when one is enabled.
    handover = MpskReceiver(
        m=2,
        sps=8.0,
        m_out=8,
        bn_carrier=0.02,
        bn_timing=0.01,
        acq_to_track=1,
        lock_thresh=0.65,
    )
    handover.steps(tx)
    assert handover.tracking == 1, "control failed — the stimulus cannot lock"


def test_continuous_view_tracks_the_carrier():
    """Acquires with no gate in the way — the tap is coupled, not gated.

    `strobe` reads the on-time output, so its input quality depends on the
    timing loop converging; what it does NOT do is wait for it. The
    discriminator steers from the first strobe whether or not timing has
    declared anything, which is the property "nothing waits" actually names
    (docs/design/mpsk.md §3.3).
    """
    tx, _ = _signal(2, foff=0.0008, snr_db=30, seed=5)
    rx = ContinuousMpskReceiver(m=2, sps=8.0, bn_carrier=0.02, bn_timing=0.01)
    rx.steps(tx)
    assert rx.norm_freq == pytest.approx(0.0008, abs=1e-4)
