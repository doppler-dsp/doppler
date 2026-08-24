# telemetry/telemetry.pyi — type stubs for the telemetry C extension.
from typing import final
import numpy as np
from numpy.typing import NDArray

@final
class TelemetryStats(tuple[int, int, int, int]):
    """Context-wide telemetry counters, snapshotted together. Per-probe detail
    is probe_names + emitted(), which stay the SSOT for it.

    Attributes
    ----------
    dropped : int
        Records lost to ring overrun, monotonic over the context's lifetime.
    emitted : int
        Records written, summed over every probe.
    capacity : int
        Ring capacity in records.
    probes : int
        Registered probes.
    """

    @property
    def dropped(self) -> int:
        """Records lost to ring overrun, monotonic over the context's
        lifetime.
        """

    @property
    def emitted(self) -> int:
        """Records written, summed over every probe."""

    @property
    def capacity(self) -> int:
        """Ring capacity in records."""

    @property
    def probes(self) -> int:
        """Registered probes."""

@final
class Telemetry:
    """Creates a telemetry context with a ring of ring_records slots.

    Parameters
    ----------
    ring_records : int, default 16384
        Requested ring capacity in records. MUST be a power of 2. Sub-page
        requests are rounded up to the page minimum (buffer.h semantics) — read
        the authoritative value back with dp_tlm_capacity().

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``ring_records must be
        a power of two (and at least the page minimum); read the granted size
        back from Telemetry.capacity``.

    Examples
    --------
    Create with defaults:

    >>> from doppler.telemetry import Telemetry
    >>> obj = Telemetry(ring_records=16384)

    """
    def __init__(self, ring_records: int = ...) -> None: ...

    def read(
        self,
        n: int = 0,
        out: NDArray[Any] | None = None,
    ) -> NDArray[Any]:
        """Drains records into out. Non-blocking.

        Consumer side of the SPSC ring: safe to call from a different thread
        than the producer. Returns immediately with whatever is available
        (possibly 0) — never spins.

        Parameters
        ----------
        n : int
            Records wanted; 0 means "everything available".
        out : NDArray[Any] | None
            Destination.

        Returns
        -------
        NDArray[Any]
            Number of records copied out.

        Examples
        --------
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> eid = tlm.probe("sync.e")
        >>> for i in range(5):
        ...     tlm.emit(eid, i / 10)
        >>> recs = tlm.read(2)          # take two
        >>> recs.shape, recs.dtype.names
        ((2,), ('n', 'value', 'probe', 'flags'))
        >>> tlm.read().shape            # 0 means "everything left"
        (3,)
        >>> tlm.read().shape            # drained
        (0,)

        """

    def read_max_out(self) -> int:
        """Upper bound on what dp_tlm_read() can return right now.

        Simply the available count: a caller sizing a destination cannot know
        the

        request will be smaller, and jm's generated binding allocates this
        much,

        reads, then resizes to what actually came back.

        Returns
        -------
        int
            Output.
        """

    def probe(self, name: str, decim: int = 1) -> int:
        """Registers (or re-registers) a named probe. Setup path, not hot.

        Idempotent by name: registering an existing name returns its id and
        updates decim (re-attach after a reset keeps ids stable). The
        decimation phase is primed so the FIRST event after registration emits.

        Parameters
        ----------
        name : str
            Probe name, e.g. "agc.gain_db". Must be shorter than
            DP_TLM_NAME_MAX.
        decim : int
            Emit every decim-th event; >= 1.

        Returns
        -------
        int
            Probe id (>= 0), or DP_ERR_INVALID on NULL/overlong name, decim ==
            0, or a full table.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            ``probe: name is NULL or too long, decim is 0, or the table is
            full``, with the return code appended (gh-869).

        Examples
        --------
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> tlm.probe("sync.e", decim=4)
        0
        >>> tlm.probe("sync.e")     # same name: same id, decim retuned
        0
        >>> tlm.probe_count
        1

        """

    def probe_id(self, name: str) -> int:
        """Looks up a probe id by name; ::DP_ERR_INVALID if unknown.

        Parameters
        ----------
        name : str
            Probe name as passed to dp_tlm_probe().

        Returns
        -------
        int
            Probe id (>= 0), or ::DP_ERR_INVALID if no such probe.

        Raises
        ------
        KeyError
            If the C call returns a negative value. The exception message is
            ``no probe by that name``, with the return code appended (gh-869).

        Examples
        --------
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> _ = tlm.probe("agc.gain_db")
        >>> tlm.probe_id("agc.gain_db")
        0
        >>> tlm.probe_id("never.registered")
        Traceback (most recent call last):
        KeyError: 'no probe by that name (rc=-4)'

        """

    def set_decim(self, name: str, decim: int) -> None:
        """Retunes an EXISTING probe's decimation, by name.

        Distinct from dp_tlm_probe(), which registers on a miss: this refuses
        an unknown name rather than quietly creating a probe nothing emits to,
        which is what a typo in a retune call deserves.

        Parameters
        ----------
        name : str
            Name of an ALREADY registered probe.
        decim : int
            Emit every decim-th event; >= 1.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_decim failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> _ = tlm.probe("sync.e", decim=1)
        >>> tlm.set_decim("sync.e", 8)      # retune the existing probe
        >>> tlm.set_decim("typo.e", 8)      # refused, not silently created
        Traceback (most recent call last):
        ValueError: set_decim failed (rc=-4)

        """

    def emit(self, id: int, v: float) -> None:
        """Validating dp_tlm_emit(): refuses an id the registry never issued.

        The out-of-line twin of the inline hot-path emit, for callers whose id
        did not come from dp_tlm_probe() on this context — in practice, a
        language binding, where the id is whatever the caller passed.
        dp_tlm_emit() checks only the ARRAY bound (see its docs: checking
        n_probes there costs ~16% of the decimated path), so an in-range but
        unregistered id reaches it and emits a record against a probe nobody
        registered. Here that is an error.

        C hot loops keep calling dp_tlm_emit() directly and pay nothing for
        this.

        Parameters
        ----------
        id : int
            Probe id from dp_tlm_probe() on THIS context.
        v : float
            The scalar, narrowed to float by the ring record.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``emit failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> pid = tlm.probe("rx.snr_db")
        >>> tlm.emit(pid, 12.5)
        >>> float(tlm.read()[0]["value"])
        12.5

        An id the registry never issued is refused, not written:

        >>> tlm.emit(pid + 1, 1.0)
        Traceback (most recent call last):
        ValueError: emit failed (rc=-4)

        """

    def set_now(self, n: int) -> None:
        """Stamps the sample index carried by subsequent records, and — when a
        capture is open — closes out the block just finished.

        Call once per block from whoever owns the pipeline's sample clock
        (`dp_tlm_set_now (tlm, clk->n)`). NULL-safe so pipeline glue can call
        it unconditionally.

        Callers already place this at the top of the block loop, *before*
        stepping, which makes it exactly the boundary a lossless capture needs:
        delegating here drains the PREVIOUS block, leaving the ring empty as
        the next one starts. That is the invariant dp_tlm_block_bound() is
        sized against, so an existing `set_now / steps / read` loop becomes
        lossless by opening a capture and changing nothing else.

        With no capture open the behaviour is byte-identical to a bare
        assignment. The delegation is a cold branch on a per-block call, never
        a per-sample one, so it is nowhere near the hot loops dp_tlm_emit()
        cares about.

        Parameters
        ----------
        n : int
            Sample index stamped into every subsequent record.

        Examples
        --------
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> pid = tlm.probe("agc.gain_db")
        >>> tlm.set_now(1000)           # top of the block, before stepping
        >>> tlm.emit(pid, -3.5)
        >>> rec = tlm.read()[0]
        >>> int(rec["n"]), float(rec["value"])
        (1000, -3.5)

        """

    def emitted(self, id: int) -> int:
        """Records written for probe id (post-decimation, post-drop).

        Reconcile against dp_tlm_dropped() to account for losses: what a probe
        emitted is what reached the ring, not what the call sites offered it.

        Parameters
        ----------
        id : int
            Probe id from dp_tlm_probe().

        Returns
        -------
        int
            Records written for that probe, 0 for an unknown id.

        Examples
        --------
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> eid = tlm.probe("sync.e", decim=2)
        >>> for i in range(4):
        ...     tlm.emit(eid, i / 10)
        >>> tlm.emitted(eid)            # decim=2: half the events
        2
        >>> tlm.dropped
        0

        """

    def stats(self) -> TelemetryStats:
        """Snapshots the context's counters. Zeroed for a NULL context.

        Returns
        -------
        TelemetryStats
            The four counters as one ::dp_tlm_stats_t value.

        Examples
        --------
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> pid = tlm.probe("agc.gain_db")
        >>> tlm.emit(pid, -3.5)
        >>> tlm.stats()
        doppler.telemetry.TelemetryStats(dropped=0, emitted=1, capacity=4096, probes=1)
        >>> tlm.stats().emitted
        1

        """

    @property
    def probe_names(self) -> dict[str, int]:
        """Registered probes as `{name: id}`, in registration order. The
        inverse of `probe_id()`, and what a consumer needs to resolve a
        record's `probe` field back to a name.
        """

    @property
    def capacity(self) -> int:
        """Ring capacity in records, after buffer.h rounds the requested size
        up to a power of two and the page minimum. Read this rather than
        assuming the constructor's argument was granted verbatim.
        """

    @property
    def dropped(self) -> int:
        """Records lost to ring overrun over this context's lifetime,
        monotonic. Non-zero means a hole; prefer a `Capture`, which makes
        overrun arithmetically impossible rather than merely countable.
        """

    @property
    def probe_count(self) -> int:
        """Number of registered probes."""

    @property
    def avail(self) -> int:
        """Records currently readable, without consuming them. A lower bound
        while a producer is running -- the true count can only grow after the
        snapshot.
        """

    @property
    def _capsule(self) -> Any:
        """capsule."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "Telemetry":
        """Enter a context manager, returning this object.

        Lets a Telemetry be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Telemetry
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the Telemetry.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never
        suppresses one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.
        """

    # jm:hand
    def read_dict(
        self, n: int = 0, index: bool = False
    ) -> dict[
        str,
        NDArray[np.float32] | tuple[NDArray[np.uint64], NDArray[np.float32]],
    ]:
        """Drains like read(), but grouped by probe name.

        The same records read() returns, split per probe so a consumer never
        writes the ``recs[recs["probe"] == tlm.probe_id(name)]["value"]``
        filter or the id-to-name inversion by hand. Every REGISTERED probe
        gets a key, including one that emitted nothing this drain, so the key
        set is stable across calls.

        Consuming: this DRAINS the ring, exactly as read() does. Calling both
        in one loop splits the records between them.

        Parameters
        ----------
        n : int, optional
            Records wanted; 0 (the default) means everything available.
        index : bool, optional
            When True each value is ``(n, values)`` — the sample indices
            alongside the values, so a real time axis is ``n / fs``. When
            False (the default) each value is just the values array.

        Returns
        -------
        dict
            ``{probe_name: values}``, or ``{probe_name: (n, values)}`` when
            `index` is True.

        Examples
        --------
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> eid = tlm.probe("sync.e")
        >>> lid = tlm.probe("sync.lock")
        >>> tlm.set_now(7)
        >>> tlm.emit(eid, 0.5)
        >>> tlm.emit(lid, 1.0)
        >>> d = tlm.read_dict()
        >>> sorted(d)
        ['sync.e', 'sync.lock']
        >>> d["sync.e"]
        array([0.5], dtype=float32)
        >>> n, v = tlm.read_dict(index=True)["sync.e"]
        >>> n.size          # drained by the call above
        0

        """

@final
class MemoryCapture:
    """Opens a capture that accumulates in memory instead of a file.

    Parameters
    ----------
    tlm : Any
        Telemetry context to capture. Must outlive the capture.
    block_samples : int
        The LARGEST number of input samples processed between two boundaries —
        the step of your own block loop, not a buffer size to tune.
        Over-stating it costs only memory; under-stating it is the one way to
        lose a record, and close() reports it.
    clock : Any
        The pipeline's sample clock, borrowed for the sidecar's time base. Read
        at close(), so later track() corrections are picked up. Must outlive
        the capture. Pass None to state that there is no time base: the sidecar
        then omits the rate and epoch keys rather than fabricating them into a
        file that outlives the process. The argument itself is not omittable —
        say None deliberately.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``capture could not be
        opened: attach every probe BEFORE opening (the ring is sized from the
        probe table, so no probes means no bound), pass a non-zero
        block_samples, use a context with no capture already open, and — for
        the file flavour — a writable path``.

    Examples
    --------
    >>> from doppler.telemetry import Telemetry, MemoryCapture
    >>> from doppler.wfm import SampleClock
    >>> tlm = Telemetry(1 << 12)
    >>> pid = tlm.probe("agc.gain_db")   # probes FIRST: they set the bound
    >>> cap = MemoryCapture(tlm, 256, SampleClock(1e6))
    >>> for blk in range(4):
    ...     tlm.set_now(blk * 256)       # drains the block just finished
    ...     tlm.emit(pid, float(blk))
    >>> cap.close()                      # raises if anything was lost
    >>> [float(v) for v in cap.records()["value"]]
    [0.0, 1.0, 2.0, 3.0]
    >>> cap.dropped
    0

    """
    def __init__(
        self,
        tlm: object,
        block_samples: int,
        clock: object | None,
    ) -> None: ...

    def records(
        self,
        n: int = 0,
        out: NDArray[Any] | None = None,
    ) -> NDArray[Any]:
        """Copies accumulated records out. Memory mode only.

        The copying twin of dp_tlm_capture_records(): same records, same order,
        but into caller memory rather than a borrowed pointer. Both exist
        because they serve opposite callers — a C consumer wants the zero-copy
        view, and a binding must not hand out a pointer the capture can free
        underneath it.

        Deliberately the same shape as dp_tlm_read(), so the two drains bind
        identically and neither needs a second convention invented for it.

        Parameters
        ----------
        n : int
            Records wanted; 0 means "everything accumulated".
        out : NDArray[Any] | None
            Destination.

        Returns
        -------
        NDArray[Any]
            Number of records copied out.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.telemetry import Telemetry, MemoryCapture
        >>> from doppler.wfm import SampleClock
        >>> tlm = Telemetry(1 << 12)
        >>> pid = tlm.probe("agc.gain_db")
        >>> cap = MemoryCapture(tlm, 256, SampleClock(1e6))
        >>> for blk in range(4):
        ...     tlm.set_now(blk * 256)
        ...     tlm.emit(pid, float(blk))
        >>> cap.close()
        >>> [float(v) for v in cap.records()["value"]]
        [0.0, 1.0, 2.0, 3.0]
        >>> cap.records(2).shape             # 0 (the default) means "all"
        (2,)

        """

    def records_max_out(self) -> int:
        """Upper bound on what dp_tlm_capture_read() can return right now.

        The accumulated count: a caller sizing a destination cannot know its
        own

        request will be smaller, and the generated binding allocates this much,

        reads, then resizes to what actually came back.

        Returns
        -------
        int
            Output.
        """

    def block(self) -> None:
        """Block boundary: drains the ring to empty.

        Grows the ring first if probes appeared since the last boundary, which
        is safe precisely here — the ring is about to be emptied and the
        producer is between blocks. Then copies everything available into the
        active staging buffer, handing it to the sink and swapping when it can
        no longer hold another block.

        **May block** in file mode, if the writer still holds the other buffer.
        That wait is the backpressure that keeps the capture lossless; it
        happens at the boundary, never inside the DSP loop.

        Usually reached through dp_tlm_set_now() rather than called directly.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``block failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> from doppler.telemetry import Telemetry, MemoryCapture
        >>> from doppler.wfm import SampleClock
        >>> tlm = Telemetry(1 << 12)
        >>> pid = tlm.probe("agc.gain_db")
        >>> cap = MemoryCapture(tlm, 256, SampleClock(1e6))
        >>> tlm.emit(pid, 1.5)

        An explicit boundary; set_now() reaches this for you:

        >>> cap.block()
        >>> cap.count
        1

        """

    def close(self) -> None:
        """Final boundary, then flush, join, and write the sidecar.

        Sweeps the tail the last block left behind, drains the staging buffers,
        joins the writer thread, closes the file and writes `<path>-meta`.
        Idempotent: a second call is a no-op returning the first call's
        verdict.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``the capture has a hole: records were dropped, which the block
            bound makes impossible unless a step ran longer than block_samples
            or no boundary was reached at all — see Capture.dropped``, with the
            return code appended (gh-869).

        Examples
        --------
        >>> from doppler.telemetry import Telemetry, MemoryCapture
        >>> from doppler.wfm import SampleClock
        >>> tlm = Telemetry(1 << 12)
        >>> pid = tlm.probe("agc.gain_db")
        >>> cap = MemoryCapture(tlm, 256, SampleClock(1e6))
        >>> for blk in range(4):
        ...     tlm.set_now(blk * 256)
        ...     tlm.emit(pid, float(blk))
        >>> cap.close()          # silent: the block contract was honoured
        >>> cap.close()          # idempotent, same verdict

        Breaking the contract -- here, never reaching a boundary at all -- is the
        one way to lose a record, and it is reported rather than absorbed:

        >>> tlm2 = Telemetry(1 << 12)
        >>> p2 = tlm2.probe("x")
        >>> bad = MemoryCapture(tlm2, 8, SampleClock(1e6))
        >>> for i in range(20000):
        ...     tlm2.emit(p2, float(i))
        >>> bad.close()  # doctest: +ELLIPSIS
        Traceback (most recent call last):
        ValueError: the capture has a hole: ...

        """

    @property
    def count(self) -> int:
        """Records captured so far, across memory and file alike."""

    @property
    def dropped(self) -> int:
        """Records the ring dropped during THIS capture (latched at open
        against the context's monotonic counter). Non-zero means a hole.
        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.

        Raises
        ------
        ValueError
            If the C destructor reports failure. Raised from an explicit call
            and from ``__exit__`` alike, so a failing teardown propagates out
            of a ``with`` block (gh-541).
        """


    def __enter__(self) -> "MemoryCapture":
        """Enter a context manager, returning this object.

        Lets a MemoryCapture be used in a `with` statement so its C resources
        are finalized deterministically on exit rather than at collection time.

        Returns
        -------
        MemoryCapture
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, finalizing the MemoryCapture.

        Equivalent to calling `close()`. The MemoryCapture is **not** released
        here: it stays usable, which is what makes results gathered during the
        `with` body readable after it. The memory is freed when the object is
        collected.

        Returns ``None``, so an exception raised inside the `with` body
        propagates normally; this never suppresses one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.

        Raises
        ------
        ValueError
            If ``close()`` reports failure. ``__exit__`` calls it and raises
            what it raises, so a failed finalize propagates out of the ``with``
            block (gh-805 §H).
        """

    # jm:hand
    def read_dict(
        self, n: int = 0, index: bool = False
    ) -> dict[
        str,
        NDArray[np.float32] | tuple[NDArray[np.uint64], NDArray[np.float32]],
    ]:
        """The captured records, grouped by probe name.

        records() hands back one structured array carrying every probe
        interleaved; this splits it, so a consumer never writes the
        ``recs[recs["probe"] == id]["value"]`` filter or the id-to-name
        inversion by hand. Every REGISTERED probe gets a key, including one
        that captured nothing, so the key set is stable.

        Like records(), this does NOT consume — call it as often as you like.

        Parameters
        ----------
        n : int, optional
            Records to take from the front; 0 (the default) means all.
        index : bool, optional
            When True each value is ``(n, values)`` — the sample indices
            alongside the values, so a real time axis is ``n / fs``.

        Returns
        -------
        dict
            ``{probe_name: values}``, or ``{probe_name: (n, values)}`` when
            `index` is True.

        Examples
        --------
        >>> from doppler.telemetry import MemoryCapture, Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> eid = tlm.probe("sync.e")
        >>> with MemoryCapture(tlm, 64, None) as cap:
        ...     tlm.set_now(0)
        ...     tlm.emit(eid, 0.25)
        ...     tlm.set_now(64)
        >>> n, v = cap.read_dict(index=True)["sync.e"]
        >>> v
        array([0.25], dtype=float32)
        >>> n
        array([0], dtype=uint64)

        """

@final
class Capture:
    """Opens a capture that accumulates in memory instead of a file.

    Parameters
    ----------
    tlm : Any
        Telemetry context to capture. Must outlive the capture.
    block_samples : int
        The LARGEST number of input samples processed between two boundaries —
        the step of your own block loop.
    path : str | os.PathLike
        Output file, truncated if it exists. The 16-byte record layout IS the
        file, so np.fromfile reads it directly; a <path>-meta JSON sidecar
        carries the probe table, the counters and the time base.
    clock : Any
        The pipeline's sample clock, borrowed for the sidecar's time base — see
        MemoryCapture. Pass None to state that there is no time base; the
        argument itself is not omittable.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``capture could not be
        opened: attach every probe BEFORE opening (the ring is sized from the
        probe table, so no probes means no bound), pass a non-zero
        block_samples, use a context with no capture already open, and — for
        the file flavour — a writable path``.

    Examples
    --------
    >>> from doppler.telemetry import Telemetry, MemoryCapture
    >>> from doppler.wfm import SampleClock
    >>> tlm = Telemetry(1 << 12)
    >>> pid = tlm.probe("agc.gain_db")   # probes FIRST: they set the bound
    >>> cap = MemoryCapture(tlm, 256, SampleClock(1e6))
    >>> for blk in range(4):
    ...     tlm.set_now(blk * 256)       # drains the block just finished
    ...     tlm.emit(pid, float(blk))
    >>> cap.close()                      # raises if anything was lost
    >>> [float(v) for v in cap.records()["value"]]
    [0.0, 1.0, 2.0, 3.0]
    >>> cap.dropped
    0

    """
    def __init__(
        self,
        tlm: object,
        block_samples: int,
        path: str | os.PathLike,
        clock: object | None,
    ) -> None: ...

    def block(self) -> None:
        """Block boundary: drains the ring to empty.

        Grows the ring first if probes appeared since the last boundary, which
        is safe precisely here — the ring is about to be emptied and the
        producer is between blocks. Then copies everything available into the
        active staging buffer, handing it to the sink and swapping when it can
        no longer hold another block.

        **May block** in file mode, if the writer still holds the other buffer.
        That wait is the backpressure that keeps the capture lossless; it
        happens at the boundary, never inside the DSP loop.

        Usually reached through dp_tlm_set_now() rather than called directly.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``block failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> from doppler.telemetry import Telemetry, MemoryCapture
        >>> from doppler.wfm import SampleClock
        >>> tlm = Telemetry(1 << 12)
        >>> pid = tlm.probe("agc.gain_db")
        >>> cap = MemoryCapture(tlm, 256, SampleClock(1e6))
        >>> tlm.emit(pid, 1.5)

        An explicit boundary; set_now() reaches this for you:

        >>> cap.block()
        >>> cap.count
        1

        """

    def close(self) -> None:
        """Final boundary, then flush, join, and write the sidecar.

        Sweeps the tail the last block left behind, drains the staging buffers,
        joins the writer thread, closes the file and writes `<path>-meta`.
        Idempotent: a second call is a no-op returning the first call's
        verdict.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``the capture has a hole: records were dropped, which the block
            bound makes impossible unless a step ran longer than block_samples
            or no boundary was reached at all — see Capture.dropped``, with the
            return code appended (gh-869).

        Examples
        --------
        >>> from doppler.telemetry import Telemetry, MemoryCapture
        >>> from doppler.wfm import SampleClock
        >>> tlm = Telemetry(1 << 12)
        >>> pid = tlm.probe("agc.gain_db")
        >>> cap = MemoryCapture(tlm, 256, SampleClock(1e6))
        >>> for blk in range(4):
        ...     tlm.set_now(blk * 256)
        ...     tlm.emit(pid, float(blk))
        >>> cap.close()          # silent: the block contract was honoured
        >>> cap.close()          # idempotent, same verdict

        Breaking the contract -- here, never reaching a boundary at all -- is the
        one way to lose a record, and it is reported rather than absorbed:

        >>> tlm2 = Telemetry(1 << 12)
        >>> p2 = tlm2.probe("x")
        >>> bad = MemoryCapture(tlm2, 8, SampleClock(1e6))
        >>> for i in range(20000):
        ...     tlm2.emit(p2, float(i))
        >>> bad.close()  # doctest: +ELLIPSIS
        Traceback (most recent call last):
        ValueError: the capture has a hole: ...

        """

    @property
    def count(self) -> int:
        """Records captured so far, across memory and file alike."""

    @property
    def dropped(self) -> int:
        """Records the ring dropped during THIS capture (latched at open
        against the context's monotonic counter). Non-zero means a hole.
        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.

        Raises
        ------
        ValueError
            If the C destructor reports failure. Raised from an explicit call
            and from ``__exit__`` alike, so a failing teardown propagates out
            of a ``with`` block (gh-541).
        """


    def __enter__(self) -> "Capture":
        """Enter a context manager, returning this object.

        Lets a Capture be used in a `with` statement so its C resources are
        finalized deterministically on exit rather than at collection time.

        Returns
        -------
        Capture
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, finalizing the Capture.

        Equivalent to calling `close()`. The Capture is **not** released here:
        it stays usable, which is what makes results gathered during the `with`
        body readable after it. The memory is freed when the object is
        collected.

        Returns ``None``, so an exception raised inside the `with` body
        propagates normally; this never suppresses one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.

        Raises
        ------
        ValueError
            If ``close()`` reports failure. ``__exit__`` calls it and raises
            what it raises, so a failed finalize propagates out of the ``with``
            block (gh-805 §H).
        """
