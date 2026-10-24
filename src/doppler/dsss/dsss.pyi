# dsss/dsss.pyi — type stubs for the dsss C extension.
from typing import Any, final, Literal
import numpy as np
import numpy.typing as npt
from numpy.typing import NDArray

@final
class PolynomialPhaseEstimate(tuple[float, float, float]):
    """Polynomial-phase estimate: carrier frequency, chirp rate, and a rough
    SNR confidence.

    Attributes
    ----------
    freq_norm : float
        frequency, cycles/sample, in [-0.5, 0.5).
    rate_norm : float
        chirp rate, cycles/sample^2.
    snr_db : float
        winning-row peak-to-mean (rough confidence).
    """

    @property
    def freq_norm(self) -> float:
        """frequency, cycles/sample, in [-0.5, 0.5)."""

    @property
    def rate_norm(self) -> float:
        """chirp rate, cycles/sample^2."""

    @property
    def snr_db(self) -> float:
        """winning-row peak-to-mean (rough confidence)."""

@final
class ReceiverStatus(tuple[int, float, float, float, float, int, int, float, float, float, float, int, int, int]):
    """AsyncDsssReceiver's status record: state (0 searching, 1 refining, 2
    tracking, 4 lost), the live estimates, both lock flags, and the two clocks
    in input samples.

    Attributes
    ----------
    state : int
        ASYNC_DSSS_RX_SEARCHING .. _LOST -- where it is.
    doppler_hz : float
        Where the emitter is NOW: the whole carrier estimate, Hz -- loop 1's plus what loop 2 has taken up beyond it (the seed while refining, 0 while searching, frozen where it was when lost). Only as good as `locked`: with the carrier unlocked, loop 1 free-runs and this wanders.
    chip_phase : float
        Code phase, chips: the live Dll's; while refining, the seed's advanced by the clock dilation since (what the hand-over starts from), so a refining receiver's signal can be excluded too.
    code_rate : float
        The Dll's code rate, chips per nominal chip (~1.0 at lock), without the carrier aid; a coasting Dll's is its held rate.
    cn0_dbhz_est : float
        C/N0 estimate, dB-Hz (the seed's).
    code_locked : int
        Presence flag: the Dll's lock detector.
    locked : int
        Health flag: the symbol-lock detector.
    lock_metric : float
        cos(2*phi) over the symbols, drives `locked`.
    lock_threshold : float
        `locked` declares strictly above this.
    car_last_error : float
        Pre-despread Costas residual, rad.
    mpsk_last_error : float
        Post-despread carrier residual, rad.
    state_samples : int
        Input samples since `state` was entered.
    both_down_samples : int
        Input samples both flags have been down without a break (the release clock); in lost it keeps counting -- samples since the flags dropped. With reacquire, the symbol lock flag's clock alone.
    refused_hits : int
        Searching hits refused as seeds by the seed domain (a Doppler at or past fs/2), since create or reset. The search went on over each one's tail.
    """

    @property
    def state(self) -> int:
        """ASYNC_DSSS_RX_SEARCHING .. _LOST -- where it is."""

    @property
    def doppler_hz(self) -> float:
        """Where the emitter is NOW: the whole carrier estimate, Hz -- loop 1's
        plus what loop 2 has taken up beyond it (the seed while refining, 0
        while searching, frozen where it was when lost). Only as good as
        `locked`: with the carrier unlocked, loop 1 free-runs and this
        wanders.
        """

    @property
    def chip_phase(self) -> float:
        """Code phase, chips: the live Dll's; while refining, the seed's
        advanced by the clock dilation since (what the hand-over starts from),
        so a refining receiver's signal can be excluded too.
        """

    @property
    def code_rate(self) -> float:
        """The Dll's code rate, chips per nominal chip (~1.0 at lock), without
        the carrier aid; a coasting Dll's is its held rate.
        """

    @property
    def cn0_dbhz_est(self) -> float:
        """C/N0 estimate, dB-Hz (the seed's)."""

    @property
    def code_locked(self) -> int:
        """Presence flag: the Dll's lock detector."""

    @property
    def locked(self) -> int:
        """Health flag: the symbol-lock detector."""

    @property
    def lock_metric(self) -> float:
        """cos(2*phi) over the symbols, drives `locked`."""

    @property
    def lock_threshold(self) -> float:
        """`locked` declares strictly above this."""

    @property
    def car_last_error(self) -> float:
        """Pre-despread Costas residual, rad."""

    @property
    def mpsk_last_error(self) -> float:
        """Post-despread carrier residual, rad."""

    @property
    def state_samples(self) -> int:
        """Input samples since `state` was entered."""

    @property
    def both_down_samples(self) -> int:
        """Input samples both flags have been down without a break (the release
        clock); in lost it keeps counting -- samples since the flags dropped.
        With reacquire, the symbol lock flag's clock alone.
        """

    @property
    def refused_hits(self) -> int:
        """Searching hits refused as seeds by the seed domain (a Doppler at or
        past fs/2), since create or reset. The search went on over each one's
        tail.
        """

@final
class PoolReceiverStatus(tuple[int, float, float, float, float, int, int, float, float, float, float, int, int, int]):
    """One AsyncDsssPool receiver's status record, read after the controller's
    step: state (0 searching, 1 refining, 2 tracking, 4 lost), the live
    estimates, both lock flags, and the two clocks in input samples.

    Attributes
    ----------
    state : int
        ASYNC_DSSS_RX_SEARCHING .. _LOST -- where it is.
    doppler_hz : float
        Where the emitter is NOW: the whole carrier estimate, Hz -- loop 1's plus what loop 2 has taken up beyond it (the seed while refining, 0 while searching, frozen where it was when lost). Only as good as `locked`: with the carrier unlocked, loop 1 free-runs and this wanders.
    chip_phase : float
        Code phase, chips: the live Dll's; while refining, the seed's advanced by the clock dilation since (what the hand-over starts from), so a refining receiver's signal can be excluded too.
    code_rate : float
        The Dll's code rate, chips per nominal chip (~1.0 at lock), without the carrier aid; a coasting Dll's is its held rate.
    cn0_dbhz_est : float
        C/N0 estimate, dB-Hz (the seed's).
    code_locked : int
        Presence flag: the Dll's lock detector.
    locked : int
        Health flag: the symbol-lock detector.
    lock_metric : float
        cos(2*phi) over the symbols, drives `locked`.
    lock_threshold : float
        `locked` declares strictly above this.
    car_last_error : float
        Pre-despread Costas residual, rad.
    mpsk_last_error : float
        Post-despread carrier residual, rad.
    state_samples : int
        Input samples since `state` was entered.
    both_down_samples : int
        Input samples both flags have been down without a break (the release clock); in lost it keeps counting -- samples since the flags dropped. With reacquire, the symbol lock flag's clock alone.
    refused_hits : int
        Searching hits refused as seeds by the seed domain (a Doppler at or past fs/2), since create or reset. The search went on over each one's tail.
    """

    @property
    def state(self) -> int:
        """ASYNC_DSSS_RX_SEARCHING .. _LOST -- where it is."""

    @property
    def doppler_hz(self) -> float:
        """Where the emitter is NOW: the whole carrier estimate, Hz -- loop 1's
        plus what loop 2 has taken up beyond it (the seed while refining, 0
        while searching, frozen where it was when lost). Only as good as
        `locked`: with the carrier unlocked, loop 1 free-runs and this
        wanders.
        """

    @property
    def chip_phase(self) -> float:
        """Code phase, chips: the live Dll's; while refining, the seed's
        advanced by the clock dilation since (what the hand-over starts from),
        so a refining receiver's signal can be excluded too.
        """

    @property
    def code_rate(self) -> float:
        """The Dll's code rate, chips per nominal chip (~1.0 at lock), without
        the carrier aid; a coasting Dll's is its held rate.
        """

    @property
    def cn0_dbhz_est(self) -> float:
        """C/N0 estimate, dB-Hz (the seed's)."""

    @property
    def code_locked(self) -> int:
        """Presence flag: the Dll's lock detector."""

    @property
    def locked(self) -> int:
        """Health flag: the symbol-lock detector."""

    @property
    def lock_metric(self) -> float:
        """cos(2*phi) over the symbols, drives `locked`."""

    @property
    def lock_threshold(self) -> float:
        """`locked` declares strictly above this."""

    @property
    def car_last_error(self) -> float:
        """Pre-despread Costas residual, rad."""

    @property
    def mpsk_last_error(self) -> float:
        """Post-despread carrier residual, rad."""

    @property
    def state_samples(self) -> int:
        """Input samples since `state` was entered."""

    @property
    def both_down_samples(self) -> int:
        """Input samples both flags have been down without a break (the release
        clock); in lost it keeps counting -- samples since the flags dropped.
        With reacquire, the symbol lock flag's clock alone.
        """

    @property
    def refused_hits(self) -> int:
        """Searching hits refused as seeds by the seed domain (a Doppler at or
        past fs/2), since create or reset. The search went on over each one's
        tail.
        """

@final
class Despreader:
    """Create a continuous DSSS despreader (COPIES code).

    Parameters
    ----------
    code : NDArray[np.uint8]
        Spreading code (0/1 chips), one period; copied.
    sps : int, default 4
        Samples per chip.
    init_norm_freq : float, default 0.0
        Seed carrier frequency, cycles/sample (the acquisition estimate).
    init_chip : float, default 0.0
        Seed code phase, chips (the acquisition estimate).
    bn_carrier : float, default 0.05
        Carrier loop noise bandwidth, normalized to the code-period (symbol)
        rate.
    bn_code : float, default 0.005
        Code loop noise bandwidth, normalized to the code-period rate.
    bn_fll : float, default 0.0
        Carrier FLL-assist bandwidth (0 = pure PLL); set > 0 for FLL-assisted
        carrier pull-in.
    zeta : float, default 0.707
        Damping factor shared by both second-order loops.
    spacing : float, default 0.5
        DLL early/late correlator tap offset, chips.
    periods_per_bit : int, default 1
        Code periods per data bit (1 = one bit per period).

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``Despreader: invalid
        parameter (need a non-empty code; for the code loop a finite init_chip,
        bn_code >= 0 and zeta > 0 both finite, whose loop gains come out
        finite, and 0 < spacing < len(code)/2; for the carrier loop bn_carrier
        >= 0, finite, whose loop gains come out finite)``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.dsss import Despreader
    >>> rng = np.random.default_rng(3)
    >>> code = rng.integers(0, 2, 31).astype(np.uint8)   # one code period
    >>> chips = np.where(code & 1, -1.0, 1.0)    # 0 -> +1, 1 -> -1
    >>> bits = rng.integers(0, 2, 40).astype(np.uint8)  # 1 bit/period
    >>> syms = np.where(bits == 1, -1.0, 1.0)
    >>> rx = np.concatenate(
    ...     [s * np.repeat(chips, 4) for s in syms]).astype(np.complex64)
    >>> d = Despreader(code, sps=4)          # seed a fresh tracking loop
    >>> data = d.bits(rx)                        # hard data bits, 1/period
    >>> e = np.mean(data != bits[:data.size])    # up to a global BPSK flip
    >>> round(float(min(e, 1.0 - e)), 4)
    0.0

    """

    def __init__(
        self,
        code: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
        sps: int = 4,
        init_norm_freq: float = 0.0,
        init_chip: float = 0.0,
        bn_carrier: float = 0.05,
        bn_code: float = 0.005,
        bn_fll: float = 0.0,
        zeta: float = 0.707,
        spacing: float = 0.5,
        periods_per_bit: int = 1,
    ) -> None: ...
    # jm:hand
    def steps(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = ...,
    ) -> NDArray[np.complex64]:
        """Track carrier + code and despread a cf32 block: per sample wipe the carrier (Costas) and correlate early/prompt/late against the code (DLL), update both loops each code period, and emit one complex prompt symbol per period.

        Without out=, the returned array is a view into a buffer reused on the
        next call (see steps_max_out() to size an out= buffer for an
        independent, alias-free result).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.
        out : NDArray[np.complex64], optional
            Caller-provided output buffer, at least max(steps_max_out(),
            len(x)) elements.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    # jm:hand
    def steps_max_out(self) -> int:
        """Max output length steps() can produce for the current state. Use to size the ``out=`` buffer."""

    # jm:hand
    def bits(
        self, x: NDArray[np.complex64], out: NDArray[np.uint8] | None = ...
    ) -> NDArray[np.uint8]:
        """Same tracking kernel as steps(), but bit-sync the per-period prompts into hard data bits: periods_per_bit prompts are coherently summed across each detected bit boundary and one 0/1 bit is emitted per data bit.

        Without out=, the returned array is a view into a buffer reused on the
        next call (see bits_max_out() to size an out= buffer for an
        independent, alias-free result).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.
        out : NDArray[np.uint8], optional
            Caller-provided output buffer, at least max(bits_max_out(),
            len(x)) elements.

        Returns
        -------
        NDArray[np.uint8]
            Output.
        """

    # jm:hand
    def bits_max_out(self) -> int:
        """Max output length bits() can produce for the current state. Use to size the ``out=`` buffer."""

    def set_telemetry(
        self,
        tlm: object | None,
        prefix: str,
        decim: int = 1,
    ) -> None:
        """Attach (or detach) a telemetry context across the despreader. Pure
        forwarder — the despreader registers no probes of its own: the carrier
        loop registers "<prefix>.car.lock" / ".e" / ".freq" / ".locked" /
        ".nco" and the code loop registers "<prefix>.code.e" / ".rate" /
        ".lock" / ".locked" / ".nco" (the ".locked" pair are the loops'
        verify-counted lockdet decisions, 0/1; ".freq" / ".rate" the filters'
        integrators and ".nco" their complete commands, see
        dp_costas_set_telemetry() and dp_dll_set_telemetry()) — ten probes, all
        thinned by decim and emitted once per code period (the despreader
        flushes both loops at its per-period update). Passing NULL detaches
        both loops. Setup path, never hot; the context is borrowed and must
        outlive the attachment (SPSC rules in dp_tlm/dp_tlm_core.h).

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "ch0".
        decim : int
            Emit every decim-th code period; >= 1.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_telemetry failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import Despreader
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> code = (np.arange(31) % 2).astype(np.uint8)
        >>> ch = Despreader(code=code, sps=4)
        >>> ch.set_telemetry(tlm, "ch0")
        >>> names = sorted(tlm.probe_names)
        >>> names[:5]
        ['ch0.car.e', 'ch0.car.freq', 'ch0.car.lock', 'ch0.car.locked', 'ch0.car.nco']
        >>> names[5:]
        ['ch0.code.e', 'ch0.code.lock', 'ch0.code.locked', 'ch0.code.nco', 'ch0.code.rate']
        >>> chips = 1.0 - 2.0 * (np.arange(31) % 2)
        >>> x = np.tile(np.repeat(chips, 4), 40).astype(np.complex64)
        >>> _ = ch.steps(x)
        >>> recs = tlm.read()   # ten records per code period
        >>> len(recs) > 0 and len(recs) % 10 == 0
        True

        """

    def configure_carrier_lock(
        self,
        up_thresh: float,
        down_thresh: float,
        n_up: int,
        n_down: int,
    ) -> None:
        """Re-tune the embedded carrier loop's lock detector directly: forwards
        to the Costas loop's configure_lock (locked flips up after n_up
        consecutive symbols with the lock-metric EMA above up_thresh, and drops
        after n_down consecutive symbols below down_thresh; see
        Costas.configure_lock). Symmetric with the carrier_locked state
        property: state is readable, so config should be writable too, rather
        than forcing a caller who needs this control to drop to raw Dll+Costas
        composition.

        Thin forwarder to dp_costas_configure_lock() on the embedded Costas
        loop — symmetric with dp_despreader_get_carrier_locked() exposing its
        state: state is readable, so config should be writable too, rather than
        forcing a caller who needs this control to drop to raw Dll+Costas
        composition instead of Despreader. See dp_costas_configure_lock() for
        the parameter semantics.

        Parameters
        ----------
        up_thresh : float
            Declare threshold on the lock-metric EMA.
        down_thresh : float
            Drop threshold (<= up_thresh for level hysteresis).
        n_up : int
            Consecutive above-threshold symbols to declare.
        n_down : int
            Consecutive below-threshold symbols to drop.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import Despreader
        >>> d = Despreader(code=np.zeros(31, dtype=np.uint8), sps=2)
        >>> d.configure_carrier_lock(0.9, 0.8, 4, 16)  # tighter declare/drop

        """

    def configure_code_lock(
        self,
        pfa: float,
        n_looks: int,
        ref_snr_db: float = 0.0,
    ) -> None:
        """Re-tune the embedded code loop's lock detector: forwards to the
        DLL's configure_lock (see Dll.configure_lock) -- the derived
        (pfa-style) entry point, matching Despreader's role as the easy
        composed API (Dll's raw escape hatch, configure_lock_raw, stays a
        Dll-only control for a caller that composes Dll+Costas directly).
        Raises ValueError for pfa outside (0, 1).

        Thin forwarder to dp_dll_configure_lock() on the embedded DLL — the
        derived (pfa-style) entry point, matching Despreader's role as the
        "easy" composed API (Dll's raw escape hatch,
        dp_dll_configure_lock_raw(), stays a Dll-only control for a caller that
        composes Dll+Costas directly). See dp_dll_configure_lock() for the
        parameter semantics.

        Parameters
        ----------
        pfa : float
            Per-decision false-alarm probability, in (0, 1).
        n_looks : int
            Non-coherent integration depth N (looks); clamped >= 1.
        ref_snr_db : float
            Noise-reference estimator SNR in dB (> 0), or 0 to derive from
            n_looks (see dp_dll_configure_lock()).

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``configure_code_lock failed``, with the return code appended
            (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import Despreader
        >>> d = Despreader(code=np.zeros(31, dtype=np.uint8), sps=2)
        >>> d.configure_code_lock(1e-3, 20)
        >>> d.code_locked
        False
        >>> d.configure_code_lock(2.0, 20)
        Traceback (most recent call last):
            ...
        ValueError: configure_code_lock failed (rc=-4)

        """

    def reset(self) -> None:
        """Re-seed both loops to the create-time frequency/phase; preserve
        config.

        Restores the carrier NCO to init_norm_freq and the code phase to
        init_chip, zeroes the loop-filter accumulators and the bit-sync
        histogram, and clears the lock detectors — the spreading code and every
        configured bandwidth are preserved. Use it to re-run the same
        despreader over an independent stream and get a fresh instance's
        result.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import Despreader
        >>> rng = np.random.default_rng(3)
        >>> code = rng.integers(0, 2, 31).astype(np.uint8)
        >>> chips = np.where(code & 1, -1.0, 1.0)
        >>> syms = np.where(rng.integers(0, 2, 40) == 1, -1.0, 1.0)
        >>> rx = np.concatenate(
        ...     [s * np.repeat(chips, 4) for s in syms]).astype(np.complex64)
        >>> d = Despreader(code=code, sps=4)
        >>> first = d.bits(rx)
        >>> d.reset()                          # re-seed to acquisition
        >>> np.array_equal(first, d.bits(rx))  # same result as a fresh object
        True

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the Despreader has already been destroyed.

        Returns
        -------
        int
            Byte length of one serialized state blob.
        """

    def get_state(self) -> bytes:
        """Serialize this object's mutable state to bytes.

        Captures the state that evolves as the object runs, including any value
        a setter changed after construction, so a blob taken now and restored
        later resumes from this point. Construction parameters are not
        restored: restore into an object built the same way. A core may check
        some of them and refuse a blob built differently.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the Despreader has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes, /) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place, including any value a setter
        changed after construction. Construction parameters are not restored:
        restore into an object built the same way. Length is validated against
        `state_bytes()` before the blob is handed to the C core, which may
        reject it too: a core may check some construction parameters and refuse
        a blob built differently.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the Despreader has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def norm_freq(self) -> float:
        """Norm freq."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...
    @property
    def code_phase(self) -> float:
        """Code phase."""

    @property
    def code_rate(self) -> float:
        """Chips the embedded DLL advances per nominal chip: ~1.0, and its
        departure from 1 is the code-rate (clock) offset being tracked.
        """

    @property
    def lock_metric(self) -> float:
        """The embedded Costas loop's lock statistic: the EMA of |Re P|/|P|
        over the prompt correlations, 1 when locked. `carrier_locked` is the
        de-chattered decision made on it.
        """

    @property
    def carrier_locked(self) -> bool:
        """Carrier lock decision: the embedded Costas loop's verify-counted
        detector on its lock-metric EMA (True = locked; see
        Costas.configure_lock).
        """

    @property
    def code_locked(self) -> bool:
        """Code lock decision: the embedded DLL's verify-counted CFAR detector
        (True = locked; see Dll.configure_lock). Live in composition — the
        despreader runs the same always-on detector Dll.steps does.
        """

    @property
    def bit_phase(self) -> int:
        """detected bit boundary (argmax flip_hist)."""

    @property
    def bn_carrier(self) -> float:
        """Bn carrier.

        Raises
        ------
        ValueError
            On assignment, if ``dp_despreader_set_bn_carrier`` refuses the
            value by returning non-zero. The exception message is ``Despreader:
            bn_carrier is outside the loop filter's domain (need bn_carrier >=
            0, finite, whose loop gains come out finite)``, with the return
            code appended.
        """
    @bn_carrier.setter
    def bn_carrier(self, value: float) -> None: ...
    @property
    def bn_code(self) -> float:
        """Bn code.

        Raises
        ------
        ValueError
            On assignment, if ``dp_despreader_set_bn_code`` refuses the value
            by returning non-zero. The exception message is ``Despreader:
            bn_code is outside the loop filter's domain (need bn_code >= 0,
            finite, whose loop gains come out finite)``, with the return code
            appended.
        """
    @bn_code.setter
    def bn_code(self, value: float) -> None: ...
    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """

    def __enter__(self) -> "Despreader":
        """Enter a context manager, returning this object.

        Lets a Despreader be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Despreader
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
        /,
    ) -> None:
        """Exit a context manager, releasing the Despreader.

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

