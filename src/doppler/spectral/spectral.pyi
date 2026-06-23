# spectral/spectral.pyi — type stubs for the spectral C extension.
from typing import Literal
import numpy as np
from numpy.typing import NDArray

class FFT:
    """Allocate a reusable 1-D FFT engine for a fixed length and sign. Two pocketfft plans are created at construction time — one for CF64 and one for CF32 — so execute calls carry no plan-setup overhead.  The same instance may be called repeatedly for independent input vectors of the same length.  @p nthreads is accepted for API parity but is ignored; pocketfft plans are single-threaded.

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
        """No-op reset (plans are immutable after creation).
        """

    def execute_cf64(self, x: complex) -> NDArray[np.complex128]:
        """Compute an out-of-place 1-D DFT on a double-precision complex input. The output is written to a fresh caller-supplied buffer; @p in and @p out must not alias.  The transform is unnormalised: the inverse DFT (sign=+1) does NOT divide by n.  Both buffers must be exactly state->n elements long.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex128]
            n (number of samples written).

        Examples
        --------
        >>> from doppler.spectral import FFT
        >>> import numpy as np
        >>> fft = FFT(n=4, sign=-1)
        >>> x = np.array([1, 0, 0, 0], dtype=np.complex128)
        >>> fft.execute_cf64(x).tolist()
        [(1+0j), (1+0j), (1+0j), (1+0j)]

        """

    def execute_cf32(self, x: complex) -> NDArray[np.complex64]:
        """Compute an out-of-place 1-D DFT on a single-precision complex input. Identical to fft_execute_cf64() but operates on float complex (CF32) buffers, halving memory bandwidth relative to the double-precision variant. Output is unnormalised; @p in and @p out must not alias.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex64]
            n (number of samples written).

        Examples
        --------
        >>> from doppler.spectral import FFT
        >>> import numpy as np
        >>> fft = FFT(n=4, sign=-1)
        >>> x = np.ones(4, dtype=np.complex64)
        >>> fft.execute_cf32(x).tolist()
        [(4+0j), 0j, 0j, 0j]

        """

    def execute_inplace_cf64(self, x: complex) -> NDArray[np.complex128]:
        """Copy @p in into @p out, then transform @p out in-place (CF64). The copy step lets callers preserve their input while keeping the output buffer hot in cache.  Semantically identical to fft_execute_cf64() for separate @p in / @p out pointers; use this variant when the caller already owns @p out and wants the result there without a second allocation.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex128]
            n (number of samples written).

        Examples
        --------
        >>> from doppler.spectral import FFT
        >>> import numpy as np
        >>> fft = FFT(n=4, sign=-1)
        >>> x = np.array([1, 0, 0, 0], dtype=np.complex128)
        >>> fft.execute_inplace_cf64(x).tolist()
        [(1+0j), (1+0j), (1+0j), (1+0j)]

        """

    def execute_inplace_cf32(self, x: complex) -> NDArray[np.complex64]:
        """Copy @p in into @p out, then transform @p out in-place (CF32). Single-precision variant of fft_execute_inplace_cf64().  Copies state->n CF32 samples from @p in to @p out, then transforms @p out with the CF32 pocketfft plan.  @p in is left unmodified.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex64]
            n (number of samples written).

        Examples
        --------
        >>> from doppler.spectral import FFT
        >>> import numpy as np
        >>> fft = FFT(n=4, sign=-1)
        >>> x = np.array([1, 0, 0, 0], dtype=np.complex64)
        >>> fft.execute_inplace_cf32(x).tolist()
        [(1+0j), (1+0j), (1+0j), (1+0j)]

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
    """Allocate a reusable 2-D FFT engine for a fixed ny×nx grid. Two pocketfft 2-D plans are built at construction time — one CF64, one CF32.  All execute calls accept and return flat row-major arrays of length ny*nx; the Python layer may reshape them with .reshape(ny, nx). @p nthreads is accepted for API parity but ignored.

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
        """No-op reset (plans are immutable after creation).
        """

    def execute_cf64(self, x: complex) -> NDArray[np.complex128]:
        """Compute an out-of-place 2-D DFT on a double-precision complex grid. @p in is a flat row-major CF64 array of length ny*nx.  The output is written to the caller-supplied @p out buffer (also ny*nx); the two must not alias.  The transform is unnormalised.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex128]
            ny*nx (number of samples written).

        Examples
        --------
        >>> from doppler.spectral import FFT2D
        >>> import numpy as np
        >>> fft2d = FFT2D(ny=4, nx=4, sign=-1)
        >>> x = np.zeros(16, dtype=np.complex128); x[0] = 1.0
        >>> out = fft2d.execute_cf64(x)
        >>> out.shape, out.dtype
        ((16,), dtype('complex128'))
        >>> bool(np.allclose(out, 1.0))
        True

        """

    def execute_cf32(self, x: complex) -> NDArray[np.complex64]:
        """Compute an out-of-place 2-D DFT on a single-precision complex grid. Single-precision variant of fft2d_execute_cf64().  Accepts and returns flat row-major CF32 arrays of length ny*nx.  Output is unnormalised; @p in and @p out must not alias.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex64]
            ny*nx (number of samples written).

        Examples
        --------
        >>> from doppler.spectral import FFT2D
        >>> import numpy as np
        >>> fft2d = FFT2D(ny=4, nx=4, sign=-1)
        >>> x = np.zeros(16, dtype=np.complex64); x[0] = 1.0
        >>> out = fft2d.execute_cf32(x)
        >>> out.shape, out.dtype
        ((16,), dtype('complex64'))
        >>> bool(np.allclose(out, 1.0))
        True

        """

    def execute_inplace_cf64(self, x: complex) -> NDArray[np.complex128]:
        """Copy @p in into @p out, then transform @p out in-place (CF64 2-D). The ny*nx CF64 samples from @p in are first memcpy'd to @p out; the 2-D DFT is then applied to @p out in-place.  @p in is left unmodified. Useful when the caller owns @p out and wants to preserve @p in.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex128]
            ny*nx (number of samples written).

        Examples
        --------
        >>> from doppler.spectral import FFT2D
        >>> import numpy as np
        >>> fft2d = FFT2D(ny=4, nx=4, sign=-1)
        >>> x = np.zeros(16, dtype=np.complex128); x[0] = 1.0
        >>> out = fft2d.execute_inplace_cf64(x)
        >>> bool(np.allclose(out, 1.0))
        True

        """

    def execute_inplace_cf32(self, x: complex) -> NDArray[np.complex64]:
        """Copy @p in into @p out, then transform @p out in-place (CF32 2-D). Single-precision variant of fft2d_execute_inplace_cf64().  Copies ny*nx CF32 samples then applies the CF32 2-D pocketfft plan to @p out.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex64]
            ny*nx (number of samples written).

        Examples
        --------
        >>> from doppler.spectral import FFT2D
        >>> import numpy as np
        >>> fft2d = FFT2D(ny=4, nx=4, sign=-1)
        >>> x = np.zeros(16, dtype=np.complex64); x[0] = 1.0
        >>> out = fft2d.execute_inplace_cf32(x)
        >>> bool(np.allclose(out, 1.0))
        True

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
    """Allocate a 1-D FFT correlator with coherent integrate-and-dump. Pre-computes conj(FFT(ref)) once at construction so each execute() call costs only two FFTs and n complex multiplies.  @p ref may be freed after this returns.  With @p dwell == 1 every call produces output; with larger values the accumulator absorbs @p dwell frames before dumping.

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
        """Zero the accumulator and reset the integration counter to 0. Equivalent to starting a fresh dwell cycle without tearing down the FFT plans.  Does NOT recompute ref_spec; use corr_set_ref() to replace the reference.

        Examples
        --------
        >>> from doppler.spectral import Corr
        >>> import numpy as np
        >>> ref = np.zeros(4, dtype=np.complex64); ref[0] = 1.0
        >>> corr = Corr(ref=ref, dwell=3)
        >>> _ = corr.execute(np.ones(4, dtype=np.complex64))
        >>> corr.count
        1
        >>> corr.reset()
        >>> corr.count
        0

        """

    def execute(self, x: complex) -> NDArray[np.complex64]:
        """Correlate one frame and optionally dump the coherent accumulator. Runs the six-step pipeline: forward FFT → pointwise multiply with ref_spec → inverse FFT → normalise (÷ n) → accumulate → conditional dump. On the @p dwell-th call the accumulator is copied to @p out, zeroed, and the counter resets; the function returns n.  All other calls return 0 and leave @p out unmodified.  In Python, a dump returns an ndarray and a no-dump returns None.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex64]
            n on a dump call, 0 otherwise (None in Python).

        Examples
        --------
        >>> from doppler.spectral import Corr
        >>> import numpy as np
        >>> ref = np.zeros(4, dtype=np.complex64); ref[0] = 1.0
        >>> corr = Corr(ref=ref, dwell=2)
        >>> x = np.ones(4, dtype=np.complex64)
        >>> corr.execute(x) is None   # frame 1 — no dump yet
        True
        >>> corr.execute(x).tolist()  # frame 2 — dump
        [(2+0j), (2+0j), (2+0j), (2+0j)]

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
    """Allocate a 2-D FFT correlator with coherent integrate-and-dump. Two-dimensional extension of corr_create().  The reference is a flat row-major ny×nx CF32 array; its conjugate spectrum is pre-computed once so each execute() call costs two 2-D FFTs plus ny*nx complex multiplies. The Python wrapper requires @p ref to be a 2-D ndarray with shape (ny, nx); it passes a flat view to C.

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
        """Zero the accumulator and reset the integration counter to 0. Equivalent to starting a fresh dwell cycle without rebuilding FFT plans or recomputing ref_spec.

        Examples
        --------
        >>> from doppler.spectral import Corr2D
        >>> import numpy as np
        >>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0
        >>> c = Corr2D(ref=ref, dwell=3)
        >>> _ = c.execute(np.ones((2, 2), dtype=np.complex64))
        >>> c.count
        1
        >>> c.reset()
        >>> c.count
        0

        """

    def execute(self, x: complex) -> NDArray[np.complex64]:
        """Correlate one 2-D frame and optionally dump the coherent accumulator. Runs the 2-D pipeline: FFT2 → pointwise multiply with ref_spec → IFFT2 → normalise (÷ ny*nx) → accumulate → conditional dump.  The Python wrapper accepts a (ny, nx) CF32 ndarray; a dump returns a flat length-ny*nx ndarray, a no-dump returns None.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        NDArray[np.complex64]
            ny*nx on a dump, 0 otherwise (None in Python).

        Examples
        --------
        >>> from doppler.spectral import Corr2D
        >>> import numpy as np
        >>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0
        >>> c = Corr2D(ref=ref, dwell=2)
        >>> x = np.ones((2, 2), dtype=np.complex64)
        >>> c.execute(x) is None   # frame 1 — no dump
        True
        >>> c.execute(x).tolist()  # frame 2 — dump
        [(2+0j), (2+0j), (2+0j), (2+0j)]

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
    """Allocate a 1-D streaming signal detector backed by an FFT correlator. Combines a corr_state_t with a double-mapped ring buffer so that arbitrary chunk sizes can be pushed.  After every int-dump the peak-to-noise test statistic is compared against @p threshold; a det_result_t is emitted when it passes.  Setting @p threshold to 0.0 unconditionally fires on every dump. The ring capacity is next_pow2(max(n, 512)) complex samples.

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
        """Reset the correlator, ring buffer, and last-corr flag. Discards any partial frame buffered in the ring and zeroes the coherent accumulator.  Equivalent to starting fresh from the same reference without rebuilding any internal object.

        Examples
        --------
        >>> from doppler.spectral import Detector
        >>> import numpy as np
        >>> ref = np.zeros(8, dtype=np.complex64); ref[0] = 1.0
        >>> det = Detector(ref=ref, dwell=1, noise_lo=1, noise_hi=7,
        ...                noise_mode="mean", threshold=0.0)
        >>> _ = det.push(np.ones(8, dtype=np.complex64))
        >>> det.reset()
        >>> det.count
        0

        """

    def push(self, x: complex) -> list[tuple[int, float, float, float]]:
        """Stream an arbitrary-length CF32 chunk through the detector pipeline. Writes samples into the ring buffer, drains complete n-sample frames through the correlator, and on every int-dump computes the test statistic peak_mag / noise_est.  Detections that pass the threshold are appended to the Python return list as (lag, peak_mag, noise_est, test_stat) tuples. In Python the result is always a list, even when empty.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        list[tuple[int, float, float, float]]
            Number of det_result_t entries written to @p result.

        Examples
        --------
        >>> from doppler.spectral import Detector
        >>> import numpy as np
        >>> ref = np.zeros(8, dtype=np.complex64); ref[0] = 1.0
        >>> det = Detector(ref=ref, dwell=1, noise_lo=1, noise_hi=7,
        ...                noise_mode="mean", threshold=0.0)
        >>> results = det.push(np.ones(8, dtype=np.complex64))
        >>> len(results)
        1
        >>> lag, peak, noise, stat = results[0]
        >>> lag, round(peak, 4), round(noise, 4), round(stat, 4)
        (0, 1.0, 1.0, 1.0)

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
    """Allocate a 2-D streaming signal detector backed by a 2-D correlator. Two-dimensional extension of detector_create().  Input frames are flat row-major CF32 arrays of length ny*nx streamed through a ring buffer.  On every int-dump the peak flat index is decomposed into (row, col) and a det_result2d_t is emitted when test_stat > threshold.  The Python wrapper accepts a (ny, nx) CF32 ndarray for both @p ref and the push input.

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
        """Reset the 2-D correlator, ring buffer, and last-corr flag. Discards any partial frame buffered in the ring and zeroes the coherent accumulator.  The reference spectrum and FFT plans are preserved.

        Examples
        --------
        >>> from doppler.spectral import Detector2D
        >>> import numpy as np
        >>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
        >>> det = Detector2D(ref=ref, dwell=1, noise_lo=1, noise_hi=15,
        ...                  noise_mode="mean", threshold=0.0)
        >>> _ = det.push(np.ones((4, 4), dtype=np.complex64))
        >>> det.reset()
        >>> det.count
        0

        """

    def push(self, x: complex) -> list[tuple[int, int, float, float, float]]:
        """Stream an arbitrary-length CF32 chunk through the 2-D detector. Identical to detector_push() except frames are ny*nx complex samples and each detection event carries (row, col) for the peak location instead of a single lag index.  In Python the result is always a list of (row, col, peak_mag, noise_est, test_stat) tuples.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        list[tuple[int, int, float, float, float]]
            Number of det_result2d_t entries written to @p result.

        Examples
        --------
        >>> from doppler.spectral import Detector2D
        >>> import numpy as np
        >>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
        >>> det = Detector2D(ref=ref, dwell=1, noise_lo=1, noise_hi=15,
        ...                  noise_mode="mean", threshold=0.0)
        >>> results = det.push(np.ones((4, 4), dtype=np.complex64))
        >>> len(results)
        1
        >>> row, col, peak, noise, stat = results[0]
        >>> row, col, round(peak, 4), round(noise, 4), round(stat, 4)
        (0, 0, 1.0, 1.0, 1.0)

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

