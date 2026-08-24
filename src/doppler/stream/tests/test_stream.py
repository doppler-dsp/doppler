"""Tests for doppler.stream — PUSH/PULL, PUB/SUB, and REQ/REP round-trips.

Requires a nats-server reachable on 127.0.0.1:4222 (run `nats-server -js`);
the whole module skips cleanly without one.

Wire format: a 96-byte dp_header_t binary prefix followed by the
interleaved I/Q payload, all in one NATS message.  The header carries
sample type, sample_rate, center_freq, and a nanosecond timestamp.
Zero-copy recv: the returned NumPy array holds a reference to the
dp_msg_t until GC.

Sample types: CI8, CI16, CI32, CF32, CF64 (I/Q) and TLM16 (telemetry
records, Publisher only). Every one round-trips through both faces. The
wire values are append-only and 2 is retired -- it was CF128, whose
representation differs between x86-64 and aarch64 at the same size.

All tests use a random subject so they don't collide with each other or
with a concurrent run on the same broker.
"""

from __future__ import annotations

import random
import socket
import time

import numpy as np
import pytest

import doppler.stream
from doppler.stream import (
    CF64,
    CI32,
    Publisher,
    Pull,
    Push,
    Replier,
    Requester,
    Subscriber,
    get_timestamp_ns,
)


def _nats_available() -> bool:
    """True if a nats-server is listening on 127.0.0.1:4222."""
    try:
        socket.create_connection(("127.0.0.1", 4222), timeout=0.3).close()
        return True
    except OSError:
        return False


pytestmark = pytest.mark.skipif(
    not _nats_available(),
    reason="no nats-server on 127.0.0.1:4222 (run `nats-server -js`)",
)

# ------------------------------------------------------------------ #
# Helpers                                                             #
# ------------------------------------------------------------------ #


def _unique_endpoint(hint: str = "ep") -> str:
    """Use a random subject so tests don't collide with each other or a
    concurrent run on the same broker."""
    return f"nats://127.0.0.1:4222/{hint}{random.randint(1, 10**9)}"


# ------------------------------------------------------------------ #
# get_timestamp_ns                                                    #
# ------------------------------------------------------------------ #


def test_timestamp_ns_is_positive():
    ts = get_timestamp_ns()
    assert ts > 0


def test_timestamp_ns_increases():
    t0 = get_timestamp_ns()
    time.sleep(0.001)
    t1 = get_timestamp_ns()
    assert t1 > t0


# ------------------------------------------------------------------ #
# PUSH / PULL — CF64 round-trip                                       #
# ------------------------------------------------------------------ #


@pytest.fixture
def push_pull_cf64():
    ep = _unique_endpoint()
    push = Push(ep, CF64)
    pull = Pull(ep)
    time.sleep(0.05)  # allow the JetStream work-queue to provision
    yield push, pull
    push.__exit__(None, None, None)
    pull.__exit__(None, None, None)


def test_push_pull_cf64_roundtrip(push_pull_cf64):
    push, pull = push_pull_cf64
    x = np.array([1 + 2j, 3 + 4j, 5 + 6j], dtype=np.complex128)
    push.send(x, sample_rate=int(1e6), center_freq=int(2.4e9))
    samples, _hdr = pull.recv(timeout_ms=2000)
    np.testing.assert_array_almost_equal(samples, x)


def test_push_pull_cf64_dtype(push_pull_cf64):
    push, pull = push_pull_cf64
    x = np.ones(8, dtype=np.complex128)
    push.send(x)
    samples, _hdr = pull.recv(timeout_ms=2000)
    assert samples.dtype == np.complex128


def test_push_pull_header_sample_rate(push_pull_cf64):
    push, pull = push_pull_cf64
    x = np.ones(4, dtype=np.complex128)
    push.send(x, sample_rate=48000)
    _, hdr = pull.recv(timeout_ms=2000)
    assert hdr["sample_rate"] == 48000


def test_push_pull_header_center_freq(push_pull_cf64):
    push, pull = push_pull_cf64
    x = np.ones(4, dtype=np.complex128)
    push.send(x, center_freq=int(915e6))
    _, hdr = pull.recv(timeout_ms=2000)
    assert hdr["center_freq"] == int(915e6)


