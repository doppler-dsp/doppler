# wfm/wfm_writer.pyi — type stubs for the wfm_writer C extension.
from typing import Literal
import numpy as np
from numpy.typing import NDArray

class Writer:
    """Path-opening + FILE-owning ctor for the generated `Writer` handle (jm kind="handle"): opens `path` ("wb"), delegates to wfm_writer_open, and marks the FILE owned so wfm_writer_close fclose's it. Returns NULL on open failure.

    Parameters
    ----------
    path : str
        path constructor parameter (required).
    file_type : Literal["raw", "csv", "blue", "sigmf"], default "raw"
        file_type constructor parameter.
    sample_type : Literal["cf32", "cf64", "ci32", "ci16", "ci8"], default "cf32"
        sample_type constructor parameter.
    endian : Literal["le", "be"], default "le"
        endian constructor parameter.
    fs : float, default 1e6
        fs constructor parameter.
    fc : float, default 0.0
        fc constructor parameter.
    total : int, default 0
        total constructor parameter.
    headroom : float, default 0.0
        headroom constructor parameter.

    """
    def __init__(self, path: str, file_type: Literal["raw", "csv", "blue", "sigmf"] = "raw", sample_type: Literal["cf32", "cf64", "ci32", "ci16", "ci8"] = "cf32", endian: Literal["le", "be"] = "le", fs: float = ..., fc: float = ..., total: int = ..., headroom: float = ...) -> None: ...

    def write(self, x: NDArray[np.complex64]) -> int:
        """Convert and write `n` complex samples.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        int
            Number of complex samples written (== n on success, else short).
        """

    def track_clipping(self, on: int = 1) -> None:
        """Enable the per-component clip *counter* (off by default; peak is always on).

        Parameters
        ----------
        on : int
            Input.
        """

    @property
    def clip_fraction(self) -> float:
        """Clip fraction."""

    @property
    def peak_dbfs(self) -> float:
        """Peak dbfs."""

    @property
    def clipped(self) -> bool:
        """Clipped."""

    def close(self) -> None:
        """Release C resources immediately.

        Raises
        ------
        OSError
            If the C destructor reports failure. Raised from
            an explicit call and from ``__exit__`` alike, so a
            failing teardown propagates out of a ``with``
            block (gh-541).
        """

    def destroy(self) -> None:
        """Release C resources immediately.

        Raises
        ------
        OSError
            If the C destructor reports failure. Raised from
            an explicit call and from ``__exit__`` alike, so a
            failing teardown propagates out of a ``with``
            block (gh-541).
        """

    def __enter__(self) -> "Writer": ...

    def __exit__(self, *args: object) -> None: ...

    # jm:hand
    def add_keyword(
        self,
        tag: str,
        type: Literal["B", "I", "L", "X", "F", "D", "A", "T"],
        value: str | int | float | list[int] | list[float],
    ) -> None:
        """Attach a BLUE extended-header keyword (BLUE captures only).

        The write-side mirror of :attr:`Reader.keywords`. Keywords are buffered
        and written as one block by :meth:`close`, after the samples.

        Parameters
        ----------
        tag : str
            Keyword tag, 1..255 characters; upper-case is preferred.
        type : {"B", "I", "L", "X", "F", "D", "A", "T"}
            Element type: B/I/L/X are 8/16/32/64-bit signed ints, F/D are
            32/64-bit floats, A is a string, T is a deprecated 32-bit int.
        value : str or int or float or sequence of int/float
            A ``str`` for type ``"A"``; a single number, or a sequence of
            numbers for a multi-element keyword, for the numeric types.

        Raises
        ------
        ValueError
            The writer is not a BLUE capture, the type code is unsupported, or
            the tag is empty or too long.
        TypeError
            Type ``"A"`` was given a non-``str`` value.

        Examples
        --------
        >>> import tempfile, os, numpy as np
        >>> from doppler.wfm import Writer, Reader
        >>> p = os.path.join(tempfile.mkdtemp(), "cap.blue")
        >>> with Writer(p, file_type="blue", sample_type="cf32", fs=1e6) as w:
        ...     _ = w.write(np.zeros(4, dtype=np.complex64))
        ...     w.add_keyword("COMMENT", "A", "10 dB pad")
        ...     w.add_keyword("GAINS", "F", [1.5, -2.5])
        >>> with Reader(p) as r:
        ...     r.keywords
        {'COMMENT': '10 dB pad', 'GAINS': [1.5, -2.5]}

        """