class PSD:
    """Create an averaging PSD estimator.

    Parameters
    ----------
    n : int, default 1024
        n constructor parameter.
    fs : float, default 1.0
        fs constructor parameter.
    window : Literal["hann", "kaiser", "blackman-harris"], default "hann"
        window constructor parameter.
    beta : float, default 0.0
        beta constructor parameter.
    pad : int, default 1
        pad constructor parameter.
    full_scale : float, default 1.0
        full_scale constructor parameter.
    bits : int, default 0
        bits constructor parameter.
    mode : Literal["mean", "exp", "maxhold", "minhold"], default "mean"
        mode constructor parameter.
    alpha : float, default 0.1
        alpha constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.spectral import PSD
    >>> obj = PSD(n=1024, fs=1.0, window="hann", beta=0.0, pad=1, full_scale=1.0, bits=0, mode="mean", alpha=0.1)

    """
    def __init__(self, n: int = ..., fs: float = ..., window: Literal["hann", "kaiser", "blackman-harris"] = "hann", beta: float = ..., pad: int = ..., full_scale: float = ..., bits: int = ..., mode: Literal["mean", "exp", "maxhold", "minhold"] = "mean", alpha: float = ...) -> None: ...

    def accumulate(self, x: NDArray[np.complex64]) -> None:
        """Window, FFT and fold floor(n_in/n) cf32 frames into the average.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Complex baseband samples (cf32).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.spectral import PSD
        >>> n = 64
        >>> w = PSD(n=n, fs=1.0, window="hann", mode="mean")
        >>> k = 8
        >>> x = np.exp(2j*np.pi*k*np.arange(n)/n).astype(np.complex64)
        >>> for _ in range(4):
        ...     w.accumulate(x)
        >>> psd = w.psd_db()
        >>> psd.shape
        (64,)
        >>> int(np.argmax(psd)) == n // 2 + k
        True
        >>> w.count
        4

        """

    def accumulate_real(self, x: NDArray[np.float32]) -> None:
        """Window, zero-pad, FFT and fold floor(n_in/n) real frames into the average.

        Parameters
        ----------
        x : NDArray[np.float32]
            Real samples (f32).
        """

    def reset(self) -> None:
        """Discard the running average; counters return to zero.
        """

    def psd_db(self) -> NDArray[np.float32]:
        """Averaged power spectrum in dB (None before any accumulate).

        Returns
        -------
        NDArray[np.float32]
            n, or 0 if empty.
        """

    def psd_dbhz(self) -> NDArray[np.float32]:
        """Averaged power spectral density in dB/Hz (None before any accumulate).

        Returns
        -------
        NDArray[np.float32]
            Output.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.spectral import PSD
        >>> w = PSD(n=32, fs=2.0, window="hann", mode="mean")
        >>> w.accumulate(np.ones(32, dtype=np.complex64))
        >>> a = w.psd_db(); b = w.psd_dbhz()
        >>> bool(np.allclose(a - b, (a - b)[0]))   # offset is a constant
        True

        """

    def power_twosided(self) -> NDArray[np.float32]:
        """Averaged linear power, DC-centred two-sided (length nfft); cg^2-normalised.

        Returns
        -------
        NDArray[np.float32]
            nfft, or 0 if empty.
        """

    def power_onesided(self) -> NDArray[np.float32]:
        """Averaged linear power, one-sided fold (length nfft/2+1); cg^2-normalised.

        Returns
        -------
        NDArray[np.float32]
            nfft/2 + 1, or 0 if empty.
        """

    def band_power(self, bands: NDArray[np.float64]) -> NDArray[np.float32]:
        """Integrated power per band in dB; bands = [lo0,hi0,lo1,hi1,...] Hz.

        Parameters
        ----------
        bands : NDArray[np.float64]
            Flat `[lo,hi,...]` band edges, Hz.

        Returns
        -------
        NDArray[np.float32]
            n_bands, or 0 if empty.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.spectral import PSD
        >>> w = PSD(n=64, fs=1.0, window="hann", mode="mean")
        >>> w.accumulate(np.ones(64, dtype=np.complex64))
        >>> pb = w.band_power(np.array([-0.5, 0.0, 0.0, 0.5]))
        >>> pb.shape
        (2,)

        """

    def total_band_power(self, bands: NDArray[np.float64]) -> float:
        """Total integrated power across all bands in dB.

        Parameters
        ----------
        bands : NDArray[np.float64]
            Flat `[lo,hi,...]` band edges, Hz.

        Returns
        -------
        float
            Total band power in dB (dB floor if empty).
        """

    def occupied_bw(self, fraction: float) -> float:
        """Occupied bandwidth in Hz holding the given fraction of total power.

        Parameters
        ----------
        fraction : float
            Power fraction in (0, 1], e.g. 0.99.

        Returns
        -------
        float
            Occupied bandwidth in Hz (0 if empty or no power).
        """

    def noise_floor(self) -> float:
        """Median of the averaged dB trace (noise-floor estimate).

        Returns
        -------
        float
            Median dB level (0 if empty).
        """

    def snr(self, lo_hz: float, hi_hz: float) -> float:
        """Peak-in-band level minus noise floor, in dB.

        Parameters
        ----------
        lo_hz : float
            Band lower edge, Hz.
        hi_hz : float
            Band upper edge, Hz.

        Returns
        -------
        float
            SNR in dB (0 if empty).
        """

    def sfdr(self, min_db: float) -> float:
        """Spurious-free dynamic range in dB from the top two peaks.

        Parameters
        ----------
        min_db : float
            Minimum peak level considered, dB.

        Returns
        -------
        float
            Carrier-minus-highest-spur level in dB (0 if fewer than two peaks).
        """

    @property
    def n(self) -> int:
        """N."""

    @property
    def nfft(self) -> int:
        """Nfft."""

    @property
    def fs(self) -> float:
        """Fs."""

    @property
    def full_scale(self) -> float:
        """Full scale."""

    @property
    def bits(self) -> int:
        """Bits."""

    @property
    def enbw(self) -> float:
        """Enbw."""

    @property
    def rbw(self) -> float:
        """Rbw."""

    @property
    def count(self) -> int:
        """Count."""

    @property
    def mode(self) -> int:
        """Mode."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "PSD": ...

    def __exit__(self, *args: object) -> None: ...

def kaiser_enbw(w: NDArray[np.float32]) -> float:
    """Kaiser enbw."""

def kaiser_window(w: NDArray[np.float32], beta: float) -> None:
    """Kaiser window."""

def kaiser_beta_for_sidelobe(atten_db: float) -> float:
    """Kaiser beta for sidelobe."""

def hann_window(w: NDArray[np.float32]) -> None:
    """Hann window."""

def blackman_harris_window(w: NDArray[np.float32]) -> None:
    """Blackman harris window."""

def magnitude_db_cf32(x: NDArray[np.complex64], lin_floor: float, offset_db: float) -> NDArray[np.float32]:
    """Magnitude db cf32."""

def magnitude_db_cf64(x: NDArray[np.complex128], lin_floor: float, offset_db: float) -> NDArray[np.float32]:
    """Magnitude db cf64."""

def find_peaks_f32(db: NDArray[np.float32], n_peaks: int, min_db: float) -> Any:
    """Find peaks f32."""

def obw_from_power(pwr: NDArray[np.float64], fs: float, frac: float) -> float:
    """Obw from power."""

def noise_floor_db(db: NDArray[np.float32]) -> float:
    """Noise floor db."""
