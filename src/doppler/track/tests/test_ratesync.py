"""RateSync — RRC matched filter fused with symbol-timing recovery.

The headline capability under test is **arbitrary-rate reception**: the
matched filter rides a double-precision fractional accumulator, so a
non-integer samples-per-symbol (17.33389, e, a drifting sample clock) is as
valid as 4.

Judging a receiver by BER alone is a trap in both directions -- a lag search
too narrow reports chance on a perfect demod, and one too wide can find a
lucky alignment on garbage -- so every lock assertion here pairs it with two
truth-free validators that need neither lag nor reference symbols:
self-referenced EVM and the blind M2M4 SNR estimator.
"""

import numpy as np
import pytest

from doppler.snr import snr_m2m4_db
from doppler.track import RateSync
from doppler.wfm import rrc_taps

BETA = 0.35
SPAN = 8


def _rrc_h(t, beta=BETA):
    """Analytic RRC h(t), t in symbol periods.

    A numpy twin of the C `wfm_rrc_h()`, needed because generating a signal
    at a NON-INTEGER samples-per-symbol cannot be done by sampling a tap
    table on an integer grid. `test_twin_matches_canonical_taps` pins it
    against the canonical `wfm.rrc_taps`, so it is a verified twin rather
    than a second copy free to drift.
    """
    t = np.asarray(t, dtype=float)
    out = np.empty_like(t)
    pt = np.pi * t
    sing0 = np.abs(t) < 1e-9
    # guarded like the C short-circuit: at beta == 0 there is no 1/(4*beta)
    # singularity to test for (and evaluating it would divide by zero)
    if beta > 0:
        sing1 = np.abs(np.abs(t) - 1.0 / (4 * beta)) < 1e-9
    else:
        sing1 = np.zeros(t.shape, dtype=bool)
    gen = ~(sing0 | sing1)
    out[sing0] = 1.0 - beta + 4.0 * beta / np.pi
    if sing1.any():
        a = np.pi / (4 * beta)
        out[sing1] = (beta / np.sqrt(2)) * (
            (1 + 2 / np.pi) * np.sin(a) + (1 - 2 / np.pi) * np.cos(a)
        )
    tg, ptg = t[gen], pt[gen]
    num = np.sin(ptg * (1 - beta)) + 4 * beta * tg * np.cos(ptg * (1 + beta))
    den = ptg * (1 - (4 * beta * tg) ** 2)
    out[gen] = num / den
    return out


def _tx(nsym, sps, tau=0.0, seed=7, beta=BETA, span=SPAN):
    """RRC-shaped BPSK at an arbitrary (fractional) sps, delayed by tau."""
    rng = np.random.default_rng(seed)
    bits = rng.integers(0, 2, nsym)
    a = (2 * bits - 1).astype(float)
    n = int(nsym * sps)
    t = (np.arange(n) - tau) / sps
    x = np.zeros(n)
    for k in range(nsym):
        lo = max(0, int(np.ceil((k - span) * sps + tau)))
        hi = min(n, int(np.floor((k + span) * sps + tau)) + 1)
        if hi > lo:
            x[lo:hi] += a[k] * _rrc_h(t[lo:hi] - k, beta)
    return bits, x.astype(np.complex64)


def _panel(bits, syms, skip):
    """(EVM, blind M2M4 dB, BER) — BER is never reported on its own."""
    y = syms[skip:]
    d = np.sign(y.real)
    g = np.vdot(d, y) / np.vdot(d, d)
    evm = float(
        np.sqrt(np.mean(np.abs(y - g * d) ** 2) / np.mean(np.abs(g * d) ** 2))
    )
    z = (y / (np.sqrt(np.mean(np.abs(y) ** 2)) + 1e-20)).astype(np.complex64)
    m2m4 = float(snr_m2m4_db(z))
    # BER with a lag search wide enough to actually find the alignment
    tx = (2 * bits.astype(int) - 1).astype(float)
    dd = d.astype(float)
    n = 1 << int(np.ceil(np.log2(len(tx) + len(dd))))
    c = np.fft.irfft(np.fft.rfft(tx, n) * np.conj(np.fft.rfft(dd, n)), n)
    lag = int(np.argmax(np.abs(c)))
    s = 1.0 if c[lag] >= 0 else -1.0
    m = min(len(dd), len(tx) - lag)
    ber = float(np.mean(s * dd[:m] != tx[lag : lag + m])) if m > 100 else 1.0
    return evm, m2m4, ber


