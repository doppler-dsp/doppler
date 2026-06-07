# delay/delay.pyi — type stubs for the delay C extension.
import numpy as np
from numpy.typing import NDArray

class DelayCf64:
    """DelayCf64 component.

    Parameters
    ----------
    num_taps : int, default 1
        num_taps constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.delay import DelayCf64
    >>> obj = DelayCf64(num_taps=1)

    """
    def __init__(self, num_taps: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def push(self, x: complex) -> None:
        """push.

        Parameters
        ----------
        x : complex
            double complex parameter.
        """

    def ptr(self) -> NDArray[np.complex128]:
        """Ptr."""

    def push_ptr(self, x: complex) -> NDArray[np.complex128]:
        """Push ptr."""

    def write(self, x: complex) -> None:
        """write.

        Parameters
        ----------
        x : complex
            Input (double complex).
        """

    @property
    def num_taps(self) -> int:
        """Num taps."""

    @property
    def capacity(self) -> int:
        """Capacity."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "DelayCf64": ...

    def __exit__(self, *args: object) -> None: ...