@final
class BurstDespreader:
    """Create a burst despreader instance.

    Parameters
    ----------
    code : NDArray[np.uint8]
        Data spreading code (0/1 chips), length code_len; copied.
    sf : int, default 1
        Spreading factor: chips integrated per prompt symbol (default: 1).
    sps : int, default 2
        Samples per chip (default: 2).
    init_norm_freq : float, default 0.0
        Seed carrier frequency, cycles/sample — the acquisition estimate;
        finite (default: 0.0).
    init_chip_phase : float, default 0.0
        Seed code phase, chips; finite (default: 0.0).
    bn_carrier : float, default 0.05
        Carrier (Costas) loop noise bandwidth, normalized to the symbol rate
        (default: 0.05).
    bn_code : float, default 0.01
        Code (DLL) loop noise bandwidth, normalized to the symbol rate
        (default: 0.01).

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``BurstDespreader:
        invalid parameter (need a code of at least sf chips, sf >= 1, sps >= 2,
        a finite init_norm_freq and init_chip_phase, and bn_carrier and bn_code
        within the loop filter's domain: each >= 0, finite, whose loop gains
        come out finite)``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.dsss import BurstDespreader
    >>> rng = np.random.default_rng(1)
    >>> code = rng.integers(0, 2, 31).astype(np.uint8)  # length-31 code
    >>> chips = np.where(code & 1, -1.0, 1.0)    # 0 -> +1, 1 -> -1
    >>> bits = rng.integers(0, 2, 30).astype(np.uint8)    # payload bits
    >>> syms = np.where(bits == 1, -1.0, 1.0)             # BPSK symbols
    >>> tx = np.concatenate(
    ...     [np.repeat(s * chips, 4) for s in syms]).astype(np.complex64)
    >>> b = BurstDespreader(code, sf=31, sps=4)           # 31 chips/symbol
    >>> sym = b.steps(tx)                        # one prompt/symbol
    >>> sym.shape
    (30,)
    >>> hard = (sym.real < 0).astype(np.uint8)            # BPSK decision
    >>> float(np.mean(hard != bits))             # payload recovered
    0.0

    """

    def __init__(
        self,
        code: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
        sf: int = 1,
        sps: int = 2,
        init_norm_freq: float = 0.0,
        init_chip_phase: float = 0.0,
        bn_carrier: float = 0.05,
        bn_code: float = 0.01,
    ) -> None: ...
    # jm:hand
    def steps(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = ...,
    ) -> NDArray[np.complex64]:
        """Despread a cf32 block; emit one complex prompt symbol per code period.

        Streams: a partial symbol is carried in state across calls. Each emitted
        symbol is the complex prompt integrate-and-dump (carrier-wiped,
        code-stripped) — its sign is the BPSK decision, its phase/magnitude the
        soft information. During a `burst_despreader_set_acq` preamble no symbols are
        emitted (the loops are pulling in); payload symbols follow.

        Without out=, the returned array is a view into a buffer reused on the
        next call (see steps_max_out() to size an out= buffer for an
        independent, alias-free result).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input CF32 samples, length x_len.
        out : NDArray[np.complex64], optional
            Caller-provided output buffer, at least max(steps_max_out(),
            len(x)) elements.

        Returns
        -------
        NDArray[np.complex64]
            Number of symbols written.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstDespreader
        >>> rng = np.random.default_rng(1)
        >>> code = rng.integers(0, 2, 31).astype(np.uint8)  # length-31 code
        >>> bits = rng.integers(0, 2, 30).astype(np.uint8)   # payload bits
        >>> chips = np.where(code & 1, -1.0, 1.0)    # 0 -> +1, 1 -> -1
        >>> syms = np.where(bits == 1, -1.0, 1.0)             # BPSK symbols
        >>> tx = np.concatenate(
        ...     [np.repeat(s * chips, 4) for s in syms]).astype(np.complex64)
        >>> d = BurstDespreader(code, sf=31, sps=4)
        >>> sym = d.steps(tx)                        # one prompt/symbol
        >>> sym.shape
        (30,)
        >>> hard = (sym.real < 0).astype(np.uint8)            # BPSK decision
        >>> float(np.mean(hard != bits))             # payload recovered
        0.0

        """

    # jm:hand
    def steps_max_out(self) -> int:
        """Max output length steps() can produce for the current state. Use to size the ``out=`` buffer."""

    # jm:hand
    def bits(
        self, x: NDArray[np.complex64], out: NDArray[np.uint8] | None = ...
    ) -> NDArray[np.uint8]:
        """Despread a cf32 block; emit one hard BPSK bit per code period.

        Same streaming kernel as burst_despreader_steps(), but emits the hard decision
        `crealf(prompt) >= 0` instead of the complex symbol.

        Without out=, the returned array is a view into a buffer reused on the
        next call (see bits_max_out() to size an out= buffer for an
        independent, alias-free result).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input CF32 samples, length x_len.
        out : NDArray[np.uint8], optional
            Caller-provided output buffer, at least max(bits_max_out(),
            len(x)) elements.

        Returns
        -------
        NDArray[np.uint8]
            Number of bits written.
        """

    # jm:hand
    def bits_max_out(self) -> int:
        """Max output length bits() can produce for the current state. Use to size the ``out=`` buffer."""

    def set_acq(
        self,
        acq_code: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
        acq_reps: int,
    ) -> None:
        """Enable preamble-aided pull-in: track acq_reps periods of the
        (distinct) acq_code coherently before despreading the payload with the
        data code. Call before feeding the burst; clears when the preamble is
        consumed.

        Track acq_reps periods of acq_code coherently (the unmodulated,
        repeated acquisition preamble — a full ±pi phase discriminator, so the
        loops pull in even a wide residual) before switching to the data code
        for the payload. Call before feeding the burst; the acq mode clears
        automatically once the preamble is consumed, and re-arms on
        dp_burst_despreader_reset(). NB: set_acq re-arms the PREAMBLE only —
        the cumulative burst statistics (lock_metric / snr_est / lock_stat /
        stat_n) are re-armed by dp_burst_despreader_reset(); call it between
        bursts.

        Parameters
        ----------
        acq_code : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
            Acquisition code (0/1), length acq_code_len; copied.
        acq_reps : int
            Number of acq-code periods in the preamble.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstDespreader
        >>> rng = np.random.default_rng(5)
        >>> acq = rng.integers(0, 2, 128).astype(np.uint8)    # long acq code
        >>> data_code = rng.integers(0, 2, 32).astype(np.uint8)
        >>> pbits = rng.integers(0, 2, 40).astype(np.uint8)
        >>> asig = np.where(acq & 1, -1.0, 1.0)
        >>> dch = np.where(data_code & 1, -1.0, 1.0)
        >>> psyms = np.where(pbits == 1, -1.0, 1.0)
        >>> pre = np.concatenate([np.repeat(asig, 4) for _ in range(4)])
        >>> pay = np.concatenate([np.repeat(s * dch, 4) for s in psyms])
        >>> burst = np.concatenate([pre, pay]).astype(np.complex64)
        >>> d = BurstDespreader(data_code, sf=32, sps=4)
        >>> d.set_acq(acq, 4)            # 4 preamble reps, pulls loops in
        >>> out = d.bits(burst)          # preamble emits nothing
        >>> out.shape                    # only the payload symbols come out
        (40,)
        >>> e = np.mean(out != pbits)
        >>> round(float(min(e, 1.0 - e)), 4)
        0.0

        """

    def reset(self) -> None:
        """Re-seed the loops to the create-time phase/frequency; preserve
        config.

        Restores the carrier NCO to the seed frequency and the code phase to
        the seed chip, zeroes the loop accumulators, and clears the cumulative
        burst read-backs (lock_metric / snr_est / lock_stat / stat_n) — the
        spreading code and bandwidths are kept. Call it between bursts so each
        burst's statistics start clean; a prior dp_burst_despreader_set_acq()
        preamble is also re-armed.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstDespreader
        >>> rng = np.random.default_rng(1)
        >>> code = rng.integers(0, 2, 31).astype(np.uint8)
        >>> chips = np.where(code & 1, -1.0, 1.0)
        >>> syms = np.where(rng.integers(0, 2, 30) == 1, -1.0, 1.0)
        >>> tx = np.concatenate(
        ...     [np.repeat(s * chips, 4) for s in syms]).astype(np.complex64)
        >>> d = BurstDespreader(code, sf=31, sps=4)
        >>> first = d.bits(tx)
        >>> d.reset()                          # re-arm for a new burst
        >>> np.array_equal(first, d.bits(tx))  # same as a fresh object
        True

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the BurstDespreader has already been
        destroyed.

        Returns
        -------
        int
            Byte length of one serialized state blob.
        """

    def get_state(self) -> bytes:
        """Serialize this object's mutable state to bytes.

        Captures the state that evolves as the object runs, including any value
        a setter changed after construction, so a blob taken now and restored
        later resumes from this point. Construction parameters are not
        restored: restore into an object built the same way. A core may check
        some of them and refuse a blob built differently.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the BurstDespreader has already been
        destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes, /) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place, including any value a setter
        changed after construction. Construction parameters are not restored:
        restore into an object built the same way. Length is validated against
        `state_bytes()` before the blob is handed to the C core, which may
        reject it too: a core may check some construction parameters and refuse
        a blob built differently.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the BurstDespreader has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def bn_carrier(self) -> float:
        """Carrier (Costas) loop noise bandwidth, normalized to the symbol
        rate.

        Raises
        ------
        ValueError
            On assignment, if ``dp_burst_despreader_set_bn_carrier`` refuses
            the value by returning non-zero. The exception message is
            ``BurstDespreader: bn_carrier is outside the loop filter's domain
            (need bn_carrier >= 0, finite, whose loop gains come out finite)``,
            with the return code appended.
        """
    @bn_carrier.setter
    def bn_carrier(self, value: float) -> None: ...
    @property
    def bn_code(self) -> float:
        """Code (DLL) loop noise bandwidth, normalized to the symbol rate.

        Raises
        ------
        ValueError
            On assignment, if ``dp_burst_despreader_set_bn_code`` refuses the
            value by returning non-zero. The exception message is
            ``BurstDespreader: bn_code is outside the loop filter's domain
            (need bn_code >= 0, finite, whose loop gains come out finite)``,
            with the return code appended.
        """
    @bn_code.setter
    def bn_code(self, value: float) -> None: ...
    @property
    def norm_freq(self) -> float:
        """Current carrier frequency estimate, cycles/sample."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...
    @property
    def code_phase(self) -> float:
        """Current tracked code phase within the symbol, chips."""

    @property
    def lock_metric(self) -> float:
        """Lock indicator in [0,1]: the mean of |Re prompt|/|prompt| over every
        prompt of the burst (cumulative, not EMA). ~1 when phase-locked; ~2/pi
        (0.637) with no carrier.
        """

    @property
    def snr_est(self) -> float:
        """Post-despread SNR estimate over the burst, accumulate-then-ratio:
        (sum Re^2 - sum Im^2)/sum Im^2, clamped >= 0. This is the effective
        post-loop SNR (residual tracking jitter included) - the quantity that
        predicts demodulation performance; it converges to the AWGN-only
        A^2/sigma^2 as the loop bandwidths shrink.
        """

    @property
    def lock_stat(self) -> float:
        """Calibrated whole-burst lock statistic R = sqrt(stat_n * sum Re^2 /
        sum Im^2) — the one-shot analog of the tracking loops' verify-counted
        detectors. Because the noise reference is estimated from as many
        samples as the signal sum, the exact H0 law is R^2 = stat_n * F(stat_n,
        stat_n): gate with R > sqrt(stat_n * det_threshold_f(pfa, stat_n)) —
        exact for every stat_n (a chi-square gate would realize tens of times
        the priced pfa). Payload prompts only; reset() re-arms.
        """

    @property
    def stat_n(self) -> int:
        """Number of prompts folded into the burst statistics so far."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """

    def __enter__(self) -> "BurstDespreader":
        """Enter a context manager, returning this object.

        Lets a BurstDespreader be used in a `with` statement so its C resources
        are released deterministically on exit rather than at collection time.

        Returns
        -------
        BurstDespreader
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
        /,
    ) -> None:
        """Exit a context manager, releasing the BurstDespreader.

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

