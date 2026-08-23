# interrupt/interrupt.pyi — type stubs for the interrupt C extension.
from typing import final

@final
class Interrupt:
    """Clear the flag, optionally install handlers, and remember what to undo.

    Parameters
    ----------
    signals : NDArray[np.int32]
        Signals to install on; empty arms nothing and the guard is only a
        handle to the flag.
    latency_ms : int, default 0
        Wait-slice override; 0 leaves the process setting alone, and only a
        non-zero value is restored.

    Raises
    ------
    OSError
        If construction fails. The exception message is ``cannot install a
        handler for one of the signals requested``.

    Examples
    --------
    >>> from doppler.interrupt import Interrupt
    >>> it = Interrupt([])
    >>> it.interrupted()
    0

    """
    def __init__(
        self,
        signals: NDArray[np.int32],
        latency_ms: int = ...,
    ) -> None: ...

    def interrupt(self) -> None:
        """Ask every blocking wait in this process to stop.

        The object's face onto dp_interrupt(). It takes a guard because that is
        how a method is called, not because the request is scoped to one -- the
        flag is process-wide, and a request through any guard is seen by every
        waiter.

        Examples
        --------
        >>> from doppler.interrupt import Interrupt
        >>> it = Interrupt([])
        >>> it.interrupt()
        >>> it.interrupted()
        1

        """

    def interrupted(self) -> int:
        """Non-zero once a stop has been requested.

        Returns
        -------
        int
            Non-zero if interrupted.

        Examples
        --------
        >>> from doppler.interrupt import Interrupt
        >>> import numpy as np
        >>> it = Interrupt(np.array([], dtype=np.int32))
        >>> it.interrupted()
        0
        >>> it.interrupt()
        >>> it.interrupted()
        1

        """

    def resume(self) -> None:
        """Clear the flag so waits proceed again.

        Examples
        --------
        >>> from doppler.interrupt import Interrupt
        >>> it = Interrupt([])
        >>> it.interrupt()
        >>> it.resume()
        >>> it.interrupted()
        0

        """

    def latency_ms(self) -> int:
        """The wait slice every blocking wait in this process uses.

        The readback for the constructor's `latency_ms`, and it reads the
        PROCESS setting rather than what this guard asked for -- those differ
        when the guard passed 0, which means "leave it alone". A value a caller
        can set and not read back is a value they cannot reason about.

        Returns
        -------
        int
            Milliseconds.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.interrupt import Interrupt
        >>> it = Interrupt(np.array([], dtype=np.int32), latency_ms=25)
        >>> it.latency_ms()
        25

        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "Interrupt":
        """Enter a context manager, returning this object.

        Lets a Interrupt be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Interrupt
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the Interrupt.

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
