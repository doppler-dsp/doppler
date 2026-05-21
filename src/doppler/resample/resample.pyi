# resample/resample.pyi — type stubs for the resample C extension.
import numpy as np
from numpy.typing import NDArray

class Resampler:
    """Polyphase resampler.

    Parameters
    ----------
    rate : float
        Output / input rate ratio.

    Examples
    --------
    >>> from doppler.resample import Resampler
    >>> r = Resampler(0.5)

    """

    def __init__(self, rate: float = ...) -> None: ...
    def reset(self) -> None:
        """Reset resampler state to post-create defaults."""

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Resample a block of CF32 samples."""

    def execute_ctrl(
        self, x: NDArray[np.complex64], ctrl: NDArray[np.complex64]
    ) -> NDArray[np.complex64]:
        """Resample with per-sample rate control."""

    @property
    def rate(self) -> float:
        """Output / input rate ratio."""

    @rate.setter
    def rate(self, value: float) -> None: ...
    @property
    def num_phases(self) -> int:
        """Number of polyphase branches."""

    @property
    def num_taps(self) -> int:
        """Taps per polyphase branch."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Resampler": ...
    def __exit__(self, *args: object) -> None: ...

class HalfbandDecimator:
    """Halfband decimator (2:1, CF32 → CF32).

    Parameters
    ----------
    hb_fir : NDArray[np.float32]
        Halfband FIR coefficients (odd-length, symmetric).

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.resample import HalfbandDecimator, _halfband_bank
    >>> h = _halfband_bank(60.0, 0.4, 0.6)
    >>> hb = HalfbandDecimator(h)

    """

    def __init__(self, hb_fir: NDArray[np.float32]) -> None: ...
    def reset(self) -> None:
        """Reset delay state."""

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Decimate a block of CF32 samples by 2."""

    @property
    def rate(self) -> float:
        """Always 0.5."""

    @property
    def num_taps(self) -> int:
        """Number of FIR taps."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "HalfbandDecimator": ...
    def __exit__(self, *args: object) -> None: ...

def kaiser_beta(atten: float) -> float:
    """Compute Kaiser window beta from stopband attenuation (dB)."""

def kaiser_num_taps(num_phases: int, atten: float, pb: float, sb: float) -> int:
    """Compute number of taps per polyphase branch for a Kaiser prototype."""
