# stream/stream.pyi — type stubs for the stream C extension.
from typing import Any, Dict, Tuple
from numpy.typing import NDArray

# ---------------------------------------------------------------------------
# Sample-type constants
#
# These map to dp_sample_type_t values on the wire.  Only CI32, CF64, and
# CF128 are fully supported by the Python binding (send + recv decode).
# CI8, CI16, and CF32 exist in the C wire protocol but the Python recv path
# raises ValueError if it encounters them; do not use those values here.
# ---------------------------------------------------------------------------

CI32: int
"""Complex int32 — interleaved ``int32_t`` I/Q (8 bytes/sample).

Sent as a flat ``int32`` array of length ``2*n_samples`` (I0, Q0, I1, Q1, …).
Received as the same flat ``int32`` layout.
"""

CF64: int
"""Complex float64 — ``double _Complex`` (16 bytes/sample).

Sent and received as ``numpy.complex128``.  Default sample type for all
sender socket types.
"""

CF128: int
"""Complex long double — ``long double _Complex`` (32 bytes/sample).

Sent and received as ``numpy.clongdouble``.
"""


def get_timestamp_ns() -> int:
    """Current wall-clock time in nanoseconds (CLOCK_REALTIME).

    Returns
    -------
    int
        Monotonically non-decreasing nanosecond timestamp.  Suitable for
        stamping outgoing frames when the caller does not supply its own
        ``timestamp_ns``.

    Examples
    --------
    >>> from doppler.stream import get_timestamp_ns
    >>> t = get_timestamp_ns()
    >>> t > 0
    True

    """
    ...


class Publisher:
    """ZMQ PUB socket — one-to-many broadcast.

    Wraps ``dp_pub_t``.  Each :meth:`send` call emits a two-frame ZMQ
    multipart message: a ``dp_header_t`` frame followed by a raw data frame.
    Multiple :class:`Subscriber` sockets can connect to one Publisher.

    Parameters
    ----------
    endpoint : str
        ZMQ endpoint to bind, e.g. ``"tcp://*:5556"`` or
        ``"ipc:///tmp/iq.sock"``.
    sample_type : int
        Wire encoding.  One of :data:`CI32`, :data:`CF64` (default),
        :data:`CF128`.

    Examples
    --------
    >>> from doppler.stream import Publisher, CF64
    >>> import numpy as np
    >>> samples = np.array([1+2j, 3+4j], dtype=np.complex128)
    >>> with Publisher("tcp://*:5556", CF64) as pub:  # doctest: +SKIP
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
        """Broadcast one block of samples.

        Parameters
        ----------
        samples : ndarray
            Data to send.  Must be C-contiguous and match the socket's
            ``sample_type``: ``complex128`` for CF64, ``clongdouble`` for
            CF128, ``int32`` for CI32.
        sample_rate : float, optional
            Samples per second written into the header (default 0).
        center_freq : float, optional
            Centre frequency in Hz written into the header (default 0).
        """
        ...

    def close(self) -> None:
        """Destroy the ZMQ socket and release all resources."""
        ...


class Subscriber:
    """ZMQ SUB socket — receives from a :class:`Publisher`.

    Wraps ``dp_sub_t``.  Connects to the Publisher's endpoint.  A single
    Subscriber can only connect to one Publisher; for fan-in use
    :class:`Pull`.

    Parameters
    ----------
    endpoint : str
        ZMQ endpoint to connect to, e.g. ``"tcp://localhost:5556"``.

    Examples
    --------
    >>> from doppler.stream import Subscriber
    >>> with Subscriber("tcp://localhost:5556") as sub:  # doctest: +SKIP
    ...     samples, hdr = sub.recv(timeout_ms=1000)

    """

    def __init__(self, endpoint: str) -> None: ...
    def __enter__(self) -> "Subscriber": ...
    def __exit__(self, *args: object) -> None: ...
    def recv(self, timeout_ms: int = -1) -> Tuple[NDArray[Any], Dict[str, Any]]:
        """Receive one signal frame.

        Parameters
        ----------
        timeout_ms : int, optional
            Milliseconds to wait before raising :exc:`TimeoutError`.
            ``-1`` (default) blocks indefinitely.

        Returns
        -------
        samples : ndarray
            Decoded data.  dtype is ``complex128`` (CF64), ``clongdouble``
            (CF128), or ``int32`` flat I/Q (CI32).
        header : dict
            Decoded ``dp_header_t`` fields:

            ``sample_rate`` : float
                Samples per second.
            ``center_freq`` : float
                Centre frequency in Hz.
            ``sample_type`` : int
                Wire sample type (one of :data:`CI32`, :data:`CF64`, …).
            ``timestamp_ns`` : int
                Frame timestamp (CLOCK_REALTIME nanoseconds).
            ``sequence`` : int
                Monotonically increasing frame counter.
            ``num_samples`` : int
                Number of IQ samples in the frame.
            ``protocol`` : int
                Wire protocol identifier.
            ``stream_id`` : int
                Opaque stream identifier.

        Raises
        ------
        TimeoutError
            If ``timeout_ms`` elapses before a frame arrives.
        """
        ...

    def close(self) -> None:
        """Destroy the ZMQ socket and release all resources."""
        ...


class Push:
    """ZMQ PUSH socket — pipeline sender.

    Wraps ``dp_push_t``.  Frames are distributed round-robin across all
    connected :class:`Pull` sockets.  Complements :class:`Pull` for
    work-queue / pipeline topologies.

    Parameters
    ----------
    endpoint : str
        ZMQ endpoint to bind, e.g. ``"tcp://*:5557"``.
    sample_type : int
        One of :data:`CI32`, :data:`CF64` (default), :data:`CF128`.

    Examples
    --------
    >>> from doppler.stream import Push, Pull, CF64
    >>> import numpy as np
    >>> ep = "tcp://127.0.0.1:5557"
    >>> with Push(ep, CF64) as push, Pull(ep) as pull:  # doctest: +SKIP
    ...     push.send(np.ones(4, dtype=np.complex128))
    ...     samples, hdr = pull.recv(timeout_ms=1000)

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
        """Send one block of samples to the next Pull worker.

        Parameters
        ----------
        samples : ndarray
            C-contiguous array matching the socket's ``sample_type``.
        sample_rate : float, optional
            Samples per second (header field, default 0).
        center_freq : float, optional
            Centre frequency in Hz (header field, default 0).
        """
        ...

    def close(self) -> None:
        """Destroy the ZMQ socket and release all resources."""
        ...


