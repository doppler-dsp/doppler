"""Tests for doppler.polyphase — filter bank designer."""

import numpy as np
import pytest

from doppler.polyphase import design_bank, to_c_header, to_npy


# ---------------------------------------------------------------------------
# design_bank — Kaiser (pure NumPy, no optional deps)
# ---------------------------------------------------------------------------

class TestDesignBankKaiser:
    def test_shape_default(self):
        bank = design_bank()
        assert bank.shape == (4096, 19)

    def test_dtype_f32(self):
        bank = design_bank()
        assert bank.dtype == np.float32

    def test_custom_shape(self):
        bank = design_bank(num_phases=512, num_taps=7)
        assert bank.shape == (512, 7)

    def test_dc_gain_unity(self):
        """Sum of all taps in any row ≈ 1 (passband gain = 1 at DC)."""
        bank = design_bank(num_phases=256, num_taps=15)
        # Phase-0 sub-filter carries most of the energy; its DC sum ≈ 1/N.
        # Full bank summed across all phases should ≈ 1.
        total = bank.sum()
        assert abs(total - 1.0) < 0.05

    def test_symmetry_prototype(self):
        """Prototype h = bank.ravel() should be approximately symmetric."""
        bank = design_bank(num_phases=64, num_taps=11)
        h = bank.ravel()
        assert np.allclose(h, h[::-1], atol=1e-5)

    def test_attenuation_increases_energy_in_stopband(self):
        """Higher attenuation → smaller stopband energy."""
        def stopband_energy(att):
            bank = design_bank(
                num_phases=64, num_taps=11,
                bands=[0.0, 0.3, 0.7, 1.0],
                amps=[1.0, 1.0, 0.0, 0.0],
                attenuation_db=att,
            )
            h = bank.ravel()
            N = len(h)
            H = np.abs(np.fft.rfft(h, n=4 * N))
            # rfftfreq is 0..0.5; our normalised scale is 0..1
            freqs = np.fft.rfftfreq(4 * N) * 2
            stop = H[freqs > 0.7]
            return float(np.sum(stop ** 2))

        assert stopband_energy(80.0) < stopband_energy(40.0)

    def test_unknown_method_raises(self):
        with pytest.raises(ValueError, match="Unknown method"):
            design_bank(method="bogus")


# ---------------------------------------------------------------------------
# design_bank — firls (requires SciPy; skipped if not installed)
# ---------------------------------------------------------------------------

try:
    import scipy  # noqa: F401
    _scipy_available = True
except ImportError:
    _scipy_available = False

firls_skip = pytest.mark.skipif(
    not _scipy_available,
    reason="SciPy not installed — skipping firls tests",
)


@firls_skip
class TestDesignBankFirls:
    def test_shape(self):
        bank = design_bank(
            num_phases=128, num_taps=9, method="firls"
        )
        assert bank.shape == (128, 9)

    def test_dtype_f32(self):
        bank = design_bank(
            num_phases=128, num_taps=9, method="firls"
        )
        assert bank.dtype == np.float32

    def test_prototype_symmetric(self):
        bank = design_bank(
            num_phases=64, num_taps=11, method="firls"
        )
        h = bank.ravel()
        assert np.allclose(h, h[::-1], atol=1e-5)


# ---------------------------------------------------------------------------
# to_c_header
# ---------------------------------------------------------------------------

class TestToCHeader:
    def test_returns_string(self):
        bank = design_bank(num_phases=4, num_taps=3)
        out = to_c_header(bank, name="test_bank")
        assert isinstance(out, str)

    def test_contains_defines(self):
        bank = design_bank(num_phases=4, num_taps=3)
        out = to_c_header(bank, name="test_bank")
        assert "TEST_BANK_NUM_PHASES 4" in out
        assert "TEST_BANK_NUM_TAPS   3" in out

    def test_contains_array(self):
        bank = design_bank(num_phases=4, num_taps=3)
        out = to_c_header(bank, name="test_bank")
        assert "static const float test_bank[4][3]" in out

    def test_num_rows_match(self):
        bank = design_bank(num_phases=4, num_taps=3)
        out = to_c_header(bank, name="test_bank")
        # Each row is a { ... } block
        assert out.count("{ ") == 4

    def test_writes_file(self, tmp_path):
        bank = design_bank(num_phases=4, num_taps=3)
        p = tmp_path / "bank.h"
        to_c_header(bank, name="bank", path=p)
        assert p.exists()
        assert "static const float bank[4][3]" in p.read_text()


# ---------------------------------------------------------------------------
# to_npy
# ---------------------------------------------------------------------------

class TestToNpy:
    def test_roundtrip(self, tmp_path):
        bank = design_bank(num_phases=16, num_taps=5)
        p = tmp_path / "bank.npy"
        to_npy(bank, p)
        loaded = np.load(str(p))
        assert loaded.shape == (16, 5)
        assert np.allclose(bank, loaded, atol=0)
