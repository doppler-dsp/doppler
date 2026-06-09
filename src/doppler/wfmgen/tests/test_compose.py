"""Integration tests for the Python composer (doppler.wfmgen.compose).

The strong test is **byte-parity against the C ``wavegen`` CLI**: a single-segment
``Composer`` written through ``Writer`` must produce the exact same file the
proven CLI does for the same flags. The rest cover the JSON round-trip, the
Writer↔read_iq round-trip per sample type, segment timing, and the DSP helpers.
"""

import hashlib
import random
import shutil
import struct
import subprocess
import time

import numpy as np
import pytest

from doppler.wfmgen import _wfmcompose as _c
from doppler.wfmgen.compose import (
    Composer,
    Segment,
    Writer,
    ZmqSink,
    dsss_spread,
    mls_poly,
    rrc_taps,
    sigmf_meta,
    write_blue_header,
)
from doppler.wfmgen.readback import read_iq

_WAVEGEN = shutil.which("wavegen")
_needs_cli = pytest.mark.skipif(
    _WAVEGEN is None, reason="wavegen CLI not on PATH"
)
# ZmqSink is POSIX-only; the C extension omits sink_* off-platform.
_needs_zmq = pytest.mark.skipif(
    not hasattr(_c, "sink_open"),
    reason="ZmqSink not available on this platform",
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


@_needs_zmq
def test_zmqsink_loopback_to_subscriber():
    """ZmqSink publishes the wfmgen fs/fc framing that doppler.stream decodes.

    ZmqSink frames with ``dp_pub_send_*`` (the shared wire format), so a
    ``doppler.stream.Subscriber`` on the same endpoint must recover the samples
    and the fs/fc tags. cf64 is used because the Python recv path decodes
    CF64/CI32/CF128 (cf32 is wire-valid but not Python-decodable).
    """
    from doppler.stream import Subscriber

    ep = f"tcp://127.0.0.1:{random.randint(49152, 65000)}"
    x = Composer(type="tone", freq=1e5, num_samples=512).compose()
    with ZmqSink(ep, sample_type="cf64") as sink, Subscriber(ep) as sub:
        time.sleep(0.15)  # PUB/SUB subscription warm-up
        sink.send(x, fs=1e6, fc=2.4e9)
        samples, hdr = sub.recv(timeout_ms=2000)
    assert samples.dtype == np.complex128 and len(samples) == 512
    assert np.allclose(samples, x.astype(np.complex128), atol=1e-6)
    assert hdr["sample_rate"] == pytest.approx(1e6)
    assert hdr["center_freq"] == pytest.approx(2.4e9)


@_needs_zmq
def test_zmqsink_idempotent_close():
    ep = f"tcp://127.0.0.1:{random.randint(49152, 65000)}"
    sink = ZmqSink(ep)
    sink.close()
    sink.close()  # idempotent


def test_write_blue_header_detached_hcb(tmp_path):
    """write_blue_header lays down a standard detached type-1000 HCB.

    Parses the 512-byte header and checks the fixed fields the C writer emits:
    magic, byte order (EEEI for little-endian), detached flag, data_size
    (total * bytes-per-sample), the 1000 type tag, the complex format code, and
    xdelta = 1/fs.
    """
    p = tmp_path / "cap.hdr"
    write_blue_header(p, sample_type="cf32", fs=1e6, total=512, detached=True)
    h = p.read_bytes()
    assert len(h) == 512
    assert h[0:4] == b"BLUE"
    assert h[4:8] == b"EEEI" and h[8:12] == b"EEEI"  # little-endian
    assert struct.unpack_from("<i", h, 12)[0] == 1  # detached
    assert struct.unpack_from("<d", h, 32)[0] == 0.0  # data_start (detached)
    assert struct.unpack_from("<d", h, 40)[0] == 512 * 8  # cf32 = 8 B/sample
    assert struct.unpack_from("<i", h, 48)[0] == 1000  # type-1000
    assert chr(h[52]) == "C" and chr(h[53]) == "F"  # complex float32
    assert struct.unpack_from("<d", h, 264)[0] == pytest.approx(1e-6)  # xdelta


def test_write_blue_header_big_endian(tmp_path):
    """endian='be' flips the rep tags and byte order of the fields."""
    p = tmp_path / "cap.hdr"
    write_blue_header(
        p, sample_type="ci16", endian="be", fs=2e6, total=100, detached=True
    )
    h = p.read_bytes()
    assert h[4:8] == b"IEEE" and h[8:12] == b"IEEE"
    assert struct.unpack_from(">d", h, 40)[0] == 100 * 4  # ci16 = 4 B/sample
    assert struct.unpack_from(">i", h, 48)[0] == 1000
    assert struct.unpack_from(">d", h, 264)[0] == pytest.approx(1 / 2e6)


def test_sigmf_meta_and_data_pair(tmp_path):
    """sigmf_meta() + Writer(file_type='sigmf') produce a valid SigMF pair.

    The metadata records the global sample rate and one annotation per segment;
    the companion .sigmf-data round-trips back through read_iq.
    """
    import json

    spec = [
        Segment("tone", freq=1e5, num_samples=256),
        Segment("qpsk", sps=8, num_samples=512, seed=3),
    ]
    x = Composer(spec).compose()
    data = tmp_path / "cap.sigmf-data"
    with Writer(data, file_type="sigmf", sample_type="cf32", fs=1e6) as w:
        w.write(x)
    meta = json.loads(sigmf_meta(sample_type="cf32", fs=1e6, segments=spec))
    assert meta["global"]["core:sample_rate"] == 1e6
    assert meta["global"]["core:datatype"] == "cf32_le"
    assert len(meta["annotations"]) == 2  # one per segment
    assert np.allclose(read_iq(str(data), "cf32"), x)
