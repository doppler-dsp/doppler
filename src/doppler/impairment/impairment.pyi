# impairment/impairment.pyi — type stubs for the impairment C extension.
import numpy as np
from numpy.typing import NDArray

class DopplerChannel:
    """DopplerChannel component.

    Parameters
    ----------
    fs : float, default 1000000.0
        fs constructor parameter.
    carrier_hz : float, default 0.0
        carrier_hz constructor parameter.
    doppler_ppm : float, default 0.0
        doppler_ppm constructor parameter.
    doppler_rate_ppm_s : float, default 0.0
        doppler_rate_ppm_s constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.impairment import DopplerChannel
    >>> obj = DopplerChannel(fs=1000000.0, carrier_hz=0.0, doppler_ppm=0.0, doppler_rate_ppm_s=0.0)

    """
    def __init__(self, fs: float = ..., carrier_hz: float = ..., doppler_ppm: float = ..., doppler_rate_ppm_s: float = ...) -> None: ...

    def execute(self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Apply clock Doppler to a block of complex baseband.

        Resamples x by `1/(1+d(t))` and multiplies the result by the coherent
        carrier `exp(j*2*pi*fc*excess(t))`. State persists across calls, so
        feeding a stream in blocks gives the same samples as one large call
        (subject to `DOPPLER_CHANNEL_MAX_BLOCK`).

        Output length is approximately `x_len/(1+d)` and varies by a sample from
        call to call as the fractional resampling accumulator crosses — that
        variation is the dilation itself, not a defect.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input block.

        Returns
        -------
        NDArray[np.complex64]
            Samples written to out.
        """

    def execute_max_out(self) -> int:
        """Max output length execute() can produce for the current state."""

    def reset(self) -> None:
        """Reset DopplerChannel to its post-create state.

        Zeroes both sample clocks (so `elapsed_s` and the carrier phase restart
        at 0) and clears the resampler's delay line and fractional accumulator.
        The configured `fs`/`carrier_hz`/`doppler_ppm`/`doppler_rate_ppm_s` are
        kept.
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def fs(self) -> float:
        """Fs."""

    @property
    def carrier_hz(self) -> float:
        """Carrier hz."""

    @property
    def doppler_ppm(self) -> float:
        """Doppler ppm."""

    @property
    def doppler_rate_ppm_s(self) -> float:
        """Doppler rate ppm s."""

    @property
    def elapsed_s(self) -> float:
        """Receive time in seconds consumed so far, the `t` every Doppler quantity is evaluated at. Advances by `n/fs` per `execute(x)` call and is zeroed by `reset()`."""

    @property
    def offset_hz(self) -> float:
        """Instantaneous carrier offset `fc * d(t)` in Hz at the current `elapsed_s` -- the frequency a receiver would have to tune out right now. Read-only diagnostic; with a non-zero `doppler_rate_ppm_s` it ramps as the stream advances."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "DopplerChannel": ...

    def __exit__(self, *args: object) -> None: ...
