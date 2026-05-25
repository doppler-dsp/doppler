"""Signal-level tests for doppler.filter.CIC.

Covers lifecycle, spectral passband, alias rejection, and decimation
correctness.  All frequency-domain checks use a zero-padded Blackman-Harris
windowed FFT so spurious lobes don't mask real failures.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.filter import CIC

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _blackman_harris(n: int) -> np.ndarray:
    a = [0.35875, 0.48829, 0.14128, 0.01168]
    k = 2 * np.pi * np.arange(n) / n
    return a[0] - a[1]*np.cos(k) + a[2]*np.cos(2*k) - a[3]*np.cos(3*k)


def _power_db(signal: np.ndarray, freq_norm: float, pad: int = 4) -> float:
    """Return dBFS power of the component nearest freq_norm (0..1) in signal."""
    n = len(signal)
    w = _blackman_harris(n)
    cg = w.mean()
    S = np.fft.fft(signal * w, n * pad)
    bins = np.fft.fftfreq(n * pad)
    idx = int(np.argmin(np.abs(bins - freq_norm)))
    peak = float(np.abs(S[idx])) / (n * cg)
    return 20.0 * np.log10(peak + 1e-300)


def _tone(freq_norm: float, n: int, dtype=np.complex64) -> np.ndarray:
    """Complex exponential at freq_norm (cycles/sample, 0..1)."""
    t = np.arange(n)
    return np.exp(2j * np.pi * freq_norm * t).astype(dtype)


def _decimate(cic: CIC, x: np.ndarray, block: int = 0) -> np.ndarray:
    """Feed x through cic in blocks, return concatenated output."""
    if block == 0:
        return np.array(cic.decimate(x), copy=True)
    chunks = []
    for i in range(0, len(x), block):
        out = cic.decimate(x[i:i + block])
        if len(out):
            chunks.append(np.array(out, copy=True))
    return np.concatenate(chunks) if chunks else np.array([], dtype=np.complex64)


# ---------------------------------------------------------------------------
# Lifecycle
# ---------------------------------------------------------------------------

class TestLifecycle:
    def test_create_defaults(self):
        obj = CIC(1, 4, 1)
        assert obj is not None

    def test_properties(self):
        obj = CIC(8, 4, 1)
        assert obj.R == 8
        assert obj.N == 4
        assert obj.M == 1
        assert obj.input_scale > 0.0
        assert obj.output_scale > 0.0

    def test_context_manager(self):
        with CIC(1, 4, 1) as obj:
            assert obj.R == 1

    def test_destroy(self):
        obj = CIC(1, 4, 1)
        obj.destroy()

    def test_reset_reproducible(self):
        R, N = 8, 3
        n_in = 8 * R * N
        x = _tone(0.01, n_in)
        obj = CIC(R, N, 1)
        out1 = np.array(obj.decimate(x), copy=True)
        obj.reset()
        out2 = np.array(obj.decimate(x), copy=True)
        np.testing.assert_array_equal(out1, out2)

    def test_reconfigure(self):
        obj = CIC(4, 2, 1)
        obj.reconfigure(8, 3, 1)
        assert obj.R == 8
        assert obj.N == 3


# ---------------------------------------------------------------------------
# Decimation mechanics
# ---------------------------------------------------------------------------

class TestDecimation:
    def test_output_count(self):
        R = 8
        obj = CIC(R, 4, 1)
        x = np.zeros(4 * R, dtype=np.complex64)
        out = obj.decimate(x)
        assert len(out) == 4

    def test_partial_block_accumulates(self):
        """R-1 samples produce 0 outputs; the R-th completes the cycle."""
        R = 8
        obj = CIC(R, 3, 1)
        x = np.zeros(R, dtype=np.complex64)
        assert len(obj.decimate(x[:R - 1])) == 0
        assert len(obj.decimate(x[R - 1:])) == 1

    def test_zero_input(self):
        obj = CIC(4, 4, 1)
        x = np.zeros(64, dtype=np.complex64)
        out = obj.decimate(x)
        np.testing.assert_array_equal(out, 0)

    def test_streaming_continuity(self):
        """Splitting input at an arbitrary boundary must give identical output."""
        R, N, n_in = 16, 3, 8 * 16
        x = _tone(0.02, n_in)
        obj_whole = CIC(R, N, 1)
        obj_split = CIC(R, N, 1)
        out_whole = _decimate(obj_whole, x)
        out_split = _decimate(obj_split, x, block=R)
        np.testing.assert_array_almost_equal(out_whole, out_split, decimal=6)

    def test_dc_passthrough(self):
        """Settled DC output must equal 1.0 within float tolerance."""
        R, N = 8, 4
        n_in = 12 * R * N          # well past N*(R-1) transient
        x = np.ones(n_in, dtype=np.complex64)
        obj = CIC(R, N, 1)
        out = obj.decimate(x)
        # Last quarter of output is fully settled
        settled = out[len(out) * 3 // 4:]
        np.testing.assert_allclose(np.abs(settled), 1.0, atol=1e-4)


# ---------------------------------------------------------------------------
# Spectral quality
# ---------------------------------------------------------------------------

class TestSpectralQuality:
    """Verify CIC passband gain and alias/image rejection via FFT."""

    # Configuration: R=8, N=4 — typical wideband SDR frontend
    R = 8
    N = 4
    # Passband tone: 5% of output Nyquist = 0.05/(2*R) of input fs
    F_PASS = 0.5 / (2 * R) * 0.10    # ≈ 0.0031 normalised to input fs
    # Alias zone: a tone at exactly fs/R aliases to DC after decimation;
    # test slightly off-null to get a well-defined attenuation figure.
    F_ALIAS = 1.0 / R * 0.95          # just inside first null

    @pytest.fixture(scope="class")
    def settled_output(self):
        """
        Returns (x_in, y_out) for a two-tone input:
          - passband tone at F_PASS
          - alias-zone tone at F_ALIAS
        Both at unit amplitude.  n_in chosen so that at least 256 output
        samples are available after the CIC transient.
        """
        n_transient = self.N * (self.R - 1)   # samples to discard
        n_out_want = 512
        n_in = (n_transient + n_out_want) * self.R
        x = _tone(self.F_PASS, n_in) + _tone(self.F_ALIAS, n_in)
        obj = CIC(self.R, self.N, 1)
        y = _decimate(obj, x)
        # Drop transient, keep settled portion
        n_drop = n_transient // self.R + 1
        return x, y[n_drop:]

    def test_passband_gain(self, settled_output):
        """Passband tone must survive with < 3 dB loss."""
        _, y = settled_output
        # After R:1 decimation the passband tone is at F_PASS*R in output fs
        f_out = self.F_PASS * self.R
        gain_db = _power_db(y, f_out)
        assert gain_db > -3.0, (
            f"Passband tone at f={self.F_PASS:.4f} attenuated by "
            f"{-gain_db:.1f} dB (expected < 3 dB)"
        )

    def test_alias_rejection(self, settled_output):
        """Alias-zone tone must be attenuated by at least 20 dB.

        A 4-stage CIC with R=8 has its first null at f=fs/R.  At 0.95×fs/R
        the theoretical attenuation is ~34 dB.  We require ≥ 20 dB so the
        test is robust to floating-point and windowing imprecision while
        still catching a broken filter.
        """
        _, y = settled_output
        # The alias-zone tone folds to (F_ALIAS % (1/R)) * R in output fs
        f_alias_out = (self.F_ALIAS % (1.0 / self.R)) * self.R
        alias_db = _power_db(y, f_alias_out)
        passband_f_out = self.F_PASS * self.R
        passband_db = _power_db(y, passband_f_out)
        rejection_db = passband_db - alias_db
        assert rejection_db >= 20.0, (
            f"Alias rejection only {rejection_db:.1f} dB "
            f"(passband={passband_db:.1f} dB, alias={alias_db:.1f} dB); "
            f"expected ≥ 20 dB"
        )

    def test_stopband_null(self):
        """Tone at exactly fs/R (first CIC null) must be heavily attenuated."""
        R, N = 8, 4
        F_NULL = 1.0 / R
        n_in = 32 * R * N
        x = _tone(F_NULL, n_in)
        obj = CIC(R, N, 1)
        y = _decimate(obj, x)
        # Drop transient, measure power of the DC alias in output
        n_drop = N * (R - 1) // R + 4
        settled = y[n_drop:]
        rms = float(np.sqrt(np.mean(np.abs(settled) ** 2)))
        # First null is exact zero; floating-point will give something tiny
        assert rms < 1e-3, (
            f"Tone at CIC null (f=fs/R) has RMS {rms:.2e} in output "
            f"(expected < 1e-3)"
        )

    def test_image_rejection_m2(self):
        """M=2 CIC has deeper image rejection than M=1 at same N and R."""
        R, N = 8, 3
        n_in = 32 * R * N
        F_STOP = 0.8 / R              # in stopband for both
        x = _tone(F_STOP, n_in)

        obj_m1 = CIC(R, N, 1)
        obj_m2 = CIC(R, N, 2)
        y_m1 = _decimate(obj_m1, x)
        y_m2 = _decimate(obj_m2, x)

        n_drop = N * (R - 1) // R + 4
        rms_m1 = float(np.sqrt(np.mean(np.abs(y_m1[n_drop:]) ** 2)))
        rms_m2 = float(np.sqrt(np.mean(np.abs(y_m2[n_drop:]) ** 2)))

        # M=2 should be at least 6 dB better in the stopband
        assert rms_m2 < rms_m1, (
            f"M=2 (RMS={rms_m2:.3e}) not better than M=1 (RMS={rms_m1:.3e}) "
            f"at f={F_STOP:.3f}"
        )
