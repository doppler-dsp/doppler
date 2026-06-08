"""Integration tests for the Python composer (doppler.wfmgen.compose).

The strong test is **byte-parity against the C ``wavegen`` CLI**: a single-segment
``Composer`` written through ``Writer`` must produce the exact same file the
proven CLI does for the same flags. The rest cover the JSON round-trip, the
Writer↔read_iq round-trip per sample type, segment timing, and the DSP helpers.
"""

import hashlib
import shutil
import subprocess

import numpy as np
import pytest

from doppler.wfmgen.compose import (
    Composer,
    Segment,
    Writer,
    dsss_spread,
    mls_poly,
    rrc_taps,
)
from doppler.wfmgen.readback import read_iq

_WAVEGEN = shutil.which("wavegen")
_needs_cli = pytest.mark.skipif(
    _WAVEGEN is None, reason="wavegen CLI not on PATH"
)


def _md5(path) -> str:
    return hashlib.md5(open(path, "rb").read()).hexdigest()


@_needs_cli
@pytest.mark.parametrize("wtype", ["tone", "noise", "pn", "bpsk", "qpsk"])
@pytest.mark.parametrize("stype", ["cf32", "ci16", "ci8"])
def test_byte_parity_vs_wavegen(tmp_path, wtype, stype):
    """Python Composer+Writer == the wavegen CLI, byte-for-byte (same defaults)."""
    n = 1024
    cli = tmp_path / "cli.iq"
    subprocess.run(
        [
            _WAVEGEN,
            "--type",
            wtype,
            "--fs",
            "1e6",
            "--freq",
            "1e5",
            "--count",
            str(n),
            "--snr",
            "100",
            "--sample_type",
            stype,
            "--endian",
            "le",
            "-o",
            str(cli),
        ],
        check=True,
    )
    # Single-segment composer with the same parameters + the CLI defaults.
    x = Composer(
        type=wtype, fs=1e6, freq=1e5, num_samples=n, snr=100.0
    ).compose()
    py = tmp_path / "py.iq"
    with Writer(py, file_type="raw", sample_type=stype, endian="le") as w:
        w.write(x)
    assert _md5(py) == _md5(cli), f"{wtype}/{stype} diverged from wavegen"


def test_json_roundtrip():
    """to_json() → from_json() reproduces the samples exactly."""
    spec = [
        Segment("pn", num_samples=127, pn_length=7),
        Segment("tone", freq=2e5, num_samples=256, off_samples=64),
        Segment("qpsk", num_samples=200, seed=42),
    ]
    a = Composer(spec, repeat=False)
    b = Composer.from_json(a.to_json())
    assert np.array_equal(a.compose(), b.compose())


@pytest.mark.parametrize("stype", ["cf32", "cf64", "ci32", "ci16", "ci8"])
def test_writer_readback_roundtrip(tmp_path, stype):
    """Writer → read_iq round-trips per sample type (cf* exact, ci* within q-step)."""
    x = Composer(type="tone", freq=1e5, num_samples=1000).compose()
    p = tmp_path / f"cap.{stype}"
    with Writer(p, sample_type=stype) as w:
        w.write(x)
    y = read_iq(str(p), stype)
    assert len(y) == len(x)
    tol = {"cf32": 1e-6, "cf64": 1e-9, "ci32": 1e-6, "ci16": 1e-3, "ci8": 1e-1}
    assert np.max(np.abs(y - x)) < tol[stype]


def test_segment_timing():
    """Composed length = sum of on-time + trailing gaps; off-time is zeros."""
    spec = [
        Segment("noise", num_samples=100, off_samples=50),
        Segment("tone", num_samples=200),
    ]
    x = Composer(spec).compose()
    assert len(x) == 100 + 50 + 200
    assert np.all(x[100:150] == 0)  # the gap


def test_streaming_matches_compose():
    """Block-wise execute() concatenates to the same array as compose()."""
    spec = [Segment("bpsk", num_samples=512, seed=7)]
    whole = Composer(spec).compose()
    c = Composer(spec)
    chunks, blk = [], c.execute(100)
    while len(blk):
        chunks.append(blk)
        blk = c.execute(100)
    assert np.array_equal(np.concatenate(chunks), whole)


def test_continuous_raises_on_compose():
    c = Composer(type="tone", continuous=True)
    assert c.continuous is True
    with pytest.raises(ValueError):
        c.compose()
    assert len(c.execute(256)) == 256  # but streams forever


def test_mls_poly_table():
    """A few known maximal-length polynomials (Galois right-shift convention)."""
    assert mls_poly(7) == 0x41
    assert mls_poly(15) == 0x4001
    assert mls_poly(31) == 0x40000004
    assert mls_poly(64) == 0x800000000000000D
    assert mls_poly(1) == 0  # out of range


def test_rrc_taps():
    t = rrc_taps(0.35, sps=4, span=6)
    assert t.dtype == np.float32 and len(t) == 2 * 6 * 4 + 1
    assert abs(float(t[len(t) // 2]) - float(t.max())) < 1e-6  # peak at centre


def test_dsss_spread():
    syms = np.array([1 + 0j, -1 + 0j], dtype=np.complex64)
    code = np.array([0, 1, 0, 1], dtype=np.uint8)  # +1 -1 +1 -1
    out = dsss_spread(syms, code, 4)
    assert out.shape == (8,)
    assert np.allclose(out[:4], [1, -1, 1, -1])
    assert np.allclose(out[4:], [-1, 1, -1, 1])


def test_context_manager_and_idempotent_close():
    with Composer(type="tone", num_samples=64) as c:
        assert len(c.execute(64)) == 64
    c.close()  # idempotent after __exit__
