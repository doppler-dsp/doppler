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
run, and after MaxAckPending (1000) unacked frames the server sends only
redeliveries, which PullSource concatenated as new data.
"""

import time
from typing import ClassVar

import numpy as np
import pytest

import doppler.stream
from doppler.specan.source import PullSource, SocketSource
from doppler.stream import CF32, Push
from doppler.tests._nats import nats_available

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
    """

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
        return out, dict(hdr)

    def ack(self, samples):
        entry = self._pending.pop(id(samples), None)
        if entry is None or entry[0] is not samples:
            raise ValueError("ack: not an un-freed recv() result")
        self.acked.append(entry[1][1]["sequence"])


def _sequenced(n):
    """n one-sample CI8 frames, sample k = k, header sequence k."""
    return [
        (np.array([k % 128, 0], np.int8), {**HDR, "sequence": k})
        for k in range(n)
    ]


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
    """#2009 against a real broker: more frames than MaxAckPending (1000).

    Without acks the server stops handing out NEW frames at 1000 pending
    and, after AckWait, sends only redeliveries: the read either times out
    short or returns repeats. Each frame carries its index as its one
    sample, so every frame must arrive exactly once, in order.
    """
    n = 1200
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
        push.close()
    assert (fs, cf) == (FS, FC)
    assert got.real.astype(int).tolist() == list(range(n))
