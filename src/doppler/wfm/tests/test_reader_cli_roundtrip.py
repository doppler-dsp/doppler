"""CLI round-trip tests for wfm.Reader: wfmgen writes -> Reader reads back.

Drives the real ``wfmgen`` CLI so the on-disk bytes are exactly what users get,
then checks :class:`doppler.wfm.Reader` reconstructs the signal within the wire
type's quantization. Reader is the C reader that superseded the old pure-Python
``read_iq`` helper; the conversion from every wire type to unit-scale
``complex64`` happens in C, so cf64 also comes back as ``complex64`` (the old
helper returned a zero-copy ``complex128`` view).
"""

import os
import pathlib
import shutil
import subprocess

import numpy as np
import pytest

from doppler.wfm import Reader


def _wfmgen_bin():
    """The one C CLI: on PATH, else the CMake build tree, else None."""
    p = shutil.which("wfmgen")
    if p:
        return p
    root = pathlib.Path(__file__).resolve().parents[4]
    for cand in root.glob("build*/**/wfmgen"):
        if cand.is_file() and os.access(cand, os.X_OK):
            return str(cand)
    return None


_WFMGEN = _wfmgen_bin()
pytestmark = pytest.mark.skipif(
    _WFMGEN is None, reason="wfmgen CLI not built / on PATH"
)


def _gen(tmp_path, sample_type, endian="le", n=2000):
    out = tmp_path / f"cap_{sample_type}_{endian}.iq"
    subprocess.run(
        [
            _WFMGEN,
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
            "--sample-type",
            sample_type,
            "--endian",
            endian,
            "-o",
            str(out),
        ],
        check=True,
    )
    return str(out)


def _read_all(path, sample_type, endian="le"):
    """Drain a whole raw capture through Reader into one complex64 array."""
    with Reader(path, sample_type=sample_type, endian=endian) as r:
        chunks, blk = [], r.read(65536)
        while len(blk):
            chunks.append(blk)
            blk = r.read(65536)
    return np.concatenate(chunks) if chunks else np.empty(0, np.complex64)


def test_cf32_reads_back_as_complex64(tmp_path):
    iq = _read_all(_gen(tmp_path, "cf32"), "cf32")
    assert iq.dtype == np.complex64
    assert np.isclose(np.mean(np.abs(iq) ** 2), 1.0, atol=0.05)  # clean tone


def test_cf64_reads_back_faithfully(tmp_path):
    # Reader always yields unit-scale complex64, converting the cf64 wire
    # samples in C (the old read_iq returned a zero-copy complex128 view).
    iq = _read_all(_gen(tmp_path, "cf64"), "cf64")
    assert iq.dtype == np.complex64
    assert np.isclose(np.mean(np.abs(iq) ** 2), 1.0, atol=0.05)


@pytest.mark.parametrize(
    "sample_type,atol",
    [("ci32", 1e-6), ("ci16", 2 / 32767), ("ci8", 2 / 127)],
)
def test_int_roundtrip_matches_cf32(tmp_path, sample_type, atol):
    truth = _read_all(_gen(tmp_path, "cf32"), "cf32")
    got = _read_all(_gen(tmp_path, sample_type), sample_type)
    assert got.dtype == np.complex64
    assert np.allclose(got, truth, atol=atol)  # within quantization


def test_big_endian_roundtrip(tmp_path):
    truth = _read_all(_gen(tmp_path, "cf32"), "cf32")
    be = _read_all(_gen(tmp_path, "ci16", endian="be"), "ci16", "be")
    assert np.allclose(be, truth, atol=2 / 32767)


def test_unknown_sample_type_rejected(tmp_path):
    with pytest.raises(ValueError):
        Reader("nope", "cf99")