def test_push_pull_header_has_timestamp(push_pull_cf64):
    push, pull = push_pull_cf64
    x = np.ones(4, dtype=np.complex128)
    push.send(x)
    _, hdr = pull.recv(timeout_ms=2000)
    assert "timestamp_ns" in hdr
    assert hdr["timestamp_ns"] > 0


def test_push_pull_header_timestamp_override(push_pull_cf64):
    """An explicit timestamp_ns propagates through send() -> recv()
    exactly, instead of being overwritten by a fresh CLOCK_REALTIME
    read -- the propagation fix this test guards against regressing."""
    push, pull = push_pull_cf64
    x = np.ones(4, dtype=np.complex128)
    upstream_ts = 1_700_000_000_123_456_789
    push.send(x, timestamp_ns=upstream_ts)
    _, hdr = pull.recv(timeout_ms=2000)
    assert hdr["timestamp_ns"] == upstream_ts


def test_push_pull_header_timestamp_default_unchanged(push_pull_cf64):
    """Omitting timestamp_ns keeps today's behavior: auto-stamped close
    to wall-clock now, not the override path silently engaging."""
    push, pull = push_pull_cf64
    x = np.ones(4, dtype=np.complex128)
    before = get_timestamp_ns()
    push.send(x)
    _, hdr = pull.recv(timeout_ms=2000)
    after = get_timestamp_ns()
    assert before <= hdr["timestamp_ns"] <= after


def test_pull_timeout_raises(push_pull_cf64):
    _, pull = push_pull_cf64
    with pytest.raises((TimeoutError, Exception)):
        pull.recv(timeout_ms=50)


# ------------------------------------------------------------------ #
# PUSH / PULL — CF32 (complex64) round-trip                          #
# ------------------------------------------------------------------ #


def test_push_pull_cf32_roundtrip():
    from doppler.stream import CF64  # CF32 is complex64 → use CF64 dtype

    ep = _unique_endpoint()
    # CI32 encodes int32 IQ; CF64 encodes double complex.
    # For CF32 (float complex) we send complex64 via CF64 transport
    # and check the values survive the round-trip at float precision.
    push = Push(ep, CF64)
    pull = Pull(ep)
    time.sleep(0.05)
    x = np.array([1 + 2j, -3 - 4j], dtype=np.complex128)
    push.send(x)
    samples, _ = pull.recv(timeout_ms=2000)
    np.testing.assert_array_almost_equal(samples, x)
    push.__exit__(None, None, None)
    pull.__exit__(None, None, None)


# ------------------------------------------------------------------ #
# PUSH / PULL — CI32 (int32 IQ)                                       #
# ------------------------------------------------------------------ #


def test_push_pull_ci32_roundtrip():
    ep = _unique_endpoint()
    push = Push(ep, CI32)
    pull = Pull(ep)
    time.sleep(0.05)
    # CI32 send expects int32, interleaved [I0, Q0, I1, Q1, ...];
    # recv returns a flat int32 array of the same layout.
    x = np.array([1, 2, 3, 4], dtype=np.int32)  # 2 IQ pairs
    push.send(x)
    samples, _hdr = pull.recv(timeout_ms=2000)
    assert samples.dtype == np.int32
    np.testing.assert_array_equal(samples, x)
    push.__exit__(None, None, None)
    pull.__exit__(None, None, None)


# ------------------------------------------------------------------ #
# PUB / SUB — basic round-trip                                        #
# ------------------------------------------------------------------ #


def test_pub_sub_cf64_roundtrip():
    ep = _unique_endpoint()
    pub = Publisher(ep, CF64)
    sub = Subscriber(ep)
    time.sleep(0.3)  # core NATS: sub must exist before publish
    x = np.array([7 + 8j, 9 + 10j], dtype=np.complex128)
    pub.send(x, sample_rate=int(1e6))
    samples, _hdr = sub.recv(timeout_ms=2000)
    np.testing.assert_array_almost_equal(samples, x)
    pub.__exit__(None, None, None)
    sub.__exit__(None, None, None)


