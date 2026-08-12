# util/util.pyi — type stubs for the util C extension.
def square_clip(y: complex, lin: float) -> complex:
    """Square-clip a complex sample: clip the real and imaginary parts
    independently to [-lin, lin] (a square region in the IQ plane).

    Parameters
    ----------
    y : complex
        Complex CF32 input sample.
    lin : float
        Per-component clip threshold (linear amplitude, >= 0). Values
        outside `[-lin, lin]` are clamped; values on the boundary are
        preserved exactly.

    Returns
    -------
    complex
        Sample with each component limited to `[-lin, lin]`.

    Examples
    --------
    >>> from doppler.util import square_clip
    >>> square_clip(0.5+0.25j, 1.0)   # within bounds, passed through
    (0.5+0.25j)
    >>> square_clip(2.0+0.5j, 1.0)    # real clipped, imag unchanged
    (1+0.5j)
    >>> square_clip(3.0-4.0j, 1.0)    # both components clipped
    (1-1j)
    >>> square_clip(0.5+0.5j, 0.25)   # smaller threshold clips both
    (0.25+0.25j)
    >>> square_clip(-2.0+0.0j, 1.0)   # negative real clipped
    (-1+0j)

    """

def saturate(v: float, lo: float, hi: float, nan_to: float) -> float:
    """Saturate a value into [lo, hi], total over every double including
    NaN and both infinities. The NaN destination is a parameter because
    which end is safe is domain knowledge: a gain control guarding a
    measured power wants the ceiling, a lock statistic wants the floor. Use
    it at the boundary where an untrusted value first becomes persistent
    state -- the input of an EMA, accumulator or integrator.

    `fmin`/`fmax` are not enough for this job. A plain `fmin(fmax(v, lo),
    hi)` propagates NaN on some platforms and silently returns a bound on
    others, and a hand-written `v > hi ? hi : v` leaves NaN untouched,
    because every comparison against NaN is false. This function has no
    fall-through: a value that is neither inside the interval, nor below
    it, nor above it can only be NaN.

    Parameters
    ----------
    v : float
        Value to saturate. Any double.
    lo : float
        Lower bound, returned for any `v < lo`.
    hi : float
        Upper bound, returned for any `v > hi`.
    nan_to : float
        Returned when `v` is NaN. Pick the end that is safe in the caller's
        own terms; it is usually `lo` or `hi`.

    Returns
    -------
    float
        `v` when `lo <= v <= hi`, otherwise `lo`, `hi` or `nan_to`.

    Notes
    -----
    Why the NaN destination is the caller's Which end is *safe* is domain
    knowledge, not arithmetic. A gain control guarding a measured power
    wants NaN at the **ceiling** — an unknown level must drive the gain
    down, because too little gain loses a signal while too much rails
    everything downstream. A lock statistic wants NaN at the **floor** — an
    unknown lock is not a lock. Baking either choice in would hand the
    wrong default to half its callers, so `nan_to` is a parameter and each
    call site states its own safe direction.

    Where to use it At the boundary where an untrusted value first becomes
    **persistent state** — the input of an EMA, an accumulator, or an
    integrator. Ahead of that boundary a bad value corrupts one output and
    is gone; past it, it is remembered and every quantity derived from it
    inherits the damage. One guard there makes the whole downstream chain
    total, where a clamp at each stage is several chances to miss one.

    Examples
    --------
    >>> from doppler.util import saturate
    >>> saturate(0.5, 0.0, 1.0, 1.0)     # inside the interval
    0.5
    >>> saturate(2.0, 0.0, 1.0, 1.0)     # above the ceiling
    1.0
    >>> saturate(-3.0, 0.0, 1.0, 1.0)    # below the floor
    0.0
    >>> saturate(float("inf"), 0.0, 1.0, 1.0)   # infinity is just above
    1.0
    >>> saturate(float("nan"), 0.0, 1.0, 1.0)   # NaN takes the caller's end
    1.0
    >>> saturate(float("nan"), 0.0, 1.0, 0.0)   # ... which may be the other
    0.0

    """
