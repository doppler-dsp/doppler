"""Tests for doppler.ddc.DDC."""

from __future__ import annotations

import warnings

import numpy as np
import pytest

from doppler.ddc import DDC, MatchedDDC

N = 4096


def _tone(freq: float, n: int, offset: int = 0) -> np.ndarray:
    t = np.arange(n, dtype=np.float64) + offset
    return np.exp(2j * np.pi * freq * t).astype(np.complex64)


def _dominant_freq(y: np.ndarray) -> float:
    S = np.abs(np.fft.fft(y))
    return float(np.fft.fftfreq(len(y))[np.argmax(S)])


# ------------------------------------------------------------------ #
# Construction                                                         #
# ------------------------------------------------------------------ #


class TestDdcConstruction:
    def test_create_basic(self):
        ddc = DDC(0.1, 0.25)
        assert ddc is not None

    def test_invalid_rate_zero(self):
        with pytest.raises((ValueError, Exception)):
            DDC(0.1, 0.0)

    def test_invalid_rate_negative(self):
        with pytest.raises((ValueError, Exception)):
            DDC(0.1, -1.0)

    def test_rate_property(self):
        ddc = DDC(0.1, 0.25)
        assert abs(ddc.rate - 0.25) < 1e-9

    def test_get_norm_freq(self):
        ddc = DDC(0.1, 0.25)
        assert abs(ddc.norm_freq - 0.1) < 1e-5

    def test_context_manager(self):
        with DDC(0.1, 0.25) as ddc:
            y = ddc.execute(np.zeros(512, dtype=np.complex64))
        assert y.dtype == np.complex64


# ------------------------------------------------------------------ #
# execute() output                                                     #
# ------------------------------------------------------------------ #


class TestDdcExecute:
    def test_returns_complex64(self):
        ddc = DDC(0.1, 0.25)
        y = ddc.execute(np.zeros(N, dtype=np.complex64))
        assert y.dtype == np.complex64

    @pytest.mark.parametrize("rate,tol", [(0.25, 0.05), (0.5, 0.05)])
    def test_output_length_approx(self, rate, tol):
        ddc = DDC(0.1, rate)
        x = np.ones(N, dtype=np.complex64)
        y = ddc.execute(x)
        expected = N * rate
        assert abs(len(y) / expected - 1.0) < tol, (
            f"rate={rate}: got {len(y)}, expected ≈{expected:.0f}"
        )

    def test_unity_rate_length(self):
        ddc = DDC(0.0, 1.0)
        x = np.ones(512, dtype=np.complex64)
        y = ddc.execute(x)
        assert len(y) == 512


# ------------------------------------------------------------------ #
# Frequency tuning                                                     #
# ------------------------------------------------------------------ #


class TestDdcTuning:
    def test_set_norm_freq_roundtrip(self):
        ddc = DDC(0.1, 0.25)
        ddc.norm_freq = 0.2
        assert abs(ddc.norm_freq - 0.2) < 1e-5

    def test_set_norm_freq_no_reset(self):
        ddc = DDC(0.1, 0.25)
        ddc.execute(np.ones(512, dtype=np.complex64))
        ddc.norm_freq = 0.2
        y = ddc.execute(np.ones(512, dtype=np.complex64))
        assert len(y) > 0


# ------------------------------------------------------------------ #
# reset()                                                              #
# ------------------------------------------------------------------ #


class TestDdcReset:
    def test_reset_gives_same_output_as_fresh(self):
        rng = np.random.default_rng(7)
        x = rng.standard_normal(N).astype(np.complex64)
        d1 = DDC(0.1, 0.25)
        d2 = DDC(0.1, 0.25)
        out1 = d1.execute(x)
        d2.execute(x)
        d2.reset()
        out2 = d2.execute(x)
        np.testing.assert_array_equal(out1, out2)

    def test_reset_preserves_rate(self):
        ddc = DDC(0.1, 0.25)
        ddc.reset()
        assert abs(ddc.rate - 0.25) < 1e-9


# ------------------------------------------------------------------ #
# Spectral                                                             #
# ------------------------------------------------------------------ #


@pytest.mark.parametrize("f_tone", [0.1, -0.1, 0.2])
def test_tone_shifted_to_dc(f_tone):
    """A tone at f_tone is brought to DC by DDC(norm_freq=-f_tone, ...)."""
    ddc = DDC(-f_tone, 0.25)
    offset = 0
    y_last = None
    for _ in range(4):
        x = _tone(f_tone, N, offset)
        y_last = ddc.execute(x)
        offset += N
    dominant = _dominant_freq(y_last)
    assert abs(dominant) < 0.02, (
        f"f_tone={f_tone}: dominant at {dominant:.4f}, expected near 0"
    )


