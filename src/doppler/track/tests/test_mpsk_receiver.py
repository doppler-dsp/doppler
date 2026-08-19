"""Integration tests for track.MpskReceiver (pulse-shaped M-PSK modem)."""

import numpy as np
import pytest

from doppler.track import MpskReceiver

from ._mpsk_rx_harness import freq_offset_inside_bw

PHI0 = {2: 0.0, 4: np.pi / 4, 8: 0.0}


def _seed(foff, m, bn_carrier, sps=8):
    """The `init_norm_freq` that leaves exactly one bound of RESIDUAL.

    Every construction in this file used to be seeded with `init_norm_freq`
    set to the stimulus's own `foff`, i.e. handed the answer. The carrier loop
    then starts on truth and never leaves its initial state, so the acquisition
    half of each of those tests -- "acquires the carrier (NDA)", "flips from
    NDA acquisition to DD tracking", "re-declares after a re-seed" -- was
    asserted against a loop that had nothing to acquire. A receiver whose
    carrier discriminator was wired to nothing passes all of them.

    So seed one acquisition bound BELOW truth: `bn_carrier / m` cycles per
    symbol, the `m` because the NDA discriminator is an M-th power. That is
    the same seed-at-the-bound rule the C tests and the validation harnesses
    hold to, and `freq_offset_inside_bw` is the one place it is computed.
    Measured 2026-08-17, acquisition is reliable out to 4x the bound and
    collapses by 6x, so seeding at the bound keeps a 4x margin -- these tests
    must not be the ones that turn a receiver change into a coin flip.

    `bn_carrier` is passed rather than read back off the receiver because the
    seed has to be known BEFORE construction; state it at the call site and
    hand the same value to the constructor, so the two cannot drift.
    """
    # The helper answers in cycles per SYMBOL; `foff` and `init_norm_freq`
    # are cycles per SAMPLE, so the conversion happens here, once.
    return foff - freq_offset_inside_bw(bn_carrier, m, 1.0) / sps


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
    bn, foff = 0.005, 0.0008
    tx, idx = _signal(m, foff=foff, snr_db=30, seed=m)
    rx = MpskReceiver(
        m=m,
        sps=8,
        m_out=4,
        bn_carrier=bn,
        bn_timing=0.01,
        init_norm_freq=_seed(foff, m, bn),
    )
    out = rx.steps(tx)
    assert out.dtype == np.complex64
    assert _ser(out, idx, m) < 0.02
    assert rx.lock > 0.15  # orientation-normalised lock is positive at lock


def test_defaults_and_keywords():
    """All ctor params default; keyword construction (no forced positional)."""
    rx = MpskReceiver()  # QPSK, sps=8, m_out=8, I&D
    assert rx.m == 4 and rx.sps == 8 and rx.m_out == 8 and rx.locked == 0
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
    assert rx.lock == 0.0 and rx.locked == 0 and rx.lock_time == -1
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


def test_carrier_lock_declares_and_demodulates():
    """The lock indicator declares on a signal the receiver demodulates.

    This was `test_acq_to_track_engages` until doppler#877 deleted the
    handover. The observable that replaces `tracking` is the indicator the
    receiver still publishes -- and pairing it with the SER is the point: a
    declaration on a stream that does not demodulate would be a false alarm,
    and a demodulating stream that never declares would make the indicator
    useless to a caller sizing a measurement window.
    """
    bn, foff = 0.005, 0.0008
    tx, idx = _signal(4, foff=foff, snr_db=25, seed=4)
    rx = MpskReceiver(
        m=4,
        sps=8,
        m_out=4,
        init_norm_freq=_seed(foff, 4, bn),
        lock_thresh=0.4,
        bn_carrier=bn,
    )
    out = rx.steps(tx)
    assert rx.locked == 1
    assert rx.lock > 0.4
    assert _ser(out, idx, 4) < 0.02


