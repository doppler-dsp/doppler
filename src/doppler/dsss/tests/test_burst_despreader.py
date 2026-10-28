"""Integration tests for doppler.dsss.BurstDespreader (DLL + Costas tracking).

Builds DSSS-BPSK bursts with the library's one polarity, bit b sent as
``1 - 2b`` (``bin_to_nrz``; home ``mpsk_core.h``), and checks the despreader
recovers the bits. Every burst starts the carrier at a phase the test knows,
or carries a preamble that tells the receiver it, so error rates are RAW: an
inverted stream is an error (doppler#2654, doppler#2664).
"""

import numpy as np
import pytest

from doppler.dsss import BurstDespreader

SPS = 4


def _spread(code, syms):
    """Oversampled DSSS-BPSK signal: each symbol * code, rect-hold by SPS."""
    chsign = np.where(code & 1, -1.0, 1.0).astype(np.float32)
    return np.concatenate([np.repeat(s * chsign, SPS) for s in syms]).astype(
        np.complex64
    )


def _ber(rec_bits, tx_syms, start=0):
    """Raw bit error rate of `rec_bits` against the sent +-1 symbols."""
    rec = 1.0 - 2.0 * np.asarray(rec_bits, np.float32)  # 1 - 2b
    return float(np.mean(rec[start:] != tx_syms[start : len(rec)]))


def _burst(rng, sf, n):
    code = rng.integers(0, 2, sf).astype(np.uint8)
    bits = rng.integers(0, 2, n).astype(np.uint8)
    syms = np.where(bits == 1, -1.0, 1.0).astype(np.float32)
    return code, syms, _spread(code, syms)


def test_create_validates():
    # sps < 2 and sf longer than the code are rejected (NULL -> error).
    with pytest.raises((ValueError, RuntimeError, Exception)):
        BurstDespreader(np.zeros(4, np.uint8), sf=4, sps=1)
    with pytest.raises((ValueError, RuntimeError, Exception)):
        BurstDespreader(np.zeros(2, np.uint8), sf=8, sps=2)


# 1e200 is finite, but its loop gains overflow: the predicate refuses it.
_BAD_BN = [float("nan"), float("inf"), -0.01, 1e200]


@pytest.mark.parametrize("bad", _BAD_BN)
@pytest.mark.parametrize("which", ["bn_carrier", "bn_code"])
def test_create_refuses_a_bandwidth_outside_the_loop_filter_domain(
    which: str, bad: float
) -> None:
    """A NaN, infinite, negative or overflowing bandwidth gave NaN loop
    gains from the first symbol (#2071): refused, by the loop filter's own
    predicate."""
    code = np.ones(31, np.uint8)
    with pytest.raises(
        ValueError,
        match="bn_carrier and bn_code within the loop filter's domain",
    ):
        BurstDespreader(code, sf=31, sps=SPS, **{which: bad})


@pytest.mark.parametrize("bad", _BAD_BN)
@pytest.mark.parametrize("which", ["bn_carrier", "bn_code"])
def test_a_refused_bandwidth_leaves_the_loop_as_it_was(
    which: str, bad: float
) -> None:
    """The setter refuses in C, the binding raises ValueError on that
    status (just-makeit#2182), and the loop keeps its bandwidth."""
    d = BurstDespreader(np.ones(31, np.uint8), sf=31, sps=SPS)
    before = getattr(d, which)
    with pytest.raises(
        ValueError, match=f"{which} is outside the loop filter's domain"
    ):
        setattr(d, which, bad)
    assert getattr(d, which) == before


def test_genie_payload_zero_offset():
    rng = np.random.default_rng(0)
    code, syms, tx = _burst(rng, 31, 60)
    d = BurstDespreader(
        code, sf=31, sps=SPS, init_norm_freq=0.0, init_chip_phase=0.0
    )
    bits = d.bits(tx)
    assert len(bits) == 60
    assert _ber(bits, syms) == 0.0
    assert d.lock_metric > 0.9


def test_steps_returns_complex64():
    rng = np.random.default_rng(1)
    code, syms, tx = _burst(rng, 31, 30)
    d = BurstDespreader(code, sf=31, sps=SPS)
    s = d.steps(tx)
    assert s.dtype == np.complex64
    # prompt symbol sign matches the transmitted symbol, raw
    assert _ber((s.real < 0).astype(np.uint8), syms) == 0.0


