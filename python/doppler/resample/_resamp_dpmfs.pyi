"""Type stubs for the dp_resamp_dpmfs C extension."""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

class ResampDpmfs:
    """Dual Phase Modified Farrow Structure resampler for complex64 samples.

    Wraps ``dp_resamp_dpmfs_t`` from the C library. A high-performance
    resampler with minimal coefficient footprint (608 bytes) suitable
    for multi-channel pipelines.

    Construct via :class:`doppler.resample.ResamplerDpmfs` rather than
    directly — it handles coefficient array layout automatically.

    Parameters
    ----------
    coeffs : object with .c attribute
        Polynomial coefficients from :func:`doppler.polyphase.fit_dpmfs`.
        The .c attribute must be a float32 array of shape ``(2, M+1, N)``,
        where:
        - ``coeffs.c[0, :, :]`` — (M+1) × N coefficients for μ ∈ [0, 0.5)
        - ``coeffs.c[1, :, :]`` — (M+1) × N coefficients for μ ∈ [0.5, 1.0)
        - M = polynomial order (1–3)
        - N = taps per branch
        Row-major layout: ``coeffs.c[j, m, k]`` is the *k*-th tap
        coefficient for phase half *j* and polynomial order *m*.
    rate : float
        Output-to-input sample-rate ratio ``fs_out / fs_in``.
        *rate* > 1 → interpolation; *rate* < 1 → decimation.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.resample import ResamplerDpmfs
    >>> from doppler.polyphase import kaiser_prototype, fit_dpmfs
    >>> _, bank = kaiser_prototype()
    >>> coeffs = fit_dpmfs(bank, M=3)
    >>> r = ResamplerDpmfs(coeffs, rate=1.5)
    >>> x = np.ones(64, dtype=np.complex64)
    >>> y = r.execute(x)
    >>> y.dtype
    dtype('complex64')
    """

    def __init__(self, coeffs, rate: float) -> None: ...

    # ------------------------------------------------------------------
    # Properties
    # ------------------------------------------------------------------

    def rate(self) -> float:
        """Return the output-to-input sample-rate ratio (fs_out / fs_in)."""
        ...

    def num_taps(self) -> int:
        """Return the number of taps per branch (N)."""
        ...

    def poly_order(self) -> int:
        """Return the polynomial order (M)."""
        ...

    # ------------------------------------------------------------------
    # Processing
    # ------------------------------------------------------------------

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Resample a block of complex64 samples.

        State is preserved across calls — suitable for streaming.

        Parameters
        ----------
        x : NDArray[np.complex64]
            1-D input array of complex64 samples.

        Returns
        -------
        NDArray[np.complex64]
            Output samples.  Length ≈ ``len(x) * rate``;
            exact count depends on the phase accumulator state.
        """
        ...

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def reset(self) -> None:
        """Zero the sample history and reset the phase accumulator."""
        ...

    def __enter__(self) -> "ResampDpmfs": ...
    def __exit__(self, *args: object) -> None: ...
