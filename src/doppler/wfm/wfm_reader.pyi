# wfm/wfm_reader.pyi — type stubs for the wfm_reader C extension.
from typing import Literal
import numpy as np
from numpy.typing import NDArray

class Reader:
    """Open a capture, auto-detecting its container.

    Parameters
    ----------
    path : str
        file to read. For a DETACHED BLUE capture this is normally the HEADER file -- `<base>.tmp` or `<base>.prm` per BLUE 3.1.1.4 (this library's own writer emits `<base>.hdr`) -- whose HCB `detached` field points at the collocated `<base>.det` payload; the extension does not decide, `detached` does. Passing the `<base>.det` directly also works (its header sibling is resolved). A SigMF `.sigmf-data` file resolves its `.sigmf-meta` sidecar the same way.
    sample_type : Literal["cf32", "cf64", "ci32", "ci16", "ci8"], default "cf32"
        sample_type constructor parameter.
    endian : Literal["le", "be"], default "le"
        endian constructor parameter.

    """
    def __init__(self, path: str, sample_type: Literal["cf32", "cf64", "ci32", "ci16", "ci8"] = "cf32", endian: Literal["le", "be"] = "le") -> None: ...

    def reset(self) -> None:
        """Rewind to the first sample of the capture.

        Seeks back to where the payload starts — 512 bytes into an attached BLUE
        file, byte 0 of a `.det` or a raw/SigMF payload — and restores the
        remaining-sample count, so the capture reads again from the top. The
        container metadata and decoded keywords are unaffected: they came from
        the header and do not change.
        """

    def read(self, out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Read up to max complex samples into out (unit-scale `float _Complex`), converting from the wire type. Returns the count read; 0 at end of file.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def read_max_out(self) -> int:
        """Max output length read() can produce for the current state."""

    @property
    def file_type(self) -> Literal["raw", "csv", "blue", "sigmf"]:
        """File type."""

    @property
    def sample_type(self) -> Literal["cf32", "cf64", "ci32", "ci16", "ci8"]:
        """Sample type."""

    @property
    def mode(self) -> Literal["complex", "scalar"]:
        """Mode."""

    @property
    def endian(self) -> Literal["le", "be"]:
        """Endian."""

    @property
    def fs(self) -> float:
        """Fs."""

    @property
    def fc(self) -> float:
        """Fc."""

    @property
    def num_samples(self) -> int:
        """Num samples."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Reader": ...

    def __exit__(self, *args: object) -> None: ...