class Pull:
    """ZMQ PULL socket — pipeline receiver.

    Wraps ``dp_pull_t``.  Receives frames from a :class:`Push` socket.
    Multiple Pull workers can share one Push sender for load distribution.

    Parameters
    ----------
    endpoint : str
        ZMQ endpoint to connect to, e.g. ``"tcp://localhost:5557"``.

    Examples
    --------
    >>> from doppler.stream import Pull
    >>> with Pull("tcp://localhost:5557") as pull:  # doctest: +SKIP
    ...     samples, hdr = pull.recv(timeout_ms=1000)

    """

    def __init__(self, endpoint: str) -> None: ...
    def __enter__(self) -> "Pull": ...
    def __exit__(self, *args: object) -> None: ...
    def recv(self, timeout_ms: int = -1) -> Tuple[NDArray[Any], Dict[str, Any]]:
        """Receive one signal frame.

        Parameters
        ----------
        timeout_ms : int, optional
            Milliseconds to wait before raising :exc:`TimeoutError`.
            ``-1`` (default) blocks indefinitely.

        Returns
        -------
        samples : ndarray
            Decoded data array.
        header : dict
            ``dp_header_t`` fields — see :meth:`Subscriber.recv` for the
            full key list.

        Raises
        ------
        TimeoutError
            If ``timeout_ms`` elapses before a frame arrives.
        """
        ...

    def close(self) -> None:
        """Destroy the ZMQ socket and release all resources."""
        ...