# ── the twin the signal generator rests on ──────────────────────────────


def test_twin_matches_canonical_taps():
    """The numpy pulse must equal the canonical C taps on the integer grid."""
    for sps in (2, 4, 8):
        for beta in (0.0, 0.22, 0.35, 1.0):
            want = np.asarray(rrc_taps(beta, sps, SPAN), dtype=float)
            t = (np.arange(2 * SPAN * sps + 1) - SPAN * sps) / sps
            got = _rrc_h(t, beta)
            got = got / np.sqrt((got**2).sum())  # rrc_taps is unit-energy
            assert np.allclose(got, want, atol=1e-6)


# ── construction ────────────────────────────────────────────────────────


def test_create_defaults():
    rx = RateSync()
    assert rx.rate == pytest.approx(4.0)
    assert rx.locked is False


@pytest.mark.parametrize(
    "kwargs",
    [
        {"sps": 0.5},  # below one sample per symbol
        {"beta": -0.1},
        {"beta": 1.5},
        {"span": 0},
        {"num_phases": 300},  # not a power of two
        {"bn": -1.0},
        {"zeta": 0.0},
        {"sps": float("nan")},  # NaN must not slip through a >= test
    ],
)
def test_create_rejects_bad_params(kwargs):
    with pytest.raises(ValueError, match="RateSync"):
        RateSync(**kwargs)


def test_context_manager_and_destroy():
    with RateSync() as rx:
        assert rx.steps(np.zeros(64, np.complex64)).dtype == np.complex64
    rx2 = RateSync()
    rx2.destroy()


# ── the headline: arbitrary (non-integer) samples per symbol ────────────