@final
class PolynomialPhaseEstimator:
    """Create a polynomial-phase estimator.

    Parameters
    ----------
    max_len : int, default 4096
        Maximum input sequence length (>= 4).
    max_rate : float, default 0.0
        Chirp-rate search half-span (cycles/sample^2); 0 searches frequency
        only (a single FFT — near-static Doppler).

    Examples
    --------
    Create with defaults:

    >>> from doppler.dsss import PolynomialPhaseEstimator
    >>> obj = PolynomialPhaseEstimator(max_len=4096, max_rate=0.0)

    """

    def __init__(self, max_len: int = 4096, max_rate: float = 0.0) -> None: ...
    def reset(self) -> None:
        """Do nothing — the estimator keeps no running state between calls.

        A feedforward analyzer computes each estimate purely from the segment
        it is handed, so there is nothing to clear. The method exists only to
        satisfy the common object protocol; calling it is always safe and has
        no effect.

        Examples
        --------
        >>> from doppler.dsss import PolynomialPhaseEstimator
        >>> p = PolynomialPhaseEstimator(max_len=512, max_rate=0.0)
        >>> p.reset()   # no-op: an estimate depends only on the next
        >>> #           segment

        """

    def estimate(self, x: complex, /) -> PolynomialPhaseEstimate:
        """Estimate (freq, chirp-rate) of a complex sequence via the 2-lag HAF.

        Runs the full 2-D matched-filter search in one shot: for each
        chirp-rate hypothesis the segment is dechirped and FFT-ed, and the peak
        of the resulting surface — refined sub-bin by parabolic interpolation
        on both axes — gives the estimate. With max_rate = 0 the rate axis
        collapses to a single FFT (pure Doppler) and the returned rate is
        forced to exactly 0.

        Feed a segment whose modulation has already been stripped (data-aided
        by the known symbols, or non-data-aided by the M-th-power trick —
        remembering that raising to the M-th power scales both returned values
        by M). The result carries freq_norm (cycles/sample), rate_norm
        (cycles/sample^2), and snr_db (a rough peak-to-mean confidence).

        Parameters
        ----------
        x : complex
            Complex segment (modulation already stripped by the caller).

        Returns
        -------
        PolynomialPhaseEstimate
            The estimate; all fields are zeroed if n_in is out of range.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import PolynomialPhaseEstimator
        >>> m = np.arange(512)
        >>> f, r = 0.05, 1e-5               # true Doppler + chirp rate
        >>> x = np.exp(2j*np.pi*(f*m + 0.5*r*m*m)).astype(np.complex64)
        >>> p = PolynomialPhaseEstimator(max_len=512, max_rate=5e-5)
        >>> e = p.estimate(x)                        # one-shot coherent search
        >>> round(e.freq_norm, 4), round(e.rate_norm, 7)
        (0.0501, 1e-05)

        """

    @property
    def max_len(self) -> int:
        """max input length (sizes the plan/scratch)."""

    @property
    def nfft(self) -> int:
        """zero-padded transform length: 4 * next_pow_two (max_len). The 4x is
        deliberate -- a finer frequency grid before the parabolic peak
        refinement, which matters because the input is often short (preamble
        partials, symbol streams). It also sizes `buf`, `spec` and `mag`, so
        the footprint is 4x what a bare next-pow2 would suggest.
        """

    @property
    def max_rate(self) -> float:
        """chirp-rate search half-span (cycles/sample^2)."""

    @property
    def n_rate(self) -> int:
        """number of chirp-rate hypotheses (1 if max_rate=0)."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """

    def __enter__(self) -> "PolynomialPhaseEstimator":
        """Enter a context manager, returning this object.

        Lets a PolynomialPhaseEstimator be used in a `with` statement so its C
        resources are released deterministically on exit rather than at
        collection time.

        Returns
        -------
        PolynomialPhaseEstimator
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
        /,
    ) -> None:
        """Exit a context manager, releasing the PolynomialPhaseEstimator.

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

