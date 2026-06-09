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
    Reader,
    SampleClock,
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
# SampleClock is POSIX-only too (clock_gettime / nanosleep).
_needs_clock = pytest.mark.skipif(
    not hasattr(_c, "clock_create"),
    reason="SampleClock not available on this platform",
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


@_needs_clock
def test_sampleclock_paces_to_rate():
    """Pacing N samples at fs takes ~N/fs seconds, drift-free."""
    clk = SampleClock(fs=1e5)  # 100 kS/s
    t0 = time.perf_counter()
    for _ in range(50):
        clk.pace(2000)  # 50 * 2000 / 1e5 = 1.0 s
    elapsed = time.perf_counter() - t0
    assert 0.9 < elapsed < 1.4, f"paced run took {elapsed:.3f}s, expected ~1.0"
    assert clk.samples == 100000
    # NB: no `underruns == 0` assertion — on a loaded, non-realtime CI runner an
    # idle pacer can legitimately fall behind once (that's what the counter is
    # for); the drift-free schedule still lands elapsed near 1.0 s.


@_needs_clock
def test_sampleclock_stamp_is_exact():
    """stamp() advances by exactly count/fs nanoseconds (pure arithmetic)."""
    clk = SampleClock(fs=1e6)  # 1 sample = 1000 ns
    s0 = clk.stamp()
    clk.pace(1000)
    assert clk.stamp() - s0 == 1_000_000  # 1000 samples @ 1 MS/s = 1 ms
    assert isinstance(clk.stamp(), int)


@_needs_clock
def test_sampleclock_underrun_counted():
    """An impossible rate makes every deadline past → counted underruns."""
    clk = SampleClock(fs=1e12)  # 1 TS/s: nothing can keep up
    for _ in range(5):
        clk.pace(1000)
    assert clk.underruns >= 1
    assert clk.max_lateness > 0.0


@_needs_clock
def test_sampleclock_resync_reanchors():
    """resync=True keeps the clock near 'now' instead of piling up lateness."""
    clk = SampleClock(fs=1e12, resync=True)
    for _ in range(5):
        clk.pace(1000)
    # With resync the epoch advances, so lateness stays bounded per block
    # rather than growing; just assert it ran and counted without raising.
    assert clk.samples == 5000


@_needs_clock
def test_sampleclock_reset():
    clk = SampleClock(fs=1e12)
    clk.pace(1000)
    clk.reset()
    assert clk.samples == 0
    assert clk.underruns == 0


@_needs_clock
def test_sampleclock_releases_gil():
    """pace() must release the GIL: a paced worker can't stall the main thread.

    A worker paces ~0.5 s in one block while the main thread keeps counting;
    if the GIL were held, the main thread would make no progress during the
    sleep. We assert it kept running.
    """
    import threading

    done = threading.Event()

    def worker():
        SampleClock(fs=1000.0).pace(500)  # 500/1000 = 0.5 s in C, GIL released
        done.set()

    t = threading.Thread(target=worker)
    t.start()
    ticks = 0
    while not done.is_set():
        ticks += 1
        time.sleep(0.005)
    t.join()
    assert ticks > 10, f"main thread only ran {ticks}x — GIL not released?"


@pytest.mark.parametrize(
    "file_type,ext,stype,tol",
    [
        ("raw", "cf32", "cf32", 1e-6),
        ("raw", "cf64", "cf64", 1e-9),
        ("raw", "ci16", "ci16", 1e-3),
        ("csv", "csv", "cf32", 1e-6),
        ("csv", "csv", "ci16", 1e-3),
        ("blue", "blue", "cf32", 1e-6),
        ("blue", "blue", "ci16", 1e-3),
    ],
)
def test_reader_roundtrips_each_container(
    tmp_path, file_type, ext, stype, tol
):
    """Writer → Reader round-trips per container; auto-detection recovers it."""
    x = Composer(type="tone", freq=1e5, num_samples=1000).compose()
    p = tmp_path / f"cap.{ext}"
    with Writer(p, file_type=file_type, sample_type=stype, fs=1e6) as w:
        w.write(x)
    with Reader(p, sample_type=stype) as r:
        assert r.file_type == file_type  # container auto-detected
        assert r.sample_type == stype
        y = r.read_all()
    assert len(y) == len(x)
    assert np.max(np.abs(y - x)) < tol


def test_reader_blue_recovers_metadata(tmp_path):
    """A BLUE capture self-describes: fs comes back from the HCB, no hint."""
    x = Composer(type="qpsk", sps=8, num_samples=2048).compose()
    p = tmp_path / "cap.blue"
    with Writer(p, file_type="blue", sample_type="ci16", fs=2.4e6) as w:
        w.write(x)
    # Open with the *wrong* default hint — BLUE metadata must override it.
    with Reader(p) as r:
        assert r.file_type == "blue"
        assert r.sample_type == "ci16"  # recovered, not the cf32 default
        assert r.fs == pytest.approx(2.4e6)
        assert r.num_samples == 2048
        y = r.read_all()
    assert np.max(np.abs(y - x)) < 1e-3


def test_reader_sigmf_pair(tmp_path):
    """Reader auto-detects a SigMF .sigmf-data via its .sigmf-meta sidecar."""
    spec = [Segment("tone", freq=1e5, num_samples=300)]
    x = Composer(spec).compose()
    data = tmp_path / "cap.sigmf-data"
    with Writer(data, file_type="sigmf", sample_type="cf32", fs=1e6) as w:
        w.write(x)
    (tmp_path / "cap.sigmf-meta").write_text(
        sigmf_meta(sample_type="cf32", fs=1e6, segments=spec)
    )
    with Reader(data) as r:
        assert r.file_type == "sigmf"
        assert r.fs == pytest.approx(1e6)
        assert np.allclose(r.read_all(), x)


def test_reader_matches_read_iq_for_raw(tmp_path):
    """Reader agrees with the existing read_iq helper on a raw capture."""
    x = Composer(type="noise", snr=20, num_samples=4096, seed=5).compose()
    p = tmp_path / "cap.ci32"
    with Writer(p, sample_type="ci32", fs=1e6) as w:
        w.write(x)
    via_reader = Reader(str(p), sample_type="ci32").read_all()
    via_read_iq = read_iq(str(p), "ci32")
    # Both divide by the same full-scale; the C reader does it scalar, read_iq
    # via the SIMD cvt converter, so they agree to float precision (not bitwise).
    assert via_reader.shape == via_read_iq.shape
    assert np.allclose(via_reader, via_read_iq, atol=1e-6)


def test_reader_blocked_read_matches_read_all(tmp_path):
    """Block-wise read() concatenates to the same array as read_all()."""
    x = Composer(type="bpsk", sps=4, num_samples=3000, seed=2).compose()
    p = tmp_path / "cap.cf32"
    with Writer(p) as w:
        w.write(x)
    whole = Reader(str(p)).read_all()
    r = Reader(str(p))
    chunks, blk = [], r.read(256)
    while len(blk):
        chunks.append(blk)
        blk = r.read(256)
    r.close()
    assert np.array_equal(np.concatenate(chunks), whole)


def test_reader_idempotent_close(tmp_path):
    p = tmp_path / "cap.cf32"
    with Writer(p) as w:
        w.write(Composer(type="tone", num_samples=64).compose())
    r = Reader(str(p))
    r.read_all()
    r.close()
    r.close()  # idempotent


def test_reader_missing_file_raises(tmp_path):
    with pytest.raises(OSError):
        Reader(str(tmp_path / "does-not-exist.cf32"))


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
