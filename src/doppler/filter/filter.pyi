# filter/filter.pyi — type stubs for the filter C extension.
import numpy as np
from numpy.typing import NDArray

class FIR:
    """Direct-form FIR filter with real or complex tap coefficients.

    Parameters
    ----------
    taps : NDArray[np.float32] or NDArray[np.complex64]
        Filter tap coefficients.  ``float32`` selects the real-tap path
        (1 FMA/tap); ``complex64`` selects the full complex-multiply path.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.filter import FIR
    >>> h = np.array([0.5, 0.25, 0.125], dtype=np.float32)
    >>> f = FIR(h)
    >>> f.num_taps
    3
    >>> f.is_real
    True
    >>> x = np.zeros(4, dtype=np.complex64); x[0] = 1+0j
    >>> y = f.execute(x)
    >>> float(y[0].real)
    0.5
    """

    def __init__(
        self,
        taps: NDArray[np.float32] | NDArray[np.complex64],
    ) -> None: ...
    def reset(self) -> None:
        """Reset delay line to zero; preserve taps and buffer capacity."""

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Filter a block of CF32 samples.

        Returns a zero-copy view into a pre-allocated internal buffer.
        Copy the output before calling execute again if you need to keep it.
        """

    @property
    def num_taps(self) -> int:
        """Number of tap coefficients."""

    @property
    def is_real(self) -> bool:
        """True if created with float32 taps, False if complex64."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "FIR": ...
    def __exit__(self, *args: object) -> None: ...
