# track/track.pyi — type stubs for the track C extension.
from typing import final, Literal
import numpy as np
from numpy.typing import NDArray

@final
class LoopFilter:
    """Create a loop_filter instance, validating its arguments.

    Parameters
    ----------
    bn : float, default 0.01
        Loop noise bandwidth, normalized cycles/sample; >= 0 and finite
        (default 0.01).
    zeta : float, default 0.707
        Damping factor; > 0 and finite (default 0.707).
    t : float, default 1.0
        Update period in samples; > 0 and finite (default 1.0).

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``bn must be >= 0, zeta
        > 0 and t > 0, and all three finite``.

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import LoopFilter
    >>> obj = LoopFilter(bn=0.01, zeta=0.707, t=1.0)

    """
    def __init__(
        self,
        bn: float = ...,
        zeta: float = ...,
        t: float = ...,
    ) -> None: ...

    def step(self, x: float) -> float:
        """Advance the loop one update with error x and return the control
        value the tracker should apply.

        The PI recurrence is `integ += ki*x; control = integ + kp*x`: the
        integrator accumulates the running frequency/rate estimate while the
        proportional term kp*x is the instantaneous phase nudge.

        Fed a constant error with nothing closing the loop, the integrator —
        and therefore the control — **ramps without bound**; measured at 1.84x
        between updates 200 and 400 at `bn = 0.02`. That is the accumulation
        working, not a defect, and it is what pulls a Costas/DLL/timing loop
        into lock once the loop IS closed, because a converging loop is one
        whose error is being driven to zero by the correction. Convergence is a
        property of the closed loop; this function is one term in it.

        Parameters
        ----------
        x : float
            Loop error (discriminator output) for this update.

        Returns
        -------
        float
            Control value integ+kp*x to drive the NCO / interpolator.

        Examples
        --------
        >>> from doppler.track import LoopFilter
        >>> lf = LoopFilter(bn=0.02, zeta=0.707, t=1.0)
        >>> round(lf.step(1.0), 6)   # unit error: control = ki + kp
        0.05331
        >>> round(lf.integ, 6)       # integrator now holds ki
        0.001385

        """

    def steps(
        self,
        x: NDArray[np.float64],
        out: NDArray[np.float64] | None = None,
    ) -> NDArray[np.float64]:
        """Filter a whole block of loop errors, returning the control value for
        each update.

        Equivalent to calling loop_filter_step() once per element of x in
        order, carrying the integrator across the block, so the loop's memory
        and lock state persist from one call to the next. This is the block
        path used to run a captured error sequence through the filter in one
        shot — a plain per-element loop, not a vectorized one: the recurrence
        is sequential, so each update depends on the one before it.

        Parameters
        ----------
        x : NDArray[np.float64]
            Loop-error array, one discriminator sample per update.

        Returns
        -------
        NDArray[np.float64]
            Output.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import LoopFilter
        >>> lf = LoopFilter(bn=0.05, zeta=0.707, t=1.0)
        >>> ctl = lf.steps(np.full(50, 0.1))   # constant error into the loop
        >>> round(float(ctl[0]), 4)            # first control nudge
        0.0133
        >>> round(float(ctl[-1]), 4)           # open loop: ramping
        0.0541

        """

    def configure(self, bn: float, zeta: float, t: float) -> None:
        """Recompute the loop gains for a new (bn, zeta, t); preserves the
        integrator.

        Recomputes the proportional and integral gains from the standard
        2nd-order form but leaves integ untouched, so a loop can be widened for
        fast acquisition and then narrowed for steady-state tracking while
        holding its accumulated frequency/rate estimate — the retune preserves
        lock.

        Parameters
        ----------
        bn : float
            Loop noise bandwidth, normalized cycles/sample (>= 0).
        zeta : float
            Damping factor (typically 0.707).
        t : float
            Update period in samples (> 0).

        Examples
        --------
        >>> from doppler.track import LoopFilter
        >>> lf = LoopFilter(bn=0.01, zeta=0.707, t=1.0)
        >>> _ = lf.step(1.0)
        >>> before = round(lf.integ, 6)
        >>> lf.configure(0.05, 0.707, 1.0)   # widen the loop, keep lock
        >>> round(lf.integ, 6) == before     # integrator preserved
        True
        >>> round(lf.kp, 6)                  # proportional gain rose
        0.124728

        """

    def reset(self) -> None:
        """Zero the integrator; keep the configured gains.

        Clears the accumulated frequency/rate estimate (integ) back to zero but
        leaves kp / ki as configured, so the loop reacquires from a clean slate
        at its current bandwidth — the right thing when a tracker drops lock
        and must restart, without re-deriving gains.

        Examples
        --------
        >>> from doppler.track import LoopFilter
        >>> lf = LoopFilter(bn=0.02, zeta=0.707, t=1.0)
        >>> for _ in range(10):
        ...     _ = lf.step(1.0)             # ramp the integrator
        >>> round(lf.integ, 6)
        0.013849
        >>> lf.reset()
        >>> lf.integ                          # integrator cleared, gains kept
        0.0

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the LoopFilter has already been destroyed.

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

        Raises ``RuntimeError`` if the LoopFilter has already been destroyed.

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
        ``RuntimeError`` if the LoopFilter has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def kp(self) -> float:
        """proportional gain (derived from bn, zeta, t)."""

    @property
    def ki(self) -> float:
        """integral gain (derived from bn, zeta, t)."""

    @property
    def integ(self) -> float:
        """integrator memory = running rate/freq estimate."""
    @integ.setter
    def integ(self, value: float) -> None: ...

    @property
    def bn(self) -> float:
        """loop noise bandwidth, normalized cycles/sample."""

    @property
    def zeta(self) -> float:
        """damping factor (0.707 = critically damped)."""

    @property
    def t(self) -> float:
        """update period in samples."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "LoopFilter":
        """Enter a context manager, returning this object.

        Lets a LoopFilter be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        LoopFilter
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the LoopFilter.

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
class Costas:
    """Costas component.

    Parameters
    ----------
    bn : float, default 0.05
        bn constructor parameter.
    zeta : float, default 0.707
        zeta constructor parameter.
    init_norm_freq : float, default 0.0
        init_norm_freq constructor parameter.
    tsamps : int, default 64
        tsamps constructor parameter.
    bn_fll : float, default 0.0
        bn_fll constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import Costas
    >>> obj = Costas(
    ...     bn=0.05,
    ...     zeta=0.707,
    ...     init_norm_freq=0.0,
    ...     tsamps=64,
    ...     bn_fll=0.0,
    ... )

    """
    def __init__(
        self,
        bn: float = ...,
        zeta: float = ...,
        init_norm_freq: float = ...,
        tsamps: int = ...,
        bn_fll: float = ...,
    ) -> None: ...

    def steps(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """De-rotate a cf32 block with the integer-NCO carrier, coherently
        integrate over each tsamps-sample symbol, run the decision-directed
        Costas discriminator, and emit one complex prompt symbol per symbol.

        The streaming Python face of the loop. For every input sample it wipes
        the (tracked) carrier off x with the integer-phase NCO, sums the result
        into the coherent integrate-and-dump accumulator, and on each symbol
        boundary (one every tsamps samples) dumps the accumulator as the
        prompt, runs the BPSK Costas discriminator to steer the NCO frequency
        and phase, and appends the mean-scaled prompt to the output. Loop state
        carries across calls, so a long capture can be fed block by block;
        exactly one prompt symbol comes out per tsamps input samples.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input samples, one complex baseband sample each.
        out : NDArray[np.complex64] | None
            Prompt-symbol output buffer.

        Returns
        -------
        NDArray[np.complex64]
            Number of prompt symbols written to out (one per tsamps input
            samples). On the Python face this is the recovered-symbol array.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import Costas
        >>> tsamps = 16
        >>> rng = np.random.default_rng(1)
        >>> bits = rng.integers(0, 2, 4000) * 2 - 1
        >>> sig = np.repeat(bits.astype(np.complex64), tsamps)
        >>> k = np.arange(len(sig))
        >>> rx = (sig * np.exp(2j * np.pi * 0.003 * k)).astype(np.complex64)
        >>> c = Costas(bn=0.05, zeta=0.707, tsamps=tsamps)
        >>> sym = c.steps(rx)             # one prompt per tsamps samples
        >>> sym.shape
        (4000,)
        >>> round(c.norm_freq, 4)         # pulled onto the 0.003 residual
        0.003
        >>> c.lock_metric > 0.9
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

    def set_telemetry(
        self,
        tlm: object | None,
        prefix: str,
        decim: int = 1,
    ) -> None:
        """Attach (or detach) a telemetry context and register the carrier
        loop's probes on it. Registers four probes, emitted once per dumped
        symbol and further thinned by decim: "<prefix>.lock" (the |Re P|/|P|
        lock-metric EMA, 1 = phase-locked), "<prefix>.e" (the PLL discriminator
        output — the loop stress), "<prefix>.freq" (the tracked NCO frequency,
        cycles/sample) and "<prefix>.locked" (the verify-counted lock decision,
        0/1 — see costas_configure_lock). Passing NULL detaches. Setup path,
        never hot: call before the producer thread starts stepping; the context
        is borrowed and must outlive the attachment (SPSC rules in
        dp_tlm/dp_tlm_core.h).

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "car" or "ch0.car".
        decim : int
            Emit every decim-th symbol; >= 1.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_telemetry failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import Costas
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> c = Costas(bn=0.05, zeta=0.707, tsamps=64)
        >>> c.set_telemetry(tlm, "car")
        >>> sorted(tlm.probe_names)
        ['car.e', 'car.freq', 'car.lock', 'car.locked']
        >>> x = np.ones(64 * 100, dtype=np.complex64)
        >>> _ = c.steps(x)
        >>> recs = tlm.read()   # four records per dumped symbol
        >>> len(recs) == 4 * 100
        True

        """

    def configure(self, bn: float, zeta: float) -> None:
        """Recompute the loop gains for a new (bn, zeta); preserves the
        frequency/phase estimate.

        Re-derives the PI coefficients from the loop bandwidth and damping and
        installs them live. The NCO frequency, phase and loop integrator are
        left untouched, so a converged loop keeps tracking straight through the
        re-tune — narrow the bandwidth once pulled in for lower phase jitter,
        or widen it to chase a faster-moving residual.

        Parameters
        ----------
        bn : float
            Loop noise bandwidth, normalised to the symbol rate.
        zeta : float
            Damping factor (0.707 = critically damped).

        Examples
        --------
        >>> from doppler.track import Costas
        >>> c = Costas(bn=0.05, zeta=0.707, init_norm_freq=0.01, tsamps=16)
        >>> c.configure(0.02, 1.0)              # narrow the loop, over-damp
        >>> (round(c.bn, 3), round(c.norm_freq, 3))  # new gains, est kept
        (0.02, 0.01)

        """

    def configure_lock(
        self,
        up_thresh: float,
        down_thresh: float,
        n_up: int,
        n_down: int,
    ) -> None:
        """Re-tune the carrier lock detector: locked flips up after n_up
        consecutive dumped symbols with the lock-metric EMA above up_thresh,
        and drops after n_down consecutive symbols below down_thresh (level +
        time hysteresis; see detection.LockDet). The defaults (0.85/0.78, 8 up
        / 32 down) derive from the metric's no-carrier statistics: |Re P|/|P|
        averages 2/pi (~0.64) under H0 with an EMA-smoothed std of ~0.07, so
        the declare threshold sits ~3 sigma above the no-carrier mean. A live
        lock survives the re-tune; the in-flight verify run restarts.

        The always-on lock decision steps a verify-counted detector
        (lockdet_core.h) on the |Re P|/|P| lock-metric EMA once per dumped
        symbol: `locked` flips up after n_up consecutive symbols with the
        metric above up_thresh and drops after n_down consecutive symbols below
        down_thresh. The defaults derive from the metric's own H0 statistics —
        with no carrier, |Re P|/|P| = |cos(theta)| for a uniform theta, whose
        mean is 2/pi (~0.637) and per-symbol std ~0.31; the COSTAS_LOCK_ALPHA =
        0.1 EMA reduces that to ~0.071, so the default declare threshold 0.85
        sits ~3 sigma above the no-carrier mean, with the drop threshold at
        0.78 for level hysteresis and 8-up/32-down verify counts for time
        hysteresis (declare fast, drop reluctantly — the EMA already correlates
        adjacent looks, so the counts guard against band-edge dwell rather than
        compounding i.i.d. probabilities). A live lock survives the re-tune;
        the in-flight verify run restarts.

        Parameters
        ----------
        up_thresh : float
            Declare threshold on the lock-metric EMA.
        down_thresh : float
            Drop threshold (<= up_thresh for level hysteresis).
        n_up : int
            Consecutive above-threshold symbols to declare; clamped to >= 1.
        n_down : int
            Consecutive below-threshold symbols to drop; clamped to >= 1.

        Examples
        --------
        >>> from doppler.track import Costas
        >>> c = Costas(bn=0.05, zeta=0.707, tsamps=64)
        >>> c.locked
        False
        >>> c.configure_lock(0.9, 0.8, 4, 16)   # tighter declare, faster drop

        """

    def reset(self) -> None:
        """Re-seed the loop to the create-time frequency/phase; preserve
        config.

        Drops the lock and rewinds the NCO, loop integrator and
        integrate-and-dump accumulators to the create-time seed frequency,
        while retaining the configured loop bandwidth, damping and
        lock-detector thresholds. Reprocess the same input after a reset and
        the output is bit-identical.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import Costas
        >>> tsamps = 16
        >>> rng = np.random.default_rng(3)
        >>> bits = rng.integers(0, 2, 1500) * 2 - 1
        >>> sig = np.repeat(bits.astype(np.complex64), tsamps)
        >>> k = np.arange(len(sig))
        >>> rx = (sig * np.exp(2j * np.pi * 0.002 * k)).astype(np.complex64)
        >>> c = Costas(bn=0.05, zeta=0.707, tsamps=tsamps)
        >>> _ = c.steps(rx)
        >>> round(c.norm_freq, 4) != 0.0     # loop pulled onto the residual
        True
        >>> c.reset()
        >>> c.norm_freq                       # back to the create-time seed
        0.0
        >>> c.lock_metric
        0.0

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the Costas has already been destroyed.

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

        Raises ``RuntimeError`` if the Costas has already been destroyed.

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
        ``RuntimeError`` if the Costas has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def bn(self) -> float:
        """PLL loop noise bandwidth (retained)."""
    @bn.setter
    def bn(self, value: float) -> None: ...

    @property
    def norm_freq(self) -> float:
        """Norm freq."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def lock_metric(self) -> float:
        """EMA of |Re P|/|P| (1 = locked)."""

    @property
    def locked(self) -> bool:
        """Current carrier lock decision: True after the verify count of
        consecutive above-threshold symbols, False again after the drop count
        of consecutive below-threshold ones (see configure_lock).
        """

    @property
    def last_error(self) -> float:
        """last PLL discriminator (loop stress)."""

    @property
    def bn_fll(self) -> float:
        """FLL-assist bandwidth (0 = pure PLL)."""
    @bn_fll.setter
    def bn_fll(self, value: float) -> None: ...

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "Costas":
        """Enter a context manager, returning this object.

        Lets a Costas be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Costas
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the Costas.

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
class Dll:
    """Create a code/timing delay-locked loop over a spreading code.

    Parameters
    ----------
    code : NDArray[np.uint8]
        Spreading code (0/1 chips), one period; copied internally.
    sps : int, default 2
        Samples per chip (default 2).
    init_chip : float, default 0.0
        Seed code phase, chips (default 0.0).
    bn : float, default 0.01
        Loop noise bandwidth (default 0.01).
    zeta : float, default 0.707
        Damping factor (default 0.707).
    spacing : float, default 0.5
        Early/late tap offset, chips (default 0.5).
    segments : int, default 1
        Partial correlations per code epoch (default 1). 1 = a coherent
        full-epoch integrate-and-dump (one prompt/period). >1 splits each epoch
        into that many sub-epoch partials: it emits that many partial
        prompts/period and tracks the code non-coherently across them (robust
        to an asynchronous data-symbol clock). segments/epoch ~ samples/symbol
        at a downstream SymbolSync when the symbol rate is near the code rate,
        so choose >= 2 for symbol-timing recovery.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.track import Dll
    >>> rng = np.random.default_rng(1)
    >>> code = rng.integers(0, 2, 31).astype(np.uint8)  # a 31-chip PN code
    >>> chip = np.where(code & 1, -1.0, 1.0)    # BPSK spreading code
    >>> x = np.tile(np.repeat(chip, 2), 60).astype(np.complex64)
    >>> d = Dll(code=code, sps=2)               # 2 samples/chip loop
    >>> sym = d.steps(x)                        # one prompt per period
    >>> sym.shape                               # 60 despread symbols
    (60,)
    >>> round(float(np.mean(sym.real[-10:])), 1)  # despread to a clean +1
    1.0
    >>> round(d.code_rate, 3)                   # code NCO at nominal rate
    1.0

    """
    def __init__(
        self,
        code: NDArray[np.uint8],
        sps: int = ...,
        init_chip: float = ...,
        bn: float = ...,
        zeta: float = ...,
        spacing: float = ...,
        segments: int = ...,
    ) -> None: ...

    def steps(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Correlate a cf32 block against the local code with early/prompt/late
        taps and steer the code NCO each code period on the non-coherent
        (sum|E|-sum|L|)/(sum|E|+sum|L|) discriminator. With segments=1
        (default) this is a coherent full-epoch integrate-and-dump: one prompt
        symbol per period. With segments>1 each epoch is split into that many
        sub-epoch partial correlations: it emits that many partial prompts per
        period (a stream at ~segments samples/symbol when the symbol rate is
        near the code rate) and tracks the code non-coherently across the
        partials, which a data flip cannot collapse (robust to an asynchronous
        data-symbol clock). segments>1 is the streaming despreader: it removes
        the PN code and outputs samples. The non-coherent loop is
        carrier-blind, so it tracks with a residual carrier still on the input;
        carrier recovery (Costas) and symbol-timing recovery (SymbolSync) are
        downstream stages fed from the partial output. Returned blocks are safe
        to keep across calls (block-size invariant): a block whose array is
        still referenced is never overwritten by a later call (jm gh-437).

        The Python face of the loop. Each code period the early/prompt/late
        correlators dump, the power-domain non-coherent early-minus-late
        discriminator runs, and the fixed-point code-phase NCO is re-steered;
        the prompt correlator value is emitted as one output symbol per period
        (or `segments` partial prompts per period when `segments > 1`). The
        loop is carrier-blind — it tracks with a residual carrier still on the
        input, so carrier recovery (Costas) and symbol-timing recovery are
        downstream stages fed from this output. Returned blocks are block-size
        invariant and safe to keep across calls (a block still referenced is
        never overwritten, jm gh-437).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Carrier-wiped input samples (one contiguous block).
        out : NDArray[np.complex64] | None
            Output buffer for the emitted prompt symbols.

        Returns
        -------
        NDArray[np.complex64]
            Number of prompt symbols written — one per completed code period
            (`segments` per period when `segments > 1`) — up to max_out.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import Dll
        >>> rng = np.random.default_rng(1)
        >>> code = rng.integers(0, 2, 31).astype(np.uint8)
        >>> chip = np.where(code & 1, -1.0, 1.0)    # BPSK spreading code
        >>> x = np.tile(np.repeat(chip, 2), 40).astype(np.complex64)
        >>> d = Dll(code=code, sps=2)
        >>> sym = d.steps(x)                        # one prompt per period
        >>> sym.dtype
        dtype('complex64')
        >>> round(float(np.mean(sym.real[-10:])), 1)  # despread to a clean +1
        1.0
        >>> round(d.code_rate, 3)                   # locked at nominal rate
        1.0

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

    def set_telemetry(
        self,
        tlm: object | None,
        prefix: str,
        decim: int = 1,
    ) -> None:
        """Attach (or detach) a telemetry context and register the code loop's
        probes on it. Registers four probes, emitted once per code epoch
        (period) and further thinned by decim: "<prefix>.e" (the
        early-minus-late envelope discriminator — the loop stress),
        "<prefix>.rate" (the tracked code rate, chips advanced per nominal
        chip, ~1.0 at lock), "<prefix>.lock" (the CFAR lock statistic R;
        compare against the configured threshold) and "<prefix>.locked" (the
        verify-counted lock decision, 0/1 — the lockdet output, so a consumer
        sees where the declare/drop rule fired without re-deriving it from the
        statistic). Passing NULL detaches. Setup path, never hot: call before
        the producer thread starts stepping; the context is borrowed and must
        outlive the attachment (SPSC rules in dp_tlm/dp_tlm_core.h).

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "code" or "ch0.code".
        decim : int
            Emit every decim-th epoch; >= 1.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_telemetry failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import Dll
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> code = np.zeros(31, dtype=np.uint8)
        >>> d = Dll(code=code, sps=2)
        >>> d.set_telemetry(tlm, "code")
        >>> sorted(tlm.probe_names)
        ['code.e', 'code.lock', 'code.locked', 'code.rate']
        >>> x = np.ones(31 * 2 * 50, dtype=np.complex64)
        >>> _ = d.steps(x)
        >>> recs = tlm.read()   # four records per code epoch
        >>> len(recs) > 0 and len(recs) % 4 == 0
        True

        """

    def configure(self, bn: float, zeta: float) -> None:
        """Recompute the loop gains for a new (bn, zeta); preserves the code
        phase/rate.

        Re-derives the 2nd-order loop filter's proportional and integral gains
        for a new noise bandwidth and damping, leaving the tracked code phase,
        code rate and correlator accumulators untouched — retune the loop
        mid-run (e.g. narrow the bandwidth once pulled in) without dropping
        lock.

        Parameters
        ----------
        bn : float
            Loop noise bandwidth, normalised to the code-period rate.
        zeta : float
            Damping factor (0.707 = critically damped).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import Dll
        >>> rng = np.random.default_rng(1)
        >>> code = rng.integers(0, 2, 31).astype(np.uint8)
        >>> d = Dll(code=code, sps=2, bn=0.01)
        >>> d.configure(bn=0.02, zeta=0.707)   # widen the bandwidth mid-run
        >>> round(d.bn, 3)
        0.02

        """

    def set_rate_aid(self, rate_aid: float) -> None:
        """Set the carrier-aiding code-rate deviation (ratio; 0 = off): a fixed
        fractional rate bias summed into the code NCO's phase_inc every epoch,
        on top of the loop's own control. For physically-coupled Doppler, pass
        carrier_offset_hz / carrier_freq_hz so the code NCO rides the code-rate
        dilation the discriminator alone can't pull in at low SNR. Applied
        continuously across the epoch (not a phase pulse), and nudges the
        current phase_inc so the aid takes effect before the first period
        update. code_rate stays the loop's own observable and is unaffected.

        A fixed fractional rate bias summed into the sample-and-hold
        `phase_inc` on top of the loop's own control every epoch -- for
        physically-coupled Doppler, `carrier_offset_hz / carrier_freq_hz`, so
        the code NCO rides the code-rate dilation the discriminator alone can't
        pull in at low SNR. Applied continuously across the epoch (via
        `phase_inc`), not as a phase pulse. Also nudges the current `phase_inc`
        so the aid takes effect before the first period update. `code_rate`
        stays the loop's own observable and is unaffected.

        Parameters
        ----------
        rate_aid : float
            Fractional code-rate deviation (e.g. 8e-6). 0 disables.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import Dll
        >>> rng = np.random.default_rng(11)
        >>> code = rng.integers(0, 2, 63).astype(np.uint8)
        >>> delta = 5e-4                                   # code-rate Doppler
        >>> idx = (np.arange(63 * 4 * 300) * (1 + delta) / 4).astype(
        ...     np.int64) % 63
        >>> x = np.where(code[idx] & 1, -1.0, 1.0).astype(np.complex64)
        >>> plain = Dll(code, sps=4, bn=0.005)
        >>> _ = plain.steps(x)
        >>> round(plain.code_rate, 4)      # loop had to pull the whole Doppler
        1.0005
        >>> aided = Dll(code, sps=4, bn=0.005)
        >>> aided.set_rate_aid(delta)      # feed the Doppler forward instead
        >>> _ = aided.steps(x)
        >>> round(aided.code_rate, 4)      # loop integrator stays at nominal
        1.0

        """

    def configure_lock(
        self,
        pfa: float,
        n_looks: int,
        ref_snr_db: float = 0.0,
    ) -> None:
        """Tune the always-on code-lock detector to a target (pfa, n_looks).
        The detector reuses acquisition's non-coherent statistic R =
        sqrt(2*sum|P|^2 / E|O|^2), where the prompt powers of n_looks
        consecutive looks are summed and E|O|^2 is an EMA of a random off-peak
        (noise) correlation re-drawn each epoch; a decision compares R against
        det_threshold_noncoherent(pfa, n_looks). Size n_looks with
        detection.det_n_noncoh(snr, ...) for your operating C/N0. The EMA
        bandwidth is sized probabilistically (detection.det_ema_alpha):
        ref_snr_db sets the noise reference's estimator SNR (mean^2/variance of
        the EMA output); the default 0.0 derives it from n_looks so the
        reference's std stays an eighth of the statistic's intrinsic H0 spread,
        floored at ~33 dB. Decisions feed a verify-counted lock detector rather
        than a single-comparison latch: locked flips up only after
        det_verify_count(pfa, pfa*1e-3) consecutive above-threshold decisions
        (2 for the default pfa=1e-3, compounding the false-declare rate three
        decades under pfa) and drops only after 2 consecutive below-threshold
        decisions, so a statistic grazing the threshold cannot chatter the
        flag. The default config is pfa=1e-3 over 20 looks. Raises ValueError
        for pfa outside (0, 1). Read the result from the locked / lock_stat /
        noise_est properties.

        The DLL carries a lock detector that reuses acquisition's non-coherent
        test statistic. Every emitted look (a partial in segments mode, or the
        full-epoch prompt when segments == 1) is also correlated at a *random
        off-peak* code phase — re-drawn each epoch and kept `noise_guard` chips
        clear of the prompt/early/late lobe — to give a signal-free CFAR noise
        sample (valid for a low-sidelobe code, e.g. Gold). The offset power
        feeds an EMA reference `E|O|^2`; the prompt powers of n_looks
        consecutive looks are summed into `S = sum|P_k|^2`, and the detector
        declares lock when

        R = sqrt(2 * S / E|O|^2) > det_threshold_noncoherent(pfa, n_looks)

        which under H0 has `P(R > eta) = marcum_q(n_looks, 0, eta)`. Size
        n_looks with det_n_noncoh(snr, ...) for the operating C/N0.

        The noise-reference EMA bandwidth is sized probabilistically via
        det_ema_alpha(): the signal-free `|O|^2` samples are exponential (0 dB
        estimator SNR per sample — a DC level in fluctuation of equal power),
        and ref_snr_db chooses the EMA output's estimator SNR
        (mean^2/variance). Passing 0 derives it from n_looks: the reference's
        relative std is held to an eighth of the statistic's intrinsic H0
        spread (`1/sqrt(N)`), floored at ~33 dB — which reproduces the classic
        `1/alpha = max(1024, 32*N)` sizing exactly, now as a consequence
        instead of a constant.

        The detector needs an off-peak code phase to sample noise from: with a
        very short code (fewer than ~2*(spacing+2)+1 chips, i.e. sf <= 6 at the
        default spacing) no offset clears the prompt/early/late lobe, the noise
        tap aliases the prompt, and the statistic pins below threshold — locked
        stays 0 (fail-closed) no matter the signal. Use a code of >= 7 chips
        (real spreading codes are far longer) for a meaningful lock decision.

        The decision itself runs through an embedded lock detector
        (lockdet_core.h) rather than a single-comparison latch: `locked` flips
        up only after det_verify_count(pfa, pfa*1e-3) CONSECUTIVE
        above-threshold decisions (the false-declare budget held three decades
        under the per-decision pfa — 2 straight for the default 1e-3), and
        drops only after 2 straight below-threshold decisions, so a statistic
        grazing the threshold cannot chatter the flag. Full control of the
        verify counts and a split declare/drop threshold pair is C-only via
        dll_configure_lock_raw().

        Parameters
        ----------
        pfa : float
            Per-decision false-alarm probability, in (0, 1).
        n_looks : int
            Non-coherent integration depth N (looks); clamped >= 1.
        ref_snr_db : float
            Noise-reference estimator SNR in dB (> 0), or 0 to derive from
            n_looks as above.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``configure_lock failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import Dll
        >>> d = Dll(code=np.zeros(31, dtype=np.uint8), sps=2)
        >>> d.configure_lock(1e-3, 20)
        >>> d.locked
        False
        >>> d.configure_lock(1e-3, 20, ref_snr_db=20.0)   # ~50-look reference
        >>> d.configure_lock(2.0, 20)
        Traceback (most recent call last):
            ...
        ValueError: configure_lock failed (rc=-4)

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
        """Escape hatch under configure_lock() for direct control of the lock
        detector's geometry: a split declare/drop threshold pair on the
        statistic R (level hysteresis), the noise-EMA coefficient alpha, and
        both verify counts n_up/n_down (time hysteresis) independently --
        configure_lock() only ever derives a symmetric threshold (up_thresh ==
        down_thresh) and a fixed n_down=2. Re-tuning clears the in-flight
        statistic and drops the lock so the next decision uses only looks
        gathered under the new config. Size up_thresh/down_thresh with
        detection.det_threshold_noncoherent(pfa, n_looks), alpha with
        detection.det_ema_alpha, and n_up/n_down with
        detection.det_verify_count. Read the result from the locked / lock_stat
        / noise_est properties.

        The escape hatch under dll_configure_lock() for a composing C caller
        that derives its own threshold/EMA/hysteresis geometry — the full
        lockdet decision rule is exposed: a split declare/drop threshold pair
        (level hysteresis) and both verify counts (time hysteresis; size them
        with det_verify_count()). Re-tuning clears the in-flight statistic and
        drops the lock so the next decision uses only looks gathered under the
        new config.

        Parameters
        ----------
        up_thresh : float
            Declare threshold on the statistic R (e.g. the CFAR eta from
            det_threshold_noncoherent()).
        down_thresh : float
            Drop threshold on R; choose <= up_thresh for level hysteresis.
        n_looks : int
            Non-coherent integration depth N (looks); clamped >= 1.
        alpha : float
            EMA coefficient for the noise reference, in (0, 1].
        n_up : int
            Consecutive above-threshold decisions to declare lock; clamped to
            >= 1.
        n_down : int
            Consecutive below-threshold decisions to drop it; clamped to >= 1.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import Dll
        >>> rng = np.random.default_rng(1)
        >>> # >= 7 chips gives a usable lock statistic
        >>> code = rng.integers(0, 2, 63).astype(np.uint8)
        >>> chip = np.where(code & 1, -1.0, 1.0)
        >>> x = np.tile(np.repeat(chip, 4), 400).astype(np.complex64)
        >>> d = Dll(code, sps=4, bn=0.005)
        >>> # raw geometry: declare at R>3, drop at R<2.5, 8-look,
        >>> # 2-of-2 hysteresis
        >>> d.configure_lock_raw(3.0, 2.5, 8, 1.0 / 1024, 2, 2)
        >>> _ = d.steps(x)
        >>> d.locked                       # cleared the declare threshold
        True
        >>> bool(d.lock_stat > 3.0)
        True

        """

    def reset(self) -> None:
        """Re-seed the loop to the create-time code phase; preserve config.

        Restores the code phase, loop filter, correlator accumulators and lock
        detector to their post-construction state while preserving the tuned
        configuration (bn/zeta, spacing, segments, lock geometry). Re-running
        the same input after a reset therefore reproduces the same tracked
        state bit-for-bit — the basis of a deterministic replay.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import Dll
        >>> rng = np.random.default_rng(21)
        >>> code = rng.integers(0, 2, 63).astype(np.uint8)
        >>> idx = (np.arange(63 * 4 * 300) * (1 + 3e-4) / 4).astype(
        ...     np.int64) % 63
        >>> x = np.where(code[idx] & 1, -1.0, 1.0).astype(np.complex64)
        >>> d = Dll(code, sps=4, bn=0.005)
        >>> _ = d.steps(x)
        >>> first = round(d.code_rate, 6)
        >>> d.reset()                     # back to the create-time code phase
        >>> _ = d.steps(x)                # same input -> same tracked rate
        >>> round(d.code_rate, 6) == first
        True

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the Dll has already been destroyed.

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

        Raises ``RuntimeError`` if the Dll has already been destroyed.

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
        ``RuntimeError`` if the Dll has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def bn(self) -> float:
        """loop noise bandwidth (retained)."""
    @bn.setter
    def bn(self, value: float) -> None: ...

    @property
    def code_phase(self) -> float:
        """Code phase."""

    @property
    def code_rate(self) -> float:
        """chips advanced per nominal chip (~1.0)."""

    @property
    def last_error(self) -> float:
        """last discriminator output (loop stress)."""

    @property
    def segments(self) -> int:
        """partial correlations per epoch (1 = full)."""

    @property
    def locked(self) -> bool:
        """Current lock decision: True after the verify count of consecutive
        above-threshold N-look decisions, False again after the drop count of
        consecutive below-threshold ones (see configure_lock).
        """

    @property
    def lock_stat(self) -> float:
        """Last code-lock test statistic R = sqrt(2*sum|P|^2 / E|O|^2); compare
        against det_threshold_noncoherent(pfa, n_looks).
        """

    @property
    def noise_est(self) -> float:
        """Current CFAR noise-power estimate E|O|^2 from the off-peak (noise)
        tap EMA.
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


    def __enter__(self) -> "Dll":
        """Enter a context manager, returning this object.

        Lets a Dll be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Dll
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the Dll.

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
class SymbolSync:
    """SymbolSync component.

    Parameters
    ----------
    sps : int, default 4
        sps constructor parameter.
    bn : float, default 0.01
        bn constructor parameter.
    zeta : float, default 0.707
        zeta constructor parameter.
    order : Literal["linear", "parabolic", "cubic"], default "cubic"
        order constructor parameter.
    ted : Literal["gardner", "dttl"], default "gardner"
        Timing-error detector: "gardner" (blind, works for any constellation)
        or "dttl" (decision-directed sign-sign Data Transition Tracking Loop;
        lower self-noise near lock but degrades faster at low SNR. BPSK/QPSK
        only -- invalid for 8PSK/QAM).

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import SymbolSync
    >>> obj = SymbolSync(
    ...     sps=4,
    ...     bn=0.01,
    ...     zeta=0.707,
    ...     order="cubic",
    ...     ted="gardner",
    ... )

    """
    def __init__(
        self,
        sps: int = ...,
        bn: float = ...,
        zeta: float = ...,
        order: Literal["linear", "parabolic", "cubic"] = "cubic",
        ted: Literal["gardner", "dttl"] = "gardner",
    ) -> None: ...

    def steps(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Recover symbol timing from an oversampled cf32 baseband block: a
        timing-error detector (Gardner or DTTL, see the `ted` param) drives an
        integer timing NCO whose post-wrap value gives the interpolation
        fraction for free, and a Farrow interpolator emits one symbol-rate
        sample per recovered symbol instant.

        symsync_step() in a loop, with the TED specialised per detector. Each
        input sample feeds the Farrow interpolator and advances the integer
        timing NCO; on a mid-symbol crossing the transition-gate interpolant is
        stored, and on a wrap the on-time interpolant is formed, the selected
        TED (Gardner or DTTL) measures the timing error, the PI loop steers the
        NCO rate, and one symbol-rate sample is emitted at the recovered
        instant. State carries across calls, so contiguous blocks give the same
        symbols as one large block.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Oversampled input samples (~sps samples per symbol).
        out : NDArray[np.complex64] | None
            Recovered symbol-rate samples.

        Returns
        -------
        NDArray[np.complex64]
            Number of recovered symbols written to out.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import SymbolSync
        >>> ss = SymbolSync(sps=4, bn=0.02, zeta=0.707)
        >>> x = np.repeat([1.0, -1.0, 1.0, -1.0], 4 * 32).astype(np.complex64)
        >>> y = ss.steps(x)             # oversampled -> one sample/symbol
        >>> y.shape[0]
        127
        >>> sorted(set(np.where(y.real >= 0, 1, -1).tolist()))  # got +/-1
        [-1, 1]
        >>> round(ss.rate, 1)              # tracked samples/symbol
        4.0

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

    def set_telemetry(
        self,
        tlm: object | None,
        prefix: str,
        decim: int = 1,
    ) -> None:
        """Attach (or detach) a telemetry context and register the timing
        loop's probes on it. Registers five probes, emitted once per recovered
        symbol and further thinned by decim: "<prefix>.e" (the normalised TED
        error — the loop stress), "<prefix>.freq" (the loop-filter control
        steering the timing NCO, fractional rate offset), "<prefix>.rate" (the
        smoothed tracked samples/symbol), "<prefix>.lock" (the last
        block-averaged lock_signal, held between avgs-look updates) and
        "<prefix>.locked" (the verify-counted lockdet decision, 0/1). Passing
        NULL detaches. Setup path, never hot: call before the producer thread
        starts stepping; the context is borrowed and must outlive the
        attachment (SPSC rules in dp_tlm/dp_tlm_core.h).

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "sync" or "rx.sync".
        decim : int
            Emit every decim-th symbol; >= 1.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_telemetry failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import SymbolSync
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> ss = SymbolSync(sps=4, bn=0.01, zeta=0.707)
        >>> ss.set_telemetry(tlm, "sync")
        >>> sorted(tlm.probe_names)
        ['sync.e', 'sync.freq', 'sync.lock', 'sync.locked', 'sync.rate']
        >>> x = np.repeat([1 + 1j, -1 - 1j], 4 * 64).astype(np.complex64)
        >>> _ = ss.steps(x)
        >>> recs = tlm.read()   # five records per recovered symbol
        >>> len(recs) > 0 and len(recs) % 5 == 0
        True

        """

    def configure(self, bn: float, zeta: float) -> None:
        """Recompute the loop gains for a new (bn, zeta); preserve the timing
        estimate.

        Retunes the PI timing loop in place: the proportional/integral gains
        are recomputed from the new noise bandwidth and damping, while the NCO
        phase, tracked rate and loop-filter integrator carry over — so a locked
        loop is re-bandwidthed (e.g. narrowed after acquisition) without losing
        lock.

        Parameters
        ----------
        bn : float
            Loop noise bandwidth, normalised to the symbol rate (>= 0).
        zeta : float
            Damping factor (0.707 = critically damped).

        Examples
        --------
        >>> from doppler.track import SymbolSync
        >>> ss = SymbolSync(sps=4, bn=0.01, zeta=0.707)
        >>> ss.configure(bn=0.05, zeta=1.0)   # widen + over-damp, to acquire
        >>> round(ss.bn, 3)
        0.05

        """

    def configure_lock(
        self,
        rolloff: float,
        esno_min_db: float,
        pfa: float,
        pd: float,
    ) -> None:
        """Tune the always-on timing-lock detector to a target (pfa, pd) at a
        given link operating point. The statistic is a Gardner-style
        eye-opening ratio, lock_signal =
        2*(|on-time|^2-|mid-symbol|^2)/(|on-time|^2+|mid-symbol|^2),
        non-coherently block-averaged over avgs looks before each decision
        (mirroring Dll's tumbling-window CFAR pattern). avgs and the declare
        threshold are sized from a Gaussian approximation: a per-look mean is
        estimated from rolloff and esno_min_db, then the classic N =
        variance*((Q^-1(pfa)-Q^-1(pd))/mean)^2 / threshold =
        Q^-1(pfa)*mean/(Q^-1(pfa)-Q^-1(pd)) derivation gives (avgs, threshold).
        No level hysteresis by default (up=down=threshold, matching
        Dll.configure_lock's shape); n_up=1, n_down=8. Raises ValueError if
        pfa/pd are outside (0, 1) or pd does not exceed pfa. Read the result
        from the locked / lock_stat properties.

        Sizes the non-coherent block size (avgs) and declare threshold from a
        Gaussian sizing of the eye-opening statistic lock_signal =
        2*(|on-time|^2-|mid|^2)/(|on-time|^2+|mid|^2): a per-look mean
        (mean_lock_detect, from rolloff and the minimum operating Es/N0) drives
        the classic N = variance*((Q^-1(pfa)-Q^-1(pd))/mean)^2 / threshold =
        Q^-1(pfa)*mean/(Q^-1(pfa)-Q^-1(pd)) derivation, implemented directly
        from a formula supplied by a doppler user (not re-derived against a
        primary source), with "variance" set from a direct measurement of
        lock_signal's real per-look variance under noise (~1.343,
        5,000,000-sample Monte Carlo) rather than the placeholder "8" this API
        originally shipped with -- see symsync_core.c's
        SYMSYNC_LOCK_STAT_VARIANCE comment for the full derivation (a
        factor-of-2 correction for the erfcinv-vs-Q^-1 convention applies on
        top of the measured variance; the two hypotheses were empirically
        compared before picking one). Empirically validated at the default
        operating point (avgs=133, threshold=0.311): 429 false declares over
        500,000 independent noise-only blocks against a nominal pfa=1e-3
        (8.58e-4, correctly sized with safe margin, not accidentally
        oversized); 2000/2000 true declares at the esno_min design SNR against
        a nominal pd=0.9 -- see native/validation/symsync_lock.c for the
        harness. No level hysteresis by default (up = down = threshold,
        matching dll_configure_lock's shape); the raw escape hatch
        (symsync_configure_lock_raw) exposes split thresholds, an explicit
        avgs, and independent n_up/n_down.

        Parameters
        ----------
        rolloff : float
            Matched-filter excess bandwidth (e.g. 0.35 for a typical RRC
            system).
        esno_min_db : float
            Minimum operating Es/N0, dB -- the worst-case link point the
            detector must still declare lock at.
        pfa : float
            Target false-alarm probability per decision, in (0, 1).
        pd : float
            Target detection probability per decision, in (0, 1); must exceed
            pfa.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``configure_lock failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> from doppler.track import SymbolSync
        >>> ss = SymbolSync(sps=4, bn=0.01, zeta=0.707)
        >>> ss.configure_lock(rolloff=0.35, esno_min_db=10.0, pfa=1e-3, pd=0.9)
        >>> ss.locked
        False
        >>> ss.configure_lock(rolloff=0.35, esno_min_db=10.0, pfa=0.9, pd=0.9)
        Traceback (most recent call last):
            ...
        ValueError: configure_lock failed (rc=-4)

        """

    def configure_lock_raw(
        self,
        avgs: int,
        up_thresh: float,
        down_thresh: float,
        n_up: int,
        n_down: int,
    ) -> None:
        """Escape hatch under configure_lock() for direct control of the lock
        detector's geometry: an explicit non-coherent block size (avgs), a
        split declare/drop threshold pair on lock_stat (level hysteresis), and
        both verify counts (time hysteresis) independently. Re-tuning clears
        the in-flight block sum and drops the lock so the next decision uses
        only looks gathered under the new config.

        The escape hatch under symsync_configure_lock() for a caller that
        derives its own averaging/threshold geometry: the block size (avgs), a
        split declare/drop threshold pair on lock_stat (level hysteresis), and
        both verify counts (time hysteresis). Re-tuning clears the in-flight
        block sum and drops the lock so the next decision uses only looks
        gathered under the new config.

        Parameters
        ----------
        avgs : int
            Non-coherent block size (looks/decision); clamped >= 1.
        up_thresh : float
            Declare threshold on lock_stat.
        down_thresh : float
            Drop threshold; choose <= up_thresh for level hysteresis.
        n_up : int
            Consecutive above-threshold decisions to declare; clamped >= 1.
        n_down : int
            Consecutive below-threshold decisions to drop; clamped >= 1.

        Examples
        --------
        >>> from doppler.track import SymbolSync
        >>> ss = SymbolSync(sps=4, bn=0.01, zeta=0.707)
        >>> ss.configure_lock_raw(64, 0.3, 0.3, 1, 8)   # 64-look block, 8-drop
        >>> ss.locked
        False
        >>> round(ss.lock_stat, 3)
        0.0

        """

    def reset(self) -> None:
        """Re-seed the timing loop to its nominal rate and zero phase.

        Restores the object to its post-create state: the timing NCO is zeroed
        to the nominal one-wrap-per-symbol rate, the Farrow history and TED
        state are cleared, the loop-filter integrator is emptied and the lock
        detector is dropped. The configured (bn, zeta), TED selection and any
        lock geometry are preserved, so the same object can be re-run on a
        fresh stream.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import SymbolSync
        >>> ss = SymbolSync(sps=4, bn=0.02, zeta=0.707)
        >>> _ = ss.steps(np.repeat([1.0, -1.0], 4 * 40).astype(np.complex64))
        >>> ss.reset()
        >>> round(ss.rate, 1)              # back to the nominal sps
        4.0
        >>> round(ss.timing_error, 3)      # loop stress cleared
        0.0

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the SymbolSync has already been destroyed.

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

        Raises ``RuntimeError`` if the SymbolSync has already been destroyed.

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
        ``RuntimeError`` if the SymbolSync has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def bn(self) -> float:
        """loop noise bandwidth (retained)."""
    @bn.setter
    def bn(self, value: float) -> None: ...

    @property
    def timing_error(self) -> float:
        """Timing error."""

    @property
    def rate(self) -> float:
        """Rate."""

    @property
    def lock_stat(self) -> float:
        """Last block-averaged lock statistic:
        mean(2*(|on-time|^2-|mid-symbol|^2)/(|on-time|^2+|mid-symbol|^2)) over
        the configured avgs looks; compare against the configured threshold
        (see configure_lock).
        """

    @property
    def locked(self) -> bool:
        """Current timing-lock decision: True after the verify count of
        consecutive above-threshold decisions, False again after the drop count
        of consecutive below-threshold ones (see configure_lock).
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


    def __enter__(self) -> "SymbolSync":
        """Enter a context manager, returning this object.

        Lets a SymbolSync be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        SymbolSync
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the SymbolSync.

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
class RateSync:
    """Create a RateSync instance.

    Parameters
    ----------
    sps : float, default 4.0
        Nominal samples per symbol. Any double >= `m` -- 17.33389 is as valid
        as 4, because the terminal stage's accumulator is a double and the loop
        only has to steer the strobe. That is the real-world case whenever the
        ADC clock is free-running against the symbol clock.
    pulse : Literal["iandd", "rrc"], default "rrc"
        Matched-filter pulse shape: "rrc" (root-raised cosine, roll-off `beta`)
        or "iandd" (unit rectangle one symbol wide -- the matched filter for a
        rectangular symbol, and exactly what an integrate-and-dump computes).
        The rectangle needs far fewer taps, so an NRZ link's matched filter is
        cheaper.
    beta : float, default 0.35
        RRC roll-off in `[0, 1]` (ignored for the rectangle).
    span : int, default 8
        One-sided RRC span in symbols (ignored for the rectangle, whose support
        is always one symbol).
    m : int, default 2
        Terminal outputs per symbol: even, 2 <= m <= 8. Gardner needs a
        transition gate half a symbol from the on-time strobe, which is why m
        must be even and at least 2. The oversampled stream is a by-product of
        the same dot products, not an extra cost. Use m >= 4 with
        pulse="iandd": the rectangle is one symbol wide, so at m=2 its matched
        filter is a 2-tap sum and the eye statistic barely opens. Measured on
        an NRZ stream, m=2 does not clear the lock detector's own declare
        threshold while m=4 clears it comfortably, with tens of dB of EVM
        between them. The rule rests on that separation, not on a particular
        pair of lock_stat values -- those move with sps and with the stream.
        The RRC spans many symbols and is unaffected.
    num_phases : int, default 1024
        Matched-filter arms; a power of two. Sets the fractional-timing
        resolution to 1/num_phases of an output period.
    bn : float, default 0.01
        Loop noise bandwidth, normalised to the symbol rate.
    zeta : float, default 0.707
        Damping factor (0.707 = critically damped).
    ted : Literal["gardner", "dttl"], default "gardner"
        Timing-error detector: "gardner" (blind, works for any constellation)
        or "dttl" (decision-directed sign-sign Data Transition Tracking Loop;
        lower self-noise near lock but degrades faster at low SNR. BPSK/QPSK
        only -- invalid for 8PSK/QAM).

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``RateSync: invalid
        parameter (need sps >= m, 0 <= beta <= 1, span >= 1, m even in [2, 8],
        num_phases a power of two >= 2, bn >= 0, zeta > 0)``.

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import RateSync
    >>> obj = RateSync(
    ...     sps=4.0,
    ...     pulse="rrc",
    ...     beta=0.35,
    ...     span=8,
    ...     m=2,
    ...     num_phases=1024,
    ...     bn=0.01,
    ...     zeta=0.707,
    ...     ted="gardner",
    ... )

    """
    def __init__(
        self,
        sps: float = ...,
        pulse: Literal["iandd", "rrc"] = "rrc",
        beta: float = ...,
        span: int = ...,
        m: int = ...,
        num_phases: int = ...,
        bn: float = ...,
        zeta: float = ...,
        ted: Literal["gardner", "dttl"] = "gardner",
    ) -> None: ...

    def steps(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Recover symbols from an oversampled cf32 baseband block. The owned
        RateConverter's terminal stage IS the matched filter, and the polyphase
        arm its accumulator selects IS the fractional timing delay, so one dot
        product does the rate conversion, the matched filtering and the
        interpolation. Every m-th output is an on-time strobe and the output
        m/2 back is the transition gate; a Gardner or DTTL detector drives a PI
        loop that steers the terminal stage's control port. State carries
        across calls, so contiguous blocks give the same symbols as one large
        block.

        ratesync_step() in a loop, with the TED specialised per detector; state
        carries across calls, so contiguous blocks give the same symbols as one
        large block.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input samples.
        out : NDArray[np.complex64] | None
            Recovered symbols.

        Returns
        -------
        NDArray[np.complex64]
            Symbols written to out.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import RateSync
        >>> syms = np.where(np.random.default_rng(3).integers(0, 2, 3000) > 0,
        ...                 1.0, -1.0)
        >>> x = (0.25 * np.repeat(syms, 8)).astype(np.complex64)  # 8 samp/sym
        >>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.01)
        >>> y = rs.steps(x)             # one symbol per transmitted symbol
        >>> round(rs.rate, 2)           # tracked samples per symbol
        8.0
        >>> bool(rs.lock_stat > 0.55)   # the timing loop has locked
        True

        """

    def steps_max_out(self) -> int:
        """Output-buffer hint for the generated binding; 0 means "the input
        length is already a safe bound" — with `sps >= m >= 2` a block can
        never yield more symbols than it has samples (mirrors symsync).

        Returns
        -------
        int
            Output.
        """

    def set_telemetry(
        self,
        tlm: object | None,
        prefix: str,
        decim: int = 1,
    ) -> None:
        """Attach (or detach) a telemetry context and register the probes.

        Registers six probes, emitted once per recovered symbol and further
        thinned by decim: "<prefix>.e" (normalised TED error), "<prefix>.ctrl"
        (the per-input control steering the strobe), "<prefix>.rate" (tracked
        samples/symbol), "<prefix>.lock" (last block-averaged lock_signal),
        "<prefix>.locked" (0/1) and "<prefix>.mu" (the timing NCO's fractional
        phase — see resamp_get_ctrl_acc()). Passing NULL detaches. Setup path,
        never hot: the context is borrowed and must outlive the attachment
        (SPSC rules in dp_tlm/dp_tlm_core.h).

        The three form one readable picture of the loop: `e` is what the
        detector saw, `ctrl` is what the filter did about it, and `mu` is where
        the sampling instant ended up as a result — the only one of the three
        that is a physical position rather than a correction.

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "sync".
        decim : int
            Emit every decim-th symbol; >= 1.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_telemetry failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> from doppler.track import RateSync
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 14)
        >>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.01)
        >>> rs.set_telemetry(tlm, "sync")   # register the six timing probes
        >>> tlm.probe_count
        6
        >>> "sync.rate" in tlm.probe_names   # tracked samples/symbol
        True

        """

    def configure(self, bn: float, zeta: float) -> None:
        """Recompute the loop gains for a new (bn, zeta); preserve the timing
        estimate.

        Only the PI coefficients change; the integrator, and therefore the
        tracked rate and the lock, carries through untouched. Use it to narrow
        the loop after acquisition (a wide bn pulls in fast, a narrow one
        tracks with less jitter) without forcing a re-acquire.

        Parameters
        ----------
        bn : float
            Loop noise bandwidth, normalised to the symbol rate.
        zeta : float
            Damping factor (0.707 = critically damped).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import RateSync
        >>> syms = np.where(np.random.default_rng(3).integers(0, 2, 3000) > 0,
        ...                 1.0, -1.0)
        >>> x = (0.25 * np.repeat(syms, 8)).astype(np.complex64)  # 8 samp/sym
        >>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.01)
        >>> _ = rs.steps(x)              # acquire and lock
        >>> rs.locked
        True
        >>> rs.configure(0.002, 0.707)   # narrow the loop; lock is kept
        >>> round(rs.bn, 3)
        0.002
        >>> rs.locked
        True

        """

    def configure_lock_raw(
        self,
        avgs: int,
        up_thresh: float,
        down_thresh: float,
        n_up: int,
        n_down: int,
    ) -> None:
        """Direct control of the lock detector's geometry: an explicit
        non-coherent block size (avgs), a split declare/drop threshold pair on
        lock_stat (level hysteresis), and both verify counts (time hysteresis)
        independently. Re-tuning clears the in-flight block sum and drops the
        lock so the next decision uses only looks gathered under the new
        config. The (pfa, pd) sizing entry point symsync exposes is
        deliberately not mirrored here: its constants were calibrated against
        symsync's own geometry by Monte Carlo, and re-exposing the formula for
        a different front end without repeating that validation would assert a
        calibration nobody measured.

        The block size (avgs), a split declare/drop threshold pair on lock_stat
        (level hysteresis) and both verify counts (time hysteresis). Re-tuning
        clears the in-flight block sum and drops the lock, so the next decision
        uses only looks gathered under the new config.

        Parameters
        ----------
        avgs : int
            Looks per decision; clamped >= 1.
        up_thresh : float
            Declare threshold on lock_stat.
        down_thresh : float
            Drop threshold; <= up_thresh for level hysteresis.
        n_up : int
            Consecutive above-threshold decisions to declare.
        n_down : int
            Consecutive below-threshold decisions to drop.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import RateSync
        >>> syms = np.where(np.random.default_rng(3).integers(0, 2, 3000) > 0,
        ...                 1.0, -1.0)
        >>> x = (0.25 * np.repeat(syms, 8)).astype(np.complex64)
        >>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.01)
        >>> _ = rs.steps(x)
        >>> rs.locked
        True
        >>> rs.configure_lock_raw(64, 0.5, 0.4, 2, 4)  # drops the lock
        >>> rs.locked
        False
        >>> rs.lock_stat                 # the in-flight block was cleared
        0.0

        """

    def reset(self) -> None:
        """Re-seed the timing loop, the cascade's filter memories, the strobe
        ring and the prime countdown.

        Configuration (sps, pulse, bank, bn, zeta, ted, lock geometry) is kept;
        only the running state is cleared, so a re-run of the same stream from
        a reset object reproduces its first-run symbols bit for bit.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import RateSync
        >>> syms = np.where(np.random.default_rng(3).integers(0, 2, 3000) > 0,
        ...                 1.0, -1.0)
        >>> x = (0.25 * np.repeat(syms, 8)).astype(np.complex64)
        >>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.01)
        >>> first = np.array(rs.steps(x))
        >>> rs.reset()
        >>> rs.ctrl, rs.locked           # back to the post-create state
        (0.0, False)
        >>> bool(np.array_equal(first, np.array(rs.steps(x))))  # reproducible
        True

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the RateSync has already been destroyed.

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

        Raises ``RuntimeError`` if the RateSync has already been destroyed.

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
        ``RuntimeError`` if the RateSync has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def bn(self) -> float:
        """loop noise bandwidth (retained)."""
    @bn.setter
    def bn(self, value: float) -> None: ...

    @property
    def timing_error(self) -> float:
        """Last normalised TED error — the loop stress."""

    @property
    def rate(self) -> float:
        """Smoothed tracked samples per symbol. Departs from the nominal `sps`
        by exactly the sample-clock offset being tracked, so it is the
        estimator a rate-disciplining caller reads.
        """

    @property
    def ctrl(self) -> float:
        """Current per-input rate deviation steering the terminal stage's
        accumulator.
        """

    @property
    def lock_stat(self) -> float:
        """Last block-averaged lock statistic:
        mean(2*(|on-time|^2-|mid|^2)/(|on-time|^2+|mid|^2)) over the configured
        avgs looks. This, not an error-vector magnitude, is the honest lock
        indicator -- a single cycle slip during acquisition drags a windowed
        EVM by 20 dB while the eye stays wide open at +0.75.
        """

    @property
    def locked(self) -> bool:
        """Current timing-lock decision: True after the verify count of
        consecutive above-threshold decisions, False again after the drop count
        of consecutive below-threshold ones.
        """

    @property
    def clipped(self) -> bool:
        """True if the cascade's CIC stage has clipped its input since the last
        reset(). A CIC bounds its input to +-1.0 and clips silently past that,
        which no timing metric reveals -- an overdriven front end degrades EVM
        by 25 dB with a perfectly healthy lock. Always False when the plan
        contains no CIC stage.
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


    def __enter__(self) -> "RateSync":
        """Enter a context manager, returning this object.

        Lets a RateSync be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        RateSync
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the RateSync.

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
class CarrierMpsk:
    """Create an M-PSK carrier loop instance.

    Parameters
    ----------
    bn : float, default 0.05
        Loop noise bandwidth (default 0.05).
    zeta : float, default 0.707
        Damping factor (default 0.707).
    init_norm_freq : float, default 0.0
        Seed carrier frequency, cycles/sample (default 0.0).
    tsamps : int, default 64
        Samples per symbol (default 64).
    bn_fll : float, default 0.0
        FLL-assist bandwidth (default 0.0 = pure PLL).
    m : int, default 4
        Constellation order M, 2/4/8 (default 4 = QPSK).

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import CarrierMpsk
    >>> obj = CarrierMpsk(
    ...     bn=0.05,
    ...     zeta=0.707,
    ...     init_norm_freq=0.0,
    ...     tsamps=64,
    ...     bn_fll=0.0,
    ...     m=4,
    ... )

    """
    def __init__(
        self,
        bn: float = ...,
        zeta: float = ...,
        init_norm_freq: float = ...,
        tsamps: int = ...,
        bn_fll: float = ...,
        m: int = ...,
    ) -> None: ...

    def steps(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """De-rotate a cf32 block with the integer-NCO carrier, coherently
        integrate over each tsamps-sample symbol, run the decision-directed
        M-PSK discriminator (slice to the nearest constellation point, error
        Im(P*conj(ahat))/|P|), and emit one complex prompt symbol per symbol.
        The loop tracks a small residual carrier (bulk Doppler removed
        upstream); it locks to one of m phases, so resolve the M-fold ambiguity
        downstream (mpsk_diff_demap or a sync word). At m=2 this is exactly the
        BPSK Costas loop.

        The block form of the inline wipeoff/update pair: for each input sample
        it de-rotates by the carrier NCO and accumulates the coherent
        integrate-and-dump; every tsamps samples it dumps the prompt, runs the
        decision-directed M-PSK discriminator (slice to the nearest
        constellation point, error `Im(P conj(ahat))/|P|`, plus the optional
        cross-product FLL assist), filters the error, and steers the NCO
        frequency and phase. Exactly one de-rotated prompt is emitted per
        completed symbol; a trailing partial symbol is carried in the
        accumulator to the next call, so a stream can be fed in blocks of any
        length with no seam.

        The loop locks to one of m carrier phases — an M-fold ambiguity on the
        absolute constellation orientation. Resolve it downstream (differential
        demapping or a sync word); this call only recovers the carrier and
        returns the prompts. At m = 2 it is exactly the BPSK Costas loop.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input block, one complex baseband sample per element.
        out : NDArray[np.complex64] | None
            Prompt output buffer written by the binding.

        Returns
        -------
        NDArray[np.complex64]
            One de-rotated prompt symbol per completed integrate-and-dump
            period; the count is `x_len / tsamps`.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.mpsk import mpsk_map
        >>> from doppler.track import CarrierMpsk
        >>> rng = np.random.default_rng(0)
        >>> sps = 16
        >>> labels = rng.integers(0, 4, 400).astype(np.uint8)
        >>> sig = np.repeat(mpsk_map(labels, 4), sps).astype(np.complex64)
        >>> k = np.arange(len(sig))
        >>> rx = (sig * np.exp(2j * np.pi * 0.002 * k)).astype(np.complex64)
        >>> c = CarrierMpsk(bn=0.04, zeta=0.707, init_norm_freq=0.0,
        ...                 tsamps=sps, bn_fll=0.02, m=4)
        >>> prompts = c.steps(rx)          # one prompt per symbol
        >>> prompts.shape
        (400,)
        >>> round(c.norm_freq, 4)       # tracked the residual carrier 0.002
        0.002
        >>> round(c.lock_metric, 2)        # decision-aligned lock metric -> 1
        1.0

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

    def configure(self, bn: float, zeta: float) -> None:
        """Recompute the loop gains for a new (bn, zeta); preserves the
        frequency/phase estimate.

        Re-derives the proportional/integral gains of the embedded 2nd-order
        loop filter for the new noise bandwidth and damping, leaving the
        running frequency and phase estimate (the NCO and the loop integrator)
        untouched — a live lock survives a re-tune. Use it to widen the loop
        for fast pull-in and then narrow it for low-jitter tracking,
        mid-stream.

        Parameters
        ----------
        bn : float
            Loop noise bandwidth, normalised to the symbol rate.
        zeta : float
            Damping factor (0.707 = critically damped).

        Examples
        --------
        >>> from doppler.track import CarrierMpsk
        >>> c = CarrierMpsk(bn=0.02, zeta=0.707, init_norm_freq=0.01,
        ...                 tsamps=16, bn_fll=0.0, m=4)
        >>> round(c.bn, 3)
        0.02
        >>> c.configure(bn=0.05, zeta=1.0)   # widen the loop mid-stream
        >>> round(c.bn, 3)
        0.05
        >>> round(c.norm_freq, 3)            # frequency estimate preserved
        0.01

        """

    def reset(self) -> None:
        """Re-seed the loop to the create-time frequency/phase; preserve
        config.

        Returns the NCO to the seed carrier passed at construction, zeroes the
        integrate-and-dump accumulator, the FLL history, and the lock/error
        diagnostics, and re-primes the loop integrator to the matching
        per-symbol frequency — the exact state a fresh carrier_mpsk_create()
        leaves. The tuning (bn, zeta, bn_fll, tsamps, m) is untouched. Call it
        at a capture boundary so a lock reached on one segment does not bias an
        unrelated next one.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.mpsk import mpsk_map
        >>> from doppler.track import CarrierMpsk
        >>> rng = np.random.default_rng(1)
        >>> sig = np.repeat(
        ...     mpsk_map(rng.integers(0, 4, 100).astype(np.uint8), 4),
        ...                 16).astype(np.complex64)
        >>> rx = (sig * np.exp(2j * np.pi * 0.003 * np.arange(len(sig)))
        ...       ).astype(np.complex64)
        >>> c = CarrierMpsk(bn=0.04, zeta=0.707, init_norm_freq=0.0,
        ...                 tsamps=16, bn_fll=0.02, m=4)
        >>> _ = c.steps(rx)
        >>> round(c.norm_freq, 3)   # loop pulled onto the residual carrier
        0.003
        >>> c.reset()               # back to the create-time seed
        >>> round(c.norm_freq, 3)
        0.0

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the CarrierMpsk has already been destroyed.

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

        Raises ``RuntimeError`` if the CarrierMpsk has already been destroyed.

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
        ``RuntimeError`` if the CarrierMpsk has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def bn(self) -> float:
        """PLL loop noise bandwidth (retained)."""
    @bn.setter
    def bn(self, value: float) -> None: ...

    @property
    def norm_freq(self) -> float:
        """Norm freq."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def lock_metric(self) -> float:
        """EMA of Re(P conj a)/|P| (1 = locked)."""

    @property
    def last_error(self) -> float:
        """last PLL discriminator (loop stress)."""

    @property
    def bn_fll(self) -> float:
        """FLL-assist bandwidth (0 = pure PLL)."""
    @bn_fll.setter
    def bn_fll(self, value: float) -> None: ...

    @property
    def m(self) -> int:
        """constellation order M (2, 4, 8)."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "CarrierMpsk":
        """Enter a context manager, returning this object.

        Lets a CarrierMpsk be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        CarrierMpsk
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the CarrierMpsk.

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
class CarrierNda:
    """Create an NDA carrier loop instance.

    Parameters
    ----------
    bn : float, default 0.01
        Loop noise bandwidth (default 0.01).
    zeta : float, default 0.707
        Damping factor (default 0.707).
    init_norm_freq : float, default 0.0
        Seed carrier frequency, cycles/sample (default 0.0).
    sps : int, default 8
        Samples per symbol (default 8).
    n : int, default 4
        MA window divisor: window = sps/n (default 4; sps%n==0).
    m : int, default 4
        Constellation order M, 2/4/8 (default 4 = QPSK).

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import CarrierNda
    >>> obj = CarrierNda(
    ...     bn=0.01,
    ...     zeta=0.707,
    ...     init_norm_freq=0.0,
    ...     sps=8,
    ...     n=4,
    ...     m=4,
    ... )

    """
    def __init__(
        self,
        bn: float = ...,
        zeta: float = ...,
        init_norm_freq: float = ...,
        sps: int = ...,
        n: int = ...,
        m: int = ...,
    ) -> None: ...

    def steps(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """De-rotate a cf32 block with the integer-NCO carrier and return the
        de-rotated samples (one per input sample). Internally the loop runs a
        non-data-aided M-th-power discriminator on an I/Q arm
        integrate-and-dump at n dumps per symbol and steers the NCO, so it
        acquires the carrier with no symbol timing and no data present (it
        strips the M-PSK modulation by raising the arm sample to the Mth
        power). It locks to one of m phases (M-fold ambiguity), resolved
        downstream. Read norm_freq for the tracked carrier and lock for the
        carrier lock metric.

        Runs the non-data-aided carrier loop over the block: each sample is
        wiped off by the integer-phase NCO, the de-rotated sample slides the
        I/Q moving-average arm, and the M-th-power discriminator (which strips
        the M-PSK data modulation) steers the NCO frequency and phase. Because
        the discriminator is data- and timing-independent, this acquires the
        carrier with no symbol timing and no data present — a bare carrier, or
        a modulated carrier before timing lock. It resolves to one of m carrier
        phases (M-fold ambiguity, resolved downstream). Read norm_freq for the
        tracked carrier (cycles/sample) and lock for the carrier lock metric.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input samples (average power at or below unity).
        out : NDArray[np.complex64] | None
            De-rotated samples, one per input.

        Returns
        -------
        NDArray[np.complex64]
            Number of de-rotated samples written to out (equals x_len).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import CarrierNda
        >>> c = CarrierNda(bn=0.01, zeta=0.707, init_norm_freq=0.0,
        ...                sps=8, n=4, m=4)
        >>> rng = np.random.default_rng(0)
        >>> k = np.arange(40000)
        >>> x = (np.exp(2j * np.pi * 0.001 * k) + 0.05 * (
        ...      rng.standard_normal(k.size)
        ...      + 1j * rng.standard_normal(k.size))).astype(np.complex64)
        >>> y = c.steps(x)                 # de-rotated toward DC
        >>> y.shape[0]
        40000
        >>> round(c.norm_freq, 4)          # tracked carrier, cycles/sample
        0.001
        >>> c.lock > 0.5                    # carrier lock metric, ~1 at lock
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

    def set_telemetry(
        self,
        tlm: object | None,
        prefix: str,
        decim: int = 1,
    ) -> None:
        """Attach (or detach) a telemetry context and register the carrier
        loop's probes on it. Registers four probes, emitted once per input
        sample (this is a sample-rate loop — use decim to thin the stream):
        "<prefix>.lock" (the lock-signal EMA, ~1 when phase-locked),
        "<prefix>.e" (the M-th-power phase discriminator — the loop stress),
        "<prefix>.freq" (the tracked carrier frequency, cycles/sample) and
        "<prefix>.locked" (the verify-counted lockdet decision, 0/1). Passing
        NULL detaches. Setup path, never hot: call before the producer thread
        starts stepping; the context is borrowed and must outlive the
        attachment (SPSC rules in dp_tlm/dp_tlm_core.h).

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "car" or "rx.car".
        decim : int
            Emit every decim-th sample; >= 1.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_telemetry failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import CarrierNda
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 14)
        >>> c = CarrierNda(bn=0.01, sps=8, n=4, m=4)
        >>> c.set_telemetry(tlm, "car", decim=8)
        >>> sorted(tlm.probe_names)
        ['car.e', 'car.freq', 'car.lock', 'car.locked']
        >>> x = np.exp(2j * np.pi * 0.005 * np.arange(4096)).astype(
        ...     np.complex64)
        >>> _ = c.steps(x)
        >>> recs = tlm.read()
        >>> len(recs[recs["probe"] == tlm.probe_id("car.e")]) == 4096 // 8
        True

        """

    def configure_lock(
        self,
        up_thresh: float,
        down_thresh: float,
        n_up: int,
        n_down: int,
    ) -> None:
        """Re-tune the carrier lock detector: locked flips up after n_up
        consecutive samples with the lock-signal EMA above up_thresh, and drops
        after n_down consecutive samples below down_thresh (level + time
        hysteresis; see detection.LockDet). Defaults are 0.5/0.4 with 64 up /
        32 down. The THRESHOLDS are Pfa-derived: 0.5 is 4.416 sigma on the
        statistic's H0 spread, a per-look false-alarm rate of 5e-6, and it
        means that at every M because the limited statistic's H0 variance is
        1/2 for all of them. The VERIFY COUNT is not derived that way and must
        not be. Compounding a per-look Pfa over n_up assumes successive looks
        are independent; this detector steps once per sample and its lock EMA
        stays correlated for roughly 39 samples, so a shorter count is counting
        one look several times. Measured against noise-only input, n_up=8 --
        the value MpskReceiver uses on this same statistic -- false-locked 4
        trials in 30, while 64 was the smallest count clean over 300. Raise
        n_up rather than lower it unless you have re-measured. A live lock
        survives the re-tune; the in-flight verify run restarts.

        Full lockdet control, mirroring costas_configure_lock(): a split
        declare/drop threshold pair on the lock-signal EMA (level hysteresis)
        and both verify counts (time hysteresis). Defaults (0.5/0.4, 64 up / 32
        down) start from MpskReceiver's own pre-existing acquisition<->
        tracking handover thresholds, but size n_up independently: `lock` is a
        fast per-sample EMA, so consecutive looks are highly autocorrelated and
        MpskReceiver's own n_up=8 does not compound the false-declare rate the
        way it would for independent looks (direct Monte Carlo against a
        noise-only, no-carrier input found real false locks at n_up=8; n_up=64
        was the smallest verify count that reliably eliminated them -- see
        carrier_nda_core.c's CARRIER_NDA_LOCK_DEFAULT_* comment for the exact
        trial data). A live lock survives the re-tune; the in-flight verify run
        restarts.

        Parameters
        ----------
        up_thresh : float
            Declare threshold on the lock-signal EMA.
        down_thresh : float
            Drop threshold; choose <= up_thresh for level hysteresis.
        n_up : int
            Consecutive above-threshold samples to declare; clamped >= 1.
        n_down : int
            Consecutive below-threshold samples to drop; clamped >= 1.

        Examples
        --------
        >>> from doppler.track import CarrierNda
        >>> c = CarrierNda(bn=0.01, sps=8, n=4, m=4)
        >>> c.locked
        False
        >>> c.configure_lock(0.6, 0.5, 16, 64)   # tighter declare, slower drop

        """

    def reset(self) -> None:
        """Re-seed the loop to the create-time frequency/phase; preserve
        config.

        Restores the object to its post-create state: the carrier NCO is reset
        to the seed frequency it was constructed with (init_norm_freq) with
        zero phase, the moving-average arm, the loop-filter integrator and the
        lock EMA are cleared, and the lock detector is dropped. The configured
        (bn, zeta), the arm geometry (sps, n) and the constellation order m are
        preserved, so the same object can re-acquire a fresh capture.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import CarrierNda
        >>> c = CarrierNda(bn=0.01, zeta=0.707, init_norm_freq=0.0,
        ...                sps=8, n=4, m=4)
        >>> rng = np.random.default_rng(0)
        >>> k = np.arange(40000)
        >>> x = (np.exp(2j * np.pi * 0.001 * k) + 0.05 * (
        ...      rng.standard_normal(k.size)
        ...      + 1j * rng.standard_normal(k.size))).astype(np.complex64)
        >>> _ = c.steps(x)
        >>> round(c.norm_freq, 4), round(c.lock, 2)   # acquired the carrier
        (0.001, 0.99)
        >>> c.reset()
        >>> round(c.norm_freq, 4), round(c.lock, 2)   # back to seed, unlocked
        (0.0, 0.0)

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the CarrierNda has already been destroyed.

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

        Raises ``RuntimeError`` if the CarrierNda has already been destroyed.

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
        ``RuntimeError`` if the CarrierNda has already been destroyed.

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
    def lock(self) -> float:
        """EMA of the lock signal (1 = locked)."""

    @property
    def locked(self) -> bool:
        """Current lock decision: True after the verify count of consecutive
        above-threshold samples, False again after the drop count of
        consecutive below-threshold ones (see configure_lock).
        """

    @property
    def last_error(self) -> float:
        """last phase discriminator (loop stress)."""

    @property
    def bn(self) -> float:
        """PLL loop noise bandwidth (retained)."""
    @bn.setter
    def bn(self, value: float) -> None: ...

    @property
    def m(self) -> int:
        """constellation order M (2, 4, 8)."""

    @property
    def n(self) -> int:
        """sets the MA window (= a 1/n-symbol box)."""

    @property
    def sps(self) -> int:
        """samples per symbol."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "CarrierNda":
        """Enter a context manager, returning this object.

        Lets a CarrierNda be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        CarrierNda
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the CarrierNda.

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
class MpskReceiver:
    """Create an M-PSK receiver.

    Parameters
    ----------
    m : int, default 4
        Constellation order M, 2/4/8 (default 4 = QPSK).
    sps : float, default 8.0
        Samples per symbol. Any double >= `m_out` -- 17.33389 is as valid as 8,
        because the front end plans its own cascade and the terminal stage's
        accumulator is a double. That is the real-world case whenever the ADC
        clock is free-running against the symbol clock.
    m_out : int, default 0
        **0 (the default) derives it** -- see docs/design/mpsk.md §8.1; pass a
        value only to pin one. The rule is the largest even count in 2..8 the
        rate allows, so a caller cannot pair an `sps` and an `m_out` that will
        not work together. Terminal outputs per symbol: even, 2..8. The Gardner
        detector takes every m_out-th output as the on-time strobe and the one
        m_out/2 back as the transition gate, so the oversampled
        matched-filtered stream falls out for free. **The default is 8 because
        that is where the I&D matched filter reaches the coherent bound.** The
        rectangle is one symbol wide, so its matched filter is an m_out-tap sum
        spanning it, and a smaller m_out samples that same integral more
        coarsely. Measured on QPSK at the default sps=8 against the coherent
        bound EVM_dB = -(Es/N0)_dB: at 18 dB Es/N0, m_out=8 lands 0.41 dB off
        the bound where m_out=4 loses 3.11 dB; at 14 dB it is 0.25 dB against
        1.71 dB -- the gap widens as noise stops hiding it. **Never pair 2 with
        pulse="iandd"**: the matched filter degenerates to a two-tap sum, the
        eye barely opens (measured lock statistic -0.34 at 2 against +0.95 at 4
        on the same NRZ stream) and acquisition itself fails about half the
        time (4/8 seeds locked at 14 dB Es/N0, against 8/8 at both 4 and 8).
        Replaces the old `n` (NDA arm dumps per symbol): the cascade's own
        outputs now feed the carrier discriminator, so there is no separate arm
        to size.
    pulse : Literal["iandd", "rrc"], default "iandd"
        Matched-filter shape (default MPSK_RX_PULSE_IANDD).
    rrc_beta : float, default 0.35
        RRC roll-off in `[0, 1]` (default 0.35; RRC only).
    rrc_span : int, default 8
        RRC one-sided span in symbols (default 8; RRC only).
    bn_carrier : float, default 0.01
        Carrier loop noise bandwidth, **normalised to the symbol rate**
        (default 0.01). A carrier loop here closes around the matched filter,
        so its dead time is that filter's group delay — keep it a small
        fraction of the symbol rate, as a real receiver does.
    zeta : float, default 0.0
        Damping factor for both loops. **0 (the default) derives it** as
        `1/sqrt(2)`, critically damped -- a constant, not a computation, and a
        parameter only because it was once thought to be one
        (docs/design/mpsk.md §8.1).
    bn_timing : float, default 0.01
        Symbol-timing loop noise bandwidth, normalised to the symbol rate
        (default 0.01).
    lock_thresh : float, default 0.0
        Declare threshold for the carrier lock indicator, on the carrier lock
        EMA. **0 (the default) derives it** as `sigma_H0 * eta(Pfa)` = 0.4999
        at `Pfa = 5e-6`; the limited statistic reads ~1.0 at lock for every M,
        so no per-M correction is carried (docs/design/mpsk.md §8.1).
    init_norm_freq : float, default 0.0
        Seed carrier frequency, cycles/sample at the input rate (default 0.0).
        This is the centre the LO is tuned to; the loop tracks the residual
        around it.
    differential : int, default 0
        bits(): differential (rotation-invariant) demap (default 0 = coherent).
    num_phases : int, default 0
        Matched-filter bank arms, a power of two; sets the fractional-timing
        resolution to 1/num_phases of an output period. **0 (the default)
        derives it** as 64, the measured saturation point -- against the 1024
        that shipped, a 16x bank for no measurable gain (docs/design/mpsk.md
        §8.1). Matched-filter bank arms; a power of two. Sets the
        fractional-timing resolution to 1/num_phases of an output period. The
        bank is sized by the POST-decimation rate, so this costs the same at
        sps=8 and sps=256.
    agc : int, default 1
        Level the front-end cascade so the timing detector's construct-time
        slope means what it says. The TED normalises by its OWN slope and
        nothing else, and that slope is computed for a UNIT-amplitude symbol
        stream -- amplitude enters the raw error as A^2 (Gardner) or A^1
        (DTTL), so a 4x level error is a 16x loop-gain error. With this on, an
        AGC sits inside the cascade just before the matched filter and drives
        the average power to the level a unit-amplitude symbol stream would
        have there, derived from the matched filter's own pulse energy rather
        than configured. On by default because the alternative is a receiver
        whose loop gain depends on how loud the input happened to be. Set 0 to
        reproduce the un-levelled behaviour exactly, which is the handle for
        attributing any measurement that moves. This is the receiver's ONLY AGC
        -- the carrier discriminator normalises by its own |z|^M and needs
        none. Read the applied gain back with `agc_gain_db`.
    bn_agc_ratio : float, default 0.0
        **0 (the default) derives it** -- see docs/design/mpsk.md §8.1; pass a
        value only to pin one. The one AGC's noise bandwidth as a fraction of
        the SLOWEST loop it feeds -- min(bn_carrier, bn_timing), not the
        carrier's alone, since the AGC feeds the timing loop directly. Must be
        in (0, 1) and construction refuses otherwise, because an AGC at or
        above the bandwidth of a loop it feeds corrects the excursions that
        loop is itself producing and the two integrate against each other; the
        signal LEVEL is a slow property of the channel, not a disturbance to
        reject at loop speed. Exposed rather than fixed because the right
        separation depends on how fast the channel's level moves against how
        fast its phase and timing do, which is a property of the link. The cold
        start is not the loop's job either way -- the AGC seeds its gain from a
        direct measurement -- so slow is cheap here.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``MpskReceiver: invalid
        parameter (need m in {2,4,8}, sps >= m_out -- sps > 2*m_out on the
        real-input MpskReceiverR, whose cascade runs behind a 2:1 halfband,
        m_out even in [2, 8], 0 <= rrc_beta <= 1, rrc_span >= 1, num_phases a
        power of two >= 2, bn >= 0, zeta > 0, 0 < bn_agc_ratio < 1)``.

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import MpskReceiver
    >>> obj = MpskReceiver(
    ...     m=4,
    ...     sps=8.0,
    ...     m_out=0,
    ...     pulse="iandd",
    ...     rrc_beta=0.35,
    ...     rrc_span=8,
    ...     bn_carrier=0.01,
    ...     zeta=0.0,
    ...     bn_timing=0.01,
    ...     lock_thresh=0.0,
    ...     init_norm_freq=0.0,
    ...     differential=0,
    ...     num_phases=0,
    ...     agc=1,
    ...     bn_agc_ratio=0.0,
    ... )

    """
    def __init__(
        self,
        m: int = ...,
        sps: float = ...,
        m_out: int = ...,
        pulse: Literal["iandd", "rrc"] = "iandd",
        rrc_beta: float = ...,
        rrc_span: int = ...,
        bn_carrier: float = ...,
        zeta: float = ...,
        bn_timing: float = ...,
        lock_thresh: float = ...,
        init_norm_freq: float = ...,
        differential: int = ...,
        num_phases: int = ...,
        agc: int = ...,
        bn_agc_ratio: float = ...,
    ) -> None: ...

    def set_telemetry(
        self,
        tlm: object | None,
        prefix: str,
        decim: int = 1,
    ) -> None:
        """Attach (or detach) a telemetry context across the receiver.
        Registers the receiver's own "<prefix>.lock" probe (the carrier lock
        EMA), then the carrier loop's "<prefix>.car.e" / ".freq" / ".locked"
        and the symbol-timing loop's "<prefix>.sync.e" / ".ctrl" / ".rate" /
        ".lock" / ".locked" / ".mu" -- ten probes emitted once per recovered
        symbol -- then the front end's AGC under "<prefix>.agc"
        ("<prefix>.agc.gain_db" and "<prefix>.agc.level_db"; see
        agc_set_telemetry()). Twelve probes total, all thinned by decim.
        Passing NULL detaches everything.

        Instrumenting it matters because it is FIRST in the chain, and a level
        error is the one kind no downstream loop can correct for itself: a TED
        normalises by its own construct-time slope, so it reads a level error
        as a loop-gain error (A^2 Gardner, A DTTL) with no other reference to
        catch it. This receiver also makes the AGC the slowest of its three
        loops by construction -- mpsk_rx_agc_bn() derives its bandwidth as a
        fraction of the slowest loop it feeds, and bn_agc_ratio is validated to
        (0, 1) -- but that is a choice of THIS composition, and slowest does
        not by itself mean longest: settling is set by the bandwidth AND by how
        far the level starts from the reference, which is unknown at
        construction. Which is exactly why it has to be measured rather than
        inferred; the zero-referenced "<prefix>.agc.level_db" is what makes
        that possible.

        With agc = 0 at construction there is no AGC to attach and the two
        probes are simply absent (fourteen, not sixteen); this still returns
        DP_OK.

        Setup path, never hot; the context is borrowed and must outlive the
        attachment (SPSC rules in dp_tlm/dp_tlm_core.h).

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "rx".
        decim : int
            Emit every decim-th symbol (every decim-th gain update for the two
            AGC probes); >= 1.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_telemetry failed``, with the return code appended (gh-869).

        Warnings
        --------
        The two AGC probes are NOT at the symbol rate the other ten are. That
        AGC sits pre-terminal in the cascade (RateConverter's tap, ahead of the
        stage the timing loop steers) and emits once per gain-update event,
        i.e. every AGC_DECIM_DEFAULT samples of that fixed-rate stream -- so it
        reports on a grid that depends on the planned cascade, not on recovered
        symbols, and a run yields a different number of AGC records than
        carrier records. Compare the two by TIME, never by record index. This
        is deliberate: the AGC's bandwidth is quoted in the pre-terminal
        stream's units precisely so it is not coupled to the loop that is
        stretching the symbol grid (see RateConverter_enable_agc()).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import MpskReceiver
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 14)   # 15 probes x ~512 syms + headroom
        >>> rx = MpskReceiver(m=4, sps=4, m_out=2)
        >>> rx.set_telemetry(tlm, "rx")
        >>> len(tlm.probe_names)
        15
        >>> rng = np.random.default_rng(7)
        >>> syms = (1 - 2 * rng.integers(0, 2, 512)).astype(np.complex64)
        >>> x = np.repeat(syms, 4)
        >>> _ = rx.steps(x)
        >>> recs = tlm.read()
        >>> tlm.dropped        # size the ring, or the counts below diverge
        0
        >>> n_sync = len(recs[recs["probe"] == tlm.probe_id("rx.sync.e")])
        >>> n_car = len(recs[recs["probe"] == tlm.probe_id("rx.car.e")])
        >>> n_sync > 0 and n_sync == n_car
        True
        >>> n_agc = len(recs[recs["probe"] == tlm.probe_id("rx.agc.gain_db")])
        >>> n_agc > 0 and n_agc != n_sync   # cascade grid, not symbol grid
        True

        """

    def steps(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Demodulate a cf32 block and return the recovered M-PSK symbols (one
        cf32 per recovered symbol period, ~ len(x)/sps outputs). Per sample the
        receiver pushes x through the matched DDC -- LO mix, decimating
        cascade, and a terminal polyphase stage whose bank IS the matched
        filter and whose selected arm IS the fractional symbol-timing delay --
        then folds every output that stage produced into two loops: a Gardner
        symbol-timing loop steering the cascade's rate_ctrl port, and a carrier
        loop steering the LO's freq_ctrl port. The carrier discriminator runs
        on the on-time strobe only -- a non-strobe output straddles two
        symbols, so its M-th power is intersymbol interference rather than
        carrier phase -- and it is the non-data-aided M-th-power error
        throughout, needing no data and no symbol timing: there is one
        discriminator, running from the first symbol to the last. The loop
        locks to one of m phases (M-fold ambiguity); resolve it with
        bits(differential) or a sync word. Read norm_freq for the tracked
        carrier and lock for the carrier lock metric.

        Runs the per-sample loop (mix + cascade + matched filter, then the
        carrier and timing loops) over x and writes one cf32 symbol per
        recovered symbol period — roughly `x_len / sps` outputs. Read norm_freq
        for the tracked carrier and lock for the carrier lock metric.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input cf32 samples.
        out : NDArray[np.complex64] | None
            Output symbols; caller provides max_out capacity.

        Returns
        -------
        NDArray[np.complex64]
            Number of symbols written.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import MpskReceiver
        >>> rng = np.random.default_rng(0)
        >>> idx = rng.integers(0, 4, 3000)                  # QPSK symbols
        >>> tx = np.repeat(np.exp(1j * (2 * np.pi * idx / 4 + np.pi / 4)), 8)
        >>> tx = tx.astype(np.complex64)                    # 8 samples/symbol
        >>> rx = MpskReceiver(m=4, sps=8, m_out=4, bn_carrier=0.02)
        >>> sym = rx.steps(tx)                              # blind NDA acquire
        >>> sym.size                                        # ~ x_len / sps
        2998
        >>> rx.lock > 0.8                                   # carrier locked
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

    def bits(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """Demodulate a cf32 block and return hard Gray-coded bits (log2(m)
        bytes of 0/1 per recovered symbol, LSB-first). Coherent by default; if
        the receiver was created with differential=1, each symbol's bits come
        from the phase DIFFERENCE between consecutive symbols
        (rotation-invariant — resolves the m-fold carrier ambiguity at ~2x the
        symbol-error rate). Same per-sample carrier/timing recovery as steps().

        Like mpsk_receiver_steps(), but each recovered symbol is sliced to its
        nearest M-PSK point and unpacked to log2(M) hard bits (LSB-first). With
        the differential option set at create time, the Gray label is taken
        from the phase *difference* between consecutive symbols
        (rotation-invariant — it resolves the M-fold carrier ambiguity), else
        from the absolute (coherent) decision.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input cf32 samples.
        out : NDArray[np.uint8] | None
            Output bytes (0/1); caller provides max_out capacity.

        Returns
        -------
        NDArray[np.uint8]
            Number of bits written.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import MpskReceiver
        >>> rng = np.random.default_rng(3)
        >>> idx = rng.integers(0, 2, 3000)                  # BPSK payload bits
        >>> tx = np.repeat(np.exp(1j * np.pi * idx), 8).astype(np.complex64)
        >>> rx = MpskReceiver(m=2, sps=8, m_out=4, bn_carrier=0.005)
        >>> b = rx.bits(tx)                                 # 1 hard bit/symbol
        >>> b.size
        2998
        >>> # settled tail matches the payload, up to the BPSK
        >>> # inversion ambiguity and the pipeline's one-symbol lead
        >>> tail = np.mean(b[1001:2001] != idx[1000:2000])
        >>> round(float(min(tail, 1 - tail)), 3)
        0.0

        """

    def bits_max_out(self) -> int:
        """Largest number of samples bits() can return in the current state.

        Size an `out=` buffer with this before calling bits(), or use it to
        allocate one up front. The bound is this object's own: what it depends
        on is a property of the algorithm, so a header block on bits_max_out()
        replaces this text.

        Returns
        -------
        int
            Upper bound on the output length; the actual call may return fewer.
        """

    def reset(self) -> None:
        """Re-seed the carrier and symbol-timing loops to their create-time
        state; preserve configuration.

        Clears the cascade's filter memory, the carrier and timing NCOs, the
        loop-filter integrators and the lock detectors, and returns the carrier
        estimate to init_norm_freq. The configuration (order, rate, pulse,
        bandwidths) is untouched, so the same input fed twice around a reset
        reproduces the same output bit-for-bit.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import MpskReceiver
        >>> rng = np.random.default_rng(0)
        >>> idx = rng.integers(0, 4, 300)
        >>> tx = np.repeat(np.exp(1j * (2 * np.pi * idx / 4 + np.pi / 4)), 8)
        >>> tx = tx.astype(np.complex64)
        >>> rx = MpskReceiver(m=4, sps=8, m_out=4)
        >>> first = rx.steps(tx)
        >>> rx.reset()                                # back to the cold state
        >>> np.array_equal(first, rx.steps(tx))       # same input, same output
        True

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the MpskReceiver has already been destroyed.

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

        Raises ``RuntimeError`` if the MpskReceiver has already been destroyed.

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
        ``RuntimeError`` if the MpskReceiver has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def agc_gain_db(self) -> float:
        """Gain the front-end AGC is applying, in dB; 0.0 when `agc=0`. The
        diagnostic for a level problem: a receiver that will not lock with a
        healthy `lock` statistic, or one whose timing loop behaves differently
        at two input levels, is asking about this number. It settles at
        -10*log10(P_in / P_ref), where P_ref is the power a unit-amplitude
        symbol stream has where the AGC sits, so a reading far from 0 dB says
        the input is far from the level the cascade was built for -- which is
        fine, and is what the AGC is for, but is worth knowing. Distinct from
        the cascade's filter response, which stays unity; the two multiply.
        """

    @property
    def norm_freq(self) -> float:
        """Carrier frequency the receiver is tracking, cycles/sample at the
        input rate: the create-time centre plus the loop's own estimate.
        """
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def lock(self) -> float:
        """EMA of the carrier lock signal."""

    @property
    def zeta(self) -> float:
        """Loop damping actually in use. Reads back the DERIVED `1/sqrt(2)`
        when the constructor was given 0, or whatever was pinned instead.
        Everything derived is reported (docs/design/mpsk.md §8.1), on the same
        argument as `RateConverter.stages`: without the readback, passing 0 is
        an instruction whose result nobody can see.
        """

    @property
    def num_phases(self) -> int:
        """Matched-filter bank arms actually in use. Reads back the DERIVED 64
        -- the measured saturation point, against the 1024 that shipped -- when
        the constructor was given 0. See `zeta` for why every derived value is
        reported.
        """

    @property
    def lock_thresh(self) -> float:
        """Handover declare threshold actually in use. Reads back the DERIVED
        `sigma_H0 * eta(Pfa)` = 0.4999 at `Pfa = 5e-6` when the constructor was
        given 0. See `zeta` for why every derived value is reported.
        """

    @property
    def lock_drop_thresh(self) -> float:
        """Carrier DROP threshold actually in use -- 0.8x `lock_thresh`, the
        level hysteresis the declare/drop pair is stated with. Exposed for the
        same reason as the declare side: anything reading `lock` against its
        decision needs BOTH edges, and computing `0.8 *` at the call site is a
        second copy of a rule this object owns. Both carrier detectors are
        initialised from this pair, so one number describes them both.
        """

    @property
    def sync_lock_thresh(self) -> float:
        """Timing DECLARE threshold on the `sync.lock` statistic. Not the
        carrier's number and not derived the same way: symsync sizes its block
        length and threshold together from (rolloff, esno_min, pfa, pd), so
        this reads back that geometry's answer. A caller plotting `sync.lock`
        needs this rather than `lock_thresh`, which belongs to a different
        statistic on a different clock.
        """

    @property
    def sync_lock_drop_thresh(self) -> float:
        """Timing DROP threshold on `sync.lock`. Equal to `sync_lock_thresh`
        when the timing loop carries no level hysteresis (up = down =
        threshold, the symsync default), so the two reading the same is
        information, not a bug -- the timing decision's hysteresis is in its
        verify COUNTS rather than its levels.
        """

    @property
    def bn_agc_ratio(self) -> float:
        """AGC bandwidth as a fraction of the slowest loop it feeds, actually
        in use. Reads back the DERIVED 0.05 when the constructor was given 0.
        See `zeta` for why every derived value is reported.
        """

    @property
    def locked(self) -> int:
        """Binary carrier-lock flag from the hysteretic (verify-counted)
        detector on `lock` -- de-chattered, unlike the raw metric. It declares
        after 8 consecutive symbols above `lock_thresh` and withdraws after 32
        below `lock_drop_thresh`, so it answers 'is this receiver locked'
        rather than 'was the statistic above the line on this symbol'. **It is
        an INDICATOR and nothing else**: it steers no loop, gates no output,
        and the M-th-power NDA discriminator runs from the first strobe whether
        or not this has declared. So a caller uses it to size a measurement
        window, and a wrong reading costs them that window and costs the
        demodulator nothing. `lock_time` dates its first declaration.
        """

    @property
    def lock_time(self) -> int:
        """Symbols from reset to the FIRST carrier-lock declaration, or -1 if
        the receiver has not locked yet -- the acquisition time as a number,
        rather than something a caller has to infer by polling `locked` in a
        loop. Dated by the same hysteretic detector `locked` reports, so the
        two cannot disagree. In SYMBOLS, not seconds: `bn_carrier` and
        `bn_timing` are both normalised to the symbol rate, so a settling
        budget quoted in symbols is comparable across every input rate, and a
        caller holding Rs divides once. Only the FIRST declaration is dated --
        a drop and re-acquire does not restamp it, because the question this
        answers is 'how long did this receiver take to lock', not 'when did it
        last hold'. `reset()` clears it to -1.
        """

    @property
    def timing_rate(self) -> float:
        """Smoothed tracked samples per symbol — departs from the nominal `sps`
        by exactly the sample-clock offset the timing loop is tracking.
        """

    @property
    def m(self) -> int:
        """constellation order M (2, 4, 8)."""

    @property
    def sps(self) -> float:
        """samples per symbol at the receiver's input."""

    @property
    def m_out(self) -> int:
        """Terminal outputs per symbol (the old `n`, now the cascade's)."""

    @property
    def clipped(self) -> int:
        """Has the cascade's CIC stage clipped its input since the last reset?
        A CIC bounds its input to |Re|, |Im| <= 1.0 and clips silently past
        that -- the output stays finite and plausible, merely distorted, at a
        cost of ~25 dB of EVM that no lock metric reveals. Always 0 for a plan
        with no CIC stage.
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


    def __enter__(self) -> "MpskReceiver":
        """Enter a context manager, returning this object.

        Lets a MpskReceiver be used in a `with` statement so its C resources
        are released deterministically on exit rather than at collection time.

        Returns
        -------
        MpskReceiver
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the MpskReceiver.

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
class BpskReceiver:
    """Create an M-PSK receiver.

    Parameters
    ----------
    sample_rate_hz : float
        ADC sample rate, Hz. One of the two facts a caller reading a capture
        actually holds -- `sps` is `sample_rate_hz / symbol_rate_hz`, a ratio
        this library computes for its own use in selecting a cascade, and
        asking for it would make the caller derive an internal quantity.
    symbol_rate_hz : float
        Symbol rate, Hz. Must leave `sample_rate_hz / symbol_rate_hz` at or
        above the derived `m_out`; a rate that cannot be strobed is refused at
        construction rather than approximated.
    carrier_freq_hz : float, default 0.0
        Carrier centre, Hz. 0 is complex baseband. Stated in Hz like everything
        else here: `MpskReceiver`'s `init_norm_freq` is cycles per SAMPLE,
        which is a unit a caller can only produce by holding `sps` and `fs`
        together, and that coupling is what this face removes.
    pulse : Literal["iandd", "rrc"], default "iandd"
        Matched-filter shape. Same default as `MpskReceiver` deliberately: one
        default across both faces beats a face-specific guess at a link
        property.
    rrc_beta : float, default 0.35
        RRC roll-off in `[0, 1]`. RRC only.
    rrc_span : int, default 8
        RRC one-sided span in symbols. RRC only.
    bn_carrier : float, default 0.01
        Carrier loop noise bandwidth, normalised to the SYMBOL rate. A design
        axis, and the one the acquisition bound `bn_carrier / m` is stated
        against.
    bn_timing : float, default 0.01
        Symbol-timing loop noise bandwidth, normalised to the symbol rate. A
        design axis.
    differential : int, default 0
        bits(): differential (rotation-invariant) demap. Left optional and
        coherent by default, matching `MpskReceiver` -- it changes what the
        output bits MEAN and is not derivable from the signal, so it is a link
        property a caller states rather than one this object guesses.
    agc : int, default 1
        Front-end AGC. On by default: it levels the cascade so the timing
        detector's construct-time slope means what it says.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``MpskReceiver: invalid
        parameter (need m in {2,4,8}, sps >= m_out -- sps > 2*m_out on the
        real-input MpskReceiverR, whose cascade runs behind a 2:1 halfband,
        m_out even in [2, 8], 0 <= rrc_beta <= 1, rrc_span >= 1, num_phases a
        power of two >= 2, bn >= 0, zeta > 0, 0 < bn_agc_ratio < 1)``.

    """
    def __init__(
        self,
        sample_rate_hz: float,
        symbol_rate_hz: float,
        carrier_freq_hz: float = ...,
        pulse: Literal["iandd", "rrc"] = "iandd",
        rrc_beta: float = ...,
        rrc_span: int = ...,
        bn_carrier: float = ...,
        bn_timing: float = ...,
        differential: int = ...,
        agc: int = ...,
    ) -> None: ...

    def set_telemetry(
        self,
        tlm: object | None,
        prefix: str,
        decim: int = 1,
    ) -> None:
        """Attach (or detach) a telemetry context across the receiver.
        Registers the receiver's own "<prefix>.lock" probe (the carrier lock
        EMA), then the carrier loop's "<prefix>.car.e" / ".freq" / ".locked"
        and the symbol-timing loop's "<prefix>.sync.e" / ".ctrl" / ".rate" /
        ".lock" / ".locked" / ".mu" -- ten probes emitted once per recovered
        symbol -- then the front end's AGC under "<prefix>.agc"
        ("<prefix>.agc.gain_db" and "<prefix>.agc.level_db"; see
        agc_set_telemetry()). Twelve probes total, all thinned by decim.
        Passing NULL detaches everything.

        Instrumenting it matters because it is FIRST in the chain, and a level
        error is the one kind no downstream loop can correct for itself: a TED
        normalises by its own construct-time slope, so it reads a level error
        as a loop-gain error (A^2 Gardner, A DTTL) with no other reference to
        catch it. This receiver also makes the AGC the slowest of its three
        loops by construction -- mpsk_rx_agc_bn() derives its bandwidth as a
        fraction of the slowest loop it feeds, and bn_agc_ratio is validated to
        (0, 1) -- but that is a choice of THIS composition, and slowest does
        not by itself mean longest: settling is set by the bandwidth AND by how
        far the level starts from the reference, which is unknown at
        construction. Which is exactly why it has to be measured rather than
        inferred; the zero-referenced "<prefix>.agc.level_db" is what makes
        that possible.

        With agc = 0 at construction there is no AGC to attach and the two
        probes are simply absent (fourteen, not sixteen); this still returns
        DP_OK.

        Setup path, never hot; the context is borrowed and must outlive the
        attachment (SPSC rules in dp_tlm/dp_tlm_core.h).

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "rx".
        decim : int
            Emit every decim-th symbol (every decim-th gain update for the two
            AGC probes); >= 1.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_telemetry failed``, with the return code appended (gh-869).

        Warnings
        --------
        The two AGC probes are NOT at the symbol rate the other ten are. That
        AGC sits pre-terminal in the cascade (RateConverter's tap, ahead of the
        stage the timing loop steers) and emits once per gain-update event,
        i.e. every AGC_DECIM_DEFAULT samples of that fixed-rate stream -- so it
        reports on a grid that depends on the planned cascade, not on recovered
        symbols, and a run yields a different number of AGC records than
        carrier records. Compare the two by TIME, never by record index. This
        is deliberate: the AGC's bandwidth is quoted in the pre-terminal
        stream's units precisely so it is not coupled to the loop that is
        stretching the symbol grid (see RateConverter_enable_agc()).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import MpskReceiver
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 14)   # 15 probes x ~512 syms + headroom
        >>> rx = MpskReceiver(m=4, sps=4, m_out=2)
        >>> rx.set_telemetry(tlm, "rx")
        >>> len(tlm.probe_names)
        15
        >>> rng = np.random.default_rng(7)
        >>> syms = (1 - 2 * rng.integers(0, 2, 512)).astype(np.complex64)
        >>> x = np.repeat(syms, 4)
        >>> _ = rx.steps(x)
        >>> recs = tlm.read()
        >>> tlm.dropped        # size the ring, or the counts below diverge
        0
        >>> n_sync = len(recs[recs["probe"] == tlm.probe_id("rx.sync.e")])
        >>> n_car = len(recs[recs["probe"] == tlm.probe_id("rx.car.e")])
        >>> n_sync > 0 and n_sync == n_car
        True
        >>> n_agc = len(recs[recs["probe"] == tlm.probe_id("rx.agc.gain_db")])
        >>> n_agc > 0 and n_agc != n_sync   # cascade grid, not symbol grid
        True

        """

    def steps(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Demodulate a cf32 block and return the recovered M-PSK symbols (one
        cf32 per recovered symbol period, ~ len(x)/sps outputs). Per sample the
        receiver pushes x through the matched DDC -- LO mix, decimating
        cascade, and a terminal polyphase stage whose bank IS the matched
        filter and whose selected arm IS the fractional symbol-timing delay --
        then folds every output that stage produced into two loops: a Gardner
        symbol-timing loop steering the cascade's rate_ctrl port, and a carrier
        loop steering the LO's freq_ctrl port. The carrier discriminator runs
        on the on-time strobe only -- a non-strobe output straddles two
        symbols, so its M-th power is intersymbol interference rather than
        carrier phase -- and it is the non-data-aided M-th-power error
        throughout, needing no data and no symbol timing: there is one
        discriminator, running from the first symbol to the last. The loop
        locks to one of m phases (M-fold ambiguity); resolve it with
        bits(differential) or a sync word. Read norm_freq for the tracked
        carrier and lock for the carrier lock metric.

        Runs the per-sample loop (mix + cascade + matched filter, then the
        carrier and timing loops) over x and writes one cf32 symbol per
        recovered symbol period — roughly `x_len / sps` outputs. Read norm_freq
        for the tracked carrier and lock for the carrier lock metric.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input cf32 samples.
        out : NDArray[np.complex64] | None
            Output symbols; caller provides max_out capacity.

        Returns
        -------
        NDArray[np.complex64]
            Number of symbols written.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import MpskReceiver
        >>> rng = np.random.default_rng(0)
        >>> idx = rng.integers(0, 4, 3000)                  # QPSK symbols
        >>> tx = np.repeat(np.exp(1j * (2 * np.pi * idx / 4 + np.pi / 4)), 8)
        >>> tx = tx.astype(np.complex64)                    # 8 samples/symbol
        >>> rx = MpskReceiver(m=4, sps=8, m_out=4, bn_carrier=0.02)
        >>> sym = rx.steps(tx)                              # blind NDA acquire
        >>> sym.size                                        # ~ x_len / sps
        2998
        >>> rx.lock > 0.8                                   # carrier locked
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

    def bits(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """Demodulate a cf32 block and return hard Gray-coded bits (log2(m)
        bytes of 0/1 per recovered symbol, LSB-first). Coherent by default; if
        the receiver was created with differential=1, each symbol's bits come
        from the phase DIFFERENCE between consecutive symbols
        (rotation-invariant — resolves the m-fold carrier ambiguity at ~2x the
        symbol-error rate). Same per-sample carrier/timing recovery as steps().

        Like mpsk_receiver_steps(), but each recovered symbol is sliced to its
        nearest M-PSK point and unpacked to log2(M) hard bits (LSB-first). With
        the differential option set at create time, the Gray label is taken
        from the phase *difference* between consecutive symbols
        (rotation-invariant — it resolves the M-fold carrier ambiguity), else
        from the absolute (coherent) decision.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input cf32 samples.
        out : NDArray[np.uint8] | None
            Output bytes (0/1); caller provides max_out capacity.

        Returns
        -------
        NDArray[np.uint8]
            Number of bits written.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import MpskReceiver
        >>> rng = np.random.default_rng(3)
        >>> idx = rng.integers(0, 2, 3000)                  # BPSK payload bits
        >>> tx = np.repeat(np.exp(1j * np.pi * idx), 8).astype(np.complex64)
        >>> rx = MpskReceiver(m=2, sps=8, m_out=4, bn_carrier=0.005)
        >>> b = rx.bits(tx)                                 # 1 hard bit/symbol
        >>> b.size
        2998
        >>> # settled tail matches the payload, up to the BPSK
        >>> # inversion ambiguity and the pipeline's one-symbol lead
        >>> tail = np.mean(b[1001:2001] != idx[1000:2000])
        >>> round(float(min(tail, 1 - tail)), 3)
        0.0

        """

    def bits_max_out(self) -> int:
        """Largest number of samples bits() can return in the current state.

        Size an `out=` buffer with this before calling bits(), or use it to
        allocate one up front. The bound is this object's own: what it depends
        on is a property of the algorithm, so a header block on bits_max_out()
        replaces this text.

        Returns
        -------
        int
            Upper bound on the output length; the actual call may return fewer.
        """

    def reset(self) -> None:
        """Re-seed the carrier and symbol-timing loops to their create-time
        state; preserve configuration.

        Clears the cascade's filter memory, the carrier and timing NCOs, the
        loop-filter integrators and the lock detectors, and returns the carrier
        estimate to init_norm_freq. The configuration (order, rate, pulse,
        bandwidths) is untouched, so the same input fed twice around a reset
        reproduces the same output bit-for-bit.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import MpskReceiver
        >>> rng = np.random.default_rng(0)
        >>> idx = rng.integers(0, 4, 300)
        >>> tx = np.repeat(np.exp(1j * (2 * np.pi * idx / 4 + np.pi / 4)), 8)
        >>> tx = tx.astype(np.complex64)
        >>> rx = MpskReceiver(m=4, sps=8, m_out=4)
        >>> first = rx.steps(tx)
        >>> rx.reset()                                # back to the cold state
        >>> np.array_equal(first, rx.steps(tx))       # same input, same output
        True

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the BpskReceiver has already been destroyed.

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

        Raises ``RuntimeError`` if the BpskReceiver has already been destroyed.

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
        ``RuntimeError`` if the BpskReceiver has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def agc_gain_db(self) -> float:
        """Gain the front-end AGC is applying, in dB; 0.0 when `agc=0`. The
        diagnostic for a level problem: a receiver that will not lock with a
        healthy `lock` statistic, or one whose timing loop behaves differently
        at two input levels, is asking about this number. It settles at
        -10*log10(P_in / P_ref), where P_ref is the power a unit-amplitude
        symbol stream has where the AGC sits, so a reading far from 0 dB says
        the input is far from the level the cascade was built for -- which is
        fine, and is what the AGC is for, but is worth knowing. Distinct from
        the cascade's filter response, which stays unity; the two multiply.
        """

    @property
    def norm_freq(self) -> float:
        """Carrier frequency the receiver is tracking, cycles/sample at the
        input rate: the create-time centre plus the loop's own estimate.
        """
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def lock(self) -> float:
        """EMA of the carrier lock signal."""

    @property
    def zeta(self) -> float:
        """Loop damping actually in use. Reads back the DERIVED `1/sqrt(2)`
        when the constructor was given 0, or whatever was pinned instead.
        Everything derived is reported (docs/design/mpsk.md §8.1), on the same
        argument as `RateConverter.stages`: without the readback, passing 0 is
        an instruction whose result nobody can see.
        """

    @property
    def num_phases(self) -> int:
        """Matched-filter bank arms actually in use. Reads back the DERIVED 64
        -- the measured saturation point, against the 1024 that shipped -- when
        the constructor was given 0. See `zeta` for why every derived value is
        reported.
        """

    @property
    def lock_thresh(self) -> float:
        """Handover declare threshold actually in use. Reads back the DERIVED
        `sigma_H0 * eta(Pfa)` = 0.4999 at `Pfa = 5e-6` when the constructor was
        given 0. See `zeta` for why every derived value is reported.
        """

    @property
    def lock_drop_thresh(self) -> float:
        """Carrier DROP threshold actually in use -- 0.8x `lock_thresh`, the
        level hysteresis the declare/drop pair is stated with. Exposed for the
        same reason as the declare side: anything reading `lock` against its
        decision needs BOTH edges, and computing `0.8 *` at the call site is a
        second copy of a rule this object owns. Both carrier detectors are
        initialised from this pair, so one number describes them both.
        """

    @property
    def sync_lock_thresh(self) -> float:
        """Timing DECLARE threshold on the `sync.lock` statistic. Not the
        carrier's number and not derived the same way: symsync sizes its block
        length and threshold together from (rolloff, esno_min, pfa, pd), so
        this reads back that geometry's answer. A caller plotting `sync.lock`
        needs this rather than `lock_thresh`, which belongs to a different
        statistic on a different clock.
        """

    @property
    def sync_lock_drop_thresh(self) -> float:
        """Timing DROP threshold on `sync.lock`. Equal to `sync_lock_thresh`
        when the timing loop carries no level hysteresis (up = down =
        threshold, the symsync default), so the two reading the same is
        information, not a bug -- the timing decision's hysteresis is in its
        verify COUNTS rather than its levels.
        """

    @property
    def bn_agc_ratio(self) -> float:
        """AGC bandwidth as a fraction of the slowest loop it feeds, actually
        in use. Reads back the DERIVED 0.05 when the constructor was given 0.
        See `zeta` for why every derived value is reported.
        """

    @property
    def locked(self) -> int:
        """Binary carrier-lock flag from the hysteretic (verify-counted)
        detector on `lock` -- de-chattered, unlike the raw metric. It declares
        after 8 consecutive symbols above `lock_thresh` and withdraws after 32
        below `lock_drop_thresh`, so it answers 'is this receiver locked'
        rather than 'was the statistic above the line on this symbol'. **It is
        an INDICATOR and nothing else**: it steers no loop, gates no output,
        and the M-th-power NDA discriminator runs from the first strobe whether
        or not this has declared. So a caller uses it to size a measurement
        window, and a wrong reading costs them that window and costs the
        demodulator nothing. `lock_time` dates its first declaration.
        """

    @property
    def lock_time(self) -> int:
        """Symbols from reset to the FIRST carrier-lock declaration, or -1 if
        the receiver has not locked yet -- the acquisition time as a number,
        rather than something a caller has to infer by polling `locked` in a
        loop. Dated by the same hysteretic detector `locked` reports, so the
        two cannot disagree. In SYMBOLS, not seconds: `bn_carrier` and
        `bn_timing` are both normalised to the symbol rate, so a settling
        budget quoted in symbols is comparable across every input rate, and a
        caller holding Rs divides once. Only the FIRST declaration is dated --
        a drop and re-acquire does not restamp it, because the question this
        answers is 'how long did this receiver take to lock', not 'when did it
        last hold'. `reset()` clears it to -1.
        """

    @property
    def timing_rate(self) -> float:
        """Smoothed tracked samples per symbol — departs from the nominal `sps`
        by exactly the sample-clock offset the timing loop is tracking.
        """

    @property
    def m(self) -> int:
        """constellation order M (2, 4, 8)."""

    @property
    def sps(self) -> float:
        """samples per symbol at the receiver's input."""

    @property
    def m_out(self) -> int:
        """Terminal outputs per symbol (the old `n`, now the cascade's)."""

    @property
    def clipped(self) -> int:
        """Has the cascade's CIC stage clipped its input since the last reset?
        A CIC bounds its input to |Re|, |Im| <= 1.0 and clips silently past
        that -- the output stays finite and plausible, merely distorted, at a
        cost of ~25 dB of EVM that no lock metric reveals. Always 0 for a plan
        with no CIC stage.
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


    def __enter__(self) -> "BpskReceiver":
        """Enter a context manager, returning this object.

        Lets a BpskReceiver be used in a `with` statement so its C resources
        are released deterministically on exit rather than at collection time.

        Returns
        -------
        BpskReceiver
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the BpskReceiver.

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
class MpskReceiverR:
    """Create the M-PSK receiver behind an R2C halfband: a real IF in. The same
    object as MpskReceiver -- same loops, same demapper, same state -- reached
    through a matched DDCR instead of a matched DDC, so `steps()` and `bits()`
    take float32 and everything else is shared verbatim. A real-valued IF is
    the usual output of a single-ended ADC, so this is the face that takes a
    digitiser's samples directly. Three things follow from the halfband and
    nothing else differs: the LO runs at HALF the input rate (handled
    internally -- every frequency on this class stays in cycles/sample at the
    real input rate), `sps` must exceed `2 * m_out` strictly rather than merely
    reaching `m_out`, and `init_norm_freq` is the real IF CENTRE rather than a
    baseband residual.

    Parameters
    ----------
    m : int, default 4
        Constellation order M, 2/4/8 (default 4 = QPSK).
    sps : float, default 32.0
        Samples per symbol. Any double **strictly greater than `2 * m_out`**
        (the cascade behind the R2C halfband runs at twice the overall rate,
        and Ddcr needs that below 0.5) -- 33.33389 is as valid as 32, because
        the front end plans its own cascade and the terminal stage's
        accumulator is a double. That is the real-world case whenever the ADC
        clock is free-running against the symbol clock. The default is 32
        rather than the complex face's 8 purely to clear that bound: `m_out`
        derives to 8 here, so anything at or below 16 cannot construct.
    m_out : int, default 0
        **0 (the default) derives it** -- see docs/design/mpsk.md 8.1; pass a
        value only to pin one. Terminal outputs per symbol: even, 2..8. The
        Gardner detector takes every m_out-th output as the on-time strobe and
        the one m_out/2 back as the transition gate, so the oversampled
        matched-filtered stream falls out for free. **The default is 8 because
        that is where the I&D matched filter reaches the coherent bound.** The
        rectangle is one symbol wide, so its matched filter is an m_out-tap sum
        spanning it, and a smaller m_out samples that same integral more
        coarsely. Measured on the complex face at its default sps=8 against the
        coherent bound EVM_dB = -(Es/N0)_dB: at 18 dB Es/N0, m_out=8 lands 0.41
        dB off the bound where m_out=4 loses 3.11 dB; at 14 dB it is 0.25 dB
        against 1.71 dB -- the gap widens as noise stops hiding it. Because
        `sps` must clear `2 * m_out` here, this default is also what puts the
        `sps` default at 32. **Never pair 2 with pulse="iandd"**: the matched
        filter degenerates to a two-tap sum, the eye barely opens (measured
        lock statistic -0.34 at 2 against +0.95 at 4 on the same NRZ stream)
        and acquisition itself fails about half the time. Replaces the old `n`
        (NDA arm dumps per symbol): the cascade's own outputs now feed the
        carrier discriminator, so there is no separate arm to size.
    pulse : Literal["iandd", "rrc"], default "iandd"
        Matched-filter shape (default MPSK_RX_PULSE_IANDD).
    rrc_beta : float, default 0.35
        RRC roll-off in `[0, 1]` (default 0.35; RRC only).
    rrc_span : int, default 8
        RRC one-sided span in symbols (default 8; RRC only).
    bn_carrier : float, default 0.01
        Carrier loop noise bandwidth, **normalised to the symbol rate**
        (default 0.01). A carrier loop here closes around the matched filter,
        so its dead time is that filter's group delay — keep it a small
        fraction of the symbol rate, as a real receiver does.
    zeta : float, default 0.0
        Damping factor for both loops. **0 (the default) derives it** as
        `1/sqrt(2)` (MPSK_RX_ZETA_DEFAULT) — a constant rather than a
        computation, since nothing in this receiver moves the optimal damping
        and both loops already share one value. Read it back with
        mpsk_receiver_get_zeta().
    bn_timing : float, default 0.01
        Symbol-timing loop noise bandwidth, normalised to the symbol rate
        (default 0.01).
    lock_thresh : float, default 0.0
        Declare threshold for the carrier lock indicator, on the carrier lock
        EMA. **0 (the default) derives it** as `sigma_H0 * eta(Pfa)` = 0.4999
        at Pfa = 5e-6; pass a value only to pin one. The metric is
        `Re((z/|z|)^M)` smoothed by an EMA, whose noise-only sd is 0.1132 for
        EVERY M, so 0.4999 is 4.42 noise sigmas; to pin your own, divide your
        Pfa's z-score into 0.1132 rather than picking by feel. The drop
        threshold sits at 0.8x for level hysteresis and both directions are
        verify-counted (8 symbols up / 32 down). Read it back with
        `lock_thresh`.
    init_norm_freq : float, default 0.0
        The real IF **centre**, cycles/sample at the real input rate -- an IF
        at 0.2*fs is 0.2, and the halved value the LO actually uses is this
        object's business, not yours. Unlike the complex face, this one does
        not acquire from a cold zero: a real IF must be tuned near, so this is
        the centre a tap buys pull-in *around* rather than from nothing.
    differential : int, default 0
        bits(): differential (rotation-invariant) demap (default 0 = coherent).
    num_phases : int, default 0
        Matched-filter bank arms; a power of two. Sets the fractional-timing
        resolution to 1/num_phases of an output period. **0 (the default)
        derives it** as 64, the measured saturation point. The bank is sized by
        the POST-decimation rate, so this costs the same at sps=8 and sps=256.
    agc : int, default 1
        Level the front-end cascade so the timing detector's construct-time
        slope means what it says. The TED normalises by its OWN slope and
        nothing else, and that slope is computed for a UNIT-amplitude symbol
        stream -- amplitude enters the raw error as A^2 (Gardner) or A^1
        (DTTL), so a 4x level error is a 16x loop-gain error. With this on, an
        AGC sits inside the cascade just before the matched filter and drives
        the average power to the level a unit-amplitude symbol stream would
        have there, derived from the matched filter's own pulse energy rather
        than configured. On by default because the alternative is a receiver
        whose loop gain depends on how loud the input happened to be. Set 0 to
        reproduce the un-levelled behaviour exactly, which is the handle for
        attributing any measurement that moves. This is the receiver's ONLY AGC
        -- the carrier discriminator normalises by its own |z|^M and needs
        none. Read the applied gain back with `agc_gain_db`. On this face the
        AGC sits behind the R2C halfband, so it levels the analytic signal at
        the intermediate rate rather than the real input.
    bn_agc_ratio : float, default 0.0
        **0 (the default) derives it** as 0.05. The one AGC's noise bandwidth
        as a fraction of the SLOWEST loop it feeds -- min(bn_carrier,
        bn_timing), not the carrier's alone, since the AGC feeds the timing
        loop directly. Must be in (0, 1) and construction refuses otherwise,
        because an AGC at or above the bandwidth of a loop it feeds corrects
        the excursions that loop is itself producing and the two integrate
        against each other; the signal LEVEL is a slow property of the channel,
        not a disturbance to reject at loop speed. Exposed rather than fixed
        because the right separation depends on how fast the channel's level
        moves against how fast its phase and timing do, which is a property of
        the link. The cold start is not the loop's job either way -- the AGC
        seeds its gain from a direct measurement -- so slow is cheap here.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``MpskReceiver: invalid
        parameter (need m in {2,4,8}, sps >= m_out -- sps > 2*m_out on the
        real-input MpskReceiverR, whose cascade runs behind a 2:1 halfband,
        m_out even in [2, 8], 0 <= rrc_beta <= 1, rrc_span >= 1, num_phases a
        power of two >= 2, bn >= 0, zeta > 0, 0 < bn_agc_ratio < 1)``.

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import MpskReceiverR
    >>> obj = MpskReceiverR(
    ...     m=4,
    ...     sps=32.0,
    ...     m_out=0,
    ...     pulse="iandd",
    ...     rrc_beta=0.35,
    ...     rrc_span=8,
    ...     bn_carrier=0.01,
    ...     zeta=0.0,
    ...     bn_timing=0.01,
    ...     lock_thresh=0.0,
    ...     init_norm_freq=0.0,
    ...     differential=0,
    ...     num_phases=0,
    ...     agc=1,
    ...     bn_agc_ratio=0.0,
    ... )

    """
    def __init__(
        self,
        m: int = ...,
        sps: float = ...,
        m_out: int = ...,
        pulse: Literal["iandd", "rrc"] = "iandd",
        rrc_beta: float = ...,
        rrc_span: int = ...,
        bn_carrier: float = ...,
        zeta: float = ...,
        bn_timing: float = ...,
        lock_thresh: float = ...,
        init_norm_freq: float = ...,
        differential: int = ...,
        num_phases: int = ...,
        agc: int = ...,
        bn_agc_ratio: float = ...,
    ) -> None: ...

    def set_telemetry(
        self,
        tlm: object | None,
        prefix: str,
        decim: int = 1,
    ) -> None:
        """Attach (or detach) a telemetry context across the receiver.
        Registers the receiver's own "<prefix>.lock" probe (the carrier lock
        EMA), then the carrier loop's "<prefix>.car.e" / ".freq" / ".locked"
        and the symbol-timing loop's "<prefix>.sync.e" / ".ctrl" / ".rate" /
        ".lock" / ".locked" / ".mu" -- ten probes emitted once per recovered
        symbol -- then the front end's AGC under "<prefix>.agc"
        ("<prefix>.agc.gain_db" and "<prefix>.agc.level_db"; see
        agc_set_telemetry()). Twelve probes total, all thinned by decim.
        Passing NULL detaches everything.

        Instrumenting it matters because it is FIRST in the chain, and a level
        error is the one kind no downstream loop can correct for itself: a TED
        normalises by its own construct-time slope, so it reads a level error
        as a loop-gain error (A^2 Gardner, A DTTL) with no other reference to
        catch it. This receiver also makes the AGC the slowest of its three
        loops by construction -- mpsk_rx_agc_bn() derives its bandwidth as a
        fraction of the slowest loop it feeds, and bn_agc_ratio is validated to
        (0, 1) -- but that is a choice of THIS composition, and slowest does
        not by itself mean longest: settling is set by the bandwidth AND by how
        far the level starts from the reference, which is unknown at
        construction. Which is exactly why it has to be measured rather than
        inferred; the zero-referenced "<prefix>.agc.level_db" is what makes
        that possible.

        With agc = 0 at construction there is no AGC to attach and the two
        probes are simply absent (fourteen, not sixteen); this still returns
        DP_OK.

        Setup path, never hot; the context is borrowed and must outlive the
        attachment (SPSC rules in dp_tlm/dp_tlm_core.h).

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "rx".
        decim : int
            Emit every decim-th symbol (every decim-th gain update for the two
            AGC probes); >= 1.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_telemetry failed``, with the return code appended (gh-869).

        Warnings
        --------
        The two AGC probes are NOT at the symbol rate the other ten are. That
        AGC sits pre-terminal in the cascade (RateConverter's tap, ahead of the
        stage the timing loop steers) and emits once per gain-update event,
        i.e. every AGC_DECIM_DEFAULT samples of that fixed-rate stream -- so it
        reports on a grid that depends on the planned cascade, not on recovered
        symbols, and a run yields a different number of AGC records than
        carrier records. Compare the two by TIME, never by record index. This
        is deliberate: the AGC's bandwidth is quoted in the pre-terminal
        stream's units precisely so it is not coupled to the loop that is
        stretching the symbol grid (see RateConverter_enable_agc()).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import MpskReceiver
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 14)   # 15 probes x ~512 syms + headroom
        >>> rx = MpskReceiver(m=4, sps=4, m_out=2)
        >>> rx.set_telemetry(tlm, "rx")
        >>> len(tlm.probe_names)
        15
        >>> rng = np.random.default_rng(7)
        >>> syms = (1 - 2 * rng.integers(0, 2, 512)).astype(np.complex64)
        >>> x = np.repeat(syms, 4)
        >>> _ = rx.steps(x)
        >>> recs = tlm.read()
        >>> tlm.dropped        # size the ring, or the counts below diverge
        0
        >>> n_sync = len(recs[recs["probe"] == tlm.probe_id("rx.sync.e")])
        >>> n_car = len(recs[recs["probe"] == tlm.probe_id("rx.car.e")])
        >>> n_sync > 0 and n_sync == n_car
        True
        >>> n_agc = len(recs[recs["probe"] == tlm.probe_id("rx.agc.gain_db")])
        >>> n_agc > 0 and n_agc != n_sync   # cascade grid, not symbol grid
        True

        """

    def reset(self) -> None:
        """Re-seed the carrier and symbol-timing loops to their create-time
        state; preserve configuration.

        Clears the cascade's filter memory, the carrier and timing NCOs, the
        loop-filter integrators and the lock detectors, and returns the carrier
        estimate to init_norm_freq. The configuration (order, rate, pulse,
        bandwidths) is untouched, so the same input fed twice around a reset
        reproduces the same output bit-for-bit.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import MpskReceiver
        >>> rng = np.random.default_rng(0)
        >>> idx = rng.integers(0, 4, 300)
        >>> tx = np.repeat(np.exp(1j * (2 * np.pi * idx / 4 + np.pi / 4)), 8)
        >>> tx = tx.astype(np.complex64)
        >>> rx = MpskReceiver(m=4, sps=8, m_out=4)
        >>> first = rx.steps(tx)
        >>> rx.reset()                                # back to the cold state
        >>> np.array_equal(first, rx.steps(tx))       # same input, same output
        True

        """

    def steps(
        self,
        x: NDArray[np.float32],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Demodulate a real f32 block and return the recovered M-PSK symbols
        (one cf32 per recovered symbol period, ~ len(x)/sps outputs). Per
        sample the receiver pushes x through the matched DDCR -- an R2C
        halfband that decimates 2:1, then LO mix, decimating cascade, and a
        terminal polyphase stage whose bank IS the matched filter and whose
        selected arm IS the fractional symbol-timing delay -- then folds every
        output that stage produced into two loops: a Gardner symbol-timing loop
        steering the cascade's rate_ctrl port, and a carrier loop steering the
        LO's freq_ctrl port. That fold is one shared implementation with the
        complex face; only the front end above it differs. The carrier
        discriminator runs on the on-time strobe only -- a non-strobe output
        straddles two symbols, so its M-th power is intersymbol interference
        rather than carrier phase -- and it is the non-data-aided M-th-power
        error throughout, needing no data and no symbol timing: there is one
        discriminator, running from the first symbol to the last. The loop
        locks to one of m phases (M-fold ambiguity); resolve it with
        bits(differential) or a sync word. Read norm_freq for the tracked
        carrier and lock for the carrier lock metric.

        mpsk_receiver_steps() taking real samples: the R2C halfband makes them
        complex before anything else touches them, and the per-sample body is
        the same one. Requires a state built by mpsk_receiver_create_real().

        Parameters
        ----------
        x : NDArray[np.float32]
            Real f32 input samples.
        out : NDArray[np.complex64] | None
            Output symbols; caller provides max_out capacity.

        Returns
        -------
        NDArray[np.complex64]
            Number of symbols written.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import MpskReceiverR
        >>> rng = np.random.default_rng(3)
        >>> idx = rng.integers(0, 4, 2400)                  # QPSK symbols
        >>> bb = np.repeat(np.exp(2j * np.pi * idx / 4), 32)  # 32 sps
        >>> n = np.arange(bb.size)
        >>> x = (0.4 * bb * np.exp(2j * np.pi * 0.25 * n)).real  # IF at fs/4
        >>> x = np.ascontiguousarray(x.astype(np.float32))
        >>> rx = MpskReceiverR(m=4, sps=32, m_out=8, init_norm_freq=0.25)
        >>> sym = rx.steps(x)
        >>> sym.size                                        # ~ x_len / sps
        2398
        >>> rx.lock > 0.8                                   # carrier locked
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

    def bits(
        self,
        x: NDArray[np.float32],
        out: NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """Demodulate a real f32 block and return hard Gray-coded bits (log2(m)
        bytes of 0/1 per recovered symbol, LSB-first). Coherent by default; if
        the receiver was created with differential=1, each symbol's bits come
        from the phase DIFFERENCE between consecutive symbols
        (rotation-invariant — resolves the m-fold carrier ambiguity at ~2x the
        symbol-error rate). Same per-sample carrier/timing recovery as steps().

        mpsk_receiver_bits() taking real samples. Requires a state built by
        mpsk_receiver_create_real().

        Parameters
        ----------
        x : NDArray[np.float32]
            Real f32 input samples.
        out : NDArray[np.uint8] | None
            Output bytes (0/1); caller provides max_out capacity.

        Returns
        -------
        NDArray[np.uint8]
            Number of bits written.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import MpskReceiverR
        >>> rng = np.random.default_rng(3)
        >>> idx = rng.integers(0, 2, 2400)                  # BPSK payload bits
        >>> bb = np.repeat(np.exp(1j * np.pi * idx), 32)
        >>> n = np.arange(bb.size)
        >>> x = (0.4 * bb * np.exp(2j * np.pi * 0.25 * n)).real  # IF at fs/4
        >>> x = np.ascontiguousarray(x.astype(np.float32))
        >>> rx = MpskReceiverR(m=2, sps=32, m_out=8, init_norm_freq=0.25,
        ...                    bn_carrier=0.005)
        >>> b = rx.bits(x)                                  # 1 hard bit/symbol
        >>> b.size
        2398
        >>> # settled tail matches the payload, up to the BPSK
        >>> # inversion ambiguity
        >>> tail = np.mean(b[1500:2300] != idx[1500:2300])
        >>> round(float(min(tail, 1 - tail)), 3)
        0.0

        """

    def bits_max_out(self) -> int:
        """Largest number of samples bits() can return in the current state.

        Size an `out=` buffer with this before calling bits(), or use it to
        allocate one up front. The bound is this object's own: what it depends
        on is a property of the algorithm, so a header block on bits_max_out()
        replaces this text.

        Returns
        -------
        int
            Upper bound on the output length; the actual call may return fewer.
        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the MpskReceiverR has already been
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

        Raises ``RuntimeError`` if the MpskReceiverR has already been
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
        ``RuntimeError`` if the MpskReceiverR has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def agc_gain_db(self) -> float:
        """Gain the front-end AGC is applying, in dB; 0.0 when `agc=0`. The
        diagnostic for a level problem: a receiver that will not lock with a
        healthy `lock` statistic, or one whose timing loop behaves differently
        at two input levels, is asking about this number. It settles at
        -10*log10(P_in / P_ref), where P_ref is the power a unit-amplitude
        symbol stream has where the AGC sits, so a reading far from 0 dB says
        the input is far from the level the cascade was built for -- which is
        fine, and is what the AGC is for, but is worth knowing. Distinct from
        the cascade's filter response, which stays unity; the two multiply.
        """

    @property
    def norm_freq(self) -> float:
        """Carrier frequency the receiver is tracking, cycles/sample at the
        input rate: the create-time centre plus the loop's own estimate.
        """
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def lock(self) -> float:
        """EMA of the carrier lock signal."""

    @property
    def zeta(self) -> float:
        """Loop damping actually in use. Reads back the DERIVED `1/sqrt(2)`
        when the constructor was given 0, or whatever was pinned instead.
        Everything derived is reported (docs/design/mpsk.md §8.1), on the same
        argument as `RateConverter.stages`: without the readback, passing 0 is
        an instruction whose result nobody can see.
        """

    @property
    def num_phases(self) -> int:
        """Matched-filter bank arms actually in use. Reads back the DERIVED 64
        -- the measured saturation point, against the 1024 that shipped -- when
        the constructor was given 0. See `zeta` for why every derived value is
        reported.
        """

    @property
    def lock_thresh(self) -> float:
        """Handover declare threshold actually in use. Reads back the DERIVED
        `sigma_H0 * eta(Pfa)` = 0.4999 at `Pfa = 5e-6` when the constructor was
        given 0. See `zeta` for why every derived value is reported.
        """

    @property
    def lock_drop_thresh(self) -> float:
        """Carrier DROP threshold actually in use -- 0.8x `lock_thresh`, the
        level hysteresis the declare/drop pair is stated with. Exposed for the
        same reason as the declare side: anything reading `lock` against its
        decision needs BOTH edges, and computing `0.8 *` at the call site is a
        second copy of a rule this object owns. Both carrier detectors are
        initialised from this pair, so one number describes them both.
        """

    @property
    def sync_lock_thresh(self) -> float:
        """Timing DECLARE threshold on the `sync.lock` statistic. Not the
        carrier's number and not derived the same way: symsync sizes its block
        length and threshold together from (rolloff, esno_min, pfa, pd), so
        this reads back that geometry's answer. A caller plotting `sync.lock`
        needs this rather than `lock_thresh`, which belongs to a different
        statistic on a different clock.
        """

    @property
    def sync_lock_drop_thresh(self) -> float:
        """Timing DROP threshold on `sync.lock`. Equal to `sync_lock_thresh`
        when the timing loop carries no level hysteresis (up = down =
        threshold, the symsync default), so the two reading the same is
        information, not a bug -- the timing decision's hysteresis is in its
        verify COUNTS rather than its levels.
        """

    @property
    def bn_agc_ratio(self) -> float:
        """AGC bandwidth as a fraction of the slowest loop it feeds, actually
        in use. Reads back the DERIVED 0.05 when the constructor was given 0.
        See `zeta` for why every derived value is reported.
        """

    @property
    def locked(self) -> int:
        """Binary carrier-lock flag from the hysteretic (verify-counted)
        detector on `lock` -- de-chattered, unlike the raw metric. It declares
        after 8 consecutive symbols above `lock_thresh` and withdraws after 32
        below `lock_drop_thresh`, so it answers 'is this receiver locked'
        rather than 'was the statistic above the line on this symbol'. **It is
        an INDICATOR and nothing else**: it steers no loop, gates no output,
        and the M-th-power NDA discriminator runs from the first strobe whether
        or not this has declared. So a caller uses it to size a measurement
        window, and a wrong reading costs them that window and costs the
        demodulator nothing. `lock_time` dates its first declaration.
        """

    @property
    def lock_time(self) -> int:
        """Symbols from reset to the FIRST carrier-lock declaration, or -1 if
        the receiver has not locked yet -- the acquisition time as a number,
        rather than something a caller has to infer by polling `locked` in a
        loop. Dated by the same hysteretic detector `locked` reports, so the
        two cannot disagree. In SYMBOLS, not seconds: `bn_carrier` and
        `bn_timing` are both normalised to the symbol rate, so a settling
        budget quoted in symbols is comparable across every input rate, and a
        caller holding Rs divides once. Only the FIRST declaration is dated --
        a drop and re-acquire does not restamp it, because the question this
        answers is 'how long did this receiver take to lock', not 'when did it
        last hold'. `reset()` clears it to -1.
        """

    @property
    def timing_rate(self) -> float:
        """Smoothed tracked samples per symbol — departs from the nominal `sps`
        by exactly the sample-clock offset the timing loop is tracking.
        """

    @property
    def m(self) -> int:
        """constellation order M (2, 4, 8)."""

    @property
    def sps(self) -> float:
        """samples per symbol at the receiver's input."""

    @property
    def m_out(self) -> int:
        """Terminal outputs per symbol (the old `n`, now the cascade's)."""

    @property
    def clipped(self) -> int:
        """Has the cascade's CIC stage clipped its input since the last reset?
        A CIC bounds its input to |Re|, |Im| <= 1.0 and clips silently past
        that -- the output stays finite and plausible, merely distorted, at a
        cost of ~25 dB of EVM that no lock metric reveals. Always 0 for a plan
        with no CIC stage.
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


    def __enter__(self) -> "MpskReceiverR":
        """Enter a context manager, returning this object.

        Lets a MpskReceiverR be used in a `with` statement so its C resources
        are released deterministically on exit rather than at collection time.

        Returns
        -------
        MpskReceiverR
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the MpskReceiverR.

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
