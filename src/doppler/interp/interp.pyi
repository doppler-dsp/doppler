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

    Examples
    --------
    >>> from doppler.interp import InterpolatedTable
    >>> import numpy as np
    >>> t = InterpolatedTable(
    ...     np.array([0.0, 1.0, 2.0], dtype=np.complex128),
    ...     method="linear")
    >>> t.n
    3

    """
    def __init__(
        self,
        table: NDArray[np.complex128],
        method: Literal["floor", "nearest", "linear"] = "linear",
    ) -> None: ...

    def reset(self) -> None:
        """No-op: InterpolatedTable is purely a function of (table, method,
        point) with no running state to reset.

        Present only to satisfy the common object interface; each execute()
        depends solely on its inputs, so a call before or after reset() returns
        identical samples.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.interp import InterpolatedTable
        >>> table = InterpolatedTable(
        ...     np.array([0.0, 1.0, 2.0], dtype=np.complex128))
        >>> table.reset()                     # no running state to clear
        >>> table.execute(np.array([1.5]))   # unchanged: (table, point)
        array([1.5+0.j])

        """

    def execute(
        self,
        x: NDArray[np.float64],
        out: NDArray[np.complex128] | None = None,
    ) -> NDArray[np.complex128]:
        """Evaluate the table at each of n_in points via periodic
        interpolation.

        Each point is wrapped mod the table length (any real value, any sign)
        and evaluated per the configured method:

        - floor:   nearest index below (`table[floor(point) mod n]`)
        - nearest: closer of the floor/next index (0.5 ties pick floor)
        - linear:  linear fit across the two bracketing indices

        Parameters
        ----------
        x : NDArray[np.float64]
            Input.
        out : NDArray[np.complex128] | None
            Output buffer; must hold at least n_in values.

        Returns
        -------
        NDArray[np.complex128]
            min(n_in, max_out) interpolated points.

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
        """No fixed cap -- execute()'s output is always sized to exactly match
        its own input length, so an `out=` buffer only ever needs to be at
        least that many elements (never a larger, unrelated minimum).

        Returns
        -------
        int
            Output.
        """

    @property
    def n(self) -> int:
        """Table length (one period), read-only."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "InterpolatedTable":
        """Enter a context manager, returning this object.

        Lets a InterpolatedTable be used in a `with` statement so its C
        resources are released deterministically on exit rather than at
        collection time.

        Returns
        -------
        InterpolatedTable
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the InterpolatedTable.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never
        suppresses one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.
        """