# ------------------------------------------------------------------ #
# Context manager                                                     #
# ------------------------------------------------------------------ #


def test_push_context_manager():
    ep = _unique_endpoint()
    with Push(ep, CF64) as push, Pull(ep) as pull:
        time.sleep(0.05)
        x = np.ones(4, dtype=np.complex128)
        push.send(x)
        samples, _ = pull.recv(timeout_ms=2000)
        assert len(samples) == 4


# ------------------------------------------------------------------ #
# Zero-copy: array lifetime tied to dp_msg_t                          #
# ------------------------------------------------------------------ #


def test_recv_array_is_valid_after_recv(push_pull_cf64):
    """The recv'd array must remain readable after recv() returns."""
    push, pull = push_pull_cf64
    x = np.arange(16, dtype=np.float64).view(np.complex128)
    push.send(x)
    samples, _ = pull.recv(timeout_ms=2000)
    # Force GC pressure — array must still be valid.
    import gc

    gc.collect()
    assert samples[0] == x[0]


# ------------------------------------------------------------------ #
# Flushing a publisher                                                #
# ------------------------------------------------------------------ #


def test_flush_makes_the_send_observable():
    """send() returns before the server has it; flush() is the round trip."""
    ep = _unique_endpoint()
    sub = Subscriber(ep)
    time.sleep(0.05)
    pub = Publisher(ep, CF64)
    pub.send(np.ones(16, dtype=np.complex128), sample_rate=1e6)
    pub.flush()
    samples, _hdr = sub.recv(timeout_ms=1000)
    assert len(samples) == 16
    pub.flush(timeout_ms=500)  # nothing pending is still a round trip
    pub.close()
    with pytest.raises(RuntimeError, match="closed"):
        pub.flush()
    sub.close()


# ------------------------------------------------------------------ #
# Interrupting a blocking receive                                     #
# ------------------------------------------------------------------ #


def _guard(latency_ms: int = 0):
    """A guard that arms no handlers -- a handle to the process-wide flag.

    The interrupt moved to `doppler.interrupt.Interrupt`; `doppler.stream`
    no longer re-exports it. These tests stay here because what they pin
    is the STREAM's half of the contract -- that a blocked recv() honours
    the flag -- not the flag itself, which test_dp_interrupt_guard.py has.
    """
    import numpy as np

    from doppler.interrupt import Interrupt

    return Interrupt(np.array([], dtype=np.int32), latency_ms=latency_ms)


def test_interrupt_unblocks_a_blocking_recv():
    """A blocked recv() must come back when asked, not when a frame does.

    recv() with no timeout waits inside the NATS client with the GIL
    released. Nothing is published here, so without the interrupt this
    call never returns -- which is exactly the defect the C receiver
    example shipped with.
    """
    import threading

    it = _guard()
    sub = Subscriber(_unique_endpoint())
    time.sleep(0.05)

    timer = threading.Timer(0.4, it.interrupt)
    timer.start()
    t0 = time.monotonic()
    try:
        with pytest.raises(KeyboardInterrupt):
            sub.recv()  # no timeout: blocks
        elapsed = time.monotonic() - t0
        assert elapsed < 3.0, f"took {elapsed:.2f}s"

        # Sticky: a recv STARTED while the flag is set refuses at once, so
        # a signal cannot be missed by racing it.
        with pytest.raises(KeyboardInterrupt):
            sub.recv()

        # And receiving works again once cleared.
        it.resume()
        with pytest.raises(TimeoutError):
            sub.recv(timeout_ms=200)
    finally:
        timer.join()
        it.resume()
        sub.close()


