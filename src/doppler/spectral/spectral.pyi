# spectral/spectral.pyi — type stubs for the spectral C extension.
from typing import Literal
import numpy as np
from numpy.typing import NDArray

class FFT:
    """Allocate a reusable 1-D FFT engine for a fixed length and sign. Two pocketfft plans are created at construction time — one for CF64 and one for CF32 — so execute calls carry no plan-setup overhead.  The same instance may be called repeatedly for independent input vectors of the same length.  nthreads is accepted for API parity but is ignored; pocketfft plans are single-threaded.

    Parameters
    ----------
    n : int, default 1024
        Transform length in samples (power of two recommended).
    sign : int, default -1
        -1 for the forward DFT, +1 for the inverse DFT.
    nthreads : int, default 1
        Accepted for API compatibility; ignored.

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

    def execute_cf64(
        self,
        x: NDArray[np.complex128],
        out: NDArray[np.complex128] | None = ...,
    ) -> NDArray[np.complex128]:
        """Compute an out-of-place 1-D DFT on a double-precision complex input. The output is written to a fresh caller-supplied buffer; in and out must not alias.  The transform is unnormalised: the inverse DFT (sign=+1) does NOT divide by n.  Both buffers must be exactly state->n elements long.

        Without out=, the returned array is a view into a buffer reused on
        the next call (see execute_cf64_max_out() to size an out= buffer
        for an independent, alias-free result).

        Parameters
        ----------
        x : NDArray[np.complex128]
            Input.
        out : NDArray[np.complex128], optional
            Caller-provided output buffer, at least
            max(execute_cf64_max_out(), len(x)) elements.

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

    def execute_cf64_max_out(self) -> int:
        """Max output length execute_cf64() can produce for the current state. Use to size the ``out=`` buffer."""

    def execute_cf32(
        self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = ...
    ) -> NDArray[np.complex64]:
        """Compute an out-of-place 1-D DFT on a single-precision complex input. Identical to fft_execute_cf64() but operates on float complex (CF32) buffers, halving memory bandwidth relative to the double-precision variant. Output is unnormalised; in and out must not alias.

        Without out=, the returned array is a view into a buffer reused on
        the next call (see execute_cf32_max_out() to size an out= buffer
        for an independent, alias-free result).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.
        out : NDArray[np.complex64], optional
            Caller-provided output buffer, at least
            max(execute_cf32_max_out(), len(x)) elements.

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

    def execute_cf32_max_out(self) -> int:
        """Max output length execute_cf32() can produce for the current state. Use to size the ``out=`` buffer."""

    def execute_ci16(self, iq: NDArray[np.int16]) -> NDArray[np.complex64]:
        """Out-of-place 1-D FFT directly on interleaved int16 I/Q (CF32 out). The int16->float convert (v/32768, full-scale +/-1.0) is fused into the transform, so it is faster than i16_to_f32 then execute_cf32.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.spectral import FFT
        >>> obj = FFT(1024, -1, 1)
        >>> y = obj.execute_ci16(np.zeros(2048, dtype=np.int16))
        >>> y.dtype
        dtype('complex64')

        """

    def execute_ci8(self, iq: NDArray[np.int8]) -> NDArray[np.complex64]:
        """Out-of-place 1-D FFT directly on interleaved int8 I/Q (CF32 out). As execute_ci16 but int8 input (v/128, full-scale +/-1.0).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.spectral import FFT
        >>> obj = FFT(1024, -1, 1)
        >>> y = obj.execute_ci8(np.zeros(2048, dtype=np.int8))
        >>> y.dtype
        dtype('complex64')

        """

    def execute_inplace_cf64(
        self,
        x: NDArray[np.complex128],
        out: NDArray[np.complex128] | None = ...,
    ) -> NDArray[np.complex128]:
        """Copy in into out, then transform out in-place (CF64). The copy step lets callers preserve their input while keeping the output buffer hot in cache.  Semantically identical to fft_execute_cf64() for separate in / out pointers; use this variant when the caller already owns out and wants the result there without a second allocation.

        Without out=, the returned array is a view into a buffer reused on
        the next call (see execute_inplace_cf64_max_out() to size an out=
        buffer for an independent, alias-free result).

        Parameters
        ----------
        x : NDArray[np.complex128]
            Input.
        out : NDArray[np.complex128], optional
            Caller-provided output buffer, at least
            max(execute_inplace_cf64_max_out(), len(x)) elements.

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

    def execute_inplace_cf64_max_out(self) -> int:
        """Max output length execute_inplace_cf64() can produce for the current state. Use to size the ``out=`` buffer."""

    def execute_inplace_cf32(
        self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = ...
    ) -> NDArray[np.complex64]:
        """Copy in into out, then transform out in-place (CF32). Single-precision variant of fft_execute_inplace_cf64().  Copies state->n CF32 samples from in to out, then transforms out with the CF32 pocketfft plan.  in is left unmodified.

        Without out=, the returned array is a view into a buffer reused on
        the next call (see execute_inplace_cf32_max_out() to size an out=
        buffer for an independent, alias-free result).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.
        out : NDArray[np.complex64], optional
            Caller-provided output buffer, at least
            max(execute_inplace_cf32_max_out(), len(x)) elements.

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

    def execute_inplace_cf32_max_out(self) -> int:
        """Max output length execute_inplace_cf32() can produce for the current state. Use to size the ``out=`` buffer."""

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
    """Allocate a reusable 2-D FFT engine for a fixed ny×nx grid. Two pocketfft 2-D plans are built at construction time — one CF64, one CF32.  All execute calls accept and return flat row-major arrays of length ny*nx; the Python layer may reshape them with .reshape(ny, nx). nthreads is accepted for API parity but ignored.

    Parameters
    ----------
    ny : int, default 64
        Number of rows (outer dimension).
    nx : int, default 64
        Number of columns (inner dimension).
    sign : int, default -1
        -1 for the forward DFT, +1 for the inverse DFT.
    nthreads : int, default 1
        Accepted for API compatibility; ignored.

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

    def execute_cf64(
        self,
        x: NDArray[np.complex128],
        out: NDArray[np.complex128] | None = ...,
    ) -> NDArray[np.complex128]:
        """Compute an out-of-place 2-D DFT on a double-precision complex grid. in is a flat row-major CF64 array of length ny*nx.  The output is written to the caller-supplied out buffer (also ny*nx); the two must not alias.  The transform is unnormalised.

        Without out=, the returned array is a view into a buffer reused on
        the next call (see execute_cf64_max_out() to size an out= buffer
        for an independent, alias-free result).

        Parameters
        ----------
        x : NDArray[np.complex128]
            Input.
        out : NDArray[np.complex128], optional
            Caller-provided output buffer, at least
            max(execute_cf64_max_out(), len(x)) elements.

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

    def execute_cf64_max_out(self) -> int:
        """Max output length execute_cf64() can produce for the current state. Use to size the ``out=`` buffer."""

    def execute_cf32(
        self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = ...
    ) -> NDArray[np.complex64]:
        """Compute an out-of-place 2-D DFT on a single-precision complex grid. Single-precision variant of fft2d_execute_cf64().  Accepts and returns flat row-major CF32 arrays of length ny*nx.  Output is unnormalised; in and out must not alias.

        Without out=, the returned array is a view into a buffer reused on
        the next call (see execute_cf32_max_out() to size an out= buffer
        for an independent, alias-free result).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.
        out : NDArray[np.complex64], optional
            Caller-provided output buffer, at least
            max(execute_cf32_max_out(), len(x)) elements.

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

    def execute_cf32_max_out(self) -> int:
        """Max output length execute_cf32() can produce for the current state. Use to size the ``out=`` buffer."""

    def execute_inplace_cf64(
        self,
        x: NDArray[np.complex128],
        out: NDArray[np.complex128] | None = ...,
    ) -> NDArray[np.complex128]:
        """Copy in into out, then transform out in-place (CF64 2-D). The ny*nx CF64 samples from in are first memcpy'd to out; the 2-D DFT is then applied to out in-place.  in is left unmodified. Useful when the caller owns out and wants to preserve in.

        Without out=, the returned array is a view into a buffer reused on
        the next call (see execute_inplace_cf64_max_out() to size an out=
        buffer for an independent, alias-free result).

        Parameters
        ----------
        x : NDArray[np.complex128]
            Input.
        out : NDArray[np.complex128], optional
            Caller-provided output buffer, at least
            max(execute_inplace_cf64_max_out(), len(x)) elements.

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

    def execute_inplace_cf64_max_out(self) -> int:
        """Max output length execute_inplace_cf64() can produce for the current state. Use to size the ``out=`` buffer."""

    def execute_inplace_cf32(
        self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = ...
    ) -> NDArray[np.complex64]:
        """Copy in into out, then transform out in-place (CF32 2-D). Single-precision variant of fft2d_execute_inplace_cf64().  Copies ny*nx CF32 samples then applies the CF32 2-D pocketfft plan to out.

        Without out=, the returned array is a view into a buffer reused on
        the next call (see execute_inplace_cf32_max_out() to size an out=
        buffer for an independent, alias-free result).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.
        out : NDArray[np.complex64], optional
            Caller-provided output buffer, at least
            max(execute_inplace_cf32_max_out(), len(x)) elements.

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

    def execute_inplace_cf32_max_out(self) -> int:
        """Max output length execute_inplace_cf32() can produce for the current state. Use to size the ``out=`` buffer."""

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
    """Allocate a 1-D FFT correlator with coherent integrate-and-dump. Pre-computes conj(FFT(ref)) once at construction so each execute() call costs only two FFTs and n complex multiplies.  ref may be freed after this returns.  With dwell == 1 every call produces output; with larger values the accumulator absorbs dwell frames before dumping.

    Parameters
    ----------
    ref : NDArray[np.complex64], default ...
        Reference signal, CF32, length n.
    dwell : int, default 1
        Integration depth; must be >= 1.  Pass 1 for immediate output on every call.
    nthreads : int, default 1
        Accepted for API compatibility; ignored.
    n_out : int, default 0
        Inverse/output length; 0 => native (n).  Must be >= n.  A larger value zero-pads the cross-spectrum before the inverse, returning the band-limited (Dirichlet) interpolation of the correlation on a finer length-n_out grid — same peak, sub-bin lag resolution.  Native is bit-exact and allocates no extra buffer.

    """
    def __init__(self, ref: NDArray[np.complex64] = ..., dwell: int = ..., nthreads: int = ..., n_out: int = ...) -> None: ...

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

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Correlate one frame and optionally dump the coherent accumulator. Runs: forward FFT → pointwise multiply with ref_spec → accumulate the cross-spectrum; on dump, inverse FFT → normalise (÷ n).  Accumulating in the frequency domain and inverting once is exactly the per-frame inverse summed, by linearity of the IFFT — valid because the dwell is **coherent** (a complex sum); a non-coherent (magnitude) integration could not defer the inverse. On the dwell-th call out is written, the accumulator is zeroed, and the counter resets; the function returns n_out.  All other calls return 0 and leave out unmodified.  In Python, a dump returns an ndarray and a no-dump returns None.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            n_out on a dump call, 0 otherwise (None in Python).

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

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def n(self) -> int:
        """N."""

    @property
    def n_out(self) -> int:
        """N out."""

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
    """Allocate a 2-D FFT correlator with coherent integrate-and-dump. Two-dimensional extension of corr_create().  The reference is a flat row-major ny×nx CF32 array; its conjugate spectrum is pre-computed once so each execute() call costs two 2-D FFTs plus ny*nx complex multiplies. The Python wrapper requires ref to be a 2-D ndarray with shape (ny, nx); it passes a flat view to C.

    Parameters
    ----------
    ref : NDArray[np.complex64], default ...
        Reference image, 2-D (ny, nx) CF32 ndarray in Python.
    dwell : int, default 1
        Integration depth; must be >= 1.
    nthreads : int, default 1
        Accepted for API compatibility; ignored.
    ny_out : int, default 0
        Inverse/output rows; 0 => native (ny).  Must be >= ny.  A larger output zero-pads the cross-spectrum before the inverse, returning the band-limited (Dirichlet) interpolation of the correlation on a finer (ny_out, nx_out) grid — same peak, sub-bin resolution.  Native is bit-exact and allocates no extra buffers.
    nx_out : int, default 0
        Inverse/output columns; 0 => native (nx).  Must be >= nx.

    """
    def __init__(self, ref: NDArray[np.complex64] = ..., dwell: int = ..., nthreads: int = ..., ny_out: int = ..., nx_out: int = ...) -> None: ...

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

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Correlate one 2-D frame and optionally dump the coherent accumulator. Runs the 2-D pipeline: FFT2 → pointwise multiply with ref_spec → accumulate the cross-spectrum; on dump, IFFT2 → normalise (÷ ny*nx).  Accumulating in the frequency domain and inverting once is exactly the per-frame inverse summed, by linearity of the IFFT — valid because the dwell is **coherent** (a complex sum); a non-coherent (magnitude) integration could not defer the inverse.  The Python wrapper accepts a (ny, nx) CF32 ndarray; a dump returns a flat length-ny*nx ndarray, a no-dump returns None.

        Parameters
        ----------
        x : NDArray[np.complex64]
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

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def ny(self) -> int:
        """Ny."""

    @property
    def nx(self) -> int:
        """Nx."""

    @property
    def ny_out(self) -> int:
        """Ny out."""

    @property
    def nx_out(self) -> int:
        """Nx out."""

    @property
    def n_out(self) -> int:
        """N out."""

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

