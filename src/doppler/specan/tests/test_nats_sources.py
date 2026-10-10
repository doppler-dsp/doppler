"""specan's NATS sources, through a stand-in transport (no broker needed).

The socket source could not show a single frame: it imported `Subscriber`
from `doppler` (it lives in `doppler.stream`), read the header as an object
(`recv` returns a dict) and cast interleaved integer I/Q straight to complex.
Its only tests drove the demo source, so none of that was caught. These
drive both NATS sources the way the bindings do -- flat interleaved integer
arrays, dict headers -- and check what comes out.

The stand-ins keep the REAL contract (#2009). The first fake returned
`(None, None)` on a timeout, while the bindings raise `TimeoutError`, and it
had no `ack`: so the dead `data is None` branches looked live, and a
PullSource that never acked looked fine. A Pull consumer is explicit-ack:
an unacked frame comes back after AckWait and stays queued for the next
run, and after `WORK_QUEUE_MAX_ACK_PENDING` unacked frames the server
sends only redeliveries, which PullSource concatenated as new data.
"""

import time
from typing import ClassVar

import numpy as np
import pytest

import doppler.stream
from doppler.specan.source import PullSource, SocketSource
from doppler.stream import CF32, WORK_QUEUE_MAX_ACK_PENDING, Push
from doppler.tests._nats import delete_stream_if_present, nats_available

FS, FC = 2.4e6, 100e6
HDR = {"sample_rate": FS, "center_freq": FC}


class _FakeTransport:
    """Hands out queued (data, header) frames; raises TimeoutError when
    there are none, as the bindings do (`stream_ext.c`)."""

    frames: ClassVar[list] = []  # each Fake subclass sets its own

    def __init__(self, address):
        self.address = address
        self._q = list(type(self).frames)

    def recv(self, timeout_ms=None):
        if not self._q:
            raise TimeoutError("recv timed out")
        item = self._q.pop(0)
        if isinstance(item, BaseException):
            raise item  # what the bindings do on an end-of-stream frame
        return item

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False

    def close(self):
        pass


class _FakePull(_FakeTransport):
    """A work-queue consumer with Pull's ack contract.

    Each delivery hands out a FRESH array (the binding's zero-copy view),
    pending until `ack` gets that very array. When the fresh queue runs dry,
    every still-pending frame is delivered again, as AckWait would, so a
    reader that does not ack sees its frames twice. `seen` and `acked` are
    the sequence numbers delivered and acked, in order.

    A frame delivered more than `MAX_DELIVERIES` times fails the test. The
    broker spaces redeliveries an AckWait apart; this fake has no clock, so
    a reader that never acks a frame would spin on it forever, and a hang
    is not a red.
    """

    MAX_DELIVERIES: ClassVar[int] = 3

    def __init__(self, address):
        super().__init__(address)
        self._pending = {}  # id(array) -> (array, (data, header))
        self.seen = []
        self.acked = []

    def recv(self, timeout_ms=None):
        if not self._q and self._pending:  # AckWait elapsed
            self._q = [item for _, item in self._pending.values()]
            self._pending.clear()
        item = super().recv(timeout_ms)
        data, hdr = item
        out = data.copy()
        self._pending[id(out)] = (out, item)
        self.seen.append(hdr["sequence"])
        if self.seen.count(hdr["sequence"]) > self.MAX_DELIVERIES:
            raise AssertionError(
                f"frame {hdr['sequence']} delivered "
                f"{self.MAX_DELIVERIES + 1} times: never acked"
            )
        return out, dict(hdr)

    def ack(self, samples):
        entry = self._pending.pop(id(samples), None)
        if entry is None or entry[0] is not samples:
            raise ValueError("ack: not an un-freed recv() result")
        self.acked.append(entry[1][1]["sequence"])


def _sequenced(n):
    """n one-sample CI8 frames, sample k = k, header sequence k."""
    return _with_sequences(range(n))


def _with_sequences(seqs):
    """One one-sample CI8 frame per header sequence, its sample the
    sequence mod 128. An exception in `seqs` is queued as it is."""
    return [
        k
        if isinstance(k, BaseException)
        else (np.array([k % 128, 0], np.int8), {**HDR, "sequence": k})
        for k in seqs
    ]


def _taken(iq):
    """The sequences (mod 128) of the one-sample frames read."""
    return (iq.real * 128).round().astype(int).tolist()


