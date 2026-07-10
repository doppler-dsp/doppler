# telemetry/telemetry.pyi — type stubs for the telemetry C extension.
import numpy as np

class Telemetry:
    """Scalar telemetry context: probe registry + lock-free record ring.

    A ``Telemetry`` wraps a C ``dp_tlm_t`` (see
    ``docs/design/telemetry.md``): a fixed-capacity table of named probes
    plus a lock-free single-producer / single-consumer ring of 16-byte
    records.  Instrumented C objects attach to it via their
    ``set_telemetry`` face and then publish scalars (loop stress, AGC
    gain, lock metrics) straight from their hot loops — one
    predicted-not-taken branch per event when detached, one ring write
    when attached.  The ring drops (and counts) on overrun, so a slow or
    absent reader can never stall the DSP thread.

    ``probe``/``emit``/``set_now`` are the producer side and must stay on
    one thread together with every attached object's stepping;
    ``read``/``dropped`` are the consumer side and may run on a different
    thread.  Register all probes before the producer starts.

    Parameters
    ----------
    ring_records : int, optional
        Requested ring capacity in records (default ``16384``).  Must be
        a power of two.  The ring's VM mirror is built at page
        granularity, so a sub-page request (fewer than 256 records on
        4 KiB pages) is rounded up; read :attr:`capacity` back for the
        size actually allocated.

    Examples
    --------
    >>> from doppler.telemetry import Telemetry
    >>> tlm = Telemetry(1 << 12)
    >>> tlm.capacity >= 1 << 12
    True
    >>> gid = tlm.probe("agc.gain_db", decim=1)
    >>> tlm.set_now(1000)
    >>> tlm.emit(gid, -3.5)
    >>> recs = tlm.read()
    >>> (int(recs["n"][0]), float(recs["value"][0]), int(recs["probe"][0]))
    (1000, -3.5, 0)
    >>> tlm.dropped
    0

    """

    capacity: int
    """Authoritative ring capacity in records (post page rounding)."""

    dropped: int
    """Total records dropped on ring overrun (monotonic)."""

    probe_count: int
    """Number of registered probes."""

    def __init__(self, ring_records: int = 16384) -> None: ...
    def probe(self, name: str, decim: int = 1) -> int:
        """Register (or re-register) a named probe and return its id.

        Registration is idempotent by name: re-registering an existing
        name returns the same id and updates the decimation, so an
        object can re-attach after a reset without churning ids.  The
        decimation phase is primed so the first event after registration
        always emits.  Setup path only — never call while the producer
        is stepping.

        Parameters
        ----------
        name : str
            Dotted probe path, e.g. ``"agc.gain_db"``.  At most 31
            characters.
        decim : int, optional
            Emit every ``decim``-th event (default 1 = every event).

        Returns
        -------
        int
            The probe id used in records' ``"probe"`` field.

        Raises
        ------
        ValueError
            Overlong name, ``decim == 0``, or the 64-entry probe table
            is full.

        Examples
        --------
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> tlm.probe("sync.e", decim=4)
        0
        >>> tlm.probe("sync.e")  # idempotent: same id
        0
        >>> tlm.probe_count
        1

        """

    def probe_id(self, name: str) -> int:
        """Look up a probe id by name.

        Raises
        ------
        KeyError
            If no probe with this name is registered.

        Examples
        --------
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> _ = tlm.probe("agc.gain_db")
        >>> tlm.probe_id("agc.gain_db")
        0

        """

    def probe_names(self) -> dict[str, int]:
        """Return the full ``name -> id`` map for registered probes.

        Examples
        --------
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> _ = tlm.probe("agc.gain_db")
        >>> _ = tlm.probe("sync.e")
        >>> tlm.probe_names()
        {'agc.gain_db': 0, 'sync.e': 1}

        """

    def emit(self, probe_id: int, value: float) -> None:
        """Record one scalar for a probe (producer side).

        For Python-side events and tests; instrumented C objects emit
        directly from their hot loops.  The value is narrowed to
        float32; the record is stamped with the current ``set_now``
        sample index.  Never blocks — on ring overrun the record is
        dropped and counted in :attr:`dropped`.

        Raises
        ------
        ValueError
            ``probe_id`` is not a registered probe.
        """

    def set_now(self, n: int) -> None:
        """Stamp the sample index carried by subsequent records.

        Producer side; call once per block from whoever owns the
        pipeline's sample clock.  If never called, records carry
        ``n == 0`` and consumers index by record order.
        """

    def read(self, max_records: int = -1) -> np.ndarray:
        """Drain records into a structured array.  Non-blocking.

        Consumer side — may run on a different thread than the
        producer.  Returns everything available (or up to
        ``max_records`` if given), possibly empty, in emission order.

        Returns
        -------
        numpy.ndarray
            Structured array with dtype
            ``[("n", "<u8"), ("value", "<f4"), ("probe", "<u2"),
            ("flags", "<u2")]`` — 16 bytes per row, the exact C record
            layout.

        Examples
        --------
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> eid = tlm.probe("sync.e")
        >>> for i in range(5):
        ...     tlm.emit(eid, i / 10)
        >>> recs = tlm.read()
        >>> recs.shape, recs.dtype.names
        ((5,), ('n', 'value', 'probe', 'flags'))
        >>> [round(float(v), 1) for v in recs["value"]]
        [0.0, 0.1, 0.2, 0.3, 0.4]
        >>> tlm.read().shape  # drained: empty now
        (0,)

        """

    def emitted(self, probe_id: int) -> int:
        """Records written for this probe (post-decimation, post-drop).

        Reconcile against :attr:`dropped` to account for losses.
        """

    def destroy(self) -> None:
        """Free the context now (idempotent).

        Detach any attached C objects first.  Further method calls
        raise ``RuntimeError``.
        """

    @property
    def _capsule(self) -> object:
        """PyCapsule borrowing the ``dp_tlm_t*`` — the attach point.

        Instrumented objects' ``set_telemetry`` bindings take this to
        attach to the context.  Non-owning: attached objects must not
        outlive the ``Telemetry``.
        """
