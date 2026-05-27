# source/source.pyi — type stubs for the source C extension.
import numpy as np
from numpy.typing import NDArray

class NCO:
    """NCO component.

    Parameters
    ----------
    norm_freq : float, default 0.0
        norm_freq constructor parameter.
    nmax : int, default 0
        nmax constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.source import NCO
    >>> obj = NCO(0.0, 0)

    """
    def __init__(self, norm_freq: float = ..., nmax: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def steps_u32(self) -> NDArray[np.uint32]:
        """Steps u32."""

    def steps_u32_scaled(self) -> NDArray[np.uint32]:
        """Steps u32 scaled."""

    def steps_u32_ovf(self) -> tuple[NDArray[np.uint32], NDArray[np.uint8]]:
        """Steps u32 ovf."""

    @property
    def norm_freq(self) -> float:
        """Norm freq."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def phase(self) -> int:
        """Phase."""
    @phase.setter
    def phase(self, value: int) -> None: ...

    @property
    def phase_inc(self) -> int:
        """Phase inc."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "NCO": ...

    def __exit__(self, *args: object) -> None: ...

class LO:
    """LO component.

    Parameters
    ----------
    norm_freq : float, default 0.0
        norm_freq constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.source import LO
    >>> obj = LO(0.0)

    """
    def __init__(self, norm_freq: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def steps(self) -> NDArray[np.complex64]:
        """Steps."""

    def steps_ctrl(self, ctrl: NDArray[np.float32]) -> NDArray[np.complex64]:
        """Steps ctrl."""

    @property
    def norm_freq(self) -> float:
        """Norm freq."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def phase(self) -> int:
        """Phase."""
    @phase.setter
    def phase(self, value: int) -> None: ...

    @property
    def phase_inc(self) -> int:
        """Phase inc."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "LO": ...

    def __exit__(self, *args: object) -> None: ...

class AWGN:
    """AWGN component.

    Parameters
    ----------
    seed : int, default 0
        seed constructor parameter.
    amplitude : float, default 1.0
        amplitude constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.source import AWGN
    >>> obj = AWGN(0, 1.0)

    """
    def __init__(self, seed: int = ..., amplitude: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def generate(self, n: int = ...) -> NDArray[np.complex64]:
        """Generate n complex CF32 AWGN samples."""

    def reseed(self, seed: int) -> None:
        """Reseed the RNG and reset state."""

    @property
    def amplitude(self) -> float:
        """Amplitude."""
    @amplitude.setter
    def amplitude(self, value: float) -> None: ...

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "AWGN": ...

    def __exit__(self, *args: object) -> None: ...
