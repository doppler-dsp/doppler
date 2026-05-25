# spectral/spectral.pyi — type stubs for the spectral C extension.
from typing import Literal
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
    def __init__(self, ny: int = ..., nx: int = ..., sign: int = ..., nthreads: int = ...) -> None: ...

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

class Corr:
    """Corr component.

    Parameters
    ----------
    ref : NDArray[np.complex64], default ...
        ref constructor parameter.
    dwell : int, default 1
        dwell constructor parameter.
    nthreads : int, default 1
        nthreads constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.spectral import Corr
    >>> obj = Corr(..., 1, 1)

    """
    def __init__(self, ref: NDArray[np.complex64] = ..., dwell: int = ..., nthreads: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def execute(self, x: complex) -> NDArray[np.complex64]:
        """Execute."""

    @property
    def n(self) -> int:
        """N."""

    @property
    def dwell(self) -> int:
        """Dwell."""

    @property
    def count(self) -> int:
        """Count."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Corr": ...

    def __exit__(self, *args: object) -> None: ...

class Corr2D:
    """Corr2D component.

    Parameters
    ----------
    ref : NDArray[np.complex64], default ...
        ref constructor parameter.
    dwell : int, default 1
        dwell constructor parameter.
    nthreads : int, default 1
        nthreads constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.spectral import Corr2D
    >>> obj = Corr2D(..., 1, 1)

    """
    def __init__(self, ref: NDArray[np.complex64] = ..., dwell: int = ..., nthreads: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def execute(self, x: complex) -> NDArray[np.complex64]:
        """Execute."""

    @property
    def ny(self) -> int:
        """Ny."""

    @property
    def nx(self) -> int:
        """Nx."""

    @property
    def dwell(self) -> int:
        """Dwell."""

    @property
    def count(self) -> int:
        """Count."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Corr2D": ...

    def __exit__(self, *args: object) -> None: ...

class Detector:
    """Detector component.

    Parameters
    ----------
    ref : NDArray[np.complex64], default ...
        ref constructor parameter.
    dwell : int, default 1
        dwell constructor parameter.
    noise_lo : int, default 0
        noise_lo constructor parameter.
    noise_hi : int, default n-1
        noise_hi constructor parameter.
    noise_mode : str, default "mean"
        noise_mode constructor parameter.
    threshold : float, default 0.0
        threshold constructor parameter.
    nthreads : int, default 1
        nthreads constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.spectral import Detector
    >>> obj = Detector(..., 1, 0, n-1, ..., 0.0, 1)

    """
    def __init__(self, ref: NDArray[np.complex64] = ..., dwell: int = ..., noise_lo: int = ..., noise_hi: int = ..., noise_mode: Literal["mean", "median", "min", "max"] = ..., threshold: float = ..., nthreads: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def push(self, x: complex) -> list[tuple[int, float, float, float]]:
        """Push."""

    @property
    def n(self) -> int:
        """N."""

    @property
    def dwell(self) -> int:
        """Dwell."""

    @property
    def count(self) -> int:
        """Count."""

    @property
    def ring_cap(self) -> int:
        """Ring cap."""

    @property
    def noise_lo(self) -> int:
        """Noise lo."""

    @property
    def noise_hi(self) -> int:
        """Noise hi."""

    @property
    def threshold(self) -> float:
        """Threshold."""

    @property
    def last_corr(self) -> NDArray[np.complex64]:
        """Last corr."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Detector": ...

    def __exit__(self, *args: object) -> None: ...

class Detector2D:
    """Detector2D component.

    Parameters
    ----------
    ref : NDArray[np.complex64], default ...
        ref constructor parameter.
    dwell : int, default 1
        dwell constructor parameter.
    noise_lo : int, default 0
        noise_lo constructor parameter.
    noise_hi : int, default ny*nx-1
        noise_hi constructor parameter.
    noise_mode : str, default "mean"
        noise_mode constructor parameter.
    threshold : float, default 0.0
        threshold constructor parameter.
    nthreads : int, default 1
        nthreads constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.spectral import Detector2D
    >>> obj = Detector2D(..., 1, 0, ny*nx-1, ..., 0.0, 1)

    """
    def __init__(self, ref: NDArray[np.complex64] = ..., dwell: int = ..., noise_lo: int = ..., noise_hi: int = ..., noise_mode: Literal["mean", "median", "min", "max"] = ..., threshold: float = ..., nthreads: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def push(self, x: complex) -> list[tuple[int, int, float, float, float]]:
        """Push."""

    @property
    def ny(self) -> int:
        """Ny."""

    @property
    def nx(self) -> int:
        """Nx."""

    @property
    def n(self) -> int:
        """N."""

    @property
    def dwell(self) -> int:
        """Dwell."""

    @property
    def count(self) -> int:
        """Count."""

    @property
    def ring_cap(self) -> int:
        """Ring cap."""

    @property
    def noise_lo(self) -> int:
        """Noise lo."""

    @property
    def noise_hi(self) -> int:
        """Noise hi."""

    @property
    def threshold(self) -> float:
        """Threshold."""

    @property
    def last_corr(self) -> NDArray[np.complex64]:
        """Last corr."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Detector2D": ...

    def __exit__(self, *args: object) -> None: ...

def kaiser_enbw(w: NDArray[np.float32]) -> float:
    """Kaiser enbw."""

def kaiser_window(w: NDArray[np.float32], beta: float) -> None:
    """Kaiser window."""

def hann_window(w: NDArray[np.float32]) -> None:
    """Hann window."""

def magnitude_db_cf32(x: NDArray[np.complex64], lin_floor: float, offset_db: float) -> NDArray[np.float32]:
    """Magnitude db cf32."""

def magnitude_db_cf64(x: NDArray[np.complex128], lin_floor: float, offset_db: float) -> NDArray[np.float32]:
    """Magnitude db cf64."""

def find_peaks_f32(db: NDArray[np.float32], n_peaks: int, min_db: float) -> list[tuple[float, float]]:
    """Find peaks f32."""
