# agc/agc.pyi — type stubs for the agc C extension.
from typing import final
import numpy as np
from numpy.typing import NDArray

@final
class AGC:
    """Construct a log-domain feedback AGC and return its heap state. The loop
    integrator starts at 0 dB (unity gain) and the power detector p_avg is
    pre-seeded to 10^(ref_db/10) linear, so the first block of on-target
    samples produces no transient. Three parameters tune the closed-loop
    behaviour: ref_db sets the target, loop_bw sets the convergence speed, and
    alpha sets the detector smoothing.

    Parameters
    ----------
    ref_db : float, default 0.0
        Target output power in dB (e.g. 0.0 for unity power).
    loop_bw : float, default 0.0025
        Loop noise bandwidth in cycles/sample. The FILTER's time constant is
        1/(4*loop_bw) samples; the object settles more slowly than that on a
        quiet input, because the detector is inside the loop and measures in
        power (see the Linear-in-dB note above — measured 1.7x to 2.2x at -40
        dB in, worse at small alpha). Treat 1/(4*loop_bw) as a floor on
        settling, not an estimate of it. Smaller values are slower and
        smoother. With agc_steps(), the pairing rule is 4*decim*loop_bw <= 0.05
        — see "Choosing decim".
    alpha : float, default 0.05
        Power-detector EMA coefficient in (0, 1]; smaller values smooth harder
        but react slower to envelope changes.

    Examples
    --------
    >>> from doppler.agc import AGC
    >>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
    >>> agc.ref_db, agc.loop_bw, agc.alpha
    (0.0, 0.0025, 0.05)
    >>> agc.gain_db, agc.applied_gain_db
    (0.0, 0.0)
    >>> agc.decim, agc.clip_db
    (8, 120.0)

    """
    def __init__(
        self,
        ref_db: float = ...,
        loop_bw: float = ...,
        alpha: float = ...,
    ) -> None: ...

    def reset(self) -> None:
        """Reset the AGC loop state to its post-create condition. Sets gain_db
        back to 0 dB (unity), clears g_last, and re-seeds the power-detector
        EMA p_avg from the current ref_db so that the first post-reset block
        produces no transient. All configuration fields (ref_db, loop_bw,
        alpha, decim, clip_db) are left untouched. Use this to process a new,
        independent signal segment without re-allocating.

        Examples
        --------
        >>> from doppler.agc import AGC
        >>> import numpy as np
        >>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
        >>> _ = agc.steps(np.full(1000, 4.0+0.0j, dtype=np.complex64))
        >>> round(agc.gain_db, 1)   # converged to -12 dB
        -12.0
        >>> agc.reset()
        >>> agc.gain_db, agc.applied_gain_db
        (0.0, 0.0)

        """

    def step(self, x: complex) -> complex:
        """Process one complex sample through the per-sample AGC loop. Applies
        the current gain, measures the output power via the EMA detector,
        advances the loop-filter integrator, then square-clips the returned
        sample to clip_db. The clip is applied after the detector update, so
        clipping never disturbs convergence. With the default
        gain_update_period == 1 this is the exact per-sample reference path;
        with gain_update_period P > 1 the detector and gain-apply still run
        every sample but the loop-filter command (and the exp10/log10 it needs)
        refreshes once per P samples — a zero-order hold on the gain that
        amortises the transcendentals on a sample-rate hot loop, the streaming
        analogue of agc_steps()' decimation. agc_steps() is the faster block
        equivalent; neither is bit-identical to the P == 1 loop once decimated,
        but both converge to the same steady state.

        Parameters
        ----------
        x : complex
            Complex input sample.

        Returns
        -------
        complex
            Gained, clipped output sample x * 10^(gain_db/20) with each
            component independently clamped to +/-10^(clip_db/20).

        Examples
        --------
        >>> from doppler.agc import AGC
        >>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
        >>> agc.step(1.0+0.0j)   # unity gain at start, 0 dB in = 0 dB out
        (1+0j)
        >>> agc.gain_db           # loop already advanced from 0 dB
        0.0
        >>> agc2 = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
        >>> agc2.step(4.0+0.0j)  # 12 dB loud; first sample at unity gain
        (4+0j)
        >>> round(agc2.gain_db, 6)  # loop starts driving gain negative
        -0.024276

        """

    def steps(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Process a block of complex samples through the decimated AGC loop.
        Splits the input into chunks of decim samples. Within each chunk the
        gain is linearly interpolated from the previous chunk's end value to
        the new loop-filter output (a first-order hold) so there is no
        inter-chunk gain staircase. The detector and loop filter run once per
        chunk on the chunk's mean power — O(n/decim) control-loop work versus
        O(n) for agc_step(). The output array may alias the input (in-place).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.

        Examples
        --------
        >>> from doppler.agc import AGC
        >>> import numpy as np
        >>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
        >>> _ = agc.steps(np.full(1000, 4.0+0.0j, dtype=np.complex64))
        >>> round(agc.gain_db, 1)   # gain converged to -12 dB
        -12.0
        >>> x = np.full(8, 4.0+0.0j, dtype=np.complex64)
        >>> y = agc.steps(x)
        >>> y.shape, y.dtype
        ((8,), dtype('complex64'))
        >>> [round(abs(v)**2, 2) for v in y.tolist()]  # output power ~1.0
        [1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0]

        """

    def set_telemetry(
        self,
        tlm: object | None,
        prefix: str,
        decim: int = 1,
    ) -> None:
        """Attach (or detach) a telemetry context and register the AGC's probes
        on it. Registers two probes, both recorded once per gain-update event
        and further thinned by decim:

        - "<prefix>.gain_db" — the loop-filter integrator, i.e. the gain the
          loop is commanding, in dB.
        - "<prefix>.level_db" — the level the power detector measures,
          `10*log10(p_avg)`, in dB. This is the loop's *input*: the integrator
          drives `ref_db - level_db` to zero, so level_db is the
          zero-referenced settling indicator. Reading it says whether the loop
          has converged without knowing the true input level, which gain_db
          alone cannot — gain_db settles to an offset that depends on how loud
          the signal happens to be.

        The pair is emitted from one update, with level_db being the belief
        that update was answering (measured before the correction is applied).
        Passing NULL detaches (probe sites revert to their single-branch
        disabled cost); re-attaching after a reset is idempotent (same name ->
        same probe id). Setup path, never hot: call before the producer thread
        starts stepping, and keep every object attached to one context on that
        one thread (the ring is SPSC — see dp_tlm/dp_tlm_core.h). The context
        is borrowed, not owned: it must outlive the attachment.

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "agc" or "rx.agc".
        decim : int
            Emit every decim-th gain update; >= 1.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_telemetry failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.agc import AGC
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
        >>> agc.set_telemetry(tlm, "agc")
        >>> sorted(tlm.probe_names)
        ['agc.gain_db', 'agc.level_db']
        >>> x = (0.5 + 0j) * np.ones(4096, dtype=np.complex64)
        >>> _ = agc.steps(x)
        >>> recs = tlm.read()      # both probes, per decim-chunk update
        >>> gain = recs[recs["probe"] == tlm.probe_id("agc.gain_db")]["value"]
        >>> lvl = recs[recs["probe"] == tlm.probe_id("agc.level_db")]["value"]
        >>> len(gain) == len(lvl) == 4096 // agc.decim
        True
        >>> round(float(gain[-1]), 1)   # -6 dB input, 0 dB ref -> +6 dB gain
        6.0
        >>> round(float(lvl[-1]), 1)    # settled: measured level == ref
        0.0

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the AGC has already been destroyed.

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

        Raises ``RuntimeError`` if the AGC has already been destroyed.

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
        ``RuntimeError`` if the AGC has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def gain_db(self) -> float:
        """Gain db."""

    @property
    def applied_gain_db(self) -> float:
        """Return the gain (in dB) actually applied to the most recent sample.
        Computes 20*log10(g_last), where g_last is the linear multiplier that
        was used on the most recently processed sample. This differs from
        gain_db (the loop integrator's current command) because the loop filter
        advances the command one step ahead after each sample: immediately
        after agc_step() gain_db already reflects the updated command while
        applied_gain_db still reflects what the signal actually saw. At loop
        convergence the two values are numerically equal. At create/reset both
        are 0.0 dB (unity).
        """

    @property
    def ref_db(self) -> float:
        """Ref db."""
    @ref_db.setter
    def ref_db(self, value: float) -> None: ...

    @property
    def loop_bw(self) -> float:
        """Loop bw."""
    @loop_bw.setter
    def loop_bw(self, value: float) -> None: ...

    @property
    def alpha(self) -> float:
        """Alpha."""
    @alpha.setter
    def alpha(self, value: float) -> None: ...

    @property
    def decim(self) -> int:
        """Emit every decim-th event, >= 1."""
    @decim.setter
    def decim(self, value: int) -> None: ...

    @property
    def clip_db(self) -> float:
        """Clip db."""
    @clip_db.setter
    def clip_db(self, value: float) -> None: ...

    @property
    def gain_update_period(self) -> int:
        """Gain update period."""
    @gain_update_period.setter
    def gain_update_period(self, value: int) -> None: ...

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "AGC":
        """Enter a context manager, returning this object.

        Lets a AGC be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        AGC
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the AGC.

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

def settling_samples(
    loop_bw: float,
    alpha: float,
    gain_err_db: float,
    tol_db: float,
) -> int:
    """How many samples this loop needs to settle -- the design query a caller
    sizing a warm-up budget, a burst preamble or an acquisition guard has to
    answer. 1/(4*loop_bw) is the loop FILTER's time constant and not the
    object's: the detector sits inside the loop and measures in power, so a
    quiet input settles more slowly. Measured, the multiplier runs from about
    0.8 on a loud start to nearly 5 on a quiet one with a slow detector. This
    runs the real loop against a constant input and counts, so there is no
    fitted curve to go stale. gain_err_db is POSITIVE for a quiet input, which
    is the slow direction and the one to budget for. Returns 0 rather than a
    plausible guess when the arguments are invalid. Design-time only: it
    allocates and iterates.
    """
