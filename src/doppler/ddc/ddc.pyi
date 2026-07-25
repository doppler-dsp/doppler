# ddc/ddc.pyi — type stubs for the ddc C extension.
from typing import final, Literal
import numpy as np
from numpy.typing import NDArray

@final
class DDC:
    """Create a complex-input Digital Down-Converter.

    Parameters
    ----------
    norm_freq : float, default 0.0
        LO frequency in cycles/sample at the input rate. Set to -f_carrier to shift a carrier at f_carrier to DC.  Any real value is accepted.
    rate : float, default 0.25
        Output rate / input rate.  Must be > 0.  Values >= 1 are up-sampling; typical use is decimation (0 < rate < 1).  Rate-agnostic: a caller wanting `m` outputs per symbol asks for `rate = m/sps`; the cascade never learns about symbols.
    pulse : Literal["iandd", "rrc", "none"], default "none"
        Matched-filter pulse for the cascade's terminal stage: "rrc" (root-raised cosine, roll-off `beta`), "iandd" (unit rectangle one symbol wide -- what an integrate-and-dump computes), or "none" for a plain down-conversion with the default Kaiser anti-alias bank. Anything but "none" makes the chain mix, decimate and matched-filter in the same dot products, and makes that stage's polyphase arm the fractional timing delay `rate_ctrl` steers. CIC droop compensation is unconditional on this path (six taps per arm, worth 28 dB).
    beta : float, default 0.35
        RRC roll-off in `[0, 1]` (ignored for the rectangle and for RC_PULSE_NONE).
    span : int, default 8
        One-sided RRC span in symbols (ignored for the rectangle, whose support is exactly one symbol).
    pulse_sps : float, default 2.0
        The pulse's period in **output** samples (2 = two samples per symbol out).
    num_phases : int, default 1024
        Terminal-stage arms; a power of two.  Sets the timing resolution to `1/num_phases` of an output period.

    Examples
    --------
    Create with defaults:

    >>> from doppler.ddc import DDC
    >>> obj = DDC(norm_freq=0.0, rate=0.25, pulse="none", beta=0.35, span=8, pulse_sps=2.0, num_phases=1024)

    """
    def __init__(self, norm_freq: float = ..., rate: float = ..., pulse: Literal["iandd", "rrc", "none"] = "none", beta: float = ..., span: int = ..., pulse_sps: float = ..., num_phases: int = ...) -> None: ...

    def execute(self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Mix input block with LO, then rate-convert.

        Parameters
        ----------
        x : NDArray[np.complex64]
            CF32 input block; accepted as float32 (auto-cast).

        Returns
        -------
        NDArray[np.complex64]
            Number of output samples written (C-only).

        Examples
        --------
        >>> from doppler.ddc import DDC
        >>> import numpy as np
        >>> ddc = DDC(norm_freq=-0.1, rate=0.25)
        >>> t = np.arange(4096)
        >>> x = np.exp(1j * 2 * np.pi * 0.1 * t).astype(np.complex64)
        >>> y = ddc.execute(x)
        >>> y.shape
        (1024,)
        >>> y.dtype
        dtype('complex64')
        >>> round(float(abs(y[500])), 2)   # shifted to DC; amplitude ≈ 1
        1.0

        """

    def execute_max_out(self) -> int:
        """Max output length execute() can produce for the current state."""

    def execute_ctrl(self, x: NDArray[np.complex64], rate_ctrl: float, freq_ctrl: float) -> NDArray[np.complex64]:
        """Mix and resample a block, steering both control ports.

        The control-port form of ddc_execute(): the LO advances by `phase_inc +
        freq_ctrl` on every sample of this block, and the cascade's terminal
        stage runs at `stage_rate + rate_ctrl`. Neither deviation is persisted —
        the centre norm_freq and rate are untouched — so a tracking loop passes
        its full filter output on every call and the DDC holds no loop state of
        its own.

        Feeding a stream through ddc_execute_ctrl_push() one sample at a time
        reproduces this call bit-for-bit when both controls are held constant,
        so the cheap block form stays correct for open-loop use (a fixed Doppler
        offset, a rate trim) and the push form is what a closed loop uses.

        Parameters
        ----------
        x : NDArray[np.complex64]
            CF32 input block.
        rate_ctrl : float
            Rate deviation added to the terminal Resampler stage's rate. Referenced to the terminal (post-decimation) rate, not the overall rate; ignored by a plan whose last stage is an integer HB/CIC with nothing to steer.
        freq_ctrl : float
            Frequency deviation added to the LO, in cycles/sample at the INPUT rate (any sign).

        Returns
        -------
        NDArray[np.complex64]
            Number of output samples written.
        """

    def execute_ctrl_push(self, x: complex, rate_ctrl: float, freq_ctrl: float) -> NDArray[np.complex64]:
        """Push ONE input sample; emit whatever outputs it completes.

        The per-input streaming form of ddc_execute_ctrl(), and the only form a
        closed loop can use: a block call has to know its whole control history
        up front, whereas a carrier or timing loop computes each correction
        *from* the outputs already emitted. Both loops close once per symbol, so
        both ports need this form.

        The mix costs one LO step per input; the cascade then emits 0 outputs
        (the common decimating case, between strobes), 1, or several.

        Parameters
        ----------
        x : complex
            One CF32 input sample.
        rate_ctrl : float
            Rate deviation for this input (terminal-stage rate).
        freq_ctrl : float
            Frequency deviation for this input, cycles/sample at the input rate.

        Returns
        -------
        NDArray[np.complex64]
            Number of outputs written (0, 1, or more).
        """

    def reset(self) -> None:
        """Zero LO phase and filter history.

        Examples
        --------
        >>> from doppler.ddc import DDC
        >>> import numpy as np
        >>> ddc = DDC(norm_freq=0.0, rate=0.25)
        >>> x = np.ones(64, dtype=np.complex64)
        >>> y1 = ddc.execute(x)
        >>> ddc.reset()
        >>> y2 = ddc.execute(x)
        >>> bool(np.array_equal(y1, y2))
        True

        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def norm_freq(self) -> float:
        """Return the current LO normalised frequency (cycles/sample)."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def rate(self) -> float:
        """Return the configured output/input rate ratio (read-only). The rate is fixed at create time; change it by destroying and recreating the DDC with the new value."""

    @property
    def clipped(self) -> bool:
        """Has the cascade's CIC clipped its input since the last reset?"""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "DDC": ...

    def __exit__(self, *args: object) -> None: ...