@pytest.mark.parametrize("sps", [4.0, 6.5, 17.33389, 2.718281828, 9.876543])
def test_locks_at_arbitrary_sps(sps):
    bits, x = _tx(2500, sps, tau=0.37)
    rx = RateSync(sps=sps, beta=BETA, span=SPAN, num_phases=1024, bn=0.005)
    y = rx.steps(x)
    assert len(y) > 2000
    evm, m2m4, ber = _panel(bits, y, skip=len(y) // 3)
    assert evm < 0.05, f"EVM {evm}"
    assert m2m4 > 20.0, f"blind SNR {m2m4} dB"
    assert ber == 0.0
    assert rx.locked is True
    assert rx.rate == pytest.approx(sps, rel=1e-3)


@pytest.mark.parametrize("tau", [0.0, 0.37, 1.6, 2.5])
def test_acquires_from_any_timing_offset(tau):
    """No half-symbol ambiguity: every start lands on the open eye, never on
    the transitions (which would show as EVM ~0.7 and chance BER)."""
    bits, x = _tx(2500, 4.0, tau=tau)
    rx = RateSync(sps=4.0, bn=0.005)
    evm, _m2m4, ber = _panel(bits, rx.steps(x), skip=800)
    assert evm < 0.05
    assert ber == 0.0


@pytest.mark.parametrize("ppm", [200.0, -200.0, 1000.0])
def test_tracks_a_sample_clock_offset(ppm):
    """Built at the nominal sps, fed a stream whose true rate differs: the
    loop pulls it in and `rate` reports the truth, not the nominal."""
    nominal = 4.0
    true_sps = nominal * (1.0 + ppm * 1e-6)
    bits, x = _tx(5000, true_sps, tau=0.11)
    rx = RateSync(sps=nominal, bn=0.005)
    y = rx.steps(x)
    evm, _m2m4, ber = _panel(bits, y, skip=len(y) // 3)
    assert evm < 0.05
    assert ber == 0.0
    assert rx.rate == pytest.approx(true_sps, abs=2e-4)
    assert abs(rx.rate - nominal) > abs(true_sps - nominal) / 2


def test_panel_degrades_together_under_awgn():
    """EVM and the blind SNR must track each other and the injected noise --
    the cross-check that makes a BER of 0 meaningful."""
    rng = np.random.default_rng(3)
    prev_evm = 0.0
    for snr_db in (20.0, 12.0, 6.0):
        bits, x = _tx(3000, 4.0, tau=0.37)
        p = np.mean(np.abs(x) ** 2)
        sigma = np.sqrt(p / (2 * 10 ** (snr_db / 10)))
        noisy = (
            x
            + sigma
            * (rng.standard_normal(len(x)) + 1j * rng.standard_normal(len(x)))
        ).astype(np.complex64)
        rx = RateSync(sps=4.0, bn=0.005)
        evm, m2m4, _ber = _panel(bits, rx.steps(noisy), skip=1000)
        # the matched filter's processing gain puts the symbol-rate SNR
        # about 10*log10(sps) above the per-sample SNR fed in
        assert m2m4 == pytest.approx(snr_db + 10 * np.log10(4.0), abs=1.5)
        assert 20 * np.log10(evm) == pytest.approx(-m2m4, abs=1.5)
        assert evm > prev_evm  # monotone: noisier in, noisier out
        prev_evm = evm


# ── the rectangular / NRZ case: same object, different prototype ────────


def _tx_nrz(nsym, sps, tau=0.0, seed=7):
    """Rectangular (NRZ) BPSK at an arbitrary sps: every sample in symbol k
    carries a_k. The common case for chip- and NRZ-rate links."""
    rng = np.random.default_rng(seed)
    bits = rng.integers(0, 2, nsym)
    a = (2 * bits - 1).astype(float)
    n = int(nsym * sps)
    x = np.zeros(n)
    for k, ak in enumerate(a):
        lo = max(0, int(np.ceil(k * sps + tau)))
        hi = min(n, int(np.ceil((k + 1) * sps + tau)))
        x[lo:hi] = ak
    return bits, x.astype(np.complex64)


@pytest.mark.parametrize("sps", [4.0, 8.0, 11.7391, 17.33389])
def test_rectangular_pulse_locks(sps):
    """pulse="iandd" is the integrate-and-dump boxcar — the matched filter
    for a rectangular symbol. Integer sps recovers it essentially exactly;
    a fractional sps pays only the rectangle's edge-sample quantisation."""
    bits, x = _tx_nrz(2500, sps, tau=0.37)
    rx = RateSync(sps=sps, pulse="iandd", num_phases=1024, bn=0.005)
    y = rx.steps(x)
    evm, _m2m4, ber = _panel(bits, y, skip=len(y) // 3)
    assert evm < 0.06, f"EVM {evm}"
    assert ber == 0.0
    assert rx.locked is True
    assert rx.rate == pytest.approx(sps, rel=1e-3)


def test_rectangular_bank_is_far_cheaper_than_rrc():
    """The rectangle spans one symbol, the RRC spans 2*span — so the NRZ
    matched filter costs a small fraction of the taps per arm."""
    rect = RateSync(sps=8.0, pulse="iandd", span=8, num_phases=64)
    rrc = RateSync(sps=8.0, pulse="rrc", span=8, num_phases=64)
    # state_bytes is dominated by the two banks' delay lines + taps
    assert rect.state_bytes() < rrc.state_bytes() / 4


def test_rrc_signal_through_a_rectangular_filter_is_worse():
    """Sanity that `pulse` actually selects the filter: an RRC-shaped stream
    demodulated with the boxcar is measurably worse than with its matched
    RRC (mismatched filtering), even though both may still decode."""
    bits, x = _tx(2500, 4.0, tau=0.37)
    matched = RateSync(sps=4.0, pulse="rrc", bn=0.005)
    mismatched = RateSync(sps=4.0, pulse="iandd", bn=0.005)
    evm_m, _, _ = _panel(bits, matched.steps(x), skip=800)
    evm_x, _, _ = _panel(bits, mismatched.steps(x), skip=800)
    assert evm_m < evm_x


# ── block invariance, buffers, properties ───────────────────────────────


def test_chunked_equals_one_block():
    _bits, x = _tx(1200, 4.0, tau=0.37)
    a = RateSync(sps=4.0)
    b = RateSync(sps=4.0)
    whole = a.steps(x)
    parts = np.concatenate([b.steps(c) for c in np.array_split(x, 7)])
    assert np.array_equal(whole, parts)


def test_steps_out_writes_into_callers_buffer():
    _, x = _tx(300, 4.0)
    rx = RateSync(sps=4.0)
    out = np.zeros(len(x), dtype=np.complex64)
    y = rx.steps(x, out=out)
    assert np.shares_memory(y, out)


def test_properties_and_reset():
    _bits, x = _tx(1500, 4.0, tau=0.37)
    rx = RateSync(sps=4.0, bn=0.005)
    rx.steps(x)
    assert rx.locked is True
    assert rx.lock_stat > 0.3
    assert abs(rx.timing_error) < 1.0
    assert abs(rx.ctrl) < 0.05
    rx.bn = 0.01
    assert rx.bn == pytest.approx(0.01)
    rx.reset()
    assert rx.locked is False
    assert rx.lock_stat == 0.0
    assert rx.ctrl == 0.0
    assert rx.rate == pytest.approx(4.0)


def test_configure_lock_raw_changes_the_decision():
    _, x = _tx(1500, 4.0, tau=0.37)
    rx = RateSync(sps=4.0)
    rx.configure_lock_raw(64, 0.9, 0.9, 1, 8)  # unreachably high threshold
    rx.steps(x)
    assert rx.locked is False
    rx.reset()
    rx.configure_lock_raw(64, 0.2, 0.2, 1, 8)
    rx.steps(x)
    assert rx.locked is True


def test_dttl_ted_also_locks():
    bits, x = _tx(2500, 4.0, tau=0.37)
    rx = RateSync(sps=4.0, bn=0.005, ted="dttl")
    evm, _m2m4, ber = _panel(bits, rx.steps(x), skip=800)
    assert evm < 0.05
    assert ber == 0.0


# ── serialization ───────────────────────────────────────────────────────


def test_state_round_trip_resumes_bit_exactly():
    _, x = _tx(1200, 4.0, tau=0.37)
    half = len(x) // 2
    a = RateSync(sps=4.0, num_phases=256)
    a.steps(x[:half])
    blob = a.get_state()
    assert len(blob) == a.state_bytes()

    b = RateSync(sps=4.0, num_phases=256)
    b.set_state(blob)
    assert np.array_equal(a.steps(x[half:]), b.steps(x[half:]))


def test_set_state_rejects_a_clobbered_blob():
    rx = RateSync(sps=4.0, num_phases=256)
    blob = bytearray(rx.get_state())
    blob[0] ^= 0xFF
    with pytest.raises(ValueError):
        rx.set_state(bytes(blob))
    with pytest.raises(ValueError):
        rx.set_state(b"\x00" * 4)
    with pytest.raises(TypeError):
        rx.set_state("not bytes")


# ── telemetry ───────────────────────────────────────────────────────────


def test_telemetry_registers_five_probes():
    from doppler.telemetry import Telemetry

    tlm = Telemetry(1 << 14)
    rx = RateSync(sps=4.0)
    rx.set_telemetry(tlm, "sync")
    assert sorted(tlm.probe_names()) == [
        "sync.ctrl",
        "sync.e",
        "sync.lock",
        "sync.locked",
        "sync.rate",
    ]
    _, x = _tx(400, 4.0, tau=0.37)
    rx.steps(x)
    recs = tlm.read()
    assert len(recs) > 0 and len(recs) % 5 == 0
    rx.set_telemetry(None, "sync")
