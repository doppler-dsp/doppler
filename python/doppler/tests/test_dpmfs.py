"""
Tests for DPMFS coefficient fitting (doppler.polyphase.fit_dpmfs).

Checks:
  - Fit quality: polynomial reconstructs the source bank to within
    a tight tolerance (rms < 1e-4 for M=3, L=4096).
  - Evaluate: output matches the source bank at sampled μ values.
  - Coefficient shapes and dtypes.
  - Edge cases: M=1 (linear), different L/N combinations.
  - to_c_header: generates syntactically plausible C.
  - validate: rms error reported matches direct evaluation.
"""

import numpy as np
import pytest

from doppler.polyphase import kaiser_prototype, fit_dpmfs


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _build_bank(atten=60.0, image=80.0, pb=0.4, sb=0.6):
    """Return (prototype_flat, polyphase[L, N])."""
    _, bank = kaiser_prototype(
        attenuation=atten,
        passband=pb,
        stopband=sb,
        image_attenuation=image,
    )
    return bank


# ---------------------------------------------------------------------------
# Basic shape / dtype
# ---------------------------------------------------------------------------


class TestFitDPMFSShape:
    def test_c_shape(self):
        bank = _build_bank()
        L, N = bank.shape
        coeffs = fit_dpmfs(bank, M=3)
        assert coeffs.c.shape == (2, 4, N)

    def test_c_dtype(self):
        bank = _build_bank()
        coeffs = fit_dpmfs(bank)
        assert coeffs.c.dtype == np.float32

    def test_M_stored(self):
        bank = _build_bank()
        coeffs = fit_dpmfs(bank, M=3)
        assert coeffs.M == 3

    def test_N_matches_bank(self):
        bank = _build_bank()
        _, N = bank.shape
        coeffs = fit_dpmfs(bank)
        assert coeffs.N == N

    def test_linear_M1_shape(self):
        bank = _build_bank()
        _, N = bank.shape
        coeffs = fit_dpmfs(bank, M=1)
        assert coeffs.c.shape == (2, 2, N)

    def test_bad_L_raises(self):
        rng = np.random.default_rng(0)
        bad = rng.random((100, 10)).astype(np.float32)
        with pytest.raises(ValueError, match="power of two"):
            fit_dpmfs(bad)

    def test_bad_M_raises(self):
        bank = _build_bank()
        with pytest.raises(ValueError, match="M must be"):
            fit_dpmfs(bank, M=0)


# ---------------------------------------------------------------------------
# Fit quality — polynomial reconstructs the source bank
# ---------------------------------------------------------------------------


class TestFitQuality:
    """RMS and max-absolute coefficient error vs the reference bank."""

    def test_rms_cubic_80db(self):
        """M=3 fit on an 80 dB, 4096-phase bank should be very tight."""
        bank = _build_bank(atten=60.0, image=80.0)
        coeffs = fit_dpmfs(bank, M=3)
        assert coeffs.residual_rms < 1e-4

    def test_rms_cubic_65db(self):
        bank = _build_bank(atten=60.0, image=65.0)
        coeffs = fit_dpmfs(bank, M=3)
        assert coeffs.residual_rms < 1e-4

    def test_rms_linear_looser(self):
        """M=1 is looser but should still be < 1e-2."""
        bank = _build_bank(atten=60.0, image=80.0)
        coeffs = fit_dpmfs(bank, M=1)
        assert coeffs.residual_rms < 1e-2

    def test_cubic_better_than_linear(self):
        bank = _build_bank()
        c1 = fit_dpmfs(bank, M=1)
        c3 = fit_dpmfs(bank, M=3)
        assert c3.residual_rms < c1.residual_rms


# ---------------------------------------------------------------------------
# evaluate() — matches the source bank at sampled μ
# ---------------------------------------------------------------------------


