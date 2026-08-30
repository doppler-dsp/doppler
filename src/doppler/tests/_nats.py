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
