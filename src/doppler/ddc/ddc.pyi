# ddc/ddc.pyi — type stubs for the ddc C extension.
import numpy as np
from numpy.typing import NDArray

class DDC:
    """Digital Down-Converter — complex input.

    Signal chain: CF32 in → LO mix → polyphase resample → CF32 out.

    Parameters
    ----------
    norm_freq : float
        LO frequency in cycles/sample at the input rate.
        Set to ``-f_carrier`` to shift a carrier at ``f_carrier`` to DC.
    rate : float
        Output / input rate ratio.  Must be > 0.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.ddc import DDC
    >>> ddc = DDC(norm_freq=-0.1, rate=0.25)
    >>> x = np.exp(2j * np.pi * 0.1 * np.arange(4096)).astype(np.complex64)
    >>> y = ddc.execute(x)
    >>> y.dtype
    dtype('complex64')

    """

    def __init__(self, norm_freq: float, rate: float) -> None: ...
    def __enter__(self) -> "DDC": ...
    def __exit__(self, *args: object) -> None: ...
    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Mix input block with LO, then resample.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input samples at the input rate.

        Returns
        -------
        NDArray[np.complex64]
            Resampled output at ``rate * fs_in``.
        """

    def reset(self) -> None:
        """Zero LO phase and resampler history."""

    def get_norm_freq(self) -> float:
        """Return the current LO normalised frequency."""

    def set_norm_freq(self, norm_freq: float) -> None:
        """Retune the LO without resetting phase or resampler history."""

    @property
    def rate(self) -> float:
        """Output / input rate ratio."""

    def destroy(self) -> None:
        """Release C resources immediately."""

class DDCR:
    """Digital Down-Converter — real input (Architecture D2).

    Signal chain:
    float in → halfband R2C (2:1, embedded fs/4 shift)
             → LO mix at intermediate rate (fs_in/2)
             → polyphase resample → CF32 out.

    Parameters
    ----------
    norm_freq : float
        Fine NCO frequency at the *intermediate* rate (fs_in/2).
        To tune a real tone at ``f_carrier`` (input-normalised) to DC:
        ``norm_freq = -(2*f_carrier + 0.5)``.
    rate : float
        Total output / input rate.  Must be in (0, 0.5).

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.ddc import DDCR
    >>> f_carrier = 0.1
    >>> ddcr = DDCR(norm_freq=-(2*f_carrier + 0.5), rate=0.25)
    >>> x = np.cos(2 * np.pi * f_carrier * np.arange(4096)).astype(np.float32)
    >>> y = ddcr.execute(x)
    >>> y.dtype
    dtype('complex64')

    """

    def __init__(self, norm_freq: float, rate: float) -> None: ...
    def __enter__(self) -> "DDCR": ...
    def __exit__(self, *args: object) -> None: ...
    def execute(self, x: NDArray[np.float32]) -> NDArray[np.complex64]:
        """Process a block of real float32 samples through the full DDC chain.

        Parameters
        ----------
        x : NDArray[np.float32]
            Real input samples at the input rate.

        Returns
        -------
        NDArray[np.complex64]
            Complex output at ``rate * fs_in``.
        """

    def reset(self) -> None:
        """Zero halfband, LO phase, and resampler history."""

    def get_norm_freq(self) -> float:
        """Return the current fine NCO normalised frequency (at intermediate rate)."""

    def set_norm_freq(self, norm_freq: float) -> None:
        """Retune the fine NCO without resetting state."""

    @property
    def rate(self) -> float:
        """Total output / input rate (fs_out / fs_in)."""

    def destroy(self) -> None:
        """Release C resources immediately."""