class TestEvaluate:
    def test_evaluate_at_phase_boundaries(self):
        """evaluate(0) and evaluate(0.5) should match bank rows 0 and L/2."""
        bank = _build_bank()
        L, N = bank.shape
        coeffs = fit_dpmfs(bank, M=3)

        h0 = coeffs.evaluate(0.0).astype(np.float64)
        ref0 = bank[0, :].astype(np.float64)
        assert np.max(np.abs(h0 - ref0)) < 1e-3

        h_half = coeffs.evaluate(0.5).astype(np.float64)
        ref_half = bank[L // 2, :].astype(np.float64)
        assert np.max(np.abs(h_half - ref_half)) < 1e-3

    def test_evaluate_shape(self):
        bank = _build_bank()
        coeffs = fit_dpmfs(bank, M=3)
        h = coeffs.evaluate(0.3)
        assert h.shape == (coeffs.N,)
        assert h.dtype == np.float32

    def test_evaluate_mu_wraps(self):
        """evaluate(μ) == evaluate(μ + 1)."""
        bank = _build_bank()
        coeffs = fit_dpmfs(bank, M=3)
        mu = 0.37
        np.testing.assert_array_equal(coeffs.evaluate(mu), coeffs.evaluate(mu + 1.0))

    def test_evaluate_grid_rms(self):
        """RMS error over 256 uniformly spaced μ values < 5e-4."""
        bank = _build_bank()
        L, N = bank.shape
        coeffs = fit_dpmfs(bank, M=3)
        stride = L // 256
        mus = np.arange(256) / 256
        errs = []
        for i, mu in enumerate(mus):
            h = coeffs.evaluate(mu).astype(np.float64)
            ref = bank[i * stride, :].astype(np.float64)
            errs.append(np.mean((h - ref) ** 2))
        rms = float(np.sqrt(np.mean(errs)))
        assert rms < 5e-4


# ---------------------------------------------------------------------------
# validate()
# ---------------------------------------------------------------------------


class TestValidate:
    def test_validate_returns_dict(self):
        bank = _build_bank()
        coeffs = fit_dpmfs(bank, M=3)
        result = coeffs.validate(bank, verbose=False)
        assert "rms" in result
        assert "max_abs" in result

    def test_validate_rms_consistent(self):
        """validate() rms should be close to residual_rms from fit."""
        bank = _build_bank()
        coeffs = fit_dpmfs(bank, M=3)
        result = coeffs.validate(bank, verbose=False)
        # validate uses evaluate() which is float32; residual_rms is
        # float64 — allow a small delta
        assert abs(result["rms"] - coeffs.residual_rms) < 1e-5

    def test_validate_max_abs_positive(self):
        bank = _build_bank()
        coeffs = fit_dpmfs(bank, M=3)
        result = coeffs.validate(bank, verbose=False)
        assert result["max_abs"] >= 0.0


# ---------------------------------------------------------------------------
# to_c_header()
# ---------------------------------------------------------------------------


class TestToCHeader:
    def test_contains_define_M(self):
        bank = _build_bank()
        coeffs = fit_dpmfs(bank, M=3)
        h = coeffs.to_c_header("dpmfs")
        assert "#define DPMFS_M  3" in h

    def test_contains_define_N(self):
        bank = _build_bank()
        _, N = bank.shape
        coeffs = fit_dpmfs(bank, M=3)
        h = coeffs.to_c_header("dpmfs")
        assert f"#define DPMFS_N  {N}" in h

    def test_contains_all_arrays(self):
        bank = _build_bank()
        coeffs = fit_dpmfs(bank, M=3)
        h = coeffs.to_c_header("dpmfs")
        for j in range(2):
            for m in range(4):
                assert f"dpmfs_c{j}_m{m}" in h

    def test_custom_name(self):
        bank = _build_bank()
        coeffs = fit_dpmfs(bank, M=3)
        h = coeffs.to_c_header("myfilter")
        assert "myfilter_c0_m0" in h
        assert "#define MYFILTER_M" in h
