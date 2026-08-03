# agc/agc.pyi — type stubs for the agc C extension.
from typing import final
import numpy as np
from numpy.typing import NDArray

@final
class AGC:
    """Construct a log-domain feedback AGC and return its heap state. The loop integrator starts at 0 dB (unity gain) and the power detector p_avg is pre-seeded to 10^(ref_db/10) linear, so the first block of on-target samples produces no transient.  Three parameters tune the closed-loop behaviour: ref_db sets the target, loop_bw sets the convergence speed, and alpha sets the detector smoothing.

    Parameters
    ----------
    ref_db : float, default 0.0
        Target output power in dB (e.g. 0.0 for unity power).
    loop_bw : float, default 0.0025
        Loop noise bandwidth in cycles/sample; the loop settles in roughly 1/(4*loop_bw) samples.  Smaller values are slower and smoother; keep well below 1/(4*decim) when using agc_steps().
    alpha : float, default 0.05
        Power-detector EMA coefficient in (0, 1]; smaller values smooth harder but react slower to envelope changes.

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
    def __init__(self, ref_db: float = ..., loop_bw: float = ..., alpha: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset the AGC loop state to its post-create condition. Sets gain_db back to 0 dB (unity), clears g_last, and re-seeds the power-detector EMA p_avg from the current ref_db so that the first post-reset block produces no transient.  All configuration fields (ref_db, loop_bw, alpha, decim, clip_db) are left untouched.  Use this to process a new, independent signal segment without re-allocating.

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
        """Process one complex sample through the per-sample AGC loop. Applies the current gain, measures the output power via the EMA detector, advances the loop-filter integrator, then square-clips the returned sample to clip_db.  The clip is applied after the detector update, so clipping never disturbs convergence.  With the default gain_update_period == 1 this is the exact per-sample reference path; with gain_update_period P > 1 the detector and gain-apply still run every sample but the loop-filter command (and the exp10/log10 it needs) refreshes once per P samples — a zero-order hold on the gain that amortises the transcendentals on a sample-rate hot loop, the streaming analogue of agc_steps()' decimation. agc_steps() is the faster block equivalent; neither is bit-identical to the P == 1 loop once decimated, but both converge to the same steady state.

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
        >>> agc2.step(4.0+0.0j)  # 12 dB loud; first sample passes at unity gain
        (4+0j)
        >>> round(agc2.gain_db, 6)  # loop starts driving gain negative
        -0.024276

        """

    def steps(self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Process a block of complex samples through the decimated AGC loop. Splits the input into chunks of decim samples.  Within each chunk the gain is linearly interpolated from the previous chunk's end value to the new loop-filter output (a first-order hold) so there is no inter-chunk gain staircase.  The detector and loop filter run once per chunk on the chunk's mean power — O(n/decim) control-loop work versus O(n) for agc_step().  The output array may alias the input (in-place).

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

    def set_telemetry(self, tlm: object | None, prefix: str, decim: int = 1) -> None:
        """Attach (or detach) a telemetry context and register the AGC's probes on it. Registers one probe, "<prefix>.gain_db" — the loop-filter integrator (the commanded gain in dB), recorded once per gain-update event and further thinned by decim.  Passing NULL detaches (probe sites revert to their single-branch disabled cost); re-attaching after a reset is idempotent (same name -> same probe id).  Setup path, never hot: call before the producer thread starts stepping, and keep every object attached to one context on that one thread (the ring is SPSC — see telemetry/telemetry.h).  The context is borrowed, not owned: it must outlive the attachment.

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "agc" or "rx.agc".
        decim : int
            Emit every decim-th gain update; >= 1.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.agc import AGC
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
        >>> agc.set_telemetry(tlm, "agc")
        >>> tlm.probe_names()
        {'agc.gain_db': 0}
        >>> x = (0.5 + 0j) * np.ones(256, dtype=np.complex64)
        >>> _ = agc.steps(x)
        >>> recs = tlm.read()          # one record per decim-chunk update
        >>> len(recs) == 256 // agc.decim
        True
        >>> bool(recs["value"][-1] > recs["value"][0])  # gain rising toward ref
        True

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
        was constructed with. Length is validated against `state_bytes()` before
        the blob is handed to the C core, and the core may reject it as well.

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
        """Return the gain (in dB) actually applied to the most recent sample. Computes 20*log10(g_last), where g_last is the linear multiplier that was used on the most recently processed sample.  This differs from gain_db (the loop integrator's current command) because the loop filter advances the command one step ahead after each sample: immediately after agc_step() gain_db already reflects the updated command while applied_gain_db still reflects what the signal actually saw.  At loop convergence the two values are numerically equal.  At create/reset both are 0.0 dB (unity)."""

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
        instead, or use the object as a context manager, which calls it on exit.

        Idempotent: calling it again on an already-released object does nothing.
        Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "AGC":
        """Enter a context manager, returning this object.

        Lets a AGC be used in a `with` statement so its C resources are released
        deterministically on exit rather than at collection time.

        Returns
        -------
        AGC
            This same object, not a copy.
        """

    def __exit__(self, exc_type: object | None = ..., exc: object | None = ..., tb: object | None = ...) -> None:
        """Exit a context manager, releasing the AGC.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never suppresses
        one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.
        """
