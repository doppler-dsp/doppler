# cvt/cvt.pyi — type stubs for the cvt C extension.
import numpy as np
from numpy.typing import NDArray

class F32ToI16:
    """F32ToI16 component.

    Parameters
    ----------
    scale : float, default 32768.0
        scale constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import F32ToI16
    >>> obj = F32ToI16(32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: float) -> int:
        """Process one input sample."""

    def steps(self, x: NDArray[np.float32], out: NDArray[np.int16] | None = None) -> NDArray[np.int16]:
        """Process a samples array."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "F32ToI16": ...

    def __exit__(self, *args: object) -> None: ...

class I16ToF32:
    """I16ToF32 component.

    Parameters
    ----------
    scale : float, default 32768.0
        scale constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import I16ToF32
    >>> obj = I16ToF32(32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: int) -> float:
        """Process one input sample."""

    def steps(self, x: NDArray[np.int16], out: NDArray[np.float32] | None = None) -> NDArray[np.float32]:
        """Process a samples array."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "I16ToF32": ...

    def __exit__(self, *args: object) -> None: ...

class F32ToI16U32:
    """F32ToI16U32 component.

    Parameters
    ----------
    scale : float, default 32768.0
        scale constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import F32ToI16U32
    >>> obj = F32ToI16U32(32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: float) -> int:
        """Process one input sample."""

    def steps(self, x: NDArray[np.float32], out: NDArray[np.uint32] | None = None) -> NDArray[np.uint32]:
        """Process a samples array."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "F32ToI16U32": ...

    def __exit__(self, *args: object) -> None: ...

class F32ToI16U64:
    """F32ToI16U64 component.

    Parameters
    ----------
    scale : float, default 32768.0
        scale constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import F32ToI16U64
    >>> obj = F32ToI16U64(32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: float) -> int:
        """Process one input sample."""

    def steps(self, x: NDArray[np.float32], out: NDArray[np.uint64] | None = None) -> NDArray[np.uint64]:
        """Process a samples array."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "F32ToI16U64": ...

    def __exit__(self, *args: object) -> None: ...

class I16U32ToF32:
    """I16U32ToF32 component.

    Parameters
    ----------
    scale : float, default 32768.0
        scale constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import I16U32ToF32
    >>> obj = I16U32ToF32(32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: int) -> float:
        """Process one input sample."""

    def steps(self, x: NDArray[np.uint32], out: NDArray[np.float32] | None = None) -> NDArray[np.float32]:
        """Process a samples array."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "I16U32ToF32": ...

    def __exit__(self, *args: object) -> None: ...

class I16U64ToF32:
    """I16U64ToF32 component.

    Parameters
    ----------
    scale : float, default 32768.0
        scale constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.cvt import I16U64ToF32
    >>> obj = I16U64ToF32(32768.0)

    """
    def __init__(self, scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: int) -> float:
        """Process one input sample."""

    def steps(self, x: NDArray[np.uint64], out: NDArray[np.float32] | None = None) -> NDArray[np.float32]:
        """Process a samples array."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "I16U64ToF32": ...

    def __exit__(self, *args: object) -> None: ...
