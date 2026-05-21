# spectral/spectral.pyi — type stubs for the spectral C extension.
import numpy as np
from numpy.typing import NDArray

class FFT:
    """FFT component.

    Parameters
    ----------
    n : int, default 1024
        n constructor parameter.
    sign : int, default -1
        sign constructor parameter.
    nthreads : int, default 1
        nthreads constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.spectral import FFT
    >>> obj = FFT(1024, -1, 1)

    """
    def __init__(self, n: int = ..., sign: int = ..., nthreads: int = ...) -> None: ...
    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def execute_cf64(self, x: complex) -> NDArray[np.complex128]:
        """Execute cf64."""

    def execute_cf32(self, x: complex) -> NDArray[np.complex64]:
        """Execute cf32."""

    def execute_inplace_cf64(self, x: complex) -> NDArray[np.complex128]:
        """Execute inplace cf64."""

    def execute_inplace_cf32(self, x: complex) -> NDArray[np.complex64]:
        """Execute inplace cf32."""

    @property
    def n(self) -> int:
        """N."""

    @property
    def sign(self) -> int:
        """Sign."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "FFT": ...
    def __exit__(self, *args: object) -> None: ...

class FFT2D:
    """FFT2D component.

    Parameters
    ----------
    ny : int, default 64
        ny constructor parameter.
    nx : int, default 64
        nx constructor parameter.
    sign : int, default -1
        sign constructor parameter.
    nthreads : int, default 1
        nthreads constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.spectral import FFT2D
    >>> obj = FFT2D(64, 64, -1, 1)

    """
    def __init__(
        self, ny: int = ..., nx: int = ..., sign: int = ..., nthreads: int = ...
    ) -> None: ...
    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def execute_cf64(self, x: complex) -> NDArray[np.complex128]:
        """Execute cf64."""

    def execute_cf32(self, x: complex) -> NDArray[np.complex64]:
        """Execute cf32."""

    def execute_inplace_cf64(self, x: complex) -> NDArray[np.complex128]:
        """Execute inplace cf64."""

    def execute_inplace_cf32(self, x: complex) -> NDArray[np.complex64]:
        """Execute inplace cf32."""

    @property
    def ny(self) -> int:
        """Ny."""

    @property
    def nx(self) -> int:
        """Nx."""

    @property
    def sign(self) -> int:
        """Sign."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "FFT2D": ...
    def __exit__(self, *args: object) -> None: ...

def kaiser_enbw(w: NDArray[np.float32]) -> float:
    """Kaiser enbw."""

def kaiser_window(w: NDArray[np.float32], beta: float) -> None:
    """Kaiser window."""

def hann_window(w: NDArray[np.float32]) -> None:
    """Hann window."""

def magnitude_db_cf32(
    x: NDArray[np.complex64], lin_floor: float, offset_db: float
) -> NDArray[np.float32]:
    """Magnitude db cf32."""

def magnitude_db_cf64(
    x: NDArray[np.complex128], lin_floor: float, offset_db: float
) -> NDArray[np.float32]:
    """Magnitude db cf64."""

def find_peaks_f32(
    db: NDArray[np.float32], n_peaks: int, min_db: float
) -> list[tuple[float, float]]:
    """Find peaks f32."""
