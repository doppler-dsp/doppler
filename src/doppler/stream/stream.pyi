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

Each complex sample occupies two consecutive ``int32_t`` elements in the
array: element 2k is I, element 2k+1 is Q.  For *n* complex samples the
send functions expect a C-contiguous ``numpy.int32`` array of length
``2*n``; recv returns the same flat layout.

Wire value: ``0``.

Examples
--------
>>> from doppler.stream import CI32
>>> CI32
0

"""

CF64: int
"""Complex float64 — ``double _Complex`` (16 bytes/sample).

Sent and received as ``numpy.complex128``.  Default sample type for all
sender socket types.

Wire value: ``1``.

Examples
--------
>>> from doppler.stream import CF64
>>> CF64
1

"""

CF128: int
"""Complex long double — ``long double _Complex`` (32 bytes/sample).

Sent and received as ``numpy.clongdouble`` (``complex256`` on x86-64
Linux where ``long double`` is 80-bit extended precision stored in 16
bytes, giving an effective 128-byte-per-pair wire format).

Wire value: ``2``.

Examples
--------
>>> from doppler.stream import CF128
>>> CF128
2

"""


def get_timestamp_ns() -> int:
    """Current wall-clock time in nanoseconds since the UNIX epoch.

    Calls ``clock_gettime(CLOCK_REALTIME)`` in the C layer.  Useful for
    stamping outgoing frames when the caller does not supply its own
    ``timestamp_ns``, or for computing round-trip latency from the value
    returned in the received header dict.

    Returns
    -------
    int
        Non-negative nanosecond timestamp.  Guaranteed to be
        monotonically non-decreasing within a single process on any
        POSIX system that supports ``CLOCK_REALTIME``.

    Examples
    --------
    >>> from doppler.stream import get_timestamp_ns
    >>> t = get_timestamp_ns()
    >>> isinstance(t, int)
    True
    >>> t > 0
    True

    """
    ...


class Publisher:
    """ZMQ PUB socket — one-to-many broadcast of signal frames.

    Wraps ``dp_pub_t``.  Each :meth:`send` call emits a two-frame ZMQ
    multipart message: a ``dp_header_t`` frame followed by a raw data
    frame.  Multiple :class:`Subscriber` sockets can connect to one
    Publisher; each subscriber receives every frame.  Slow subscribers
    drop frames according to ZMQ's high-watermark (HWM) policy —
    there is no back-pressure.

    The socket *binds* to ``endpoint``; Subscribers connect to it.
    Use ``"ipc:///tmp/feed"`` for intra-host transfers (lower latency
    than TCP) and ``"tcp://*:PORT"`` for inter-host.

    Parameters
    ----------
    endpoint : str
        ZMQ endpoint to bind, e.g. ``"tcp://*:5556"`` or
        ``"ipc:///tmp/iq.sock"``.
    sample_type : int
        Wire encoding.  One of :data:`CI32`, :data:`CF64` (default),
        :data:`CF128`.  Raises :exc:`ValueError` for any other value.

    Raises
    ------
    ValueError
        If ``sample_type`` is not one of the three supported values.
    RuntimeError
        If the ZMQ context or socket cannot be created (e.g. the port
        is already in use).

    Examples
    --------
    Construct, use as a context manager, and verify the type:

    >>> from doppler.stream import Publisher, CF64
    >>> pub = Publisher("tcp://*:19100", CF64)
    >>> type(pub).__name__
    'Publisher'
    >>> pub.close()

    Context-manager form (preferred — ensures close on exception):

    >>> with Publisher("tcp://*:19101", CF64) as pub:
    ...     type(pub).__name__
    'Publisher'

    Full send round-trip with a :class:`Subscriber` (requires a live
    ZMQ connection and a brief warm-up sleep):

    >>> import numpy as np, time                        # doctest: +SKIP
    >>> from doppler.stream import Subscriber           # doctest: +SKIP
    >>> pub = Publisher("tcp://*:5556", CF64)           # doctest: +SKIP
    >>> sub = Subscriber("tcp://localhost:5556")        # doctest: +SKIP
    >>> time.sleep(0.1)                                 # doctest: +SKIP
    >>> pub.send(np.array([1+2j, 3+4j],                # doctest: +SKIP
    ...          dtype=np.complex128),                  # doctest: +SKIP
    ...          sample_rate=int(1e6),                  # doctest: +SKIP
    ...          center_freq=int(2.4e9))                # doctest: +SKIP
    >>> samples, hdr = sub.recv(timeout_ms=2000)        # doctest: +SKIP
    >>> pub.close(); sub.close()                        # doctest: +SKIP
    """

    def __init__(self, endpoint: str, sample_type: int = ...) -> None:
        """Create a Publisher socket and bind to ``endpoint``.

        Parameters
        ----------
        endpoint : str
            ZMQ endpoint string to bind, e.g. ``"tcp://*:5556"`` or
            ``"ipc:///tmp/feed"``.
        sample_type : int, optional
            Wire encoding: :data:`CI32`, :data:`CF64` (default), or
            :data:`CF128`.
        """
        ...

    def __enter__(self) -> "Publisher":
        """Return ``self`` for use as a context manager.

        Examples
        --------
        >>> from doppler.stream import Publisher, CF64
        >>> with Publisher("tcp://*:19102", CF64) as pub:
        ...     type(pub).__name__
        'Publisher'

        """
        ...

    def __exit__(self, *args: object) -> None:
        """Call :meth:`close` on context-manager exit."""
        ...

    def send(
        self,
        samples: NDArray[Any],
        sample_rate: float = 0,
        center_freq: float = 0,
    ) -> None:
        """Broadcast one block of samples to all connected Subscribers.

        Constructs a ``dp_header_t`` with the supplied metadata (plus an
        auto-generated ``timestamp_ns`` from ``CLOCK_REALTIME`` and a
        per-socket monotonically increasing ``sequence`` number), then
        sends a two-frame ZMQ multipart message: header frame followed
        by raw sample bytes.  The call releases the GIL while blocked in
        ZMQ, so other Python threads can run concurrently.

        Parameters
        ----------
        samples : ndarray
            C-contiguous array whose dtype must match the socket's
            ``sample_type``: ``numpy.complex128`` for :data:`CF64`,
            ``numpy.clongdouble`` for :data:`CF128`, ``numpy.int32``
            for :data:`CI32` (interleaved I/Q, length ``2*n_samples``).
        sample_rate : float, optional
            Samples per second written into the header (default 0).
        center_freq : float, optional
            Centre frequency in Hz written into the header (default 0).

        Raises
        ------
        TypeError
            If ``samples.dtype`` does not match the socket's
            ``sample_type``.
        ValueError
            If ``samples`` is not C-contiguous.
        RuntimeError
            If the ZMQ send fails.

        Examples
        --------
        >>> from doppler.stream import Publisher, CF64  # doctest: +SKIP
        >>> import numpy as np, time                    # doctest: +SKIP
        >>> pub = Publisher("tcp://*:5556", CF64)       # doctest: +SKIP
        >>> pub.send(np.ones(4, dtype=np.complex128),   # doctest: +SKIP
        ...          sample_rate=int(48000),             # doctest: +SKIP
        ...          center_freq=int(433e6))             # doctest: +SKIP
        >>> pub.close()                                 # doctest: +SKIP
        """
        ...

    def close(self) -> None:
        """Destroy the ZMQ socket and release all resources.

        Calls ``dp_pub_destroy()``.  Safe to call multiple times —
        subsequent calls are no-ops.  After ``close()`` the object
        must not be used for sending.

        Examples
        --------
        >>> from doppler.stream import Publisher, CF64
        >>> pub = Publisher("tcp://*:19103", CF64)
        >>> pub.close()
        >>> pub.close()  # idempotent — no error

        """
        ...


class Subscriber:
    """ZMQ SUB socket — receives signal frames from a :class:`Publisher`.

    Wraps ``dp_sub_t``.  Connects to the Publisher's endpoint.  The
    socket subscribes to all topics (empty ZMQ topic filter), so it
    receives every frame the Publisher sends.

    Unlike :class:`Pull`, a single Subscriber socket connects to exactly
    one Publisher.  For fan-in (receiving from multiple publishers) or
    load-balanced consumption, use :class:`Pull`.

    The recv path is zero-copy: the returned NumPy array's memory is
    owned by an internal ``dp_msg_t`` handle.  The ZMQ buffer is freed
    only when the array (and all views of it) are garbage-collected.

    Parameters
    ----------
    endpoint : str
        ZMQ endpoint to connect to, e.g. ``"tcp://localhost:5556"`` or
        ``"ipc:///tmp/feed"``.

    Raises
    ------
    RuntimeError
        If the ZMQ context or socket cannot be created.

    Examples
    --------
    >>> from doppler.stream import Subscriber
    >>> sub = Subscriber("tcp://localhost:19104")
    >>> type(sub).__name__
    'Subscriber'
    >>> sub.close()

    Context-manager form:

    >>> with Subscriber("tcp://localhost:19105") as sub:
    ...     type(sub).__name__
    'Subscriber'

    Receive one frame (requires a live :class:`Publisher`):

    >>> sub.recv(timeout_ms=500)            # doctest: +SKIP
    (array([1.+2.j, 3.+4.j]), {'sample_rate': 1000000.0, ...})
    """

    def __init__(self, endpoint: str) -> None:
        """Create a Subscriber socket and connect to ``endpoint``.

        Parameters
        ----------
        endpoint : str
            ZMQ endpoint to connect to, e.g.
            ``"tcp://localhost:5556"``.
        """
        ...

    def __enter__(self) -> "Subscriber":
        """Return ``self`` for use as a context manager.

        Examples
        --------
        >>> from doppler.stream import Subscriber
        >>> with Subscriber("tcp://localhost:19106") as sub:
        ...     type(sub).__name__
        'Subscriber'

        """
        ...

    def __exit__(self, *args: object) -> None:
        """Call :meth:`close` on context-manager exit."""
        ...

    def recv(
        self, timeout_ms: int = -1
    ) -> Tuple[NDArray[Any], Dict[str, Any]]:
        """Receive one signal frame from the connected Publisher.

        Blocks until a frame arrives or the optional timeout expires.
        The returned NumPy array is a zero-copy view into the ZMQ
        message buffer; the buffer is freed when the array is
        garbage-collected.

        Parameters
        ----------
        timeout_ms : int, optional
            Milliseconds to wait before raising :exc:`TimeoutError`.
            ``-1`` (default) blocks indefinitely; ``0`` returns
            immediately if no frame is available.

        Returns
        -------
        samples : ndarray
            Decoded sample data.  dtype is ``numpy.complex128``
            (:data:`CF64`), ``numpy.clongdouble`` (:data:`CF128`), or
            ``numpy.int32`` flat interleaved I/Q (:data:`CI32`).
        header : dict
            Decoded ``dp_header_t`` fields:

            ``sample_rate`` : float
                Samples per second as reported by the sender.
            ``center_freq`` : float
                Centre frequency in Hz as reported by the sender.
            ``sample_type`` : int
                Wire sample type (one of :data:`CI32`, :data:`CF64`,
                :data:`CF128`).
            ``timestamp_ns`` : int
                Frame timestamp (``CLOCK_REALTIME`` nanoseconds) set
                by the sender at the moment of the send call.
            ``sequence`` : int
                Monotonically increasing per-sender frame counter;
                gaps indicate dropped frames.
            ``num_samples`` : int
                Number of IQ samples in the frame (``len(samples)``
                for CF64/CF128; ``len(samples)//2`` for CI32).
            ``protocol`` : int
                Wire protocol (0 = SIGS, 1 = DIFI/VITA 49).
            ``stream_id`` : int
                Opaque stream identifier (0 for SIGS protocol).

        Raises
        ------
        TimeoutError
            If ``timeout_ms`` elapses before a frame arrives.
        RuntimeError
            If the ZMQ recv fails for any other reason.

        Examples
        --------
        >>> from doppler.stream import Subscriber          # doctest: +SKIP
        >>> with Subscriber("tcp://localhost:5556") as sub:# doctest: +SKIP
        ...     samples, hdr = sub.recv(timeout_ms=1000)  # doctest: +SKIP
        ...     print(samples.dtype, hdr["sample_rate"])  # doctest: +SKIP
        """
        ...

    def close(self) -> None:
        """Destroy the ZMQ socket and release all resources.

        Calls ``dp_sub_destroy()``.  Safe to call multiple times.

        Examples
        --------
        >>> from doppler.stream import Subscriber
        >>> sub = Subscriber("tcp://localhost:19107")
        >>> sub.close()
        >>> sub.close()  # idempotent — no error

        """
        ...


class Push:
    """ZMQ PUSH socket — pipeline sender for load-balanced distribution.

    Wraps ``dp_push_t``.  Frames are distributed round-robin across all
    connected :class:`Pull` sockets.  Each frame is delivered to exactly
    one Pull consumer, unlike :class:`Publisher` which fans out to all
    subscribers.

    Use the PUSH/PULL pattern when you have a pool of workers and want
    automatic load balancing, or when you need back-pressure (PUSH blocks
    when no Pull worker is ready to receive).

    The socket *binds* to ``endpoint``; Pull workers connect to it.

    Parameters
    ----------
    endpoint : str
        ZMQ endpoint to bind, e.g. ``"tcp://*:5557"``.
    sample_type : int
        Wire encoding.  One of :data:`CI32`, :data:`CF64` (default),
        :data:`CF128`.  Raises :exc:`ValueError` for any other value.

    Raises
    ------
    ValueError
        If ``sample_type`` is not one of the three supported values.
    RuntimeError
        If the ZMQ context or socket cannot be created.

    Examples
    --------
    >>> from doppler.stream import Push, CF64
    >>> push = Push("tcp://*:19108", CF64)
    >>> type(push).__name__
    'Push'
    >>> push.close()

    Context-manager form:

    >>> with Push("tcp://*:19109", CF64) as push:
    ...     type(push).__name__
    'Push'

    Round-trip with a :class:`Pull` worker (requires a live connection):

    >>> import numpy as np, time                        # doctest: +SKIP
    >>> from doppler.stream import Pull                 # doctest: +SKIP
    >>> push = Push("tcp://127.0.0.1:5557", CF64)       # doctest: +SKIP
    >>> pull = Pull("tcp://127.0.0.1:5557")             # doctest: +SKIP
    >>> time.sleep(0.05)                                # doctest: +SKIP
    >>> push.send(np.ones(4, dtype=np.complex128))      # doctest: +SKIP
    >>> samples, hdr = pull.recv(timeout_ms=2000)       # doctest: +SKIP
    >>> push.close(); pull.close()                      # doctest: +SKIP
    """

    def __init__(self, endpoint: str, sample_type: int = ...) -> None:
        """Create a Push socket and bind to ``endpoint``.

        Parameters
        ----------
        endpoint : str
            ZMQ endpoint to bind, e.g. ``"tcp://*:5557"``.
        sample_type : int, optional
            Wire encoding: :data:`CI32`, :data:`CF64` (default), or
            :data:`CF128`.
        """
        ...

    def __enter__(self) -> "Push":
        """Return ``self`` for use as a context manager.

        Examples
        --------
        >>> from doppler.stream import Push, CF64
        >>> with Push("tcp://*:19110", CF64) as push:
        ...     type(push).__name__
        'Push'

        """
        ...

    def __exit__(self, *args: object) -> None:
        """Call :meth:`close` on context-manager exit."""
        ...

    def send(
        self,
        samples: NDArray[Any],
        sample_rate: float = 0,
        center_freq: float = 0,
    ) -> None:
        """Send one block of samples to the next available Pull worker.

        Frames are distributed round-robin.  The call blocks until a
        Pull socket is ready to accept (back-pressure), then releases
        the GIL while blocked in ZMQ.

        Parameters
        ----------
        samples : ndarray
            C-contiguous array whose dtype must match the socket's
            ``sample_type``: ``numpy.complex128`` for :data:`CF64`,
            ``numpy.clongdouble`` for :data:`CF128`, ``numpy.int32``
            for :data:`CI32` (interleaved I/Q, length ``2*n``).
        sample_rate : float, optional
            Samples per second written into the header (default 0).
        center_freq : float, optional
            Centre frequency in Hz written into the header (default 0).

        Raises
        ------
        TypeError
            If ``samples.dtype`` does not match the socket's
            ``sample_type``.
        ValueError
            If ``samples`` is not C-contiguous.
        RuntimeError
            If the ZMQ send fails.

        Examples
        --------
        >>> from doppler.stream import Push, Pull, CF64  # doctest: +SKIP
        >>> import numpy as np, time                     # doctest: +SKIP
        >>> push = Push("tcp://127.0.0.1:5557", CF64)    # doctest: +SKIP
        >>> pull = Pull("tcp://127.0.0.1:5557")          # doctest: +SKIP
        >>> time.sleep(0.05)                             # doctest: +SKIP
        >>> push.send(np.array([1+2j, 3+4j],            # doctest: +SKIP
        ...           dtype=np.complex128),              # doctest: +SKIP
        ...           sample_rate=int(48000))            # doctest: +SKIP
        >>> samples, hdr = pull.recv(timeout_ms=2000)   # doctest: +SKIP
        >>> push.close(); pull.close()                  # doctest: +SKIP
        """
        ...

    def close(self) -> None:
        """Destroy the ZMQ socket and release all resources.

        Calls ``dp_push_destroy()``.  Safe to call multiple times.

        Examples
        --------
        >>> from doppler.stream import Push, CF64
        >>> push = Push("tcp://*:19111", CF64)
        >>> push.close()
        >>> push.close()  # idempotent — no error

        """
        ...


class Pull:
    """ZMQ PULL socket — pipeline receiver for load-balanced workers.

    Wraps ``dp_pull_t``.  Receives frames from a :class:`Push` socket.
    Multiple Pull workers can share one Push sender; each frame goes to
    exactly one worker (round-robin from the Push side).

    The recv path is zero-copy: see :class:`Subscriber` for the buffer
    lifetime semantics.

    The socket *connects* to the Push endpoint; the Push socket binds.

    Parameters
    ----------
    endpoint : str
        ZMQ endpoint to connect to, e.g. ``"tcp://localhost:5557"``.

    Raises
    ------
    RuntimeError
        If the ZMQ context or socket cannot be created.

    Examples
    --------
    >>> from doppler.stream import Pull
    >>> pull = Pull("tcp://localhost:19112")
    >>> type(pull).__name__
    'Pull'
    >>> pull.close()

    Context-manager form:

    >>> with Pull("tcp://localhost:19113") as pull:
    ...     type(pull).__name__
    'Pull'

    Receive one frame (requires a live :class:`Push`):

    >>> pull.recv(timeout_ms=500)             # doctest: +SKIP
    (array([1.+2.j, ...]), {'sample_rate': ..., ...})
    """

    def __init__(self, endpoint: str) -> None:
        """Create a Pull socket and connect to ``endpoint``.

        Parameters
        ----------
        endpoint : str
            ZMQ endpoint to connect to, e.g.
            ``"tcp://localhost:5557"``.
        """
        ...

    def __enter__(self) -> "Pull":
        """Return ``self`` for use as a context manager.

        Examples
        --------
        >>> from doppler.stream import Pull
        >>> with Pull("tcp://localhost:19114") as pull:
        ...     type(pull).__name__
        'Pull'

        """
        ...

    def __exit__(self, *args: object) -> None:
        """Call :meth:`close` on context-manager exit."""
        ...

    def recv(
        self, timeout_ms: int = -1
    ) -> Tuple[NDArray[Any], Dict[str, Any]]:
        """Receive one signal frame from the connected Push socket.

        Blocks until a frame arrives or the optional timeout expires.
        The returned NumPy array is a zero-copy view into the ZMQ
        message buffer; the buffer is freed when the array is
        garbage-collected.

        Parameters
        ----------
        timeout_ms : int, optional
            Milliseconds to wait before raising :exc:`TimeoutError`.
            ``-1`` (default) blocks indefinitely; ``0`` returns
            immediately if no frame is available.

        Returns
        -------
        samples : ndarray
            Decoded sample data.  dtype is ``numpy.complex128``
            (:data:`CF64`), ``numpy.clongdouble`` (:data:`CF128`), or
            ``numpy.int32`` flat interleaved I/Q (:data:`CI32`).
        header : dict
            Decoded ``dp_header_t`` fields — see
            :meth:`Subscriber.recv` for the full key list.

        Raises
        ------
        TimeoutError
            If ``timeout_ms`` elapses before a frame arrives.
        RuntimeError
            If the ZMQ recv fails for any other reason.

        Examples
        --------
        >>> from doppler.stream import Pull             # doctest: +SKIP
        >>> with Pull("tcp://localhost:5557") as pull:  # doctest: +SKIP
        ...     samples, hdr = pull.recv(timeout_ms=1000)# doctest: +SKIP
        ...     print(samples.dtype, hdr["num_samples"])# doctest: +SKIP
        """
        ...

    def ack(self, samples: NDArray[Any]) -> None:
        """Acknowledge a frame on a durable (JetStream) consumer.

        For the resilient NATS work-queue tier (a ``nats://`` Pull), delivery
        is at-least-once: a frame stays pending until acked and is redelivered
        if the worker dies first.  Pass the array returned by :meth:`recv`
        once it has been fully processed, then drop the array.

        A no-op for transports without acks (ZMQ, NATS core PUB/SUB), so it is
        always safe to call.

        Parameters
        ----------
        samples : ndarray
            The samples array returned by :meth:`recv` (its buffer still
            alive — do not ack after the array is garbage-collected).

        Raises
        ------
        ValueError
            If ``samples`` is not an un-freed :meth:`recv` result.
        RuntimeError
            If the acknowledgement fails.

        Examples
        --------
        >>> from doppler.stream import Pull                 # doctest: +SKIP
        >>> pull = Pull("nats://localhost:4222/work")       # doctest: +SKIP
        >>> samples, hdr = pull.recv(timeout_ms=1000)       # doctest: +SKIP
        >>> pull.ack(samples)                               # doctest: +SKIP
        """
        ...

    def close(self) -> None:
        """Destroy the ZMQ socket and release all resources.

        Calls ``dp_pull_destroy()``.  Safe to call multiple times.

        Examples
        --------
        >>> from doppler.stream import Pull
        >>> pull = Pull("tcp://localhost:19115")
        >>> pull.close()
        >>> pull.close()  # idempotent — no error

        """
        ...


class Requester:
    """ZMQ REQ socket — sends a request frame, then waits for a reply.

    Wraps ``dp_req_t``.  The REQ/REP pattern is strictly alternating:
    :meth:`send` must be called before :meth:`recv`, and :meth:`recv`
    must complete before the next :meth:`send`.  Violating this order
    triggers a ZMQ FSM error.

    Complements :class:`Replier`.  Use this pattern for control-plane
    messages (tuning commands, metadata queries) or synchronous
    signal-frame RPC where one peer processes each frame and returns a
    result.

    The socket *connects* to the Replier endpoint.

    Parameters
    ----------
    endpoint : str
        ZMQ endpoint to connect to, e.g. ``"tcp://localhost:5558"``.
    sample_type : int
        Wire encoding of frames *sent* by this socket.  One of
        :data:`CI32`, :data:`CF64` (default), :data:`CF128`.  The
        reply frame's type is determined by the :class:`Replier`.

    Raises
    ------
    ValueError
        If ``sample_type`` is not one of the three supported values.
    RuntimeError
        If the ZMQ context or socket cannot be created.

    Examples
    --------
    >>> from doppler.stream import Requester, CF64
    >>> req = Requester("tcp://localhost:19116", CF64)
    >>> type(req).__name__
    'Requester'
    >>> req.close()

    Context-manager form:

    >>> with Requester("tcp://localhost:19117", CF64) as req:
    ...     type(req).__name__
    'Requester'

    Full REQ/REP round-trip (requires a live :class:`Replier`):

    >>> import numpy as np, time                            # doctest: +SKIP
    >>> from doppler.stream import Replier                  # doctest: +SKIP
    >>> rep = Replier("tcp://*:5558", CF64)                 # doctest: +SKIP
    >>> req = Requester("tcp://localhost:5558", CF64)       # doctest: +SKIP
    >>> time.sleep(0.05)                                    # doctest: +SKIP
    >>> req.send(np.ones(4, dtype=np.complex128),           # doctest: +SKIP
    ...          sample_rate=int(1e6))                      # doctest: +SKIP
    >>> request, hdr = rep.recv(timeout_ms=2000)            # doctest: +SKIP
    >>> rep.send(request)                                   # doctest: +SKIP
    >>> reply, hdr2 = req.recv(timeout_ms=2000)             # doctest: +SKIP
    >>> req.close(); rep.close()                            # doctest: +SKIP
    """

    def __init__(self, endpoint: str, sample_type: int = ...) -> None:
        """Create a Requester socket and connect to ``endpoint``.

        Parameters
        ----------
        endpoint : str
            ZMQ endpoint to connect to, e.g.
            ``"tcp://localhost:5558"``.
        sample_type : int, optional
            Wire encoding of frames sent by this socket:
            :data:`CI32`, :data:`CF64` (default), or :data:`CF128`.
        """
        ...

    def __enter__(self) -> "Requester":
        """Return ``self`` for use as a context manager.

        Examples
        --------
        >>> from doppler.stream import Requester, CF64
        >>> with Requester("tcp://localhost:19118", CF64) as req:
        ...     type(req).__name__
        'Requester'

        """
        ...

    def __exit__(self, *args: object) -> None:
        """Call :meth:`close` on context-manager exit."""
        ...

    def send(
        self,
        samples: NDArray[Any],
        sample_rate: float = 0,
        center_freq: float = 0,
    ) -> None:
        """Send a request frame to the connected :class:`Replier`.

        The ZMQ REQ FSM requires that each :meth:`send` be followed by
        exactly one :meth:`recv` before the next send.  Calling
        ``send`` twice in a row raises a ZMQ error.

        Parameters
        ----------
        samples : ndarray
            C-contiguous array whose dtype must match the socket's
            ``sample_type``: ``numpy.complex128`` for :data:`CF64`,
            ``numpy.clongdouble`` for :data:`CF128`, ``numpy.int32``
            for :data:`CI32`.
        sample_rate : float, optional
            Samples per second written into the header (default 0).
        center_freq : float, optional
            Centre frequency in Hz written into the header (default 0).

        Raises
        ------
        TypeError
            If ``samples.dtype`` does not match the socket's
            ``sample_type``.
        ValueError
            If ``samples`` is not C-contiguous.
        RuntimeError
            If the ZMQ send fails (including FSM violation).

        Examples
        --------
        >>> from doppler.stream import Requester, CF64   # doctest: +SKIP
        >>> import numpy as np                           # doctest: +SKIP
        >>> req = Requester("tcp://localhost:5558", CF64)# doctest: +SKIP
        >>> req.send(np.ones(4, dtype=np.complex128),    # doctest: +SKIP
        ...          sample_rate=int(1e6))               # doctest: +SKIP
        """
        ...

    def recv(
        self, timeout_ms: int = -1
    ) -> Tuple[NDArray[Any], Dict[str, Any]]:
        """Receive the reply frame from the :class:`Replier`.

        Must be called after :meth:`send`; calling without a prior
        ``send`` triggers a ZMQ FSM error.  Blocks until the reply
        arrives or the timeout expires.

        Parameters
        ----------
        timeout_ms : int, optional
            Milliseconds to wait before raising :exc:`TimeoutError`.
            ``-1`` (default) blocks indefinitely.

        Returns
        -------
        samples : ndarray
            Decoded reply data.  dtype mirrors the :class:`Replier`'s
            ``sample_type``.
        header : dict
            Decoded ``dp_header_t`` fields — see
            :meth:`Subscriber.recv` for the full key list.

        Raises
        ------
        TimeoutError
            If ``timeout_ms`` elapses before the reply arrives.
        RuntimeError
            If the ZMQ recv fails (including FSM violation from
            calling ``recv`` without a prior ``send``).

        Examples
        --------
        >>> from doppler.stream import Requester, CF64   # doctest: +SKIP
        >>> req = Requester("tcp://localhost:5558", CF64)# doctest: +SKIP
        >>> req.send(np.ones(4, dtype=np.complex128))    # doctest: +SKIP
        >>> reply, hdr = req.recv(timeout_ms=2000)       # doctest: +SKIP
        """
        ...

    def close(self) -> None:
        """Destroy the ZMQ socket and release all resources.

        Calls ``dp_req_destroy()``.  Safe to call multiple times.

        Examples
        --------
        >>> from doppler.stream import Requester, CF64
        >>> req = Requester("tcp://localhost:19119", CF64)
        >>> req.close()
        >>> req.close()  # idempotent — no error

        """
        ...


class Replier:
    """ZMQ REP socket — receives a request frame, then sends a reply.

    Wraps ``dp_rep_t``.  The REQ/REP pattern is strictly alternating:
    :meth:`recv` must be called first to consume a request, then
    :meth:`send` emits the reply.  Violating this order triggers a ZMQ
    FSM error.

    Complements :class:`Requester`.  Use for control-plane responses or
    signal-frame RPC where the Replier processes each frame and returns
    a result synchronously.

    The socket *binds* to ``endpoint``; Requesters connect to it.

    Parameters
    ----------
    endpoint : str
        ZMQ endpoint to bind, e.g. ``"tcp://*:5558"``.
    sample_type : int
        Wire encoding of frames *sent* by this socket (the reply).  One
        of :data:`CI32`, :data:`CF64` (default), :data:`CF128`.  The
        request frame's type is determined by the :class:`Requester`.

    Raises
    ------
    ValueError
        If ``sample_type`` is not one of the three supported values.
    RuntimeError
        If the ZMQ context or socket cannot be created.

    Examples
    --------
    >>> from doppler.stream import Replier, CF64
    >>> rep = Replier("tcp://*:19120", CF64)
    >>> type(rep).__name__
    'Replier'
    >>> rep.close()

    Context-manager form:

    >>> with Replier("tcp://*:19121", CF64) as rep:
    ...     type(rep).__name__
    'Replier'

    Full REQ/REP server loop (requires a live :class:`Requester`):

    >>> from doppler.stream import Requester            # doctest: +SKIP
    >>> import numpy as np, time                        # doctest: +SKIP
    >>> rep = Replier("tcp://*:5558", CF64)             # doctest: +SKIP
    >>> req = Requester("tcp://localhost:5558", CF64)   # doctest: +SKIP
    >>> time.sleep(0.05)                                # doctest: +SKIP
    >>> req.send(np.ones(4, dtype=np.complex128))       # doctest: +SKIP
    >>> request, hdr = rep.recv(timeout_ms=2000)        # doctest: +SKIP
    >>> rep.send(request, sample_rate=hdr["sample_rate"])# doctest: +SKIP
    >>> reply, _ = req.recv(timeout_ms=2000)            # doctest: +SKIP
    >>> req.close(); rep.close()                        # doctest: +SKIP
    """

    def __init__(self, endpoint: str, sample_type: int = ...) -> None:
        """Create a Replier socket and bind to ``endpoint``.

        Parameters
        ----------
        endpoint : str
            ZMQ endpoint to bind, e.g. ``"tcp://*:5558"``.
        sample_type : int, optional
            Wire encoding of reply frames sent by this socket:
            :data:`CI32`, :data:`CF64` (default), or :data:`CF128`.
        """
        ...

    def __enter__(self) -> "Replier":
        """Return ``self`` for use as a context manager.

        Examples
        --------
        >>> from doppler.stream import Replier, CF64
        >>> with Replier("tcp://*:19122", CF64) as rep:
        ...     type(rep).__name__
        'Replier'

        """
        ...

    def __exit__(self, *args: object) -> None:
        """Call :meth:`close` on context-manager exit."""
        ...

    def recv(
        self, timeout_ms: int = -1
    ) -> Tuple[NDArray[Any], Dict[str, Any]]:
        """Receive one request frame from the :class:`Requester`.

        Must be called before :meth:`send`; calling ``send`` before
        ``recv`` triggers a ZMQ FSM error.  Blocks until a request
        arrives or the timeout expires.

        Parameters
        ----------
        timeout_ms : int, optional
            Milliseconds to wait before raising :exc:`TimeoutError`.
            ``-1`` (default) blocks indefinitely.

        Returns
        -------
        samples : ndarray
            Decoded request data.  dtype mirrors the
            :class:`Requester`'s ``sample_type``.
        header : dict
            Decoded ``dp_header_t`` fields — see
            :meth:`Subscriber.recv` for the full key list.

        Raises
        ------
        TimeoutError
            If ``timeout_ms`` elapses before a request arrives.
        RuntimeError
            If the ZMQ recv fails (including FSM violation from
            calling ``recv`` twice in a row).

        Examples
        --------
        >>> from doppler.stream import Replier, CF64    # doctest: +SKIP
        >>> rep = Replier("tcp://*:5558", CF64)         # doctest: +SKIP
        >>> request, hdr = rep.recv(timeout_ms=5000)    # doctest: +SKIP
        >>> rep.send(request,                           # doctest: +SKIP
        ...          sample_rate=hdr["sample_rate"])    # doctest: +SKIP
        """
        ...

    def send(
        self,
        samples: NDArray[Any],
        sample_rate: float = 0,
        center_freq: float = 0,
    ) -> None:
        """Send the reply frame back to the :class:`Requester`.

        Must be called after :meth:`recv`; calling without a prior
        ``recv`` triggers a ZMQ FSM error.  After this call returns,
        the Replier is ready to ``recv`` the next request.

        Parameters
        ----------
        samples : ndarray
            C-contiguous array whose dtype must match the socket's
            ``sample_type``: ``numpy.complex128`` for :data:`CF64`,
            ``numpy.clongdouble`` for :data:`CF128`, ``numpy.int32``
            for :data:`CI32`.
        sample_rate : float, optional
            Samples per second written into the reply header
            (default 0).
        center_freq : float, optional
            Centre frequency in Hz written into the reply header
            (default 0).

        Raises
        ------
        TypeError
            If ``samples.dtype`` does not match the socket's
            ``sample_type``.
        ValueError
            If ``samples`` is not C-contiguous.
        RuntimeError
            If the ZMQ send fails (including FSM violation from
            calling ``send`` before ``recv``).

        Examples
        --------
        >>> from doppler.stream import Replier, CF64    # doctest: +SKIP
        >>> import numpy as np                          # doctest: +SKIP
        >>> rep = Replier("tcp://*:5558", CF64)         # doctest: +SKIP
        >>> request, hdr = rep.recv(timeout_ms=5000)    # doctest: +SKIP
        >>> rep.send(np.zeros_like(request),            # doctest: +SKIP
        ...          sample_rate=hdr["sample_rate"])    # doctest: +SKIP
        """
        ...

    def close(self) -> None:
        """Destroy the ZMQ socket and release all resources.

        Calls ``dp_rep_destroy()``.  Safe to call multiple times.

        Examples
        --------
        >>> from doppler.stream import Replier, CF64
        >>> rep = Replier("tcp://*:19123", CF64)
        >>> rep.close()
        >>> rep.close()  # idempotent — no error

        """
        ...
