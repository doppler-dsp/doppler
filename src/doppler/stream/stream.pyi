# stream/stream.pyi — type stubs for the stream C extension.
from typing import Any, Tuple
from numpy.typing import NDArray

CI32: int
CF64: int
CF128: int

def get_timestamp_ns() -> int:
    """Current wall-clock time in nanoseconds (CLOCK_REALTIME)."""
    ...

class Publisher:
    """ZMQ PUB socket — one-to-many broadcast.

    Wraps ``dp_pub_t``.  Each :meth:`send` call emits a two-frame
    multipart message: a ``dp_header_t`` frame followed by a data frame.

    Parameters
    ----------
    endpoint : str
        ZMQ endpoint string, e.g. ``"tcp://*:5556"``.
    sample_type : int
        One of :data:`CI32`, :data:`CF64`, :data:`CF128`.

    Examples
    --------
    >>> from doppler.stream import Publisher, CF64
    >>> import numpy as np
    >>> samples = np.array([1+2j, 3+4j], dtype=np.complex128)
    >>> with Publisher("tcp://*:5556", CF64) as pub:
    ...     pub.send(samples, sample_rate=int(1e6), center_freq=int(2.4e9))

    """

    def __init__(self, endpoint: str, sample_type: int = ...) -> None: ...
    def __enter__(self) -> "Publisher": ...
    def __exit__(self, *args: object) -> None: ...
    def send(
        self,
        samples: NDArray[Any],
        sample_rate: float = 0,
        center_freq: float = 0,
    ) -> None:
        """Send one block of samples."""
        ...
    def close(self) -> None:
        """Destroy the ZMQ socket."""
        ...

class Subscriber:
    """ZMQ SUB socket — receives from a :class:`Publisher`.

    Wraps ``dp_sub_t``.

    Parameters
    ----------
    endpoint : str
        ZMQ endpoint to connect to, e.g. ``"tcp://localhost:5556"``.
    """

    def __init__(self, endpoint: str) -> None: ...
    def __enter__(self) -> "Subscriber": ...
    def __exit__(self, *args: object) -> None: ...
    def recv(self, timeout_ms: int = -1) -> Tuple[NDArray[Any], dict]:
        """Receive one signal frame.

        Returns
        -------
        samples : ndarray
            Data frame decoded into the appropriate NumPy dtype.
        header : dict
            Decoded ``dp_header_t`` fields (``sample_rate``,
            ``center_freq``, ``sample_type``, …).
        """
        ...
    def close(self) -> None:
        """Destroy the ZMQ socket."""
        ...

class Push:
    """ZMQ PUSH socket — pipeline sender.

    Wraps ``dp_push_t``.

    Parameters
    ----------
    endpoint : str
        ZMQ endpoint to bind or connect to.
    sample_type : int
        One of :data:`CI32`, :data:`CF64`, :data:`CF128`.
    """

    def __init__(self, endpoint: str, sample_type: int = ...) -> None: ...
    def __enter__(self) -> "Push": ...
    def __exit__(self, *args: object) -> None: ...
    def send(
        self,
        samples: NDArray[Any],
        sample_rate: float = 0,
        center_freq: float = 0,
    ) -> None:
        """Send one block of samples."""
        ...
    def close(self) -> None:
        """Destroy the ZMQ socket."""
        ...

class Pull:
    """ZMQ PULL socket — pipeline receiver.

    Wraps ``dp_pull_t``.

    Parameters
    ----------
    endpoint : str
        ZMQ endpoint to connect to.
    """

    def __init__(self, endpoint: str) -> None: ...
    def __enter__(self) -> "Pull": ...
    def __exit__(self, *args: object) -> None: ...
    def recv(self, timeout_ms: int = -1) -> Tuple[NDArray[Any], dict]:
        """Receive one signal frame."""
        ...
    def close(self) -> None:
        """Destroy the ZMQ socket."""
        ...

class Requester:
    """ZMQ REQ socket — sends a request, waits for a reply.

    Wraps ``dp_req_t``.

    Parameters
    ----------
    endpoint : str
        ZMQ endpoint to connect to.
    sample_type : int
        One of :data:`CI32`, :data:`CF64`, :data:`CF128`.
    """

    def __init__(self, endpoint: str, sample_type: int = ...) -> None: ...
    def __enter__(self) -> "Requester": ...
    def __exit__(self, *args: object) -> None: ...
    def send(
        self,
        samples: NDArray[Any],
        sample_rate: float = 0,
        center_freq: float = 0,
    ) -> None:
        """Send a request frame."""
        ...
    def recv(self, timeout_ms: int = -1) -> Tuple[NDArray[Any], dict]:
        """Receive the reply frame."""
        ...
    def close(self) -> None:
        """Destroy the ZMQ socket."""
        ...

class Replier:
    """ZMQ REP socket — receives a request, sends a reply.

    Wraps ``dp_rep_t``.

    Parameters
    ----------
    endpoint : str
        ZMQ endpoint to bind.
    sample_type : int
        One of :data:`CI32`, :data:`CF64`, :data:`CF128`.
    """

    def __init__(self, endpoint: str, sample_type: int = ...) -> None: ...
    def __enter__(self) -> "Replier": ...
    def __exit__(self, *args: object) -> None: ...
    def recv(self, timeout_ms: int = -1) -> Tuple[NDArray[Any], dict]:
        """Receive one request frame."""
        ...
    def send(
        self,
        samples: NDArray[Any],
        sample_rate: float = 0,
        center_freq: float = 0,
    ) -> None:
        """Send the reply frame."""
        ...
    def close(self) -> None:
        """Destroy the ZMQ socket."""
        ...
