# spectral/spectral.pyi — type stubs for the spectral C extension.
from typing import Literal
import numpy as np
from numpy.typing import NDArray

class FFT:
    """Create a 1-D FFT instance.

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
    >>> obj = FFT(n=1024, sign=-1, nthreads=1)

    """
    def __init__(self, n: int = ..., sign: int = ..., nthreads: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def execute_cf64(self, x: complex) -> NDArray[np.complex128]:
        """Out-of-place 1-D CF64 FFT.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex128]
            n (samples written).
        """

    def execute_cf32(self, x: complex) -> NDArray[np.complex64]:
        """Out-of-place 1-D CF32 FFT.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex64]
            n (samples written).
        """

    def execute_inplace_cf64(self, x: complex) -> NDArray[np.complex128]:
        """In-place 1-D CF64 FFT (copies in→out, then transforms in out).

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex128]
            n (samples written).
        """

    def execute_inplace_cf32(self, x: complex) -> NDArray[np.complex64]:
        """In-place 1-D CF32 FFT (copies in→out, then transforms in out).

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex64]
            n (samples written).
        """

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
    """Create a 2-D FFT instance.

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
    >>> obj = FFT2D(ny=64, nx=64, sign=-1, nthreads=1)

    """
    def __init__(self, ny: int = ..., nx: int = ..., sign: int = ..., nthreads: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def execute_cf64(self, x: complex) -> NDArray[np.complex128]:
        """Out-of-place 2-D CF64 FFT.  Returns ny*nx.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex128]
            Output.
        """

    def execute_cf32(self, x: complex) -> NDArray[np.complex64]:
        """Out-of-place 2-D CF32 FFT.  Returns ny*nx.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def execute_inplace_cf64(self, x: complex) -> NDArray[np.complex128]:
        """In-place 2-D CF64 FFT (copies in→out, then transforms).

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex128]
            Output.
        """

    def execute_inplace_cf32(self, x: complex) -> NDArray[np.complex64]:
        """In-place 2-D CF32 FFT (copies in→out, then transforms).

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

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
    """Create a 1-D FFT correlator.

    Parameters
    ----------
    ref : NDArray[np.complex64], default ...
        ref constructor parameter.
    dwell : int, default 1
        dwell constructor parameter.
    nthreads : int, default 1
        nthreads constructor parameter.

    """
    def __init__(self, ref: NDArray[np.complex64] = ..., dwell: int = ..., nthreads: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def execute(self, x: complex) -> NDArray[np.complex64]:
        """Correlate one frame and optionally dump the accumulator.

        Steps:

        1. FFT(in) → work_fft

        2. `work_fft[k] *= ref_spec[k]`  (frequency-domain multiplication)

        3. IFFT(work_fft) → work_ifft  (unnormalized; divide by n)

        4. `accum[k] += work_ifft[k] / n`

        5. count++

        6. If count == dwell: copy accum → out, zero accum, reset count,

        return n.

        7. Otherwise: return 0 (no output this call).

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex64]
            n on a dump, 0 otherwise.
        """

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
    """Create a 2-D FFT correlator.

    Parameters
    ----------
    ref : NDArray[np.complex64], default ...
        ref constructor parameter.
    dwell : int, default 1
        dwell constructor parameter.
    nthreads : int, default 1
        nthreads constructor parameter.

    """
    def __init__(self, ref: NDArray[np.complex64] = ..., dwell: int = ..., nthreads: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def execute(self, x: complex) -> NDArray[np.complex64]:
        """Correlate one 2-D frame and optionally dump the accumulator.

        Steps:

        1. FFT2(in) → work_fft

        2. `work_fft[k] *= ref_spec[k]`

        3. IFFT2(work_fft) → work_ifft  (divide by ny*nx)

        4. `accum[k] += work_ifft[k] / (ny*nx)`

        5. If count == dwell: copy accum → out, zero, reset, return ny*nx.

        6. Otherwise: return 0.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex64]
            ny*nx on a dump, 0 otherwise.
        """

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
    """Create a 1-D signal detector.

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
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        noise_mode constructor parameter.
    threshold : float, default 0.0
        threshold constructor parameter.
    nthreads : int, default 1
        nthreads constructor parameter.

    """
    def __init__(self, ref: NDArray[np.complex64] = ..., dwell: int = ..., noise_lo: int = ..., noise_hi: int = ..., noise_mode: Literal["mean", "median", "min", "max"] = "mean", threshold: float = ..., nthreads: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def push(self, x: complex) -> list[tuple[int, float, float, float]]:
        """Push an arbitrary-length CF32 chunk through the detector.

        Writes @p n_in complex samples into the ring buffer in the minimum number

        of chunks that fit, then drains all complete n-sample frames through the

        correlator.  On every int-dump a test statistic is computed; if it passes

        the threshold, a det_result_t is appended to @p result[].  The function

        returns as soon as @p n_in samples have been consumed or @p max_results

        detections have been stored, whichever comes first.


        The @p result array must be pre-allocated by the caller.  A stack array of

        64 elements is sufficient for any realistic push size:

        @code

        det_result_t buf[64];

        size_t n = detector_push(det, chunk, len, buf, 64);

        @endcode

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        list[tuple[int, float, float, float]]
            Number of detections stored in @p result[].
        """

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
    """Create a 2-D signal detector.

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
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        noise_mode constructor parameter.
    threshold : float, default 0.0
        threshold constructor parameter.
    nthreads : int, default 1
        nthreads constructor parameter.

    """
    def __init__(self, ref: NDArray[np.complex64] = ..., dwell: int = ..., noise_lo: int = ..., noise_hi: int = ..., noise_mode: Literal["mean", "median", "min", "max"] = "mean", threshold: float = ..., nthreads: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def push(self, x: complex) -> list[tuple[int, int, float, float, float]]:
        """Push an arbitrary-length CF32 chunk through the 2-D detector.

        Behaviour is identical to detector_push() except frames have length ny*nx

        and results carry (row, col) instead of a single lag.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        list[tuple[int, int, float, float, float]]
            Number of detections stored in @p result[].
        """

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

def find_peaks_f32(db: NDArray[np.float32], n_peaks: int, min_db: float) -> Any:
    """Find peaks f32."""
