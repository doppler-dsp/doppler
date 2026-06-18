"""Round-trip tests for IqFile: generate -> read back -> faithful complex64.

Drives the real ``wfmgen`` CLI so the on-disk bytes are exactly what users get,
then checks ``IqFile(path, sample_type).read(nsamples)`` reconstructs the signal
within the wire type's quantization (floats exact, ints within one LSB). IqFile
is the C-first replacement for the old hand-Python ``read_iq``.
"""

import os
import pathlib
import shutil
import subprocess

import numpy as np
import pytest

from doppler.wfm import IqFile


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

# bytes per complex sample, per wire type (mirrors wfm_bytes_per_sample).
_BPS = {"cf32": 8, "cf64": 16, "ci32": 8, "ci16": 4, "ci8": 2}


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


def _read_all(path, sample_type, endian="le"):
    r = IqFile(path, sample_type, endian)
    return r.read(r.nsamples)


def test_cf32_is_complex64(tmp_path):
    iq = _read_all(_gen(tmp_path, "cf32"), "cf32")
    assert iq.dtype == np.complex64
    assert np.isclose(np.mean(np.abs(iq) ** 2), 1.0, atol=0.05)  # clean tone


def test_cf64_round_trips(tmp_path):
    truth = _read_all(_gen(tmp_path, "cf32"), "cf32")
    iq = _read_all(_gen(tmp_path, "cf64"), "cf64")
    assert iq.dtype == np.complex64
    assert np.allclose(iq, truth, atol=1e-6)


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
    be = _read_all(_gen(tmp_path, "ci16", endian="be"), "ci16", endian="be")
    assert np.allclose(be, truth, atol=2 / 32767)


@pytest.mark.parametrize(
    "sample_type", ["cf32", "cf64", "ci32", "ci16", "ci8"]
)
def test_nsamples_matches_filesize(tmp_path, sample_type):
    n = 777
    path = _gen(tmp_path, sample_type, n=n)
    r = IqFile(path, sample_type)
    assert r.nsamples == n
    assert r.nsamples == os.path.getsize(path) // _BPS[sample_type]


def test_partial_then_remaining(tmp_path):
    """Two reads concatenate to the whole-file read (position advances)."""
    path = _gen(tmp_path, "cf32", n=512)
    whole = _read_all(path, "cf32")
    r = IqFile(path, "cf32")
    head = r.read(200)
    tail = r.read(r.nsamples - 200)
    assert np.array_equal(np.concatenate([head, tail]), whole)


def test_close_then_read_is_guarded(tmp_path):
    """After close(), reads return a zero-filled array (no stale data)."""
    r = IqFile(_gen(tmp_path, "cf32", n=64), "cf32")
    r.close()
    assert np.all(r.read(64) == 0)
