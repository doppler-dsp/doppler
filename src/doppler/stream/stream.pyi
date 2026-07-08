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
    """NATS core PUB — one-to-many broadcast of signal frames.

    Wraps ``dp_pub_t``.  Each :meth:`send` call stages one
    ``[dp_header_t][raw data]`` buffer and publishes it to the
    endpoint's ``iq.<subject>.<type>`` NATS subject.  Multiple
    :class:`Subscriber` sockets can subscribe to the same subject;
    each subscriber receives every frame.  NATS core has no
    persistence or back-pressure: a slow subscriber simply misses
    frames published while it isn't reading.

    The endpoint identifies a subject on a NATS broker; both
    Publisher and Subscriber connect to the same ``nats-server`` (no
    bind/connect distinction — the broker mediates fan-out).

    Parameters
    ----------
    endpoint : str
        NATS endpoint, e.g. ``"nats://127.0.0.1:4222/iq"``
        (``host:port/subject``; the subject defaults to ``"default"``
        if omitted). Requires a running ``nats-server``.
    sample_type : int
        Wire encoding.  One of :data:`CI32`, :data:`CF64` (default),
        :data:`CF128`.  Raises :exc:`ValueError` for any other value.

    Raises
    ------
    ValueError
        If ``sample_type`` is not one of the three supported values.
    RuntimeError
        If ``nats-server`` isn't reachable at ``endpoint``.

    Examples
    --------
    Construct, use as a context manager, and verify the type:

    >>> from doppler.stream import Publisher, CF64
    >>> pub = Publisher("nats://127.0.0.1:4222/t19100", CF64)
    >>> type(pub).__name__
    'Publisher'
    >>> pub.close()

    Context-manager form (preferred — ensures close on exception):

    >>> with Publisher("nats://127.0.0.1:4222/t19101", CF64) as pub:
    ...     type(pub).__name__
    'Publisher'

    Full send round-trip with a :class:`Subscriber` (requires a live
    ``nats-server`` and a brief warm-up sleep so the subscription is
    established before the first publish):

    >>> import numpy as np, time                        # doctest: +SKIP
    >>> from doppler.stream import Subscriber           # doctest: +SKIP
    >>> pub = Publisher("nats://127.0.0.1:4222/iq", CF64)           # doctest: +SKIP
    >>> sub = Subscriber("nats://127.0.0.1:4222/iq")        # doctest: +SKIP
    >>> time.sleep(0.1)                                 # doctest: +SKIP
    >>> pub.send(np.array([1+2j, 3+4j],                # doctest: +SKIP
    ...          dtype=np.complex128),                  # doctest: +SKIP
    ...          sample_rate=int(1e6),                  # doctest: +SKIP
    ...          center_freq=int(2.4e9))                # doctest: +SKIP
    >>> samples, hdr = sub.recv(timeout_ms=2000)        # doctest: +SKIP
    >>> pub.close(); sub.close()                        # doctest: +SKIP
    """

    def __init__(self, endpoint: str, sample_type: int = ...) -> None:
        """Create a Publisher and connect to ``endpoint``'s NATS broker.

        Parameters
        ----------
        endpoint : str
            NATS endpoint, e.g. ``"nats://127.0.0.1:4222/iq"``.
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
        >>> with Publisher("nats://127.0.0.1:4222/t19102", CF64) as pub:
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
        per-socket monotonically increasing ``sequence`` number), stages
        one ``[header][raw sample bytes]`` buffer (NATS has no scatter/
        gather send, so header and data are copied into one contiguous
        buffer first), and publishes it.  The call releases the GIL
        while blocked in the underlying NATS client, so other Python
        threads can run concurrently.

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
            If the publish fails (e.g. the broker connection dropped).

        Examples
        --------
        >>> from doppler.stream import Publisher, CF64  # doctest: +SKIP
        >>> import numpy as np, time                    # doctest: +SKIP
        >>> pub = Publisher("nats://127.0.0.1:4222/iq", CF64)       # doctest: +SKIP
        >>> pub.send(np.ones(4, dtype=np.complex128),   # doctest: +SKIP
        ...          sample_rate=int(48000),             # doctest: +SKIP
        ...          center_freq=int(433e6))             # doctest: +SKIP
        >>> pub.close()                                 # doctest: +SKIP
        """
        ...

    def close(self) -> None:
        """Destroy the underlying NATS handle and release all resources.

        Calls ``dp_pub_destroy()``.  Safe to call multiple times —
        subsequent calls are no-ops.  After ``close()`` the object
        must not be used for sending.

        Examples
        --------
        >>> from doppler.stream import Publisher, CF64
        >>> pub = Publisher("nats://127.0.0.1:4222/t19103", CF64)
        >>> pub.close()
        >>> pub.close()  # idempotent — no error

        """
        ...