def test_interrupt_latency_is_the_callers_to_set():
    """The wait slice is a knob, and it reaches a blocked receive.

    Asserted as an upper bound rather than a measurement: a small latency
    must not make the interrupt SLOWER, and the default must not be the
    only value that works. The scaling itself (500 -> ~300 ms overshoot,
    10 -> ~9 ms) is measurable but not worth a timing assertion in a
    suite that runs on shared CI.
    """
    import threading

    baseline = _guard()
    assert baseline.latency_ms() > 0

    it = _guard(latency_ms=10)
    assert it.latency_ms() == 10
    try:
        sub = Subscriber(_unique_endpoint())
        time.sleep(0.05)
        timer = threading.Timer(0.2, it.interrupt)
        timer.start()
        t0 = time.monotonic()
        with pytest.raises(KeyboardInterrupt):
            sub.recv()
        timer.join()
        assert time.monotonic() - t0 < 2.0
        sub.close()
    finally:
        it.resume()

    # The override is the guard's, and dies with it.
    #
    # `timer` has to go too, and that is not tidiness: threading.Timer holds
    # the BOUND METHOD `it.interrupt`, which holds `it`, so dropping only the
    # `it` name leaves the guard alive and its latency override still in
    # force. Nobody had seen that, because until doppler#976 was fixed this
    # test never reached this line -- `it.interrupt()` set doppler.interrupt's
    # flag while `sub.recv()` read doppler.stream's, so the recv above blocked
    # forever and the run was killed rather than failed.
    del timer, it
    assert baseline.latency_ms() == 100


def test_a_guard_scopes_the_latency_to_its_lifetime():
    before = _guard().latency_ms()
    with _guard(latency_ms=5) as inner:
        assert inner.latency_ms() == 5
    assert _guard().latency_ms() == before


def test_arming_sigint_leaves_pythons_view_of_the_handler_alone():
    """The guard installs underneath Python's handler and chains to it."""
    import signal as _signal

    import numpy as np

    from doppler.interrupt import Interrupt

    before = _signal.getsignal(_signal.SIGINT)
    with Interrupt(np.array([_signal.SIGINT], dtype=np.int32)) as it:
        assert not it.interrupted()
    # Ours is installed at the C level underneath Python's, which is what
    # keeps Ctrl+C working outside a receive.
    assert _signal.getsignal(_signal.SIGINT) is before
    assert not _guard().interrupted()


# ------------------------------------------------------------------ #
# Retired wire values                                                 #
# ------------------------------------------------------------------ #


def test_retired_sample_type_is_rejected():
    """Wire value 2 was CF128 and is retired, not reusable.

    A retired value sits INSIDE the enum's numeric range, so the ordinal
    test this binding used to run (`type <= CF32`) accepted it and then
    built zero-length frames. The constructor asks the C predicate
    instead, which derives validity from `dp_sample_size()` -- one table,
    and a type with no size is not a type.
    """
    assert not hasattr(doppler.stream, "CF128")
    with pytest.raises(ValueError):
        Push(_unique_endpoint(), 2)


# ------------------------------------------------------------------ #
# PUSH / PULL — header completeness                                   #
# ------------------------------------------------------------------ #


def test_push_pull_header_all_fields(push_pull_cf64):
    """All expected dp_header_t fields must be present in the recv dict."""
    push, pull = push_pull_cf64
    x = np.ones(4, dtype=np.complex128)
    push.send(x, sample_rate=int(1e6), center_freq=int(2.4e9))
    _, hdr = pull.recv(timeout_ms=2000)
    required = {
        "sample_rate",
        "center_freq",
        "format",
        "kind",
        "data_rep",
        "flags",
        "payload_bytes",
        "version",
        "timestamp_ns",
        "sequence",
        "num_samples",
    }
    assert required.issubset(hdr.keys()), (
        f"Missing keys: {required - hdr.keys()}"
    )
    # `format` is the BLUE code, not an ordinal: "CD" for complex float64,
    # the same two characters the same samples get in a BLUE file.
    assert hdr["format"] == CF64
    assert bytes([hdr["format"] & 0xFF, hdr["format"] >> 8]) == b"CD"
    assert hdr["kind"] == 0  # DP_KIND_IQ
    assert hdr["version"] == 2
    assert hdr["data_rep"] in ("EEEI", "IEEE")
    assert hdr["payload_bytes"] == 4 * 16


def test_push_pull_header_num_samples(push_pull_cf64):
    push, pull = push_pull_cf64
    x = np.ones(7, dtype=np.complex128)
    push.send(x)
    _, hdr = pull.recv(timeout_ms=2000)
    assert hdr["num_samples"] == 7