class CorrDetector:
    """Allocate a 1-D streaming signal detector backed by an FFT correlator. Combines a corr_state_t with a double-mapped ring buffer so that arbitrary chunk sizes can be pushed.  After every int-dump the peak-to-noise test statistic is compared against threshold; a det_result_t is emitted when it passes.  Setting threshold to 0.0 unconditionally fires on every dump. The ring capacity is next_pow2(max(n, 512)) complex samples.

    Parameters
    ----------
    ref : NDArray[np.complex64], default ...
        Reference signal, CF32 ndarray of length n.
    dwell : int, default 1
        Int-dump depth; must be >= 1.
    noise_lo : int, default 0
        Lower noise bin index (inclusive, 0-based).
    noise_hi : int, default n-1
        Upper noise bin index (inclusive, < n).
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        Noise aggregation: "mean", "median", "min", or "max".
    threshold : float, default 0.0
        Test-stat gate; 0.0 = always emit.
    nthreads : int, default 1
        Accepted for API compatibility; ignored.

    """
    def __init__(self, ref: NDArray[np.complex64] = ..., dwell: int = ..., noise_lo: int = ..., noise_hi: int = ..., noise_mode: Literal["mean", "median", "min", "max"] = "mean", threshold: float = ..., nthreads: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset the correlator, ring buffer, and last-corr flag. Discards any partial frame buffered in the ring and zeroes the coherent accumulator.  Equivalent to starting fresh from the same reference without rebuilding any internal object.

        Examples
        --------
        >>> from doppler.spectral import CorrDetector
        >>> import numpy as np
        >>> ref = np.zeros(8, dtype=np.complex64); ref[0] = 1.0
        >>> det = CorrDetector(ref=ref, dwell=1, noise_lo=1, noise_hi=7,
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
            Number of det_result_t entries written to result.

        Examples
        --------
        >>> from doppler.spectral import CorrDetector
        >>> import numpy as np
        >>> ref = np.zeros(8, dtype=np.complex64); ref[0] = 1.0
        >>> det = CorrDetector(ref=ref, dwell=1, noise_lo=1, noise_hi=7,
        ...                noise_mode="mean", threshold=0.0)
        >>> results = det.push(np.ones(8, dtype=np.complex64))
        >>> len(results)
        1
        >>> lag, peak, noise, stat = results[0]
        >>> lag, round(peak, 4), round(noise, 4), round(stat, 4)
        (0, 1.0, 1.0, 1.0)

        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

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
        """The correlation vector from the most recent push() that produced a result (None before that). This is a zero-copy view into a buffer owned by the detector and reused every push() -- the next push() (even one that doesn't produce a result) overwrites it in place. Copy the array before the next push() if you need to retain it."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "CorrDetector": ...

    def __exit__(self, *args: object) -> None: ...

class CorrDetector2D:
    """Allocate a 2-D streaming signal detector backed by a 2-D correlator. Two-dimensional extension of detector_create().  Input frames are flat row-major CF32 arrays of length ny*nx streamed through a ring buffer.  On every int-dump the peak flat index is decomposed into (row, col) and a det_result2d_t is emitted when test_stat > threshold.  The Python wrapper accepts a (ny, nx) CF32 ndarray for both ref and the push input.

    Parameters
    ----------
    ref : NDArray[np.complex64], default ...
        2-D reference image, (ny, nx) CF32 ndarray in Python.
    dwell : int, default 1
        Int-dump depth; must be >= 1.
    noise_lo : int, default 0
        Lower flat-index noise bin (inclusive, 0-based).
    noise_hi : int, default ny*nx-1
        Upper flat-index noise bin (inclusive, < ny*nx).
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        Noise aggregation: "mean", "median", "min", or "max".
    threshold : float, default 0.0
        Test-stat gate; 0.0 = always emit.
    nthreads : int, default 1
        Accepted for API compatibility; ignored.

    """
    def __init__(self, ref: NDArray[np.complex64] = ..., dwell: int = ..., noise_lo: int = ..., noise_hi: int = ..., noise_mode: Literal["mean", "median", "min", "max"] = "mean", threshold: float = ..., nthreads: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset the 2-D correlator, ring buffer, and last-corr flag. Discards any partial frame buffered in the ring and zeroes the coherent accumulator.  The reference spectrum and FFT plans are preserved.

        Examples
        --------
        >>> from doppler.spectral import CorrDetector2D
        >>> import numpy as np
        >>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
        >>> det = CorrDetector2D(ref=ref, dwell=1, noise_lo=1, noise_hi=15,
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
            Number of det_result2d_t entries written to result.

        Examples
        --------
        >>> from doppler.spectral import CorrDetector2D
        >>> import numpy as np
        >>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
        >>> det = CorrDetector2D(ref=ref, dwell=1, noise_lo=1, noise_hi=15,
        ...                  noise_mode="mean", threshold=0.0)
        >>> results = det.push(np.ones((4, 4), dtype=np.complex64))
        >>> len(results)
        1
        >>> row, col, peak, noise, stat = results[0]
        >>> row, col, round(peak, 4), round(noise, 4), round(stat, 4)
        (0, 0, 1.0, 1.0, 1.0)

        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

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
        """The correlation vector from the most recent push() that produced a result (None before that). This is a zero-copy view into a buffer owned by the detector and reused every push() -- the next push() (even one that doesn't produce a result) overwrites it in place. Copy the array before the next push() if you need to retain it."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "CorrDetector2D": ...

    def __exit__(self, *args: object) -> None: ...

class PSD:
    """Create an averaging PSD estimator.

    Parameters
    ----------
    n : int, default 1024
        Window / frame length in samples.  Must be >= 2.
    fs : float, default 1.0
        Sample rate in Hz (used for dB/Hz and band frequencies).
    window : Literal["hann", "kaiser", "blackman-harris"], default "hann"
        Window index: 0 = Hann, 1 = Kaiser, 2 = Blackman-Harris.
    beta : float, default 0.0
        Kaiser beta (ignored for Hann/Blackman-Harris).
    pad : int, default 1
        Zero-pad factor (>= 1); nfft = next_pow2(n * pad).
    full_scale : float, default 1.0
        Amplitude that reads 0 dBFS in the dB getters (> 0). Ignored when bits > 0.
    bits : int, default 0
        ADC depth: when > 0, sets full_scale = 2^(bits-1) (the single definition of the dBFS reference); 0 = use full_scale directly.
    mode : Literal["mean", "exp", "maxhold", "minhold"], default "mean"
        Averaging mode index (0=mean, 1=exp, 2=maxhold, 3=minhold).
    alpha : float, default 0.1
        EMA smoothing factor (exp mode only).

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

    def band_power(
        self, bands: NDArray[np.float64], out: NDArray[np.float32] | None = ...
    ) -> NDArray[np.float32]:
        """Integrated power per band in dB; bands = [lo0,hi0,lo1,hi1,...] Hz.

        Without out=, the returned array is a view into a buffer reused on
        the next call (see band_power_max_out() to size an out= buffer for
        an independent, alias-free result).

        Parameters
        ----------
        bands : NDArray[np.float64]
            Flat `[lo,hi,...]` band edges, Hz.
        out : NDArray[np.float32], optional
            Caller-provided output buffer, at least
            max(band_power_max_out(), len(bands)) elements.

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

    def band_power_max_out(self) -> int:
        """Max output length band_power() can produce for the current state. Use to size the ``out=`` buffer."""

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

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

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
    """Compute the equivalent noise bandwidth of a window in bins. ENBW = N * sum(w²) / (sum(w))² quantifies how many noise bins the window smears into the main lobe.  A rectangular window has ENBW = 1.0; tapered windows are > 1.0.  Works with any window type, not just Kaiser.

    Parameters
    ----------
    w : NDArray[np.float32]
        Float32 window coefficients array; any length >= 1.

    Returns
    -------
    float
        ENBW in bins (dimensionless).

    Examples
    --------
    >>> from doppler.spectral import kaiser_enbw, hann_window
    >>> import numpy as np
    >>> w = np.zeros(8, dtype=np.float32)
    >>> hann_window(w)
    >>> round(kaiser_enbw(w), 4)
    1.7143

    """

def kaiser_window(w: NDArray[np.float32], beta: float) -> None:
    """Fill w with a Kaiser window of shape parameter beta. I0 is computed via the converging power-series expansion.  Increasing beta raises sidelobe attenuation at the cost of a wider main lobe (beta=0 → rectangular, beta≈6 → ~60 dB sidelobe rejection).  The output is normalised so that `w[0]` = `w[N-1]` = I0(0)/I0(beta).

    Parameters
    ----------
    w : NDArray[np.float32]
        Output buffer modified in-place; must be length >= 1.
    beta : float
        Window shape parameter (float, >= 0).

    Examples
    --------
    >>> from doppler.spectral import kaiser_window
    >>> import numpy as np
    >>> w = np.zeros(8, dtype=np.float32)
    >>> kaiser_window(w, 6.0)
    >>> [round(v, 4) for v in w.tolist()]
    [0.0149, 0.1998, 0.5913, 0.9454, 0.9454, 0.5913, 0.1998, 0.0149]

    """

def kaiser_beta_for_sidelobe(atten_db: float) -> float:
    """Kaiser beta achieving a target *window* peak-sidelobe attenuation.

    Inverts the Kaiser window-design formula (Kaiser 1974) so the window's
    own peak sidelobe sits at -atten_db: A > 60 dB : beta = 0.12438 * (A +
    6.3) 13.26 < A <= 60 dB : beta = 0.76609*(A-13.26)^0.4 +
    0.09834*(A-13.26) A <= 13.26 dB : beta = 0.0 (rectangular, sidelobes ~
    -13.3 dB) Picking the smallest beta meeting a dynamic-range target keeps
    the main lobe (hence ENBW / resolution bandwidth) as narrow as the
    requirement allows — the basis of the measurement suite's auto-window
    selection.

    This differs from doppler.resample.kaiser_beta(), which uses the Kaiser
    *FIR-filter* formula (A there is a filter stopband ripple, not a window
    sidelobe — about 13 dB lower for the same beta).

    Parameters
    ----------
    atten_db : float
        Desired window peak-sidelobe attenuation in dB (positive).

    Returns
    -------
    float
        Kaiser beta (>= 0.0).

    Examples
    --------
    >>> from doppler.spectral import kaiser_beta_for_sidelobe
    >>> round(kaiser_beta_for_sidelobe(90.0), 4)
    11.9778
    >>> kaiser_beta_for_sidelobe(10.0)
    0.0

    """

def hann_window(w: NDArray[np.float32]) -> None:
    """Fill w with a Hann (raised-cosine) window. Computes w(k) = 0.5*(1 - cos(2π k/(N-1))) for k = 0..N-1.  The window tapers smoothly to zero at both endpoints, providing ~31 dB first-sidelobe rejection.  Takes no shape parameter; use Kaiser for adjustable roll-off.

    Parameters
    ----------
    w : NDArray[np.float32]
        Output buffer modified in-place; must be length >= 1.

    Examples
    --------
    >>> from doppler.spectral import hann_window
    >>> import numpy as np
    >>> w = np.zeros(8, dtype=np.float32)
    >>> hann_window(w)
    >>> [round(v, 4) for v in w.tolist()]
    [0.0, 0.1883, 0.6113, 0.9505, 0.9505, 0.6113, 0.1883, 0.0]

    """

def blackman_harris_window(w: NDArray[np.float32]) -> None:
    """Fill w with a 4-term Blackman-Harris window. Computes the minimum 4-term Blackman-Harris window: w(k) = 0.35875 - 0.48829*cos(2πk/(N-1)) + 0.14128*cos(4πk/(N-1)) - 0.01168*cos(6πk/(N-1)) for k = 0..N-1.  Provides approximately 92 dB first-sidelobe rejection, far deeper than Hann (~31 dB) or Kaiser at β=8 (~80 dB).  Use for quantization and decimation spectra where you need to see low-level artefacts below the noise floor.

    Parameters
    ----------
    w : NDArray[np.float32]
        Output buffer modified in-place; must be length >= 1.

    Examples
    --------
    >>> from doppler.spectral import blackman_harris_window
    >>> import numpy as np
    >>> w = np.zeros(8, dtype=np.float32)
    >>> blackman_harris_window(w)
    >>> [round(v, 4) for v in w.tolist()]
    [0.0001, 0.0334, 0.3328, 0.8894, 0.8894, 0.3328, 0.0334, 0.0001]

    """

def magnitude_db_cf32(x: NDArray[np.complex64], lin_floor: float, offset_db: float) -> NDArray[np.float32]:
    """Convert a CF32 complex spectrum to F32 dB magnitudes. Computes out(k) = 20*log10(max(|x(k)|, lin_floor)) + offset_db for each bin.  The lin_floor guard prevents log10(0); a value of 1e-12 corresponds to a -240 dB noise floor.  offset_db shifts the entire output for calibration (e.g., normalise to 0 dBFS).

    Parameters
    ----------
    x : NDArray[np.complex64]
        CF32 complex spectrum array, length x_len.
    lin_floor : float
        Linear amplitude floor (must be > 0, e.g. 1e-12).
    offset_db : float
        Calibration offset added to every output bin.

    Returns
    -------
    NDArray[np.float32]
        Output.

    Examples
    --------
    >>> from doppler.spectral import magnitude_db_cf32
    >>> import numpy as np
    >>> x = np.array([1+0j, 0.1+0j, 0+0j], dtype=np.complex64)
    >>> magnitude_db_cf32(x, 1e-12, 0.0).tolist()
    [0.0, -20.0, -240.0]

    """

def magnitude_db_cf64(x: NDArray[np.complex128], lin_floor: float, offset_db: float) -> NDArray[np.float32]:
    """Convert a CF64 complex spectrum to F32 dB magnitudes. Double-precision variant of magnitude_db_cf32().  Accepts a CF64 input array and a double lin_floor; output is still F32 because downstream display code typically works in single precision.  The formula and offset_db semantics are identical.

    Parameters
    ----------
    x : NDArray[np.complex128]
        CF64 complex spectrum array, length x_len.
    lin_floor : float
        Linear amplitude floor (double, must be > 0).
    offset_db : float
        Calibration offset added to every output bin.

    Returns
    -------
    NDArray[np.float32]
        Output.

    Examples
    --------
    >>> from doppler.spectral import magnitude_db_cf64
    >>> import numpy as np
    >>> x = np.array([1+0j, 10+0j], dtype=np.complex128)
    >>> magnitude_db_cf64(x, 1e-12, 0.0).tolist()
    [0.0, 20.0]

    """

def find_peaks_f32(db: NDArray[np.float32], n_peaks: int, min_db: float) -> Any:
    """Find up to n_peaks local maxima in a DC-centred F32 dB spectrum. Three-step algorithm: (1) local-max scan — `db[k]` > `db[k-1]` && `db[k]` >= `db[k+1]` with `db[k]` > min_db; (2) parabolic interpolation on each local maximum to produce sub-bin freq_norm accuracy; (3) sort descending and return the top n_peaks.  freq_norm is DC-centred: bin i maps to freq_norm = (i - N/2) / N so DC (bin N/2) → 0.0 and the first negative frequency bin → −0.5.  The spectrum must have at least 3 bins.

    Parameters
    ----------
    db : NDArray[np.float32]
        F32 dB spectrum, DC-centred, length >= 3.
    n_peaks : int
        Maximum number of peaks to return.
    min_db : float
        Amplitude gate; local maxima below this are discarded.

    Returns
    -------
    Any
        Number of dp_peak_t entries written to result.

    Examples
    --------
    >>> from doppler.spectral import find_peaks_f32
    >>> import numpy as np
    >>> db = np.full(32, -60.0, dtype=np.float32)
    >>> db[7] = -15.0; db[8] = -10.0; db[9] = -15.0
    >>> peaks = find_peaks_f32(db, 2, -30.0)
    >>> peaks
    [(-0.25, -10.0)]

    """

def obw_from_power(pwr: NDArray[np.float64], fs: float, frac: float) -> float:
    """Obw from power."""

def noise_floor_db(db: NDArray[np.float32]) -> float:
    """Noise floor db."""
