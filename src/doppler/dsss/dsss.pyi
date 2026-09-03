# dsss/dsss.pyi — type stubs for the dsss C extension.
from typing import final, Literal
import numpy as np
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
class ReceiverStatus(tuple[int, float, float, float, float, int, int, float, float, float, float, int, int]):
    """AsyncDsssReceiver's status record: state (0 searching, 1 refining, 2
    tracking, 3 idle, 4 lost), the live estimates, both lock flags, and the two
    clocks in input samples.

    Attributes
    ----------
    state : int
        One of the ASYNC_DSSS_RX_SEARCHING .. _LOST values.
    doppler_hz : float
        Signed coarse Doppler, folded, Hz.
    chip_phase : float
        Chips, Dll's own instantaneous-phase convention (the mirror image of acq_result_t::code_phase's correlation-lag convention -- see acq_build_handoff()'s doc comment).
    code_rate : float
        chips advanced per nominal chip (~1.0).
    cn0_dbhz_est : float
        C/N0 lower bound from the hit, dB-Hz.
    code_locked : int
        Presence flag: the Dll's lock detector.
    locked : int
        Health flag: the symbol-lock detector.
    lock_metric : float
        mean of |Re P|/|P| over the burst (~1 locked, ~2/pi with no carrier).
    lock_threshold : float
        `locked` latches above this.
    car_last_error : float
        Pre-despread Costas residual, rad.
    mpsk_last_error : float
        Post-despread carrier residual, rad.
    state_samples : int
        Running: samples fed since the current state was entered.
    both_down_samples : int
        Running: consecutive samples fed while tracking with BOTH lock flags down -- the release clock; lost keeps it counting.
    """

    @property
    def state(self) -> int:
        """One of the ASYNC_DSSS_RX_SEARCHING .. _LOST values."""

    @property
    def doppler_hz(self) -> float:
        """Signed coarse Doppler, folded, Hz."""

    @property
    def chip_phase(self) -> float:
        """Chips, Dll's own instantaneous-phase convention (the mirror image of
        acq_result_t::code_phase's correlation-lag convention -- see
        acq_build_handoff()'s doc comment).
        """

    @property
    def code_rate(self) -> float:
        """chips advanced per nominal chip (~1.0)."""

    @property
    def cn0_dbhz_est(self) -> float:
        """C/N0 lower bound from the hit, dB-Hz."""

    @property
    def code_locked(self) -> int:
        """Presence flag: the Dll's lock detector."""

    @property
    def locked(self) -> int:
        """Health flag: the symbol-lock detector."""

    @property
    def lock_metric(self) -> float:
        """mean of |Re P|/|P| over the burst (~1 locked, ~2/pi with no
        carrier).
        """

    @property
    def lock_threshold(self) -> float:
        """`locked` latches above this."""

    @property
    def car_last_error(self) -> float:
        """Pre-despread Costas residual, rad."""

    @property
    def mpsk_last_error(self) -> float:
        """Post-despread carrier residual, rad."""

    @property
    def state_samples(self) -> int:
        """Running: samples fed since the current state was entered."""

    @property
    def both_down_samples(self) -> int:
        """Running: consecutive samples fed while tracking with BOTH lock flags
        down -- the release clock; lost keeps it counting.
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
        code: NDArray[np.uint8],
        sps: int = ...,
        init_norm_freq: float = ...,
        init_chip: float = ...,
        bn_carrier: float = ...,
        bn_code: float = ...,
        bn_fll: float = ...,
        zeta: float = ...,
        spacing: float = ...,
        periods_per_bit: int = ...,
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
        loop registers "<prefix>.car.lock" / ".e" / ".freq" / ".locked" and the
        code loop registers "<prefix>.code.e" / ".rate" / ".lock" / ".locked"
        (the ".locked" pair are the loops' verify-counted lockdet decisions,
        0/1) — eight probes, all thinned by decim and emitted once per code
        period (the despreader flushes both loops at its per-period update).
        Passing NULL detaches both loops. Setup path, never hot; the context is
        borrowed and must outlive the attachment (SPSC rules in
        dp_tlm/dp_tlm_core.h).

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
        >>> names[:4]
        ['ch0.car.e', 'ch0.car.freq', 'ch0.car.lock', 'ch0.car.locked']
        >>> names[4:]
        ['ch0.code.e', 'ch0.code.lock', 'ch0.code.locked', 'ch0.code.rate']
        >>> chips = 1.0 - 2.0 * (np.arange(31) % 2)
        >>> x = np.tile(np.repeat(chips, 4), 40).astype(np.complex64)
        >>> _ = ch.steps(x)
        >>> recs = tlm.read()   # eight records per code period
        >>> len(recs) > 0 and len(recs) % 8 == 0
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

        Thin forwarder to costas_configure_lock() on the embedded Costas loop —
        symmetric with despreader_get_carrier_locked() exposing its state:
        state is readable, so config should be writable too, rather than
        forcing a caller who needs this control to drop to raw Dll+Costas
        composition instead of Despreader. See costas_configure_lock() for the
        parameter semantics.

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

        Thin forwarder to dll_configure_lock() on the embedded DLL — the
        derived (pfa-style) entry point, matching Despreader's role as the
        "easy" composed API (Dll's raw escape hatch, dll_configure_lock_raw(),
        stays a Dll-only control for a caller that composes Dll+Costas
        directly). See dll_configure_lock() for the parameter semantics.

        Parameters
        ----------
        pfa : float
            Per-decision false-alarm probability, in (0, 1).
        n_looks : int
            Non-coherent integration depth N (looks); clamped >= 1.
        ref_snr_db : float
            Noise-reference estimator SNR in dB (> 0), or 0 to derive from
            n_looks (see dll_configure_lock()).

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

        Captures exactly the state that evolves as the object runs, so a blob
        taken now and restored later resumes from this point. Construction
        parameters are not included: restore into an object built the same way.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the Despreader has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()`
        before the blob is handed to the C core, and the core may reject it as
        well.

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
        """chips advanced per nominal chip (~1.0)."""

    @property
    def lock_metric(self) -> float:
        """EMA of |Re P|/|P| (1 = locked)."""

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
        """Bn carrier."""
    @bn_carrier.setter
    def bn_carrier(self, value: float) -> None: ...

    @property
    def bn_code(self) -> float:
        """Bn code."""
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
        Seed carrier frequency, cycles/sample — the acquisition estimate
        (default: 0.0).
    init_chip_phase : float, default 0.0
        Seed code phase, chips (default: 0.0).
    bn_carrier : float, default 0.05
        Carrier (Costas) loop noise bandwidth, normalized to the symbol rate
        (default: 0.05).
    bn_code : float, default 0.01
        Code (DLL) loop noise bandwidth, normalized to the symbol rate
        (default: 0.01).

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
        code: NDArray[np.uint8],
        sf: int = ...,
        sps: int = ...,
        init_norm_freq: float = ...,
        init_chip_phase: float = ...,
        bn_carrier: float = ...,
        bn_code: float = ...,
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

    def set_acq(self, acq_code: NDArray[np.uint8], acq_reps: int) -> None:
        """Enable preamble-aided pull-in: track acq_reps periods of the
        (distinct) acq_code coherently before despreading the payload with the
        data code. Call before feeding the burst; clears when the preamble is
        consumed.

        Track acq_reps periods of acq_code coherently (the unmodulated,
        repeated acquisition preamble — a full ±pi phase discriminator, so the
        loops pull in even a wide residual) before switching to the data code
        for the payload. Call before feeding the burst; the acq mode clears
        automatically once the preamble is consumed, and re-arms on
        burst_despreader_reset(). NB: set_acq re-arms the PREAMBLE only — the
        cumulative burst statistics (lock_metric / snr_est / lock_stat /
        stat_n) are re-armed by burst_despreader_reset(); call it between
        bursts.

        Parameters
        ----------
        acq_code : NDArray[np.uint8]
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
        burst's statistics start clean; a prior burst_despreader_set_acq()
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

        Captures exactly the state that evolves as the object runs, so a blob
        taken now and restored later resumes from this point. Construction
        parameters are not included: restore into an object built the same way.

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

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()`
        before the blob is handed to the C core, and the core may reject it as
        well.

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
        """
    @bn_carrier.setter
    def bn_carrier(self, value: float) -> None: ...

    @property
    def bn_code(self) -> float:
        """Code (DLL) loop noise bandwidth, normalized to the symbol rate."""
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
class Acquisition:
    """Create a continuous-mode acquisition engine: always wideband
    window-tiling, never coherent multi-epoch combining.

    Parameters
    ----------
    code : NDArray[np.uint8]
        PN chips (0/1), length code_len.
    spc : int, default 4
        Samples per chip (>= 1).
    chip_rate : float, default 1000000.0
        Chip rate in Hz (> 0).
    symbol_rate : float, default 1000.0
        Continuous data-symbol rate in Hz; <= 0 means no known clock.
        Diagnostic only (exposed via acq_state_t::epochs_per_symbol), doesn't
        feed sizing: this engine never coherently combines regardless of the
        data-modulation clock.
    cn0_dbhz : float, default 50.0
        Carrier-to-noise density in dB-Hz (> 0).
    doppler_uncertainty : float, default 0.0
        One-sided Doppler search half-range in Hz; 0 uses the full native span
        +/- chip_rate/(2*sf) (still window-tiled, at window_bins=1).
    pfa : float, default 1e-3
        Target system (max-of-N) false-alarm probability (0,1).
    pd : float, default 0.9
        Target detection probability (0,1).
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        CFAR mode index: 0=mean, 1=median, 2=min, 3=max.

    Warns
    -----
    UserWarning
        Emitted after construction when ``underpowered`` holds: ``Acquisition
        is under-powered: pd_predicted < pd at this cn0_dbhz. Raise cn0_dbhz or
        narrow doppler_uncertainty.``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.dsss import Acquisition
    >>> from doppler.wfm import PN, mls_poly
    >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
    ...                      length=5).generate(31)).astype(np.uint8)
    >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
    ...     np.complex64)
    >>> burst = np.tile(np.roll(s0, 17), 23).astype(np.complex64)
    >>> a = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0)
    >>> a.push(burst)[0][:2]    # detects (Doppler-window bin, code phase)
    (0, 17)

    """
    def __init__(
        self,
        code: NDArray[np.uint8],
        spc: int = ...,
        chip_rate: float = ...,
        symbol_rate: float = ...,
        cn0_dbhz: float = ...,
        doppler_uncertainty: float = ...,
        pfa: float = ...,
        pd: float = ...,
        noise_mode: Literal["mean", "median", "min", "max"] = "mean",
    ) -> None: ...

    def reset(self) -> None:
        """Drain the input ring and reset the coherent accumulator.

        Discards any buffered samples that have not yet completed a frame and
        clears the non-coherent power accumulator and dwell bookkeeping, so the
        next push() begins a fresh search from an empty ring. The construction
        parameters — grid, thresholds, and PN reference — are untouched; only
        the in-flight streaming state is dropped.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import Acquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> burst = np.tile(np.roll(s0, 17), 23).astype(np.complex64)
        >>> a = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0)
        >>> _ = a.push(burst[:100])   # a partial frame, buffered mid-stream
        >>> a.reset()                 # drop it before it can bias a detection
        >>> a.push(burst)[0][:2]      # (Doppler bin, code phase)
        (0, 17)

        """

    def push(
        self,
        x: complex,
    ) -> list[tuple[int, int, float, float, float, float, int]]:
        """Stream raw samples; emit one event per CFAR dump above threshold.

        Buffers x, then for every complete frame applies the slow-time Doppler
        FFT, correlates against the PN reference, dumps the coherent surface
        (or, when n_noncoh > 1, accumulates |·|² over n_noncoh looks first),
        gates the peak on the auto-configured threshold, and appends an
        acq_result_t. Each event carries the peak's Doppler bin and code phase
        (the two search axes), its CFAR statistic, and an estimated C/N0 — see
        acq_result_t.

        Parameters
        ----------
        x : complex
            Raw input, interleaved CF32, n_in complex samples.

        Returns
        -------
        list[tuple[int, int, float, float, float, float, int]]
            Number of events written (0 … max_results).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import Acquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> a = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0,
        ...                 doppler_uncertainty=40e3)
        >>> fs = 1e6 * 4                    # sample rate = chip_rate * spc
        >>> t = np.arange(a.code_bins * a.n_noncoh)
        >>> carrier = np.exp(2j * np.pi * (a.doppler_res_hz / fs) * t)
        >>> sig = (np.tile(np.roll(s0, 17), a.n_noncoh)
        ...        * carrier).astype(np.complex64)
        >>> a.push(sig)[0][:2]              # (Doppler-window bin, code phase)
        (1, 17)

        """

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the search grid directly, bypassing both auto-sizing searches --
        the advanced escape hatch (mirrors
        Dll.configure_lock_raw/Costas.configure_lock). Resizes every
        buffer/plan that depends on the grid (the slow-time FFT, the code
        correlator, the reference, and every per-frame scratch buffer),
        re-derives the threshold ladder for the pinned grid from the same
        physics __init__ used, and clears in-flight accumulation (ring
        contents, the non-coherent power accumulator, dwell bookkeeping) --
        call between push() calls, never a substitute for one. Raises
        ValueError if doppler_bins is outside [1, reps] or n_noncoh is outside
        [1, 256] (the internal non-coherent-look safety-valve ceiling).

        Resizes every buffer/plan that depends on the grid (the slow-time FFT,
        the code correlator, the reference, and every per-frame scratch
        buffer), re-derives the threshold ladder for the pinned grid from the
        same physics acq_create_burst()/acq_create_continuous() used, and
        clears in-flight accumulation (ring contents, the non-coherent power
        accumulator, dwell bookkeeping) — call between push() calls, never a
        substitute for one.

        Parameters
        ----------
        doppler_bins : int
            Coherent depth to pin, in `[1, reps]`.
        n_noncoh : int
            Non-coherent look count to pin, in `[1,
            ACQ_N_NONCOH_SAFETY_CEILING]`.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``configure_search_raw failed``, with the return code appended
            (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import Acquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> a = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0)
        >>> a.configure_search_raw(doppler_bins=1, n_noncoh=4)  # pin the grid
        >>> a.doppler_bins, a.n_noncoh
        (1, 4)
        >>> burst = np.tile(np.roll(s0, 17), 4).astype(np.complex64)
        >>> a.push(burst)[0][:2]      # detects at the pinned grid
        (0, 17)

        """

    def set_max_peaks(self, n: int) -> None:
        """How many peaks a dwell may report -- the peak list's capacity
        (docs/design/async-dsss-receiver.md section 7.1). One (the default) is
        the classic gated maximum. More lists every peak above the same gate,
        strongest first, with an exclusion zone of one Doppler bin by one chip
        around each (one emitter's main lobe, so its own shoulders are not the
        next peak) and the two-epoch rule for a peak at an already-listed code
        phase (a data transition inside the epoch splits one emitter into twins
        at its own code phase on other tiles; such a peak is held for one dwell
        and listed only if it is still there, at the same tile, on the next).
        Each listed peak is one record from push(), all of a dwell's sharing
        samples_consumed and noise_est; a held twin takes one of the n slots
        that dwell but is not reported. The threshold does not change with n.
        Raises ValueError outside 1..64. Clears the held candidates.

        One (the default) is the classic detector -- the maximum of the
        surface, gated. More is the list of docs/design/async-dsss-receiver.md
        §7.1: every peak above the same gate, strongest first, each with an
        exclusion zone of one Doppler bin by one chip around it (one emitter's
        main lobe, so its own shoulders are not the next peak), and the
        two-epoch rule for a peak at an already-listed code phase -- a data
        transition inside the epoch splits one emitter into twins at its own
        code phase on other tiles, so such a peak is held for one dwell and
        listed only if it was there, at the same tile, on the previous one.
        Each listed peak is one acq_result_t from acq_push(), all of a dwell's
        sharing its `samples_consumed` and `noise_est`. A held twin takes a
        slot of the `n` for that dwell but is not reported. The threshold does
        not change: a second peak is another draw from the same cells against
        the same union bound. Clears the held candidates.

        Parameters
        ----------
        n : int
            1 … ACQ_MAX_PEAKS.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_max_peaks failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import Acquisition
        >>> code = (np.arange(31) * 5 % 2).astype(np.uint8)
        >>> a = Acquisition(code, spc=2, chip_rate=1e6, symbol_rate=1e3,
        ...                 cn0_dbhz=50.0, doppler_uncertainty=50e3)
        >>> a.max_peaks
        1
        >>> a.set_max_peaks(8)
        >>> a.max_peaks
        8

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the Acquisition has already been destroyed.

        Returns
        -------
        int
            Byte length of one serialized state blob.
        """

    def get_state(self) -> bytes:
        """Serialize this object's mutable state to bytes.

        Captures exactly the state that evolves as the object runs, so a blob
        taken now and restored later resumes from this point. Construction
        parameters are not included: restore into an object built the same way.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the Acquisition has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()`
        before the blob is handed to the C core, and the core may reject it as
        well.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the Acquisition has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def max_peaks(self) -> int:
        """The peak list's capacity per dwell (1 = the classic gated maximum);
        set with set_max_peaks().
        """

    @property
    def code_bins(self) -> int:
        """Code-phase hypotheses searched (= sf*spc, one code period)."""

    @property
    def doppler_bins(self) -> int:
        """Effective Doppler search granularity this engine picked: the
        window-tile count (this engine always window-tiles -- see acq_core.h's
        file doc comment -- so this is window_bins, never a coherent-depth
        axis).
        """

    @property
    def sf(self) -> int:
        """Chips per PN segment, inferred from len(code)."""

    @property
    def spc(self) -> int:
        """Samples per chip (chip-rate oversample factor)."""

    @property
    def n_noncoh(self) -> int:
        """Non-coherent looks per detection (1 = pure coherent)."""

    @property
    def ring_cap(self) -> int:
        """Input ring capacity in complex samples."""

    @property
    def noise_lo(self) -> int:
        """First CFAR reference bin (inclusive)."""

    @property
    def noise_hi(self) -> int:
        """Last CFAR reference bin (inclusive)."""

    @property
    def threshold(self) -> float:
        """CFAR gate on the test statistic (coherent path)."""

    @property
    def eta(self) -> float:
        """Raw per-cell Rayleigh amplitude threshold."""

    @property
    def eta_nc(self) -> float:
        """Non-coherent CFAR threshold (order-N_nc Marcum)."""

    @property
    def pfa_cell(self) -> float:
        """Bonferroni per-cell false-alarm probability over the searched
        cells.
        """

    @property
    def pd_predicted(self) -> float:
        """Predicted Pd at cn0_dbhz and the chosen grid: the average Pd over
        the straddle priors (slow-time scalloping, intra-segment rotation,
        code-phase sample offset - quadrature over uniform priors), matching
        what the Monte-Carlo characterization measures rather than the on-grid
        best case.
        """

    @property
    def straddle_loss(self) -> float:
        """Mean amplitude derating of the correlation peak from grid straddle
        (slow-time Doppler scalloping x intra-segment rotation x code-phase
        sample offset, each averaged over a uniform prior) - a diagnostic
        summary; 20*log10(straddle_loss) is the loss in dB. Sizing and
        pd_predicted average Pd itself over the priors (Pd at this mean
        amplitude would overstate the mean Pd).
        """

    @property
    def fs(self) -> float:
        """Sample rate (Hz) = chip_rate * spc."""

    @property
    def chip_rate(self) -> float:
        """Chip rate (Hz)."""

    @property
    def cn0_dbhz(self) -> float:
        """Carrier-to-noise density used to size the search (dB-Hz)."""

    @property
    def doppler_span_hz(self) -> float:
        """Native unambiguous Doppler half-range = +/- chip_rate/(2*sf) Hz."""

    @property
    def doppler_res_hz(self) -> float:
        """Doppler bin width = chip_rate/(sf*doppler_bins) Hz."""

    @property
    def pd(self) -> float:
        """Target detection probability."""

    @property
    def underpowered(self) -> bool:
        """True when pd_predicted < pd -- the search cannot meet the target pd
        at this cn0_dbhz and geometry. The engine still builds a best-effort
        grid rather than failing; because C cannot raise a Python warning from
        a successful create, construction also emits a UserWarning in this
        case.
        """

    @property
    def symbol_rate(self) -> float:
        """Continuous data-symbol rate (Hz) this engine was built with --
        diagnostic only, doesn't feed sizing (this engine never coherently
        combines regardless).
        """

    @property
    def epochs_per_symbol(self) -> float:
        """(chip_rate/sf)/symbol_rate -- code epochs per data symbol; 0 when
        symbol_rate is 0.
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


    def __enter__(self) -> "Acquisition":
        """Enter a context manager, returning this object.

        Lets a Acquisition be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Acquisition
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the Acquisition.

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
class BurstAcquisition:
    """Create a burst-mode acquisition engine (forwards to acq_create_burst()
    -- see its doc comment in acq_core.h for the full physics).

    Parameters
    ----------
    code : NDArray[np.uint8]
        PN chips (0/1), length code_len.
    reps : int, default 1
        Max coherent code repetitions (>= 1).
    spc : int, default 4
        Samples per chip (>= 1).
    chip_rate : float, default 1000000.0
        Chip rate in Hz (> 0).
    cn0_dbhz : float, default 0.0
        Carrier-to-noise density in dB-Hz (> 0).
    doppler_uncertainty : float, default 0.0
        One-sided Doppler search half-range in Hz.
    pfa : float, default 1e-3
        Target system false-alarm probability (0,1).
    pd : float, default 0.9
        Target detection probability (0,1).
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        CFAR mode index: 0=mean, 1=median, 2=min, 3=max.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.dsss import BurstAcquisition
    >>> from doppler.wfm import PN, mls_poly
    >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
    ...                      length=5).generate(31)).astype(np.uint8)
    >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
    ...     np.complex64)
    >>> burst = np.tile(np.roll(s0, 17), 24).astype(np.complex64)
    >>> b = BurstAcquisition(code, reps=8, spc=4, chip_rate=1e6,
    ...                      cn0_dbhz=50.0)
    >>> b.push(burst)[0][:2]      # detects (Doppler bin, code phase)
    (0, 17)

    """
    def __init__(
        self,
        code: NDArray[np.uint8],
        reps: int = ...,
        spc: int = ...,
        chip_rate: float = ...,
        cn0_dbhz: float = ...,
        doppler_uncertainty: float = ...,
        pfa: float = ...,
        pd: float = ...,
        noise_mode: Literal["mean", "median", "min", "max"] = "mean",
    ) -> None: ...

    def reset(self) -> None:
        """Drain the input ring and reset the coherent accumulator.

        Forwards to acq_reset() on the embedded engine: discards any buffered
        samples that have not yet completed a frame and clears the non-coherent
        power accumulator and dwell bookkeeping, so the next push() begins a
        fresh search from an empty ring. Construction parameters are untouched.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstAcquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> burst = np.tile(np.roll(s0, 17), 24).astype(np.complex64)
        >>> b = BurstAcquisition(code, reps=8, spc=4, chip_rate=1e6,
        ...                      cn0_dbhz=50.0)
        >>> _ = b.push(burst[:100])   # a partial frame, buffered mid-stream
        >>> b.reset()                 # drop it before it can bias a detection
        >>> b.push(burst)[0][:2]      # (Doppler bin, code phase)
        (0, 17)

        """

    def push(
        self,
        x: complex,
    ) -> list[tuple[int, int, float, float, float, float, int]]:
        """Stream raw samples; emit one event per CFAR dump above threshold.

        Forwards to acq_push() on the embedded engine (see its doc comment in
        acq_core.h for the framing/CFAR mechanics). Each event carries the
        peak's Doppler bin and code phase (the two search axes), its CFAR
        statistic, and an estimated C/N0 — see acq_result_t.

        Parameters
        ----------
        x : complex
            Raw input, interleaved CF32, n_in complex samples.

        Returns
        -------
        list[tuple[int, int, float, float, float, float, int]]
            Number of events written (0 … max_results).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstAcquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> burst = np.tile(np.roll(s0, 17), 24).astype(np.complex64)
        >>> b = BurstAcquisition(code, reps=8, spc=4, chip_rate=1e6,
        ...                      cn0_dbhz=50.0)
        >>> b.push(burst)[0][:2]      # (Doppler bin, code phase)
        (0, 17)

        """

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the search grid directly, bypassing both auto-sizing searches --
        the advanced escape hatch (mirrors
        Dll.configure_lock_raw/Costas.configure_lock). Resizes every
        buffer/plan that depends on the grid (the slow-time FFT, the code
        correlator, the reference, and every per-frame scratch buffer),
        re-derives the threshold ladder for the pinned grid from the same
        physics __init__ used, and clears in-flight accumulation (ring
        contents, the non-coherent power accumulator, dwell bookkeeping) --
        call between push() calls, never a substitute for one. Raises
        ValueError if doppler_bins is outside [1, reps] or n_noncoh is outside
        [1, 256] (the internal non-coherent-look safety-valve ceiling).

        Forwards to acq_configure_search_raw() on the embedded engine (see its
        doc comment in acq_core.h): resizes every grid-dependent buffer/plan,
        re-derives the threshold ladder for the pinned grid, and clears
        in-flight accumulation — call between push() calls, never a substitute
        for one.

        Parameters
        ----------
        doppler_bins : int
            Coherent depth to pin, in `[1, reps]`.
        n_noncoh : int
            Non-coherent look count to pin, in `[1,
            ACQ_N_NONCOH_SAFETY_CEILING]`.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``configure_search_raw failed``, with the return code appended
            (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstAcquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> b = BurstAcquisition(code, reps=8, spc=4, chip_rate=1e6,
        ...                      cn0_dbhz=50.0)
        >>> b.configure_search_raw(doppler_bins=4, n_noncoh=2)  # pin the grid
        >>> b.doppler_bins, b.n_noncoh
        (4, 2)
        >>> burst = np.tile(np.roll(s0, 17), 8).astype(np.complex64)
        >>> b.push(burst)[0][:2]      # detects at the pinned grid
        (0, 17)

        """

    def set_max_peaks(self, n: int) -> None:
        """How many peaks a dwell may report -- the peak list's capacity
        (docs/design/async-dsss-receiver.md section 7.1). One (the default) is
        the classic gated maximum. More lists every peak above the same gate,
        strongest first, with an exclusion zone of one Doppler bin by one chip
        around each (one emitter's main lobe, so its own shoulders are not the
        next peak) and the two-epoch rule for a peak at an already-listed code
        phase (a data transition inside the epoch splits one emitter into twins
        at its own code phase on other tiles; such a peak is held for one dwell
        and listed only if it is still there, at the same tile, on the next).
        Each listed peak is one record from push(), all of a dwell's sharing
        samples_consumed and noise_est; a held twin takes one of the n slots
        that dwell but is not reported. The threshold does not change with n.
        Raises ValueError outside 1..64. Clears the held candidates.

        Forwards to acq_set_max_peaks() on the embedded engine (see its doc
        comment in acq_core.h): one is the classic gated maximum; more is the
        list of docs/design/async-dsss-receiver.md §7.1 -- every peak above the
        same gate, strongest first, an exclusion zone of one Doppler bin by one
        chip around each, and the two-epoch rule for a peak at an
        already-listed code phase. Each listed peak is one result from push().

        Parameters
        ----------
        n : int
            1 … ACQ_MAX_PEAKS.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_max_peaks failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstAcquisition
        >>> code = (np.arange(31) * 5 % 2).astype(np.uint8)
        >>> b = BurstAcquisition(code, reps=8, spc=4, chip_rate=1e6,
        ...                      cn0_dbhz=50.0)
        >>> b.set_max_peaks(4)
        >>> b.max_peaks
        4

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the BurstAcquisition has already been
        destroyed.

        Returns
        -------
        int
            Byte length of one serialized state blob.
        """

    def get_state(self) -> bytes:
        """Serialize this object's mutable state to bytes.

        Captures exactly the state that evolves as the object runs, so a blob
        taken now and restored later resumes from this point. Construction
        parameters are not included: restore into an object built the same way.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the BurstAcquisition has already been
        destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()`
        before the blob is handed to the C core, and the core may reject it as
        well.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the BurstAcquisition has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def max_peaks(self) -> int:
        """The peak list's capacity per dwell (1 = the classic gated maximum);
        set with set_max_peaks().
        """

    @property
    def code_bins(self) -> int:
        """Code-phase hypotheses searched (= sf*spc, one code period)."""

    @property
    def doppler_bins(self) -> int:
        """Coherent depth chosen: the slow-time FFT length in code reps (<=
        reps), unless doppler_uncertainty exceeds the native span, in which
        case this reports the wideband window-tile count instead (coherent
        depth forced to 1 -- see acq_core.h's file doc comment).
        """

    @property
    def sf(self) -> int:
        """Chips per PN segment, inferred from len(code)."""

    @property
    def spc(self) -> int:
        """Samples per chip (chip-rate oversample factor)."""

    @property
    def reps(self) -> int:
        """Max coherent code repetitions (the coherence ceiling)."""

    @property
    def n_noncoh(self) -> int:
        """Non-coherent looks per detection (1 = pure coherent)."""

    @property
    def ring_cap(self) -> int:
        """Input ring capacity in complex samples."""

    @property
    def noise_lo(self) -> int:
        """First CFAR reference bin (inclusive)."""

    @property
    def noise_hi(self) -> int:
        """Last CFAR reference bin (inclusive)."""

    @property
    def threshold(self) -> float:
        """CFAR gate on the test statistic (coherent path)."""

    @property
    def eta(self) -> float:
        """Raw per-cell Rayleigh amplitude threshold."""

    @property
    def eta_nc(self) -> float:
        """Non-coherent CFAR threshold (order-N_nc Marcum)."""

    @property
    def pfa_cell(self) -> float:
        """Bonferroni per-cell false-alarm probability over the searched
        cells.
        """

    @property
    def pd_predicted(self) -> float:
        """Predicted Pd at cn0_dbhz and the chosen grid: the average Pd over
        the straddle priors (slow-time scalloping, intra-segment rotation,
        code-phase sample offset - quadrature over uniform priors), matching
        what the Monte-Carlo characterization measures rather than the on-grid
        best case.
        """

    @property
    def straddle_loss(self) -> float:
        """Mean amplitude derating of the correlation peak from grid straddle
        (slow-time Doppler scalloping x intra-segment rotation x code-phase
        sample offset, each averaged over a uniform prior) - a diagnostic
        summary; 20*log10(straddle_loss) is the loss in dB. Sizing and
        pd_predicted average Pd itself over the priors (Pd at this mean
        amplitude would overstate the mean Pd).
        """

    @property
    def fs(self) -> float:
        """Sample rate (Hz) = chip_rate * spc."""

    @property
    def chip_rate(self) -> float:
        """Chip rate (Hz)."""

    @property
    def cn0_dbhz(self) -> float:
        """Carrier-to-noise density used to size the search (dB-Hz)."""

    @property
    def doppler_span_hz(self) -> float:
        """Native unambiguous Doppler half-range = +/- chip_rate/(2*sf) Hz."""

    @property
    def doppler_res_hz(self) -> float:
        """Doppler bin width = chip_rate/(sf*doppler_bins) Hz."""

    @property
    def pd(self) -> float:
        """Target detection probability."""

    @property
    def underpowered(self) -> bool:
        """True when pd_predicted < pd -- the search cannot meet the target pd
        at this cn0_dbhz and geometry. The engine still builds a best-effort
        grid rather than failing; because C cannot raise a Python warning from
        a successful create, construction also emits a UserWarning in this
        case.
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


    def __enter__(self) -> "BurstAcquisition":
        """Enter a context manager, returning this object.

        Lets a BurstAcquisition be used in a `with` statement so its C
        resources are released deterministically on exit rather than at
        collection time.

        Returns
        -------
        BurstAcquisition
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the BurstAcquisition.

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
    def __init__(self, max_len: int = ..., max_rate: float = ...) -> None: ...

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

    def estimate(self, x: complex) -> PolynomialPhaseEstimate:
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
        """zero-padded transform length: 4 * next_pow2 (max_len). The 4x is
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
    """Create a feedforward BPSK DSSS burst demodulator.

    Parameters
    ----------
    data_code : NDArray[np.uint8]
        Data spreading code, one 0/1 chip per element; copied into the object
        (its length is the data spreading factor, chips/symbol).
    spc : int, default 4
        Samples per chip (front-end oversample).
    chip_rate : float, default 1.0e6
        Chip rate (Hz); sets the sample rate as spc*chip_rate.
    carrier_hz : float, default 0.0
        RF carrier (Hz) for code-Doppler scaling; 0 = ignore.
    max_rate : float, default 0.0
        Chirp-rate search half-span (cycles/sample^2 at the input rate); 0 =
        Doppler only (no rate search).
    frame_syms : int, default 0
        Symbols the frame occupies after the sync word — how many bits demod()
        hands back per burst. What they mean is a frame description's business.
    est_segments : int, default 10
        Partial correlations per acq period (segmentation for the feedforward
        estimate; larger tolerates more rate).

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.dsss import BurstDemod
    >>> spc, acq_sf, reps, data_sf = 4, 500, 5, 50
    >>> sync = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)
    >>> acode = ((np.arange(acq_sf) * 2654435761 >> 13) & 1).astype(
    ...     np.uint8)
    >>> dcode = ((np.arange(data_sf) * 40503 >> 7) & 1).astype(np.uint8)
    >>> payload = ((np.arange(64) * 7 + 3) & 1).astype(np.uint8)
    >>> def crc16(bits):
    ...     c = 0xFFFF
    ...     for b in bits:
    ...         c ^= (int(b) & 1) << 15
    ...         c = (((c << 1) ^ 0x1021) & 0xFFFF
    ...              if c & 0x8000 else (c << 1) & 0xFFFF)
    ...     return c
    >>> crc = crc16(payload)
    >>> crc_bits = np.array(
    ...     [(crc >> (15 - j)) & 1 for j in range(16)], np.uint8)
    >>> frame = np.concatenate([sync, payload, crc_bits])
    >>> csign = lambda b: np.where(np.asarray(b) & 1, -1.0, 1.0)
    >>> chips = ([np.tile(csign(acode), reps)]
    ...          + [csign(b) * csign(dcode) for b in frame])
    >>> bb = np.repeat(np.concatenate(chips), spc).astype(np.complex64)
    >>> n = np.arange(len(bb))
    >>> f0 = 0.012
    >>> x = (bb * np.exp(2j * np.pi * f0 * n)).astype(np.complex64)
    >>> d = BurstDemod(dcode, spc=spc, chip_rate=1e6, frame_syms=len(frame))
    >>> d.set_preamble(acode, reps)   # unmodulated (f0, rate) preamble
    >>> d.set_sync(sync)              # Barker-13: frame align + sign fix
    >>> d.set_prior(f0, 0)            # coarse Doppler + preamble start
    >>> bits = d.demod(x)      # estimate -> dechirp -> despread -> slice
    >>> bool(np.array_equal(bits, frame))   # the FRAME, not the payload
    True

    """
    def __init__(
        self,
        data_code: NDArray[np.uint8],
        spc: int = ...,
        chip_rate: float = ...,
        carrier_hz: float = ...,
        max_rate: float = ...,
        frame_syms: int = ...,
        est_segments: int = ...,
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
        >>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, frame_syms=93)
        >>> d.reset()          # clears the estimates, keeps the config
        >>> d.frame_offset
        0

        """

    def set_preamble(self, acq_code: NDArray[np.uint8], reps: int) -> None:
        """Set the (unmodulated) acquisition preamble code + repetition count
        used for the feedforward (f0, rate) estimate.

        The preamble is the acq spreading code transmitted reps times with no
        data modulation; demod() segment-despreads it into partial correlations
        and feeds those to the polynomial-phase estimator to recover the coarse
        (frequency, chirp-rate). Call once after construction; the code is
        copied.

        Parameters
        ----------
        acq_code : NDArray[np.uint8]
            Acq preamble spreading code, one 0/1 chip per element; copied into
            the object.
        reps : int
            Number of preamble repetitions in the burst.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstDemod
        >>> dcode = (np.arange(50) & 1).astype(np.uint8)
        >>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, frame_syms=93)
        >>> acode = (np.arange(500) & 1).astype(np.uint8)  # unmodulated
        >>> d.set_preamble(acode, reps=5)  # 5 reps drive the (f0, rate) fit

        """

    def set_sync(self, sync: NDArray[np.uint8]) -> None:
        """Set the known frame-sync word (0/1 BPSK symbols) used for frame
        alignment and phase/sign resolution. The ONLY thing this object is told
        about the frame's content, and for a physical-layer reason: without the
        sign the slicer would be a coin toss. Where the payload sits, which
        stages cover what and whether a check passed all need the frame's
        description and belong one layer up (doppler#1022).

        After the data section is despread to soft BPSK symbols, demod()
        correlates them against this word; the complex correlation peak locates
        the frame (its frame_offset) and its phase resolves the residual
        carrier rotation and the BPSK sign ambiguity before slicing. Pass the
        word as 0/1 symbols; it is copied and stored internally as +/-1.

        This is the ONLY thing this object is told about the frame's content,
        and it is told it for a physical-layer reason: without the sign the
        slicer would be a coin toss. Everything else — where the payload sits,
        which stages cover what, whether a check passed — needs the frame's
        description and belongs one layer up (doppler#1022).

        Parameters
        ----------
        sync : NDArray[np.uint8]
            Frame-sync word, one 0/1 symbol per element; copied.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstDemod
        >>> dcode = (np.arange(50) & 1).astype(np.uint8)
        >>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, frame_syms=93)
        >>> sync = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)
        >>> d.set_sync(sync)   # Barker-13: frame align + phase/sign fix

        """

    def llrs(
        self,
        count: int = 1,
        out: NDArray[np.float32] | None = None,
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
        out : NDArray[np.float32] | None
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
        >>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, frame_syms=93)
        >>> d.set_sync(np.zeros(13, dtype=np.uint8))
        >>> d.llrs_max_out(1)          # one per frame symbol
        93

        """

    def llrs_max_out(self, n: int) -> int:
        """Max LLRs burst_demod_llrs() writes: the frame's length in bits.

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
        out: NDArray[np.complex64] | None = None,
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

        Same span and same normalisation as burst_demod_llrs(): the whole
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
        out : NDArray[np.complex64] | None
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
        >>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, frame_syms=93)
        >>> d.set_sync(np.zeros(13, dtype=np.uint8))
        >>> d.symbols_max_out(1)       # one per frame symbol, as llrs()
        93

        """

    def symbols_max_out(self, n: int) -> int:
        """Max symbols burst_demod_symbols() writes: the frame's length.

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
        >>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, frame_syms=93)
        >>> d.set_prior(0.012, start=0)   # coarse Doppler + start, from acq

        """

    # jm:hand
    def demod(
        self, x: NDArray[np.complex64], out: NDArray[np.uint8] | None = ...
    ) -> NDArray[np.uint8]:
        """Demodulate a burst (preamble + frame); return the payload bits. Read-back properties report the estimates + CRC validity.

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
    def est_snr_db(self) -> float:
        """estimator confidence (dB)."""

    @property
    def frame_syms(self) -> int:
        """symbols the frame occupies AFTER the sync word — a number the caller
        states. What they MEAN is the frame description's business, one layer
        up.
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
class BurstCapture:
    """Create a burst capture: acquisition, refine and retention behind one
    push().

    Parameters
    ----------
    acq_code : NDArray[np.uint8]
        Preamble PN chips (0/1), length acq_code_len.
    burst_len : int, default 8192
        Samples in one burst -- what gets captured.
    reps : int, default 5
        Preamble code repetitions.
    spc : int, default 4
        Samples per chip.
    chip_rate : float, default 1000000.0
        Chip rate, Hz.
    cn0_dbhz : float, default 0.0
        C/N0 the search is sized for, dB-Hz.
    doppler_uncertainty : float, default 0.0
        Doppler search half-range, Hz (0 = native).
    pfa : float, default 1e-3
        Target false-alarm probability, in (0, 1).
    pd : float, default 0.9
        Target detection probability, in (0, 1).
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        CFAR reference: 0=mean, 1=median, 2=min, 3=max.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``BurstCapture: invalid
        parameter (need non-empty acq_code, reps >= 1, spc >= 1, chip_rate > 0,
        burst_len >= 1, cn0_dbhz >= 0, 0 < pfa < 1, 0 < pd < 1)``.

    Warns
    -----
    UserWarning
        Emitted after construction when ``underpowered`` holds: ``BurstCapture:
        the search cannot meet the requested pd at this cn0_dbhz and geometry
        (pd_predicted < pd). It still builds a best-effort grid, so the symptom
        is bursts that are never captured rather than an error. Lower pd, raise
        cn0_dbhz, or give the preamble more repetitions.``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.dsss import BurstCapture
    >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
    >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
    >>> cap.burst_len
    512
    >>> cap.retain_span == cap.refine_span + cap.burst_len
    True

    """
    def __init__(
        self,
        acq_code: NDArray[np.uint8],
        burst_len: int = ...,
        reps: int = ...,
        spc: int = ...,
        chip_rate: float = ...,
        cn0_dbhz: float = ...,
        doppler_uncertainty: float = ...,
        pfa: float = ...,
        pd: float = ...,
        noise_mode: Literal["mean", "median", "min", "max"] = "mean",
    ) -> None: ...

    def push(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Stream raw cf32 samples and get back the SAMPLES of every burst
        whose window has fully arrived, concatenated: burst i occupies
        burst_len samples starting at i*burst_len, and events() returns the
        matching record for each. Samples feed the embedded BurstAcquisition
        and are retained in a history ring; when a detection fires, the refine
        stage correlates one code period at each preamble position to recover
        the exact preamble start -- the one quantity acquisition structurally
        cannot report, since its code_phase is a lag modulo one code period --
        and the window is emitted the moment its last sample has arrived. It
        stops there: what to DO with a burst (demodulate it, write it to a
        file, ship it to another process) is the caller's. An empty return is
        normal, not an error: it means no burst completed in this call. Accepts
        any block size -- the history ring is a contiguous window over the
        stream and is never reset between bursts, so a burst whose tail falls
        outside one call is completed by a later one.

        Windows are concatenated: burst `i` occupies `burst_len` samples
        starting at `i*burst_len`, and events() returns the matching record for
        each. Every sample of x is consumed. An empty return is normal -- it
        means no burst completed in this call.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input samples, x_len long.
        out : NDArray[np.complex64] | None
            Written with the completed windows; may be NULL to drop.

        Returns
        -------
        NDArray[np.complex64]
            Samples written -- always a multiple of `burst_len`.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
        >>> win = cap.push(np.zeros(4096, dtype=np.complex64))
        >>> win.size % cap.burst_len        # whole windows, never a partial
        0
        >>> win.size                        # silence, so no burst completed
        0

        """

    def push_max_out(self, x_len: int) -> int:
        """Upper bound on samples push() can return for x_len input.

        Distinct bursts cannot overlap, so `x_len` samples complete at most

        `x_len/burst_len + 1` of them, plus whatever is already queued.

        Parameters
        ----------
        x_len : int
            Input.

        Returns
        -------
        int
            Output.
        """

    def detections(
        self,
        count: int = 1,
        out: NDArray[Any] | None = None,
    ) -> NDArray[Any]:
        """Every hit the search made in the last push(), unfiltered — before
        the claim rule coalesced the several detections of one preamble, and
        before the suppression window dropped the ones inside a burst already
        captured. So several rows can name one burst and a row can be a false
        alarm; that is the point. Each carries the STREAM-ABSOLUTE code epoch,
        which acquisition's own `code_phase` is not (it is a lag modulo one
        code period), plus the folded Doppler, the C/N0 lower bound and the
        CFAR statistic that gated it. Read `events()` instead for the bursts
        that survived and whose windows arrived. Valid until the next push(),
        reset() or set_state().

        BEFORE the claim rule and the suppression window: several rows can name
        one preamble, and a row can be a false alarm. That is the point -- this
        is what acquisition FOUND, and `events()` is what survived. Valid until
        the next push(), reset() or set_state().

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : NDArray[Any] | None
            Optional pre-allocated output buffer. When given, the result is
            written into it and the returned array is a view of exactly the
            samples produced; when omitted, a fresh array is allocated.

        Returns
        -------
        NDArray[Any]
            Output.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
        >>> _ = cap.push(np.zeros(4096, dtype=np.complex64))
        >>> # what the search found, against what became a burst
        >>> len(cap.detections()) >= len(cap.events())
        True

        """

    def detections_max_out(self, n: int) -> int:
        """Raw detections available from the last push(). n is ignored.

        Parameters
        ----------
        n : int
            Input.

        Returns
        -------
        int
            Output.
        """

    def events(
        self,
        count: int = 1,
        out: NDArray[Any] | None = None,
    ) -> NDArray[Any]:
        """The event record for each burst the last push() returned. Row i
        describes the window at samples[i*burst_len ...] of that push. Valid
        until the next push(), reset() or set_state().

        Row `i` describes the window at `i*burst_len`. Valid until the next
        push(), reset() or set_state().

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : NDArray[Any] | None
            Optional pre-allocated output buffer. When given, the result is
            written into it and the returned array is a view of exactly the
            samples produced; when omitted, a fresh array is allocated.

        Returns
        -------
        NDArray[Any]
            Output.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
        >>> win = cap.push(np.zeros(4096, dtype=np.complex64))
        >>> len(cap.events()) == win.size // cap.burst_len
        True

        """

    def events_max_out(self, n: int) -> int:
        """Records available from the last push(). n is ignored.

        Parameters
        ----------
        n : int
            Input.

        Returns
        -------
        int
            Output.
        """

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the embedded BurstAcquisition's search grid directly, bypassing
        the auto-sizing -- the escape hatch for a caller who wants a specific
        (doppler_bins, n_noncoh). Forwards to the engine unchanged.

        The escape hatch for a caller who wants a specific (doppler_bins,
        n_noncoh). Forwards to the engine, with one refusal of this object's
        own: a grid whose anchor can lag the preamble by more than refine
        reaches -- `n_noncoh * doppler_bins` code periods against `k_lo` -- is
        rejected rather than accepted and silently mis-refined. Acquisition
        stamps a hit at the end of the LAST accumulated look, so every look
        past the one holding the preamble moves the anchor a whole frame later;
        a burst has one frame of preamble, so `n_noncoh = 1` is the grid a
        capture wants and the sizer now always picks (doppler#1181).

        Parameters
        ----------
        doppler_bins : int
            Input.
        n_noncoh : int
            Input.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``configure_search_raw failed``, with the return code appended
            (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
        >>> cap.configure_search_raw(4, 1)   # 4 Doppler bins, coherent only

        """

    def release(self, i: int) -> None:
        """Give back the span window `i` of the last push() claimed. An emitted
        window owns its whole span: detections inside it are the payload firing
        against the acquisition code, so they are HELD rather than reported. A
        consumer that knows better -- a demodulator whose CRC failed -- calls
        this for that window, and the held detections are searched again on the
        next push(). Unreleased, they are dropped when the next push() begins.
        Raises ValueError if `i` is not a window of the last push().

        An emitted window owns its whole span: a detection inside it is the
        payload firing against the acquisition code, not a new burst, so it is
        HELD rather than reported. Whether the window WAS a burst is a verdict
        this object cannot reach -- it stops at samples; error detection,
        whatever form the frame gives it, is the consumer's -- so a consumer
        that knows better calls this for that window, and the held detections
        are searched again on the next push(). Unreleased, they are dropped
        when the next push() begins, which is exactly the behaviour a consumer
        with no verdict always had.

        What it prevents (doppler#1181): a spurious window ending just after a
        real burst begins used to swallow that burst's first detections -- the
        receiver's own design says only a DECODED burst may own a span (§10.3,
        doppler#1004), and the capture underneath had been owning it on
        emission.

        Must be called BEFORE the next push(): `i` indexes THIS push's windows.

        Parameters
        ----------
        i : int
            Input.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``release failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
        >>> _ = cap.push(np.zeros(4096, dtype=np.complex64))
        >>> cap.release(0)   # no window 0 in a quiet push
        Traceback (most recent call last):
          ...
        ValueError: release failed (rc=-4)

        """

    def reset(self) -> None:
        """Return to the searching state: resets the embedded acquisition,
        drops the history ring's contents, clears every queued detection and
        every read-back, so a fresh stream cannot inherit the previous one's
        position. Construction parameters are untouched.

        Resets the embedded acquisition, rewinds the history ring, clears every
        queued detection and every read-back. Construction parameters are
        untouched; `dropped` deliberately survives, because a lost burst stays
        lost.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
        >>> cap.push(np.zeros(4096, dtype=np.complex64)).size
        0
        >>> cap.reset()
        >>> cap.pending
        0

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the BurstCapture has already been destroyed.

        Returns
        -------
        int
            Byte length of one serialized state blob.
        """

    def get_state(self) -> bytes:
        """Serialize this object's mutable state to bytes.

        Captures exactly the state that evolves as the object runs, so a blob
        taken now and restored later resumes from this point. Construction
        parameters are not included: restore into an object built the same way.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the BurstCapture has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()`
        before the blob is handed to the C core, and the core may reject it as
        well.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the BurstCapture has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def preamble_start(self) -> int:
        """Stream-absolute sample index the most recent window's preamble
        starts at. NEVER LATE: a window that began after the preamble has
        destroyed the burst, so the refine stage's obligation is to be
        early-or-exact and to say how early.
        """

    @property
    def doppler_hz_est(self) -> float:
        """Folded/signed coarse Doppler estimate of the most recent window, Hz.
        Acquisition's own bin, mapped through dp_fftfreq -- the ONE home for
        that fold, because a consumer seeded on the wrong side of it is off by
        the full search span.
        """

    @property
    def doppler_res_hz(self) -> float:
        """Acquisition's native Doppler bin width =
        chip_rate/(sf*coherent_bins), Hz. The width of doppler_hz_est: the
        estimate is that value +/- half of this.
        """

    @property
    def cn0_dbhz_est(self) -> float:
        """Estimated carrier-to-noise density of the most recent window
        (dB-Hz), backed out of the hit's test statistic. A LOWER BOUND: it
        tracks the true C/N0 while receiver noise dominates the CFAR estimate,
        then saturates at the code's own autocorrelation-sidelobe floor once
        the true C/N0 exceeds what this code and geometry can resolve -- a real
        ceiling, not a fault.
        """

    @property
    def refine_margin(self) -> float:
        """The refine stage's own confidence: the best rival code period's
        score over the winner's. The envelope is (reps-1)/reps when the right
        repetition wins, so compare against THAT and never a constant -- the
        floor rises with depth (0.55 at reps=2, 0.77 at 4, 0.94 at 16). Near 1
        means the period was not resolved, which nothing else in the chain can
        see.
        """

    @property
    def burst_len(self) -> int:
        """Samples in one emitted window -- the burst length this capture was
        built for, and the stride of a row in push()'s return.
        """

    @property
    def refine_span(self) -> int:
        """Coalescing window, in samples -- the reach over which two detections
        are ONE preamble. Both sides of that test are burst STARTS (resolved
        code epochs), so this bounds start-to-start separation, NOT the dead
        air between bursts. The two differ by a whole burst, and reading it as
        dead air costs a caller real airtime for nothing: the gap actually
        required is `max(0, refine_span - burst_len)`, which is 0 whenever a
        burst is longer than the refine reach (doppler#1085).
        """

    @property
    def min_gap(self) -> int:
        """Dead air to leave BETWEEN bursts, in samples — edge to edge, not
        start to start. Derived rather than documented as a rule the caller has
        to apply: a detection's anchor is the code epoch of whichever frame
        detected, and acquisition's framing is not aligned to the preamble, so
        the last frame that can detect sits up to `reps * code_period` past the
        true start. CLAIM merges two anchors closer than `refine_span`, so a
        pair survives only when `gap >= refine_span + reps*code_period -
        burst_len`. **Zero is a real answer** — a burst longer than
        `refine_span + reps*P` needs no gap for the claim rule's sake — but it
        does not mean zero is wise: a zero gap is a continuous stream rather
        than a burst link, and it measures 88% at a geometry where this reads
        0. Replaces the prose `max(0, refine_span - burst_len)`, which was
        short by the whole detection-lag term: 32 samples against 528 at the C
        suite's geometry (doppler#1172).
        """

    @property
    def retain_span(self) -> int:
        """History kept per anchor, in samples -- the MINIMUM TRAILING CONTEXT.
        `refine_span` plus one whole burst. A burst closer than this to the end
        of what has been pushed is held rather than emitted, because refine
        cannot yet see the samples it needs. Feed at least this many more, or
        the last burst of a capture never comes out.
        """

    @property
    def underpowered(self) -> bool:
        """True when the search cannot meet the requested `pd` at this
        `cn0_dbhz` and geometry — `pd_predicted < pd`. The grid is still built,
        best-effort, so the symptom is bursts that are never captured rather
        than a failure. Construction also emits a UserWarning; this is the same
        fact as a value, for a caller that would rather ask than catch.
        """

    @property
    def pd_predicted(self) -> float:
        """Detection probability the sized grid actually predicts at
        `cn0_dbhz`. The number behind `underpowered`, and the one to compare
        against the `pd` that was asked for.
        """

    @property
    def eta(self) -> float:
        """Coherent detection gate: the normalised statistic a single-look
        decision must clear, from `pfa` spread across the search surface. In
        force when `n_noncoh == 1`.
        """

    @property
    def eta_nc(self) -> float:
        """Non-coherent detection gate — the one in force when `n_noncoh > 1`,
        which is the usual case. Higher than `eta` for the same `pfa`, because
        combining looks costs the threshold what it buys in sensitivity.
        """

    @property
    def straddle_loss(self) -> float:
        """Correlation kept, worst case, by a burst landing BETWEEN grid points
        rather than on one. The search is a finite grid in Doppler and code
        phase, so a real burst almost never sits on a hypothesis exactly; this
        is what that costs, and it is already priced into `pd_predicted`.
        """

    @property
    def doppler_bins(self) -> int:
        """Doppler hypotheses searched — the coherent depth the sizer chose,
        bounded by `reps`. `configure_search_raw` is what pins it.
        """

    @property
    def n_noncoh(self) -> int:
        """Non-coherent looks combined per decision. Above 1 the object needs
        that many frames before it can decide at all, which is why a caller
        sweeping in short dwells has to pin it.
        """

    @property
    def code_bins(self) -> int:
        """Code-phase hypotheses per Doppler row: one segment in samples, `sf *
        spc`.
        """

    @property
    def doppler_span_hz(self) -> float:
        """Unambiguous Doppler half-range, ± this. Beyond it the per-segment
        integrate-and-dump's sinc rolloff suppresses the correlation, so a
        burst outside the span is not merely harder to find — it is nulled.
        """

    @property
    def pending(self) -> int:
        """Detections held because their burst window has NOT fully arrived.
        push() deliberately emits nothing for these: a window is returned when
        it is complete, not when it is guessed at. What this exists for is the
        other end -- a caller closing a file or a socket while this is non-zero
        is discarding a burst that would have been captured, and every other
        read-back looks identical to "nothing was ever there".
        """

    @property
    def dropped(self) -> int:
        """Samples the history ring refused, lifetime. A LOST BURST each, not a
        statistic -- it survives reset().
        """

    @property
    def n_bursts(self) -> int:
        """Windows emitted, lifetime."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "BurstCapture":
        """Enter a context manager, returning this object.

        Lets a BurstCapture be used in a `with` statement so its C resources
        are released deterministically on exit rather than at collection time.

        Returns
        -------
        BurstCapture
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the BurstCapture.

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
class PersistentBurstCapture:
    """Create a capture whose look-back lives in a FILE.

    Parameters
    ----------
    path : str | os.PathLike
        File to back the ring with; not NULL and not empty.
    acq_code : NDArray[np.uint8]
        Preamble PN chips (0/1), length acq_code_len.
    burst_len : int, default 8192
        Samples in one burst -- what gets captured.
    reps : int, default 5
        Preamble code repetitions.
    spc : int, default 4
        Samples per chip.
    chip_rate : float, default 1000000.0
        Chip rate, Hz.
    cn0_dbhz : float, default 0.0
        C/N0 the search is sized for, dB-Hz.
    doppler_uncertainty : float, default 0.0
        Doppler search half-range, Hz (0 = native).
    pfa : float, default 1e-3
        Target false-alarm probability, in (0, 1).
    pd : float, default 0.9
        Target detection probability, in (0, 1).
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        CFAR reference: 0=mean, 1=median, 2=min, 3=max.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``BurstCapture: invalid
        parameter (need non-empty acq_code, reps >= 1, spc >= 1, chip_rate > 0,
        burst_len >= 1, cn0_dbhz >= 0, 0 < pfa < 1, 0 < pd < 1)``.

    Warns
    -----
    UserWarning
        Emitted after construction when ``underpowered`` holds: ``BurstCapture:
        the search cannot meet the requested pd at this cn0_dbhz and geometry
        (pd_predicted < pd). It still builds a best-effort grid, so the symptom
        is bursts that are never captured rather than an error. Lower pd, raise
        cn0_dbhz, or give the preamble more repetitions.``.

    Examples
    --------
    >>> import numpy as np, tempfile, os
    >>> from doppler.dsss import BurstCapture, PersistentBurstCapture
    >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
    >>> path = os.path.join(tempfile.mkdtemp(), "ring.cf32")
    >>> cap = PersistentBurstCapture(path, code, burst_len=512,
    ...                             reps=4, spc=2)
    >>> ram = BurstCapture(code, burst_len=512, reps=4, spc=2)
    >>> _ = cap.push(np.zeros(4096, dtype=np.complex64))
    >>> # the look-back is in the file, so the blob stops carrying it
    >>> ram.state_bytes() - cap.state_bytes() == ram.retain_span * 8
    True
    >>> os.path.getsize(path) > 0
    True

    """
    def __init__(
        self,
        path: str | os.PathLike,
        acq_code: NDArray[np.uint8],
        burst_len: int = ...,
        reps: int = ...,
        spc: int = ...,
        chip_rate: float = ...,
        cn0_dbhz: float = ...,
        doppler_uncertainty: float = ...,
        pfa: float = ...,
        pd: float = ...,
        noise_mode: Literal["mean", "median", "min", "max"] = "mean",
    ) -> None: ...

    def push(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Stream raw cf32 samples and get back the SAMPLES of every burst
        whose window has fully arrived, concatenated: burst i occupies
        burst_len samples starting at i*burst_len, and events() returns the
        matching record for each. Samples feed the embedded BurstAcquisition
        and are retained in a history ring; when a detection fires, the refine
        stage correlates one code period at each preamble position to recover
        the exact preamble start -- the one quantity acquisition structurally
        cannot report, since its code_phase is a lag modulo one code period --
        and the window is emitted the moment its last sample has arrived. It
        stops there: what to DO with a burst (demodulate it, write it to a
        file, ship it to another process) is the caller's. An empty return is
        normal, not an error: it means no burst completed in this call. Accepts
        any block size -- the history ring is a contiguous window over the
        stream and is never reset between bursts, so a burst whose tail falls
        outside one call is completed by a later one.

        Windows are concatenated: burst `i` occupies `burst_len` samples
        starting at `i*burst_len`, and events() returns the matching record for
        each. Every sample of x is consumed. An empty return is normal -- it
        means no burst completed in this call.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input samples, x_len long.
        out : NDArray[np.complex64] | None
            Written with the completed windows; may be NULL to drop.

        Returns
        -------
        NDArray[np.complex64]
            Samples written -- always a multiple of `burst_len`.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
        >>> win = cap.push(np.zeros(4096, dtype=np.complex64))
        >>> win.size % cap.burst_len        # whole windows, never a partial
        0
        >>> win.size                        # silence, so no burst completed
        0

        """

    def push_max_out(self, x_len: int) -> int:
        """Upper bound on samples push() can return for x_len input.

        Distinct bursts cannot overlap, so `x_len` samples complete at most

        `x_len/burst_len + 1` of them, plus whatever is already queued.

        Parameters
        ----------
        x_len : int
            Input.

        Returns
        -------
        int
            Output.
        """

    def detections(
        self,
        count: int = 1,
        out: NDArray[Any] | None = None,
    ) -> NDArray[Any]:
        """Every hit the search made in the last push(), unfiltered — before
        the claim rule coalesced the several detections of one preamble, and
        before the suppression window dropped the ones inside a burst already
        captured. So several rows can name one burst and a row can be a false
        alarm; that is the point. Each carries the STREAM-ABSOLUTE code epoch,
        which acquisition's own `code_phase` is not (it is a lag modulo one
        code period), plus the folded Doppler, the C/N0 lower bound and the
        CFAR statistic that gated it. Read `events()` instead for the bursts
        that survived and whose windows arrived. Valid until the next push(),
        reset() or set_state().

        BEFORE the claim rule and the suppression window: several rows can name
        one preamble, and a row can be a false alarm. That is the point -- this
        is what acquisition FOUND, and `events()` is what survived. Valid until
        the next push(), reset() or set_state().

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : NDArray[Any] | None
            Optional pre-allocated output buffer. When given, the result is
            written into it and the returned array is a view of exactly the
            samples produced; when omitted, a fresh array is allocated.

        Returns
        -------
        NDArray[Any]
            Output.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
        >>> _ = cap.push(np.zeros(4096, dtype=np.complex64))
        >>> # what the search found, against what became a burst
        >>> len(cap.detections()) >= len(cap.events())
        True

        """

    def detections_max_out(self, n: int) -> int:
        """Raw detections available from the last push(). n is ignored.

        Parameters
        ----------
        n : int
            Input.

        Returns
        -------
        int
            Output.
        """

    def events(
        self,
        count: int = 1,
        out: NDArray[Any] | None = None,
    ) -> NDArray[Any]:
        """The event record for each burst the last push() returned. Row i
        describes the window at samples[i*burst_len ...] of that push. Valid
        until the next push(), reset() or set_state().

        Row `i` describes the window at `i*burst_len`. Valid until the next
        push(), reset() or set_state().

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : NDArray[Any] | None
            Optional pre-allocated output buffer. When given, the result is
            written into it and the returned array is a view of exactly the
            samples produced; when omitted, a fresh array is allocated.

        Returns
        -------
        NDArray[Any]
            Output.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
        >>> win = cap.push(np.zeros(4096, dtype=np.complex64))
        >>> len(cap.events()) == win.size // cap.burst_len
        True

        """

    def events_max_out(self, n: int) -> int:
        """Records available from the last push(). n is ignored.

        Parameters
        ----------
        n : int
            Input.

        Returns
        -------
        int
            Output.
        """

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the embedded BurstAcquisition's search grid directly, bypassing
        the auto-sizing -- the escape hatch for a caller who wants a specific
        (doppler_bins, n_noncoh). Forwards to the engine unchanged.

        The escape hatch for a caller who wants a specific (doppler_bins,
        n_noncoh). Forwards to the engine, with one refusal of this object's
        own: a grid whose anchor can lag the preamble by more than refine
        reaches -- `n_noncoh * doppler_bins` code periods against `k_lo` -- is
        rejected rather than accepted and silently mis-refined. Acquisition
        stamps a hit at the end of the LAST accumulated look, so every look
        past the one holding the preamble moves the anchor a whole frame later;
        a burst has one frame of preamble, so `n_noncoh = 1` is the grid a
        capture wants and the sizer now always picks (doppler#1181).

        Parameters
        ----------
        doppler_bins : int
            Input.
        n_noncoh : int
            Input.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``configure_search_raw failed``, with the return code appended
            (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
        >>> cap.configure_search_raw(4, 1)   # 4 Doppler bins, coherent only

        """

    def release(self, i: int) -> None:
        """Give back the span window `i` of the last push() claimed. An emitted
        window owns its whole span: detections inside it are the payload firing
        against the acquisition code, so they are HELD rather than reported. A
        consumer that knows better -- a demodulator whose CRC failed -- calls
        this for that window, and the held detections are searched again on the
        next push(). Unreleased, they are dropped when the next push() begins.
        Raises ValueError if `i` is not a window of the last push().

        An emitted window owns its whole span: a detection inside it is the
        payload firing against the acquisition code, not a new burst, so it is
        HELD rather than reported. Whether the window WAS a burst is a verdict
        this object cannot reach -- it stops at samples; error detection,
        whatever form the frame gives it, is the consumer's -- so a consumer
        that knows better calls this for that window, and the held detections
        are searched again on the next push(). Unreleased, they are dropped
        when the next push() begins, which is exactly the behaviour a consumer
        with no verdict always had.

        What it prevents (doppler#1181): a spurious window ending just after a
        real burst begins used to swallow that burst's first detections -- the
        receiver's own design says only a DECODED burst may own a span (§10.3,
        doppler#1004), and the capture underneath had been owning it on
        emission.

        Must be called BEFORE the next push(): `i` indexes THIS push's windows.

        Parameters
        ----------
        i : int
            Input.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``release failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
        >>> _ = cap.push(np.zeros(4096, dtype=np.complex64))
        >>> cap.release(0)   # no window 0 in a quiet push
        Traceback (most recent call last):
          ...
        ValueError: release failed (rc=-4)

        """

    def reset(self) -> None:
        """Return to the searching state: resets the embedded acquisition,
        drops the history ring's contents, clears every queued detection and
        every read-back, so a fresh stream cannot inherit the previous one's
        position. Construction parameters are untouched.

        Resets the embedded acquisition, rewinds the history ring, clears every
        queued detection and every read-back. Construction parameters are
        untouched; `dropped` deliberately survives, because a lost burst stays
        lost.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> cap = BurstCapture(code, burst_len=512, reps=4, spc=2)
        >>> cap.push(np.zeros(4096, dtype=np.complex64)).size
        0
        >>> cap.reset()
        >>> cap.pending
        0

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the PersistentBurstCapture has already been
        destroyed.

        Returns
        -------
        int
            Byte length of one serialized state blob.
        """

    def get_state(self) -> bytes:
        """Serialize this object's mutable state to bytes.

        Captures exactly the state that evolves as the object runs, so a blob
        taken now and restored later resumes from this point. Construction
        parameters are not included: restore into an object built the same way.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the PersistentBurstCapture has already been
        destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()`
        before the blob is handed to the C core, and the core may reject it as
        well.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the PersistentBurstCapture has already been
        destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def preamble_start(self) -> int:
        """Stream-absolute sample index the most recent window's preamble
        starts at. NEVER LATE: a window that began after the preamble has
        destroyed the burst, so the refine stage's obligation is to be
        early-or-exact and to say how early.
        """

    @property
    def doppler_hz_est(self) -> float:
        """Folded/signed coarse Doppler estimate of the most recent window, Hz.
        Acquisition's own bin, mapped through dp_fftfreq -- the ONE home for
        that fold, because a consumer seeded on the wrong side of it is off by
        the full search span.
        """

    @property
    def doppler_res_hz(self) -> float:
        """Acquisition's native Doppler bin width =
        chip_rate/(sf*coherent_bins), Hz. The width of doppler_hz_est: the
        estimate is that value +/- half of this.
        """

    @property
    def cn0_dbhz_est(self) -> float:
        """Estimated carrier-to-noise density of the most recent window
        (dB-Hz), backed out of the hit's test statistic. A LOWER BOUND: it
        tracks the true C/N0 while receiver noise dominates the CFAR estimate,
        then saturates at the code's own autocorrelation-sidelobe floor once
        the true C/N0 exceeds what this code and geometry can resolve -- a real
        ceiling, not a fault.
        """

    @property
    def refine_margin(self) -> float:
        """The refine stage's own confidence: the best rival code period's
        score over the winner's. The envelope is (reps-1)/reps when the right
        repetition wins, so compare against THAT and never a constant -- the
        floor rises with depth (0.55 at reps=2, 0.77 at 4, 0.94 at 16). Near 1
        means the period was not resolved, which nothing else in the chain can
        see.
        """

    @property
    def burst_len(self) -> int:
        """Samples in one emitted window -- the burst length this capture was
        built for, and the stride of a row in push()'s return.
        """

    @property
    def refine_span(self) -> int:
        """Coalescing window, in samples -- the reach over which two detections
        are ONE preamble. Both sides of that test are burst STARTS (resolved
        code epochs), so this bounds start-to-start separation, NOT the dead
        air between bursts. The two differ by a whole burst, and reading it as
        dead air costs a caller real airtime for nothing: the gap actually
        required is `max(0, refine_span - burst_len)`, which is 0 whenever a
        burst is longer than the refine reach (doppler#1085).
        """

    @property
    def min_gap(self) -> int:
        """Dead air to leave BETWEEN bursts, in samples — edge to edge, not
        start to start. Derived rather than documented as a rule the caller has
        to apply: a detection's anchor is the code epoch of whichever frame
        detected, and acquisition's framing is not aligned to the preamble, so
        the last frame that can detect sits up to `reps * code_period` past the
        true start. CLAIM merges two anchors closer than `refine_span`, so a
        pair survives only when `gap >= refine_span + reps*code_period -
        burst_len`. **Zero is a real answer** — a burst longer than
        `refine_span + reps*P` needs no gap for the claim rule's sake — but it
        does not mean zero is wise: a zero gap is a continuous stream rather
        than a burst link, and it measures 88% at a geometry where this reads
        0. Replaces the prose `max(0, refine_span - burst_len)`, which was
        short by the whole detection-lag term: 32 samples against 528 at the C
        suite's geometry (doppler#1172).
        """

    @property
    def retain_span(self) -> int:
        """History kept per anchor, in samples -- the MINIMUM TRAILING CONTEXT.
        `refine_span` plus one whole burst. A burst closer than this to the end
        of what has been pushed is held rather than emitted, because refine
        cannot yet see the samples it needs. Feed at least this many more, or
        the last burst of a capture never comes out.
        """

    @property
    def underpowered(self) -> bool:
        """True when the search cannot meet the requested `pd` at this
        `cn0_dbhz` and geometry — `pd_predicted < pd`. The grid is still built,
        best-effort, so the symptom is bursts that are never captured rather
        than a failure. Construction also emits a UserWarning; this is the same
        fact as a value, for a caller that would rather ask than catch.
        """

    @property
    def pd_predicted(self) -> float:
        """Detection probability the sized grid actually predicts at
        `cn0_dbhz`. The number behind `underpowered`, and the one to compare
        against the `pd` that was asked for.
        """

    @property
    def eta(self) -> float:
        """Coherent detection gate: the normalised statistic a single-look
        decision must clear, from `pfa` spread across the search surface. In
        force when `n_noncoh == 1`.
        """

    @property
    def eta_nc(self) -> float:
        """Non-coherent detection gate — the one in force when `n_noncoh > 1`,
        which is the usual case. Higher than `eta` for the same `pfa`, because
        combining looks costs the threshold what it buys in sensitivity.
        """

    @property
    def straddle_loss(self) -> float:
        """Correlation kept, worst case, by a burst landing BETWEEN grid points
        rather than on one. The search is a finite grid in Doppler and code
        phase, so a real burst almost never sits on a hypothesis exactly; this
        is what that costs, and it is already priced into `pd_predicted`.
        """

    @property
    def doppler_bins(self) -> int:
        """Doppler hypotheses searched — the coherent depth the sizer chose,
        bounded by `reps`. `configure_search_raw` is what pins it.
        """

    @property
    def n_noncoh(self) -> int:
        """Non-coherent looks combined per decision. Above 1 the object needs
        that many frames before it can decide at all, which is why a caller
        sweeping in short dwells has to pin it.
        """

    @property
    def code_bins(self) -> int:
        """Code-phase hypotheses per Doppler row: one segment in samples, `sf *
        spc`.
        """

    @property
    def doppler_span_hz(self) -> float:
        """Unambiguous Doppler half-range, ± this. Beyond it the per-segment
        integrate-and-dump's sinc rolloff suppresses the correlation, so a
        burst outside the span is not merely harder to find — it is nulled.
        """

    @property
    def pending(self) -> int:
        """Detections held because their burst window has NOT fully arrived.
        push() deliberately emits nothing for these: a window is returned when
        it is complete, not when it is guessed at. What this exists for is the
        other end -- a caller closing a file or a socket while this is non-zero
        is discarding a burst that would have been captured, and every other
        read-back looks identical to "nothing was ever there".
        """

    @property
    def dropped(self) -> int:
        """Samples the history ring refused, lifetime. A LOST BURST each, not a
        statistic -- it survives reset().
        """

    @property
    def n_bursts(self) -> int:
        """Windows emitted, lifetime."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "PersistentBurstCapture":
        """Enter a context manager, returning this object.

        Lets a PersistentBurstCapture be used in a `with` statement so its C
        resources are released deterministically on exit rather than at
        collection time.

        Returns
        -------
        PersistentBurstCapture
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the PersistentBurstCapture.

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
        `acq_create_continuous()`).
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
        Dll's own non-coherent partial-correlation count per code epoch — its
        tracking- robustness parameter, independent of `sps` (see the module
        docstring); default 4, this story's own validated sweet spot.
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
        parameter (need a non-empty code, chip_rate > 0, symbol_rate > 0, spc
        >= 1, m in {2,4,8}, segments >= 1, sps >= 2 -- sps = 1 cannot carry an
        m_out, whose smallest legal value is 2 and which MpskReceiver requires
        sps to reach)``.

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
        code: NDArray[np.uint8],
        chip_rate: float = ...,
        symbol_rate: float = ...,
        spc: int = ...,
        m: int = ...,
        cn0_dbhz: float = ...,
        pfa: float = ...,
        pd: float = ...,
        doppler_uncertainty: float = ...,
        segments: int = ...,
        sps: int = ...,
        differential: int = ...,
    ) -> None: ...

    def steps(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
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
        the unconsumed tail of THIS call — computed exactly from
        `acq->samples_consumed`, no samples dropped or double-fed — is handed
        straight to them in the same call. While tracking, samples feed `Dll ->
        RateConverter -> MpskReceiver` in sequence. Accepts any block size;
        state carries across calls (`Acquisition`/`Dll`/
        `RateConverter`/`MpskReceiver` are all already block-size invariant, so
        this object needs no ring-buffering of its own).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input cf32 samples.
        out : NDArray[np.complex64] | None
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
        a failed pin leaves the receiver on its prior grid.

        The escape hatch for the one composition-specific knob this object adds
        beyond its children's own: `segments` (Dll's tracking parameter) and
        `sps`/`n` (MpskReceiver's sample-rate/carrier-arm parameters) are
        indepen­dently overridable here, still bridged by a freshly-sized
        `RateConverter` — never coupled to each other (see the module
        docstring). Rebuilds `dll`/`rc`/`rx` with every replacement allocated
        first, only freeing and adopting the old ones once every allocation has
        succeeded (mirrors `Acquisition`'s own `acq_regrid()` discipline) — a
        failed pin leaves the receiver tracking on its prior grid, not
        half-destroyed. Only meaningful once tracking (the grid defaults still
        apply to create-time auto-sizing for the next hit while searching; call
        `dsss_receiver_create()` with different `segments`/`sps` for that, or
        re-pin here again after the next hit).

        Parameters
        ----------
        segments : int
            Dll tracking segments per code period.
        sps : int
            MpskReceiver samples per symbol (the resample target).
        n : int
            MpskReceiver's carrier-arm count; must divide sps.

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

        Captures exactly the state that evolves as the object runs, so a blob
        taken now and restored later resumes from this point. Construction
        parameters are not included: restore into an object built the same way.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the DsssReceiver has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()`
        before the blob is handed to the C core, and the core may reject it as
        well.

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
        Chip rate, Hz. Required.
    symbol_rate : float, default 1000.0
        Data-symbol rate, Hz. Required.
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
        MpskReceiver's differential demap; default 0 (coherent).
    refine_max_error_db : float, default 0.5
        Max tolerable async-lookback correlation-power loss driving the
        refine-stage collection Dll's coherent-I&D window count via
        dll_lookback_segments(). Oversampling the epoch is required for the
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
        CarrierAcquisition's own sequential mode; default false -- sequential
        mode's early per-block test fires on far too little averaging at SPEC's
        own Es/N0 floor (confirmed: as few as 4 blocks, 150-200+ Hz off); false
        waits the full design_snr-derived dwell_target, matching
        freq_refine.refine_seed_ carrier_acq()'s own validated default.
    refine_max_n_blocks : int, default 100000
        CarrierAcquisition's own give-up cap in sequential mode; default
        100000.
    carrier_freq_hz : float, default 0.0
        Nominal RF carrier frequency, Hz, enabling carrier->code aiding; 0.0
        (default) = off. When > 0, the coupled code-rate Doppler
        (carrier_offset/carrier_freq) is fed to the tracking Dll via
        dll_set_rate_aid() so the code loop rides a dilated clock the
        discriminator alone can't pull in at low SNR. Set to the receiver's own
        downlink RF frequency for a physically-coupled Doppler capture.
    lost_confirm_s : float, default 0.0
        Release rule: both lock flags down, continuously, for longer than this
        many seconds puts the receiver in the lost state (see get_lost()). Size
        it past the longest fade the link must ride. The clock also runs from
        the first tracking sample, when neither flag is up yet, so a hand-off
        that never locks within the interval is released the same way as an
        emitter that leaves. Default 0.0 = never -- the searching flavor's exit
        is reset(), as before.

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
    def __init__(
        self,
        code: NDArray[np.uint8],
        chip_rate: float = ...,
        symbol_rate: float = ...,
        spc: int = ...,
        m: int = ...,
        cn0_dbhz: float = ...,
        pfa: float = ...,
        pd: float = ...,
        doppler_uncertainty: float = ...,
        segments: int = ...,
        sps: int = ...,
        differential: int = ...,
        refine_max_error_db: float = ...,
        refine_samples_per_symbol: int = ...,
        refine_design_margin_db: float = ...,
        refine_n_fft: int = ...,
        refine_zero_pad: int = ...,
        refine_sequential: bool = ...,
        refine_max_n_blocks: int = ...,
        carrier_freq_hz: float = ...,
        lost_confirm_s: float = ...,
    ) -> None: ...

    def steps(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Stream raw cf32 samples through the receiver. While searching,
        samples feed the embedded Acquisition and nothing is emitted. On a hit,
        the refine stage (a frozen-carrier Dll collection feeding
        CarrierAcquisition) is built and seeded from it, and the unconsumed
        tail of this call is handed straight to it -- no samples dropped. Once
        CarrierAcquisition reports ready (or its own give-up cap is reached),
        the live tracking chain (Dll + per-partial Costas + RateConverter +
        MpskReceiver) is built fresh, seeded from the ORIGINAL handoff chip
        phase and the refined-or-unrefined Doppler estimate, and demodulated
        symbols are returned from then on. Accepts any block size; state
        carries across calls.

        Drives the search -> refine -> track state machine. While searching or
        refining, nothing is emitted (an empty return is normal, not an error):
        a hit seeds the frozen-carrier refine chain, `CarrierAcquisition`
        sharpens the coarse Doppler estimate, and only once it is ready (or
        gives up) is the live tracking chain built and demodulation begins.
        Accepts any block size; state carries across calls, so a capture can be
        fed in frames of any length with no seam. Idle (hand-off mode, before a
        seed) and lost (after the release rule fires) consume the samples and
        emit nothing, so the feeding loop is the same in every state; while
        tracking, the release clock runs on the two lock flags after every call
        (see `lost_confirm_s`). Under SPEC's coupled offset + 500 Hz/s Doppler
        ramp the pre-despread Costas removes the full carrier dynamics before
        the code loop, so the recovered constellation lands cleanly on the BPSK
        real axis.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input cf32 samples.
        out : NDArray[np.complex64] | None
            Output symbols; caller provides max_out capacity.

        Returns
        -------
        NDArray[np.complex64]
            Number of symbols written (0 while searching/refining, or while
            tracking with not yet a full symbol's worth of input).

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

    def seed(
        self,
        chip_phase: float,
        doppler_hz_est: float,
        cn0_dbhz_est: float,
    ) -> None:
        """Take a detection from outside and start refining from it: the hit's
        chip phase (Dll's instantaneous convention, at the next sample fed),
        coarse Doppler estimate and C/N0 estimate -- exactly what the searching
        flavor's own hit produces. Accepted while idle (hand-off flavor) or
        searching; refused on a receiver that already holds a seed (refining,
        tracking or lost -- reset() releases it) and for a chip_phase outside
        [0, code_len).

        The hand-off of docs/design/async-dsss-receiver.md section 11.1: the
        three numbers a searcher's hit carries that this receiver uses --
        `acq_handoff_t`'s `chip_phase`, `doppler_hz_est` and `cn0_dbhz_est` --
        exactly as its own hit would have produced them (the searching flavor's
        `steps()` calls this on its own hit). `chip_phase` is the code's
        instantaneous phase in chips, Dll's convention, at the FIRST sample of
        the next `steps()` call; the Python-side conversion from a lag is
        `doppler.dsss.handoff`. The refine chain is rebuilt from the seed and
        the state becomes refining; the unconsumed tail is the caller's to
        feed.

        Refused (`DP_ERR_INVALID`, nothing changes) on a receiver that is not
        waiting for one -- refining, tracking or lost -- because "assigned
        once" is a property of the object, not of the caller's bookkeeping;
        `reset()` releases it. Accepted while idle (hand-off mode) or searching
        (the searching flavor: an outside hit simply beats its own). Also
        refused for a `chip_phase` outside `[0, code_len)` or a non-finite
        value.

        Parameters
        ----------
        chip_phase : float
            Code phase at the next sample, chips, in `[0, code_len)`.
        doppler_hz_est : float
            Coarse Doppler estimate, Hz (the refine stage sharpens it).
        cn0_dbhz_est : float
            The hit's C/N0 estimate, dB-Hz; reported back by get_cn0_dbhz_est()
            until tracking refreshes it.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``seed refused: the receiver already holds an assignment (refining,
            tracking or lost -- reset() releases it), or chip_phase is outside
            [0, code_len)``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import HandoffAsyncDsssReceiver
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> rx = HandoffAsyncDsssReceiver(code, chip_rate=3.069e6,
        ...                               symbol_rate=2700.0, spc=2)
        >>> rx.seed(chip_phase=512.25, doppler_hz_est=-1500.0,
        ...         cn0_dbhz_est=48.0)
        >>> (rx.idle, rx.refining, rx.doppler_hz, rx.cn0_dbhz_est)
        (0, 1, -1500.0, 48.0)

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
        """One consistent picture of the receiver, by value (design section
        11.3): state, where the emitter is now (live Doppler, chip phase, code
        rate, C/N0), both lock flags with the symbol-lock metric and threshold,
        both residual carrier errors, and the two clocks in input samples
        (since the state was entered; both flags down without a break). Read on
        demand by the holder of a pool -- the one-at-a-time properties are the
        same fields' other face. No timestamp: the holder owns the sample clock
        and stamps it.

        Cheap and allocation-free: every field is a read of live state. The
        one-at-a-time getters below report the same fields; this is the face a
        pool holder uses.

        Returns
        -------
        ReceiverStatus
            The record, by value.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import HandoffAsyncDsssReceiver
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> rx = HandoffAsyncDsssReceiver(code, chip_rate=3.069e6,
        ...                               symbol_rate=2700.0, spc=2)
        >>> st = rx.status()
        >>> (st.state, st.doppler_hz, st.code_locked, st.locked)   # idle
        (3, 0.0, 0, 0)
        >>> rx.seed(chip_phase=100.0, doppler_hz_est=-250.0, cn0_dbhz_est=50.0)
        >>> st = rx.status()
        >>> (st.state, round(st.doppler_hz, 6), st.cn0_dbhz_est)  # refining
        (1, -250.0, 50.0)
        >>> _ = rx.steps(np.zeros(2046, np.complex64))
        >>> rx.status().state_samples                             # since seed
        2046

        """

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the embedded Acquisition's search grid directly, bypassing the
        symbol_rate-driven auto-sizing. Only meaningful while searching.

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
        >>> from doppler.dsss import AsyncDsssReceiver
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
        ...                        spc=2, doppler_uncertainty=500.0)
        >>> rx.configure_search_raw(doppler_bins=1, n_noncoh=16)  # pin it
        >>> rx.refining                # still searching, on the pinned grid
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
        """Re-tune the live-tracking Dll's code-lock detector directly. Only
        meaningful once tracking has begun; a no-op while searching or
        refining.

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
        >>> rx.tracking                       # a no-op until tracking begins
        0

        """

    def configure_chain_raw(self, segments: int, sps: int, n: int) -> None:
        """Pin the live-tracking despread/resample/demod grid directly,
        bypassing the create-time segments/sps defaults. Only meaningful once
        tracking; rebuilds the chain with every replacement allocated first, so
        a failed pin leaves the receiver on its prior grid.

        Parameters
        ----------
        segments : int
            Live-tracking Dll segments per code period.
        sps : int
            MpskReceiver samples per symbol (the resample target).
        n : int
            MpskReceiver's carrier-arm count; must divide sps.

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
        """Return to the searching state: resets the embedded Acquisition and
        frees every refine-stage/track-stage child (rebuilt from scratch on the
        next hit).

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

        Captures exactly the state that evolves as the object runs, so a blob
        taken now and restored later resumes from this point. Construction
        parameters are not included: restore into an object built the same way.

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

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()`
        before the blob is handed to the C core, and the core may reject it as
        well.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the AsyncDsssReceiver has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def tracking(self) -> int:
        """1 once the live tracking chain is built and demodulating; 0 while
        searching or refining.
        """

    @property
    def refining(self) -> int:
        """1 while the refine stage (CarrierAcquisition collection) is active;
        0 while searching or tracking.
        """

    @property
    def idle(self) -> int:
        """1 while waiting for a seed (the hand-off flavor before seed() or
        after reset()); 0 in every other state.
        """

    @property
    def lost(self) -> int:
        """1 once the release rule has fired: both lock flags were down,
        continuously, for longer than lost_confirm_s while tracking. The loops
        have stopped and samples are discarded; the holder releases the
        assignment and calls reset(). Always 0 with lost_confirm_s = 0.
        """

    @property
    def doppler_hz(self) -> float:
        """The current best Doppler estimate: the coarse handoff value while
        refining, the CarrierAcquisition-refined value once tracking.
        """

    @property
    def cn0_dbhz_est(self) -> float:
        """Cached from the winning acquisition hit."""

    @property
    def segments(self) -> int:
        """Live-tracking Dll's own segments -- distinct from refine_segments
        above (see the module docstring / dll_lookback_segments()'s own doc on
        the WINDOWS vs TRACK_WINDOWS split).
        """

    @property
    def sps(self) -> int:
        """MpskReceiver's own samples/symbol."""

    @property
    def n(self) -> int:
        """MpskReceiver's own carrier-arm count."""

    @property
    def chip_phase(self) -> float:
        """Live Dll code phase in chips, Dll's own instantaneous-phase
        convention (the mirror image of acq_result_t::code_phase's
        correlation-lag convention -- see acq_build_handoff()'s doc
        comment).
        """

    @property
    def code_rate(self) -> float:
        """Live Dll code rate: chips advanced per nominal chip (~1.0)."""

    @property
    def lock(self) -> float:
        """decision rule on lock_metric: thresholds + verify counters, stepped
        per symbol.
        """

    @property
    def norm_freq(self) -> float:
        """Smoothed carrier estimate (integrator only, cycles/sample of the
        MpskReceiver output rate); lags a Doppler ramp by the constant Type-II
        ramp error.
        """

    @property
    def nco_freq(self) -> float:
        """Live carrier loop-filter output = NCO frequency command
        (cycles/sample of the MpskReceiver output rate): its mean tracks a
        Doppler ramp with no lag, its variance is the carrier loop stress.
        """

    @property
    def locked(self) -> int:
        """Binary receiver lock: the hysteretic (up/down verify-counted) lock
        detector on the emitted symbols -- declared when lock_metric stays >=
        lock_threshold for the up-count and dropped below it for the
        down-count.
        """

    @property
    def lock_metric(self) -> float:
        """Symbol-lock metric: SNR-weighted running mean of the BPSK lock
        signal (I^2-Q^2)/(I^2+Q^2) = cos(2*phi) over the emitted symbols
        (locked -> ~+1). Drives `locked`; exposed for engineering debug.
        """

    @property
    def lock_threshold(self) -> float:
        """The lock_metric declare threshold `locked` latches above (the
        lockdet up_thresh); exposed alongside lock_metric for engineering
        debug.
        """

    @property
    def car_last_error(self) -> float:
        """Pre-despread Costas phase discriminator (rad): the residual carrier
        phase loop 1 (de-rotates before the Dll) is not nulling. Engineering
        debug.
        """

    @property
    def car_nco_freq(self) -> float:
        """Loop 1 (pre-despread Costas) loop-filter output = NCO frequency
        command, cycles/sample of the front-end (chip_rate*spc) rate.
        Engineering debug.
        """

    @property
    def mpsk_last_error(self) -> float:
        """MpskReceiver carrier phase discriminator (rad): the residual carrier
        phase loop 2 (post-despread) is not nulling. Engineering debug.
        """

    @property
    def code_locked(self) -> int:
        """Binary code-lock flag from the live tracking Dll's own
        verify-counted (pfa-tuned) lock detector -- the fundamental DSSS "am I
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
class HandoffAsyncDsssReceiver:
    """Create a receiver in hand-off mode: idle, with no search of its own.

    Parameters
    ----------
    code : NDArray[np.uint8]
        Spreading code, 0/1 chips (see async_dsss_receiver_create()).
    chip_rate : float, default 1000000.0
        Chip rate, Hz. Required.
    symbol_rate : float, default 1000.0
        Data-symbol rate, Hz. Required.
    spc : int, default 2
        Samples/chip; default 2.
    m : int, default 2
        PSK order, 2/4/8; default 2.
    cn0_dbhz : float, default 55.0
        Design C/N0, dB-Hz; default 55.0 (derated by `refine_design_margin_db`
        into CarrierAcquisition's design_snr).
    pfa : float, default 1e-3
        CarrierAcquisition's false-alarm target; default 1e-3.
    pd : float, default 0.9
        CarrierAcquisition's detection target; default 0.9.
    segments : int, default 4
        Live-tracking Dll's segments; default 4.
    sps : int, default 8
        MpskReceiver's samples/symbol; default 8.
    differential : int, default 0
        MpskReceiver's differential demap; default 0.
    refine_max_error_db : float, default 0.5
        As async_dsss_receiver_create().
    refine_samples_per_symbol : int, default 4
        As async_dsss_receiver_create().
    refine_design_margin_db : float, default 14.0
        As async_dsss_receiver_create().
    refine_n_fft : int, default 64
        As async_dsss_receiver_create().
    refine_zero_pad : int, default 8
        As async_dsss_receiver_create().
    refine_sequential : bool, default False
        As async_dsss_receiver_create().
    refine_max_n_blocks : int, default 100000
        As async_dsss_receiver_create().
    carrier_freq_hz : float, default 0.0
        Nominal RF carrier for carrier->code aiding; 0.0 (default) = off.
    lost_confirm_s : float, default 2.0
        Release rule, seconds of both flags down; default 2.0. 0 = never lost.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.dsss import Acquisition, HandoffAsyncDsssReceiver
    >>> from doppler.dsss import bin_to_signed
    >>> from doppler.dsss.handoff import dll_init_chip_from_acq
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
    >>> sig = (data[si] * csign[(idx // spc) % sf]
    ...        * np.exp(1j * 2 * np.pi * 0.5 * 500.0 * t * t))
    >>> cn0 = 20.0 + 10 * np.log10(sym)         # Es/N0 = 20 dB
    >>> sigma = np.sqrt(fs / 10 ** (cn0 / 10))
    >>> pre = 5 * te                            # noise-only lead-in
    >>> noise = (sigma / np.sqrt(2)) * (rng.standard_normal(pre + n)
    ...          + 1j * rng.standard_normal(pre + n))
    >>> x = (np.concatenate([np.zeros(pre), sig]).astype(np.complex64)
    ...      + noise.astype(np.complex64))

    The search is a separate object -- in a pool, one searcher per
    channel serves every receiver on it. Its hit is a correlation lag and
    a Doppler bin; the two documented helpers turn those into the seed:

    >>> acq = Acquisition(code, spc=spc, chip_rate=chip, symbol_rate=sym,
    ...                   cn0_dbhz=cn0, doppler_uncertainty=500.0)
    >>> for p in range(0, len(x) - te, te):
    ...     hits = acq.push(x[p:p + te])
    ...     if hits:
    ...         break
    >>> d_bin, lag, _, _, _, cn0_est, consumed = hits[0]
    >>> chip_phase = dll_init_chip_from_acq(lag, spc, sf)
    >>> res_hz = acq.doppler_res_hz
    >>> doppler_hz = bin_to_signed(d_bin, acq.doppler_bins) * res_hz

    The receiver never searched: it waits idle, takes the seed, and the
    samples from the hit onwards go to it.

    >>> rx = HandoffAsyncDsssReceiver(
    ...     code, chip_rate=chip, symbol_rate=sym, spc=spc, cn0_dbhz=cn0)
    >>> rx.idle
    1
    >>> rx.seed(chip_phase, doppler_hz, cn0_est)
    >>> (rx.idle, rx.refining)
    (0, 1)
    >>> syms = [rx.steps(x[p:p + te])
    ...         for p in range(int(consumed), len(x) - te, te)]
    >>> syms = np.concatenate([s for s in syms if len(s)])
    >>> rx.tracking                  # refined and tracking, no search
    1
    >>> len(syms) > 300
    True
    >>> bool(np.mean(syms.real**2) > 10 * np.mean(syms.imag**2))
    True

    Assigned once: a second seed is refused until reset(), which in this
    mode returns to idle, not to searching.

    >>> rx.seed(0.0, 0.0, cn0)  # doctest: +ELLIPSIS
    Traceback (most recent call last):
        ...
    ValueError: seed refused: ...
    >>> rx.reset()
    >>> rx.idle
    1

    """
    def __init__(
        self,
        code: NDArray[np.uint8],
        chip_rate: float = ...,
        symbol_rate: float = ...,
        spc: int = ...,
        m: int = ...,
        cn0_dbhz: float = ...,
        pfa: float = ...,
        pd: float = ...,
        segments: int = ...,
        sps: int = ...,
        differential: int = ...,
        refine_max_error_db: float = ...,
        refine_samples_per_symbol: int = ...,
        refine_design_margin_db: float = ...,
        refine_n_fft: int = ...,
        refine_zero_pad: int = ...,
        refine_sequential: bool = ...,
        refine_max_n_blocks: int = ...,
        carrier_freq_hz: float = ...,
        lost_confirm_s: float = ...,
    ) -> None: ...

    def steps(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Stream raw cf32 samples through the receiver. While searching,
        samples feed the embedded Acquisition and nothing is emitted. On a hit,
        the refine stage (a frozen-carrier Dll collection feeding
        CarrierAcquisition) is built and seeded from it, and the unconsumed
        tail of this call is handed straight to it -- no samples dropped. Once
        CarrierAcquisition reports ready (or its own give-up cap is reached),
        the live tracking chain (Dll + per-partial Costas + RateConverter +
        MpskReceiver) is built fresh, seeded from the ORIGINAL handoff chip
        phase and the refined-or-unrefined Doppler estimate, and demodulated
        symbols are returned from then on. Accepts any block size; state
        carries across calls.

        Drives the search -> refine -> track state machine. While searching or
        refining, nothing is emitted (an empty return is normal, not an error):
        a hit seeds the frozen-carrier refine chain, `CarrierAcquisition`
        sharpens the coarse Doppler estimate, and only once it is ready (or
        gives up) is the live tracking chain built and demodulation begins.
        Accepts any block size; state carries across calls, so a capture can be
        fed in frames of any length with no seam. Idle (hand-off mode, before a
        seed) and lost (after the release rule fires) consume the samples and
        emit nothing, so the feeding loop is the same in every state; while
        tracking, the release clock runs on the two lock flags after every call
        (see `lost_confirm_s`). Under SPEC's coupled offset + 500 Hz/s Doppler
        ramp the pre-despread Costas removes the full carrier dynamics before
        the code loop, so the recovered constellation lands cleanly on the BPSK
        real axis.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input cf32 samples.
        out : NDArray[np.complex64] | None
            Output symbols; caller provides max_out capacity.

        Returns
        -------
        NDArray[np.complex64]
            Number of symbols written (0 while searching/refining, or while
            tracking with not yet a full symbol's worth of input).

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

    def seed(
        self,
        chip_phase: float,
        doppler_hz_est: float,
        cn0_dbhz_est: float,
    ) -> None:
        """Take a detection from outside and start refining from it: the hit's
        chip phase (Dll's instantaneous convention, at the next sample fed),
        coarse Doppler estimate and C/N0 estimate -- exactly what the searching
        flavor's own hit produces. Accepted while idle (hand-off flavor) or
        searching; refused on a receiver that already holds a seed (refining,
        tracking or lost -- reset() releases it) and for a chip_phase outside
        [0, code_len).

        The hand-off of docs/design/async-dsss-receiver.md section 11.1: the
        three numbers a searcher's hit carries that this receiver uses --
        `acq_handoff_t`'s `chip_phase`, `doppler_hz_est` and `cn0_dbhz_est` --
        exactly as its own hit would have produced them (the searching flavor's
        `steps()` calls this on its own hit). `chip_phase` is the code's
        instantaneous phase in chips, Dll's convention, at the FIRST sample of
        the next `steps()` call; the Python-side conversion from a lag is
        `doppler.dsss.handoff`. The refine chain is rebuilt from the seed and
        the state becomes refining; the unconsumed tail is the caller's to
        feed.

        Refused (`DP_ERR_INVALID`, nothing changes) on a receiver that is not
        waiting for one -- refining, tracking or lost -- because "assigned
        once" is a property of the object, not of the caller's bookkeeping;
        `reset()` releases it. Accepted while idle (hand-off mode) or searching
        (the searching flavor: an outside hit simply beats its own). Also
        refused for a `chip_phase` outside `[0, code_len)` or a non-finite
        value.

        Parameters
        ----------
        chip_phase : float
            Code phase at the next sample, chips, in `[0, code_len)`.
        doppler_hz_est : float
            Coarse Doppler estimate, Hz (the refine stage sharpens it).
        cn0_dbhz_est : float
            The hit's C/N0 estimate, dB-Hz; reported back by get_cn0_dbhz_est()
            until tracking refreshes it.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``seed refused: the receiver already holds an assignment (refining,
            tracking or lost -- reset() releases it), or chip_phase is outside
            [0, code_len)``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import HandoffAsyncDsssReceiver
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> rx = HandoffAsyncDsssReceiver(code, chip_rate=3.069e6,
        ...                               symbol_rate=2700.0, spc=2)
        >>> rx.seed(chip_phase=512.25, doppler_hz_est=-1500.0,
        ...         cn0_dbhz_est=48.0)
        >>> (rx.idle, rx.refining, rx.doppler_hz, rx.cn0_dbhz_est)
        (0, 1, -1500.0, 48.0)

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
        """One consistent picture of the receiver, by value (design section
        11.3): state, where the emitter is now (live Doppler, chip phase, code
        rate, C/N0), both lock flags with the symbol-lock metric and threshold,
        both residual carrier errors, and the two clocks in input samples
        (since the state was entered; both flags down without a break). Read on
        demand by the holder of a pool -- the one-at-a-time properties are the
        same fields' other face. No timestamp: the holder owns the sample clock
        and stamps it.

        Cheap and allocation-free: every field is a read of live state. The
        one-at-a-time getters below report the same fields; this is the face a
        pool holder uses.

        Returns
        -------
        ReceiverStatus
            The record, by value.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import HandoffAsyncDsssReceiver
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> rx = HandoffAsyncDsssReceiver(code, chip_rate=3.069e6,
        ...                               symbol_rate=2700.0, spc=2)
        >>> st = rx.status()
        >>> (st.state, st.doppler_hz, st.code_locked, st.locked)   # idle
        (3, 0.0, 0, 0)
        >>> rx.seed(chip_phase=100.0, doppler_hz_est=-250.0, cn0_dbhz_est=50.0)
        >>> st = rx.status()
        >>> (st.state, round(st.doppler_hz, 6), st.cn0_dbhz_est)  # refining
        (1, -250.0, 50.0)
        >>> _ = rx.steps(np.zeros(2046, np.complex64))
        >>> rx.status().state_samples                             # since seed
        2046

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
        """Re-tune the live-tracking Dll's code-lock detector directly. Only
        meaningful once tracking has begun; a no-op while searching or
        refining.

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
        >>> rx.tracking                       # a no-op until tracking begins
        0

        """

    def configure_chain_raw(self, segments: int, sps: int, n: int) -> None:
        """Pin the live-tracking despread/resample/demod grid directly,
        bypassing the create-time segments/sps defaults. Only meaningful once
        tracking; rebuilds the chain with every replacement allocated first, so
        a failed pin leaves the receiver on its prior grid.

        Parameters
        ----------
        segments : int
            Live-tracking Dll segments per code period.
        sps : int
            MpskReceiver samples per symbol (the resample target).
        n : int
            MpskReceiver's carrier-arm count; must divide sps.

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
        """Return to the searching state: resets the embedded Acquisition and
        frees every refine-stage/track-stage child (rebuilt from scratch on the
        next hit).

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

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the HandoffAsyncDsssReceiver has already
        been destroyed.

        Returns
        -------
        int
            Byte length of one serialized state blob.
        """

    def get_state(self) -> bytes:
        """Serialize this object's mutable state to bytes.

        Captures exactly the state that evolves as the object runs, so a blob
        taken now and restored later resumes from this point. Construction
        parameters are not included: restore into an object built the same way.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the HandoffAsyncDsssReceiver has already
        been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()`
        before the blob is handed to the C core, and the core may reject it as
        well.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the HandoffAsyncDsssReceiver has already been
        destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def tracking(self) -> int:
        """1 once the live tracking chain is built and demodulating; 0 while
        searching or refining.
        """

    @property
    def refining(self) -> int:
        """1 while the refine stage (CarrierAcquisition collection) is active;
        0 while searching or tracking.
        """

    @property
    def idle(self) -> int:
        """1 while waiting for a seed (the hand-off flavor before seed() or
        after reset()); 0 in every other state.
        """

    @property
    def lost(self) -> int:
        """1 once the release rule has fired: both lock flags were down,
        continuously, for longer than lost_confirm_s while tracking. The loops
        have stopped and samples are discarded; the holder releases the
        assignment and calls reset(). Always 0 with lost_confirm_s = 0.
        """

    @property
    def doppler_hz(self) -> float:
        """The current best Doppler estimate: the coarse handoff value while
        refining, the CarrierAcquisition-refined value once tracking.
        """

    @property
    def cn0_dbhz_est(self) -> float:
        """Cached from the winning acquisition hit."""

    @property
    def segments(self) -> int:
        """Live-tracking Dll's own segments -- distinct from refine_segments
        above (see the module docstring / dll_lookback_segments()'s own doc on
        the WINDOWS vs TRACK_WINDOWS split).
        """

    @property
    def sps(self) -> int:
        """MpskReceiver's own samples/symbol."""

    @property
    def n(self) -> int:
        """MpskReceiver's own carrier-arm count."""

    @property
    def chip_phase(self) -> float:
        """Live Dll code phase in chips, Dll's own instantaneous-phase
        convention (the mirror image of acq_result_t::code_phase's
        correlation-lag convention -- see acq_build_handoff()'s doc
        comment).
        """

    @property
    def code_rate(self) -> float:
        """Live Dll code rate: chips advanced per nominal chip (~1.0)."""

    @property
    def lock(self) -> float:
        """decision rule on lock_metric: thresholds + verify counters, stepped
        per symbol.
        """

    @property
    def norm_freq(self) -> float:
        """Smoothed carrier estimate (integrator only, cycles/sample of the
        MpskReceiver output rate); lags a Doppler ramp by the constant Type-II
        ramp error.
        """

    @property
    def nco_freq(self) -> float:
        """Live carrier loop-filter output = NCO frequency command
        (cycles/sample of the MpskReceiver output rate): its mean tracks a
        Doppler ramp with no lag, its variance is the carrier loop stress.
        """

    @property
    def locked(self) -> int:
        """Binary receiver lock: the hysteretic (up/down verify-counted) lock
        detector on the emitted symbols -- declared when lock_metric stays >=
        lock_threshold for the up-count and dropped below it for the
        down-count.
        """

    @property
    def lock_metric(self) -> float:
        """Symbol-lock metric: SNR-weighted running mean of the BPSK lock
        signal (I^2-Q^2)/(I^2+Q^2) = cos(2*phi) over the emitted symbols
        (locked -> ~+1). Drives `locked`; exposed for engineering debug.
        """

    @property
    def lock_threshold(self) -> float:
        """The lock_metric declare threshold `locked` latches above (the
        lockdet up_thresh); exposed alongside lock_metric for engineering
        debug.
        """

    @property
    def car_last_error(self) -> float:
        """Pre-despread Costas phase discriminator (rad): the residual carrier
        phase loop 1 (de-rotates before the Dll) is not nulling. Engineering
        debug.
        """

    @property
    def car_nco_freq(self) -> float:
        """Loop 1 (pre-despread Costas) loop-filter output = NCO frequency
        command, cycles/sample of the front-end (chip_rate*spc) rate.
        Engineering debug.
        """

    @property
    def mpsk_last_error(self) -> float:
        """MpskReceiver carrier phase discriminator (rad): the residual carrier
        phase loop 2 (post-despread) is not nulling. Engineering debug.
        """

    @property
    def code_locked(self) -> int:
        """Binary code-lock flag from the live tracking Dll's own
        verify-counted (pfa-tuned) lock detector -- the fundamental DSSS "am I
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


    def __enter__(self) -> "HandoffAsyncDsssReceiver":
        """Enter a context manager, returning this object.

        Lets a HandoffAsyncDsssReceiver be used in a `with` statement so its C
        resources are released deterministically on exit rather than at
        collection time.

        Returns
        -------
        HandoffAsyncDsssReceiver
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the HandoffAsyncDsssReceiver.

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
    """Create a burst receiver: acquisition, refine and demodulation composed
    behind one push().

    Parameters
    ----------
    acq_code : NDArray[np.uint8]
        Preamble PN chips (0/1), length acq_code_len.
    data_code : NDArray[np.uint8]
        Payload spreading chips (0/1), data_code_len long.
    sync : NDArray[np.uint8]
        Frame sync word (0/1 symbols), sync_len long.
    reps : int, default 5
        Preamble code repetitions (>= 1).
    spc : int, default 4
        Samples per chip (>= 1).
    chip_rate : float, default 1000000.0
        Chip rate in Hz (> 0).
    frame_syms : int, default 64
        Frame symbols per burst (>= 1) — what push() returns, bit for bit.
    cn0_dbhz : float, default 0.0
        Carrier-to-noise density in dB-Hz (> 0), sizing the acquisition search.
    doppler_uncertainty : float, default 0.0
        One-sided Doppler half-range, Hz.
    pfa : float, default 1e-3
        Target false-alarm probability, in (0, 1).
    pd : float, default 0.9
        Target detection probability, in (0, 1).
    carrier_hz : float, default 0.0
        RF carrier (Hz) for code-Doppler; 0 = ignore.
    max_rate : float, default 0.0
        Chirp-rate search half-span (cycles/sample^2).
    est_segments : int, default 10
        Segments the feedforward estimator fits over.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``DsssBurstReceiver:
        invalid parameter (need non-empty acq_code/data_code/sync, reps >= 1,
        spc >= 1, chip_rate > 0, frame_syms >= 1, cn0_dbhz >= 0, 0 < pfa < 1, 0
        < pd < 1)``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.dsss import DsssBurstReceiver
    >>> rng = np.random.default_rng(0)
    >>> acq = rng.integers(0, 2, 31).astype(np.uint8)
    >>> dat = rng.integers(0, 2, 8).astype(np.uint8)
    >>> syn = np.zeros(13, dtype=np.uint8)
    >>> rx = DsssBurstReceiver(acq, dat, syn, reps=4, spc=4,
    ...                        frame_syms=32)
    >>> rx.n_bursts
    0

    """
    def __init__(
        self,
        acq_code: NDArray[np.uint8],
        data_code: NDArray[np.uint8],
        sync: NDArray[np.uint8],
        reps: int = ...,
        spc: int = ...,
        chip_rate: float = ...,
        frame_syms: int = ...,
        cn0_dbhz: float = ...,
        doppler_uncertainty: float = ...,
        pfa: float = ...,
        pd: float = ...,
        carrier_hz: float = ...,
        max_rate: float = ...,
        est_segments: int = ...,
    ) -> None: ...

    def push(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.uint8] | None = None,
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
        x : NDArray[np.complex64]
            Input samples (cf32), x_len long.
        out : NDArray[np.uint8] | None
            Payload bits, caller-owned, max_out long.

        Returns
        -------
        NDArray[np.uint8]
            Bits written to out -- `n_bursts_returned * frame_syms`.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import DsssBurstReceiver
        >>> rng = np.random.default_rng(0)
        >>> rx = DsssBurstReceiver(
        ...     rng.integers(0, 2, 31).astype(np.uint8),
        ...     rng.integers(0, 2, 8).astype(np.uint8),
        ...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, frame_syms=32)
        >>> bits = rx.push(np.zeros(4096, dtype=np.complex64))
        >>> bits.size            # silence carries no burst
        0

        """

    def push_max_out(self, x_len: int) -> int:
        """Max bits push() can write for an input of x_len samples.

        push() returns EVERY burst it completed, so the bound scales with the

        input: distinct bursts cannot overlap, so they are at least `burst_len`

        apart, and a push of x_len samples can complete at most

        `x_len/burst_len + 1` of them -- plus every detection already queued
        from

        an earlier call, which is `q_cap`.

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
        out: NDArray[np.float32] | None = None,
    ) -> NDArray[np.float32]:
        """The SOFT bits of every burst the last push() returned, concatenated:
        burst i occupies llr[i*frame_bits:(i+1)*frame_bits], in the same order
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
        `frame_bits` per burst: burst i starts at `i * frame_bits`, in the
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
        out : NDArray[np.float32] | None
            Receives the LLRs.

        Returns
        -------
        NDArray[np.float32]
            LLRs written.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import DsssBurstReceiver
        >>> rng = np.random.default_rng(0)
        >>> rx = DsssBurstReceiver(
        ...     rng.integers(0, 2, 31).astype(np.uint8),
        ...     rng.integers(0, 2, 8).astype(np.uint8),
        ...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, frame_syms=32)
        >>> bits = rx.push(np.zeros(4096, dtype=np.complex64))
        >>> len(bits), len(rx.llrs(rx.llrs_max_out(1)))   # nothing decoded
        (0, 0)

        """

    def llrs_max_out(self, n: int) -> int:
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
        out: NDArray[Any] | None = None,
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
        out : NDArray[Any] | None
            Records, caller-owned, max_out long.

        Returns
        -------
        NDArray[Any]
            Records written to out -- `min(events_max_out(), max_out)`.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import DsssBurstReceiver
        >>> rng = np.random.default_rng(0)
        >>> rx = DsssBurstReceiver(
        ...     rng.integers(0, 2, 31).astype(np.uint8),
        ...     rng.integers(0, 2, 8).astype(np.uint8),
        ...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, frame_syms=32)
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
        >>> rng = np.random.default_rng(0)
        >>> rx = DsssBurstReceiver(
        ...     rng.integers(0, 2, 31).astype(np.uint8),
        ...     rng.integers(0, 2, 8).astype(np.uint8),
        ...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, frame_syms=32)
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
        >>> rng = np.random.default_rng(0)
        >>> rx = DsssBurstReceiver(
        ...     rng.integers(0, 2, 31).astype(np.uint8),
        ...     rng.integers(0, 2, 8).astype(np.uint8),
        ...     np.zeros(13, dtype=np.uint8), reps=4, spc=4, frame_syms=32)
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

        Captures exactly the state that evolves as the object runs, so a blob
        taken now and restored later resumes from this point. Construction
        parameters are not included: restore into an object built the same way.

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

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()`
        before the blob is handed to the C core, and the core may reject it as
        well.

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
        """Exact stream position of the preamble."""

    @property
    def doppler_hz_est(self) -> float:
        """Signed coarse Doppler, Hz."""

    @property
    def doppler_res_hz(self) -> float:
        """Acquisition's native bin width, Hz."""

    @property
    def cn0_dbhz_est(self) -> float:
        """C/N0 lower bound from the hit, dB-Hz."""

    @property
    def est_freq_hz(self) -> float:
        """Demod's residual-frequency estimate."""

    @property
    def est_rate_hz(self) -> float:
        """Demod's chirp-rate estimate."""

    @property
    def est_snr_db(self) -> float:
        """Demod's post-decode SNR estimate."""

    @property
    def refine_margin(self) -> float:
        """Runner-up period over the winner."""

    @property
    def frame_valid(self) -> bool:
        """Whether the most recent window's frame passed its error detection --
        this receiver's frame ends in a CRC-16. The verdict that decides
        whether the window OWNS its span: a failed window is given back to the
        capture (`release`), so a decoy ahead of a real burst cannot swallow it
        (doppler#1181). Per burst, read `events()['frame_valid']`.
        """

    @property
    def min_gap(self) -> int:
        """Dead air to leave BETWEEN bursts, in samples — edge to edge. The
        number a caller placing bursts actually wants, derived by the capture
        rather than left as a rule to apply: `refine_span + reps*code_period -
        burst_len`, floored at zero. `refine_span` below is the reach it comes
        from, and is a start-to-start bound rather than a gap.
        """

    @property
    def refine_span(self) -> int:
        """Coalescing window, in samples -- the reach over which two detections
        are ONE preamble. Both sides of that test are burst STARTS (resolved
        code epochs), so this bounds start-to-start separation, NOT the dead
        air between bursts. The two differ by a whole burst, and reading it as
        dead air costs a caller real airtime for nothing. The gap actually
        required is max(0, refine_span - burst_len) burst_len = retain_span -
        refine_span which was wrong -- short by the whole detection-lag term.
        **Read `min_gap` instead**; the object derives it (doppler#1172). The
        SHAPE of the old rule was right and is worth keeping: this reach is a
        start-to-start bound, and reading it as required silence cost 9% of
        airtime. At a 255-chip code, `reps=5`, `spc=2` the reach is 12240
        samples; an 8316-sample burst placed 8916 apart start-to-start (600
        samples of dead air against the 3924 required) yields 2 decodes from 7
        bursts with `dropped == 0`, while at `frame_syms=2053` the same code
        gives a 261228-sample burst -- 21.3x the reach -- and every spacing
        down to ZERO dead air yields 7 of 7 (doppler#1085).
        """

    @property
    def retain_span(self) -> int:
        """History kept per anchor, in samples -- the MINIMUM TRAILING CONTEXT.
        `refine_span` plus one whole burst. A burst closer than this to the end
        of what has been pushed is held rather than emitted, because refine
        cannot yet see the samples it needs. Feed at least this many more, or
        the last burst of a capture never comes out. At the geometry above the
        boundary is sharp: 20500 trailing samples against a `retain_span` of
        20556 loses the burst.
        """

    @property
    def pending(self) -> int:
        """Detections held because their burst window has not fully arrived.
        `push()` emits nothing for these on purpose -- a burst is returned when
        it is complete, not when it is guessed at. Feed more samples and it
        comes out bit-exact, wherever the split fell. Read it at the END of a
        stream. A caller closing a file or a socket while this is non-zero is
        discarding a burst that would have decoded, and nothing else
        distinguishes that from an empty capture: `dropped` counts samples the
        ring refused, `n_bursts` counts what was demodulated, and a truncated
        burst is neither.
        """

    @property
    def dropped(self) -> int:
        """Overrun ctr."""

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

def bin_to_signed(bin: int, n_bins: int) -> int:
    """Map an FFT bin index to its SIGNED frequency index --
    numpy.fft.fftfreq(n) * n, exactly: 0 = DC, ascending positive to
    (n-1)/2, then wrapping negative, so an even grid's Nyquist bin is -n/2.
    Multiply by doppler_res_hz for Hz. Call this rather than writing the
    fold out: the search and its hand-off must agree on the convention, and
    a consumer seeded on the wrong side of it is off by the full search
    span -- a failure that once surfaced here as a receiver reporting
    tracking while decoding noise. A thin wrapper over dp_fftfreq_index()
    in clib_common.h, so C callers inline the same code.

    Parameters
    ----------
    bin : int
        Bin index in `[0, n_bins)`.
    n_bins : int
        Grid size.

    Returns
    -------
    int
        Signed index in `[-(n_bins/2), +((n_bins-1)/2)]`.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.dsss import bin_to_signed
    >>> [bin_to_signed(b, 8) for b in range(8)]
    [0, 1, 2, 3, -4, -3, -2, -1]
    >>> (np.fft.fftfreq(8) * 8).astype(int).tolist()   # same convention
    [0, 1, 2, 3, -4, -3, -2, -1]
    >>> bin_to_signed(4, 7)                         # odd grid: no ambiguity
    -3

    """