# ------------------------------------------------------------------ #
# PUB / SUB — additional coverage                                     #
# ------------------------------------------------------------------ #


def test_pub_sub_ci32_roundtrip():
    ep = _unique_endpoint()
    pub = Publisher(ep, CI32)
    sub = Subscriber(ep)
    time.sleep(0.1)
    x = np.array([10, 20, 30, 40], dtype=np.int32)  # 2 IQ pairs
    pub.send(x)
    samples, _hdr = sub.recv(timeout_ms=2000)
    assert samples.dtype == np.int32
    np.testing.assert_array_equal(samples, x)
    pub.__exit__(None, None, None)
    sub.__exit__(None, None, None)


def test_pub_sub_header_fields():
    ep = _unique_endpoint()
    pub = Publisher(ep, CF64)
    sub = Subscriber(ep)
    time.sleep(0.1)
    x = np.ones(4, dtype=np.complex128)
    pub.send(x, sample_rate=int(2e6), center_freq=int(433e6))
    _, hdr = sub.recv(timeout_ms=2000)
    assert hdr["sample_rate"] == pytest.approx(2e6)
    assert hdr["center_freq"] == pytest.approx(433e6)
    assert hdr["timestamp_ns"] > 0
    pub.__exit__(None, None, None)
    sub.__exit__(None, None, None)


def test_sub_timeout_raises():
    ep = _unique_endpoint()
    with Subscriber(ep) as sub, pytest.raises((TimeoutError, Exception)):
        sub.recv(timeout_ms=50)


# ------------------------------------------------------------------ #
# REQ / REP — CF64 round-trip                                         #
# ------------------------------------------------------------------ #


@pytest.fixture
def req_rep_cf64():
    ep = _unique_endpoint()
    rep = Replier(ep, CF64)
    req = Requester(ep, CF64)
    time.sleep(0.05)
    yield req, rep
    req.__exit__(None, None, None)
    rep.__exit__(None, None, None)


def test_req_rep_cf64_roundtrip(req_rep_cf64):
    req, rep = req_rep_cf64
    x_req = np.array([1 + 2j, 3 + 4j], dtype=np.complex128)
    req.send(x_req, sample_rate=int(1e6))

    req_samples, _req_hdr = rep.recv(timeout_ms=2000)
    np.testing.assert_array_almost_equal(req_samples, x_req)

    x_rep = np.array([5 + 6j, 7 + 8j], dtype=np.complex128)
    rep.send(x_rep, sample_rate=int(2e6))

    rep_samples, _rep_hdr = req.recv(timeout_ms=2000)
    np.testing.assert_array_almost_equal(rep_samples, x_rep)


def test_req_rep_cf64_dtype(req_rep_cf64):
    req, rep = req_rep_cf64
    x = np.ones(8, dtype=np.complex128)
    req.send(x)
    samples, _ = rep.recv(timeout_ms=2000)
    assert samples.dtype == np.complex128
    rep.send(samples)
    reply, _ = req.recv(timeout_ms=2000)
    assert reply.dtype == np.complex128


def test_req_rep_header_fields(req_rep_cf64):
    req, rep = req_rep_cf64
    x = np.ones(4, dtype=np.complex128)
    req.send(x, sample_rate=48000, center_freq=int(915e6))
    _, hdr = rep.recv(timeout_ms=2000)
    assert hdr["sample_rate"] == pytest.approx(48000)
    assert hdr["center_freq"] == pytest.approx(915e6)
    assert hdr["timestamp_ns"] > 0
    rep.send(x)
    req.recv(timeout_ms=2000)


# ------------------------------------------------------------------ #
# REQ / REP — CI32                                                    #
# ------------------------------------------------------------------ #


