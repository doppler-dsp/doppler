# wfm/wfm_reader.pyi — type stubs for the wfm_reader C extension.
from typing import Any, final, Literal
import numpy as np
from numpy.typing import NDArray

@final
class Reader:
    """Open a capture, auto-detecting its file type from its content.

    Parameters
    ----------
    path : str
        file to read -- a `str` or any `os.PathLike` from Python. For a DETACHED BLUE capture this is normally the HEADER file -- `<base>.tmp` or `<base>.prm` per BLUE 3.1.1.4 (this library's own writer emits `<base>.hdr`) -- whose HCB `detached` field points at the collocated `<base>.det` payload; the extension does not decide, `detached` does. Passing the `<base>.det` directly also works (its header sibling is resolved). A SigMF `.sigmf-data` file resolves its `.sigmf-meta` sidecar the same way.
    sample_type : Literal["cf32", "cf64", "ci32", "ci16", "ci8"], default "cf32"
        the wire sample type, used only as a HINT for the headerless file types (raw, CSV) -- BLUE and SigMF carry their own and ignore it. `"cf32"`, `"cf64"`, `"ci32"`, `"ci16"` or `"ci8"` from Python; the matching 0..4 from C. A wrong hint does not fail; see ::wfm_reader_get_trailing_bytes.
    endian : Literal["le", "be"], default "le"
        byte order, likewise a hint that only headerless raw uses; `"le"` or `"be"` from Python, 0 or 1 from C.

    """
    def __init__(self, path: str, sample_type: Literal["cf32", "cf64", "ci32", "ci16", "ci8"] = "cf32", endian: Literal["le", "be"] = "le") -> None: ...

    def reset(self) -> None:
        """Rewind to the first sample of the capture.

        Seeks back to where the payload starts — 512 bytes into an attached BLUE
        file, byte 0 of a `.det` or a raw/SigMF payload — and restores the
        remaining-sample count, so the capture reads again from the top. The
        file's metadata and decoded keywords are unaffected: they came from the
        header and do not change.
        """

    def read(self, count: int = 1, out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Read up to count samples, returning them as `complex64`.

        Samples come out at unit scale whatever the wire type was: a float type
        is reinterpreted, an integer type is divided by its full scale. Returns
        fewer than asked at the end of the capture, and 0 once it is exhausted,
        so a `while` over the result terminates. Never returns more than the
        file's declared payload — trailing bytes past `data_size` (an extended
        header, X-Midas slack) are not samples.

        Returns
        -------
        NDArray[np.complex64]
            Output.

        Examples
        --------
        >>> import pathlib, tempfile
        >>> from doppler.wfm import Composer, Reader, Segment, Writer
        >>> tmp = tempfile.TemporaryDirectory()
        >>> p = pathlib.Path(tmp.name) / "capture.blue"
        >>> x = Composer([Segment("qpsk", sps=8, num_samples=1024)]).compose()
        >>> with Writer(p, file_type="blue", sample_type="ci16",
        ...             fs=2.4e6, fc=1.2e9) as w:
        ...     _ = w.write(x)
        >>> r = Reader(p)
        >>> r.file_type, r.sample_type, r.endian
        ('blue', 'ci16', 'le')
        >>> r.fs, r.fc, r.fc_source
        (2400000.0, 1200000000.0, 'FREQ')
        >>> total = 0
        >>> while len(block := r.read(256)):
        ...     total += len(block)
        >>> total
        1024
        >>> r.close()
        >>> tmp.cleanup()   # directory and contents removed

        """

    def read_max_out(self) -> int:
        """Max output length read() can produce for the current state."""

    @property
    def file_type(self) -> Literal["raw", "csv", "blue", "sigmf"]:
        """Which file type the capture turned out to be -- `"raw"`, `"csv"`, `"blue"` or `"sigmf"`. Detected from the file's CONTENT, not its name, so a CSV called `capture.dat` reports `"csv"` and a BLUE file called `capture.csv` reports `"blue"`. `"raw"` is also the fallback for a file nothing else recognised, so it means "headerless interleaved I/Q at the sample_type you passed" rather than a positive identification."""

    @property
    def sample_type(self) -> Literal["cf32", "cf64", "ci32", "ci16", "ci8"]:
        """The wire sample type the samples are being decoded FROM -- `"cf32"`, `"cf64"`, `"ci32"`, `"ci16"` or `"ci8"`. For BLUE and SigMF this was read from the file's metadata and is authoritative; for raw and CSV it is simply the hint passed to the constructor, echoed back. `read()` returns `complex64` at unit scale regardless."""

    @property
    def mode(self) -> Literal["complex", "scalar"]:
        """Components per wire sample: `"complex"` for interleaved I/Q, `"scalar"` for a real capture. Only BLUE carries this (its `format` field's mode designator, `C` or `S`); every other file type is complex. A scalar capture still reads back as `complex64` -- the imaginary part is exactly 0, so a real signal lands on the real axis."""

    @property
    def endian(self) -> Literal["le", "be"]:
        """Byte order of the samples on the wire, `"le"` or `"be"`. Read from the metadata for BLUE (the HCB's `head_rep`) and SigMF (the `_be`/`_le` datatype suffix); for raw it is the constructor hint echoed back. CSV is text and ignores it."""

    @property
    def fs(self) -> float:
        """Sample rate in Hz, or 0.0 when the file type does not carry one. BLUE derives it from the header's `xdelta` (fs = 1/xdelta); SigMF reads `core:sample_rate`. Raw and CSV have nowhere to record a rate, so they always report 0.0 -- whatever rate the capture was taken at has to travel with it by other means."""

    @property
    def fc(self) -> float:
        """Centre frequency in Hz, or 0.0 when nothing in the capture declares one. **0.0 is ambiguous on its own** -- a genuine baseband capture and a capture whose frequency could not be found report the same number -- so read `fc_source` alongside it: `"none"` there is what distinguishes them. SigMF takes it from `captures[0]["core:frequency"]`; BLUE from a `FREQ` keyword (see `fc_source` for the tags tried), in either the ASCII HCB keyword area or the typed extended header. Raw and CSV carry no metadata at all."""

    @property
    def num_samples(self) -> int:
        """Total samples in the capture, or 0 when the file type cannot say. BLUE takes it from the header's `data_size`; raw and SigMF divide the file length by the sample stride. A CSV has to be counted, so the first read of this property scans the file once (the scan is exact -- it parses rows the same way `read` does -- and leaves the read position alone); every later read is free."""

    @property
    def fc_source(self) -> Literal["none", "FREQ", "RF_FREQ", "CENTER_FREQ", "F_C", "core:frequency"]:
        """Which piece of metadata `fc` was read from -- the keyword's own tag (`"FREQ"`, `"RF_FREQ"`, `"CENTER_FREQ"`, `"F_C"`), `"core:frequency"` for SigMF, or `"none"` when nothing carried it. Check this before trusting `fc == 0.0`: `"none"` means not found, anything else means the capture really does say 0 Hz. BLUE type-1000 has no header field for centre frequency, so an RF capture conveys it as a keyword; `FREQ` in the HCB keyword area is the X-Midas convention and is tried first."""

    @property
    def trailing_bytes(self) -> int:
        """Payload bytes left over after the last whole sample; 0 for a capture whose declared sample type and mode match its content, and always 0 for CSV. Non-zero means either the `sample_type`/`endian` hint is wrong for a headerless file type or the capture is truncated -- the reader cannot tell which, and stops at the last complete sample either way. This is the only signal available for a raw file: a wrong hint does not fail, it returns plausible garbage at the wrong stride."""

    @property
    def keywords(self) -> dict[str, str | int | float | list[int] | list[float]]:
        """The BLUE extended header as a {tag: value} dict, in file order; empty when the capture carries no extended header. Values follow the keyword type: a str for A, an int/float for a single-element numeric keyword, a list for a multi-element one. For a detached capture these come from the HEADER file."""

    @property
    def header(self) -> dict[str, str | int | float | list[int] | list[float]]:
        """The BLUE header control block as a {field: value} dict, under the names the format itself uses -- `version`, `head_rep`, `data_rep`, `detached`, `protected`, `pipe`, `ext_start`, `ext_size`, `data_start`, `data_size`, `type`, `format`, `flagmask`, `timecode`, `inlet`, `outlets`, `outmask`, `pipeloc`, `pipesize`, `in_byte`, `out_byte`, `outbytes`, `keylength`, and the type-1000 adjunct `xstart`, `xdelta`, `xunits`. Empty for a non-BLUE file type. Nothing is renamed or omitted, so what you see is what the file holds; the decoded keywords are in `keywords`."""

    def close(self) -> None:
        """Release C resources immediately."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Reader": ...

    def __exit__(self, *args: object) -> None: ...