class Requester:
    """ZMQ REQ socket — sends a request frame, then waits for a reply.

    Wraps ``dp_req_t``.  The REQ/REP pattern is strictly alternating:
    :meth:`send` must be called before :meth:`recv`, and :meth:`recv`
    must complete before the next :meth:`send`.  Complements
    :class:`Replier`.

    Parameters
    ----------
    endpoint : str
        ZMQ endpoint to connect to, e.g. ``"tcp://localhost:5558"``.
    sample_type : int
        One of :data:`CI32`, :data:`CF64` (default), :data:`CF128`.
        Controls the dtype of frames *sent* by this socket.

    Examples
    --------
    >>> from doppler.stream import Requester, CF64
    >>> import numpy as np
    >>> with Requester("tcp://localhost:5558", CF64) as req:  # doctest: +SKIP
    ...     req.send(np.ones(4, dtype=np.complex128), sample_rate=int(1e6))
    ...     reply, hdr = req.recv(timeout_ms=2000)

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
        """Send a request frame to the connected :class:`Replier`.

        Parameters
        ----------
        samples : ndarray
            C-contiguous array matching the socket's ``sample_type``.
        sample_rate : float, optional
            Samples per second (header field, default 0).
        center_freq : float, optional
            Centre frequency in Hz (header field, default 0).
        """
        ...

    def recv(self, timeout_ms: int = -1) -> Tuple[NDArray[Any], Dict[str, Any]]:
        """Receive the reply frame from the :class:`Replier`.

        Must be called after :meth:`send`; calling without a prior send
        results in undefined behaviour (ZMQ FSM violation).

        Parameters
        ----------
        timeout_ms : int, optional
            Milliseconds to wait before raising :exc:`TimeoutError`.
            ``-1`` (default) blocks indefinitely.

        Returns
        -------
        samples : ndarray
            Decoded reply data.
        header : dict
            ``dp_header_t`` fields — see :meth:`Subscriber.recv` for the
            full key list.

        Raises
        ------
        TimeoutError
            If ``timeout_ms`` elapses before the reply arrives.
        """
        ...

    def close(self) -> None:
        """Destroy the ZMQ socket and release all resources."""
        ...


class Replier:
    """ZMQ REP socket — receives a request, then sends a reply.

    Wraps ``dp_rep_t``.  The REQ/REP pattern is strictly alternating:
    :meth:`recv` must be called first, then :meth:`send`.  Complements
    :class:`Requester`.

    Parameters
    ----------
    endpoint : str
        ZMQ endpoint to bind, e.g. ``"tcp://*:5558"``.
    sample_type : int
        One of :data:`CI32`, :data:`CF64` (default), :data:`CF128`.
        Controls the dtype of frames *sent* by this socket (the reply).

    Examples
    --------
    >>> from doppler.stream import Replier, CF64
    >>> import numpy as np
    >>> with Replier("tcp://*:5558", CF64) as rep:  # doctest: +SKIP
    ...     request, hdr = rep.recv(timeout_ms=5000)
    ...     # process request...
    ...     rep.send(request, sample_rate=hdr["sample_rate"])

    """

    def __init__(self, endpoint: str, sample_type: int = ...) -> None: ...
    def __enter__(self) -> "Replier": ...
    def __exit__(self, *args: object) -> None: ...
    def recv(self, timeout_ms: int = -1) -> Tuple[NDArray[Any], Dict[str, Any]]:
        """Receive one request frame from the :class:`Requester`.

        Parameters
        ----------
        timeout_ms : int, optional
            Milliseconds to wait before raising :exc:`TimeoutError`.
            ``-1`` (default) blocks indefinitely.

        Returns
        -------
        samples : ndarray
            Decoded request data.
        header : dict
            ``dp_header_t`` fields — see :meth:`Subscriber.recv` for the
            full key list.

        Raises
        ------
        TimeoutError
            If ``timeout_ms`` elapses before a request arrives.
        """
        ...

    def send(
        self,
        samples: NDArray[Any],
        sample_rate: float = 0,
        center_freq: float = 0,
    ) -> None:
        """Send the reply frame back to the :class:`Requester`.

        Must be called after :meth:`recv`; calling without a prior recv
        results in undefined behaviour (ZMQ FSM violation).

        Parameters
        ----------
        samples : ndarray
            C-contiguous array matching the socket's ``sample_type``.
        sample_rate : float, optional
            Samples per second (header field, default 0).
        center_freq : float, optional
            Centre frequency in Hz (header field, default 0).
        """
        ...

    def close(self) -> None:
        """Destroy the ZMQ socket and release all resources."""
        ...
