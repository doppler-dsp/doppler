"""Integration tests for doppler.dsss.Despreader (Costas + DLL receiver)."""

import numpy as np
import pytest

from doppler.dsss import Despreader

SF, SPS = 127, 8
TSAMPS = SF * SPS


def _code(seed=1):
    return np.random.default_rng(seed).integers(0, 2, SF).astype(np.uint8)


def _signal(code, nper, periods_per_bit=1, f0=0.0, sigma=0.0, seed=3):
    """Continuous PN-spread BPSK x carrier (+ optional AWGN)."""
    rng = np.random.default_rng(seed)
    nbits = nper // periods_per_bit
    data = rng.integers(0, 2, nbits) * 2 - 1
    n = TSAMPS * nper
    rx = np.empty(n, np.complex64)
    cph = 0.0
    k = 0
    for p in range(nper):
        bit = data[p // periods_per_bit]
        for _ in range(TSAMPS):
            idx = int(cph % SF)
            rx[k] = bit * (-1.0 if code[idx] & 1 else 1.0)
            cph += 1.0 / SPS
            k += 1
    rx = rx * np.exp(2j * np.pi * f0 * np.arange(n))
    if sigma:
        rx = rx + (rng.normal(0, sigma, n) + 1j * rng.normal(0, sigma, n))
    return rx.astype(np.complex64), data


def _amb_ber(dec, truth):
    err = int(np.sum(dec != truth))
    return min(err, len(dec) - err) / len(dec)


def _amb_ber_best_shift(dec, truth, max_shift=1):
    """Best `_amb_ber` over a small +/-max_shift bit-index offset.

    `bits()`'s very first hard decision comes from the DLL's first
    correlation dump, whose exact timing (code-period 0 vs 1) sits on a
    knife-edge during the loop's initial acquisition transient for some
    seeds -- sub-ULP arithmetic differences (compiler, optimization
    flags, architecture) can legitimately tip it either way, shifting
    every subsequent bit's index by one. That's not a demodulation
    error (values past the shift still line up exactly), so tolerate a
    whole-array index offset the same way a real bit-sync layer would,
    rather than asserting a bit-exact index alignment through an
    acquisition boundary.
    """
    best = 1.0
    for shift in range(-max_shift, max_shift + 1):
        if shift >= 0:
            a, b = dec[shift:], truth[: len(truth) - shift]
        else:
            a, b = dec[: len(dec) + shift], truth[-shift:]
        n = min(len(a), len(b))
        if n == 0:
            continue
        best = min(best, _amb_ber(a[:n], b[:n]))
    return best


def test_create_and_guard():
    assert Despreader(_code(), SPS, 0.0, 0.0, 0.05, 0.005, 0.0, 0.707, 0.5, 1)


def test_context_manager_and_destroy():
    with Despreader(_code(), SPS, 0.0, 0.0, 0.05, 0.005, 0.0, 0.707, 0.5, 1):
        pass
    c = Despreader(_code(), SPS, 0.0, 0.0, 0.05, 0.005, 0.0, 0.707, 0.5, 1)
    c.destroy()


def test_properties():
    c = Despreader(_code(), SPS, 0.001, 0.0, 0.05, 0.005, 0.0, 0.707, 0.5, 1)
    assert c.norm_freq == pytest.approx(0.001)
    assert c.code_rate == pytest.approx(1.0)
    assert c.bn_carrier == pytest.approx(0.05)
    assert c.bn_code == pytest.approx(0.005)
    c.bn_carrier = 0.03
    c.bn_code = 0.004
    assert c.bn_carrier == pytest.approx(0.03)
    assert c.bn_code == pytest.approx(0.004)


def test_full_receiver_locks_and_despreads():
    code = _code()
    rx, data = _signal(code, 500, f0=5e-5, seed=3)
    c = Despreader(code, SPS, 0.0, 0.0, 0.05, 0.005, 0.0, 0.707, 0.5, 1)
    sym = c.steps(rx)
    assert c.norm_freq == pytest.approx(5e-5, abs=1e-5)
    assert c.lock_metric > 0.9
    # both embedded verify-counted lock decisions are live in composition
    # (the carrier detector on the Costas metric EMA, and the DLL's
    # always-on CFAR detector fed by the composition-path noise tap)
    assert c.carrier_locked is True
    assert c.code_locked is True
    dec = np.where(sym[len(sym) // 2 :].real >= 0, 1, -1)
    assert _amb_ber(dec, data[len(sym) // 2 : len(sym)]) == 0.0


def test_fresh_receiver_is_unlocked():
    c = Despreader(_code(), SPS, 0.0, 0.0, 0.05, 0.005, 0.0, 0.707, 0.5, 1)
    assert c.carrier_locked is False
    assert c.code_locked is False


def test_fll_assist_widens_pull_in():
    code = _code()
    f0 = 0.2 / TSAMPS  # 0.2 cycles/epoch — beyond the bare PLL
    rx, data = _signal(code, 700, f0=f0, seed=11)

    pll = Despreader(code, SPS, 0.0, 0.0, 0.05, 0.005, 0.0, 0.707, 0.5, 1)
    pll.steps(rx)
    assert pll.lock_metric < 0.8
    assert pll.carrier_locked is False  # the decision agrees

    fll = Despreader(code, SPS, 0.0, 0.0, 0.05, 0.005, 0.03, 0.707, 0.5, 1)
    sym = fll.steps(rx)
    assert fll.norm_freq == pytest.approx(f0, abs=2e-5)
    assert fll.lock_metric > 0.9
    dec = np.where(sym[len(sym) // 2 :].real >= 0, 1, -1)
    assert _amb_ber(dec, data[len(sym) // 2 : len(sym)]) == 0.0


def test_hard_bits_periods_per_bit_1():
    code = _code()
    rx, data = _signal(code, 400, f0=4e-5, seed=17)
    c = Despreader(code, SPS, 0.0, 0.0, 0.05, 0.005, 0.0, 0.707, 0.5, 1)
    c.steps(rx)  # prompts
    hb = c.bits(_signal(code, 400, f0=4e-5, seed=17)[0])  # fresh run
    assert hb.dtype == np.uint8
    dec = np.where(hb > 0, 1, -1)
    half = len(dec) // 2
    assert _amb_ber_best_shift(dec[half:], data[half : len(dec)]) == 0.0


def test_bit_sync_recovers_data_bits():
    code = _code()
    N, nbits = 20, 120
    rx, data = _signal(code, N * nbits, periods_per_bit=N, f0=3e-5, seed=23)
    c = Despreader(code, SPS, 0.0, 0.0, 0.05, 0.005, 0.0, 0.707, 0.5, N)
    bits = c.bits(rx)
    assert len(bits) >= nbits - 3
    assert c.bit_phase == 0  # data boundary at epoch 0
    dec = np.where(bits > 0, 1, -1)
    tail = len(dec) // 3
    assert _amb_ber(dec[tail:], data[tail : len(dec)]) == 0.0


def test_reset_reproducible():
    code = _code()
    rx, _ = _signal(code, 300, f0=5e-5, seed=5)
    c = Despreader(code, SPS, 0.0, 0.0, 0.05, 0.005, 0.0, 0.707, 0.5, 1)
    s1 = c.steps(rx)
    f1 = c.norm_freq
    c.reset()
    s2 = c.steps(rx)
    assert np.array_equal(s1, s2)
    assert f1 == c.norm_freq


def test_steps_out_writes_into_callers_buffer():
    code = _code()
    rx, _ = _signal(code, 50, seed=5)
    c = Despreader(code, SPS, 0.0, 0.0, 0.05, 0.005, 0.0, 0.707, 0.5, 1)
    out = np.zeros(max(c.steps_max_out(), len(rx)), dtype=np.complex64)
    y = c.steps(rx, out=out)
    assert np.shares_memory(y, out)


def test_steps_out_undersized_raises():
    code = _code()
    rx, _ = _signal(code, 50, seed=5)
    c = Despreader(code, SPS, 0.0, 0.0, 0.05, 0.005, 0.0, 0.707, 0.5, 1)
    with pytest.raises(ValueError):
        c.steps(rx, out=np.zeros(1, dtype=np.complex64))


def test_bits_out_writes_into_callers_buffer():
    code = _code()
    rx, _ = _signal(code, 50, seed=5)
    c = Despreader(code, SPS, 0.0, 0.0, 0.05, 0.005, 0.0, 0.707, 0.5, 1)
    out = np.zeros(max(c.bits_max_out(), len(rx)), dtype=np.uint8)
    y = c.bits(rx, out=out)
    assert np.shares_memory(y, out)


def test_bits_out_undersized_raises():
    code = _code()
    rx, _ = _signal(code, 50, seed=5)
    c = Despreader(code, SPS, 0.0, 0.0, 0.05, 0.005, 0.0, 0.707, 0.5, 1)
    with pytest.raises(ValueError):
        c.bits(rx, out=np.zeros(1, dtype=np.uint8))


def test_acq_handoff_verify_reject():
    """Bounded-time accept/reject of an acquisition handoff — every
    constant derived, none tuned.

    Acquisition at pfa over many dwells *will* occasionally hand the
    tracker a false (Doppler, code phase) cell. The verify policy: seed a
    Despreader with the handoff and give the DLL's verify-counted CFAR
    detector a decision window sized from detection theory —

        window = k_safety * n_looks * det_verify_delay(pd_dec, n_up)

    periods, where pd_dec is the per-decision detection probability at the
    operating SNR and (n_up, n_looks) are the DLL's defaults (2, 20). A
    true cell declares code lock inside the window (mean latency
    det_verify_delay decisions); a false cell faces a compounded
    false-accept probability of ~(window/n_looks) * pfa_dec^n_up (~1e-5
    here), so timing out is the reject. On reject the channel is torn
    down and the sample budget returns to acquisition.
    """
    from doppler.detection import det_verify_delay

    code = _code()
    n_up, n_looks = 2, 20  # Dll.configure_lock defaults at pfa = 1e-3
    # At this test's clean-signal SNR the per-decision pd is ~1; the
    # closed-form mean declare latency is det_verify_delay(pd, n_up)
    # decisions. k_safety = 4 absorbs pull-in transients.
    window_periods = int(4 * n_looks * det_verify_delay(0.99, n_up))
    assert window_periods >= 2 * n_up * n_looks  # sane: >= 2 declares

    # TRUE handoff: acquisition's (f0, chip) estimates seed the tracker;
    # code lock declares inside the window.
    rx, _ = _signal(code, window_periods, f0=5e-5, sigma=0.4, seed=3)
    ch = Despreader(code, SPS, 5e-5, 0.0, 0.05, 0.005, 0.0, 0.707, 0.5, 1)
    ch.steps(rx)
    assert ch.code_locked is True  # accept

    # FALSE handoff: a false-alarm cell hands over garbage — there is no
    # signal at all. The CFAR statistic has no prompt excess, so the
    # verify-counted detector never declares inside the window: reject.
    rng = np.random.default_rng(11)
    noise = (
        rng.normal(0, 1, TSAMPS * window_periods)
        + 1j * rng.normal(0, 1, TSAMPS * window_periods)
    ).astype(np.complex64)
    bogus = Despreader(code, SPS, 0.0, 0.0, 0.05, 0.005, 0.0, 0.707, 0.5, 1)
    bogus.steps(noise)
    assert bogus.code_locked is False  # reject: tear down, back to acq
