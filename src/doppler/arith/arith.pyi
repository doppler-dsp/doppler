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
        """Accumulate one Q15 sample into the running total. The sample is sign-extended to 64 bits before addition, ensuring that negative samples subtract correctly from the accumulator without wrap.

        Parameters
        ----------
        x : int
            Q15 input sample (int16_t, range `[-32768, 32767]`).

        Examples
        --------
        >>> from doppler.arith import AccQ15
        >>> obj = AccQ15(0)
        >>> obj.step(100)
        >>> obj.step(200)
        >>> obj.get()
        300

        """

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
        """Accumulate one Q8 sample into the running total. The sample is sign-extended to 32 bits before addition so negative samples correctly subtract from the accumulator.

        Parameters
        ----------
        x : int
            Q8 input sample (int8_t, range `[-128, 127]`).

        Examples
        --------
        >>> from doppler.arith import AccQ8
        >>> obj = AccQ8(0)
        >>> obj.step(10)
        >>> obj.step(20)
        >>> obj.get()
        30

        """

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
    """Elementwise saturating two's complement add of two Q15 arrays.

    Parameters
    ----------
    a : NDArray[np.int16]
        First input array (int16_t).
    b : NDArray[np.int16]
        Second input array (int16_t), same length as a.

    Returns
    -------
    NDArray[np.int16]
        Output.

    Examples
    --------
    >>> from doppler.arith import add_q15
    >>> import numpy as np
    >>> a = np.array([100, 20000, -20000], dtype=np.int16)
    >>> b = np.array([50,  20000, -20000], dtype=np.int16)
    >>> add_q15(a, b).tolist()
    [150, 32767, -32768]

    """

def sub_q15(a: NDArray[np.int16], b: NDArray[np.int16]) -> NDArray[np.int16]:
    """Elementwise saturating two's complement subtract of two Q15 arrays.

    Parameters
    ----------
    a : NDArray[np.int16]
        Minuend array (int16_t).
    b : NDArray[np.int16]
        Subtrahend array (int16_t), same length as a.

    Returns
    -------
    NDArray[np.int16]
        Output.

    Examples
    --------
    >>> from doppler.arith import sub_q15
    >>> import numpy as np
    >>> a = np.array([100,  0, -32768], dtype=np.int16)
    >>> b = np.array([50,   0,     10], dtype=np.int16)
    >>> sub_q15(a, b).tolist()
    [50, 0, -32768]

    """

def mul_q15(a: NDArray[np.int16], b: NDArray[np.int16]) -> NDArray[np.int16]:
    """Elementwise Q15 multiply with round-half-up: out[i] = sat16((a[i]*b[i] + 16384) >> 15).

    Parameters
    ----------
    a : NDArray[np.int16]
        First input array (int16_t).
    b : NDArray[np.int16]
        Second input array (int16_t), same length as a.

    Returns
    -------
    NDArray[np.int16]
        Output.

    Examples
    --------
    >>> from doppler.arith import mul_q15
    >>> import numpy as np
    >>> a = np.array([16384, 16384, 32767], dtype=np.int16)
    >>> b = np.array([16384, -16384, 32767], dtype=np.int16)
    >>> mul_q15(a, b).tolist()
    [8192, -8192, 32766]

    """

def dot_q15(a: NDArray[np.int16], b: NDArray[np.int16]) -> int:
    """Inner product of two Q15 arrays. Returns the raw Q30 accumulation as int64_t. Shift right 15 to get a Q15 scalar.

    Parameters
    ----------
    a : NDArray[np.int16]
        First input array (int16_t).
    b : NDArray[np.int16]
        Second input array (int16_t), same length as a.

    Returns
    -------
    int
        Raw Q30 accumulation (int64_t).

    Examples
    --------
    >>> from doppler.arith import dot_q15
    >>> import numpy as np
    >>> a = np.array([100, 200, 300], dtype=np.int16)
    >>> b = np.array([1, 2, 3], dtype=np.int16)
    >>> dot_q15(a, b)
    1400

    """

def shl_q15(a: NDArray[np.int16], n: int) -> NDArray[np.int16]:
    """Elementwise arithmetic left shift of a Q15 array with saturation. Equivalent to multiplying by 2^n in fixed-point.

    Parameters
    ----------
    a : NDArray[np.int16]
        Input array (int16_t).
    n : int
        Shift count (non-negative integer).

    Returns
    -------
    NDArray[np.int16]
        Output.

    Examples
    --------
    >>> from doppler.arith import shl_q15
    >>> import numpy as np
    >>> a = np.array([8192, 16384, 20000], dtype=np.int16)
    >>> shl_q15(a, 1).tolist()
    [16384, 32767, 32767]

    """

def shr_q15(a: NDArray[np.int16], n: int) -> NDArray[np.int16]:
    """Elementwise arithmetic right shift of a Q15 array with round-half-up. Equivalent to dividing by 2^n.

    Parameters
    ----------
    a : NDArray[np.int16]
        Input array (int16_t).
    n : int
        Shift count (non-negative integer).

    Returns
    -------
    NDArray[np.int16]
        Output.

    Examples
    --------
    >>> from doppler.arith import shr_q15
    >>> import numpy as np
    >>> a = np.array([100, 101, 102, -100], dtype=np.int16)
    >>> shr_q15(a, 2).tolist()
    [25, 25, 26, -25]

    """

