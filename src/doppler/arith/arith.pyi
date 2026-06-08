# arith/arith.pyi — type stubs for the arith C extension.
import numpy as np
from numpy.typing import NDArray

class AccQ15:
    """Allocate and initialise an AccQ15 accumulator. The accumulator starts at the supplied initial value and may be driven sample-by-sample (step), in bulk (steps), or via multiply-accumulate (madd). The internal register is a 64-bit signed integer so it will not overflow in any realistic DSP workload.

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
        """Reset the accumulator to zero, mirroring the post-create state. Does not re-initialise to the constructor's acc value — always resets to zero, matching the default initial state for a clean sweep.

        Examples
        --------
        >>> from doppler.arith import AccQ15
        >>> obj = AccQ15(0)
        >>> obj.step(42)
        >>> obj.reset()
        >>> obj.get()
        0

        """

    def step(self, x: int) -> None:
        """Process one input sample."""

    def steps(self, x: NDArray[np.int16]) -> None:
        """Accumulate a contiguous block of Q15 samples. Equivalent to calling step() n times but faster for large arrays because the loop can be auto-vectorised by the compiler.

        Parameters
        ----------
        x : NDArray[np.int16]
            Input.

        Examples
        --------
        >>> from doppler.arith import AccQ15
        >>> import numpy as np
        >>> obj = AccQ15(0)
        >>> obj.steps(np.array([1, 2, 3, 4, 5], dtype=np.int16))
        >>> obj.get()
        15

        """

    def get(self) -> int:
        """Return the current accumulated value without resetting.

        Returns
        -------
        int
            Current accumulator value (int64_t).

        Examples
        --------
        >>> from doppler.arith import AccQ15
        >>> import numpy as np
        >>> obj = AccQ15(0)
        >>> obj.steps(np.array([10, 20, 30], dtype=np.int16))
        >>> obj.get()
        60

        """

    def dump(self) -> int:
        """Return the accumulated value and reset to zero.

        Returns
        -------
        int
            Accumulator value before the reset (int64_t).

        Examples
        --------
        >>> from doppler.arith import AccQ15
        >>> import numpy as np
        >>> obj = AccQ15(0)
        >>> obj.steps(np.array([1, 2, 3, 4, 5], dtype=np.int16))
        >>> obj.dump()
        15
        >>> obj.get()
        0

        """

    def madd(self, a: NDArray[np.int16], b: NDArray[np.int16]) -> None:
        """Multiply-accumulate: acc += sum(a[i] * b[i]) for i in [0, len(a)). Uses AVX2 when available.

        Parameters
        ----------
        a : NDArray[np.int16]
            First input array (int16_t).
        b : NDArray[np.int16]
            Second input array (int16_t), same length as a.

        Examples
        --------
        >>> from doppler.arith import AccQ15
        >>> import numpy as np
        >>> obj = AccQ15(0)
        >>> a = np.array([100, 200, 300], dtype=np.int16)
        >>> b = np.array([10, 20, 30], dtype=np.int16)
        >>> obj.madd(a, b)
        >>> obj.get()
        14000

        """

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "AccQ15": ...

    def __exit__(self, *args: object) -> None: ...

class AccQ8:
    """Allocate and initialise an AccQ8 accumulator. The accumulator starts at the supplied initial value and accepts Q8 (int8_t) samples via step(), steps(), or madd(). The 32-bit internal register handles up to roughly 16 million max-magnitude samples before wrap — sufficient for all standard DSP block sizes.

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
        """Reset the accumulator to zero, mirroring the post-create state. Always resets to zero regardless of the original constructor value, so it is safe to call at the start of any new accumulation window.

        Examples
        --------
        >>> from doppler.arith import AccQ8
        >>> obj = AccQ8(0)
        >>> obj.step(42)
        >>> obj.reset()
        >>> obj.get()
        0

        """

    def step(self, x: int) -> None:
        """Process one input sample."""

    def steps(self, x: NDArray[np.int8]) -> None:
        """Accumulate a contiguous block of Q8 samples. Equivalent to calling step() n times; the single loop is more amenable to auto-vectorisation than repeated method calls.

        Parameters
        ----------
        x : NDArray[np.int8]
            Input.

        Examples
        --------
        >>> from doppler.arith import AccQ8
        >>> import numpy as np
        >>> obj = AccQ8(0)
        >>> obj.steps(np.array([1, 2, 3, 4, 5], dtype=np.int8))
        >>> obj.get()
        15

        """

    def get(self) -> int:
        """Return the current accumulated value without resetting.

        Returns
        -------
        int
            Current accumulator value (int32_t).

        Examples
        --------
        >>> from doppler.arith import AccQ8
        >>> import numpy as np
        >>> obj = AccQ8(0)
        >>> obj.steps(np.array([10, 20, 30], dtype=np.int8))
        >>> obj.get()
        60

        """

    def dump(self) -> int:
        """Return the accumulated value and reset to zero.

        Returns
        -------
        int
            Accumulator value before the reset (int32_t).

        Examples
        --------
        >>> from doppler.arith import AccQ8
        >>> import numpy as np
        >>> obj = AccQ8(0)
        >>> obj.steps(np.array([1, 2, 3, 4, 5], dtype=np.int8))
        >>> obj.dump()
        15
        >>> obj.get()
        0

        """

    def madd(self, a: NDArray[np.int8], b: NDArray[np.int8]) -> None:
        """Multiply-accumulate: acc += sum(a[i] * b[i]) for i in [0, len(a)).

        Parameters
        ----------
        a : NDArray[np.int8]
            First input array (int8_t).
        b : NDArray[np.int8]
            Second input array (int8_t), same length as a.

        Examples
        --------
        >>> from doppler.arith import AccQ8
        >>> import numpy as np
        >>> obj = AccQ8(0)
        >>> a = np.array([10, 20, 30], dtype=np.int8)
        >>> b = np.array([1, 2, 3], dtype=np.int8)
        >>> obj.madd(a, b)
        >>> obj.get()
        140

        """

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
