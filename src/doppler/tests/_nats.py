"""Is a NATS broker listening? One definition, for tests that need one.

Four test modules already carry a private `_nats_available()` of their own
(`cli/tests/test_compose.py`, `wfm/tests/test_api_surface.py`,
`wfm/tests/test_compose.py`, `stream/tests/test_stream.py`). They are
identical in effect and predate this module; new tests should import from
here rather than make it five, and those four are worth migrating in a
pass of their own.
"""

from __future__ import annotations

import socket

HOST = "127.0.0.1"
PORT = 4222


def nats_available(host: str = HOST, port: int = PORT) -> bool:
    """True when something accepts a TCP connection on the broker's port.

    Deliberately only a connect: a deeper probe would need a NATS client,
    and the point is to decide whether to skip, not to diagnose.
    """
    try:
        with socket.create_connection((host, port), timeout=0.5):
            return True
    except OSError:
        return False


# The transport's own words for "that stream is already gone". The C layer
# reports what the broker said rather than a bare code (doppler#1131), which
# is what makes this distinguishable at all: the full text is
# "delete_stream failed: Invalid argument -- Not Found".
_STREAM_ALREADY_GONE = "Not Found"


def delete_stream_if_present(push) -> None:
    """Delete a work-queue stream, tolerating one that is already absent.

    ``Push.delete_stream()`` is deliberately strict -- deleting a stream
    that is not there is an error, and
    ``test_deleting_a_stream_twice_reports_the_brokers_refusal`` pins that.
    Strictness is right for the API and wrong for CLEANUP: teardown runs
    whatever the test did, and deleting something twice is the ordinary
    shape of cleanup code. Unconditional deletion turned a passing test
    into a CI ERROR against whichever test used the fixture last
    (doppler#1147).

    Only the ALREADY-ABSENT case is tolerated, so the gate doppler#1136
    asked for survives: a stream that EXISTS and cannot be deleted still
    fails the run, and so the 40 GB of work-queue residue that issue was
    about would still be caught.

    Not the cause, ruled out while fixing this: subject collision between
    xdist workers. ``_unique_endpoint`` draws from ``random``, which each
    worker seeds from urandom, and nothing in the suite calls
    ``random.seed`` -- only ``numpy.random.seed``, in a different module.

    Parameters
    ----------
    push : doppler.stream.Push
        The producer that provisioned the stream. Any other ``RuntimeError``
        -- an unreachable broker, a timeout -- propagates unchanged.
    """
    try:
        push.delete_stream()
    except RuntimeError as exc:
        if _STREAM_ALREADY_GONE not in str(exc):
            raise
