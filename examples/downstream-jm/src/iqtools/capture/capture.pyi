# capture/capture.pyi — type stubs for the capture C extension.
from typing import final, Literal
import numpy as np
from numpy.typing import NDArray

@final
class Capture:
    """Capture component.

    Parameters
    ----------
    path : str
        path constructor parameter (required).

    """
    def __init__(self, path: str) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def read(self, count: int = 1, out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Read up to `count` samples as unit-scale complex64; an empty array at end of file.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def read_max_out(self) -> int:
        """Max output length read() can produce for the current state."""

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
        """Release C resources immediately."""

    def __enter__(self) -> "Capture": ...

    def __exit__(self, *args: object) -> None: ...

@final
class RawCapture:
    """RawCapture component.

    Parameters
    ----------
    path : str
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
    def __init__(self, path: str, sample_type: Literal["cf32", "cf64", "ci32", "ci16", "ci8"] = "ci16", endian: Literal["le", "be"] = "le", fs: float = ..., fc: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def read(self, count: int = 1, out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Read up to `count` samples as unit-scale complex64; an empty array at end of file.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def read_max_out(self) -> int:
        """Max output length read() can produce for the current state."""

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
        """Release C resources immediately."""

    def __enter__(self) -> "RawCapture": ...

    def __exit__(self, *args: object) -> None: ...
