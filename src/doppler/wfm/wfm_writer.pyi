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
        where to write -- a `str` or any `os.PathLike` from Python. For
        `file_type="sigmf"` this MUST end in `.sigmf-data`: a SigMF capture is
        a `<base>.sigmf-data` + `<base>.sigmf-meta` pair found by name, and
        close() writes the sidecar beside it.
    fs : float
        sample rate (Hz), and REQUIRED -- there is no default. BLUE stores it
        as `xdelta = 1/fs`, SigMF and the raw/CSV `sidecar` as
        `core:sample_rate`. Pass 0.0 to say the rate is not known: that writes
        `xdelta = 0` and omits `core:sample_rate`, where a defaulted value
        would have written a rate nobody supplied into a file that outlives the
        process.
    file_type : Literal["raw", "csv", "blue", "sigmf"], default "raw"
        `"raw"` (headerless interleaved I/Q), `"csv"` (one `I,Q` line per
        sample), `"blue"` (self-describing X-Midas/REDHAWK type-1000) or
        `"sigmf"`. BLUE and SigMF record `fs`/`fc`/`t0` in the capture itself;
        raw and CSV have nowhere to put them and keep them in the `sidecar`
        instead.
    sample_type : Literal["cf32", "cf64", "ci32", "ci16", "ci8"], default "cf32"
        wire type: `"cf32"`, `"cf64"`, `"ci32"`, `"ci16"` or `"ci8"`. The
        integer types quantise ±1.0 to full scale and can clip -- see
        track_clipping()/peak_dbfs.
    endian : Literal["le", "be"], default "le"
        `"le"` or `"be"`; ignored for CSV, which is text.
    fc : float, default 0.0
        centre frequency (Hz). BLUE records it as a `FREQ` keyword, SigMF as
        `captures[0]["core:frequency"]`, raw and CSV in the `sidecar`. 0.0
        writes nothing, in every one of them -- absent is how this library says
        "not stated", which is what `Reader.fc_source` reports back.
    total : int, default 0
        expected sample count, for the BLUE header; close() patches the real
        count, so 0 is fine when unknown.
    headroom : float, default 0.0
        dB of output backoff (gain = 10^(-H/20)) applied before quantisation. A
        single scale, so it does not change any power ratio -- only the
        absolute level. 0 is a bit-exact no-op.
    t0 : float, default 0.0
        capture start, seconds since the UNIX epoch. Optional where `fs` is
        required, because a capture with no wall-clock anchor is still readable
        and one with no rate is not. BLUE stores it as a J1950 timecode, SigMF
        as `captures[0]["core:datetime"]`, raw and CSV in the `sidecar`. 0.0
        means unset and stays unset -- it is never written as 1970. `Reader.t0`
        / `Reader.t0_source` read it back.
    sidecar : bool, default True
        write a `<path>.sigmf-meta` JSON beside a `"raw"` or `"csv"` capture,
        recording the `fs`, `fc` and `t0` those containers have nowhere to
        keep. On by default: the caller already supplied the values at
        construction, and dropping them on the floor left a file nobody -- its
        own author included -- could interpret. Only what was actually stated
        is written; nothing is invented. It is SigMF-SHAPED, not a SigMF
        capture: the spec pairs `.sigmf-data`, so the name is APPENDED rather
        than swapped (`cap.raw` -> `cap.raw.sigmf-meta`), which keeps it 1:1
        with its data file and unable to collide with a real capture's
        metadata. Ignored for `"blue"` (its header already carries all three)
        and for `"sigmf"`, where the sidecar is half the capture and cannot be
        turned off. Pass false when an extra file beside the capture would
        break a downstream glob.

    Raises
    ------
    OSError
        If construction fails. The exception message is ``cannot open the
        capture for writing: check the path, the directory, and permissions --
        and note that file_type="sigmf" requires a path ending in .sigmf-data,
        since a SigMF capture is a <base>.sigmf-data + <base>.sigmf-meta pair
        found by name``.

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
    >>> with Reader(p) as r:                    # everything round-trips
    ...     back = r.read(len(x))
    ...     r.fs, r.fc, r.num_samples, r.keywords["COMMENT"]
    (2400000.0, 1200000000.0, 1024, 'demo')
    >>> bool(np.array_equal(back, x))
    True

    A raw capture has nowhere to put `fs`/`fc`, so they go beside it:

    >>> q = pathlib.Path(tmp.name) / "capture.raw"
    >>> with Writer(q, fs=2.4e6, fc=1.2e9) as w:
    ...     w.write(x)
    1024
    >>> (q.parent / "capture.raw.sigmf-meta").exists()
    True
    >>> tmp.cleanup()

    """
    def __init__(
        self,
        path: str | os.PathLike,
        fs: float,
        file_type: Literal["raw", "csv", "blue", "sigmf"] = "raw",
        sample_type: Literal["cf32", "cf64", "ci32", "ci16", "ci8"] = "cf32",
        endian: Literal["le", "be"] = "le",
        fc: float = ...,
        total: int = ...,
        headroom: float = ...,
        t0: float = ...,
        sidecar: bool = ...,
    ) -> None: ...

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

    def flush(self) -> None:
        """Make every sample written so far durable and observable to a
        concurrent reader, without ending the capture. Leaves the file on a
        sample boundary, which is what lets a follower read it without meeting
        a partial sample. Raises OSError if this or any earlier write failed; a
        capture is not finished until close().

        Leaves the file on a sample boundary -- write() emits whole samples, so
        a flush BETWEEN write calls is what lets a follower read the capture
        without meeting a partial one. Raises `OSError` if this or any earlier
        write failed; a capture is not complete until close().

        Raises
        ------
        OSError
            If the C call returns a non-zero status. The exception message is
            ``flush failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import pathlib, tempfile
        >>> import numpy as np
        >>> from doppler.wfm import Reader, Writer
        >>> tmp = tempfile.TemporaryDirectory()
        >>> p = pathlib.Path(tmp.name) / "live.blue"
        >>> w = Writer(p, file_type="blue", sample_type="ci16", fs=2.4e6)
        >>> _ = w.write(np.zeros(16, dtype=np.complex64))
        >>> w.flush()                    # the samples are on disk now
        >>> Reader(p).read_follow(16).size
        16
        >>> w.close()
        >>> tmp.cleanup()

        """

    def track_clipping(self, on: int = 1) -> None:
        """Enable the per-component clip *counter* (off by default; peak is
        always on).

        Parameters
        ----------
        on : int
            Input.
        """

    def add_keyword(
        self,
        tag: str,
        type: str,
        value: str | int | float | Sequence[int] | Sequence[float],
    ) -> None:
        """Attach a BLUE extended-header keyword (BLUE captures only). `type`
        is a single character (B/I/L/X int, F/D float, A string, T deprecated
        int); `value` is a str for A, a single int/float, or a sequence of
        them. Keywords are buffered and written at close(). The read side is
        Reader.keywords.
        """

    @property
    def clip_fraction(self) -> float:
        """Fraction (0..1) of I/Q components that saturated. Always 0.0 unless
        `track_clipping()` was enabled before writing -- the counter is the one
        extra per-sample compare, so it is opt-in. `peak_dbfs` is always
        tracked and is enough to tell you clipping happened; this tells you how
        much.
        """

    @property
    def peak_dbfs(self) -> float:
        """Largest per-axis magnitude written so far, in dBFS (full scale = 0
        dBFS, so a value above 0 means an integer capture clipped). Always
        tracked. It is also the remedy: back off by `ceil(peak_dbfs)` dB of
        `headroom` and the capture fits. Float wire types never clip but still
        report a peak. `-inf` before anything is written.
        """

    @property
    def clipped(self) -> bool:
        """True if an integer capture saturated -- `peak_dbfs > 0` and the wire
        type is one of `ci32`/`ci16`/`ci8`. Always False for `cf32`/`cf64`,
        which cannot clip: a float sample above full scale is merely loud.
        """

    def close(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.

        Raises
        ------
        OSError
            If the C destructor reports failure. Raised from an explicit call
            and from ``__exit__`` alike, so a failing teardown propagates out
            of a ``with`` block (gh-541).
        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.

        Raises
        ------
        OSError
            If the C destructor reports failure. Raised from an explicit call
            and from ``__exit__`` alike, so a failing teardown propagates out
            of a ``with`` block (gh-541).
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

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the Writer.

        Equivalent to calling `close()`. Returns ``None``, so an exception
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

        Raises
        ------
        OSError
            If the C destructor reports failure. Raised from an explicit call
            and from ``__exit__`` alike, so a failing teardown propagates out
            of a ``with`` block (gh-541).
        """

def write_blue_header(
    path: str | os.PathLike,
    fs: float,
    sample_type: str = 'cf32',
    endian: str = 'le',
    fc: float = 0.0,
    data_start: float = 0.0,
    total: int = 0,
    detached: int = 1,
    t0: float = 0.0,
) -> None:
    """Write a standalone BLUE type-1000 HCB header (the detached .hdr): 512
    bytes carrying the BLUE magic, byte order, data_size (total x
    bytes-per-sample), the type-1000 tag and xdelta = 1/fs. Pair it with a
    detached .det body of raw interleaved I/Q. Raises on a failed write.
    """
