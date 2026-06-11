"""Round-trip tests for read_iq: generate -> read back -> faithful complex64.

Drives the real ``wavegen`` CLI so the on-disk bytes are exactly what users get,
then checks read_iq reconstructs the signal within the wire type's quantization.
"""

import shutil
import subprocess

import numpy as np
import pytest

from doppler.wfm.readback import read_iq

_WAVEGEN = shutil.which("wavegen")
pytestmark = pytest.mark.skipif(
    _WAVEGEN is None, reason="wavegen CLI not on PATH"
)


def _gen(tmp_path, sample_type, endian="le", n=2000):
    out = tmp_path / f"cap_{sample_type}_{endian}.iq"
    subprocess.run(
        [
            _WAVEGEN,
            "--type",
            "tone",
            "--fs",
            "1e6",
            "--freq",
            "1e5",
            "--count",
            str(n),
            "--snr",
            "100",
            "--sample_type",
            sample_type,
            "--endian",
            endian,
            "-o",
            str(out),
        ],
        check=True,
    )
    return str(out)


def test_cf32_is_zero_copy_complex64(tmp_path):
    iq = read_iq(_gen(tmp_path, "cf32"), "cf32")
    assert iq.dtype == np.complex64
    assert np.isclose(np.mean(np.abs(iq) ** 2), 1.0, atol=0.05)  # clean tone


def test_cf64_is_complex128(tmp_path):
    iq = read_iq(_gen(tmp_path, "cf64"), "cf64")
    assert iq.dtype == np.complex128


@pytest.mark.parametrize(
    "sample_type,atol",
    [("ci32", 1e-6), ("ci16", 2 / 32767), ("ci8", 2 / 127)],
)
def test_int_roundtrip_matches_cf32(tmp_path, sample_type, atol):
    truth = read_iq(_gen(tmp_path, "cf32"), "cf32")
    got = read_iq(_gen(tmp_path, sample_type), sample_type)
    assert got.dtype == np.complex64
    assert np.allclose(got, truth, atol=atol)  # within quantization


def test_raw_is_zero_copy_two_column_view(tmp_path):
    raw = read_iq(_gen(tmp_path, "ci16", n=128), "ci16", raw=True)
    assert raw.shape == (128, 2) and raw.dtype == np.int16
    # cf32 raw → the underlying float pairs
    fraw = read_iq(_gen(tmp_path, "cf32", n=128), "cf32", raw=True)
    assert fraw.shape == (128, 2) and fraw.dtype == np.float32


def test_big_endian_roundtrip(tmp_path):
    truth = read_iq(_gen(tmp_path, "cf32"), "cf32")
    be = read_iq(_gen(tmp_path, "ci16", endian="be"), "ci16", endian="be")
    assert np.allclose(be, truth, atol=2 / 32767)


def test_unknown_sample_type_rejected(tmp_path):
    with pytest.raises(ValueError):
        read_iq("nope", "cf99")
