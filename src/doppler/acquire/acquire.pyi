# acquire/acquire.pyi — type stubs for the acquire C extension.
from typing import Literal
import numpy as np
from numpy.typing import NDArray

class CarrierAcquisition:
    """CarrierAcquisition component.

    Parameters
    ----------
    sample_rate_hz : float, default .0
        sample_rate_hz constructor parameter.
    symbol_rate_hz : float, default .0
        symbol_rate_hz constructor parameter.
    resolution_hz : float, default 0.0
        resolution_hz constructor parameter.
    zero_pad : int, default 4
        zero_pad constructor parameter.
    window : Literal["hann", "kaiser", "blackman-harris"], default "hann"
        window constructor parameter.
    beta : float, default 0.0
        beta constructor parameter.
    psd_template : NDArray[np.float32], default ...
        psd_template constructor parameter.
    pfa : float, default 1e-3
        pfa constructor parameter.
    pd : float, default 0.9
        pd constructor parameter.
    design_snr : float, default 2.0
        design_snr constructor parameter.
    sequential : bool, default true
        sequential constructor parameter.
    max_n_blocks : int, default 100000
        max_n_blocks constructor parameter.

    """
    def __init__(self, sample_rate_hz: float = ..., symbol_rate_hz: float = ..., resolution_hz: float = ..., zero_pad: int = ..., window: Literal["hann", "kaiser", "blackman-harris"] = "hann", beta: float = ..., psd_template: NDArray[np.float32] = ..., pfa: float = ..., pd: float = ..., design_snr: float = ..., sequential: bool = ..., max_n_blocks: int = ...) -> None: ...

    def steps(self, x: NDArray[np.complex64]) -> None:
        """Fold raw complex samples into the running PSD average and test for a detection; any chunk size across repeated calls (a partial trailing block carries to the next call).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Raw complex input samples (cf32).
        """

    def reset(self) -> None:
        """Discard the running PSD average and detection state; counters return to zero.
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def ready(self) -> bool:
        """True once a detection has fired (or the dwell_target give-up cap was reached) -- residual_hz is only meaningful once this is true."""

    @property
    def residual_hz(self) -> float:
        """Sub-bin-refined residual carrier frequency estimate, Hz. Valid only when ready is true."""

    @property
    def n_blocks(self) -> int:
        """Number of n_fft-length blocks actually folded into the PSD average so far."""

    @property
    def dwell_target(self) -> int:
        """Non-sequential mode's precomputed fixed wait count, from det_n_noncoh(design_snr, ...) at construction. Ignored by sequential mode's own give-up bound -- see max_n_blocks."""

    @property
    def max_n_blocks(self) -> int:
        """Sequential mode's own give-up cap (independent of dwell_target) -- the max_n_blocks constructor argument, echoed back."""

    @property
    def nfft(self) -> int:
        """PSD transform length (next_pow2(n_fft*zero_pad)) -- the length any caller-supplied template array must match."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "CarrierAcquisition": ...

    def __exit__(self, *args: object) -> None: ...