def add_q8(a: NDArray[np.int8], b: NDArray[np.int8]) -> NDArray[np.int8]:
    """Elementwise saturating two's complement add of two Q8 arrays.

    Parameters
    ----------
    a : NDArray[np.int8]
        First input array (int8_t).
    b : NDArray[np.int8]
        Second input array (int8_t), same length as a.

    Returns
    -------
    NDArray[np.int8]
        Output.

    Examples
    --------
    >>> from doppler.arith import add_q8
    >>> import numpy as np
    >>> a = np.array([50, 100, -100], dtype=np.int8)
    >>> b = np.array([50,  30,  -50], dtype=np.int8)
    >>> add_q8(a, b).tolist()
    [100, 127, -128]

    """

def sub_q8(a: NDArray[np.int8], b: NDArray[np.int8]) -> NDArray[np.int8]:
    """Elementwise saturating two's complement subtract of two Q8 arrays.

    Parameters
    ----------
    a : NDArray[np.int8]
        Minuend array (int8_t).
    b : NDArray[np.int8]
        Subtrahend array (int8_t), same length as a.

    Returns
    -------
    NDArray[np.int8]
        Output.

    Examples
    --------
    >>> from doppler.arith import sub_q8
    >>> import numpy as np
    >>> a = np.array([50,   0, -128], dtype=np.int8)
    >>> b = np.array([30,   0,   10], dtype=np.int8)
    >>> sub_q8(a, b).tolist()
    [20, 0, -128]

    """

def mul_q8(a: NDArray[np.int8], b: NDArray[np.int8]) -> NDArray[np.int8]:
    """Elementwise Q8 multiply with round-half-up: out[i] = sat8((a[i]*b[i] + 64) >> 7).

    Parameters
    ----------
    a : NDArray[np.int8]
        First input array (int8_t).
    b : NDArray[np.int8]
        Second input array (int8_t), same length as a.

    Returns
    -------
    NDArray[np.int8]
        Output.

    Examples
    --------
    >>> from doppler.arith import mul_q8
    >>> import numpy as np
    >>> a = np.array([64,  64, -64], dtype=np.int8)
    >>> b = np.array([64, -64,  64], dtype=np.int8)
    >>> mul_q8(a, b).tolist()
    [32, -32, -32]

    """

def dot_q8(a: NDArray[np.int8], b: NDArray[np.int8]) -> int:
    """Inner product of two Q8 arrays. Returns the raw Q14 accumulation as int32_t.

    Parameters
    ----------
    a : NDArray[np.int8]
        First input array (int8_t).
    b : NDArray[np.int8]
        Second input array (int8_t), same length as a.

    Returns
    -------
    int
        Raw Q14 accumulation (int32_t).

    Examples
    --------
    >>> from doppler.arith import dot_q8
    >>> import numpy as np
    >>> a = np.array([10, 20, 30], dtype=np.int8)
    >>> b = np.array([1, 2, 3], dtype=np.int8)
    >>> dot_q8(a, b)
    140

    """

def shl_q8(a: NDArray[np.int8], n: int) -> NDArray[np.int8]:
    """Elementwise arithmetic left shift of a Q8 array with saturation.

    Parameters
    ----------
    a : NDArray[np.int8]
        Input array (int8_t).
    n : int
        Shift count (non-negative integer).

    Returns
    -------
    NDArray[np.int8]
        Output.

    Examples
    --------
    >>> from doppler.arith import shl_q8
    >>> import numpy as np
    >>> a = np.array([10, 50, 64], dtype=np.int8)
    >>> shl_q8(a, 1).tolist()
    [20, 100, 127]

    """

def shr_q8(a: NDArray[np.int8], n: int) -> NDArray[np.int8]:
    """Elementwise arithmetic right shift of a Q8 array with round-half-up.

    Parameters
    ----------
    a : NDArray[np.int8]
        Input array (int8_t).
    n : int
        Shift count (non-negative integer).

    Returns
    -------
    NDArray[np.int8]
        Output.

    Examples
    --------
    >>> from doppler.arith import shr_q8
    >>> import numpy as np
    >>> a = np.array([10, 11, 12, -10], dtype=np.int8)
    >>> shr_q8(a, 2).tolist()
    [3, 3, 3, -2]

    """

def shl_i64(a: NDArray[np.int64], n: int) -> NDArray[np.int64]:
    """Elementwise logical left shift of an int64_t array. No saturation (caller ensures no overflow).

    Parameters
    ----------
    a : NDArray[np.int64]
        Input array (int64_t).
    n : int
        Shift count (non-negative integer; >= 63 yields 0).

    Returns
    -------
    NDArray[np.int64]
        Output.

    Examples
    --------
    >>> from doppler.arith import shl_i64
    >>> import numpy as np
    >>> a = np.array([100, 200, -200], dtype=np.int64)
    >>> shl_i64(a, 3).tolist()
    [800, 1600, -1600]

    """

def shr_i64(a: NDArray[np.int64], n: int) -> NDArray[np.int64]:
    """Elementwise arithmetic right shift of an int64_t array with round-half-up. Useful for normalising dot_q15 Q30 results back to Q15.

    Parameters
    ----------
    a : NDArray[np.int64]
        Input array (int64_t).
    n : int
        Shift count (non-negative integer; >= 63 is clamped to 63).

    Returns
    -------
    NDArray[np.int64]
        Output.

    Examples
    --------
    >>> from doppler.arith import dot_q15, shr_i64
    >>> import numpy as np
    >>> raw = dot_q15(
    ...     np.array([16384, 16384], dtype=np.int16),
    ...     np.array([16384, 16384], dtype=np.int16),
    ... )
    >>> shr_i64(np.array([raw], dtype=np.int64), 15).tolist()
    [16384]

    """
