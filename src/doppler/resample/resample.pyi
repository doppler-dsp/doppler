# resample/resample.pyi — type stubs for the resample C extension.
import numpy as np
from numpy.typing import NDArray

class Resampler:
    """Resampler component.

    Parameters
    ----------
    rate : float, default 0.0
        rate constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.resample import Resampler
    >>> obj = Resampler(0.0)

    """
    def __init__(self, rate: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Execute."""

    def execute_ctrl(self, x: NDArray[np.complex64], ctrl: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Execute ctrl."""

    def reset(self) -> None:
        """Reset."""

    @property
    def rate(self) -> float:
        """Rate."""
    @rate.setter
    def rate(self, value: float) -> None: ...

    @property
    def num_phases(self) -> int:
        """Num phases."""

    @property
    def num_taps(self) -> int:
        """Num taps."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Resampler": ...

    def __exit__(self, *args: object) -> None: ...

class Halfbanddecimator:
    """Halfbanddecimator component.

    Parameters
    ----------
    h : Any, default ...
        h constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.resample import Halfbanddecimator
    >>> obj = Halfbanddecimator(...)

    """
    def __init__(self, h: NDArray[np.float32] = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Execute."""

    def reset(self) -> None:
        """Reset."""

    @property
    def rate(self) -> float:
        """Rate."""

    @property
    def num_taps(self) -> int:
        """Num taps."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Halfbanddecimator": ...

    def __exit__(self, *args: object) -> None: ...

def kaiser_beta(atten: float) -> float:
    """Kaiser beta."""

def kaiser_num_taps(num_phases: int, atten: float, pb: float, sb: float) -> int:
    """Kaiser num taps."""