class Subscriber:
    """NATS core SUB — receives signal frames from one or more Publishers.

    Wraps ``dp_sub_t``.  Subscribes to the endpoint's ``iq.<subject>.>``
    NATS subject, so it receives every frame any :class:`Publisher` bound
    to that subject sends (fan-out: every Subscriber on the subject gets
    every frame).  There is no "connect to exactly one Publisher" concept
    — the broker mediates delivery by subject, not by socket.

    For load-balanced consumption (each frame delivered to exactly one
    of several workers, with durable redelivery) use :class:`Pull`
    instead, which is backed by NATS JetStream rather than NATS core.

    The recv path is zero-copy: the returned NumPy array's memory is
    owned by an internal ``dp_msg_t`` handle that points into the
    received NATS message; the buffer is freed only when the array
    (and all views of it) are garbage-collected.

    Parameters
    ----------
    endpoint : str
        NATS endpoint, e.g. ``"nats://127.0.0.1:4222/iq"``
        (``host:port/subject``; the subject defaults to ``"default"``
        if omitted). Requires a running ``nats-server``.

    Raises
    ------
    RuntimeError
        If ``nats-server`` isn't reachable at ``endpoint``.

    Examples
    --------
    >>> from doppler.stream import Subscriber
    >>> sub = Subscriber("nats://127.0.0.1:4222/t19104")
    >>> type(sub).__name__
    'Subscriber'
    >>> sub.close()

    Context-manager form:

    >>> with Subscriber("nats://127.0.0.1:4222/t19105") as sub:
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
            NATS endpoint, e.g.
            ``"nats://127.0.0.1:4222/iq"``.
        """
        ...

    def __enter__(self) -> "Subscriber":
        """Return ``self`` for use as a context manager.

        Examples
        --------
        >>> from doppler.stream import Subscriber
        >>> with Subscriber("nats://127.0.0.1:4222/t19106") as sub:
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
        The returned NumPy array is a zero-copy view into the received
        NATS message buffer; the buffer is freed when the array is
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
            If the underlying NATS recv fails for any other reason.

        Examples
        --------
        >>> from doppler.stream import Subscriber          # doctest: +SKIP
        >>> with Subscriber("nats://127.0.0.1:4222/iq") as sub:# doctest: +SKIP
        ...     samples, hdr = sub.recv(timeout_ms=1000)  # doctest: +SKIP
        ...     print(samples.dtype, hdr["sample_rate"])  # doctest: +SKIP
        """
        ...

    def close(self) -> None:
        """Destroy the underlying NATS handle and release all resources.

        Calls ``dp_sub_destroy()``.  Safe to call multiple times.

        Examples
        --------
        >>> from doppler.stream import Subscriber
        >>> sub = Subscriber("nats://127.0.0.1:4222/t19107")
        >>> sub.close()
        >>> sub.close()  # idempotent — no error

        """
        ...