def _frames():
    a = np.array([64, -128, 0, 127, -64, 32], np.int8)  # 3 samples
    b = np.array([16384, -32768, 0, 8192], np.int16)  # 2 samples
    want = np.concatenate(
        [
            (a[0::2] + 1j * a[1::2]) / 128.0,
            (b[0::2] + 1j * b[1::2]) / 32768.0,
        ]
    ).astype(np.complex64)
    return [
        (a, {**HDR, "sequence": 0}),
        (b, {**HDR, "sequence": 1}),
    ], want


SOURCES = [
    (SocketSource, "Subscriber", _FakeTransport),
    (PullSource, "Pull", _FakePull),
]


@pytest.mark.parametrize("source_cls, attr, base", SOURCES)
def test_a_nats_source_decodes_integer_frames(
    monkeypatch, source_cls, attr, base
):
    frames, want = _frames()
    fake = type("Fake", (base,), {"frames": frames})
    monkeypatch.setattr(doppler.stream, attr, fake)

    src = source_cls("nats://127.0.0.1:1/test", timeout_ms=10)
    got, fs, cf = src.read(len(want))

    assert got.dtype == np.complex64
    np.testing.assert_allclose(got, want, rtol=1e-6)
    assert (fs, cf) == (FS, FC)  # read from the dict header


@pytest.mark.parametrize("source_cls, attr, base", SOURCES)
def test_end_of_stream_returns_the_buffered_samples(
    monkeypatch, source_cls, attr, base
):
    # recv raises EOFError on the end-of-stream frame every publisher sends
    # on exit (uno_q_pub always does); the source must hand back what it
    # holds, like a timeout, not raise into the DSP loop.
    frames, want = _frames()
    fake = type("Fake", (base,), {"frames": [*frames, EOFError("eos")]})
    monkeypatch.setattr(doppler.stream, attr, fake)

    src = source_cls("nats://127.0.0.1:1/test", timeout_ms=10)
    got, fs, cf = src.read(len(want) + 16)  # more than will ever arrive

    np.testing.assert_allclose(got, want, rtol=1e-6)
    assert (fs, cf) == (FS, FC)


@pytest.mark.parametrize("source_cls, attr, base", SOURCES)
def test_a_timeout_returns_the_buffered_samples(
    monkeypatch, source_cls, attr, base
):
    """The bindings RAISE TimeoutError; a source must not let it out."""
    frames, want = _frames()
    monkeypatch.setattr(
        doppler.stream, attr, type("Fake", (base,), {"frames": frames})
    )
    src = source_cls("nats://127.0.0.1:1/test", timeout_ms=10)
    got, _, _ = src.read(len(want) + 16)  # more than will ever arrive
    np.testing.assert_allclose(got, want, rtol=1e-6)


def test_pull_source_acks_every_frame_exactly_once(monkeypatch):
    """#2009: each frame is acked once it is consumed, so none comes back.

    Asking for more than will ever arrive drains the queue to a timeout:
    a PullSource that does not ack sees every frame again (the fake's
    AckWait), and reads them as new samples.
    """
    n = 50
    monkeypatch.setattr(
        doppler.stream,
        "Pull",
        type("Fake", (_FakePull,), {"frames": _sequenced(n)}),
    )
    src = PullSource("nats://127.0.0.1:1/test", timeout_ms=10)
    got, _, _ = src.read(n + 16)
    pull = src._pull
    assert pull.seen == list(range(n)), "a frame was delivered twice"
    assert pull.acked == list(range(n)), "a frame was not acked exactly once"
    assert len(got) == n  # and no redelivery was read as new data


@pytest.mark.skipif(
    not nats_available(), reason="no NATS broker on 127.0.0.1:4222"
)
def test_pull_source_reads_past_max_ack_pending_without_a_repeat():
    """#2009 against a real broker: more frames than MaxAckPending.

    Without acks the server stops handing out NEW frames once
    `WORK_QUEUE_MAX_ACK_PENDING` are pending and, after AckWait, sends
    only redeliveries: the read either times out short or returns
    repeats. Each frame carries its index as its one sample, so every
    frame must arrive exactly once, in order.
    """
    n = WORK_QUEUE_MAX_ACK_PENDING + 200
    endpoint = f"nats://127.0.0.1:4222/specan-pull-{int(time.time() * 1e6)}"
    push = Push(endpoint, CF32)  # provisions the work queue
    try:
        for k in range(n):
            push.send(np.array([k], np.complex64), FS, FC)
        src = PullSource(endpoint, timeout_ms=3000)
        try:
            got, fs, cf = src.read(n)
        finally:
            src.close()
    finally:
        delete_stream_if_present(push)  # a work queue outlives the test
        push.close()
    assert (fs, cf) == (FS, FC)
    assert got.real.astype(int).tolist() == list(range(n))


