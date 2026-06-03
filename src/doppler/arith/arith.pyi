# arith/arith.pyi — type stubs for the arith C extension.
import numpy as np
from numpy.typing import NDArray

class AccQ15:
    """AccQ15 component.

    Parameters
    ----------
    acc : int, default 0
        acc state variable.

    Examples
    --------
    Create with defaults:

    >>> from doppler.arith import AccQ15
    >>> obj = AccQ15(0)
    >>> obj.get_acc()
    0

    Reset restores defaults:

    >>> obj.set_acc(42)
    >>> obj.reset()
    >>> obj.get_acc()
    0

    """
    def __init__(self, acc: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: int) -> None:
        """Process one input sample."""

    def steps(self, x: NDArray[np.int16]) -> None:
        """Process a samples array."""

    def get(self) -> int:
        """Get."""

    def dump(self) -> int:
        """Dump."""

    def madd(self, a: NDArray[np.int16], b: NDArray[np.int16]) -> None:
        """Madd."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "AccQ15": ...

    def __exit__(self, *args: object) -> None: ...

class AccQ8:
    """AccQ8 component.

    Parameters
    ----------
    acc : int, default 0
        acc state variable.

    Examples
    --------
    Create with defaults:

    >>> from doppler.arith import AccQ8
    >>> obj = AccQ8(0)
    >>> obj.get_acc()
    0

    Reset restores defaults:

    >>> obj.set_acc(42)
    >>> obj.reset()
    >>> obj.get_acc()
    0

    """
    def __init__(self, acc: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: int) -> None:
        """Process one input sample."""

    def steps(self, x: NDArray[np.int8]) -> None:
        """Process a samples array."""

    def get(self) -> int:
        """Get."""

    def dump(self) -> int:
        """Dump."""

    def madd(self, a: NDArray[np.int8], b: NDArray[np.int8]) -> None:
        """Madd."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "AccQ8": ...

    def __exit__(self, *args: object) -> None: ...

def add_q15(a: NDArray[np.int16], b: NDArray[np.int16]) -> NDArray[np.int16]:
    """Elementwise saturating two's complement add of two Q15 arrays."""

def sub_q15(a: NDArray[np.int16], b: NDArray[np.int16]) -> NDArray[np.int16]:
    """Elementwise saturating two's complement subtract of two Q15 arrays."""

def mul_q15(a: NDArray[np.int16], b: NDArray[np.int16]) -> NDArray[np.int16]:
    """Elementwise Q15 multiply with round-half-up: out[i] = sat16((a[i]*b[i] + 16384) >> 15)."""

def dot_q15(a: NDArray[np.int16], b: NDArray[np.int16]) -> int:
    """Inner product of two Q15 arrays. Returns the raw Q30 accumulation as int64_t. Shift right 15 to get a Q15 scalar."""

def shl_q15(a: NDArray[np.int16], n: int) -> NDArray[np.int16]:
    """Elementwise arithmetic left shift of a Q15 array with saturation. Equivalent to multiplying by 2^n in fixed-point."""

def shr_q15(a: NDArray[np.int16], n: int) -> NDArray[np.int16]:
    """Elementwise arithmetic right shift of a Q15 array with round-half-up. Equivalent to dividing by 2^n."""

def add_q8(a: NDArray[np.int8], b: NDArray[np.int8]) -> NDArray[np.int8]:
    """Elementwise saturating two's complement add of two Q8 arrays."""

def sub_q8(a: NDArray[np.int8], b: NDArray[np.int8]) -> NDArray[np.int8]:
    """Elementwise saturating two's complement subtract of two Q8 arrays."""

def mul_q8(a: NDArray[np.int8], b: NDArray[np.int8]) -> NDArray[np.int8]:
    """Elementwise Q8 multiply with round-half-up: out[i] = sat8((a[i]*b[i] + 64) >> 7)."""

def dot_q8(a: NDArray[np.int8], b: NDArray[np.int8]) -> int:
    """Inner product of two Q8 arrays. Returns the raw Q14 accumulation as int32_t."""

def shl_q8(a: NDArray[np.int8], n: int) -> NDArray[np.int8]:
    """Elementwise arithmetic left shift of a Q8 array with saturation."""

def shr_q8(a: NDArray[np.int8], n: int) -> NDArray[np.int8]:
    """Elementwise arithmetic right shift of a Q8 array with round-half-up."""

def shl_i64(a: NDArray[np.int64], n: int) -> NDArray[np.int64]:
    """Elementwise logical left shift of an int64_t array. No saturation (caller ensures no overflow)."""

def shr_i64(a: NDArray[np.int64], n: int) -> NDArray[np.int64]:
    """Elementwise arithmetic right shift of an int64_t array with round-half-up. Useful for normalising dot_q15 Q30 results back to Q15."""