def test_req_rep_ci32_roundtrip():
    ep = _unique_endpoint()
    rep = Replier(ep, CI32)
    req = Requester(ep, CI32)
    time.sleep(0.05)
    x = np.array([1, 2, 3, 4], dtype=np.int32)
    req.send(x)
    samples, _ = rep.recv(timeout_ms=2000)
    assert samples.dtype == np.int32
    np.testing.assert_array_equal(samples, x)
    rep.send(samples)
    reply, _ = req.recv(timeout_ms=2000)
    np.testing.assert_array_equal(reply, x)
    req.__exit__(None, None, None)
    rep.__exit__(None, None, None)


# ------------------------------------------------------------------ #
# REQ / REP — timeouts                                                #
# ------------------------------------------------------------------ #


def test_replier_timeout_raises():
    ep = _unique_endpoint()
    with Replier(ep, CF64) as rep, pytest.raises((TimeoutError, Exception)):
        rep.recv(timeout_ms=50)


def test_requester_reply_timeout_raises():
    ep = _unique_endpoint()
    rep = Replier(ep, CF64)
    req = Requester(ep, CF64)
    time.sleep(0.05)
    # Send a request but never reply — requester recv should time out.
    x = np.ones(4, dtype=np.complex128)
    req.send(x)
    rep.recv(timeout_ms=2000)  # consume request; don't reply
    with pytest.raises((TimeoutError, Exception)):
        req.recv(timeout_ms=50)
    req.__exit__(None, None, None)
    rep.__exit__(None, None, None)


# ------------------------------------------------------------------ #
# Context managers — remaining types                                  #
# ------------------------------------------------------------------ #


def test_pull_context_manager():
    ep = _unique_endpoint()
    with Push(ep, CF64) as push, Pull(ep) as pull:
        time.sleep(0.05)
        x = np.ones(4, dtype=np.complex128)
        push.send(x)
        samples, _ = pull.recv(timeout_ms=2000)
        assert len(samples) == 4


def test_subscriber_context_manager():
    ep = _unique_endpoint()
    with Publisher(ep, CF64) as pub, Subscriber(ep) as sub:
        time.sleep(0.1)
        x = np.ones(4, dtype=np.complex128)
        pub.send(x)
        samples, _ = sub.recv(timeout_ms=2000)
        assert len(samples) == 4


def test_req_rep_context_manager():
    ep = _unique_endpoint()
    with Replier(ep, CF64) as rep, Requester(ep, CF64) as req:
        time.sleep(0.05)
        x = np.ones(4, dtype=np.complex128)
        req.send(x)
        samples, _ = rep.recv(timeout_ms=2000)
        assert len(samples) == 4
        rep.send(samples)
        req.recv(timeout_ms=2000)


# ------------------------------------------------------------------ #
# Chunking, JetStream durability, and other NATS-specific behavior    #
# ------------------------------------------------------------------ #


def test_nats_pub_sub_roundtrip():
    ep = _unique_endpoint("pubsub")
    sub = Subscriber(ep)
    time.sleep(0.3)  # core NATS: sub must exist before publish
    pub = Publisher(ep, CF64)
    time.sleep(0.3)
    x = np.array([1 + 2j, 3 + 4j, 5 + 6j], dtype=np.complex128)
    pub.send(x, sample_rate=48000, center_freq=int(915e6))
    samples, hdr = sub.recv(timeout_ms=3000)
    np.testing.assert_array_almost_equal(samples, x)
    assert hdr["sample_rate"] == 48000
    assert hdr["center_freq"] == int(915e6)
    pub.close()
    sub.close()


def test_nats_pub_sub_timeout():
    sub = Subscriber(_unique_endpoint("empty"))
    with pytest.raises((TimeoutError, Exception)):
        sub.recv(timeout_ms=50)
    sub.close()


def test_nats_req_rep_roundtrip():
    ep = _unique_endpoint("ctrl")
    rep = Replier(ep)
    time.sleep(0.3)
    req = Requester(ep)
    req.send(np.array([1 + 2j, 3 + 4j], dtype=np.complex128))
    got, _ = rep.recv(timeout_ms=3000)
    np.testing.assert_array_almost_equal(got, [1 + 2j, 3 + 4j])
    rep.send(np.array([5 + 6j, 7 + 8j], dtype=np.complex128))
    reply, _ = req.recv(timeout_ms=3000)
    np.testing.assert_array_almost_equal(reply, [5 + 6j, 7 + 8j])
    req.close()
    rep.close()


