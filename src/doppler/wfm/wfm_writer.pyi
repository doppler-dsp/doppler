# wfm/wfm_writer.pyi — type stubs for the wfm_writer C extension.
from typing import final, Literal
from collections.abc import Sequence
import os
import numpy as np
from numpy.typing import NDArray

@final
class Writer:
    """Open a capture for writing.

    Parameters
    ----------
    path : str | os.PathLike
        where to write -- a `str` or any `os.PathLike` from Python. For `file_type="sigmf"` this MUST end in `.sigmf-data`: a SigMF capture is a `<base>.sigmf-data` + `<base>.sigmf-meta` pair found by name, and close() writes the sidecar beside it.
    file_type : Literal["raw", "csv", "blue", "sigmf"], default "raw"
        `"raw"` (headerless interleaved I/Q), `"csv"` (one `I,Q` line per sample), `"blue"` (self-describing X-Midas/REDHAWK type-1000) or `"sigmf"`. Only BLUE and SigMF record `fs`/`fc`; raw and CSV have nowhere to put them.
    sample_type : Literal["cf32", "cf64", "ci32", "ci16", "ci8"], default "cf32"
        wire type: `"cf32"`, `"cf64"`, `"ci32"`, `"ci16"` or `"ci8"`. The integer types quantise ±1.0 to full scale and can clip -- see track_clipping()/peak_dbfs.
    endian : Literal["le", "be"], default "le"
        `"le"` or `"be"`; ignored for CSV, which is text.
    fs : float, default 1e6
        sample rate (Hz). BLUE stores it as `xdelta = 1/fs`, SigMF as `core:sample_rate`.
    fc : float, default 0.0
        centre frequency (Hz). BLUE records it as a `FREQ` keyword, SigMF as `captures[0]["core:frequency"]`; raw and CSV drop it. 0.0 writes nothing.
    total : int, default 0
        expected sample count, for the BLUE header; close() patches the real count, so 0 is fine when unknown.
    headroom : float, default 0.0
        dB of output backoff (gain = 10^(-H/20)) applied before quantisation. A single scale, so it does not change any power ratio -- only the absolute level. 0 is a bit-exact no-op.

    Examples
    --------
    >>> import pathlib, tempfile
    >>> import numpy as np
    >>> from doppler.wfm import Reader, Writer
    >>> tmp = tempfile.TemporaryDirectory()
    >>> p = pathlib.Path(tmp.name) / "capture.blue"
    >>> x = np.arange(1024, dtype=np.complex64) / 1024.0
    >>> with Writer(p, file_type="blue", sample_type="cf32",
    ...             fs=2.4e6, fc=1.2e9) as w:
    ...     w.write(x)                              # samples in
    ...     w.add_keyword("COMMENT", "A", "demo")   # tag the header
    1024
    >>> p.exists()
    True
    >>> with Reader(p) as r:                        # everything round-trips
    ...     back = r.read(len(x))
    ...     r.fs, r.fc, r.num_samples, r.keywords["COMMENT"]
    (2400000.0, 1200000000.0, 1024, 'demo')
    >>> bool(np.array_equal(back, x))
    True
    >>> tmp.cleanup()

    """
    def __init__(self, path: str | os.PathLike, file_type: Literal["raw", "csv", "blue", "sigmf"] = "raw", sample_type: Literal["cf32", "cf64", "ci32", "ci16", "ci8"] = "cf32", endian: Literal["le", "be"] = "le", fs: float = ..., fc: float = ..., total: int = ..., headroom: float = ...) -> None: ...

    def write(self, x: NDArray[np.complex64]) -> int:
        """Convert and write a block of samples.

        Takes `complex64` at unit scale and emits it in the writer's wire type.
        Call as many times as you like; the capture is the concatenation.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        int
            the number of samples that actually landed — equal to what you
            passed on success, fewer if the write was short (a full disk, a
            quota). A short return is the per-block signal; close() reports the
            same failure for the capture as a whole.

        Examples
        --------
        >>> import pathlib, tempfile
        >>> from doppler.wfm import Composer, Reader, Segment, Writer
        >>> tmp = tempfile.TemporaryDirectory()
        >>> p = pathlib.Path(tmp.name) / "capture.blue"
        >>> x = Composer([Segment("qpsk", sps=8, num_samples=1024)]).compose()
        >>> with Writer(p, file_type="blue", sample_type="ci16",
        ...             fs=2.4e6, fc=1.2e9) as w:
        ...     w.write(x)
        1024
        >>> r = Reader(p)
        >>> r.fs, r.fc, r.num_samples
        (2400000.0, 1200000000.0, 1024)
        >>> r.close()
        >>> tmp.cleanup()   # directory and contents removed

        """

    def track_clipping(self, on: int = 1) -> None:
        """Enable the per-component clip *counter* (off by default; peak is always on).

        Parameters
        ----------
        on : int
            Input.
        """

    def add_keyword(self, tag: str, type: str, value: str | int | float | Sequence[int] | Sequence[float]) -> None:
        """Attach a BLUE extended-header keyword (BLUE captures only). `type` is a single character (B/I/L/X int, F/D float, A string, T deprecated int); `value` is a str for A, a single int/float, or a sequence of them. Keywords are buffered and written at close(). The read side is Reader.keywords."""

    @property
    def clip_fraction(self) -> float:
        """Fraction (0..1) of I/Q components that saturated. Always 0.0 unless `track_clipping()` was enabled before writing -- the counter is the one extra per-sample compare, so it is opt-in. `peak_dbfs` is always tracked and is enough to tell you clipping happened; this tells you how much."""

    @property
    def peak_dbfs(self) -> float:
        """Largest per-axis magnitude written so far, in dBFS (full scale = 0 dBFS, so a value above 0 means an integer capture clipped). Always tracked. It is also the remedy: back off by `ceil(peak_dbfs)` dB of `headroom` and the capture fits. Float wire types never clip but still report a peak. `-inf` before anything is written."""

    @property
    def clipped(self) -> bool:
        """True if an integer capture saturated -- `peak_dbfs > 0` and the wire type is one of `ci32`/`ci16`/`ci8`. Always False for `cf32`/`cf64`, which cannot clip: a float sample above full scale is merely loud."""

    def close(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on exit.

        Idempotent: calling it again on an already-released object does nothing.
        Every other method raises ``RuntimeError`` once it has run.

        Raises
        ------
        OSError
            If the C destructor reports failure. Raised from an explicit call and from ``__exit__`` alike, so a failing teardown propagates out of a ``with`` block (gh-541).
        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on exit.

        Idempotent: calling it again on an already-released object does nothing.
        Every other method raises ``RuntimeError`` once it has run.

        Raises
        ------
        OSError
            If the C destructor reports failure. Raised from an explicit call and from ``__exit__`` alike, so a failing teardown propagates out of a ``with`` block (gh-541).
        """


    def __enter__(self) -> "Writer":
        """Enter a context manager, returning this object.

        Lets a Writer be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Writer
            This same object, not a copy.
        """

    def __exit__(self, exc_type: object | None = ..., exc: object | None = ..., tb: object | None = ...) -> None:
        """Exit a context manager, releasing the Writer.

        Equivalent to calling `close()`. Returns ``None``, so an exception
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

def write_blue_header(path: str | os.PathLike, sample_type: str = 'cf32', endian: str = 'le', fs: float = 1e6, fc: float = 0.0, data_start: float = 0.0, total: int = 0, detached: int = 1) -> None:
    """Write a standalone BLUE type-1000 HCB header (the detached .hdr): 512 bytes carrying the BLUE magic, byte order, data_size (total x bytes-per-sample), the type-1000 tag and xdelta = 1/fs. Pair it with a detached .det body of raw interleaved I/Q. Raises on a failed write."""