class Push:
    """NATS JetStream work-queue producer — durable pipeline sender.

    Wraps ``dp_push_t``.  Each :meth:`send` is a synchronous, server-
    acked JetStream publish: the frame is persisted (and replicated, on
    a clustered broker) before the call returns, so a producer crash
    never silently drops a frame.  Each persisted frame is later
    delivered to exactly one :class:`Pull` worker (competing-consumers
    distribution), unlike :class:`Publisher` which fans out to every
    Subscriber.

    Use the PUSH/PULL pattern when you have a pool of workers pulling
    from a shared, durable queue and want at-least-once delivery with
    redelivery on worker crash (see :meth:`Pull.ack`).

    Both ends address the same NATS work-queue subject; there is no
    bind/connect distinction (the broker mediates delivery).

    Parameters
    ----------
    endpoint : str
        NATS endpoint, e.g. ``"nats://127.0.0.1:4222/work"``.
    sample_type : int
        Wire encoding.  One of :data:`CI32`, :data:`CF64` (default),
        :data:`CF128`.  Raises :exc:`ValueError` for any other value.

    Raises
    ------
    ValueError
        If ``sample_type`` is not one of the three supported values.
    RuntimeError
        If ``nats-server`` isn't reachable at ``endpoint``.

    Examples
    --------
    >>> from doppler.stream import Push, CF64
    >>> push = Push("nats://127.0.0.1:4222/t19108", CF64)
    >>> type(push).__name__
    'Push'
    >>> push.close()

    Context-manager form:

    >>> with Push("nats://127.0.0.1:4222/t19109", CF64) as push:
    ...     type(push).__name__
    'Push'

    Round-trip with a :class:`Pull` worker (requires a live connection):

    >>> import numpy as np, time                        # doctest: +SKIP
    >>> from doppler.stream import Pull                 # doctest: +SKIP
    >>> push = Push("nats://127.0.0.1:4222/work", CF64)       # doctest: +SKIP
    >>> pull = Pull("nats://127.0.0.1:4222/work")             # doctest: +SKIP
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
            NATS endpoint, e.g. ``"nats://127.0.0.1:4222/work"``.
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
        >>> with Push("nats://127.0.0.1:4222/t19110", CF64) as push:
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
        """Durably publish one block of samples to the work-queue.

        Blocks until the broker acks that the frame is persisted (and
        replicated, on a clustered broker) — not until a Pull worker is
        ready; the frame waits in the queue for the next available
        worker. Releases the GIL while blocked on the broker round trip.

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
            If the publish fails (e.g. the broker connection dropped).

        Examples
        --------
        >>> from doppler.stream import Push, Pull, CF64  # doctest: +SKIP
        >>> import numpy as np, time                     # doctest: +SKIP
        >>> push = Push("nats://127.0.0.1:4222/work", CF64)    # doctest: +SKIP
        >>> pull = Pull("nats://127.0.0.1:4222/work")          # doctest: +SKIP
        >>> time.sleep(0.05)                             # doctest: +SKIP
        >>> push.send(np.array([1+2j, 3+4j],            # doctest: +SKIP
        ...           dtype=np.complex128),              # doctest: +SKIP
        ...           sample_rate=int(48000))            # doctest: +SKIP
        >>> samples, hdr = pull.recv(timeout_ms=2000)   # doctest: +SKIP
        >>> push.close(); pull.close()                  # doctest: +SKIP
        """
        ...

    def close(self) -> None:
        """Destroy the underlying NATS handle and release all resources.

        Calls ``dp_push_destroy()``.  Safe to call multiple times.

        Examples
        --------
        >>> from doppler.stream import Push, CF64
        >>> push = Push("nats://127.0.0.1:4222/t19111", CF64)
        >>> push.close()
        >>> push.close()  # idempotent — no error

        """
        ...


class Pull:
    """NATS JetStream work-queue consumer — durable pipeline receiver.

    Wraps ``dp_pull_t``.  Consumes frames persisted by one or more
    :class:`Push` producers.  Multiple Pull workers can share the same
    durable consumer group; each frame is delivered to exactly one
    worker.  Call :meth:`ack` once a frame is fully processed — an
    un-acked frame is redelivered if the worker dies first.

    The recv path is zero-copy: see :class:`Subscriber` for the buffer
    lifetime semantics.

    Both ends address the same NATS work-queue subject; there is no
    bind/connect distinction (the broker mediates delivery).

    Parameters
    ----------
    endpoint : str
        NATS endpoint, e.g. ``"nats://127.0.0.1:4222/work"``. Requires
        a running ``nats-server -js`` (JetStream enabled).

    Raises
    ------
    RuntimeError
        If ``nats-server`` isn't reachable at ``endpoint``.

    Examples
    --------
    >>> from doppler.stream import Push, Pull, CF64
    >>> _ = Push("nats://127.0.0.1:4222/t19112", CF64)  # provisions the queue
    >>> pull = Pull("nats://127.0.0.1:4222/t19112")
    >>> type(pull).__name__
    'Pull'
    >>> pull.close()

    Context-manager form:

    >>> _ = Push("nats://127.0.0.1:4222/t19113", CF64)  # provisions the queue
    >>> with Pull("nats://127.0.0.1:4222/t19113") as pull:
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
            NATS endpoint, e.g.
            ``"nats://127.0.0.1:4222/work"``.
        """
        ...

    def __enter__(self) -> "Pull":
        """Return ``self`` for use as a context manager.

        Examples
        --------
        >>> from doppler.stream import Push, Pull, CF64
        >>> _ = Push("nats://127.0.0.1:4222/t19114", CF64)  # provisions queue
        >>> with Pull("nats://127.0.0.1:4222/t19114") as pull:
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
        The returned NumPy array is a zero-copy view into the received
        NATS message buffer; the buffer is freed when the array is
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
            If the underlying NATS recv fails for any other reason.

        Examples
        --------
        >>> from doppler.stream import Pull             # doctest: +SKIP
        >>> with Pull("nats://127.0.0.1:4222/work") as pull:  # doctest: +SKIP
        ...     samples, hdr = pull.recv(timeout_ms=1000)# doctest: +SKIP
        ...     print(samples.dtype, hdr["num_samples"])# doctest: +SKIP
        """
        ...

    def ack(self, samples: NDArray[Any]) -> None:
        """Acknowledge a frame consumed from the JetStream work-queue.

        Delivery is at-least-once: a frame stays pending until acked
        and is redelivered to another worker if this one dies first.
        Pass the array returned by :meth:`recv` once it has been fully
        processed, then drop the array.

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
        """Destroy the underlying NATS handle and release all resources.

        Calls ``dp_pull_destroy()``.  Safe to call multiple times.

        Examples
        --------
        >>> from doppler.stream import Push, Pull, CF64
        >>> _ = Push("nats://127.0.0.1:4222/t19115", CF64)  # provisions queue
        >>> pull = Pull("nats://127.0.0.1:4222/t19115")
        >>> pull.close()
        >>> pull.close()  # idempotent — no error

        """
        ...


