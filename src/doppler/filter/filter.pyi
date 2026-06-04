# filter/filter.pyi — type stubs for the filter C extension.
import numpy as np
from numpy.typing import NDArray

class FIR:
    """Create a FIR filter from complex CF32 tap coefficients.

    Parameters
    ----------
    taps : NDArray[np.complex64], default ...
        taps constructor parameter.

    """
    def __init__(self, taps: NDArray[np.complex64] = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def execute(self, x: complex) -> NDArray[np.complex64]:
        """Filter n_in CF32 samples; write results to out.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex64]
            n_in on success, 0 on allocation failure.
        """

    @property
    def num_taps(self) -> int:
        """Number of tap coefficients."""

    @property
    def is_real(self) -> bool:
        """1 if filter was created with real taps, 0 if complex."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "FIR": ...

    def __exit__(self, *args: object) -> None: ...

class HBDecimQ15:
    """Allocate and initialise a fixed-point halfband 2:1 decimator.

    Parameters
    ----------
    h : NDArray[np.float32], default ...
        h constructor parameter.

    """
    def __init__(self, h: NDArray[np.float32] = ...) -> None: ...

    def execute(self, x: NDArray[np.int16]) -> NDArray[np.int16]:
        """Decimate a block of interleaved IQ int16 samples by 2.

        Input layout:  I0 Q0 I1 Q1 ... (2*n_in  int16_t values).

        Output layout: I0 Q0 I1 Q1 ... (2*n_out int16_t values), n_out <= n_in/2.


        If n_in is odd the trailing even IQ pair is buffered and consumed on

        the next call.  Output buffer must be allocated for at least

        2 * ((n_in + 1) / 2) int16_t elements to be safe.

        Parameters
        ----------
        x : NDArray[np.int16]
            Input.

        Returns
        -------
        NDArray[np.int16]
            Number of complex output samples written.
        """

    def reset(self) -> None:
        """Zero all delay rings and clear the pending-sample flag.
        """

    @property
    def num_taps(self) -> int:
        """Returns num_taps as supplied to hbdecim_q15_create."""

    @property
    def rate(self) -> float:
        """Always returns 0.5."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "HBDecimQ15": ...

    def __exit__(self, *args: object) -> None: ...
