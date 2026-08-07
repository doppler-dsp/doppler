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
        Normalised frequency −0.5..+0.5 (DC-centred).
    rate_norm : float
        chirp rate, cycles/sample^2.
    snr_db : float
        winning-row peak-to-mean (rough confidence).
    """

    @property
    def freq_norm(self) -> float:
        """Normalised frequency −0.5..+0.5 (DC-centred)."""

    @property
    def rate_norm(self) -> float:
        """chirp rate, cycles/sample^2."""

    @property
    def snr_db(self) -> float:
        """winning-row peak-to-mean (rough confidence)."""

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
        // seed from acquisition (norm_freq cyc/sample, chip phase in chips):
        burst_despreader_state_t *d = burst_despreader_create(code, n, 32, 2, f0, chip, .05, .01);
        float complex sym[256];
        size_t k = burst_despreader_steps(d, rx, rx_len, sym, 256);
        // hard bit of sym[i] = crealf(sym[i]) >= 0
        burst_despreader_destroy(d);

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
    cn0_dbhz : float, default 50.0
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
        """zero-padded transform length (next pow2 of max_len)."""

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
    payload_len : int, default 0
        Number of payload data symbols (bits) in a frame.
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
    >>> d = BurstDemod(dcode, spc=spc, chip_rate=1e6, payload_len=64)
    >>> d.set_preamble(acode, reps)   # unmodulated (f0, rate) preamble
    >>> d.set_sync(sync)              # Barker-13 frame-sync word
    >>> d.set_prior(f0, 0)           # coarse Doppler + preamble start
    >>> bits = d.demod(x)      # estimate -> dechirp -> despread -> slice
    >>> int(d.frame_valid), bool(np.array_equal(bits, payload))
    (1, True)

    """
    def __init__(
        self,
        data_code: NDArray[np.uint8],
        spc: int = ...,
        chip_rate: float = ...,
        carrier_hz: float = ...,
        max_rate: float = ...,
        payload_len: int = ...,
        est_segments: int = ...,
    ) -> None: ...

    def reset(self) -> None:
        """Clear the per-burst read-backs, leaving the configuration intact.

        Zeros the after-demod fields (frame_valid, frame_offset, n_symbols, and
        the est_* estimates) so a stale result cannot be mistaken for a fresh
        one. The spreading codes, sync word, and prior set up before the first
        burst are preserved, so the object is immediately ready to demodulate
        the next burst.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstDemod
        >>> dcode = (np.arange(50) & 1).astype(np.uint8)
        >>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, payload_len=64)
        >>> d.reset()          # clears est_ + frame_valid, keeps config
        >>> d.frame_valid
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
        >>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, payload_len=64)
        >>> acode = (np.arange(500) & 1).astype(np.uint8)  # unmodulated
        >>> d.set_preamble(acode, reps=5)  # 5 reps drive the (f0, rate) fit

        """

    def set_sync(self, sync: NDArray[np.uint8]) -> None:
        """Set the known frame-sync word (0/1 BPSK symbols) used for frame
        alignment + phase/sign resolution.

        After the data section is despread to soft BPSK symbols, demod()
        correlates them against this word; the complex correlation peak locates
        the frame (its frame_offset) and its phase resolves the residual
        carrier rotation and the BPSK sign ambiguity before slicing. Pass the
        word as 0/1 symbols; it is copied and stored internally as +/-1.

        Parameters
        ----------
        sync : NDArray[np.uint8]
            Frame-sync word, one 0/1 symbol per element; copied.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import BurstDemod
        >>> dcode = (np.arange(50) & 1).astype(np.uint8)
        >>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, payload_len=64)
        >>> sync = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)
        >>> d.set_sync(sync)   # Barker-13: frame align + phase/sign fix

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
        >>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, payload_len=64)
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
    def frame_valid(self) -> int:
        """1 if the CRC-16 trailer matched."""

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
    def payload_len(self) -> int:
        """payload data symbols (bits)."""

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
        succeeded (mirrors `Acquisition`'s own `_regrid()` discipline) — a
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
        fed in frames of any length with no seam. Under SPEC's coupled offset +
        500 Hz/s Doppler ramp the pre-despread Costas removes the full carrier
        dynamics before the code loop, so the recovered constellation lands
        cleanly on the BPSK real axis.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input cf32 samples.

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
        """Chips, Dll's own instantaneous-phase convention (the mirror image of
        acq_result_t::code_phase's correlation-lag convention -- see
        acq_build_handoff()'s doc comment).
        """

    @property
    def code_rate(self) -> float:
        """chips advanced per nominal chip (~1.0)."""

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
