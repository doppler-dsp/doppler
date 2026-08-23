"""End of stream: a subscriber learns the sender finished.

Before this existed, "no frame arrived" meant either "the sender is idle"
or "the sender is gone", and nothing could tell them apart — the same
ambiguity the ring buffer and the file reader have, in a third costume.
See `docs/design/io-termination.md`.

The vocabulary is deliberately Python's own: `EOFError`, not a bespoke
exception and not a `RuntimeError`, because "the producer finished" is an
ordinary end of a loop rather than a failure.
"""

from __future__ import annotations

import socket
import time

import numpy as np
import pytest

from doppler.stream import CF32, Publisher, Subscriber


def _broker_reachable(host: str = "127.0.0.1", port: int = 4222) -> bool:
    try:
        with socket.create_connection((host, port), timeout=0.5):
            return True
    except OSError:
        return False


pytestmark = pytest.mark.skipif(
    not _broker_reachable(), reason="no NATS broker on 127.0.0.1:4222"
)

#: Long enough that a missing frame is a real absence, not a slow broker.
RECV_MS = 3000


def _pair(subject: str) -> tuple[Subscriber, Publisher]:
    sub = Subscriber(f"nats://127.0.0.1:4222/{subject}")
    pub = Publisher(f"nats://127.0.0.1:4222/{subject}", CF32)
    time.sleep(0.3)  # let the subscription reach the broker
    return sub, pub


def test_recv_raises_eof_after_send_eos() -> None:
    """The marker arrives after real data, and ends the stream."""
    sub, pub = _pair("eos-basic")
    try:
        pub.send(np.zeros(8, dtype=np.complex64), 1e6, 1e9)
        pub.send_eos()

        sub.recv(timeout_ms=RECV_MS)  # the data frame

        with pytest.raises(EOFError):
            sub.recv(timeout_ms=RECV_MS)
    finally:
        pub.close()
        sub.close()


def test_eof_is_not_a_timeout() -> None:
    """ "Finished" and "nothing yet" must not be the same answer.

    This is the whole point of the marker: a timeout means only that
    nothing has arrived *so far*, which is exactly what a consumer cannot
    act on.
    """
    sub, pub = _pair("eos-vs-timeout")
    try:
        # Nothing sent at all: a short receive times out rather than
        # reporting an end that has not happened.
        with pytest.raises(Exception) as caught:
            sub.recv(timeout_ms=200)
        assert not isinstance(caught.value, EOFError), (
            "an idle sender must not look like a finished one"
        )

        pub.send_eos()
        with pytest.raises(EOFError):
            sub.recv(timeout_ms=RECV_MS)
    finally:
        pub.close()
        sub.close()


def test_eos_with_no_data_at_all() -> None:
    """A sender that finishes without sending anything still says so."""
    sub, pub = _pair("eos-empty")
    try:
        pub.send_eos()
        with pytest.raises(EOFError):
            sub.recv(timeout_ms=RECV_MS)
    finally:
        pub.close()
        sub.close()
