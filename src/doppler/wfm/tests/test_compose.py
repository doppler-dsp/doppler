"""Integration tests for the Python composer (doppler.wfm.compose).

The strong test is **byte-parity against the C ``wfmgen`` CLI**: a single-segment
``Composer`` written through ``Writer`` must produce the exact same file the
one C CLI does for the same flags. The rest cover the JSON round-trip, the
Writer↔read_iq round-trip per sample type, segment timing, and the DSP helpers.
"""

import hashlib
import os
import pathlib
import random
import shutil
import struct
import subprocess
import time

import numpy as np
import pytest

from doppler.wfm import _wfmcompose as _c
from doppler.wfm import mls_poly, rrc_taps, dsss_spread
from doppler.wfm.compose import (
    Composer,
    Reader,
    SampleClock,
    Segment,
    Timeline,
    Writer,
    ZmqSink,
    noise,
    qpsk,
    sigmf_meta,
    tone,
    write_blue_header,
)
from doppler.wfm.readback import read_iq


def _read_all(r):
    """Drain a Reader into one array via blocked read() (the generated handle
    exposes read(n); read_all was an old hand-class convenience)."""
    chunks, blk = [], r.read(65536)
    while len(blk):
        chunks.append(blk)
        blk = r.read(65536)
    return np.concatenate(chunks) if chunks else np.empty(0, np.complex64)


def _wfmgen_bin():
    """The one C CLI: on PATH (wheel console-shim or installed), else the
    CMake build tree, else None (parity tests skip)."""
    p = shutil.which("wfmgen")
    if p:
        return p
    root = pathlib.Path(__file__).resolve().parents[4]
    for cand in root.glob("build*/**/wfmgen"):
        if cand.is_file() and os.access(cand, os.X_OK):
            return str(cand)
    return None