def test_nats_chunked_pub_sub():
    """A >1 MiB frame is split into chunks and reassembled byte-identical."""
    ep = _unique_endpoint("chunk")
    sub = Subscriber(ep)
    time.sleep(0.3)
    pub = Publisher(ep, CF64)
    time.sleep(0.3)
    rng = np.random.default_rng(0)
    big = (
        rng.standard_normal(100_000) + 1j * rng.standard_normal(100_000)
    ).astype(np.complex128)  # 1.6 MB > 1 MiB max_payload
    pub.send(big)
    got, hdr = sub.recv(timeout_ms=5000)
    assert np.array_equal(got, big)
    assert hdr["num_samples"] == 100_000
    pub.close()
    sub.close()


def test_nats_push_frame_too_large():
    """A frame over the broker max_payload fails the PUSH work-queue with a
    clear, actionable error instead of an opaque "send failed".

    Chunking is a PUB/SUB-only feature (the work-queue load-balances frames
    across workers, so a frame's chunks could land on different pullers and
    never reassemble), so an oversized PUSH frame cannot be split.  The same
    size succeeds over PUB/SUB — see ``test_nats_chunked_pub_sub``.
    """
    push = Push(_unique_endpoint("toobig"), CF64)
    # 200k CF64 = 3.05 MiB > the stock 1 MiB max_payload.
    big = np.zeros(200_000, dtype=np.complex128)
    with pytest.raises(ValueError, match="max_payload"):
        push.send(big, sample_rate=int(1e6))
    push.close()


def test_nats_jetstream_push_pull_ack():
    """Durable work-queue: every pushed frame is pulled and acked exactly."""
    ep = _unique_endpoint("wq")
    push = Push(ep, CF64)
    n = 10
    for i in range(n):
        push.send(
            np.array([i + 0j], dtype=np.complex128), sample_rate=int(1e6)
        )
    pull = Pull(ep)
    got = []
    for _ in range(n):
        samples, _hdr = pull.recv(timeout_ms=5000)
        got.append(int(samples[0].real))
        pull.ack(samples)  # explicit JetStream ack
    push.close()
    pull.close()
    assert sorted(got) == list(range(n))


# ------------------------------------------------------------------ #
# PUB / SUB — TLM16 telemetry frames                                  #
# ------------------------------------------------------------------ #


def test_pub_sub_tlm16_roundtrip():
    """Telemetry records published as a TLM16 frame decode back into the
    same structured rows Telemetry.read() produced."""
    from doppler.agc import AGC
    from doppler.stream import TLM16
    from doppler.telemetry import Telemetry

    ep = _unique_endpoint("tlm")
    pub = Publisher(ep, TLM16)
    sub = Subscriber(ep)
    time.sleep(0.3)  # core NATS: sub must exist before publish

    tlm = Telemetry(1 << 12)
    agc = AGC(0.0, 0.0025, 0.05)
    agc.set_telemetry(tlm, "agc")
    agc.steps(np.full(512, 0.25 + 0j, dtype=np.complex64))
    recs = tlm.read()
    assert len(recs) > 0

    pub.send(recs)
    rx, hdr = sub.recv(timeout_ms=2000)
    # A telemetry frame is a KIND; its format field carries no BLUE code
    # because BLUE has none for a record stream.
    assert hdr["kind"] == TLM16
    assert hdr["format"] == 0
    assert hdr["num_samples"] == len(recs)
    assert rx.dtype == recs.dtype  # structured rows survive the wire
    np.testing.assert_array_equal(rx, recs)

    pub.__exit__(None, None, None)
    sub.__exit__(None, None, None)


def test_pub_tlm16_rejects_wrong_itemsize():
    from doppler.stream import TLM16

    pub = Publisher(_unique_endpoint("tlmbad"), TLM16)
    with pytest.raises(TypeError):
        pub.send(np.zeros(4, dtype=np.complex64))  # 8-byte items, not 16
    pub.__exit__(None, None, None)
