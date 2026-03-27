"""Tests for doppler.polyphase — filter bank designer."""

import numpy as np
import pytest

from doppler.polyphase import PolyphaseBank, design_bank, to_c_header, to_npy


# ---------------------------------------------------------------------------
# design_bank — Kaiser (pure NumPy, no optional deps)
# ---------------------------------------------------------------------------

class TestDesignBankKaiser:
    def test_returns_polyphase_bank(self):
        pb = design_bank()
        assert isinstance(pb, PolyphaseBank)

    def test_shape_default(self):
        pb = design_bank()
        assert pb.shape == (4096, 19)

    def test_dtype_f32(self):
        pb = design_bank()
        assert pb.dtype == np.float32

    def test_custom_shape(self):
        pb = design_bank(num_phases=512, num_taps=7)
        assert pb.shape == (512, 7)

    def test_metadata_stored(self):
        pb = design_bank(
            num_phases=64, num_taps=11,
            bands=(0.0, 0.3, 0.7, 1.0),
            amps=(1.0, 1.0, 0.0, 0.0),
            attenuation_db=70.0,
            method="kaiser",
        )
        assert pb.bands == (0.0, 0.3, 0.7, 1.0)
        assert pb.amps == (1.0, 1.0, 0.0, 0.0)
        assert pb.attenuation_db == 70.0
        assert pb.method == "kaiser"

    def test_dc_gain_unity(self):
        """Sum of all prototype taps ≈ 1 (DC gain = 1 at baseband)."""
        pb = design_bank(num_phases=256, num_taps=15)
        total = pb.bank.sum()
        assert abs(total - 1.0) < 0.05

    def test_symmetry_prototype(self):
        """Prototype h = bank.ravel() should be approximately symmetric."""
        pb = design_bank(num_phases=64, num_taps=11)
        h = pb.ravel()
        assert np.allclose(h, h[::-1], atol=1e-5)

    def test_attenuation_increases_energy_in_stopband(self):
        """Higher attenuation → smaller stopband energy.

        Uses enough taps (64 phases × 31) so both 40 dB and 80 dB
        designs are fully realised without transition-band truncation.
        """
        num_phases = 64
        num_taps = 31
        stopband_edge = 0.7  # baseband-normalised

        def stopband_energy(att):
            pb = design_bank(
                num_phases=num_phases, num_taps=num_taps,
                bands=[0.0, 0.3, 0.7, 1.0],
                amps=[1.0, 1.0, 0.0, 0.0],
                attenuation_db=att,
            )
            h = pb.ravel()
            N = len(h)
            H = np.abs(np.fft.rfft(h, n=4 * N))
            # prototype-rate normalised freqs (0..1)
            freqs = np.fft.rfftfreq(4 * N) * 2
            thresh = stopband_edge / num_phases
            return float(np.sum(H[freqs > thresh] ** 2))

        assert stopband_energy(80.0) < stopband_energy(40.0)

    def test_unknown_method_raises(self):
        with pytest.raises(ValueError, match="Unknown method"):
            design_bank(method="bogus")

    def test_array_protocol(self):
        """np.array(pb) should return the underlying bank array."""
        pb = design_bank(num_phases=8, num_taps=3)
        arr = np.array(pb)
        assert arr.shape == (8, 3)
        assert np.array_equal(arr, pb.bank)

    def test_indexing(self):
        """pb[k] should return sub-filter k."""
        pb = design_bank(num_phases=16, num_taps=5)
        row = pb[0]
        assert row.shape == (5,)
        assert np.array_equal(row, pb.bank[0])


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
        pb = design_bank(num_phases=128, num_taps=9, method="firls")
        assert pb.shape == (128, 9)

    def test_dtype_f32(self):
        pb = design_bank(num_phases=128, num_taps=9, method="firls")
        assert pb.dtype == np.float32

    def test_prototype_symmetric(self):
        pb = design_bank(num_phases=64, num_taps=11, method="firls")
        h = pb.ravel()
        assert np.allclose(h, h[::-1], atol=1e-5)

    def test_metadata_method(self):
        pb = design_bank(num_phases=32, num_taps=7, method="firls")
        assert pb.method == "firls"


# ---------------------------------------------------------------------------
# to_c_header
# ---------------------------------------------------------------------------

class TestToCHeader:
    def test_returns_string(self):
        pb = design_bank(num_phases=4, num_taps=3)
        out = to_c_header(pb, name="test_bank")
        assert isinstance(out, str)

    def test_contains_defines(self):
        pb = design_bank(num_phases=4, num_taps=3)
        out = to_c_header(pb, name="test_bank")
        assert "TEST_BANK_NUM_PHASES 4" in out
        assert "TEST_BANK_NUM_TAPS   3" in out

    def test_contains_array(self):
        pb = design_bank(num_phases=4, num_taps=3)
        out = to_c_header(pb, name="test_bank")
        assert "static const float test_bank[4][3]" in out

    def test_num_rows_match(self):
        pb = design_bank(num_phases=4, num_taps=3)
        out = to_c_header(pb, name="test_bank")
        assert out.count("{ ") == 4

    def test_metadata_in_comment(self):
        pb = design_bank(
            num_phases=4, num_taps=3, attenuation_db=70.0, method="kaiser"
        )
        out = to_c_header(pb, name="test_bank")
        assert "kaiser" in out
        assert "70.0 dB" in out

    def test_writes_file(self, tmp_path):
        pb = design_bank(num_phases=4, num_taps=3)
        p = tmp_path / "bank.h"
        to_c_header(pb, name="bank", path=p)
        assert p.exists()
        assert "static const float bank[4][3]" in p.read_text()


# ---------------------------------------------------------------------------
# to_npy
# ---------------------------------------------------------------------------

class TestToNpy:
    def test_roundtrip(self, tmp_path):
        pb = design_bank(num_phases=16, num_taps=5)
        p = tmp_path / "bank.npy"
        to_npy(pb, p)
        loaded = np.load(str(p))
        assert loaded.shape == (16, 5)
        assert np.allclose(pb.bank, loaded, atol=0)
