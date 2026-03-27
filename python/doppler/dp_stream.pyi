"""Type stubs for the dp_stream C extension."""

from __future__ import annotations

from typing import Any

import numpy as np
from numpy.typing import NDArray

# ---------------------------------------------------------------------------
# Sample-type constants
# ---------------------------------------------------------------------------

CI32: int
CF64: int
CF128: int

# ---------------------------------------------------------------------------
# Utilities
# ---------------------------------------------------------------------------


def get_timestamp_ns() -> int:
    """Return the current time as nanoseconds since the Unix epoch."""
    ...


# ---------------------------------------------------------------------------
# Header dict returned by recv
# ---------------------------------------------------------------------------

class _Header(dict[str, Any]):
    """Dict with at least ``sample_type``, ``sample_rate``,
    ``center_freq``, and ``timestamp_ns`` keys."""
    ...


# ---------------------------------------------------------------------------
# PUB / SUB
# ---------------------------------------------------------------------------


class Publisher:
    """ZMQ PUB socket — broadcasts signal frames to all subscribers.

    Parameters
    ----------
    address:
        ZMQ endpoint, e.g. ``"tcp://*:5555"``.
    sample_type:
        One of :data:`CI32`, :data:`CF64`, :data:`CF128`.
    """

    def __init__(self, address: str, sample_type: int) -> None: ...

    def send(
        self,
        samples: NDArray[Any],
        *,
        sample_rate: float = 0.0,
        center_freq: float = 0.0,
    ) -> None:
        """Broadcast a signal frame.

        Parameters
        ----------
        samples:
            1-D array of CI32 (int32×2), CF64 (complex128), or CF128
            samples matching the socket's *sample_type*.
        sample_rate:
            Samples per second (stored in header; 0 = unspecified).
        center_freq:
            Centre frequency in Hz (stored in header; 0 = unspecified).
        """
        ...

    def close(self) -> None: ...

    def __enter__(self) -> "Publisher": ...
    def __exit__(self, *args: object) -> None: ...


class Subscriber:
    """ZMQ SUB socket — receives signal frames from a publisher.

    Parameters
    ----------
    address:
        ZMQ endpoint, e.g. ``"tcp://localhost:5555"``.
    """

    def __init__(self, address: str) -> None: ...

    def recv(
        self,
        *,
        timeout_ms: int = -1,
    ) -> tuple[NDArray[Any], dict[str, Any]]:
        """Receive one signal frame (zero-copy).

        Parameters
        ----------
        timeout_ms:
            Milliseconds to wait; ``-1`` (default) blocks indefinitely.

        Returns
        -------
        samples:
            1-D NumPy array backed by the ZMQ buffer.  dtype matches
            the sender's *sample_type* (complex128 for CF64, etc.).
        header:
            Dict with keys ``sample_type``, ``sample_rate``,
            ``center_freq``, ``timestamp_ns``, ``num_samples``.

        Raises
        ------
        TimeoutError
            If *timeout_ms* expires before a frame arrives.
        """
        ...

    def close(self) -> None: ...

    def __enter__(self) -> "Subscriber": ...
    def __exit__(self, *args: object) -> None: ...


# ---------------------------------------------------------------------------
# PUSH / PULL
# ---------------------------------------------------------------------------


class Push:
    """ZMQ PUSH socket — sends frames to a PULL socket (pipeline)."""

    def __init__(self, address: str, sample_type: int) -> None: ...

    def send(
        self,
        samples: NDArray[Any],
        *,
        sample_rate: float = 0.0,
        center_freq: float = 0.0,
    ) -> None: ...

    def close(self) -> None: ...

    def __enter__(self) -> "Push": ...
    def __exit__(self, *args: object) -> None: ...


class Pull:
    """ZMQ PULL socket — receives frames from a PUSH socket (pipeline)."""

    def __init__(self, address: str) -> None: ...

    def recv(
        self,
        *,
        timeout_ms: int = -1,
    ) -> tuple[NDArray[Any], dict[str, Any]]: ...

    def close(self) -> None: ...

    def __enter__(self) -> "Pull": ...
    def __exit__(self, *args: object) -> None: ...


# ---------------------------------------------------------------------------
# REQ / REP
# ---------------------------------------------------------------------------


class Requester:
    """ZMQ REQ socket — sends a request and waits for one reply."""

    def __init__(self, address: str, sample_type: int) -> None: ...

    def send(
        self,
        samples: NDArray[Any],
        *,
        sample_rate: float = 0.0,
        center_freq: float = 0.0,
    ) -> None: ...

    def recv(
        self,
        *,
        timeout_ms: int = -1,
    ) -> tuple[NDArray[Any], dict[str, Any]]: ...

    def close(self) -> None: ...

    def __enter__(self) -> "Requester": ...
    def __exit__(self, *args: object) -> None: ...


class Replier:
    """ZMQ REP socket — receives a request and sends one reply."""

    def __init__(self, address: str, sample_type: int) -> None: ...

    def recv(
        self,
        *,
        timeout_ms: int = -1,
    ) -> tuple[NDArray[Any], dict[str, Any]]: ...

    def send(
        self,
        samples: NDArray[Any],
        *,
        sample_rate: float = 0.0,
        center_freq: float = 0.0,
    ) -> None: ...

    def close(self) -> None: ...

    def __enter__(self) -> "Replier": ...
    def __exit__(self, *args: object) -> None: ...