class Requester:
    """NATS request/reply — sends a request frame, then waits for a reply.

    Wraps ``dp_req_t``.  Built on a NATS request: :meth:`send` publishes
    to the endpoint's subject with a reply-to address (a dedicated
    inbox created for this Requester), and :meth:`recv` waits on that
    same inbox for the :class:`Replier`'s answer.  Use this pattern
    strictly alternating (send, then recv, then send again) — unlike
    ZMQ's REQ socket there is no enforced state machine, but calling
    ``recv`` before a matching ``send`` simply blocks/times out on an
    empty inbox rather than raising immediately.

    Complements :class:`Replier`.  Use this pattern for control-plane
    messages (tuning commands, metadata queries) or synchronous
    signal-frame RPC where one peer processes each frame and returns a
    result.

    Both ends address the same NATS subject; there is no bind/connect
    distinction (the broker mediates delivery via the reply-to inbox).

    Parameters
    ----------
    endpoint : str
        NATS endpoint, e.g. ``"nats://127.0.0.1:4222/ctrl"``.
    sample_type : int
        Wire encoding of frames *sent* by this socket.  One of
        :data:`CI32`, :data:`CF64` (default), :data:`CF128`.  The
        reply frame's type is determined by the :class:`Replier`.

    Raises
    ------
    ValueError
        If ``sample_type`` is not one of the three supported values.
    RuntimeError
        If ``nats-server`` isn't reachable at ``endpoint``.

    Examples
    --------
    >>> from doppler.stream import Requester, CF64
    >>> req = Requester("nats://127.0.0.1:4222/t19116", CF64)
    >>> type(req).__name__
    'Requester'
    >>> req.close()

    Context-manager form:

    >>> with Requester("nats://127.0.0.1:4222/t19117", CF64) as req:
    ...     type(req).__name__
    'Requester'

    Full REQ/REP round-trip (requires a live :class:`Replier`):

    >>> import numpy as np, time                            # doctest: +SKIP
    >>> from doppler.stream import Replier                  # doctest: +SKIP
    >>> rep = Replier("nats://127.0.0.1:4222/ctrl", CF64)                 # doctest: +SKIP
    >>> req = Requester("nats://127.0.0.1:4222/ctrl", CF64)       # doctest: +SKIP
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
            NATS endpoint, e.g.
            ``"nats://127.0.0.1:4222/ctrl"``.
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
        >>> with Requester("nats://127.0.0.1:4222/t19118", CF64) as req:
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
        """Publish a request frame with this Requester's reply-to inbox.

        Use strictly alternating with :meth:`recv` (send, recv, send,
        recv, ...) — sending again before consuming the previous reply
        works mechanically but leaves an unread message in the inbox
        for the next :meth:`recv` to pick up, which will desync the
        request/reply pairing.

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
            If the publish fails (e.g. the broker connection dropped).

        Examples
        --------
        >>> from doppler.stream import Requester, CF64   # doctest: +SKIP
        >>> import numpy as np                           # doctest: +SKIP
        >>> req = Requester("nats://127.0.0.1:4222/ctrl", CF64)# doctest: +SKIP
        >>> req.send(np.ones(4, dtype=np.complex128),    # doctest: +SKIP
        ...          sample_rate=int(1e6))               # doctest: +SKIP
        """
        ...

    def recv(
        self, timeout_ms: int = -1
    ) -> Tuple[NDArray[Any], Dict[str, Any]]:
        """Receive the reply frame from the :class:`Replier`.

        Waits on this Requester's reply-to inbox.  Call after
        :meth:`send`; calling without a prior ``send`` just blocks (or
        times out) on an empty inbox rather than raising immediately.

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
            If ``timeout_ms`` elapses before a reply arrives (including
            calling ``recv`` without a prior ``send``).

        Examples
        --------
        >>> from doppler.stream import Requester, CF64   # doctest: +SKIP
        >>> req = Requester("nats://127.0.0.1:4222/ctrl", CF64)# doctest: +SKIP
        >>> req.send(np.ones(4, dtype=np.complex128))    # doctest: +SKIP
        >>> reply, hdr = req.recv(timeout_ms=2000)       # doctest: +SKIP
        """
        ...

    def close(self) -> None:
        """Destroy the underlying NATS handle and release all resources.

        Calls ``dp_req_destroy()``.  Safe to call multiple times.

        Examples
        --------
        >>> from doppler.stream import Requester, CF64
        >>> req = Requester("nats://127.0.0.1:4222/t19119", CF64)
        >>> req.close()
        >>> req.close()  # idempotent — no error

        """
        ...


class Replier:
    """NATS request/reply — receives a request frame, then sends a reply.

    Wraps ``dp_rep_t``.  Subscribes to the endpoint's subject; each
    :meth:`recv` captures the request's reply-to inbox, and the next
    :meth:`send` publishes the reply directly to that inbox.  Calling
    ``send`` before ``recv`` has captured a request raises
    :exc:`RuntimeError` immediately (there is no reply-to target yet)
    — unlike ZMQ's REP FSM, ``recv`` itself has no such restriction and
    simply waits for the next request.

    Complements :class:`Requester`.  Use for control-plane responses or
    signal-frame RPC where the Replier processes each frame and returns
    a result synchronously.

    Both ends address the same NATS subject; there is no bind/connect
    distinction (the broker mediates delivery via the reply-to inbox).

    Parameters
    ----------
    endpoint : str
        NATS endpoint, e.g. ``"nats://127.0.0.1:4222/ctrl"``.
    sample_type : int
        Wire encoding of frames *sent* by this socket (the reply).  One
        of :data:`CI32`, :data:`CF64` (default), :data:`CF128`.  The
        request frame's type is determined by the :class:`Requester`.

    Raises
    ------
    ValueError
        If ``sample_type`` is not one of the three supported values.
    RuntimeError
        If ``nats-server`` isn't reachable at ``endpoint``.

    Examples
    --------
    >>> from doppler.stream import Replier, CF64
    >>> rep = Replier("nats://127.0.0.1:4222/t19120", CF64)
    >>> type(rep).__name__
    'Replier'
    >>> rep.close()

    Context-manager form:

    >>> with Replier("nats://127.0.0.1:4222/t19121", CF64) as rep:
    ...     type(rep).__name__
    'Replier'

    Full REQ/REP server loop (requires a live :class:`Requester`):

    >>> from doppler.stream import Requester            # doctest: +SKIP
    >>> import numpy as np, time                        # doctest: +SKIP
    >>> rep = Replier("nats://127.0.0.1:4222/ctrl", CF64)             # doctest: +SKIP
    >>> req = Requester("nats://127.0.0.1:4222/ctrl", CF64)   # doctest: +SKIP
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
            NATS endpoint, e.g. ``"nats://127.0.0.1:4222/ctrl"``.
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
        >>> with Replier("nats://127.0.0.1:4222/t19122", CF64) as rep:
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
        """Receive one request frame from a :class:`Requester`.

        Blocks until a request arrives or the timeout expires. Captures
        the request's reply-to inbox so the next :meth:`send` reaches
        the right Requester.

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
            If the underlying NATS recv fails for any other reason.

        Examples
        --------
        >>> from doppler.stream import Replier, CF64    # doctest: +SKIP
        >>> rep = Replier("nats://127.0.0.1:4222/ctrl", CF64)         # doctest: +SKIP
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
        """Publish the reply to the request's captured reply-to inbox.

        Must be called after :meth:`recv`; calling without a prior
        ``recv`` raises :exc:`RuntimeError` immediately (there is no
        reply-to target yet). After this call returns, the Replier is
        ready to ``recv`` the next request.

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
            If ``send`` is called before ``recv`` has captured a
            request, or the publish otherwise fails.

        Examples
        --------
        >>> from doppler.stream import Replier, CF64    # doctest: +SKIP
        >>> import numpy as np                          # doctest: +SKIP
        >>> rep = Replier("nats://127.0.0.1:4222/ctrl", CF64)         # doctest: +SKIP
        >>> request, hdr = rep.recv(timeout_ms=5000)    # doctest: +SKIP
        >>> rep.send(np.zeros_like(request),            # doctest: +SKIP
        ...          sample_rate=hdr["sample_rate"])    # doctest: +SKIP
        """
        ...

    def close(self) -> None:
        """Destroy the underlying NATS handle and release all resources.

        Calls ``dp_rep_destroy()``.  Safe to call multiple times.

        Examples
        --------
        >>> from doppler.stream import Replier, CF64
        >>> rep = Replier("nats://127.0.0.1:4222/t19123", CF64)
        >>> rep.close()
        >>> rep.close()  # idempotent — no error

        """
        ...