@final
class BurstDemod:
    """The Python binding's constructor: dp_burst_demod_create_desc without the
    `why` out-parameter.

    Parameters
    ----------
    data_code : NDArray[np.uint8]
        the data spreading code, 0/1 chips.
    frame : Any
        The frame description the transmitter spread (a Frame or a FrameDesc,
        built or not): its first field is the sync word the receiver correlates
        for, and its layout is the frame's length. Copied at construction, so
        it need not outlive the receiver.
    spc : int, default 4
        samples per chip.
    chip_rate : float, default 1.0e6
        chips per second.
    carrier_hz : float, default 0.0
        the carrier the baseband is offset by, Hz.
    max_rate : float, default 0.0
        the Doppler rate searched, cycles/sample^2.
    est_segments : int, default 10
        partials per acquisition period for the estimate.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``BurstDemod: invalid
        parameter (need a non-empty data_code, spc >= 1, chip_rate > 0,
        max_rate >= 0, est_segments >= 1) or a frame description whose first
        field is not known bits (empty, derived by a stage, a data field,
        covered by a stage, or named "preamble"), or whose stages emit a new
        stream (a convolutional code)``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.dsss import BurstDemod
    >>> from doppler.wfm import Frame
    >>> spc, acq_sf, reps, data_sf = 4, 500, 5, 50
    >>> sync = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)
    >>> acode = ((np.arange(acq_sf) * 2654435761 >> 13) & 1).astype(
    ...     np.uint8)
    >>> dcode = ((np.arange(data_sf) * 40503 >> 7) & 1).astype(np.uint8)
    >>> payload = ((np.arange(64) * 7 + 3) & 1).astype(np.uint8)
    >>> desc = Frame(sync=sync, payload=payload, crc="crc16")
    >>> frame = desc.bits()    # sync | payload | CRC-16: ONE description
    >>> csign = lambda b: np.where(np.asarray(b) & 1, -1.0, 1.0)
    >>> chips = ([np.tile(csign(acode), reps)]
    ...          + [csign(b) * csign(dcode) for b in frame])
    >>> bb = np.repeat(np.concatenate(chips), spc).astype(np.complex64)
    >>> n = np.arange(len(bb))
    >>> f0 = 0.012
    >>> x = (bb * np.exp(2j * np.pi * f0 * n)).astype(np.complex64)
    >>> d = BurstDemod(dcode, desc, spc=spc, chip_rate=1e6)
    >>> d.set_preamble(acode, reps)   # unmodulated (f0, rate) preamble
    >>> d.set_prior(f0, 0)            # coarse Doppler + preamble start
    >>> bits = d.demod(x)      # estimate -> dechirp -> despread -> slice
    >>> bool(np.array_equal(bits, frame))   # the FRAME, not the payload
    True

    """

    def __init__(
        self,
        data_code: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
        frame: object,
        spc: int = 4,
        chip_rate: float = 1.0e6,
        carrier_hz: float = 0.0,
        max_rate: float = 0.0,
        est_segments: int = 10,
    ) -> None: ...
    def reset(self) -> None:
        """Clear the per-burst read-backs, leaving the configuration intact.

        Zeros the after-demod fields (frame_offset, n_symbols, and the est_*
        estimates) so a stale result cannot be mistaken for a fresh one. The
        spreading codes, sync word, and prior set up before the first burst are
        preserved, so the object is immediately ready to demodulate the next
        burst.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstDemod
        >>> dcode = (np.arange(50) & 1).astype(np.uint8)
        >>> from doppler.wfm import Frame
        >>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(64, np.uint8),
        ...              crc="crc16")      # 13 + 64 + 16 = 93 symbols
        >>> d = BurstDemod(dcode, desc, spc=4, chip_rate=1e6)
        >>> d.reset()          # clears the estimates, keeps the config
        >>> d.frame_offset
        0

        """

    def set_preamble(
        self,
        acq_code: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
        reps: int,
    ) -> None:
        """Set the (unmodulated) acquisition preamble code + repetition count
        used for the feedforward (f0, rate) estimate.

        The preamble is the acq spreading code transmitted reps times with no
        data modulation; demod() segment-despreads it into partial correlations
        and feeds those to the polynomial-phase estimator to recover the coarse
        (frequency, chirp-rate). Call once after construction; the code is
        copied.

        Parameters
        ----------
        acq_code : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
            Acq preamble spreading code, one 0/1 chip per element; copied into
            the object.
        reps : int
            Number of preamble repetitions in the burst.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstDemod
        >>> dcode = (np.arange(50) & 1).astype(np.uint8)
        >>> from doppler.wfm import Frame
        >>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(64, np.uint8),
        ...              crc="crc16")      # 13 + 64 + 16 = 93 symbols
        >>> d = BurstDemod(dcode, desc, spc=4, chip_rate=1e6)
        >>> acode = (np.arange(500) & 1).astype(np.uint8)  # unmodulated
        >>> d.set_preamble(acode, reps=5)  # 5 reps drive the (f0, rate) fit

        """

    def llrs(
        self,
        count: int = 1,
        out: npt.NDArray[np.float32] | None = None,
    ) -> NDArray[np.float32]:
        """The soft bits of the last demod() — one LLR per FRAME bit, in
        `mpsk_soft_demap`'s convention: positive means bit 0, so `L < 0`
        reproduces exactly the bits demod() returned. `Re(sym * derot)` IS the
        log-likelihood ratio up to a scale and used to be computed, sliced to
        one bit and freed; a hard decision costs roughly 2 dB of the coding
        gain a soft-input decoder exists to deliver. Spans the whole frame
        rather than the payload alone, because a code covers what its
        description says it covers. Scaled by `est_n0`, the burst's own noise
        estimate, so LLRs from different bursts are comparable — a Viterbi
        would not care, but combining across bursts does.

        `crealf(sym * derot)` IS the log-likelihood ratio up to a scale, and it
        was computed, sliced to one bit and freed on every burst. A hard
        decision throws away roughly 2 dB of the coding gain a soft-input
        decoder exists to deliver (`mpsk_soft_demap`'s own docstring), so this
        is what makes a coded burst worth coding.

        **The convention is not a new one**: `mpsk_soft_demap`'s, which is
        `mpsk_demap`'s decision rule seen a second way. Positive means bit 0,
        so `L < 0` reproduces exactly the bits demod() returned — asserted in
        the tests rather than assumed.

        Spans the WHOLE frame, not just the payload, because a code covers what
        its description says it covers and a decoder needs the bits the code
        protects. The payload's own span is `field_off`/`field_bits` of the
        layout.

        Scaled by est_n0 rather than left raw: a Viterbi is invariant to a
        positive scale, but LLRs from different bursts are not comparable
        without one, and combining across bursts needs them to be.

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : npt.NDArray[np.float32] | None
            Receives the LLRs, one per frame bit.

        Returns
        -------
        NDArray[np.float32]
            LLRs written — `min(frame bits, max_out)`, or 0 if the last demod()
            produced no frame.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstDemod
        >>> dcode = (np.arange(50) & 1).astype(np.uint8)
        >>> from doppler.wfm import Frame
        >>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(64, np.uint8),
        ...              crc="crc16")      # 13 + 64 + 16 = 93 symbols
        >>> d = BurstDemod(dcode, desc, spc=4, chip_rate=1e6)
        >>> d.llrs_max_out(1)          # one per frame symbol
        93

        """

    def llrs_max_out(self, n: int, /) -> int:
        """Max LLRs dp_burst_demod_llrs() writes: the frame's length in bits.

        Parameters
        ----------
        n : int
            Ignored — the count is the last demod()'s frame.

        Returns
        -------
        int
            Output.
        """

    def symbols(
        self,
        count: int = 1,
        out: npt.NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """The DEROTATED complex symbols of the last demod() — the
        constellation `llrs()` is the real part of. Same span and
        normalisation: the whole frame, scaled to unit mean-|Re|, so
        `symbols.real` is `llrs()` up to `est_n0`. The quadrature is why this
        exists: after derotation the real axis carries the signal and the
        imaginary axis carries noise alone, so a residual phase error — which
        scales Re by `cos(phi)` without adding noise — is indistinguishable
        from a genuine amplitude or SNR loss in mean |LLR|, in LLR spread and
        in BER alike. Measured over 20000 BPSK symbols, a 30° phase error and
        an amplitude loss of `cos(30°)` agreed to three decimals in all three
        and differed only in Q/I energy, 0.386 against 0.077. That is a
        pointing problem against a link-budget one, on a burst this object
        already characterised well enough to know. It was built either way and
        freed unread (doppler#1087).

        Same span and same normalisation as dp_burst_demod_llrs(): the whole
        frame, scaled to unit mean-|Re| by the burst's own estimate, so
        `crealf(symbols[k])` is that bit's LLR up to est_n0.

        The quadrature is why this exists. After derotation the real axis
        carries the signal and the imaginary axis carries noise alone, so Q is
        diagnostic: a residual phase error scales Re by `cos(phi)` WITHOUT
        adding noise, which makes it indistinguishable from a genuine amplitude
        or SNR loss in mean |LLR|, in LLR spread and in BER alike. Measured
        over 20000 BPSK symbols, a 30 degree phase error and an amplitude loss
        of `cos(30 deg)` agreed to three decimals in all three, and differed
        only in Q/I energy — 0.386 against 0.077 (doppler#1087). That is the
        difference between a pointing problem and a link-budget one, on a burst
        this object already characterised well enough to know.

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : npt.NDArray[np.complex64] | None
            Receives the symbols, one per frame bit.

        Returns
        -------
        NDArray[np.complex64]
            Symbols written — `min(frame bits, max_out)`, or 0 if the last
            demod() produced no frame.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstDemod
        >>> dcode = (np.arange(50) & 1).astype(np.uint8)
        >>> from doppler.wfm import Frame
        >>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(64, np.uint8),
        ...              crc="crc16")      # 13 + 64 + 16 = 93 symbols
        >>> d = BurstDemod(dcode, desc, spc=4, chip_rate=1e6)
        >>> d.symbols_max_out(1)       # one per frame symbol, as llrs()
        93

        """

    def symbols_max_out(self, n: int, /) -> int:
        """Max symbols dp_burst_demod_symbols() writes: the frame's length.

        Parameters
        ----------
        n : int
            Ignored — the count is the last demod()'s frame.

        Returns
        -------
        int
            Output.
        """

    def set_prior(self, f0_coarse: float, start: int) -> None:
        """Seed from acquisition: coarse Doppler (cycles/sample at the input
        rate) and the preamble start sample.

        These come from the upstream acquisition stage: f0_coarse centres the
        feedforward frequency search near the true Doppler, and start tells
        demod() where the preamble begins within the burst so it despreads the
        right samples. Call once per burst before demod().

        Parameters
        ----------
        f0_coarse : float
            Coarse Doppler prior (cycles/sample at the input rate).
        start : int
            Preamble start sample index within the burst.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstDemod
        >>> dcode = (np.arange(50) & 1).astype(np.uint8)
        >>> from doppler.wfm import Frame
        >>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(64, np.uint8),
        ...              crc="crc16")      # 13 + 64 + 16 = 93 symbols
        >>> d = BurstDemod(dcode, desc, spc=4, chip_rate=1e6)
        >>> d.set_prior(0.012, start=0)   # coarse Doppler + start, from acq

        """

    # jm:hand
    def demod(
        self, x: NDArray[np.complex64], out: NDArray[np.uint8] | None = ...
    ) -> NDArray[np.uint8]:
        """Demodulate a burst (preamble + frame); return the frame's bits: sync header, payload, then CRC-16 trailer. Read-back properties report the estimates + CRC validity.

        Without out=, the returned array is a view into a buffer reused on
        the next call (see demod_max_out(), or payload_len, to size an out=
        buffer for an independent, alias-free result).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.
        out : NDArray[np.uint8], optional
            Caller-provided output buffer, at least max(demod_max_out(),
            len(x)) elements.

        Returns
        -------
        NDArray[np.uint8]
            Number of bits written (0 on failure / too-short burst). The read-back fields (frame_valid, est_*, frame_offset) are updated.
        """

    # jm:hand
    def demod_max_out(self) -> int:
        """Max output length demod() can produce for the current state. Use to size the ``out=`` buffer."""

    @property
    def frame_offset(self) -> int:
        """symbol offset of the sync word."""

    @property
    def n_symbols(self) -> int:
        """despread data symbols produced."""

    @property
    def est_freq_hz(self) -> float:
        """estimated residual Doppler (Hz)."""

    @property
    def est_rate_hz(self) -> float:
        """estimated Doppler rate (Hz/s)."""

    @property
    def est_cn0_dbhz(self) -> float:
        """Carrier-to-noise DENSITY of the last demod(), dB-Hz, referred to the
        CHANNEL. The realized symbol estimate `1/est_n0` lifted to a density by
        the symbol rate (`chip_rate/data_sf`, the identity C/N0 = Es/N0 * Rs)
        and corrected for the despreading loss that `est_timing_chips`
        measures. Sample-rate invariant, so a front-end rate change does not
        move it, and it does not move when the receiver's own start is off by a
        fraction of a chip -- which a realized SNR does, by 4.8 dB at half a
        chip. Zero until a demod() produces a frame. Replaces `est_snr_db`,
        which was never an SNR: it published the preamble estimator's spectral
        prominence, carrying the full coherent processing gain and swinging 33
        dB with alignment at a fixed input (doppler#1304).
        """

    @property
    def est_timing_chips(self) -> float:
        """Burst-start error the last demod() MEASURED, in chips, signed,
        relative to the `start` given to set_prior(). The whole-sample part was
        removed before despreading; only the sub-sample fraction remains a
        loss, and only that fraction is taken out of `est_cn0_dbhz`. The
        symbols keep the loss either way. Acquisition resolves a start to one
        SAMPLE, so a residual of a fraction of a chip is structural rather than
        a caller's error -- which is why the number is published instead of
        assumed to be zero.
        """

    @property
    def frame_syms(self) -> int:
        """symbols the frame occupies, sync word included — the description's
        layout length, read at create. What they MEAN is the frame
        description's business, one layer up.
        """

    @property
    def est_n0(self) -> float:
        """Noise power the LLRs are scaled by, referred to unit symbol
        amplitude — `2·var(Im)/mean|Re|²` over the derotated frame, floored at
        1e-12 so a noiseless capture stays finite. `llrs()` is divided by this,
        so multiplying back recovers the raw projection, and two bursts are
        comparable only because both were scaled by their own estimate. Reading
        it beside `symbols()` is what turns the constellation into an absolute
        measurement rather than a picture.
        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """

    def __enter__(self) -> "BurstDemod":
        """Enter a context manager, returning this object.

        Lets a BurstDemod be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        BurstDemod
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
        /,
    ) -> None:
        """Exit a context manager, releasing the BurstDemod.

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

@final
class DsssReceiver:
    """Create a DSSS receiver in the searching state.

    Parameters
    ----------
    code : NDArray[np.uint8]
        Spreading code, one 0/1 chip per element (0 -> +1, 1 -> -1 BPSK; only
        the low bit is used, so pass 0/1, not +/-1).
    chip_rate : float, default 1000000.0
        Chip rate, Hz. Required.
    symbol_rate : float, default 1000.0
        Data-symbol rate, Hz. Required — passed straight to the embedded
        Acquisition's own `symbol_rate` (diagnostic there; see
        `dp_acq_create_continuous()`).
    spc : int, default 2
        Samples/chip (front-end oversample); default 2 (fs = 2x chip_rate).
    m : int, default 2
        PSK order, 2/4/8; default 2 (BPSK).
    cn0_dbhz : float, default 55.0
        Design C/N0 for acquisition sizing, dB-Hz; default 55.0.
    pfa : float, default 1e-3
        Acquisition false-alarm target; default 1e-3.
    pd : float, default 0.9
        Acquisition detection-probability target; default 0.9.
    doppler_uncertainty : float, default 100.0
        One-sided Doppler search half-range, Hz; default 100.0.
    segments : int, default 4
        Dll's partial correlations per code epoch: its despread output rate and
        the grain of its look-back window, independent of `sps` (see the module
        docstring); default 4.
    sps : int, default 8
        MpskReceiver's samples/symbol, reached by an internal RateConverter
        bridging the despreader's own partial rate to this rate; default 8,
        MpskReceiver's own constructor default.
    differential : int, default 0
        MpskReceiver's differential (rotation- invariant) demap; default 0
        (coherent).

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``DsssReceiver: invalid
        parameter (need a code of at least 2 chips, a finite chip_rate > 0, a
        finite symbol_rate > 0 and <= chip_rate, spc >= 1, m in {2,4,8},
        segments >= 1, sps >= 2 -- sps = 1 cannot carry an m_out, whose
        smallest legal value is 2 and which MpskReceiver requires sps to reach;
        a chain resampling ratio, sps * symbol_rate * code_len / (chip_rate *
        segments), that is a normal double; and for the searcher a finite
        cn0_dbhz, 0 < pfa < 1, 0 < pd < 1, a finite doppler_uncertainty >=
        0)``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.dsss import DsssReceiver
    >>> from doppler.wfm import Gold
    >>> sf, chip, sym, spc = 1023, 3.0e6, 2100.0, 2
    >>> fs, te, tsym = chip * spc, sf * spc, chip * spc / sym
    >>> code = np.asarray(Gold().generate(sf)).astype(np.uint8)
    >>> csign = np.where(code & 1, -1.0, 1.0)
    >>> rng = np.random.default_rng(6)
    >>> n = int(400 * tsym) + 2 * te            # 400 BPSK data symbols
    >>> idx = np.arange(n)
    >>> data = (rng.integers(0, 2, 404) * 2 - 1).astype(float)
    >>> si = np.clip((idx / tsym).astype(int), 0, 403)
    >>> spread = data[si] * csign[(idx // spc) % sf]        # DSSS chips
    >>> sig = spread * np.exp(2j * np.pi * (50.0 / fs) * idx)  # +50 Hz
    >>> pre = 3 * te                     # noise-only lead-in, pre-signal
    >>> sigma = np.sqrt(fs / 10 ** (90.0 / 10))            # ~90 dB-Hz C/N0
    >>> noise = (sigma / np.sqrt(2)) * (rng.standard_normal(pre + n)
    ...          + 1j * rng.standard_normal(pre + n))
    >>> x = (np.concatenate([np.zeros(pre), sig]).astype(np.complex64)
    ...      + noise.astype(np.complex64))
    >>> rx = DsssReceiver(code, chip_rate=chip, symbol_rate=sym, spc=spc,
    ...                   cn0_dbhz=55.0, doppler_uncertainty=100.0)
    >>> syms = [rx.steps(x[p:p + te]) for p in range(0, len(x) - te, te)]
    >>> syms = np.concatenate([s for s in syms if len(s)])
    >>> rx.tracking                  # acquired, now demodulating
    1
    >>> len(syms) > 300              # a few hundred symbols recovered
    True

    Nearly all the energy lands on I, so the BPSK phase is resolved:

    >>> bool(np.mean(syms.real**2) > 10 * np.mean(syms.imag**2))
    True

    """

    def __init__(
        self,
        code: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
        chip_rate: float = 1000000.0,
        symbol_rate: float = 1000.0,
        spc: int = 2,
        m: int = 2,
        cn0_dbhz: float = 55.0,
        pfa: float = 1e-3,
        pd: float = 0.9,
        doppler_uncertainty: float = 100.0,
        segments: int = 4,
        sps: int = 8,
        differential: int = 0,
    ) -> None: ...
    def steps(
        self,
        x: npt.NDArray[np.complex64],
        out: npt.NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Stream raw cf32 samples through the receiver. While searching,
        samples feed the embedded Acquisition and nothing is emitted (an empty
        array is normal, not an error). The moment a hit fires,
        Dll/RateConverter/MpskReceiver are built and seeded from it -- the same
        phase-inversion hand-off and rate-bridging this project's
        async-DSSS-receiver gallery story validated by hand -- and the
        unconsumed tail of this same call is handed straight to them, so no
        samples are dropped at the transition. While tracking, samples feed Dll
        -> RateConverter -> MpskReceiver in sequence and demodulated symbols
        are returned. Accepts any block size; state carries across calls.

        While searching, samples feed the embedded Acquisition and nothing is
        emitted (0 return is normal, not an error). The moment a hit fires,
        `Dll`/`RateConverter`/`MpskReceiver` are built and seeded from it, and
        the rest of THIS call from where the hit's frame ended — the hit's own
        `samples_consumed`, so no samples are dropped or double-fed — is handed
        straight to them in the same call. While tracking, samples feed `Dll ->
        RateConverter -> MpskReceiver` in sequence. Accepts any block size;
        state carries across calls (`Acquisition`/`Dll`/
        `RateConverter`/`MpskReceiver` are all already block-size invariant, so
        this object needs no ring-buffering of its own).

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            Input cf32 samples.
        out : npt.NDArray[np.complex64] | None
            Output symbols; caller provides max_out capacity.

        Returns
        -------
        NDArray[np.complex64]
            Number of symbols written (0 while searching, or while tracking
            with not yet a full symbol's worth of input).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import DsssReceiver
        >>> from doppler.wfm import Gold
        >>> sf, chip, sym, spc = 1023, 3.0e6, 2100.0, 2
        >>> fs, te, tsym = chip * spc, sf * spc, chip * spc / sym
        >>> code = np.asarray(Gold().generate(sf)).astype(np.uint8)
        >>> csign = np.where(code & 1, -1.0, 1.0)
        >>> rng = np.random.default_rng(6)
        >>> n = int(400 * tsym) + 2 * te            # 400 BPSK data symbols
        >>> idx = np.arange(n)
        >>> data = (rng.integers(0, 2, 404) * 2 - 1).astype(float)
        >>> si = np.clip((idx / tsym).astype(int), 0, 403)
        >>> spread = data[si] * csign[(idx // spc) % sf]        # DSSS chips
        >>> sig = spread * np.exp(2j * np.pi * (50.0 / fs) * idx)  # +50 Hz
        >>> pre = 3 * te                     # noise-only lead-in, pre-signal
        >>> sigma = np.sqrt(fs / 10 ** (90.0 / 10))            # ~90 dB-Hz C/N0
        >>> noise = (sigma / np.sqrt(2)) * (rng.standard_normal(pre + n)
        ...          + 1j * rng.standard_normal(pre + n))
        >>> x = (np.concatenate([np.zeros(pre), sig]).astype(np.complex64)
        ...      + noise.astype(np.complex64))
        >>> rx = DsssReceiver(code, chip_rate=chip, symbol_rate=sym, spc=spc,
        ...                   cn0_dbhz=55.0, doppler_uncertainty=100.0)
        >>> syms = [rx.steps(x[p:p + te]) for p in range(0, len(x) - te, te)]
        >>> syms = np.concatenate([s for s in syms if len(s)])
        >>> rx.tracking                  # acquired and now demodulating
        1
        >>> len(syms) > 300              # a few hundred symbols recovered
        True

        Nearly all the energy lands on I, so the BPSK phase is resolved:

        >>> bool(np.mean(syms.real**2) > 10 * np.mean(syms.imag**2))
        True

        """

    def steps_max_out(self) -> int:
        """Largest number of samples steps() can return in the current state.

        Size an `out=` buffer with this before calling steps(), or use it to
        allocate one up front. The bound is this object's own: what it depends
        on is a property of the algorithm, so a header block on steps_max_out()
        replaces this text.

        Returns
        -------
        int
            Upper bound on the output length; the actual call may return fewer.
        """

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the embedded Acquisition's search grid directly, bypassing the
        symbol_rate-driven auto-sizing -- the escape hatch for a power user who
        wants a specific (doppler_bins, n_noncoh). Only meaningful while
        searching.

        Parameters
        ----------
        doppler_bins : int
            Number of Doppler window tiles to search (>= 1); capped by the
            create-time `doppler_uncertainty` span (one tile per code-epoch
            Doppler bin width).
        n_noncoh : int
            Non-coherent looks accumulated per grid cell (1..256); more looks
            buys sensitivity at the cost of dwell, replacing the auto-sized
            count.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``configure_search_raw failed``, with the return code appended
            (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import DsssReceiver
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0, spc=2)
        >>> rx.configure_search_raw(doppler_bins=1, n_noncoh=16)  # pin it
        >>> rx.tracking                # still searching, on the pinned grid
        0

        """

    def configure_lock_raw(
        self,
        up_thresh: float,
        down_thresh: float,
        n_looks: int,
        alpha: float,
        n_up: int,
        n_down: int,
    ) -> None:
        """Re-tune the embedded Dll's code-lock detector directly. Only
        meaningful once tracking has begun; a no-op while searching.

        Parameters
        ----------
        up_thresh : float
            CFAR-statistic level to declare code lock (hit when the statistic
            exceeds it).
        down_thresh : float
            Level below which a look is a miss; choose <= up_thresh for level
            hysteresis.
        n_looks : int
            Looks per decision — the DLL's non-coherent integration depth
            feeding one statistic.
        alpha : float
            EMA smoothing coefficient on the lock statistic (0..1); smaller is
            smoother/slower.
        n_up : int
            Consecutive hits required to declare lock.
        n_down : int
            Consecutive misses required to drop lock.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import DsssReceiver
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0, spc=2)
        >>> rx.configure_lock_raw(up_thresh=0.4, down_thresh=0.2, n_looks=20,
        ...                       alpha=0.1, n_up=5, n_down=3)
        >>> rx.tracking                # a no-op until a hit builds the Dll
        0

        """

    def configure_chain_raw(self, segments: int, sps: int, n: int) -> None:
        """Pin the despread/resample/demod grid directly, bypassing the
        create-time segments/sps defaults -- segments (Dll's tracking
        parameter) and sps/n (MpskReceiver's rate/carrier-arm parameters) stay
        independently overridable here, still bridged by a freshly-sized
        RateConverter, never coupled to each other. Only meaningful once
        tracking; rebuilds the chain with every replacement allocated first, so
        a pin refused on its domain leaves the receiver on its prior grid. A
        grid too large for the chain to allocate still aborts; sizing it out is
        #2112 work.

        The escape hatch for the one composition-specific knob this object adds
        beyond its children's own: `segments` (Dll's tracking parameter) and
        `sps`/`n` (MpskReceiver's samples and terminal outputs per symbol) are
        independently overridable here, still bridged by a freshly-sized
        `RateConverter` -- never coupled to each other (see the module
        docstring). It rebuilds `dll`/`rc`/`rx` at once, allocating every
        replacement before freeing the old ones (`Acquisition`'s `acq_regrid()`
        discipline), and the pinned grid is the one every later chain is built
        on: the next hit's hand-off and reset() both rebuild at it.

        The new grid is checked before anything is rebuilt, and refused with
        the receiver left on its prior grid: segments >= 1, sps >= 2, a symbol
        no faster than a chip and a resampling ratio that is a normal double
        (doppler#2103), and an n MpskReceiver would accept (doppler#2426).
        Still an abort: an allocation failure in the rebuild, which runs under
        dp_xnn (doppler#2112).

        Parameters
        ----------
        segments : int
            Dll tracking segments per code period, >= 1.
        sps : int
            MpskReceiver samples per symbol (the resample target), >= 2.
        n : int
            MpskReceiver's terminal outputs per symbol (`m_out`): inside
            dp_ratesync_m_ok() (even, 2..8), and dividing sps.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``configure_chain_raw failed``, with the return code appended
            (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import DsssReceiver
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0, spc=2)
        >>> rx.configure_chain_raw(segments=6, sps=8, n=8)  # re-pin the chain
        >>> rx.segments                       # tracking grid updated in place
        6

        """

    def reset(self) -> None:
        """Return to the searching state: resets the embedded Acquisition and
        frees Dll/RateConverter/MpskReceiver (rebuilt from scratch on the next
        hit).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import DsssReceiver
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0, spc=2)
        >>> rx.reset()                 # abort any lock, hunt from scratch
        >>> (rx.tracking, rx.chip_phase)   # back to searching, all cleared
        (0, 0.0)

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the DsssReceiver has already been destroyed.

        Returns
        -------
        int
            Byte length of one serialized state blob.
        """

    def get_state(self) -> bytes:
        """Serialize this object's mutable state to bytes.

        Captures the state that evolves as the object runs, including any value
        a setter changed after construction, so a blob taken now and restored
        later resumes from this point. Construction parameters are not
        restored: restore into an object built the same way. A core may check
        some of them and refuse a blob built differently.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the DsssReceiver has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes, /) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place, including any value a setter
        changed after construction. Construction parameters are not restored:
        restore into an object built the same way. Length is validated against
        `state_bytes()` before the blob is handed to the C core, which may
        reject it too: a core may check some construction parameters and refuse
        a blob built differently.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the DsssReceiver has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def tracking(self) -> int:
        """0 = searching, 1 = locked and demodulating."""

    @property
    def doppler_hz(self) -> float:
        """Doppler hz."""

    @property
    def cn0_dbhz_est(self) -> float:
        """Cached from the winning acquisition hit."""

    @property
    def segments(self) -> int:
        """Dll's own tracking parameter."""

    @property
    def sps(self) -> int:
        """MpskReceiver's own samples/symbol."""

    @property
    def n(self) -> int:
        """MpskReceiver's own carrier-arm count."""

    @property
    def chip_phase(self) -> float:
        """Dll's live tracked code phase (chips); 0.0 while searching."""

    @property
    def code_rate(self) -> float:
        """Dll's own tracking-quality indicator; 1.0 while searching."""

    @property
    def lock(self) -> float:
        """MpskReceiver's carrier lock EMA; 0.0 while searching."""

    @property
    def norm_freq(self) -> float:
        """MpskReceiver's tracked carrier frequency; 0.0 while searching."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """

    def __enter__(self) -> "DsssReceiver":
        """Enter a context manager, returning this object.

        Lets a DsssReceiver be used in a `with` statement so its C resources
        are released deterministically on exit rather than at collection time.

        Returns
        -------
        DsssReceiver
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
        /,
    ) -> None:
        """Exit a context manager, releasing the DsssReceiver.

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

@final
class AsyncDsssReceiver:
    """Create an AsyncDsssReceiver in the searching state.

    Parameters
    ----------
    code : NDArray[np.uint8]
        Spreading code, one 0/1 chip per element (0 -> +1, 1 -> -1 BPSK; only
        the low bit is used, so pass 0/1, not +/-1).
    chip_rate : float, default 1000000.0
        Chip rate, Hz.
    symbol_rate : float, default 1000.0
        Data-symbol rate, Hz: finite, > 0, at most `chip_rate`, and at most
        2^20 Dll partials per symbol at `segments`.
    spc : int, default 2
        Samples/chip; default 2.
    m : int, default 2
        PSK order, 2/4/8; default 2 (BPSK).
    cn0_dbhz : float, default 55.0
        Design C/N0, dB-Hz; default 55.0 -- feeds BOTH the embedded
        Acquisition's own sizing AND (derated by `refine_design_margin_db`)
        CarrierAcquisition's `design_snr`.
    pfa : float, default 1e-3
        Acquisition false-alarm target; default 1e-3. Also CarrierAcquisition's
        own `pfa`.
    pd : float, default 0.9
        Acquisition detection-probability target; default 0.9. Also
        CarrierAcquisition's own `pd`.
    doppler_uncertainty : float, default 100.0
        One-sided Doppler search half-range, Hz; default 100.0.
    segments : int, default 4
        Live-tracking Dll's own segments; default 4.
    sps : int, default 8
        MpskReceiver's samples/symbol; default 8.
    differential : int, default 0
        MpskReceiver's differential demap; default 0 (coherent). It acts in
        MpskReceiver's bits(), which this object does not offer, so steps()'
        symbols are the same either way (#2471).
    refine_max_error_db : float, default 0.5
        Max tolerable async-lookback correlation-power loss driving the
        refine-stage collection Dll's coherent-I&D window count via
        dp_dll_lookback_segments(). Oversampling the epoch is required for the
        asynchronous data: the residual carrier rides a ~symbol_rate-wide
        data-modulated spectrum, so segments>1 (default yields 11 at
        tsamps=2046) samples it above Nyquist; segments=1 undersamples and
        aliases it. Default 0.5.
    refine_samples_per_symbol : int, default 4
        CarrierAcquisition's own operating rate = this * symbol_rate; default
        4.
    refine_design_margin_db : float, default 14.0
        Empirical derating of cn0_dbhz before CarrierAcquisition's design_snr;
        default 14.0.
    refine_n_fft : int, default 64
        CarrierAcquisition's own block size; default 64.
    refine_zero_pad : int, default 8
        CarrierAcquisition's own zero_pad; default 8.
    refine_sequential : bool, default False
        CarrierAcquisition's sequential mode; default false -- its early
        per-block test fires on far too little averaging at SPEC's Es/N0 floor
        (as few as 4 blocks, 150-200+ Hz off), so false waits the full
        design_snr-derived dwell.
    refine_max_n_blocks : int, default 100000
        CarrierAcquisition's give-up cap in sequential mode, and the cap on the
        dwell in either mode; default 100000.
    carrier_freq_hz : float, default 0.0
        Nominal RF carrier frequency, Hz, enabling carrier->code aiding; 0.0
        (default) = off. When > 0, the coupled code-rate Doppler
        (carrier_offset/carrier_freq) is fed to the tracking Dll via
        dp_dll_set_rate_aid() so the code loop rides a dilated clock the
        discriminator alone can't pull in at low SNR. Set to the receiver's own
        downlink RF frequency for a physically-coupled Doppler capture.
        Coupled, it is above half the sample rate (dp_acq_carrier_freq_ok()).
    lost_confirm_s : float, default 0.0
        Release rule: both lock flags down, continuously, for longer than this
        many seconds puts the receiver in the lost state (see get_lost()). Size
        it past the longest fade the link must ride. The clock also runs from
        the first tracking sample, when neither flag is up yet, so a hand-off
        that never locks within the interval is released the same way as an
        emitter that leaves. Default 0.0 = never -- the exit is then reset().
    reacquire : bool, default False
        Release on symbol lock alone, and search again by itself. With it set,
        the release clock counts samples of tracking with the SYMBOL lock flag
        down (code lock is not consulted), and when it passes lost_confirm_s
        the receiver resets itself to searching instead of entering lost: it
        never waits for reset(). A pool of receivers that are always online
        uses it. The clock runs from the first tracking sample, as above, so a
        track that never locks (a noise false alarm) is released too; size
        lost_confirm_s past the pull-in time to symbol lock, or a slow lock is
        released before it comes. Requires a lost_confirm_s of at least one
        input sample, since a release that never fires would never search
        again; a blob in the lost state is refused by set_state(). Default
        false: the release rule above.
    code_only_s : float, default 0.0
        Seconds of code-only signal the waveform holds at any chip phase (a
        symbol-free window; design async-dsss-parallel.md §4), >= 0 and finite.
        The receiver hands its searcher `code_only_epochs = floor(code_only_s *
        chip_rate / code_len) - 1` (at least 1), and the searcher's slow-time
        depth follows: `coherent_bins = D`, at most `(code_only_epochs + 1)/2`,
        rounded down to a 7-smooth length (dp_acq_create_continuous()). Default
        0: no window, D = 1.
    doppler_rate : float, default 0.0
        Doppler rate in Hz/s, >= 0 and finite, that the slow-time depth is
        bounded against: at most `f_epoch / sqrt(2 * doppler_rate)` epochs
        (dp_acq_create_continuous()). Default 0: the window is the only bound.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``AsyncDsssReceiver:
        invalid parameter (need a code of at least 2 chips, a finite chip_rate
        > 0, a finite symbol_rate > 0 and <= chip_rate, spc >= 1, m in {2,4,8},
        segments >= 1, sps >= 2, carrier_freq_hz 0 or finite and above
        chip_rate * spc / 2, lost_confirm_s >= 0 (and at least one input sample
        with reacquire), refine_samples_per_symbol >= 1, refine_n_fft >= 1,
        refine_zero_pad >= 1, a symbol of at most 2^20 Dll partials, segments *
        chip_rate / (code_len * symbol_rate) <= 1048576; and for the searcher a
        finite cn0_dbhz, 0 < pfa < 1, 0 < pd < 1, a finite doppler_uncertainty
        >= 0)``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.dsss import AsyncDsssReceiver
    >>> from doppler.wfm import Gold
    >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
    >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
    ...                        spc=2, doppler_uncertainty=500.0)
    >>> (rx.tracking, rx.refining, rx.lost)       # searching
    (0, 0, 0)

    """

    def __init__(
        self,
        code: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
        chip_rate: float = 1000000.0,
        symbol_rate: float = 1000.0,
        spc: int = 2,
        m: int = 2,
        cn0_dbhz: float = 55.0,
        pfa: float = 1e-3,
        pd: float = 0.9,
        doppler_uncertainty: float = 100.0,
        segments: int = 4,
        sps: int = 8,
        differential: int = 0,
        refine_max_error_db: float = 0.5,
        refine_samples_per_symbol: int = 4,
        refine_design_margin_db: float = 14.0,
        refine_n_fft: int = 64,
        refine_zero_pad: int = 8,
        refine_sequential: bool = False,
        refine_max_n_blocks: int = 100000,
        carrier_freq_hz: float = 0.0,
        lost_confirm_s: float = 0.0,
        reacquire: bool = False,
        code_only_s: float = 0.0,
        doppler_rate: float = 0.0,
    ) -> None: ...
    def steps(
        self,
        x: npt.NDArray[np.complex64],
        out: npt.NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Stream raw cf32 samples; emit demodulated symbols once tracking.

        Drives the state machine. It emits nothing while searching or refining
        (an empty return is normal, not an error): a hit seeds the
        frozen-carrier refine chain, `CarrierAcquisition` sharpens the coarse
        Doppler estimate, and only once it is ready (or gives up) is the live
        tracking chain built and demodulation begins. A hit inside a call hands
        the rest of the call straight to the refine chain, so no sample is
        dropped there; at the refine's end, what the refine did not consume of
        the last call goes on to the live chain. Accepts any block size; state
        carries across calls, so a capture can be fed in frames of any length
        with no seam. Lost (after the release rule fires) consumes the samples
        and emit nothing, so the feeding loop is the same in every state; while
        tracking, the release clock runs on the two lock flags after every
        call, or on the symbol lock flag alone with `reacquire` (see
        `lost_confirm_s`). Tracking emits symbols whether or not they are data:
        a noise false alarm can reach tracking and emit until the release rule
        lets it go -- or, at the default lost_confirm_s of 0, until reset() --
        and symbol lock (`locked`) is what says the symbols are a signal's, so
        read it rather than the count (#2439). Under SPEC's coupled offset +
        500 Hz/s Doppler ramp the pre-despread Costas removes the full carrier
        dynamics before the code loop, so the recovered constellation lands
        cleanly on the BPSK real axis.

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            Input cf32 samples.
        out : npt.NDArray[np.complex64] | None
            Output symbols; caller provides max_out capacity.

        Returns
        -------
        NDArray[np.complex64]
            Number of symbols written: 0 while searching, refining or lost, or
            before a full symbol's worth of input.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import AsyncDsssReceiver
        >>> from doppler.wfm import Gold
        >>> sf, chip, sym, spc = 1023, 3.069e6, 2700.0, 2
        >>> fs, te, tsym = chip * spc, sf * spc, chip * spc / sym
        >>> code = np.asarray(Gold().generate(sf)).astype(np.uint8)
        >>> csign = np.where(code & 1, -1.0, 1.0)
        >>> rng = np.random.default_rng(21)
        >>> n = int(600 * tsym) + 4 * te            # 600 async BPSK symbols
        >>> idx = np.arange(n)
        >>> data = (rng.integers(0, 2, 604) * 2 - 1).astype(float)
        >>> si = np.clip((idx / tsym).astype(int), 0, 603)
        >>> t = idx / fs

        DSSS chips on a carrier sweeping at 500 Hz/s — the ramp the async
        receiver has to track:

        >>> sig = (data[si] * csign[(idx // spc) % sf]
        ...        * np.exp(1j * 2 * np.pi * 0.5 * 500.0 * t * t))
        >>> cn0 = 20.0 + 10 * np.log10(sym)         # Es/N0 = 20 dB
        >>> sigma = np.sqrt(fs / 10 ** (cn0 / 10))
        >>> pre = 5 * te                            # noise-only lead-in
        >>> noise = (sigma / np.sqrt(2)) * (rng.standard_normal(pre + n)
        ...          + 1j * rng.standard_normal(pre + n))
        >>> x = (np.concatenate([np.zeros(pre), sig]).astype(np.complex64)
        ...      + noise.astype(np.complex64))
        >>> rx = AsyncDsssReceiver(
        ...     code, chip_rate=chip, symbol_rate=sym, spc=spc,
        ...     cn0_dbhz=cn0, doppler_uncertainty=500.0)
        >>> syms = [rx.steps(x[p:p + te]) for p in range(0, len(x) - te, te)]
        >>> syms = np.concatenate([s for s in syms if len(s)])
        >>> rx.tracking                  # searched, refined, now tracking
        1
        >>> len(syms) > 300              # symbols recovered under the ramp
        True

        Nearly all the energy lands on I, so the BPSK phase is resolved:

        >>> bool(np.mean(syms.real**2) > 10 * np.mean(syms.imag**2))
        True

        """

    def steps_max_out(self) -> int:
        """The output capacity steps() needs beyond the input's.

        Returns 0, which is NOT a bound. A call that completes a code period
        carried over from earlier calls emits all of that period's symbols
        (code_len times symbol_rate over chip_rate of them), even if it brings
        one sample. At more than one symbol per period a call can therefore
        emit more symbols than its `x_len`, and those past `max_out` are
        dropped (see dp_async_dsss_receiver_steps()). A true bound is #2469.

        Returns
        -------
        int
            0.
        """

    def seed(
        self,
        chip_phase: float,
        doppler_hz_est: float,
        cn0_dbhz_est: float,
    ) -> None:
        """Take a detection from outside and start refining from it.

        The hand-off of docs/design/async-dsss-receiver.md §2.2: the three
        numbers a searcher's hit carries that this receiver uses --
        `acq_handoff_t`'s `chip_phase`, `doppler_hz_est` and `cn0_dbhz_est` --
        exactly as its own hit would have produced them (`steps()` calls this
        on its own hit). `chip_phase` is the code's instantaneous phase in
        chips, Dll's convention, at the FIRST sample of the next `steps()`
        call; dp_acq_build_handoff() makes it from a hit's lag. The refine
        chain is rebuilt from the seed and the state becomes refining; the
        unconsumed tail is the caller's to feed.

        Refused (`DP_ERR_INVALID`, nothing changes) on a receiver that is not
        waiting for one -- refining, tracking or lost -- because "assigned
        once" is a property of the object, not of the caller's bookkeeping;
        `reset()` releases it. Accepted while searching: an outside hit simply
        beats the receiver's own. Also refused outside the seed domain,
        dp_async_dsss_receiver_seed_ok().

        Parameters
        ----------
        chip_phase : float
            Code phase at the next sample, chips, in `[0, code_len)`.
        doppler_hz_est : float
            Coarse Doppler estimate, Hz (the refine stage sharpens it).
        cn0_dbhz_est : float
            The hit's C/N0 estimate, dB-Hz; reported back by
            get_cn0_dbhz_est(), unchanged until reset().

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``seed refused: the receiver already holds an assignment (refining,
            tracking or lost -- reset() releases it), or chip_phase is outside
            [0, code_len), or doppler_hz_est is not finite and below chip_rate
            * spc / 2 in magnitude``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import AsyncDsssReceiver
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6,
        ...                        symbol_rate=2700.0, spc=2)
        >>> rx.seed(chip_phase=512.25, doppler_hz_est=-1500.0,
        ...         cn0_dbhz_est=48.0)
        >>> (rx.refining, rx.doppler_hz, rx.cn0_dbhz_est)
        (1, -1500.0, 48.0)

        Already assigned -- refused until reset():

        >>> rx.seed(0.0, 0.0, 48.0)      # doctest: +ELLIPSIS
        Traceback (most recent call last):
            ...
        ValueError: seed refused: ...
        >>> rx.reset()

        A chip phase must be inside the code, `[0, code_len)`:

        >>> rx.seed(1023.0, 0.0, 48.0)   # doctest: +ELLIPSIS
        Traceback (most recent call last):
            ...
        ValueError: seed refused: ...

        """

    def status(self) -> ReceiverStatus:
        """Read the status record, by value: the state; where the emitter is
        now (the live carrier), its chip phase, code rate and C/N0; both lock
        flags with the symbol-lock metric and threshold; both carrier
        residuals; and the two clocks in input samples (since the state was
        entered; both flags down without a break).

        Cheap and allocation-free: every field is a read of live state. The
        one-at-a-time getters report the same fields, `doppler_hz` apart (see
        the record); this is the face a controller reads. No timestamp: the
        clocks are in input samples, and the holder, which owns the sample
        clock, stamps the record.

        Returns
        -------
        ReceiverStatus
            The record, by value.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import AsyncDsssReceiver
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6,
        ...                        symbol_rate=2700.0, spc=2)
        >>> st = rx.status()
        >>> (st.state, st.doppler_hz, st.code_locked, st.locked)   # searching
        (0, 0.0, 0, 0)
        >>> rx.seed(chip_phase=100.0, doppler_hz_est=-250.0, cn0_dbhz_est=50.0)
        >>> st = rx.status()
        >>> (st.state, round(st.doppler_hz, 6), st.cn0_dbhz_est)  # refining
        (1, -250.0, 50.0)
        >>> _ = rx.steps(np.zeros(2046, np.complex64))
        >>> rx.status().state_samples                             # since seed
        2046

        """

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the embedded Acquisition's search grid directly. Forwards to
        `dp_acq_configure_search_raw()` -- the escape hatch under this object's
        `symbol_rate`-driven auto-sizing, for a power user who wants a specific
        `(doppler_bins, n_noncoh)`. The pin replaces the whole auto-sized grid,
        the window-tiled search included. Only meaningful while searching; the
        search does not run again until the next `reset()`.

        Parameters
        ----------
        doppler_bins : int
            The coherent depth to pin, in `[1, reps]`: 1, as the receiver's
            continuous engine has `reps = 1`.
        n_noncoh : int
            Non-coherent looks accumulated per grid cell (1..256); more looks
            buys sensitivity at the cost of dwell, replacing the auto-sized
            count.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``configure_search_raw failed``, with the return code appended
            (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import AsyncDsssReceiver
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
        ...                        spc=2, doppler_uncertainty=500.0)
        >>> rx.configure_search_raw(doppler_bins=1, n_noncoh=16)  # pin it
        >>> rx.refining                # still searching, on the pinned grid
        0

        """

    def set_refine_min_blocks(self, n_blocks: int) -> None:
        """Floor the refine's dwell at n_blocks, whatever the detection sizing
        asks (docs/design/async-dsss-receiver-measurements.md §12.16, #1265).

        CarrierAcquisition's dwell is sized for DETECTION at the derated C/N0
        (`cn0_dbhz - refine_design_margin_db`), so it shortens as the C/N0
        rises -- two blocks at 45 dB-Hz with the shipped margin -- while the
        noise of the estimate it hands the tracking chain does not shorten with
        it: 210 Hz at two blocks against a chain that pulls in from a few
        hundred, so one hand-over in sixty landed outside and tracked the code
        with the carrier never locked. Seven blocks (42 ms, the default and the
        floor dwell of docs/design/async-dsss-receiver-measurements.md §12.10)
        hold the estimate to 77 Hz. Applied to the next refine chain built -- a
        receiver already refining keeps its dwell. A mutator's value, so it
        travels in the state blob (#2022). `n_blocks` of 0 removes the floor;
        the value is clamped by `refine_max_n_blocks` where that cap is lower.

        Parameters
        ----------
        n_blocks : int
            The floor, blocks.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_refine_min_blocks failed``, with the return code appended
            (gh-869).

        Examples
        --------
        >>> from doppler.dsss import AsyncDsssReceiver
        >>> rx = AsyncDsssReceiver(code=[1, 0, 1, 1, 0, 0, 1], chip_rate=1e6,
        ...                        symbol_rate=1e6 / 28.0, spc=4, cn0_dbhz=60.0)
        >>> rx.refine_min_blocks                     # the default floor
        7
        >>> rx.set_refine_min_blocks(12)
        >>> rx.refine_min_blocks
        12

        """

    def configure_lock_raw(
        self,
        up_thresh: float,
        down_thresh: float,
        n_looks: int,
        alpha: float,
        n_up: int,
        n_down: int,
    ) -> None:
        """Re-tune the live-tracking Dll's code-lock detector directly.
        Forwards to `dp_dll_configure_lock_raw()` on the live tracking Dll (the
        one behind `get_code_locked()`), NOT the refine-stage collection Dll.
        It retunes the Dll the receiver holds now; every chain build (a seed,
        the hand-over, reset() or configure_chain_raw()) replaces that Dll with
        a freshly sized one, so call it once tracking. The detector is the
        hysteretic lockdet over the DLL's per-N-look CFAR statistic — the
        levels and verify counts trade declare latency against false-alarm
        rate.

        Parameters
        ----------
        up_thresh : float
            CFAR-statistic level to declare code lock (hit when the statistic
            exceeds it).
        down_thresh : float
            Level below which a look is a miss; choose <= up_thresh for level
            hysteresis.
        n_looks : int
            Looks per decision — the DLL's non-coherent integration depth
            feeding one statistic.
        alpha : float
            EMA smoothing coefficient on the lock statistic (0..1); smaller is
            smoother/slower.
        n_up : int
            Consecutive hits required to declare lock.
        n_down : int
            Consecutive misses required to drop lock.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import AsyncDsssReceiver
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
        ...                        spc=2, doppler_uncertainty=500.0)
        >>> rx.configure_lock_raw(up_thresh=0.4, down_thresh=0.2, n_looks=20,
        ...                       alpha=0.1, n_up=5, n_down=3)
        >>> rx.tracking                       # replaced at the next chain build
        0

        """

    def configure_chain_raw(self, segments: int, sps: int, n: int) -> None:
        """Pin the live-tracking despread/resample/demod grid directly. The
        escape hatch for the composition-specific knob: `segments` (the live
        Dll's tracking parameter) and `sps`/`n` (MpskReceiver's samples and
        terminal outputs per symbol) are independently overridable, still
        bridged by a freshly-sized `RateConverter` and never coupled to each
        other. While searching/refining it re-pins the grid used to build the
        next tracking chain; once tracking it rebuilds `dll`/`rc`/`rx` in
        place, allocating every replacement before adopting it, and re-seeds
        the rebuilt chain from the whole carrier -- loop 1 plus loop 2's
        integrator, the sum status() reports -- so a retune keeps the signal it
        was tracking.

        The new grid is checked as create() checked the first, before anything
        is rebuilt, and refused with the receiver left on its prior grid: the
        Dll's domain at segments, sps >= 2, a symbol period of at most 2^20 Dll
        partials, which the Dll's aid would otherwise refuse mid-build
        (doppler#2103), and an n MpskReceiver would refuse (#2421). Still an
        abort: an allocation failure in the rebuild (doppler#2112).

        Parameters
        ----------
        segments : int
            Live-tracking Dll segments per code period.
        sps : int
            MpskReceiver samples per symbol (the resample target), >= 2.
        n : int
            MpskReceiver's terminal outputs per symbol (`m_out`): inside
            dp_ratesync_m_ok() (even, 2..8), and dividing sps.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``configure_chain_raw failed``, with the return code appended
            (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import AsyncDsssReceiver
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
        ...                        spc=2, doppler_uncertainty=500.0)
        >>> rx.configure_chain_raw(segments=6, sps=8, n=8)  # re-pin the chain
        >>> rx.segments                       # tracking grid updated in place
        6

        """

    def reset(self) -> None:
        """Return to the searching state. Resets the embedded Acquisition and
        rebuilds the refine-stage chain and the live tracking chain back to
        their placeholder seed (phase 0, no Doppler). A receiver that has
        locked cannot be "reset back to tracking the same signal," only back to
        searching -- every object's reset() semantics in this codebase.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import AsyncDsssReceiver
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
        ...                        spc=2, doppler_uncertainty=500.0)
        >>> rx.reset()                 # abort any lock, hunt from scratch
        >>> (rx.tracking, rx.refining, rx.chip_phase)   # all cleared
        (0, 0, 0.0)

        """

    def set_telemetry(
        self,
        tlm: object | None,
        prefix: str,
        decim: int = 1,
    ) -> None:
        """Attach (or detach) a telemetry context across the receiver's loops,
        and register the receiver's own lifecycle and symbol-lock decision.

        The receiver reports under the prefix, once per steps() call that
        carries samples, after the call, so on the time axis at the stamp the
        caller set for that block (dp_tlm_set_now()):

        - "<prefix>.state" -- where the call left the receiver, the
          ASYNC_DSSS_RX_* value: 0 searching, 1 refining, 2 tracking, 4 lost
          (status().state);
        - "<prefix>.handover_hz" -- the Doppler the chain was built from, Hz
          (dp_async_dsss_receiver_get_doppler_hz()): 0 before a seed, the
          seed's while refining, the refined value from the hand-over on, and 0
          again after a refused hand-over or a reset(), which return the
          receiver to searching. A record is a float, so it resolves about 0.5
          Hz near 5 MHz. Not the live carrier: status().doppler_hz reads that,
          and ".car.freq" and ".car.nco" are the carrier loop's own view of it,
          in cycles a sample at the input rate, so times chip_rate * spc for
          Hz;
        - "<prefix>.refused" -- the hits refused as seeds so far
          (status().refused_hits).

        Feed blocks of about a code period, as the examples do, and these read
        as a time series of the lifecycle: when the search hit, how long the
        refine took, what it handed over, when the release came.

        Each loop reports under the prefix, at its own update rate:

        - "<prefix>.car" -- the chip-domain carrier loop
          (dp_costas_set_telemetry(): ".lock", ".e", ".freq", ".locked",
          ".nco"), once per carrier update: once a code period, except while it
          coasts, when loop 1 is held;
        - "<prefix>.code" -- the code loop (dp_dll_set_telemetry(): ".e",
          ".rate", ".lock", ".locked", ".nco"), once per discriminator update;
        - "<prefix>.demod" -- the symbol-domain demodulator
          (dp_mpsk_receiver_set_telemetry(): its lock, its carrier and timing
          loops, the symbols, its AGC), once per recovered symbol (the AGC's
          pair once per gain update);
        - "<prefix>.acq" -- the acquisition engine (dp_acq_set_telemetry()),
          once per decided dwell;
        - "<prefix>.rc" -- the rate converter between the code loop and the
          demodulator (dp_RateConverter_set_telemetry()). It reports only an
          AGC, and this receiver builds it without one, so it registers no
          probes;
        - "<prefix>.sym.lock" and "<prefix>.sym.locked" -- the receiver's own
          symbol-lock metric (dp_async_dsss_receiver_get_lock_metric()) and its
          decision (dp_async_dsss_receiver_get_locked()), the one its users act
          on, once per emitted symbol.

        The carrier and code loops report their filters two ways: ".car.freq"
        and ".code.rate" (and the demodulator's ".demod.car.freq") are the
        integrators, each loop's frequency memory, which a coast holds;
        ".car.nco", ".code.nco" and ".demod.car.nco" are the complete commands
        the NCOs run at, the proportional term (and the code loop's carrier
        aid) included. ".demod.sync.rate" is the timing loop's output, and ".e"
        is each discriminator's input to its filter.

        The attachment is a REQUEST kept on the receiver: every seed rebuilds
        the tracking chain, and the rebuilt loops are attached again under the
        same names (so the same probe ids). Records are therefore continuous
        across re-seeds, and a reset keeps the attachment too.

        Setup path, never hot; the context is borrowed and must outlive the
        attachment, and the receiver must step on one thread (SPSC rules in
        dp_tlm/dp_tlm_core.h). Passing NULL detaches every loop.

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix; NULL means "rx".
        decim : int
            Emit every decim-th event on each probe; >= 1.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_telemetry failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import AsyncDsssReceiver
        >>> from doppler.telemetry import Telemetry
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
        ...                        spc=2, doppler_uncertainty=500.0)
        >>> tlm = Telemetry(1 << 12)
        >>> rx.set_telemetry(tlm, "rx")
        >>> sorted(p for p in tlm.probe_names if p.startswith("rx.c"))
        ['rx.car.e', 'rx.car.freq', 'rx.car.lock', 'rx.car.locked', 'rx.car.nco', 'rx.code.e', 'rx.code.lock', 'rx.code.locked', 'rx.code.nco', 'rx.code.rate']
        >>> sorted(p for p in tlm.probe_names if p.startswith(("rx.s", "rx.h",
        ...                                                    "rx.r")))
        ['rx.handover_hz', 'rx.refused', 'rx.state', 'rx.sym.lock', 'rx.sym.locked']
        >>> rx.set_telemetry(None, "rx")      # detach

        """

    def set_threads(self, n: int) -> None:
        """Set the workers the searcher fans its Doppler tiles across.

        Forwards to dp_acq_set_threads() on the receiver's searcher. The search
        is bit-identical at every count, so this changes the cost of a steps()
        call and nothing about its output. A pool that steps its receivers in
        parallel sets each one to 1. A mutator's value, so it travels in the
        state blob (#2022). The blob carries the count that RAN, not the
        request: set_threads(0) on a 20-core writer restores as 20 workers on
        any reader, by design -- a resumed receiver runs as the one that was
        checkpointed, and a pool's 1 is 1 everywhere. Setup path; not while
        another thread is inside steps().

        Parameters
        ----------
        n : int
            Thread count; 0 = online cores, 1 = serial.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_threads failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> from doppler.dsss import AsyncDsssReceiver
        >>> rx = AsyncDsssReceiver(code=[1, 0, 1, 1, 0, 0, 1], chip_rate=1e6,
        ...                        symbol_rate=1e6 / 28.0, spc=4, cn0_dbhz=60.0)
        >>> rx.set_threads(1)
        >>> rx.threads
        1

        """

    def set_rank(self, rank: int) -> None:
        """Take the rank-th surviving peak of a dwell, not the strongest.

        The searcher reports up to ASYNC_DSSS_RX_MAX_PEAKS peaks per dwell,
        strongest first. The receiver drops every peak within one chip
        (circularly) of a stronger one, so one signal's neighbouring cells hold
        one place; then every peak a stronger signal's own sidelobes could
        explain; then every peak within one chip of an excluded code phase
        (set_exclusions()). The survivors are ranked from 0. A dwell with no
        peak at this rank hands nothing off and the search goes on.

        The sidelobe rule: a strong signal's list also holds its code's
        Doppler-offset sidelobes, up to dp_acq_dsl_db() below it and hundreds
        of chips from its phase (design acq-surface.md §8). A peak counts as a
        second signal only if its test_stat exceeds the stronger signals'
        bound, times that level, by more than the detector's own gate
        (dp_acq_gate_stat()). The bound is the strongest signal's test_stat; on
        a deep block (get_coherent_bins() > 1) it is the sum of the stronger
        signals on the peak's own Doppler row, modulo the epoch rate, when that
        is larger, since their fixed sidelobes can stack on one cell. So the
        range does not narrow with the number of signals on the air
        (doppler#2554). A sidelobe then passes only when the noise on it alone
        would have cleared the gate, the rate the search already runs at. What
        it costs is near-far: a second signal much more than dp_acq_dsl_db()
        below a stronger one (-16.95 dB on the SPEC Gold-1023 search) is not
        taken while the stronger one is on the air (async-dsss-parallel.md §3).
        Searchers that share a stream and its block grid (get_coherent_bins())
        and hold ranks 0..k-1 therefore take k different signals in one dwell.
        Takes effect at the next dwell. A mutator's value, so it travels in the
        state blob (#2022).

        Parameters
        ----------
        rank : int
            0 (the strongest, the default) to ASYNC_DSSS_RX_MAX_PEAKS - 1.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``rank refused: it must be below ASYNC_DSSS_RX_MAX_PEAKS (16), the
            peaks a dwell reports``, with the return code appended (gh-869).

        Examples
        --------
        >>> from doppler.dsss import AsyncDsssReceiver
        >>> rx = AsyncDsssReceiver(code=[1, 0, 1, 1, 0, 0, 1], chip_rate=1e6,
        ...                        symbol_rate=1e6 / 28.0, spc=4, cn0_dbhz=60.0)
        >>> rx.set_rank(1)
        >>> rx.rank
        1

        """

    def set_exclusions(self, phases: npt.NDArray[np.float64]) -> None:
        """Code phases the search must not take: signals another receiver
        already tracks.

        Replaces the list. A peak within one chip, circularly, of any of them
        is dropped from a dwell's list at any Doppler, before ranking
        (set_rank()). Phases are chips in status().chip_phase's convention and
        are folded into [0, code_len). An empty list (the default) excludes
        nothing. A tracking receiver is not affected: the list applies to its
        next search. A mutator's value, so it travels in the state blob
        (#2022).

        Parameters
        ----------
        phases : npt.NDArray[np.float64]
            Code phases, chips; finite. May be NULL when phases_len is 0.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``exclusions refused: every phase must be finite, and at most
            ASYNC_DSSS_RX_MAX_EXCLUSIONS (64) of them``, with the return code
            appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import AsyncDsssReceiver
        >>> rx = AsyncDsssReceiver(code=[1, 0, 1, 1, 0, 0, 1], chip_rate=1e6,
        ...                        symbol_rate=1e6 / 28.0, spc=4, cn0_dbhz=60.0)
        >>> rx.set_exclusions(np.array([2.5, 9.0]))   # 9 folds to 2
        >>> rx.n_exclusions
        2

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the AsyncDsssReceiver has already been
        destroyed.

        Returns
        -------
        int
            Byte length of one serialized state blob.
        """

    def get_state(self) -> bytes:
        """Serialize this object's mutable state to bytes.

        Captures the state that evolves as the object runs, including any value
        a setter changed after construction, so a blob taken now and restored
        later resumes from this point. Construction parameters are not
        restored: restore into an object built the same way. A core may check
        some of them and refuse a blob built differently.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the AsyncDsssReceiver has already been
        destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes, /) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place, including any value a setter
        changed after construction. Construction parameters are not restored:
        restore into an object built the same way. Length is validated against
        `state_bytes()` before the blob is handed to the C core, which may
        reject it too: a core may check some construction parameters and refuse
        a blob built differently.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the AsyncDsssReceiver has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def refine_min_blocks(self) -> int:
        """Floor on the refine's dwell, in blocks (default 7). The refine's
        detection sizing needs fewer blocks the higher the C/N0, but the
        estimate the carrier chain must pull in from does not sharpen with it
        (#1265). Set with set_refine_min_blocks(), which carries the
        measurement.
        """

    @property
    def threads(self) -> int:
        """Workers the searcher runs, the calling thread included; 1 = serial.
        Set with dp_async_dsss_receiver_set_threads().
        """

    @property
    def rank(self) -> int:
        """Which surviving peak of a dwell the search takes; 0 = the strongest.
        Set with dp_async_dsss_receiver_set_rank().
        """

    @property
    def n_exclusions(self) -> int:
        """Code phases set_exclusions() holds."""

    @property
    def coherent_bins(self) -> int:
        """The searcher's slow-time depth D: epochs per coherent block."""

    @property
    def lifecycle(self) -> Literal["searching", "refining", "tracking", "reserved", "lost"]:
        """Where the receiver is in its lifecycle, by name: `"searching"`,
        `"refining"`, `"tracking"` or `"lost"` (released, waiting for
        `reset()`). The same state as `status().state`, which carries it as the
        C integer. With `reacquire` a release goes back to `"searching"` and
        never reads `"lost"`.
        """

    @property
    def tracking(self) -> int:
        """1 once the live tracking chain is built and demodulating; 0 in every
        other state.
        """

    @property
    def refining(self) -> int:
        """1 while refining: the CarrierAcquisition dwell; 0 in every other
        state.
        """

    @property
    def lost(self) -> int:
        """1 once the release rule has fired: both lock flags were down,
        continuously, for longer than `lost_confirm_s` while tracking -- an
        emitter that left, or a seed that never locked. The loops have stopped
        and samples are consumed and discarded; the holder releases the
        assignment and calls reset(). 0 in every other state, and always 0 with
        `lost_confirm_s = 0`.
        """

    @property
    def doppler_hz(self) -> float:
        """The Doppler estimate the chain was built from, Hz: the seed's while
        refining, the CarrierAcquisition-refined value once tracking. The live
        carrier is status().doppler_hz.
        """

    @property
    def cn0_dbhz_est(self) -> float:
        """The seed's C/N0 estimate, dB-Hz, unchanged until reset()."""

    @property
    def segments(self) -> int:
        """The live-tracking Dll's partial correlations per code period
        (refine_segments is the refine collector's).
        """

    @property
    def sps(self) -> int:
        """MpskReceiver's samples per symbol."""

    @property
    def n(self) -> int:
        """MpskReceiver's terminal outputs per symbol (`m_out`: even, 2..8)."""

    @property
    def chip_phase(self) -> float:
        """The live Dll's code phase, chips, in the Dll's instantaneous
        convention -- the mirror of acq_result_t::code_phase's correlation-lag
        convention (dp_acq_build_handoff()). While refining there is no live
        Dll: it is the seed's phase advanced by the clock dilation since, the
        phase the hand-over starts the live chain from.
        """

    @property
    def code_rate(self) -> float:
        """The Dll's code rate, chips per nominal chip (~1.0 at lock), without
        the carrier aid; a coasting Dll reports its held rate.
        """

    @property
    def lock(self) -> float:
        """The carrier lock statistic of the MpskReceiver this receiver tracks
        with: the EMA of its M-th-power lock signal, near 1 when locked and
        near 0 on noise.
        """

    @property
    def norm_freq(self) -> float:
        """The post-despread carrier the MpskReceiver tracks: its centre plus
        its loop's integrator, cycles/sample at its input rate (`sps *
        symbol_rate`). Lags a Doppler ramp by the Type-II ramp error.
        """

    @property
    def nco_freq(self) -> float:
        """The MpskReceiver's carrier NCO command, its loop filter's whole
        output, cycles/sample at its input rate (`sps * symbol_rate`). Its mean
        tracks a Doppler ramp with no lag (unlike get_norm_freq()); its
        variance is the carrier loop's stress.
        """

    @property
    def locked(self) -> int:
        """The receiver's symbol-lock flag: a hysteretic detector over
        get_lock_metric(), declared after 30 consecutive symbols strictly above
        get_lock_threshold() (0.5) and dropped after 15 consecutive symbols
        strictly below 0.3 (ASYNC_DSSS_RX_LOCK_N_UP, _UP, _N_DOWN and _DOWN,
        fixed at create). The flag the release rule and a pool's holder act
        on.
        """

    @property
    def lock_metric(self) -> float:
        """Symbol-lock metric = SNR-weighted EMA of (I^2-Q^2)/(I^2+Q^2) =
        cos(2*phi) over the emitted symbols (locked -> ~+1). Drives
        `locked`.
        """

    @property
    def lock_threshold(self) -> float:
        """The lock-metric threshold `locked` is declared strictly above (0.5,
        ASYNC_DSSS_RX_LOCK_UP); exposed alongside lock_metric for engineering
        debug.
        """

    @property
    def car_last_error(self) -> float:
        """Pre-despread Costas phase discriminator (rad): the residual carrier
        phase LOOP 1 (which de-rotates before the Dll) is not nulling.
        """

    @property
    def car_nco_freq(self) -> float:
        """LOOP 1 (pre-despread Costas) loop-filter output = NCO frequency
        command, cycles/sample of the front-end (chip_rate*spc) rate.
        """

    @property
    def mpsk_last_error(self) -> float:
        """MpskReceiver carrier phase discriminator (rad): the residual carrier
        phase LOOP 2 (post-despread) is not nulling.
        """

    @property
    def code_locked(self) -> int:
        """Binary code-lock flag from the live tracking Dll's own
        verify-counted (pfa-tuned) lock detector — the fundamental DSSS "am I
        despreading" lock, de-chattered by up/down hysteresis.
        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """

    def __enter__(self) -> "AsyncDsssReceiver":
        """Enter a context manager, returning this object.

        Lets a AsyncDsssReceiver be used in a `with` statement so its C
        resources are released deterministically on exit rather than at
        collection time.

        Returns
        -------
        AsyncDsssReceiver
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
        /,
    ) -> None:
        """Exit a context manager, releasing the AsyncDsssReceiver.

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

@final
class AsyncDsssPool:
    """Create a pool of n_receivers receivers built alike.

    Parameters
    ----------
    code : NDArray[np.uint8]
        Spreading code, 0/1 chips.
    n_receivers : int, default 3
        Receivers, 1 to ASYNC_DSSS_POOL_MAX_RECEIVERS; the most signals the
        pool holds at once (default 3).
    chip_rate : float, default 1000000.0
        Chip rate, Hz.
    symbol_rate : float, default 1000.0
        Symbol rate, Hz.
    spc : int, default 2
        Samples per chip.
    m : int, default 2
        PSK order.
    cn0_dbhz : float, default 55.0
        Design C/N0, dB-Hz.
    pfa : float, default 1e-3
        Search false-alarm probability.
    pd : float, default 0.9
        Search detection probability.
    doppler_uncertainty : float, default 100.0
        Doppler search half-range, Hz.
    segments : int, default 4
        Dll segments per symbol.
    sps : int, default 8
        Demodulator samples per symbol.
    differential : int, default 0
        Differential detection (nonzero) or coherent.
    refine_max_error_db : float, default 0.5
        Refine stage's allowed loss, dB.
    refine_samples_per_symbol : int, default 4
        Refine stage's samples per symbol.
    refine_design_margin_db : float, default 14.0
        Refine stage's design margin, dB.
    refine_n_fft : int, default 64
        Refine stage's FFT length.
    refine_zero_pad : int, default 8
        Refine stage's zero-pad factor.
    refine_sequential : bool, default False
        Refine stage's sequential mode.
    refine_max_n_blocks : int, default 100000
        Refine stage's block cap.
    carrier_freq_hz : float, default 0.0
        RF carrier for code aiding, Hz; 0 = none.
    lost_confirm_s : float, default 2.0
        The release rule's confirm time, s (default 2.0 here: a pool whose
        receivers never release never frees one). 0 = never lost.
    reacquire : bool, default False
        The receiver's own release-to-search.
    code_only_s : float, default 0.0
        Code-only window, s: sets the slow-time depth D.
    doppler_rate : float, default 0.0
        Doppler rate bound, Hz/s.
    threads : int, default 0
        Workers push() steps the receivers on, the calling thread included: 1 =
        serial, 0 = the online cores (default). Never more than n_receivers
        run.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``AsyncDsssPool:
        invalid parameter (need 1 <= n_receivers <= 16, threads >= 0, and every
        parameter AsyncDsssReceiver takes: a code of at least 2 chips, a finite
        chip_rate > 0, a finite symbol_rate > 0 and <= chip_rate, spc >= 1, m
        in {2,4,8}, segments >= 1, sps >= 2, carrier_freq_hz 0 or finite and
        above chip_rate * spc / 2, lost_confirm_s >= 0 (and at least one input
        sample with reacquire), refine_samples_per_symbol >= 1, refine_n_fft >=
        1, refine_zero_pad >= 1; and for the searcher a finite cn0_dbhz, 0 <
        pfa < 1, 0 < pd < 1, a finite doppler_uncertainty >= 0)``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.dsss import AsyncDsssPool
    >>> from doppler.wfm import Gold
    >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
    >>> pool = AsyncDsssPool(code, n_receivers=3, chip_rate=3.069e6,
    ...                      symbol_rate=2700.0, spc=2,
    ...                      doppler_uncertainty=500.0, threads=1)
    >>> (pool.n_receivers, pool.threads, pool.coherent_bins)
    (3, 1, 1)
    >>> AsyncDsssPool(code, n_receivers=17)    # one rank per peak: 16 at most
    Traceback (most recent call last):
        ...
    ValueError: AsyncDsssPool: invalid parameter ...

    """

    def __init__(
        self,
        code: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
        n_receivers: int = 3,
        chip_rate: float = 1000000.0,
        symbol_rate: float = 1000.0,
        spc: int = 2,
        m: int = 2,
        cn0_dbhz: float = 55.0,
        pfa: float = 1e-3,
        pd: float = 0.9,
        doppler_uncertainty: float = 100.0,
        segments: int = 4,
        sps: int = 8,
        differential: int = 0,
        refine_max_error_db: float = 0.5,
        refine_samples_per_symbol: int = 4,
        refine_design_margin_db: float = 14.0,
        refine_n_fft: int = 64,
        refine_zero_pad: int = 8,
        refine_sequential: bool = False,
        refine_max_n_blocks: int = 100000,
        carrier_freq_hz: float = 0.0,
        lost_confirm_s: float = 2.0,
        reacquire: bool = False,
        code_only_s: float = 0.0,
        doppler_rate: float = 0.0,
        threads: int = 0,
    ) -> None: ...
    def push(self, x: npt.NDArray[np.complex64]) -> int:
        """Step every receiver on one block, running the controller at every
        code period it crosses.

        Hands every whole code period it completes to every receiver (header
        step 1), keeps each one's symbols for symbols(), and after each period
        releases the lost, resolves duplicates, releases ghosts and hands out
        exclusions and ranks (step 2). The samples past the last whole period
        are kept for the next push. Accepts any block size, and state carries
        across calls: the result is a function of the stream, not of its
        blocking.

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            Input cf32 samples.

        Returns
        -------
        int
            The receivers holding a signal (refining or tracking) at the end of
            the block.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import AsyncDsssPool
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,
        ...                      symbol_rate=2700.0, spc=2,
        ...                      doppler_uncertainty=500.0, threads=1)
        >>> pool.push(np.zeros(2046, dtype=np.complex64))
        0

        """

    def status(self, receiver: int) -> PoolReceiverStatus:
        """Receiver receiver's status, now.

        dp_async_dsss_receiver_status() of that receiver, as of the last whole
        code period fed, after the controller's step on it: a receiver the
        controller released reads searching. A receiver past the pool returns a
        zeroed record with `state` ASYNC_DSSS_POOL_NO_RECEIVER.

        Parameters
        ----------
        receiver : int
            0 to n_receivers - 1.

        Returns
        -------
        PoolReceiverStatus
            The status record.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import AsyncDsssPool
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,
        ...                      symbol_rate=2700.0, spc=2,
        ...                      doppler_uncertainty=500.0, threads=1)
        >>> (pool.status(0).state, pool.status(2).state)  # 2 is past the pool
        (0, -1)

        """

    def symbols(
        self,
        receiver: int,
        out: npt.NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """The symbols receiver receiver emitted during the last push().

        Empty for a receiver that was searching, refining or lost throughout
        the push, and for a receiver past the pool. Tracking emits symbols
        whether or not they are a signal's (dp_async_dsss_receiver_steps()):
        read status().locked for that. A push replaces them, and set_state()
        and reset() clear them.

        Parameters
        ----------
        receiver : int
            0 to n_receivers - 1.
        out : npt.NDArray[np.complex64] | None
            Output symbols.

        Returns
        -------
        NDArray[np.complex64]
            Symbols written.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import AsyncDsssPool
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,
        ...                      symbol_rate=2700.0, spc=2,
        ...                      doppler_uncertainty=500.0, threads=1)
        >>> _ = pool.push(np.zeros(2046, dtype=np.complex64))
        >>> pool.symbols(0).size                    # searching: nothing yet
        0

        """

    def symbols_max_out(self) -> int:
        """The most symbols symbols() can return: the largest count any
        receiver emitted during the last push().

        Returns
        -------
        int
            Output.
        """

    def reset(self) -> None:
        """Every receiver back to searching, with no exclusions and rank 0; the
        clock, the release counts and the ghost history back to 0.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import AsyncDsssPool
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> pool = AsyncDsssPool(code, chip_rate=3.069e6, symbol_rate=2700.0,
        ...                      spc=2, doppler_uncertainty=500.0, threads=1)
        >>> _ = pool.push(np.zeros(2046, dtype=np.complex64))
        >>> pool.samples
        2046
        >>> pool.reset()
        >>> pool.samples
        0

        """

    def set_threads(self, n: int) -> None:
        """Set the workers push() steps the receivers on.

        The receivers' results do not change with it (header step 1); only the
        time a push takes does. A mutator's value, so it travels in the state
        blob (#2022).

        Parameters
        ----------
        n : int
            1 = serial, 0 = the online cores; at most n_receivers run.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``threads refused: it must be >= 0 (0 = the online cores), and 1
            while a telemetry context is attached``, with the return code
            appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import AsyncDsssPool
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,
        ...                      symbol_rate=2700.0, spc=2,
        ...                      doppler_uncertainty=500.0)
        >>> pool.set_threads(1)
        >>> pool.threads
        1

        """

    def set_telemetry(
        self,
        tlm: object | None,
        prefix: str,
        decim: int = 1,
    ) -> None:
        """Attach (or detach) a telemetry context across every receiver.

        Receiver i reports under "<prefix>.r<i>" exactly as
        dp_async_dsss_receiver_set_telemetry() describes, on the one context. A
        context is single-producer, so this needs one producer: it is refused
        unless push() runs on one worker (get_threads() 1), and while a context
        is attached set_threads() refuses more. Parallel telemetry, one context
        per receiver, is doppler#2547.

        Whole or nothing: every receiver's names must fit before any registers
        on tlm (they are first attached to a scratch context), and a refusal
        leaves the pool on its previous context, or detached. The attachment is
        a request kept on the pool, so a receiver the controller resets keeps
        reporting under the same names.

        Parameters
        ----------
        tlm : object | None
            Context to attach, or NULL to detach every receiver.
        prefix : str
            Probe-name prefix; NULL means "pool".
        decim : int
            Emit every decim-th event on each probe; >= 1.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_telemetry failed: the pool must run on one worker (threads
            1), decim must be >= 1, and every receiver's names must fit the
            prefix and the context's table``, with the return code appended
            (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import AsyncDsssPool
        >>> from doppler.telemetry import Telemetry
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,
        ...                      symbol_rate=2700.0, spc=2,
        ...                      doppler_uncertainty=500.0, threads=1)
        >>> tlm = Telemetry(1 << 16)
        >>> pool.set_telemetry(tlm, "pool")
        >>> sorted(p for p in tlm.probe_names if p.endswith(".state"))
        ['pool.r0.state', 'pool.r1.state']

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the AsyncDsssPool has already been
        destroyed.

        Returns
        -------
        int
            Byte length of one serialized state blob.
        """

    def get_state(self) -> bytes:
        """Serialize this object's mutable state to bytes.

        Captures the state that evolves as the object runs, including any value
        a setter changed after construction, so a blob taken now and restored
        later resumes from this point. Construction parameters are not
        restored: restore into an object built the same way. A core may check
        some of them and refuse a blob built differently.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the AsyncDsssPool has already been
        destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes, /) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place, including any value a setter
        changed after construction. Construction parameters are not restored:
        restore into an object built the same way. Length is validated against
        `state_bytes()` before the blob is handed to the C core, which may
        reject it too: a core may check some construction parameters and refuse
        a blob built differently.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the AsyncDsssPool has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def threads(self) -> int:
        """Workers push() steps the receivers on, the calling thread included:
        what set_threads() asked for, at most n_receivers, and the online cores
        for 0.
        """

    @property
    def n_receivers(self) -> int:
        """Receivers, fixed at create."""

    @property
    def samples(self) -> int:
        """Input samples pushed since create or reset: the pool's shared
        clock.
        """

    @property
    def released_lost(self) -> int:
        """Lost receivers the controller reset."""

    @property
    def released_duplicate(self) -> int:
        """Duplicate holders it reset."""

    @property
    def released_ghost(self) -> int:
        """Sidelobe ghosts it reset."""

    @property
    def coherent_bins(self) -> int:
        """The receivers' slow-time depth D, epochs per coherent block
        (dp_async_dsss_receiver_get_coherent_bins()); alike for all.
        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """

    def __enter__(self) -> "AsyncDsssPool":
        """Enter a context manager, returning this object.

        Lets a AsyncDsssPool be used in a `with` statement so its C resources
        are released deterministically on exit rather than at collection time.

        Returns
        -------
        AsyncDsssPool
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
        /,
    ) -> None:
        """Exit a context manager, releasing the AsyncDsssPool.

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

@final
class DsssBurstReceiver:
    """The Python binding's constructor: dp_dsss_burst_receiver_create_desc
    without the `why` out-parameter.

    Parameters
    ----------
    acq_code : NDArray[np.uint8]
        preamble code, 0/1 chips.
    data_code : NDArray[np.uint8]
        payload spreading code, 0/1 chips.
    frame : Any
        The frame description the transmitter spread (a Frame or a FrameDesc,
        built or not): its first field is the sync word the receiver correlates
        for, and its layout is the frame's length. Copied at construction, so
        it need not outlive the receiver.
    reps : int, default 5
        preamble code repetitions.
    spc : int, default 4
        samples per chip.
    chip_rate : float, default 1000000.0
        chips per second.
    cn0_dbhz : float
        design C/N0 for the acquisition, dB-Hz (or NaN).
    doppler_uncertainty : float, default 0.0
        the Doppler span to search, cycles/sample.
    pfa : float, default 1e-3
        false-alarm probability, in (0, 1).
    pd : float, default 0.9
        detection probability, in (0, 1).
    carrier_hz : float, default 0.0
        the carrier the baseband is offset by, Hz.
    max_rate : float, default 0.0
        the Doppler rate expected, cycles/sample^2.
    est_segments : int, default 10
        partials per acquisition period.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``DsssBurstReceiver:
        invalid parameter (need non-empty acq_code/data_code, reps >= 1, spc >=
        1, chip_rate > 0, cn0_dbhz finite or NaN, 0 < pfa < 1, 0 < pd < 1) or a
        frame description whose first field is not known bits (empty, derived
        by a stage, a data field, covered by a stage, or named "preamble"), or
        whose stages emit a new stream (a convolutional code)``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.dsss import DsssBurstReceiver
    >>> from doppler.wfm import Frame
    >>> rng = np.random.default_rng(0)
    >>> acq = rng.integers(0, 2, 31).astype(np.uint8)
    >>> dat = rng.integers(0, 2, 8).astype(np.uint8)
    >>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(3, np.uint8),
    ...              crc="crc16")     # the 32-symbol frame the burst carries
    >>> rx = DsssBurstReceiver(acq, dat, desc, reps=4, spc=4)
    >>> rx.n_bursts
    0

    """

    def __init__(
        self,
        acq_code: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
        data_code: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
        frame: object,
        reps: int = 5,
        spc: int = 4,
        chip_rate: float = 1000000.0,
        cn0_dbhz: float = ...,
        doppler_uncertainty: float = 0.0,
        pfa: float = 1e-3,
        pd: float = 0.9,
        carrier_hz: float = 0.0,
        max_rate: float = 0.0,
        est_segments: int = 10,
    ) -> None: ...
    def push(
        self,
        x: npt.NDArray[np.complex64],
        out: npt.NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """Stream raw cf32 samples and get back the FRAME BITS of every burst
        that completed. Samples feed the embedded BurstAcquisition and are
        retained in a history ring; when a detection fires, the refine stage
        correlates the whole preamble to recover the exact preamble start --
        the one quantity acquisition structurally cannot report, since its
        code_phase is a lag modulo one code period -- and the burst is
        demodulated the moment its last sample has arrived. Every sample is
        consumed and every burst that completes is returned by the call that
        completed it, with frames concatenated: burst i occupies frame_syms
        bits starting at i*frame_syms, and events() returns the matching record
        for each. The bits are the frame as RECEIVED -- sync word first, every
        symbol after it -- because this object stops at decisions: undoing the
        frame (a CRC, an outer code, a randomiser) needs its description and is
        `wfm.Frame.deframe`'s job. An empty return is normal, not an error: it
        means no burst completed in this call. Accepts any block size -- the
        history ring is a contiguous window over the stream and is never reset
        between bursts, so a payload whose tail falls outside one call is
        completed by a later one.

        Retains x in the history ring and feeds the embedded acquisition. When
        a detection fires, the refine stage correlates the whole preamble to
        recover the exact preamble start -- the quantity acquisition
        structurally cannot report, its code phase being a lag modulo one code
        period -- and the burst is demodulated once its last sample has
        arrived.

        EVERY SAMPLE OF x IS CONSUMED, and every burst that completes is
        returned by the call that completed it. Payloads are concatenated, so
        burst `i` occupies `out` from `i*frame_syms`, and `events()` returns
        the matching record for each. Returning 0 is normal, not an error: it
        means no burst completed in this call.

        This is the contract doppler#1008 broke. push() used to return at most
        one burst per call AND abandon the rest of its input to do it, so a
        block carrying several bursts lost all but the first -- measured at 6/6
        decoded with 333-sample blocks against 1/6 with one large one. The
        history ring is a contiguous window over the stream and is never reset
        between bursts, so a payload whose tail falls outside one call is
        completed by a later one.

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            Input samples (cf32), x_len long.
        out : npt.NDArray[np.uint8] | None
            Payload bits, caller-owned, max_out long.

        Returns
        -------
        NDArray[np.uint8]
            Bits written to out -- `n_bursts_returned * frame_syms`.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import DsssBurstReceiver
        >>> from doppler.wfm import Frame
        >>> rng = np.random.default_rng(0)
        >>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(3, np.uint8),
        ...              crc="crc16")     # 13 + 3 + 16 = 32 symbols a burst
        >>> rx = DsssBurstReceiver(
        ...     rng.integers(0, 2, 31).astype(np.uint8),
        ...     rng.integers(0, 2, 8).astype(np.uint8), desc, reps=4, spc=4)
        >>> bits = rx.push(np.zeros(4096, dtype=np.complex64))
        >>> bits.size            # silence carries no burst
        0

        """

    def push_max_out(self, x_len: int, /) -> int:
        """Max bits push() can write for an input of x_len samples.

        push() returns EVERY burst it completed, so the bound scales with the
        input: distinct bursts cannot overlap, so they are at least `burst_len`
        apart, and a push of x_len samples can complete at most
        `x_len/burst_len + 1` of them -- plus every detection already queued
        from an earlier call, which is `q_cap`.

        Parameters
        ----------
        x_len : int
            Number of input samples the caller is about to push.

        Returns
        -------
        int
            `(x_len/burst_len + 1 + q_cap) * frame_syms`.
        """

    def llrs(
        self,
        count: int = 1,
        out: npt.NDArray[np.float32] | None = None,
    ) -> NDArray[np.float32]:
        """The SOFT bits of every burst the last push() returned, concatenated:
        burst i occupies llr[i*frame_syms:(i+1)*frame_syms], in the same order
        as push()'s payloads and events()' rows. `mpsk_soft_demap`'s convention
        — positive means bit 0, so `L < 0` reproduces exactly the bits push()
        returned, which is asserted rather than assumed. Spans the WHOLE frame
        rather than the payload alone, because a code covers what its
        description says it covers and a decoder needs the bits the code
        protects. Scaled by the burst's own noise estimate: a Viterbi is
        invariant to a positive scale, but LLRs from different bursts are not
        comparable without one. Valid until the next push(), reset() or
        set_state().

        `crealf(sym * derot)` IS the log-likelihood ratio up to a scale, and
        the demodulator used to compute it, slice it to one bit and free it. A
        hard decision throws away roughly 2 dB of the coding gain a soft-input
        decoder exists to deliver (`mpsk_soft_demap`'s own docstring), which is
        what makes a coded burst worth coding.

        Concatenated the same way push()'s payloads are, one row of
        `frame_syms` per burst: burst i starts at `i * frame_syms`, in the
        order events() reports. The convention is `mpsk_soft_demap`'s —
        positive means bit 0, so `L < 0` reproduces exactly the bits push()
        returned. Spans the WHOLE frame rather than the payload alone, because
        a code covers what its description says it covers.

        Valid until the next push(), reset() or set_state(); deliberately not
        serialized, for the same reason events() is not: it describes one call.

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : npt.NDArray[np.float32] | None
            Receives the LLRs.

        Returns
        -------
        NDArray[np.float32]
            LLRs written.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import DsssBurstReceiver
        >>> from doppler.wfm import Frame
        >>> rng = np.random.default_rng(0)
        >>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(3, np.uint8),
        ...              crc="crc16")     # 13 + 3 + 16 = 32 symbols a burst
        >>> rx = DsssBurstReceiver(
        ...     rng.integers(0, 2, 31).astype(np.uint8),
        ...     rng.integers(0, 2, 8).astype(np.uint8), desc, reps=4, spc=4)
        >>> bits = rx.push(np.zeros(4096, dtype=np.complex64))
        >>> len(bits), len(rx.llrs(rx.llrs_max_out(1)))   # nothing decoded
        (0, 0)

        """

    def llrs_max_out(self, n: int, /) -> int:
        """Max LLRs llrs() writes: frame bits x the bursts the last push
        returned.

        Parameters
        ----------
        n : int
            Ignored, as in llrs().

        Returns
        -------
        int
            Output.
        """

    def events(
        self,
        count: int = 1,
        out: npt.NDArray[Any] | None = None,
    ) -> NDArray[Any]:
        """The event record for each burst the last push() returned. Row i
        describes the frame at bits[i*frame_syms ...] of that push. Valid until
        the next push(), reset() or set_state().

        Row `i` describes the payload at `out[i*frame_syms ...]` of that push.
        A single push can complete many bursts and each needs its own event, so
        these are a list rather than the scalar read-backs -- those still exist
        and still describe the LAST burst, but they cannot speak for the
        others.

        Valid until the next push(), reset() or set_state(). Deliberately not
        serialized: it describes one call, and keeping it out of the blob is
        what holds state_bytes() to a pure function of configuration.

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : npt.NDArray[Any] | None
            Records, caller-owned, max_out long.

        Returns
        -------
        NDArray[Any]
            Records written to out -- `min(events_max_out(), max_out)`.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import DsssBurstReceiver
        >>> from doppler.wfm import Frame
        >>> rng = np.random.default_rng(0)
        >>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(3, np.uint8),
        ...              crc="crc16")     # 13 + 3 + 16 = 32 symbols a burst
        >>> rx = DsssBurstReceiver(
        ...     rng.integers(0, 2, 31).astype(np.uint8),
        ...     rng.integers(0, 2, 8).astype(np.uint8), desc, reps=4, spc=4)
        >>> bits = rx.push(np.zeros(4096, dtype=np.complex64))
        >>> len(rx.events()) == bits.size // 32   # one record per payload
        True

        """

    def events_max_out(self) -> int:
        """Max records events() writes: one per burst the last push() returned.

        Returns
        -------
        int
            The number of bursts the most recent push() completed.
        """

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the embedded BurstAcquisition's search grid directly, bypassing
        the auto-sizing -- the escape hatch for a caller who wants a specific
        (doppler_bins, n_noncoh). Forwards to the engine unchanged.

        The escape hatch for a caller who wants a specific (doppler_bins,
        n_noncoh) rather than the grid the cn0_dbhz/pfa/pd sizing chooses.
        Forwards to the embedded engine unchanged.

        Parameters
        ----------
        doppler_bins : int
            Coherent depth to pin, in `[1, reps]`.
        n_noncoh : int
            Non-coherent looks to combine.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``configure_search_raw failed``, with the return code appended
            (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import DsssBurstReceiver
        >>> from doppler.wfm import Frame
        >>> rng = np.random.default_rng(0)
        >>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(3, np.uint8),
        ...              crc="crc16")     # 13 + 3 + 16 = 32 symbols a burst
        >>> rx = DsssBurstReceiver(
        ...     rng.integers(0, 2, 31).astype(np.uint8),
        ...     rng.integers(0, 2, 8).astype(np.uint8), desc, reps=4, spc=4)
        >>> rx.configure_search_raw(doppler_bins=1, n_noncoh=1)

        """

    def reset(self) -> None:
        """Return to the searching state: resets the embedded acquisition,
        drops the history ring's contents and clears every read-back, so a
        fresh stream cannot inherit the previous burst's verdict. Construction
        parameters are untouched.

        Resets the embedded acquisition, discards the retained look-back, and
        clears all the event fields, so a fresh stream cannot inherit the
        previous burst's verdict. The lifetime counters (`n_bursts`, `dropped`)
        deliberately survive -- a reset that zeroed them could hide that this
        receiver had already lost samples. Construction parameters are
        untouched.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import DsssBurstReceiver
        >>> from doppler.wfm import Frame
        >>> rng = np.random.default_rng(0)
        >>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(3, np.uint8),
        ...              crc="crc16")     # 13 + 3 + 16 = 32 symbols a burst
        >>> rx = DsssBurstReceiver(
        ...     rng.integers(0, 2, 31).astype(np.uint8),
        ...     rng.integers(0, 2, 8).astype(np.uint8), desc, reps=4, spc=4)
        >>> _ = rx.push(np.zeros(1024, dtype=np.complex64))
        >>> rx.reset()

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the DsssBurstReceiver has already been
        destroyed.

        Returns
        -------
        int
            Byte length of one serialized state blob.
        """

    def get_state(self) -> bytes:
        """Serialize this object's mutable state to bytes.

        Captures the state that evolves as the object runs, including any value
        a setter changed after construction, so a blob taken now and restored
        later resumes from this point. Construction parameters are not
        restored: restore into an object built the same way. A core may check
        some of them and refuse a blob built differently.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the DsssBurstReceiver has already been
        destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes, /) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place, including any value a setter
        changed after construction. Construction parameters are not restored:
        restore into an object built the same way. Length is validated against
        `state_bytes()` before the blob is handed to the C core, which may
        reject it too: a core may check some construction parameters and refuse
        a blob built differently.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the DsssBurstReceiver has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def preamble_start(self) -> int:
        """Stream-absolute preamble start. Never late."""

    @property
    def doppler_hz_est(self) -> float:
        """Signed coarse Doppler, Hz."""

    @property
    def doppler_res_hz(self) -> float:
        """Width of that estimate."""

    @property
    def cn0_dbhz_est(self) -> float:
        """C/N0 lower bound, dB-Hz (saturating)."""

    @property
    def est_freq_hz(self) -> float:
        """Demod's own residual estimate, Hz."""

    @property
    def est_rate_hz(self) -> float:
        """Demod's own chirp-rate estimate."""

    @property
    def demod_cn0_dbhz(self) -> float:
        """The demodulator's C/N0 for the last completed burst, dB-Hz, referred
        to the CHANNEL: measured on the decoded symbols and corrected for the
        sub-chip start error `demod_timing_chips` reports. Distinct from
        `cn0_dbhz_est`, which is ACQUISITION's saturating lower bound from the
        hit; this one is a measurement, and the two are named for their stage
        rather than by word order so they cannot be confused. Replaces
        `est_snr_db`, which was never an SNR (doppler#1304).
        """

    @property
    def demod_timing_chips(self) -> float:
        """Burst-start error the demodulator measured on the last completed
        burst, in chips, signed. Whole samples were removed before despreading;
        the remaining fraction of a sample is what `demod_cn0_dbhz` is
        corrected for. Acquisition resolves a start to one SAMPLE, so a
        non-zero residual here is structural.
        """

    @property
    def frame_valid(self) -> bool:
        """Whether the most recent window's frame passed its error detection --
        the frame description's CRC stage covered it. The verdict that decides
        whether the window OWNS its span: a failed window is given back to the
        capture (`release`), so a decoy ahead of a real burst cannot swallow it
        (doppler#1181). The verdict is the frame description's own, so a frame
        whose description has no CRC stage is never valid -- its bits are still
        returned, and every window is released (doppler#1769). Per burst, read
        `events()['frame_valid']`.
        """

    @property
    def min_gap(self) -> int:
        """Dead air to leave BETWEEN bursts, in samples — edge to edge.

        The number a caller placing bursts actually wants, derived by the capture
        rather than left as a rule to apply: `refine_span + reps*code_period -
        burst_len`, floored at zero. Bursts spaced by exactly this all decode:
        measured at a 255-chip code, `reps=5`, `spc=2` over 40 lead offsets, every
        burst at its exact sample (doppler#1514). It is a guarantee, not the edge:
        at that geometry bursts an eighth of it apart decode too (doppler#1530).
        """

    @property
    def refine_span(self) -> int:
        """Coalescing window, in samples -- the reach over which two detections
        are ONE preamble.

        Both sides of that test are burst STARTS (resolved code epochs), so this
        bounds start-to-start separation, NOT the dead air between bursts: the two
        differ by a whole burst plus the detection lag. **Read `min_gap` for the
        gap to leave**; it is derived from this reach (doppler#1172). How much the
        reach matters depends on the burst: at a 255-chip code, `reps=5`, `spc=2`
        it is 12240 samples, and 8316-sample bursts with 600 samples of dead air
        between them (against the 3924 `min_gap` asks for) decode 2 of 7; the same
        code with a 2053-symbol frame gives 261228-sample bursts, 21.3x the reach,
        and every spacing down to zero dead air decodes 7 of 7 (doppler#1085).
        """

    @property
    def retain_span(self) -> int:
        """History kept per anchor, in samples -- the MINIMUM TRAILING CONTEXT.

        `refine_span` plus one whole burst. A burst closer than this to the end of
        what has been pushed is held rather than emitted, because refine cannot yet
        see the samples it needs. Feed at least this many more, or the last burst of
        a capture never comes out. At the geometry above the boundary is sharp: 20500
        trailing samples against a `retain_span` of 20556 loses the burst.
        """

    @property
    def pending(self) -> int:
        """Detections held because their burst window has not fully arrived.

        `push()` emits nothing for these on purpose -- a burst is returned when it
        is complete, not when it is guessed at. Feed more samples and it comes out
        bit-exact, wherever the split fell.

        Read it at the END of a stream. A caller closing a file or a socket while
        this is non-zero is discarding a burst that would have decoded, and nothing
        else distinguishes that from an empty capture: `dropped` counts look-back
        abandoned with a burst, `n_bursts` counts what was demodulated, and a
        truncated burst is neither.
        """

    @property
    def dropped(self) -> int:
        """Samples of look-back the capture discarded while a queued burst
        still needed them -- never refused input, which push() does not do. A
        LOST BURST, not a statistic -- a lifetime count that survives
        reset().
        """

    @property
    def n_bursts(self) -> int:
        """Bursts DEMODULATED, lifetime. Distinct from the capture's own count,
        which is windows EMITTED: they differ by any window the demodulator
        refused, and that difference is the thing worth seeing.
        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """

    def __enter__(self) -> "DsssBurstReceiver":
        """Enter a context manager, returning this object.

        Lets a DsssBurstReceiver be used in a `with` statement so its C
        resources are released deterministically on exit rather than at
        collection time.

        Returns
        -------
        DsssBurstReceiver
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
        /,
    ) -> None:
        """Exit a context manager, releasing the DsssBurstReceiver.

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
