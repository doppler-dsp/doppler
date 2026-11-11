"""Tests for HalfbandDecimatorR2C (a jm object since #3002).

HalfbandDecimatorR2C wraps dp_hbdecim_r2c (real F32 -> CF32), the AVX-512
halfband path with the fs/4 shift built in. HalfbandDecimator's own cases
live in test_hbdecim.py.

Spectral tests use a Blackman-Harris window to get ~90 dB of dynamic
range, matching the ~60 dB filter rejection spec.

HalfbandDecimatorR2C specifics:
  - Input: real float32 at fs
  - Output: CF32 at fs/2 with embedded fs/4 frequency shift
  - A tone at fs/4 (norm_freq=0.25) lands at DC after the decimator.
  - A tone at f_in lands at f_in - fs/4 = f_in - 0.25 in the output
    (output-normalised: (f_in - 0.25) / 0.5).
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.resample import (
    HalfbandDecimatorR2C,
    _halfband_bank,
)

# ------------------------------------------------------------------ #
# Fixture: FIR branch of a 60 dB Kaiser halfband bank                #
# ------------------------------------------------------------------ #


@pytest.fixture(scope="module")
def fir_h() -> np.ndarray:
    """Return the FIR branch (1-D float32) of the default halfband bank."""
    bank = _halfband_bank()  # shape (2, N), 60 dB
    centre = bank.shape[1] // 2
    # The delay branch has a large centre tap; FIR branch has smaller.
    fir_row = (
        0 if abs(float(bank[0, centre])) < abs(float(bank[1, centre])) else 1
    )
    return np.ascontiguousarray(bank[fir_row])


# ------------------------------------------------------------------ #
# Spectral helpers                                                    #
# ------------------------------------------------------------------ #


def _blackman_harris(n: int) -> np.ndarray:
    a = [0.35875, 0.48829, 0.14128, 0.01168]
    k = 2 * np.pi * np.arange(n) / n
    return (
        a[0] - a[1] * np.cos(k) + a[2] * np.cos(2 * k) - a[3] * np.cos(3 * k)
    )


def _spectrum_db(signal: np.ndarray):
    """Return (freqs, amplitude_dB) using Blackman-Harris window."""
    n = len(signal)
    w = _blackman_harris(n)
    cg = w.mean()
    S = np.fft.fft(signal * w, 4 * n)
    amp_db = 20 * np.log10(np.abs(S) / (n * cg) + 1e-300)
    return np.fft.fftfreq(4 * n), amp_db


def _peak_near(bins, db, freq, tol=0.02) -> float:
    mask = np.abs(bins - freq) < tol
    return float(db[mask].max()) if mask.any() else -300.0


# ------------------------------------------------------------------ #
# HalfbandDecimatorR2C                                               #
# ------------------------------------------------------------------ #


class TestHalfbandDecimatorR2C:
    def test_create(self, fir_h):
        d = HalfbandDecimatorR2C(fir_h)
        assert d.rate == pytest.approx(0.5)

    def test_num_taps(self, fir_h):
        d = HalfbandDecimatorR2C(fir_h)
        assert d.num_taps == len(fir_h)

    # ---- execute output properties ---------------------------------- #

    def test_execute_output_dtype_is_complex64(self, fir_h):
        d = HalfbandDecimatorR2C(fir_h)
        y = d.execute(np.ones(128, dtype=np.float32))
        assert y.dtype == np.complex64

    def test_execute_length_even(self, fir_h):
        d = HalfbandDecimatorR2C(fir_h)
        y = d.execute(np.ones(128, dtype=np.float32))
        assert len(y) == 64

    def test_float64_input_is_refused_not_narrowed(self, fir_h):
        """float64 input raises rather than being silently narrowed to
        float32: the library's unsafe-cast rule. (The hand-written binding
        this object replaced force-cast it, #3002.)"""
        d = HalfbandDecimatorR2C(fir_h)
        with pytest.raises(TypeError):
            d.execute(np.ones(128, dtype=np.float64))
        assert d.execute(np.ones(128, dtype=np.float32)).dtype == np.complex64

    def test_float64_taps_are_refused_not_narrowed(self, fir_h):
        """The taps half of the same rule: float64 taps raise, float32
        taps build."""
        with pytest.raises(TypeError):
            HalfbandDecimatorR2C(fir_h.astype(np.float64))
        assert HalfbandDecimatorR2C(fir_h.astype(np.float32)).num_taps > 0

    # ---- reset ------------------------------------------------------ #

    def test_reset_matches_fresh(self, fir_h):
        x = np.random.default_rng(1).standard_normal(256).astype(np.float32)
        d1 = HalfbandDecimatorR2C(fir_h)
        d2 = HalfbandDecimatorR2C(fir_h)
        out1 = d1.execute(x)
        _ = d2.execute(x)
        d2.reset()
        out2 = d2.execute(x)
        np.testing.assert_array_equal(out1, out2)

    # ---- spectral: tone at fs/4 → DC -------------------------------- #

    def test_tone_at_quarter_fs_goes_to_dc(self, fir_h):
        """A real tone at fs/4 (norm_freq=0.25) is shifted to DC by
        the embedded fs/4 mix and should dominate at bin 0.

        The decimator multiplies each input sample by e^{j(π/2)k},
        which rotates the spectrum by -fs/4.  A tone at +fs/4 thus
        lands at DC before the halfband filter sees it.
        """
        N = 4096
        # Run several blocks with continuous time.
        d = HalfbandDecimatorR2C(fir_h)
        offset = 0
        y_last = None
        for _ in range(4):
            t = np.arange(N, dtype=np.float64) + offset
            x = np.cos(2 * np.pi * 0.25 * t).astype(np.float32)
            y_last = d.execute(x)
            offset += N
        bins, db = _spectrum_db(y_last)
        dominant = float(bins[np.argmax(db)])
        assert abs(dominant) < 0.02, (
            f"Tone at fs/4: dominant output bin at {dominant:.4f}, expected ~0"
        )

    # ---- spectral: passband tone survives ---------------------------- #

    def test_passband_tone_survives(self, fir_h):
        """A real tone at fs/4 + 0.05 survives with < 1 dB amplitude change.

        After the embedded -fs/4 shift, a tone at f_in=0.3 appears at
        f_in - 0.25 = 0.05 in the intermediate domain.  Output-normalised:
        0.05 / 0.5 = 0.1.  This is well inside the passband (edge at 0.2
        in intermediate = 0.4 output-normalised).

        Time must be continuous across blocks so the cosine phase stays
        coherent with the filter's internal phase rotation.
        """
        N = 4096
        f_in = 0.30  # 0.30 - 0.25 = 0.05 intermediate → 0.10 output-norm
        d = HalfbandDecimatorR2C(fir_h)
        offset = 0
        y_last = None
        for _ in range(4):
            t = np.arange(N, dtype=np.float64) + offset
            x = np.cos(2 * np.pi * f_in * t).astype(np.float32)
            y_last = d.execute(x)
            offset += N
        f_out_expected = 0.10
        bins, db = _spectrum_db(y_last)
        amp = _peak_near(bins, db, f_out_expected, tol=0.05)
        # A real cosine has power split between ±f, and only the positive
        # sideband (at f_in - fs/4 = 0.05) passes the R2C filter -- but the
        # analytic-signal convention DOUBLES it, so `Re{y}` reconstructs the
        # input rather than half of it.  Amplitude is therefore preserved:
        # 0 dB, not the -6.02 dB the half-convention gave.  Allow ±1 dB.
        assert abs(amp - 0.0) < 1.0, (
            f"f_in={f_in}: amplitude {amp:+.2f} dB, expected ~0 dB"
        )

    # ---- context manager -------------------------------------------- #

    def test_context_manager(self, fir_h):
        with HalfbandDecimatorR2C(fir_h) as d:
            y = d.execute(np.ones(64, dtype=np.float32))
            assert y.dtype == np.complex64
