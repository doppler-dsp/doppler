# capture/capture.pyi — type stubs for the capture C extension.
from typing import final, Literal
import os
import numpy as np
from numpy.typing import NDArray

@final
class CaptureSummary(tuple[int, float, float]):
    """A capture at a glance: sample count and the resolved fs/fc.

    Attributes
    ----------
    num_samples : int
        Samples the reader decoded from the capture.
    fs_hz : float
        Sample rate (Hz); 0 if the file never stated it.
    fc_hz : float
        Centre frequency (Hz); 0 if the file was silent.
    """

    @property
    def num_samples(self) -> int:
        """Samples the reader decoded from the capture."""

    @property
    def fs_hz(self) -> float:
        """Sample rate (Hz); 0 if the file never stated it."""

    @property
    def fc_hz(self) -> float:
        """Centre frequency (Hz); 0 if the file was silent."""

@final
class Capture:
    """Capture component.

    Parameters
    ----------
    path : str | os.PathLike
        path constructor parameter (required).

    """
    def __init__(self, path: str | os.PathLike) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def read(self, count: int = 1, out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Read up to `count` samples as unit-scale complex64; an empty array at end of file.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def read_max_out(self, n: int) -> int:
        """Largest number of samples read() can return for n inputs.

        Size an `out=` buffer with this before calling read(), or use it to
        allocate one up front. The bound is this object's own: what it depends
        on is a property of the algorithm, so a header block on read_max_out()
        replaces this text.

        Parameters
        ----------
        n : int
            Number of input samples read() will be given.

        Returns
        -------
        int
            Upper bound on the output length; the actual call may return fewer.
        """

    def summary(self) -> CaptureSummary:
        """Summary."""

    @property
    def fs(self) -> float:
        """Sample rate in Hz. Read from the file for BLUE/SigMF; supplied by you for a `RawCapture`."""

    @property
    def fc(self) -> float:
        """Centre frequency in Hz. Read from the file for BLUE/SigMF; supplied by you for a `RawCapture`."""

    @property
    def num_samples(self) -> int:
        """Total samples in the capture."""

    @property
    def metadata_source(self) -> Literal["none", "file", "supplied"]:
        """Where `fs`/`fc` came from -- `"file"` when the capture declared them (BLUE, SigMF), `"supplied"` when you passed them to `RawCapture`, or `"none"` when neither. This is the property the view exists to make honest: with a plain `Capture` over a headerless file the numbers are defaults, and without this you cannot tell a default from a reading."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on exit.

        Idempotent: calling it again on an already-released object does nothing.
        Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "Capture":
        """Enter a context manager, returning this object.

        Lets a Capture be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Capture
            This same object, not a copy.
        """

    def __exit__(self, exc_type: object | None = ..., exc: object | None = ..., tb: object | None = ...) -> None:
        """Exit a context manager, releasing the Capture.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never suppresses
        one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.
        """

@final
class RawCapture:
    """RawCapture component.

    Parameters
    ----------
    path : str | os.PathLike
        path constructor parameter (required).
    sample_type : Literal["cf32", "cf64", "ci32", "ci16", "ci8"], default "ci16"
        sample_type constructor parameter.
    endian : Literal["le", "be"], default "le"
        endian constructor parameter.
    fs : float, default 1.0
        fs constructor parameter.
    fc : float, default 0.0
        fc constructor parameter.

    """
    def __init__(self, path: str | os.PathLike, sample_type: Literal["cf32", "cf64", "ci32", "ci16", "ci8"] = "ci16", endian: Literal["le", "be"] = "le", fs: float = ..., fc: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def read(self, count: int = 1, out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Read up to `count` samples as unit-scale complex64; an empty array at end of file.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def read_max_out(self, n: int) -> int:
        """Largest number of samples read() can return for n inputs.

        Size an `out=` buffer with this before calling read(), or use it to
        allocate one up front. The bound is this object's own: what it depends
        on is a property of the algorithm, so a header block on read_max_out()
        replaces this text.

        Parameters
        ----------
        n : int
            Number of input samples read() will be given.

        Returns
        -------
        int
            Upper bound on the output length; the actual call may return fewer.
        """

    def summary(self) -> CaptureSummary:
        """Summary."""

    @property
    def fs(self) -> float:
        """Sample rate in Hz. Read from the file for BLUE/SigMF; supplied by you for a `RawCapture`."""

    @property
    def fc(self) -> float:
        """Centre frequency in Hz. Read from the file for BLUE/SigMF; supplied by you for a `RawCapture`."""

    @property
    def num_samples(self) -> int:
        """Total samples in the capture."""

    @property
    def metadata_source(self) -> Literal["none", "file", "supplied"]:
        """Where `fs`/`fc` came from -- `"file"` when the capture declared them (BLUE, SigMF), `"supplied"` when you passed them to `RawCapture`, or `"none"` when neither. This is the property the view exists to make honest: with a plain `Capture` over a headerless file the numbers are defaults, and without this you cannot tell a default from a reading."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on exit.

        Idempotent: calling it again on an already-released object does nothing.
        Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "RawCapture":
        """Enter a context manager, returning this object.

        Lets a RawCapture be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        RawCapture
            This same object, not a copy.
        """

    def __exit__(self, exc_type: object | None = ..., exc: object | None = ..., tb: object | None = ...) -> None:
        """Exit a context manager, releasing the RawCapture.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never suppresses
        one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.
        """
