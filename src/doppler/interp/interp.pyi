# interp/interp.pyi — type stubs for the interp C extension.
from typing import final, Literal
import numpy as np
from numpy.typing import NDArray

@final
class InterpolatedTable:
    """Create an InterpolatedTable instance.

    Parameters
    ----------
    table : NDArray[np.complex128]
        Complex table, one period, length table_len.
    method : Literal["floor", "nearest", "linear"], default "linear"
        0 = floor, 1 = nearest, 2 = linear.

    """
    def __init__(self, table: NDArray[np.complex128] = ..., method: Literal["floor", "nearest", "linear"] = "linear") -> None: ...

    def reset(self) -> None:
        """No-op: InterpolatedTable is purely a function of (table, method, point) with no running state to reset.
        """

    def execute(self, x: NDArray[np.float64], out: NDArray[np.complex128] | None = None) -> NDArray[np.complex128]:
        """Evaluate the table at each of n_in points via periodic interpolation.

        Each point is wrapped mod the table length (any real value, any sign)
        and evaluated per the configured method: - floor: `table[floor(point)
        mod n]` - nearest: the floor or the next index, whichever `point` is
        closer to (an exact 0.5 tie selects the floor index) - linear: linear
        interpolation between the floor index and the next one, at the
        fractional position between them

        Parameters
        ----------
        x : NDArray[np.float64]
            Input.

        Returns
        -------
        NDArray[np.complex128]
            n_in (always).

        Examples
        --------
        >>> from doppler.interp import InterpolatedTable
        >>> import numpy as np
        >>> ramp = InterpolatedTable(
        ...     np.array([0.0, 1.0, 2.0], dtype=np.complex128))
        >>> ramp.execute(np.array([0.5, 1.1]))
        array([0.5+0.j, 1.1+0.j])

        """

    def execute_max_out(self) -> int:
        """Max output length execute() can produce for the current state."""

    @property
    def n(self) -> int:
        """Table length (one period), read-only."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "InterpolatedTable": ...

    def __exit__(self, *args: object) -> None: ...