def _pull_fake(monkeypatch, frames):
    monkeypatch.setattr(
        doppler.stream, "Pull", type("Fake", (_FakePull,), {"frames": frames})
    )
    return PullSource("nats://127.0.0.1:1/test", timeout_ms=10)


def test_an_undecodable_frame_is_acked_once_and_skipped(monkeypatch):
    """Unacked, a frame no decoder reads came back every AckWait forever
    and stayed in the queue for the next run."""
    bad = (np.array([1, 2], np.uint16), {**HDR, "sequence": 0})  # no decoder
    good = (np.array([64, 0], np.int8), {**HDR, "sequence": 1})
    src = _pull_fake(monkeypatch, [bad, good])
    got, _, _ = src.read(16)
    assert len(got) == 1
    assert src.undecodable == 1
    assert src._pull.acked == [0, 1]
    assert src._pull.seen == [0, 1]  # never redelivered


def test_a_repeated_sequence_is_taken_once(monkeypatch):
    """An ack is fire-and-forget; a lost one means a redelivery, which must
    not be read as new samples. It is still acked: it has been handled."""
    frames = _sequenced(3)
    frames.insert(2, frames[1])  # 0, 1, 1 again, 2
    src = _pull_fake(monkeypatch, frames)
    got, _, _ = src.read(16)
    assert _taken(got) == [0, 1, 2]
    assert src.duplicates == 1
    assert src._pull.acked == [0, 1, 1, 2]


def test_a_producer_after_an_end_of_stream_counts_from_zero(monkeypatch):
    """A new sender's sequence starts again at 0. Read as redeliveries of
    the old one's 5..7, its frames were acked away, and the display froze
    until it passed the old high-water mark."""
    src = _pull_fake(
        monkeypatch, _with_sequences([5, 6, 7, EOFError("eos"), 0, 1, 2])
    )
    first, _, _ = src.read(16)  # up to the end-of-stream
    second, _, _ = src.read(16)  # specan keeps reading
    assert _taken(first) == [5, 6, 7]
    assert _taken(second) == [0, 1, 2]
    assert src.duplicates == 0


def test_a_backwards_jump_past_max_ack_pending_is_a_new_producer(
    monkeypatch,
):
    """No end-of-stream: the queue's backlog from a previous run ends at a
    high sequence, and the new run starts at 0. A redelivery can only be
    so far behind; a bigger jump is a restart, and its frames are taken."""
    src = _pull_fake(monkeypatch, _with_sequences([10**6, 0, 1, 2]))
    got, _, _ = src.read(16)
    assert _taken(got) == [10**6 % 128, 0, 1, 2]
    assert src.duplicates == 0
    assert src.restarts == 1


def test_a_skipped_sequence_is_counted(monkeypatch):
    frames = [f for f in _sequenced(4) if f[1]["sequence"] != 2]
    src = _pull_fake(monkeypatch, frames)
    src.read(16)
    assert src.gaps == 1


def test_a_timeout_before_the_first_frame_is_not_an_error(monkeypatch):
    """The sink starts before the producer in `doppler compose`: read()
    times out empty with rate 0.0, and process() must wait, not fail."""
    from doppler.specan.config import SpecanConfig
    from doppler.specan.engine import SpecanEngine

    src = _pull_fake(monkeypatch, [])
    iq, fs, cf = src.read(4096)
    assert iq.size == 0 and (fs, cf) == (0.0, 0.0)
    engine = SpecanEngine(SpecanConfig())
    try:
        assert engine.process(iq, fs, cf) is None
    finally:
        engine.close()


def _engine():
    from doppler.specan.config import SpecanConfig
    from doppler.specan.engine import SpecanEngine

    return SpecanEngine(SpecanConfig())


def test_samples_without_a_rate_are_an_error_not_a_wait():
    """Push.send defaults sample_rate to 0. Such a stream used to sit on
    "Waiting for signal..." for ever, acking every frame away."""
    engine = _engine()
    try:
        with pytest.raises(ValueError, match=r"sample rate of 0\.0 Hz"):
            engine.process(np.ones(4096, np.complex64), 0.0, 0.0)
    finally:
        engine.close()


def test_an_empty_block_after_a_chain_leaves_it_working():
    """A timeout mid-stream is an empty block too. Rebuilding the chain for
    its 0 Hz would tear the working one down."""
    x = (np.random.default_rng(0).standard_normal(8192) * 0.1).astype(
        np.complex64
    )
    engine = _engine()
    try:
        assert engine.process(x, FS, FC) is not None
        assert engine.process(np.empty(0, np.complex64), 0.0, 0.0) is None
        assert engine.process(x, FS, FC) is not None  # still the same chain
    finally:
        engine.close()