def test_ddc_execute_result_survives_buffer_grow():
    # gh-219 regression: holding an execute() result across a larger execute()
    # (which grows the internal buffer) must not dangle. The output is a fresh
    # numpy-owned array per call, not a view of a realloc'd internal buffer.
    d = DDC(0.1, 0.25)
    rng = np.random.default_rng(0)
    y1 = d.execute(
        (rng.standard_normal(64) + 1j * rng.standard_normal(64)).astype(
            np.complex64
        )
    )
    snapshot = y1.copy()
    big = d.execute(
        (rng.standard_normal(8192) + 1j * rng.standard_normal(8192)).astype(
            np.complex64
        )
    )
    assert y1.ctypes.data != big.ctypes.data  # independent buffers
    assert np.array_equal(y1, snapshot)  # no use-after-free


def test_ddc_state_roundtrip_resume():
    """The serializable (elastic) face: serialize the complex DDC mid-stream,
    restore into a fresh DDC from the same (norm_freq, rate), and resume — the
    continuation matches an uninterrupted run bit-for-bit; a wrong-size or
    clobbered blob is rejected."""
    rng = np.random.default_rng(7)
    x = (rng.standard_normal(2400) + 1j * rng.standard_normal(2400)).astype(
        np.complex64
    )
    cut = 900

    ref = DDC(-0.1, 0.25)
    ref.execute(x[:cut])
    tail_ref = ref.execute(x[cut:])

    a = DDC(-0.1, 0.25)
    a.execute(x[:cut])
    blob = a.get_state()
    assert isinstance(blob, bytes) and len(blob) == a.state_bytes()

    b = DDC(-0.1, 0.25)
    b.set_state(blob)
    assert np.array_equal(b.execute(x[cut:]), tail_ref)

    with pytest.raises(ValueError):  # size mismatch
        b.set_state(blob[:-1])
    with pytest.raises(ValueError):  # clobbered envelope magic
        b.set_state(bytes([blob[0] ^ 0xFF]) + blob[1:])


# ------------------------------------------------------------------ #
# Layer 3 — pulse passthrough and the two control ports                #
# ------------------------------------------------------------------ #


def _rrc_bpsk(
    sps: float,
    n_sym: int,
    fc: float,
    beta: float = 0.35,
    span: int = 8,
    seed: int = 3,
) -> tuple[np.ndarray, np.ndarray]:
    """RRC-shaped BPSK on a carrier, plus the symbols that made it.

    Amplitude 0.25 keeps a CIC plan inside its +-1.0 input bound.
    """
    rng = np.random.default_rng(seed)
    bits = rng.integers(0, 2, n_sym) * 2 - 1
    n = int(n_sym * sps) + 64
    i = np.arange(n)
    t = (i[:, None] - (np.arange(n_sym) + span) * sps) / sps
    h = np.zeros_like(t)
    m = np.abs(t) <= span
    tt = t[m]
    # Root-raised-cosine impulse response (t in symbols).
    num = np.sin(np.pi * tt * (1 - beta)) + 4 * beta * tt * np.cos(
        np.pi * tt * (1 + beta)
    )
    den = np.pi * tt * (1 - (4 * beta * tt) ** 2)
    h[m] = np.where(np.abs(den) < 1e-12, 0.0, num / np.where(den == 0, 1, den))
    # Removable singularities at t = 0 and t = +-1/(4 beta).
    h[m & (np.abs(t) < 1e-9)] = 1 - beta + 4 * beta / np.pi
    a = h @ bits
    x = (0.25 * a * np.exp(2j * np.pi * fc * i)).astype(np.complex64)
    return x, bits


def test_matched_is_a_flavor_of_the_same_object():
    """MatchedDDC is the same object built by a different constructor: same
    state, same methods, one pulse-shaped terminal stage instead of the
    Kaiser one."""
    plain = DDC(0.0, 0.125)
    matched = MatchedDDC(0.0, 0.125, pulse="rrc")
    assert plain.rate == matched.rate == 0.125
    x = np.zeros(1024, dtype=np.complex64)
    assert len(matched.execute(x)) == len(plain.execute(x))
    assert matched.state_bytes() != plain.state_bytes()  # different plan
    with pytest.raises(ValueError):
        MatchedDDC(0.0, 0.125, pulse="bogus")


def test_narrow_rectangle_warns_at_construction():
    """A one-symbol-wide rectangle sampled twice per symbol is a two-tap
    matched filter — it builds, it just barely opens the eye, so the object
    says so instead of leaving it to be discovered downstream."""
    with pytest.warns(UserWarning, match="iandd"):
        MatchedDDC(0.0, 0.125, pulse="iandd", pulse_sps=2.0)

    # Wide enough, or a pulse the caveat does not apply to: silent.
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        wide = MatchedDDC(0.0, 0.125, pulse="iandd", pulse_sps=4.0)
        rrc = MatchedDDC(0.0, 0.125, pulse="rrc", pulse_sps=2.0)

    # The same condition is readable, not only raisable — the push and pull
    # halves of one diagnostic.
    assert not wide.narrow_pulse
    assert not rrc.narrow_pulse
    assert not DDC(0.0, 0.125).narrow_pulse
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        assert MatchedDDC(
            0.0, 0.125, pulse="iandd", pulse_sps=2.0
        ).narrow_pulse


