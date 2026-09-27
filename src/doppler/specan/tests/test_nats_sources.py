"""specan's NATS sources, through a stand-in transport (no broker needed).

The socket source could not show a single frame: it imported `Subscriber`
from `doppler` (it lives in `doppler.stream`), read the header as an object
(`recv` returns a dict) and cast interleaved integer I/Q straight to complex.
Its only tests drove the demo source, so none of that was caught. These
drive both NATS sources the way the bindings do -- flat interleaved integer
arrays, dict headers -- and check what comes out.
"""

from typing import ClassVar

import numpy as np
import pytest

import doppler.stream
from doppler.specan.source import PullSource, SocketSource

FS, FC = 2.4e6, 100e6
HDR = {"sample_rate": FS, "center_freq": FC}


class _FakeTransport:
    """Hands out queued (data, header) frames, then (None, None) = timeout."""

    frames: ClassVar[list] = []  # each Fake subclass sets its own

    def __init__(self, address):
        self.address = address
        self._q = list(type(self).frames)

    def recv(self, timeout_ms=None):
        if not self._q:
            return (None, None)
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


def _frames():
    a = np.array([64, -128, 0, 127, -64, 32], np.int8)  # 3 samples
    b = np.array([16384, -32768, 0, 8192], np.int16)  # 2 samples
    want = np.concatenate(
        [
            (a[0::2] + 1j * a[1::2]) / 128.0,
            (b[0::2] + 1j * b[1::2]) / 32768.0,
        ]
    ).astype(np.complex64)
    return [(a, dict(HDR)), (b, dict(HDR))], want


@pytest.mark.parametrize(
    "source_cls, attr", [(SocketSource, "Subscriber"), (PullSource, "Pull")]
)
def test_a_nats_source_decodes_integer_frames(monkeypatch, source_cls, attr):
    frames, want = _frames()
    fake = type("Fake", (_FakeTransport,), {"frames": frames})
    monkeypatch.setattr(doppler.stream, attr, fake)

    src = source_cls("nats://127.0.0.1:1/test", timeout_ms=10)
    got, fs, cf = src.read(len(want))

    assert got.dtype == np.complex64
    np.testing.assert_allclose(got, want, rtol=1e-6)
    assert (fs, cf) == (FS, FC)  # read from the dict header


@pytest.mark.parametrize(
    "source_cls, attr", [(SocketSource, "Subscriber"), (PullSource, "Pull")]
)
def test_end_of_stream_returns_the_buffered_samples(
    monkeypatch, source_cls, attr
):
    # recv raises EOFError on the end-of-stream frame every publisher sends
    # on exit (uno_q_pub always does); the source must hand back what it
    # holds, like a timeout, not raise into the DSP loop.
    frames, want = _frames()
    fake = type(
        "Fake", (_FakeTransport,), {"frames": [*frames, EOFError("eos")]}
    )
    monkeypatch.setattr(doppler.stream, attr, fake)

    src = source_cls("nats://127.0.0.1:1/test", timeout_ms=10)
    got, fs, cf = src.read(len(want) + 16)  # more than will ever arrive

    np.testing.assert_allclose(got, want, rtol=1e-6)
    assert (fs, cf) == (FS, FC)
