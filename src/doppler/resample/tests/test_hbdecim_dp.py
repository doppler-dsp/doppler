"""Tests for HalfbandDecimatorDp and HalfbandDecimatorR2C.

Both wrap the C library's dp_hbdecim_cf32_t / dp_hbdecim_r2cf32_t
(AVX-512 halfband path).  They are distinct from the just-makeit-internal
HalfbandDecimator that wraps hbdecim_core.c.

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
    HalfbandDecimatorDp,
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
# HalfbandDecimatorDp — construction                                 #
# ------------------------------------------------------------------ #


class TestHalfbandDecimatorDp:
    def test_create(self, fir_h):
        d = HalfbandDecimatorDp(fir_h)
        assert d.rate == pytest.approx(0.5)

    def test_num_taps(self, fir_h):
        d = HalfbandDecimatorDp(fir_h)
        assert d.num_taps == len(fir_h)

    def test_wrong_dtype_coerced(self, fir_h):
        """float64 h is coerced to float32 without error."""
        HalfbandDecimatorDp(fir_h.astype(np.float64))

    def test_2d_h_raises(self, fir_h):
        with pytest.raises(ValueError):
            HalfbandDecimatorDp(np.stack([fir_h, fir_h]))

    # ---- execute output properties ---------------------------------- #

    def test_execute_dtype(self, fir_h):
        d = HalfbandDecimatorDp(fir_h)
        y = d.execute(np.ones(128, dtype=np.complex64))
        assert y.dtype == np.complex64

    def test_execute_length_exact(self, fir_h):
        """Even-length input → exactly len(x)//2 output samples."""
        d = HalfbandDecimatorDp(fir_h)
        y = d.execute(np.ones(128, dtype=np.complex64))
        assert len(y) == 64

    def test_execute_odd_length(self, fir_h):
        """Odd-length block: one sample is buffered, rest decimated."""
        d = HalfbandDecimatorDp(fir_h)
        y = d.execute(np.ones(129, dtype=np.complex64))
        assert len(y) == 64  # dangling sample buffered

    # ---- reset ------------------------------------------------------ #

    def test_reset_matches_fresh(self, fir_h):
        x = np.random.default_rng(0).standard_normal(256).astype(np.complex64)
        d1 = HalfbandDecimatorDp(fir_h)
        d2 = HalfbandDecimatorDp(fir_h)
        out1 = d1.execute(x)
        _ = d2.execute(x)  # advance d2
        d2.reset()
        out2 = d2.execute(x)
        np.testing.assert_array_equal(out1, out2)

    # ---- spectral: passband gain ≈ 0 dB ----------------------------- #

    def test_passband_amplitude(self, fir_h):
        """Passband tone (f_in=0.1 → output at 0.2) changes amplitude < 0.5 dB.

        The halfband filter's passband edge is 0.4×(fs/2) = 0.2×fs_in.
        Frequencies below 0.2 in input-normalised terms are in the passband.
        """
        N = 4096
        f_in = 0.1
        t = np.arange(N)
        x = np.exp(2j * np.pi * f_in * t).astype(np.complex64)
        d = HalfbandDecimatorDp(fir_h)
        # Two blocks to flush transient
        _ = d.execute(x)
        y = d.execute(x)
        f_out = 2 * f_in  # output-normalised: 0.2
        bins, db = _spectrum_db(y)
        amp = _peak_near(bins, db, f_out)
        assert abs(amp) < 0.5, f"f_in={f_in}: passband amplitude {amp:+.2f} dB"

    # ---- spectral: stopband rejection ≥ 50 dBc --------------------- #

    def test_stopband_rejection(self, fir_h):
        """Stopband tone (f_in=0.4 → alias at -0.2 output) is ≥50 dB below
        passband.

        Passband tone f_pb=0.1 → output at 0.2.
        Stopband tone f_sb=0.4 is above the stopband edge (0.3×fs_in);
        it aliases to output bin 0.4/0.5 - 1 = -0.2 (complex-valued spectrum).
        """
        N = 4096
        f_pb = 0.1  # passband; output at 0.2
        f_sb = 0.4  # stopband; aliases to -0.2 in output
        t = np.arange(N)
        x = (
            np.exp(2j * np.pi * f_pb * t) + np.exp(2j * np.pi * f_sb * t)
        ).astype(np.complex64)
        d = HalfbandDecimatorDp(fir_h)
        _ = d.execute(x)
        y = d.execute(x)
        bins, db = _spectrum_db(y)
        # Passband peak at 0.2
        pb_peak = _peak_near(bins, db, 2 * f_pb)
        # Stopband alias at f_sb*2 - 1 = -0.2
        alias_out = 2 * f_sb - 1.0  # = -0.2
        alias_db = _peak_near(bins, db, alias_out)
        assert pb_peak - alias_db > 50.0, (
            f"Stopband alias: pb={pb_peak:.1f} dB, alias={alias_db:.1f} dB, "
            f"rejection={pb_peak - alias_db:.1f} dB (need >50)"
        )

    # ---- context manager -------------------------------------------- #

    def test_context_manager(self, fir_h):
        with HalfbandDecimatorDp(fir_h) as d:
            y = d.execute(np.ones(64, dtype=np.complex64))
            assert y.dtype == np.complex64


# ------------------------------------------------------------------ #
# HalfbandDecimatorR2C — construction                                #
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

    def test_execute_accepts_float64_input(self, fir_h):
        """Float64 real input is coerced to float32."""
        d = HalfbandDecimatorR2C(fir_h)
        y = d.execute(np.ones(128, dtype=np.float64))
        assert y.dtype == np.complex64

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
        # A real cosine has power split between ±f; only the positive
        # sideband (at f_in - fs/4 = 0.05) passes the R2C filter, so
        # the output is half amplitude = −6.02 dB.  Allow ±1 dB.
        assert abs(amp - (-6.02)) < 1.0, (
            f"f_in={f_in}: amplitude {amp:+.2f} dB, expected ~-6.02 dB"
        )

    # ---- context manager -------------------------------------------- #

    def test_context_manager(self, fir_h):
        with HalfbandDecimatorR2C(fir_h) as d:
            y = d.execute(np.ones(64, dtype=np.float32))
            assert y.dtype == np.complex64
