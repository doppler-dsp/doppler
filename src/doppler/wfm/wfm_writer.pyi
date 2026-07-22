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

    def reset(self) -> None:
        """Reset state to post-create defaults."""

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

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Writer": ...

    def __exit__(self, *args: object) -> None: ...