_WFMGEN = _wfmgen_bin()
_needs_wfmgen = pytest.mark.skipif(
    _WFMGEN is None, reason="wfmgen CLI not built / on PATH"
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


@_needs_wfmgen
@pytest.mark.parametrize("wtype", ["tone", "noise", "pn", "bpsk", "qpsk"])
@pytest.mark.parametrize("stype", ["cf32", "ci16", "ci8"])
def test_byte_parity_vs_wfmgen(tmp_path, wtype, stype):
    """Python Composer+Writer == the wfmgen CLI single-segment run, byte-for-
    byte (same defaults). A 1-segment wfmgen run is the old single-shot path."""
    n = 1024
    cli = tmp_path / "cli.iq"
    subprocess.run(
        [
            _WFMGEN,
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
    assert _md5(py) == _md5(cli), f"{wtype}/{stype} diverged from wfmgen"


@_needs_wfmgen
def test_chirp_byte_parity_vs_wfmgen(tmp_path):
    """A chirp segment is byte-identical between the Composer and the CLI; the
    sweep span = the segment's num_samples = --count."""
    n = 4096
    cli = tmp_path / "cli.iq"
    subprocess.run(
        [
            _WFMGEN,
            "--type",
            "chirp",
            "--fs",
            "1e6",
            "--freq",
            "1e5",
            "--f_end",
            "3e5",
            "--count",
            str(n),
            "--sample_type",
            "cf32",
            "-o",
            str(cli),
        ],
        check=True,
    )
    x = Composer(
        Segment("chirp", fs=1e6, freq=1e5, f_end=3e5, num_samples=n)
    ).compose()
    py = tmp_path / "py.iq"
    with Writer(py, file_type="raw", sample_type="cf32") as w:
        w.write(x)
    assert _md5(py) == _md5(cli)


def test_chirp_json_roundtrip():
    """f_end survives the JSON spec round-trip (and only chirp carries it)."""
    a = Composer([Segment("chirp", freq=1e5, f_end=4e5, num_samples=1000)])
    js = a.to_json()
    assert '"f_end"' in js
    b = Composer.from_json(js)
    assert np.array_equal(a.compose(), b.compose())
    assert b.segments[0].f_end == 4e5
    # a plain tone spec never grows an f_end key (back-compat / byte-stable)
    assert '"f_end"' not in Composer([Segment("tone")]).to_json()


def test_chirp_in_timeline_and_sum():
    """A chirp composes both in time (.add) and summed (.sum) with other srcs."""
    from doppler.wfm import chirp, tone

    tl = Segment("chirp", freq=1e5, f_end=2e5, num_samples=1000).add(
        Segment("tone", freq=0, num_samples=500)
    )
    assert len(Composer(tl).compose()) == 1500
    mix = Segment.sum(
        chirp(f_start=1e5, f_end=2e5),
        tone(freq=-2e5, level=-6),
        num_samples=2048,
    )
    assert len(Composer(mix).compose()) == 2048


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
        sink.send(x, 1e6, 2.4e9)
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


def test_stream_yields_blocks_matching_compose():
    """stream() concatenates to the same array as compose()."""
    spec = [Segment("qpsk", sps=8, num_samples=2000, seed=3)]
    whole = Composer(spec).compose()
    streamed = np.concatenate(list(Composer(spec).stream(256)))
    assert np.array_equal(streamed, whole)


def test_stream_block_sizes():
    """Every block is `block` long except a possibly-short final one."""
    c = Composer(type="tone", num_samples=1000)
    blocks = list(c.stream(256))
    assert [len(b) for b in blocks] == [256, 256, 256, 232]  # 3*256 + 232


def test_stream_continuous_is_infinite():
    """A continuous spec streams forever — take a few blocks and stop."""
    import itertools

    c = Composer(type="tone", continuous=True)
    blocks = list(itertools.islice(c.stream(512), 5))
    assert len(blocks) == 5 and all(len(b) == 512 for b in blocks)


@_needs_clock
def test_stream_realtime_paces():
    """stream(block, realtime=fs) paces blocks in C at fs (~ N/fs total)."""
    # 100 blocks of 1000 @ 1e5 = 1.0 s; paced at segments[0].fs.
    c = Composer(type="tone", fs=1e5, num_samples=100_000)
    t0 = time.perf_counter()
    n = sum(len(b) for b in c.stream(1000, realtime=c.segments[0].fs))
    elapsed = time.perf_counter() - t0
    assert n == 100_000
    assert 0.9 < elapsed < 1.4, (
        f"paced stream took {elapsed:.3f}s, expected ~1.0"
    )


@_needs_clock
def test_stream_realtime_float_rate_overrides():
    """A faster realtime rate drains quickly."""
    c = Composer(type="tone", fs=1e5, num_samples=10_000)
    t0 = time.perf_counter()
    list(c.stream(1000, realtime=1e7))  # 10 MS/s → ~1 ms total
    assert time.perf_counter() - t0 < 0.3


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
        y = _read_all(r)
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
        y = _read_all(r)
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
        assert np.allclose(_read_all(r), x)


def test_reader_matches_read_iq_for_raw(tmp_path):
    """Reader agrees with the existing read_iq helper on a raw capture."""
    x = Composer(type="noise", snr=20, num_samples=4096, seed=5).compose()
    p = tmp_path / "cap.ci32"
    with Writer(p, sample_type="ci32", fs=1e6) as w:
        w.write(x)
    via_reader = _read_all(Reader(str(p), sample_type="ci32"))
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
    whole = _read_all(Reader(str(p)))
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
    _read_all(r)
    r.close()
    r.close()  # idempotent


def test_reader_missing_file_raises(tmp_path):
    # The generated handle raises RuntimeError when the C open returns NULL.
    with pytest.raises((OSError, RuntimeError)):
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


def test_writer_clip_detection(tmp_path):
    """Writer tracks the peak (always) and the clipped fraction (opt-in).

    The generated handle reads stats live from the open writer, so read them
    before close(); after close() any property access raises (handle freed).
    """
    # peak magnitude 2.0 (clips in ci16); 2 of 4 components exceed full-scale.
    x = np.array([1.5 + 0.5j, -0.5 - 2.0j], dtype=np.complex64)
    w = Writer(tmp_path / "clip.ci16", sample_type="ci16")
    w.track_clipping(True)
    w.write(x)
    assert w.clipped
    assert abs(w.peak_dbfs - 20.0 * np.log10(2.0)) < 1e-4
    assert abs(w.clip_fraction - 0.5) < 1e-6
    w.close()
    with pytest.raises(RuntimeError):
        _ = w.clipped  # the handle is freed; live stats are no longer readable

    # clean at full scale: peak 0 dBFS, no clip.
    c = Writer(tmp_path / "clean.ci16", sample_type="ci16")
    c.write(np.array([1.0 + 1.0j, -1.0 - 1.0j], dtype=np.complex64))
    assert not c.clipped and abs(c.peak_dbfs) < 1e-4
    c.close()

    # float never clips, even past full scale.
    f = Writer(tmp_path / "x.cf32", sample_type="cf32")
    f.write(x)
    assert not f.clipped
    f.close()


def test_writer_headroom(tmp_path):
    """``headroom`` backs the signal off by a common gain: 0 dB is a bit-exact
    no-op, 6.02 dB halves it, and enough headroom clears clipping."""
    x = np.array([1.0 + 0j, 0.6 - 0.8j], dtype=np.complex64)

    # 6.0206 dB → gain 0.5
    with Writer(
        tmp_path / "hr.cf32", sample_type="cf32", headroom=6.0206
    ) as w:
        w.write(x)
    assert np.allclose(read_iq(str(tmp_path / "hr.cf32"), "cf32"), x * 0.5)

    # 0 dB (default) is verbatim
    with Writer(tmp_path / "h0.cf32", sample_type="cf32") as w:
        w.write(x)
    assert np.allclose(read_iq(str(tmp_path / "h0.cf32"), "cf32"), x)

    # enough headroom clears a clip that would saturate at unity gain
    with Writer(tmp_path / "c.ci16", sample_type="ci16", headroom=12.0) as w:
        w.track_clipping(True)
        w.write(np.array([1.5 + 0j, -2.0 + 0.3j], dtype=np.complex64))
        assert not w.clipped


def test_segment_level():
    """Per-segment ``level`` (dBFS) scales the segment by 10^(level/20):
    -6.02 dB halves it, 0 dB is a no-op, it's SNR-invariant, JSON carries it."""
    base = Composer([Segment("tone", num_samples=256)]).compose()
    half = Composer(
        [Segment("tone", num_samples=256, level=-6.020599913)]
    ).compose()
    assert np.allclose(half, base * 0.5)

    # level=0 is identical to omitting it
    a = Composer([Segment("qpsk", num_samples=512, seed=5)]).compose()
    b = Composer(
        [Segment("qpsk", num_samples=512, seed=5, level=0.0)]
    ).compose()
    assert np.array_equal(a, b)

    # SNR-invariant: noise power scales with level**2, the ratio is preserved
    n0 = Composer([Segment("noise", num_samples=50000, seed=2)]).compose()
    n6 = Composer(
        [Segment("noise", num_samples=50000, seed=2, level=-6.0206)]
    ).compose()
    assert np.isclose(np.var(n6) / np.var(n0), 0.25, rtol=2e-2)

    # JSON round-trip carries level (and omits it at 0)
    spec = [Segment("qpsk", num_samples=256, level=-10.0)]
    js = Composer(spec).to_json()
    assert '"level"' in js
    assert np.array_equal(
        Composer.from_json(js).compose(), Composer(spec).compose()
    )


# ── Phase 4b: .sum() multi-source segments + noise resolution ─────────────────
# (_WFMGEN / _needs_wfmgen are defined at the top of the module.)


def test_sum_compose_basic():
    """Segment.sum mixes sources over one span; the C resolver adds the floor."""
    seg = Segment.sum(
        qpsk(snr=15, snr_mode="esno"),
        tone(freq=2e5, level=-12),
        num_samples=4096,
    )
    assert len(seg.sources) == 2
    x = Composer([seg]).compose()
    assert x.dtype == np.complex64 and len(x) == 4096


def test_sum_json_roundtrip():
    """A summed segment serialises as a "sum" array and round-trips exactly."""
    import json

    seg = Segment.sum(
        qpsk(snr=12, snr_mode="esno", seed=3),
        tone(freq=1.5e5, level=-10),
        num_samples=8192,
    )
    js = Composer([seg]).to_json()
    s0 = json.loads(js)["segments"][0]
    assert "sum" in s0 and "type" not in s0  # nested, not inline
    # resolver made the floor explicit: qpsk (cleaned) + tone + noise
    assert [src["type"] for src in s0["sum"]] == ["qpsk", "tone", "noise"]
    assert np.array_equal(
        Composer.from_json(js).compose(), Composer([seg]).compose()
    )


def test_sum_one_source_equals_bare():
    """A 1-source sum is the bundled single-source path, bit-for-bit.

    The resolver is a no-op at one source, so Segment.sum(qpsk(snr=15)) must be
    byte-identical to the plain Composer(qpsk, snr=15) — the bundled AWGN that
    cannot be split is preserved through the nested-tuple path.
    """
    a = Composer(
        [Segment.sum(qpsk(snr=15, seed=9), num_samples=4096)]
    ).compose()
    b = Composer(type="qpsk", snr=15.0, seed=9, num_samples=4096).compose()
    assert np.array_equal(a, b)


def test_sum_floor_power():
    """The resolved noise floor reproduces the anchor's snr → measured SNR.

    A DC tone (freq 0) is a constant signal, so |mean|**2 is the signal power
    and var() is the noise power; their ratio is the SNR the anchor asked for.
    """
    seg = Segment.sum(
        tone(freq=0.0, snr=15.0, snr_mode="fs"),  # anchor → floor at -15 dBFS
        tone(freq=3e5, level=-120),  # negligible 2nd source (forces a sum)
        num_samples=200_000,
    )
    x = Composer([seg]).compose().astype(np.complex128)
    snr_db = 10 * np.log10(abs(x.mean()) ** 2 / x.var())
    assert abs(snr_db - 15.0) < 0.5


def test_sum_explicit_noise_floor():
    """noise(level=N) sets the floor directly at N dBFS (no anchor needed)."""
    seg = Segment.sum(
        tone(freq=0.0, level=-120),  # negligible signal
        noise(level=-13.0),  # explicit floor
        num_samples=200_000,
    )
    x = Composer([seg]).compose().astype(np.complex128)
    assert np.isclose(x.var(), 10 ** (-13.0 / 10), rtol=5e-2)


@_needs_wfmgen
def test_sum_cli_parity(tmp_path):
    """wfmgen --from-file == the Python composer for a summed spec, byte-exact."""
    seg = Segment.sum(
        qpsk(snr=12, snr_mode="esno", seed=3),
        tone(freq=1.5e5, level=-10),
        num_samples=8192,
    )
    spec = tmp_path / "sum.json"
    spec.write_text(Composer([seg]).to_json())
    cli = tmp_path / "cli.cf32"
    subprocess.run(
        [
            _WFMGEN,
            "--from-file",
            str(spec),
            "--sample_type",
            "cf32",
            "--output",
            str(cli),
        ],
        check=True,
    )
    cli_iq = np.fromfile(cli, dtype=np.complex64)
    assert np.array_equal(cli_iq, Composer([seg]).compose())


def test_sum_reject_overspecified():
    """A non-anchor source giving both snr and level is a spec error."""
    seg = Segment.sum(
        qpsk(snr=10),  # anchor
        tone(snr=5, level=-3),  # over-specified: snr AND level
        num_samples=4096,
    )
    with pytest.raises(Exception):
        Composer([seg]).compose()


def test_sum_needs_a_source():
    """Segment.sum with no sources is rejected up front."""
    with pytest.raises(ValueError):
        Segment.sum(num_samples=1024)


# ── Phase 5: .add() timeline ergonomics ──────────────────────────────────────


def test_add_builds_timeline_equal_to_list():
    """seg.add(other) composes identically to the explicit segment list."""
    a = Segment("tone", freq=1e5, num_samples=1000, off_samples=500)
    b = Segment.sum(qpsk(snr=15), tone(level=-12), num_samples=4096)
    tl = a.add(b)
    assert isinstance(tl, Timeline) and len(tl) == 2
    assert np.array_equal(Composer(tl).compose(), Composer([a, b]).compose())


def test_timeline_add_chains():
    """Timeline.add appends and is chainable; order is preserved."""
    a = Segment("tone", freq=1e5, num_samples=500)
    b = Segment("pn", num_samples=127, pn_length=7)
    c = Segment("qpsk", num_samples=200, seed=4)
    tl = a.add(b).add(c)
    assert [s for s in tl] == [a, b, c] and tl[1] is b
    assert np.array_equal(
        Composer(tl).compose(), Composer([a, b, c]).compose()
    )


def test_composer_accepts_lone_segment_and_timeline():
    """Composer takes a bare Segment or a Timeline, not just a list."""
    seg = Segment("tone", freq=1e5, num_samples=256)
    assert np.array_equal(Composer(seg).compose(), Composer([seg]).compose())
    tl = Timeline([seg])
    assert np.array_equal(Composer(tl).compose(), Composer([seg]).compose())


def test_timeline_json_roundtrip():
    """A timeline round-trips through JSON like any segment list."""
    tl = Segment("tone", freq=1e5, num_samples=300).add(
        Segment.sum(qpsk(snr=12), tone(level=-9), num_samples=512)
    )
    js = Composer(tl).to_json()
    assert np.array_equal(
        Composer.from_json(js).compose(), Composer(tl).compose()
    )


@_needs_wfmgen
def test_bits_byte_parity_vs_wfmgen(tmp_path):
    """A bits segment is byte-identical between the Composer and the CLI."""

    n = 64
    cli = tmp_path / "cli.cf32"
    subprocess.run(
        [
            _WFMGEN,
            "--type",
            "bits",
            "--bits",
            "10110100",
            "--modulation",
            "qpsk",
            "--sps",
            "4",
            "--fs",
            "1e6",
            "--count",
            str(n),
            "--sample_type",
            "cf32",
            "-o",
            str(cli),
        ],
        check=True,
    )
    x = Composer(
        Segment(
            "bits",
            pattern="10110100",
            modulation="qpsk",
            sps=4,
            fs=1e6,
            num_samples=n,
        )
    ).compose()
    py = tmp_path / "py.cf32"
    with Writer(py, file_type="raw", sample_type="cf32") as w:
        w.write(x)
    assert _md5(py) == _md5(cli)


def test_bits_json_roundtrip():
    """pattern + modulation survive the JSON spec round-trip; non-bits don't
    grow the keys (byte-stable)."""

    a = Composer(
        [
            Segment(
                "bits",
                pattern="110100",
                modulation="bpsk",
                sps=2,
                num_samples=12,
            )
        ]
    )
    js = a.to_json()
    assert '"pattern"' in js and '"modulation"' in js
    b = Composer.from_json(js)
    assert np.array_equal(a.compose(), b.compose())
    assert '"pattern"' not in Composer([Segment("tone")]).to_json()


def test_bits_in_sum_scene():
    """A bits source mixes in a .sum() scene with another waveform."""
    from doppler.wfm import bits, tone

    mix = Segment.sum(
        bits(pattern="10110101", modulation="bpsk", sps=4),
        tone(freq=2e5, level=-6),
        num_samples=128,
    )
    assert len(Composer(mix).compose()) == 128


@_needs_wfmgen
def test_rrc_byte_parity_vs_wfmgen(tmp_path):
    """An RRC-shaped segment is byte-identical between the Composer and CLI."""
    from doppler.wfm import qpsk  # noqa: F401

    n = 4096
    cli = tmp_path / "cli.cf32"
    subprocess.run(
        [
            _WFMGEN,
            "--type",
            "qpsk",
            "--sps",
            "8",
            "--snr",
            "100",
            "--seed",
            "3",
            "--pulse",
            "rrc",
            "--rrc-beta",
            "0.22",
            "--rrc-span",
            "8",
            "--fs",
            "1e6",
            "--count",
            str(n),
            "--sample_type",
            "cf32",
            "-o",
            str(cli),
        ],
        check=True,
    )
    x = Composer(
        Segment(
            "qpsk",
            sps=8,
            snr=100,
            seed=3,
            pulse="rrc",
            rrc_beta=0.22,
            rrc_span=8,
            fs=1e6,
            num_samples=n,
        )
    ).compose()
    py = tmp_path / "py.cf32"
    with Writer(py, file_type="raw", sample_type="cf32") as w:
        w.write(x)
    assert _md5(py) == _md5(cli)


def test_rrc_json_roundtrip():
    """pulse/rrc_beta/rrc_span survive JSON; a rect spec never grows the keys."""
    a = Composer(
        [
            Segment(
                "qpsk",
                sps=8,
                seed=3,
                pulse="rrc",
                rrc_beta=0.3,
                rrc_span=6,
                num_samples=2048,
            )
        ]
    )
    js = a.to_json()
    assert '"pulse"' in js and '"rrc_beta"' in js
    b = Composer.from_json(js)
    assert np.array_equal(a.compose(), b.compose())
    assert b.segments[0].pulse == "rrc"
    assert (
        '"pulse"' not in Composer([Segment("qpsk", num_samples=64)]).to_json()
    )
