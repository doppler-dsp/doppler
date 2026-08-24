# filter/filter.pyi — type stubs for the filter C extension.
from typing import final
import numpy as np
from numpy.typing import NDArray

@final
class FIR:
    """Create a FIR filter from complex CF32 tap coefficients. Implements a
    direct-form FIR convolution: `y[n]` = sum_k `h[k]`*`x[n-k]`. The tap array
    is copied at creation; the caller may free it afterward. Use
    fir_create_real() instead when all imaginary parts are zero — that path
    costs 1 FMA/tap versus 2 FMA + permute + mul here.

    Parameters
    ----------
    taps : NDArray[np.complex64]
        Array of taps_len CF32 coefficients (I+jQ each), copied.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.filter import FIR
    >>> taps = np.array([0.25+0j, 0.5+0j, 0.25+0j], dtype=np.complex64)
    >>> fir = FIR(taps)
    >>> fir.num_taps
    3
    >>> fir.is_real
    False

    """
    def __init__(self, taps: NDArray[np.complex64]) -> None: ...

    def reset(self) -> None:
        """Zero the delay line; preserve taps and scratch capacity. After a
        reset the filter behaves identically to a freshly constructed instance
        of the same length, without paying the allocation cost again. Call this
        between unrelated signal segments to prevent inter-segment leakage
        through the delay line.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.filter import FIR
        >>> taps = np.array([0.25+0j, 0.5+0j, 0.25+0j], dtype=np.complex64)
        >>> fir = FIR(taps)
        >>> x = np.array([1+0j, 0+0j, 0+0j], dtype=np.complex64)
        >>> _ = fir.execute(x)
        >>> fir.reset()
        >>> y = fir.execute(x)
        >>> [round(float(v.real), 4) for v in y]
        [0.25, 0.5, 0.25]

        """

    def execute(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Filter n_in CF32 samples and write the results to out. Each output
        sample is the inner product of the tap vector with the current delay
        line. The delay line is updated with each input sample so state carries
        over across successive calls — process frames of any size without gaps
        or overlap. The scratch buffer is grown lazily on the first call and
        reused on subsequent calls of the same size.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.
        out : NDArray[np.complex64] | None
            Output buffer; caller must provide space for n_in CF32 values.

        Returns
        -------
        NDArray[np.complex64]
            Number of output samples written (always == n_in).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.filter import FIR
        >>> taps = np.array([0.25+0j, 0.5+0j, 0.25+0j], dtype=np.complex64)
        >>> fir = FIR(taps)
        >>> x = np.array([1+0j, 0+0j, 0+0j], dtype=np.complex64)
        >>> y = fir.execute(x)
        >>> y.dtype
        dtype('complex64')
        >>> y.shape
        (3,)
        >>> [round(float(v.real), 4) for v in y]
        [0.25, 0.5, 0.25]

        """

    def execute_max_out(self) -> int:
        """Always 0 -- FIR is a 1:1 transform, not a bounded-capacity one.

        fir_execute() always writes exactly n_in samples; there is no

        call-independent upper bound smaller than the input length for this

        function to report. An `out=` buffer must be sized to exactly

        `len(x)`, not to this function's return value.

        Returns
        -------
        int
            Output.
        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the FIR has already been destroyed.

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

        Raises ``RuntimeError`` if the FIR has already been destroyed.

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
        ``RuntimeError`` if the FIR has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def num_taps(self) -> int:
        """Number of tap coefficients supplied at creation. This equals the
        filter group delay plus one, and determines the minimum input block
        length for which no latency is observable.
        """

    @property
    def is_real(self) -> bool:
        """True when the filter was created with real-valued tap coefficients.
        Real-tap filters (fir_create_real) use a cheaper inner loop: 1 FMA/tap
        versus the 2 FMA + lane permute required for complex multiplication.
        Use this flag to confirm which constructor path was used at runtime.
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


    def __enter__(self) -> "FIR":
        """Enter a context manager, returning this object.

        Lets a FIR be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        FIR
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the FIR.

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
class MovingAverage:
    """MovingAverage component.

    Parameters
    ----------
    len : int, default 4
        len constructor parameter.
    gain : float, default 1.0
        gain constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.filter import MovingAverage
    >>> obj = MovingAverage(len=4, gain=1.0)

    """
    def __init__(self, len: int = ..., gain: float = ...) -> None: ...

    def step(self, x: complex) -> complex:
        """Slide the window by one sample; return the gained moving average.

        O(1): add x, drop the sample leaving the window, return `acc · scale`
        (= `gain · acc / len`) — one multiply.

        Parameters
        ----------
        x : complex
            One input sample.

        Returns
        -------
        complex
            The gained window mean after admitting x.

        Examples
        --------
        >>> from doppler.filter import MovingAverage
        >>> ma = MovingAverage(2)   # 2-sample sliding window, unit gain
        >>> [round(ma.step(v).real, 4) for v in (1 + 0j, 3 + 0j, 3 + 0j)]
        [0.5, 2.0, 3.0]

        """

    def steps(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Filter a block: write the gained moving average of each sample.

        Applies boxcar_step() to each input sample in turn, so the window sum
        and ring carry across the block exactly as they would sample by sample
        — a stream can be processed in frames of any size with no seam.
        Immediately after a reset the first len-1 outputs average over a
        partial (still filling) window and ramp in.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input samples.

        Returns
        -------
        NDArray[np.complex64]
            Output.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.filter import MovingAverage
        >>> ma = MovingAverage(3)                          # 3-sample window
        >>> x = np.ones(5, np.complex64)                   # unit step input
        >>> [round(v, 4) for v in ma.steps(x).real.tolist()]
        [0.3333, 0.6667, 1.0, 1.0, 1.0]

        """

    def reset(self) -> None:
        """Clear the window (zero the ring and the running sum); keep the
        configured length and gain.

        Returns the filter to its just-constructed state: the delay ring and
        the running window sum are zeroed while len and gain are preserved, so
        the next len-1 outputs ramp in over a partial window exactly as they
        did on a fresh instance. Call it at a segment boundary so samples from
        one capture do not average into an unrelated next one.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.filter import MovingAverage
        >>> ma = MovingAverage(2)                         # 2-sample window
        >>> _ = ma.steps(np.ones(4, np.complex64))        # fill the window
        >>> ma.reset()                                    # clear it
        >>> round(ma.step(1 + 0j).real, 4)                # ramps in from empty
        0.5

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the MovingAverage has already been
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

        Raises ``RuntimeError`` if the MovingAverage has already been
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
        ``RuntimeError`` if the MovingAverage has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def len(self) -> int:
        """window length (1 .. BOXCAR_MAX_LEN)."""

    @property
    def gain(self) -> float:
        """Current output gain."""
    @gain.setter
    def gain(self, value: float) -> None: ...

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "MovingAverage":
        """Enter a context manager, returning this object.

        Lets a MovingAverage be used in a `with` statement so its C resources
        are released deterministically on exit rather than at collection time.

        Returns
        -------
        MovingAverage
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the MovingAverage.

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

def design_lowpass(
    fpass: float = 0.4,
    fstop: float = 0.6,
    atten_db: float = 60.0,
) -> NDArray[np.float32]:
    """Kaiser-windowed-sinc lowpass FIR taps, auto-sized by kaiser_num_taps
    (Nyquist-normalised fpass/fstop band edges, unit-DC-gain float32 taps).
    """
