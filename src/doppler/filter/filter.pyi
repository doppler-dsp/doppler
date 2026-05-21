# filter/filter.pyi — type stubs for the filter C extension.
import numpy as np
from numpy.typing import NDArray

class FIR:
    """FIR component.

    Examples
    --------
    Create with defaults:

    >>> from doppler.filter import FIR
    >>> obj = FIR()

    """
    def __init__(self, /, *args, **kwargs) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def execute(self, x: complex) -> NDArray[np.complex64]:
        """Execute."""

    @property
    def num_taps(self) -> int:
        """Num taps."""

    @property
    def is_real(self) -> bool:
        """Is real."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "FIR": ...

    def __exit__(self, *args: object) -> None: ...
