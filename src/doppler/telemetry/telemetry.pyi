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

    Examples
    --------
    Create with defaults:

    >>> from doppler.telemetry import Telemetry
    >>> obj = Telemetry(ring_records=16384)

    """
    def __init__(self, ring_records: int = ...) -> None: ...

    def read(self, n: int = 0) -> NDArray[Any]:
        """Drains records into out. Non-blocking.

        Consumer side of the SPSC ring: safe to call from a different thread
        than the producer. Returns immediately with whatever is available
        (possibly 0) — never spins.

        Parameters
        ----------
        n : int
            Records wanted; 0 means "everything available".

        Returns
        -------
        NDArray[Any]
            Number of records copied out.
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
        """

    def probe_id(self, name: str) -> int:
        """Looks up a probe id by name; ::DP_ERR_INVALID if unknown.

        Parameters
        ----------
        name : str
            Input.

        Returns
        -------
        int
            Output.
        """

    def set_decim(self, name: str, decim: int) -> None:
        """Retunes an EXISTING probe's decimation, by name.

        Distinct from dp_tlm_probe(), which registers on a miss: this refuses
        an unknown name rather than quietly creating a probe nothing emits to,
        which is what a typo in a retune call deserves.

        Parameters
        ----------
        name : str
            Input.
        decim : int
            Input.
        """

    def emit(self, id: int, v: float) -> None:
        """Records one scalar for probe id. The hot-path primitive.

        Detached (t NULL) this is one branch — the entire disabled cost.
        Attached: bump the probe's decimation phase, and on the decim-th event
        write one 16-byte record (value narrowed to float, stamped with the
        context's current now). Never blocks, never allocates; on ring overrun
        the record is dropped and counted.

        id must come from a successful dp_tlm_probe() on this context — an
        object's set_telemetry fails the whole attach otherwise.

        The bound checked here is the ARRAY's, not the registry's. probes is a
        fixed DP_TLM_MAX_PROBES array, so the unguarded indexing this used to
        do turned any out-of-range id into an out-of-bounds write — reachable
        from a language binding, where the id is whatever the caller passed,
        and `Telemetry.emit(1000000, 1.0)` segfaulted the interpreter.
        Comparing against the compile-time constant (unsigned, so a negative id
        fails it too) needs no memory and measures free. Comparing against
        n_probes instead would also reject an in-range-but-unregistered id, but
        it loads a field on the early-return path and cost ~16% of the
        decimated case (bench_telemetry_core, ABBA-interleaved) — so *that*
        check belongs at the binding boundary, where the id is untrusted, not
        in the hot loop, where the caller holds an id dp_tlm_probe() gave it.

        Parameters
        ----------
        id : int
            Input.
        v : float
            Input.
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
            Input.
        """

    def emitted(self, id: int) -> int:
        """Records written for probe id (post-decimation, post-drop).

        Parameters
        ----------
        id : int
            Input.

        Returns
        -------
        int
            Output.
        """

    def stats(self) -> TelemetryStats:
        """Snapshots the context's counters. Zeroed for a NULL context.

        Returns
        -------
        TelemetryStats
            Output.
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
