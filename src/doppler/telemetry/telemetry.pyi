# telemetry/telemetry.pyi — type stubs for the telemetry C extension.
import numpy as np

def capture(
    ring: int = 65536,
    decim: int = 1,
    clock: object | None = None,
    **objects: object,
) -> Telemetry:
    """Attach telemetry to every object given, and return the context.

    ``set_telemetry`` already registers *all* of an object's probes and
    forwards to its children -- one attach on an ``MpskReceiver`` gets
    11. What hurt was the ceremony around it. The **keyword becomes the
    probe prefix**, so the naming stays explicit rather than guessed
    from a class name.

    Parameters
    ----------
    ring : int, default 65536
        Ring capacity in records (~1 MiB). Generous on purpose:
        over-allocating a few MB is cheaper than either of the
        alternatives -- deriving a size from a duration hint trades one
        guess for two, and growing on demand would break the lock-free
        SPSC invariant.
    decim : int, default 1
        Emit every ``decim``-th event, on every probe attached here.
        1 is every event.
    clock : wfm.SampleClock, optional
        The pipeline's sample clock. **Not** an ``fs``/``t0`` pair: the
        time base belongs to the data and already exists as a doppler
        primitive, so telemetry carries a reference to the clock rather
        than re-declaring one. Left unset it stays ``None`` -- an
        ordinal axis, never an invented one.
    **objects
        The objects to attach; each keyword becomes that object's probe
        prefix. ``ring``, ``decim`` and ``clock`` are reserved, so an
        object named one of those must use :meth:`Telemetry.probe` and
        ``set_telemetry`` directly.

    Returns
    -------
    Telemetry
        The context, with every object already attached.

    Raises
    ------
    TypeError
        If any object is passed positionally -- the keyword is the
        prefix, so there is nothing to infer from a bare argument.

    Examples
    --------
    >>> from doppler.telemetry import capture
    >>> from doppler.track import MpskReceiver
    >>> rx = MpskReceiver(m=4, sps=8, m_out=4)
    >>> tlm = capture(rx=rx)          # every probe, prefixed "rx."
    >>> len(tlm.probe_names())
    11
    >>> sorted(tlm.probe_names())[:2]
    ['rx.car.e', 'rx.car.freq']

    """

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

    def read_dict(
        self, index: bool = False, max_records: int = -1
    ) -> dict[str, np.ndarray] | dict[str, tuple[np.ndarray, np.ndarray]]:
        """Drain like :meth:`read`, grouped by probe **name**.

        The same single drain and the same SPSC contract; only the shape
        differs.  This replaces the ``recs[recs["probe"] == tlm.probe_id
        (name)]["value"]`` filter (plus the ``{v: k for k, v in ...}``
        id→name inversion beside it) that every consumer was rewriting.

        Parameters
        ----------
        index : bool, default False
            Also return each record's stamped sample index, as
            ``{name: (n, values)}``.  That is what lets a plot carry a
            real time axis — ``n / fs`` in seconds — instead of an
            ordinal.  See :meth:`set_now`, which is what stamps ``n``.
        max_records : int, default -1
            Drain at most this many records; -1 takes everything
            available.

        Returns
        -------
        dict
            ``{name: values}`` (float32), or ``{name: (n, values)}`` with
            ``index=True`` (uint64, float32).  **Every registered probe
            gets a key**, including probes with nothing in this batch —
            an empty array — so the dict's shape does not change from
            call to call while draining block by block.

        Examples
        --------
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> e, lock = tlm.probe("sync.e"), tlm.probe("rx.lock")
        >>> tlm.set_now(1024)
        >>> for i in range(3):
        ...     tlm.emit(e, i / 10)
        >>> series = tlm.read_dict(index=True)
        >>> sorted(series)                    # a key per probe, always
        ['rx.lock', 'sync.e']
        >>> n, v = series["sync.e"]
        >>> [round(float(x), 1) for x in v]
        [0.0, 0.1, 0.2]
        >>> [int(x) for x in n]               # the stamped sample index
        [1024, 1024, 1024]
        >>> series["rx.lock"][1].size          # registered, nothing sent
        0

        """

    def set_decim(self, name: str, decim: int) -> None:
        """Emit every ``decim``-th event for one probe.

        Thins one noisy series while its siblings stay at full rate.
        Raises ``KeyError`` if the probe is not registered, and
        ``ValueError`` if ``decim`` is 0.

        Examples
        --------
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> e = tlm.probe("sync.e")
        >>> tlm.set_decim("sync.e", 3)
        >>> for i in range(9):
        ...     tlm.emit(e, i)
        >>> [float(v) for v in tlm.read_dict()["sync.e"]]  # every 3rd
        [0.0, 3.0, 6.0]

        """

    def stats(self) -> dict:
        """Reconcile what the probes emitted against what was dropped.

        Returns
        -------
        dict
            ``{"emitted": {name: count}, "dropped": int,
            "capacity": int, "probes": int}``.  ``dropped`` is
            ring-wide, not per probe: a record lost to overrun no longer
            knows which probe it came from.

        Examples
        --------
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> a = tlm.probe("agc.gain_db")
        >>> tlm.emit(a, -3.0)
        >>> s = tlm.stats()
        >>> s["emitted"], s["dropped"], s["probes"]
        ({'agc.gain_db': 1}, 0, 1)

        """

    def emitted(self, probe_id: int) -> int:
        """Records written for this probe (post-decimation, post-drop).

        Reconcile against :attr:`dropped` to account for losses, or call
        :meth:`stats` for both at once.
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