def test_seeded_carrier_offset_tracks():
    rng = np.random.default_rng(2)
    code, syms, tx = _burst(rng, 63, 300)
    f0 = 0.0006
    rx = (tx * np.exp(2j * np.pi * f0 * np.arange(len(tx)))).astype(
        np.complex64
    )
    d = BurstDespreader(
        code, sf=63, sps=SPS, init_norm_freq=f0, init_chip_phase=0.0
    )
    bits = d.bits(rx)
    assert _ber(bits, syms, start=len(bits) // 4) == 0.0
    assert abs(d.norm_freq - f0) < 1e-4
    assert d.lock_metric > 0.95


def test_dll_pulls_in_code_phase():
    rng = np.random.default_rng(3)
    code, syms, tx = _burst(rng, 63, 300)
    d = BurstDespreader(
        code, sf=63, sps=SPS, init_chip_phase=0.4, bn_code=0.01
    )
    bits = d.bits(tx)
    assert _ber(bits, syms, start=len(bits) // 4) == 0.0


def test_reset_reproduces():
    rng = np.random.default_rng(4)
    code, _syms, tx = _burst(rng, 31, 50)
    d = BurstDespreader(code, sf=31, sps=SPS, init_norm_freq=0.0001)
    b1 = d.bits(tx.copy())
    d.reset()
    b2 = d.bits(tx.copy())
    np.testing.assert_array_equal(b1, b2)


def test_preamble_aided_distinct_codes():
    """Realistic scenario: long acq code, 5 reps, distinct 32-chip data code.

    The unmodulated preamble pulls the loops in (set_acq), then the payload is
    despread with the data code. BER 0, raw, even with an unseeded carrier
    offset: the preamble sets the payload's absolute phase (doppler#2664).
    """
    rng = np.random.default_rng(5)
    acq_sf, data_sf, reps = 512, 32, 5
    acq = rng.integers(0, 2, acq_sf).astype(np.uint8)
    data = rng.integers(0, 2, data_sf).astype(np.uint8)
    asig = np.where(acq & 1, -1.0, 1.0).astype(np.float32)
    bits = rng.integers(0, 2, 200).astype(np.uint8)
    syms = np.where(bits == 1, -1.0, 1.0).astype(np.float32)
    pre = np.concatenate([np.repeat(asig, SPS) for _ in range(reps)])
    pay = _spread(data, syms)
    burst = np.concatenate([pre, pay]).astype(np.complex64)
    f0 = 8e-5
    rx = (burst * np.exp(2j * np.pi * f0 * np.arange(len(burst)))).astype(
        np.complex64
    )
    d = BurstDespreader(data, sf=data_sf, sps=SPS, init_norm_freq=0.0)
    d.set_acq(acq, reps)
    out = d.bits(rx)
    # the preamble emits nothing; only the payload symbols come out
    assert len(out) == len(syms)
    assert _ber(out, syms, start=len(out) // 5) == 0.0
    assert abs(d.norm_freq - f0) < 5e-5


def test_bits_follow_one_minus_2b_at_a_known_phase():
    """bits() decides by the library's one polarity (doppler#2654).

    Bit b is sent as ``1 - 2b`` through ``bin_to_nrz``. With no preamble the
    carrier phase the caller hands in is the reference: at 0 the decisions
    are the sent bits, raw; at pi every one is complemented.
    """
    from doppler.cvt import bin_to_nrz

    rng = np.random.default_rng(2654)
    code = rng.integers(0, 2, 31).astype(np.uint8)
    tx = rng.integers(0, 2, 64).astype(np.uint8)
    sym = np.zeros(tx.size, np.float32)
    bin_to_nrz(tx, sym)
    base = _spread(code, sym)
    for phase, want in ((0.0, tx), (np.pi, 1 - tx)):
        rx = (base * np.exp(1j * phase)).astype(np.complex64)
        got = BurstDespreader(code, sf=31, sps=SPS).bits(rx)
        assert np.array_equal(got, want), phase


def _preamble_burst(seed, phase, f0, reps=5, n=60, chip_snr_db=None):
    """A set_acq burst -- `reps` periods of a 512-chip acq code, then `n`
    payload bits on a 32-chip data code -- at carrier `phase` (rad) and
    offset `f0` (cycles/sample), optionally in complex AWGN at a per-sample
    chip SNR. Returns (despreader set up for it, unseeded; rx; sent bits).

    One data-symbol period of silence follows the payload. Without it the
    last symbol completes on the input's final sample, and a code clock one
    rounding behind leaves it pending: correct streaming, but the burst
    came back a symbol short on CI's arm64 leg (doppler#2664)."""
    from doppler.cvt import bin_to_nrz

    rng = np.random.default_rng(seed)
    acq = rng.integers(0, 2, 512).astype(np.uint8)
    data = rng.integers(0, 2, 32).astype(np.uint8)
    tx = rng.integers(0, 2, n).astype(np.uint8)
    sym = np.zeros(tx.size, np.float32)
    bin_to_nrz(tx, sym)
    pre = np.tile(np.repeat(1.0 - 2.0 * acq, SPS), reps)
    burst = np.concatenate([pre, _spread(data, sym), np.zeros(32 * SPS)])
    t = np.arange(burst.size)
    rx = burst * np.exp(1j * (phase + 2 * np.pi * f0 * t))
    if chip_snr_db is not None:
        sigma = 10 ** (-chip_snr_db / 20) / np.sqrt(2)
        rx = rx + sigma * (
            rng.standard_normal(t.size) + 1j * rng.standard_normal(t.size)
        )
    d = BurstDespreader(data, sf=32, sps=SPS)  # carrier NOT seeded
    d.set_acq(acq, reps)
    return d, rx.astype(np.complex64), tx


# pi and pi +- 1e-6 are the atan2 discriminator's branch cut: the first
# prompt reads +pi or -pi on the sign of a rounding residue, and either
# sign must walk the loop round to the same answer.
_PHASES = [
    0.0,
    np.pi / 2,
    np.pi - 1e-6,
    np.pi,
    np.pi + 1e-6,
    3 * np.pi / 2,
]
_OFFSETS = [0.0, 2e-5, 4e-5, 8e-5]


@pytest.mark.parametrize("f0", _OFFSETS)
@pytest.mark.parametrize("phase", _PHASES)
def test_a_preamble_is_the_phase_reference(phase, f0):
    """The preamble sets the payload's ABSOLUTE phase (doppler#2664).

    An unmodulated preamble is a known +1 (bit 0), so the receiver was told
    the phase and must hand the payload over at it -- unseeded, from any
    starting phase, at any offset the preamble's loop tracks. Five reps do
    not pull 8e-5 in: the loop ends up to 1.8 rad off, and the payload's
    Costas detector locked at pi and decoded every bit complemented in 24 of
    the 64 cells at 0, pi/2, pi and 3pi/2 before the handover took the
    preamble's phase. Scored RAW: an inversion is 60 errors, and a burst
    that comes back short fails as short.
    """
    for seed in (1, 2, 3, 5):
        d, rx, tx = _preamble_burst(seed, phase, f0)
        got = d.bits(rx)
        assert len(got) >= tx.size, (seed, "short", len(got))
        assert np.array_equal(got[: tx.size], tx), seed


@pytest.mark.parametrize("phase", _PHASES)
def test_the_preamble_phase_holds_in_noise(phase):
    """The same, one rep short of the minimum and in noise.

    A single-rep preamble has one prompt to take the phase from, and -3 dB
    chip SNR puts noise on it. The bound is still zero errors: 128 samples a
    payload symbol make Es/N0 = 128 * 10**-0.3 = 18 dB, where BPSK's BER is
    Q(sqrt(2 * 64)) < 1e-29. An inversion would be every bit.
    """
    for f0 in _OFFSETS:
        for seed in (1, 2, 3):
            d, rx, tx = _preamble_burst(
                seed, phase, f0, reps=1, chip_snr_db=-3.0
            )
            got = d.bits(rx)
            assert len(got) >= tx.size, (f0, seed, "short", len(got))
            assert np.array_equal(got[: tx.size], tx), (f0, seed)


def test_property_accessors():
    d = BurstDespreader(
        np.ones(16, np.uint8), sf=16, sps=2, init_norm_freq=0.001
    )
    # getters
    assert d.norm_freq == pytest.approx(0.001, abs=1e-6)
    for attr in (
        "bn_carrier",
        "bn_code",
        "code_phase",
        "lock_metric",
        "snr_est",
    ):
        assert isinstance(getattr(d, attr), float)
    # setters round-trip (bn setters recompute the loop gains internally)
    d.bn_carrier = 0.06
    assert d.bn_carrier == pytest.approx(0.06)
    d.bn_code = 0.02
    assert d.bn_code == pytest.approx(0.02)
    d.norm_freq = 0.0
    assert d.norm_freq == pytest.approx(0.0, abs=1e-9)


def test_context_manager_and_destroy():
    d = BurstDespreader(np.ones(8, np.uint8), sf=8, sps=2)
    with d:
        pass
    d.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        d.steps(np.zeros(16, np.complex64))


def test_steps_out_writes_into_callers_buffer():
    x = np.random.default_rng(0).standard_normal(64).astype(np.complex64)
    d = BurstDespreader(np.ones(4, np.uint8), sf=4, sps=4)
    out = np.zeros(len(x), dtype=np.complex64)
    y = d.steps(x, out=out)
    assert np.shares_memory(y, out)


def test_steps_out_undersized_raises():
    x = np.zeros(64, dtype=np.complex64)
    d = BurstDespreader(np.ones(4, np.uint8), sf=4, sps=4)
    out = np.zeros(1, dtype=np.complex64)
    with pytest.raises(ValueError):
        d.steps(x, out=out)


def test_bits_out_writes_into_callers_buffer():
    x = np.random.default_rng(0).standard_normal(64).astype(np.complex64)
    d = BurstDespreader(np.ones(4, np.uint8), sf=4, sps=4)
    out = np.zeros(len(x), dtype=np.uint8)
    y = d.bits(x, out=out)
    assert np.shares_memory(y, out)


def test_bits_out_undersized_raises():
    x = np.zeros(64, dtype=np.complex64)
    d = BurstDespreader(np.ones(4, np.uint8), sf=4, sps=4)
    out = np.zeros(1, dtype=np.uint8)
    with pytest.raises(ValueError):
        d.bits(x, out=out)


def test_burst_lock_statistic_calibrated():
    """The whole-burst lock statistic gates H1 vs H0 at a derived threshold.

    R = sqrt(stat_n * sum Re^2 / sum Im^2) has the exact H0 law
    R^2 = stat_n * F(stat_n, stat_n) — the noise reference is estimated
    from as many samples as the signal sum, so det_threshold_f (not a
    chi-square gate, which would realize ~25x the priced pfa here) sets
    the threshold: a live burst clears it by a wide margin, a noise-only
    "burst" stays under it at an honest 1e-3, and no caller-invented
    threshold is involved.
    """
    import math

    from doppler.detection import det_threshold_f

    rng = np.random.default_rng(7)
    code, _syms, x = _burst(rng, 31, 64)
    x = x + 0.3 * (
        rng.standard_normal(x.size) + 1j * rng.standard_normal(x.size)
    ).astype(np.complex64)

    b = BurstDespreader(code=code, sf=31, sps=SPS)
    b.bits(x)
    assert b.stat_n == 64
    eta = math.sqrt(b.stat_n * det_threshold_f(1e-3, b.stat_n))
    assert b.lock_stat > eta
    assert b.lock_metric > 0.9
    assert b.snr_est > 10.0  # ~31 dB AWGN-only; jitter-included estimate

    # H0: pure noise through a fresh instance never clears the gate, and
    # the lock metric hovers at the no-carrier mean 2/pi.
    n = BurstDespreader(code=code, sf=31, sps=SPS)
    noise = (
        rng.standard_normal(x.size) + 1j * rng.standard_normal(x.size)
    ).astype(np.complex64)
    n.bits(noise)
    eta0 = math.sqrt(n.stat_n * det_threshold_f(1e-3, n.stat_n))
    assert n.lock_stat < eta0
    assert 0.45 < n.lock_metric < 0.8
    assert n.snr_est < 2.0

    # reset() re-arms the one-shot statistics.
    b.reset()
    assert b.stat_n == 0 and b.lock_stat == 0.0


def test_set_state_carries_set_acq_and_refuses_what_it_cannot_index():
    """#2041: a blob from a despreader with ``set_acq`` active, restored into
    one without, used to return without error and crash the next
    ``steps()``. The acq code is ``set_acq``'s value, so it travels in the
    blob, and its length is the blob's size: a despreader with no acq code,
    or one of another length, refuses the blob and is left as it was. One
    whose acq code has the same length restores the source's preamble and
    then runs exactly as the source does."""
    rng = np.random.default_rng(41)
    data = rng.integers(0, 2, 31).astype(np.uint8)
    acq = rng.integers(0, 2, 127).astype(np.uint8)
    other = (1 - acq).astype(np.uint8)
    x = (rng.standard_normal(4096) + 1j * rng.standard_normal(4096)).astype(
        np.complex64
    )

    a = BurstDespreader(data, sf=31, sps=4)
    a.set_acq(acq, 4)
    a.steps(x[:600])  # into the preamble
    blob = a.get_state()

    # no acq code: refused, and untouched
    b = BurstDespreader(data, sf=31, sps=4)
    before = b.get_state()
    with pytest.raises(ValueError):
        b.set_state(blob)
    assert b.get_state() == before
    b.steps(x[:64])  # and it still runs

    # an acq code of another length: refused, untouched
    c = BurstDespreader(data, sf=31, sps=4)
    c.set_acq(acq[:63], 4)
    before = c.get_state()
    with pytest.raises(ValueError):
        c.set_state(blob)
    assert c.get_state() == before

    # same length, other chips and periods: the source's preamble restores
    d = BurstDespreader(data, sf=31, sps=4)
    d.set_acq(other, 2)
    d.set_state(blob)
    out = a.steps(x[600:])
    assert out.size > 0  # the comparison below is not of two empties
    np.testing.assert_array_equal(out, d.steps(x[600:]))


def test_set_state_checks_the_blob_in_c_not_only_its_size():
    """#2041, round 2: a blob the same size as the target's but made under
    another create-time seed passes the binding's size check and is refused
    by the C set_state itself -- "rejected", not "size mismatch" -- leaving
    the target as it was. A setter's value is state, so the loops' bn
    travels; and a seed that is not finite is refused at construction,
    because set_state compares the seeds and NaN equals nothing."""
    rng = np.random.default_rng(2041)
    data = rng.integers(0, 2, 31).astype(np.uint8)
    x = (rng.standard_normal(2048) + 1j * rng.standard_normal(2048)).astype(
        np.complex64
    )

    src = BurstDespreader(data, sf=31, sps=4, init_norm_freq=0.01)
    t = BurstDespreader(data, sf=31, sps=4)
    blob, before = src.get_state(), t.get_state()
    assert len(blob) == len(before)  # past the binding's size check
    with pytest.raises(ValueError, match="rejected"):
        t.set_state(blob)
    assert t.get_state() == before

    s2 = BurstDespreader(data, sf=31, sps=4)
    s2.bn_carrier = 0.02
    s2.steps(x[:600])
    t.set_state(s2.get_state())
    assert t.bn_carrier == 0.02
    out = s2.steps(x[600:])
    assert out.size > 0
    np.testing.assert_array_equal(out, t.steps(x[600:]))

    with pytest.raises(ValueError, match="finite"):
        BurstDespreader(data, sf=31, sps=4, init_chip_phase=float("nan"))


def test_norm_freq_is_written_at_the_symbol_period_in_force():
    """#2648: during a ``set_acq`` preamble the symbol is ``acq_sf * sps``
    samples, and a frequency set then must read back unchanged after one
    symbol. It used to be scaled by the data period and came back as
    ``val * sf / acq_sf``. Both loop bandwidths are 0, so only the writes
    move the carrier integrator."""
    rng = np.random.default_rng(2648)
    data = rng.integers(0, 2, 32).astype(np.uint8)
    acq = rng.integers(0, 2, 128).astype(np.uint8)
    d = BurstDespreader(
        data,
        sf=32,
        sps=SPS,
        init_norm_freq=0.01,
        bn_carrier=0.0,
        bn_code=0.0,
    )
    d.set_acq(acq, 4)
    one = np.zeros(128 * SPS, np.complex64)
    d.steps(one)
    assert d.norm_freq == pytest.approx(0.01, abs=1e-12)
    d.norm_freq = 0.02
    d.steps(one)
    assert d.norm_freq == pytest.approx(0.02, abs=1e-12)
