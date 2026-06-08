# agc/agc.pyi — type stubs for the agc C extension.
import numpy as np
from numpy.typing import NDArray

class AGC:
    """Construct a log-domain feedback AGC and return its heap state. The loop integrator starts at 0 dB (unity gain) and the power detector @c p_avg is pre-seeded to @c 10^(ref_db/10) linear, so the first block of on-target samples produces no transient.  Three parameters tune the closed-loop behaviour: @p ref_db sets the target, @p loop_bw sets the convergence speed, and @p alpha sets the detector smoothing.

    Parameters
    ----------
    ref_db : float, default 0.0
        ref_db constructor parameter.
    loop_bw : float, default 0.0025
        loop_bw constructor parameter.
    alpha : float, default 0.05
        alpha constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.agc import AGC
    >>> obj = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)

    """
    def __init__(self, ref_db: float = ..., loop_bw: float = ..., alpha: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset the AGC loop state to its post-create condition. Sets @c gain_db back to 0 dB (unity), clears @c g_last, and re-seeds the power-detector EMA @c p_avg from the current @c ref_db so that the first post-reset block produces no transient.  All configuration fields (@c ref_db, @c loop_bw, @c alpha, @c decim, @c clip_db) are left untouched.  Use this to process a new, independent signal segment without re-allocating.

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
        """Process one input sample."""

    def steps(self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Process a block of complex samples through the decimated AGC loop. Splits the input into chunks of @c decim samples.  Within each chunk the gain is linearly interpolated from the previous chunk's end value to the new loop-filter output (a first-order hold) so there is no inter-chunk gain staircase.  The detector and loop filter run once per chunk on the chunk's mean power — O(n/decim) control-loop work versus O(n) for agc_step().  The output array may alias the input (in-place).

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

    @property
    def gain_db(self) -> float:
        """Gain db."""

    @property
    def applied_gain_db(self) -> float:
        """Return the gain (in dB) actually applied to the most recent sample. Computes @c 20*log10(g_last), where @c g_last is the linear multiplier that was used on the most recently processed sample.  This differs from @c gain_db (the loop integrator's current command) because the loop filter advances the command one step ahead after each sample: immediately after agc_step() @c gain_db already reflects the updated command while @c applied_gain_db still reflects what the signal actually saw.  At loop convergence the two values are numerically equal.  At create/reset both are 0.0 dB (unity)."""

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
        """Decim."""
    @decim.setter
    def decim(self, value: int) -> None: ...

    @property
    def clip_db(self) -> float:
        """Clip db."""
    @clip_db.setter
    def clip_db(self, value: float) -> None: ...

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "AGC": ...

    def __exit__(self, *args: object) -> None: ...