def test_carrier_lock_withdraws_and_re_declares():
    """A sustained lock loss withdraws the declaration; a signal restores it.

    The indicator is verify-counted both ways (8 symbols up, 32 down with a
    0.8x drop threshold). This was `test_acq_to_track_two_way`, which asked
    the same question of the handover's detector; the two were stepped on the
    same statistic with the same constants, so the surviving one inherits the
    case unchanged.

    **The withdrawal is asserted as an EVENT, not as the state at the end of
    the noise.** What the design promises is that a sustained loss withdraws
    the declaration; it does not promise the receiver then *stays* withdrawn
    while being fed noise, and it cannot: with nothing but noise in, the
    M-th-power lock metric random-walks, and 8 consecutive samples above
    `lock_thresh` is a bar noise clears from time to time. Measured over this
    stretch it reaches +0.52 against a 0.4 declare threshold. So the final
    state after 5000 noise symbols is a sample of that walk -- it was
    asserted as 0 here and held only until the arm-AGC seeding changed by a
    few LSBs, after which it still held on one machine's libm and flipped on
    CI's. Asserting the transition itself tests the documented behaviour and
    is immune to where the walk happens to end.
    """
    bn, foff = 0.005, 0.0008
    tx, _ = _signal(4, foff=foff, snr_db=25, seed=4)
    noise, _ = _signal(4, foff=foff, snr_db=-10, seed=6)
    rx = MpskReceiver(
        m=4,
        sps=8,
        m_out=4,
        init_norm_freq=_seed(foff, 4, bn),
        lock_thresh=0.4,
        bn_carrier=bn,
    )
    rx.steps(tx)
    assert rx.locked == 1

    # Feed the outage in blocks and watch for the withdrawal.
    blk = len(noise) // 10
    dropped = False
    for i in range(10):
        rx.steps(noise[i * blk : (i + 1) * blk])
        dropped = dropped or rx.locked == 0
    assert dropped, "a sustained lock loss never withdrew the declaration"

    # Re-seed the carrier the way a real outer acquisition would: an
    # estimate, not the exact truth. During the outage the discriminator saw
    # only noise and random-walked the shared NCO.
    rx.norm_freq = _seed(foff, 4, bn)
    rx.steps(tx)
    assert rx.locked == 1  # re-declared