def test_freq_port_is_the_lo_axis():
    """Steering by f from a centre of zero is the same stream as tuning the
    LO to f and steering by nothing — bit for bit. The port adds no scaling
    and no state: the centre frequency never moves."""
    rng = np.random.default_rng(11)
    x = (
        0.25 * (rng.standard_normal(4096) + 1j * rng.standard_normal(4096))
    ).astype(np.complex64)
    f = 0.037

    steered = MatchedDDC(0.0, 0.125, pulse="rrc").execute_ctrl(x, 0.0, f)
    tuned = MatchedDDC(f, 0.125, pulse="rrc").execute_ctrl(x, 0.0, 0.0)
    unsteered = MatchedDDC(0.0, 0.125, pulse="rrc").execute_ctrl(x, 0.0, 0.0)

    assert np.array_equal(steered, tuned)
    assert not np.allclose(steered, unsteered)  # teeth

    d = MatchedDDC(0.0, 0.125, pulse="rrc")
    d.execute_ctrl(x, 0.0, f)
    assert d.norm_freq == 0.0


def test_push_equals_block_on_both_ports():
    """The block form is the cheap open-loop path; the push form is the only
    one a closed loop can use. Held constant, they must not drift."""
    rng = np.random.default_rng(5)
    x = (
        0.25 * (rng.standard_normal(2048) + 1j * rng.standard_normal(2048))
    ).astype(np.complex64)

    block = MatchedDDC(-0.1, 0.125, pulse="rrc").execute_ctrl(x, 2e-3, 1e-2)
    pusher = MatchedDDC(-0.1, 0.125, pulse="rrc")
    pushed = np.concatenate(
        [pusher.execute_ctrl_push(complex(v), 2e-3, 1e-2) for v in x]
    )
    assert np.array_equal(block, pushed)


def test_matched_ddc_recovers_symbols():
    """End to end: a carrier carrying RRC-BPSK at 16 samples/symbol comes out
    as symbols at two per symbol, matched-filtered by the same dot product
    that decimated it. rate = 2/16 divides exactly, so the plain planner would
    leave nothing steerable at the end — the matched path appends the stage
    anyway.

    The assertion is that every symbol is recovered, not a filter-quality
    number: the transmit timing phase here is arbitrary and the strobe grid is
    fixed (nothing steers it until Layer 2's loop is wrapped around this), so
    an EVM measured at one phase measures the phase, not the filter. The C
    suite sweeps the phase grid and pins -45 dB.
    """
    fc = 0.09375
    x, bits = _rrc_bpsk(16.0, 400, fc)
    y = MatchedDDC(-fc, 2 / 16, pulse="rrc").execute(x)

    # Best alignment over strobe parity and lag; fit the complex gain.
    best, best_ber = np.inf, 1.0
    for par in range(2):
        for lag in range(140):
            idx = lag + par + 2 * np.arange(40, 360)
            if idx[-1] >= len(y):
                continue
            s, b = y[idx], bits[40:360]
            g = np.mean(s * b)
            evm = np.sqrt(np.mean(np.abs(s - g * b) ** 2) / abs(g) ** 2)
            if evm < best:
                best = evm
                best_ber = np.mean(np.sign((s / g).real) != b)
    assert best_ber == 0.0  # every symbol recovered, noiseless
    assert 20 * np.log10(best) < -15.0


def test_carrier_loop_pulls_in_through_the_freq_port():
    """The carrier port closes. A tone 0.01 cycles/sample off the LO's tuning
    spins at the output; a first-order loop reading the output phase increment
    and writing freq_ctrl drives it to zero and parks on the mistune.

    The gain is small because the loop closes AROUND the matched filter, so
    its dead time is that filter's group delay — the same reason a receiver's
    carrier loop bandwidth is a small fraction of the symbol rate.
    """
    f0, tuned, rate, mu = 0.05, -0.04, 0.25, 0.01
    d = MatchedDDC(tuned, rate, pulse="rrc")
    x = (0.25 * np.exp(2j * np.pi * f0 * np.arange(8192))).astype(np.complex64)

    freq_ctrl, prev, e = 0.0, None, 0.0
    for i, v in enumerate(x):
        for o in d.execute_ctrl_push(complex(v), 0.0, freq_ctrl):
            if prev is not None and i > 1024:
                e = float(np.angle(o * np.conj(prev)) / (2 * np.pi))
                freq_ctrl -= mu * e * rate
            prev = o

    assert abs(f0 + tuned + freq_ctrl) < 1e-6  # parked on the mistune
    assert abs(e) < 1e-5  # residual gone
    assert d.norm_freq == tuned  # the centre never moved


def test_clipped_forwards_from_the_cascade():
    """The CIC's silent input bound survives the composition: a DDC that
    plans a CIC inherits it, and nothing in the samples says so."""
    x = (2.0 * np.cos(0.11 * np.arange(1024))).astype(np.complex64)

    d = MatchedDDC(0.0, 2 / 64, pulse="rrc")  # plans CIC(32)
    assert d.clipped is False
    d.execute(x)
    assert d.clipped is True
    d.reset()
    assert d.clipped is False  # sticky until reset, not forever

    h = MatchedDDC(0.0, 0.5, pulse="rrc")  # halfband plan: no CIC, scale-free
    h.execute(x)
    assert h.clipped is False
