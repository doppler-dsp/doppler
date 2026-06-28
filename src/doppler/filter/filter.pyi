# filter/filter.pyi — type stubs for the filter C extension.
import numpy as np
from numpy.typing import NDArray

class FIR:
    """Create a FIR filter from complex CF32 tap coefficients. Implements a direct-form FIR convolution: `y[n]` = sum_k `h[k]`*`x[n-k]`. The tap array is copied at creation; the caller may free it afterward. Use fir_create_real() instead when all imaginary parts are zero — that path costs 1 FMA/tap versus 2 FMA + permute + mul here.

    Parameters
    ----------
    taps : NDArray[np.complex64], default ...
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

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
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

class HBDecimQ15:
    """Allocate and initialise a fixed-point halfband 2:1 decimator. The FIR branch coefficients are supplied as float and converted internally to Q15 with a x0.5 polyphase rate scaling.  The full halfband prototype is sparse (every other tap is zero); supply only the non-zero FIR branch taps, not the full sparse prototype.

    Parameters
    ----------
    h : NDArray[np.float32], default ...
        Float FIR branch coefficients of length num_taps. Must be symmetric (`h[k]` == `h[num_taps-1-k]`).

    """
    def __init__(self, h: NDArray[np.float32] = ...) -> None: ...

    def execute(self, x: NDArray[np.int16]) -> NDArray[np.int16]:
        """Decimate a block of interleaved IQ int16 samples by 2. Input must be interleaved int16_t IQ pairs (I₀ Q₀ I₁ Q₁ …); pass a 1-D array of 2*n_complex elements.  Each pair of complex input samples produces one complex output sample, so an array of length 2N yields at most N output pairs (2N int16 output values).  If n_in is odd the trailing IQ pair is buffered and consumed on the next call.

        Parameters
        ----------
        x : NDArray[np.int16]
            Input.

        Returns
        -------
        NDArray[np.int16]
            Number of int16_t values written to out.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.filter import HBDecimQ15
        >>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)
        >>> dec = HBDecimQ15(h)
        >>> x = np.array([1000, 0, 1000, 0, 1000, 0, 1000, 0], dtype=np.int16)
        >>> y = dec.execute(x)
        >>> y.dtype
        dtype('int16')
        >>> y.shape
        (4,)
        >>> y.tolist()
        [0, 0, 625, 0]

        """

    def reset(self) -> None:
        """Zero all delay rings and clear the pending-sample flag. After a reset the decimator behaves identically to a freshly constructed instance: the four dual-write delay rings are zeroed and has_pending is cleared, so no partial IQ pair carries over.  Call this between unrelated signal segments to prevent inter-segment leakage.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.filter import HBDecimQ15
        >>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)
        >>> dec = HBDecimQ15(h)
        >>> x = np.array([1000, 0, 1000, 0, 1000, 0, 1000, 0], dtype=np.int16)
        >>> _ = dec.execute(x)
        >>> dec.reset()
        >>> y = dec.execute(x)
        >>> y.tolist()
        [0, 0, 625, 0]

        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def num_taps(self) -> int:
        """FIR branch length as supplied to the constructor. This is the count of non-zero symmetric taps in the FIR branch, not the full sparse halfband prototype length.  Useful for introspection when chaining multiple stages with programmatically computed filter banks."""

    @property
    def rate(self) -> float:
        """The sample-rate reduction factor; always 0.5 for 2:1 decimation. Exposed as a read-only property so pipelines can query the rate of each stage programmatically without hard-coding the 2:1 assumption."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "HBDecimQ15": ...

    def __exit__(self, *args: object) -> None: ...
