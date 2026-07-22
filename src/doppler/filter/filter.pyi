# filter/filter.pyi — type stubs for the filter C extension.
import numpy as np
from numpy.typing import NDArray

class FIR:
    """Create a FIR filter from complex CF32 tap coefficients. Implements a direct-form FIR convolution: `y[n]` = sum_k `h[k]`*`x[n-k]`. The tap array is copied at creation; the caller may free it afterward. Use fir_create_real() instead when all imaginary parts are zero — that path costs 1 FMA/tap versus 2 FMA + permute + mul here.

    Parameters
    ----------
    taps : NDArray[np.complex64]
        Array of num_taps CF32 coefficients (I+jQ each), copied.

    """
    def __init__(self, taps: NDArray[np.complex64] = ...) -> None: ...

    def reset(self) -> None:
        """Zero the delay line; preserve taps and scratch capacity. After a reset the filter behaves identically to a freshly constructed instance of the same length, without paying the allocation cost again. Call this between unrelated signal segments to prevent inter-segment leakage through the delay line.

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

    def execute(self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Filter n_in CF32 samples and write the results to out. Each output sample is the inner product of the tap vector with the current delay line.  The delay line is updated with each input sample so state carries over across successive calls — process frames of any size without gaps or overlap.  The scratch buffer is grown lazily on the first call and reused on subsequent calls of the same size.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

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
        """Max output length execute() can produce for the current state."""

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def num_taps(self) -> int:
        """Number of tap coefficients supplied at creation. This equals the filter group delay plus one, and determines the minimum input block length for which no latency is observable."""

    @property
    def is_real(self) -> bool:
        """True when the filter was created with real-valued tap coefficients. Real-tap filters (fir_create_real) use a cheaper inner loop: 1 FMA/tap versus the 2 FMA + lane permute required for complex multiplication. Use this flag to confirm which constructor path was used at runtime."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "FIR": ...

    def __exit__(self, *args: object) -> None: ...

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

        O(1): add x, drop the sample leaving the window, return `acc · scale` (=
        `gain · acc / len`) — one multiply.

        Parameters
        ----------
        x : complex
            One input sample.

        Returns
        -------
        complex
            The gained window mean after admitting x.
        """

    def steps(self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Filter a block: write the gained moving average of each sample.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def reset(self) -> None:
        """Clear the window (zero the ring and the running sum); keep the configured length and gain.
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def len(self) -> int:
        """Len."""

    @property
    def gain(self) -> float:
        """Current output gain."""
    @gain.setter
    def gain(self, value: float) -> None: ...

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "MovingAverage": ...

    def __exit__(self, *args: object) -> None: ...

def design_lowpass(fpass: float = 0.4, fstop: float = 0.6, atten_db: float = 60.0) -> NDArray[np.float32]:
    """Kaiser-windowed-sinc lowpass FIR taps, auto-sized by kaiser_num_taps (Nyquist-normalised fpass/fstop band edges, unit-DC-gain float32 taps)."""
