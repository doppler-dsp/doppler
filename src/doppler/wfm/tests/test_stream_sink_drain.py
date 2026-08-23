"""`StreamSink.drain()` — the durable-completion verb on the Python face.

A send hands a block to the NATS client and returns; the client writes it
in the background. Closing without draining leaves the tail to the
client's own best-effort flush — 500 ms, no failure report, silently
dropped past that. `drain` is how a caller asks the question with a budget
it chose and gets an answer it can act on.

Broker-gated, like the rest of the stream suite: these skip cleanly when
nothing answers on :4222 and run against CI's live broker.
"""

from __future__ import annotations

import socket

import numpy as np
import pytest

from doppler import wfm

ENDPOINT = "nats://127.0.0.1:4222/iq"


def _broker_reachable(host: str = "127.0.0.1", port: int = 4222) -> bool:
    try:
        with socket.create_connection((host, port), timeout=0.5):
            return True
    except OSError:
        return False


pytestmark = pytest.mark.skipif(
    not _broker_reachable(), reason="no NATS broker on 127.0.0.1:4222"
)


def test_drain_after_send_returns_cleanly() -> None:
    """The ordinary path: everything published has reached the server."""
    sink = wfm.StreamSink(ENDPOINT, "cf32")
    try:
        sink.send(np.zeros(1024, dtype=np.complex64), 1e6, 2.4e9)
        sink.drain(2000)
    finally:
        sink.close()


def test_drain_with_nothing_sent_is_fine() -> None:
    """Draining an idle sink is not an error — there is simply no backlog."""
    sink = wfm.StreamSink(ENDPOINT, "cf32")
    try:
        sink.drain(2000)
    finally:
        sink.close()


def test_drain_default_budget() -> None:
    """`timeout_ms` is optional; 0 selects the stream layer's 5 s default."""
    sink = wfm.StreamSink(ENDPOINT, "cf32")
    try:
        sink.send(np.zeros(256, dtype=np.complex64), 1e6, 2.4e9)
        sink.drain()
    finally:
        sink.close()


def test_send_eos_then_drain_is_the_ordered_shutdown() -> None:
    """The order the header prescribes: stop producing, say so, let it land.

    `send_eos()` must precede `drain()` — a drain cannot be reversed and
    refuses sends once it reaches its publish-flushing phase, so an
    end-of-stream issued afterwards may simply not go.
    """
    sink = wfm.StreamSink(ENDPOINT, "cf32")
    try:
        sink.send(np.zeros(256, dtype=np.complex64), 1e6, 2.4e9)
        sink.send_eos()
        sink.drain(2000)
    finally:
        sink.close()


def test_send_eos_with_nothing_sent() -> None:
    """A sender that finishes without sending anything still says so."""
    sink = wfm.StreamSink(ENDPOINT, "cf32")
    try:
        sink.send_eos()
        sink.drain(2000)
    finally:
        sink.close()


def test_close_after_drain_is_just_the_free() -> None:
    """Drain leaves the sink finished; closing next must still work.

    The documented order is drain-then-close, so the pair has to be safe —
    a drain that left the handle unusable would make the advice unusable
    too.
    """
    sink = wfm.StreamSink(ENDPOINT, "cf32")
    sink.send(np.zeros(512, dtype=np.complex64), 1e6, 2.4e9)
    sink.drain(2000)
    sink.close()
