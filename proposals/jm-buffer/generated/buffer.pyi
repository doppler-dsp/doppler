# buffer/buffer.pyi — type stubs for the buffer C extension.
from typing import final
import numpy as np
from numpy.typing import NDArray

@final
class F32Buffer:
    """F32Buffer component.

    Parameters
    ----------
    n_samples : int, default 0
        n_samples constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from dpring.buffer import F32Buffer
    >>> obj = F32Buffer(n_samples=0)
    >>> obj.get_gain()
    0.0

    """
    def __init__(self, n_samples: int = 0) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def wait(self, n: int) -> NDArray[np.complex64]:
        """Wait."""

    def consume(self, n: int) -> None:
        """Consume."""

    def available(self) -> int:
        """Available."""

    def closed(self) -> int:
        """Closed."""

    def close(self) -> None:
        """Close."""
    def get_gain(self) -> float:
        """Return current gain."""

    def set_gain(self, value: float) -> None:
        """Set gain."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "F32Buffer":
        """Enter a context manager, returning this object.

        Lets a F32Buffer be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        F32Buffer
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the F32Buffer.

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