def test_lock_time_dates_the_first_declaration_only():
    """`lock_time` answers "how long did this take", not "when last held".

    A drop and re-declare must not restamp it; `reset()` clears it to -1.
    Paired with the withdrawal case above because that is the only way to
    reach a second declaration and so the only way this can be wrong.
    """
    bn, foff = 0.005, 0.0008
    tx, _ = _signal(4, foff=foff, snr_db=25, seed=4)
    noise, _ = _signal(4, foff=foff, snr_db=-10, seed=6)
    rx = MpskReceiver(
        m=4,
        sps=8,
        m_out=4,
        init_norm_freq=_seed(foff, 4, bn),
        lock_thresh=0.4,
        bn_carrier=bn,
    )
    assert rx.lock_time == -1  # nothing has been declared yet
    rx.steps(tx)
    assert rx.locked == 1
    first = rx.lock_time
    assert first >= 0

    blk = len(noise) // 10
    for i in range(10):
        rx.steps(noise[i * blk : (i + 1) * blk])
    rx.norm_freq = _seed(foff, 4, bn)
    rx.steps(tx)
    assert rx.lock_time == first, "a re-declaration must not restamp it"

    rx.reset()
    assert rx.lock_time == -1


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
    # 8PSK is the hard case here: differential demapping roughly doubles
    # the symbol-error rate and 8PSK's decision margin is only +-pi/8, so the
    # NDA discriminator's own phase jitter is the dominant error term. This
    # used to hand the carrier to a decision-directed loop for that reason;
    # measured, the handover was worth 0.09 dB at the 8PSK anchor and it is
    # gone (doppler#877), so the NDA loop carries this case on its own.
    rx = MpskReceiver(
        m=m,
        sps=8,
        m_out=4,
        bn_carrier=0.005,
        bn_timing=0.01,
        differential=1,
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
    bn, foff = 0.01, 0.0005
    tx, _ = _signal(4, foff=foff, snr_db=30, nsym=3000, seed=7)
    kw = {
        "m": 4,
        "sps": 8,
        "m_out": 4,
        "bn_carrier": bn,
        "init_norm_freq": _seed(foff, 4, bn),
    }
    whole = MpskReceiver(**kw).steps(tx)
    rx = MpskReceiver(**kw)
    parts = [rx.steps(tx[i : i + 1000]) for i in range(0, tx.size, 1000)]
    chunked = np.concatenate(parts)
    assert chunked.size == whole.size
    assert np.allclose(chunked, whole, atol=1e-4)


def test_empty_input():
    rx = MpskReceiver(m=4, sps=8, m_out=4)
    out = rx.steps(np.zeros(0, np.complex64))
    assert out.size == 0


def test_reset_reproducible():
    bn, foff = 0.01, 0.0008
    tx, _ = _signal(4, foff=foff, snr_db=30, seed=9)
    rx = MpskReceiver(
        m=4, sps=8, m_out=4, bn_carrier=bn, init_norm_freq=_seed(foff, 4, bn)
    )
    first = rx.steps(tx)
    rx.reset()
    assert rx.locked == 0 and rx.lock == 0.0 and rx.lock_time == -1
    second = rx.steps(tx)
    assert np.array_equal(first, second)


# ── The surface that is GONE, and the behaviour that replaced it ─────────


def test_removed_knobs_are_gone_not_defaulted():
    """`acq_to_track` and friends are refused, not silently accepted.

    `ContinuousMpskReceiver` existed to pin exactly these off; with the
    handover deleted (doppler#877) it pinned nothing and was a duplicate of
    `MpskReceiver`, so it is gone too. What remains worth testing is that the
    removed knobs are REFUSED rather than quietly ignored -- a constructor
    that swallows `acq_to_track=1` and does nothing with it is the worst of
    the three possible outcomes, because the caller's code keeps working and
    stops meaning what it says.
    """
    import doppler.track as track

    assert not hasattr(track, "ContinuousMpskReceiver")

    for kw in ("acq_to_track", "nda_tap"):
        with pytest.raises(TypeError):
            MpskReceiver(m=2, sps=8.0, **{kw: 1})

    # `configure_lock` retuned only the handover's detector, never the lock
    # indicator's, so it desynced the two detectors it appeared to configure.
    assert not hasattr(MpskReceiver(m=2, sps=8.0), "configure_lock")
    assert not hasattr(MpskReceiver(m=2, sps=8.0), "tracking")


def test_the_nda_steer_is_ungated():
    """The loop steers from the first strobe, not from the lock declaration.

    This is what `ContinuousMpskReceiver` used to assert as `tracking == 0`,
    stated as the property that actually matters and can actually fail. The
    failure mode is an `if (locked)` in front of the steer, which leaves the
    estimate at exactly 0.0 until the indicator declares -- and which every
    end-of-run assertion in this file would still pass. So the estimate is
    read at the last block before the declaration, not at the end.
    """
    foff = 0.0008
    tx, _idx = _signal(2, foff=foff, snr_db=30, seed=21)
    rx = MpskReceiver(m=2, sps=8.0, m_out=8, bn_carrier=0.02, bn_timing=0.01)

    blk = 8  # one symbol
    f_undeclared, declared = 0.0, False
    for i in range(0, tx.size - blk, blk):
        rx.steps(tx[i : i + blk])
        if rx.locked:
            declared = True
            break
        f_undeclared = rx.norm_freq
    assert declared, "the indicator never declared — the case would be vacuous"
    # Non-zero and toward the offset rather than away from it. A gated steer
    # reads exactly 0.0 here, and that is the whole comparison -- the SIZE of
    # the fraction is not asserted, because `locked` is a threshold test with
    # hysteresis on a phase-coherence statistic and its declaration instant
    # carries no information about how converged the loop is.
    assert abs(f_undeclared) > 0.25 * foff
    assert f_undeclared * foff > 0.0


def test_receiver_tracks_the_carrier_and_demodulates():
    """Acquires with no gate of any kind: one discriminator, always running."""
    tx, idx = _signal(2, foff=0.0008, snr_db=30, seed=5)
    rx = MpskReceiver(m=2, sps=8.0, m_out=8, bn_carrier=0.02, bn_timing=0.01)
    out = rx.steps(tx)
    assert out.size > 0
    assert rx.norm_freq == pytest.approx(0.0008, abs=1e-4)
    assert rx.lock > 0.5
    assert _ser(out, idx, 2) < 0.01
