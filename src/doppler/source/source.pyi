# source/source.pyi — type stubs for the source C extension.
import numpy as np
from numpy.typing import NDArray

class NCO:
    """Create an NCO instance.

    Parameters
    ----------
    norm_freq : float, default 0.0
        norm_freq constructor parameter.
    nmax : int, default 0
        nmax constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.source import NCO
    >>> obj = NCO(norm_freq=0.0, nmax=0)

    """
    def __init__(self, norm_freq: float = ..., nmax: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def steps_u32(self) -> NDArray[np.uint32]:
        """Advance n samples; write raw uint32 accumulator values.

        Output is emitted before increment: out(0) = current phase,

        out(1) = phase + phase_inc, etc.  Returns n.

        Returns
        -------
        NDArray[np.uint32]
            Output.
        """

    def steps_u32_scaled(self) -> NDArray[np.uint32]:
        """Advance n samples; values scaled to [0, nmax).

        Uses the branchless fixed-point identity:

        out(i) = (uint64_t)phase * nmax >> 32

        When nmax == 0 falls back to the raw accumulator.

        Returns
        -------
        NDArray[np.uint32]
            Output.
        """

    def steps_u32_ovf(self) -> tuple[NDArray[np.uint32], NDArray[np.uint8]]:
        """Advance n samples; write raw phase values and per-sample carry.

        out(i)  — raw 32-bit phase value (same as nco_steps_u32).

        out1(i) — 1 when the accumulator wrapped on sample i, 0 otherwise.

        The carry marks the boundary of one input period; useful for

        polyphase sample-clock generation.

        Returns
        -------
        tuple[NDArray[np.uint32], NDArray[np.uint8]]
            Output.
        """

    @property
    def norm_freq(self) -> float:
        """Norm freq."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def phase(self) -> int:
        """Phase."""
    @phase.setter
    def phase(self, value: int) -> None: ...

    @property
    def phase_inc(self) -> int:
        """Phase inc."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "NCO": ...

    def __exit__(self, *args: object) -> None: ...

class LO:
    """Create an LO instance.

    Parameters
    ----------
    norm_freq : float, default 0.0
        norm_freq constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.source import LO
    >>> obj = LO(norm_freq=0.0)

    """
    def __init__(self, norm_freq: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def steps(self) -> NDArray[np.complex64]:
        """Generate n CF32 phasors at the current norm_freq.

        Output is emitted before increment: out(0) corresponds to the phase

        at entry, out(1) to phase + phase_inc, etc.  Returns n.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def steps_ctrl(self, ctrl: NDArray[np.float32]) -> NDArray[np.complex64]:
        """Generate CF32 phasors with per-sample FM deviation.

        ctrl(i) (real float, fractional part used) is converted to a per-sample

        phase-increment delta added on top of the base phase_inc.  The base

        norm_freq is not modified.


        Output length equals ctrl_len.  Returns ctrl_len.

        Parameters
        ----------
        ctrl : NDArray[np.float32]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    @property
    def norm_freq(self) -> float:
        """Norm freq."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def phase(self) -> int:
        """Phase."""
    @phase.setter
    def phase(self, value: int) -> None: ...

    @property
    def phase_inc(self) -> int:
        """Phase inc."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "LO": ...

    def __exit__(self, *args: object) -> None: ...

class AWGN:
    """Create an AWGN generator.

    Parameters
    ----------
    seed : int, default 0
        seed constructor parameter.
    amplitude : float, default 1.0
        amplitude constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.source import AWGN
    >>> obj = AWGN(seed=0, amplitude=1.0)

    """
    def __init__(self, seed: int = ..., amplitude: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def generate(self) -> NDArray[np.complex64]:
        """Generate n complex CF32 AWGN samples.

        Returns
        -------
        NDArray[np.complex64]
            n (always; generate produces exactly n samples).
        """

    def reseed(self, seed: int) -> complex:
        """Reseed the RNG and reset state.

        Parameters
        ----------
        seed : int
            Input.

        Returns
        -------
        complex
            Output.
        """

    @property
    def amplitude(self) -> float:
        """Return the current amplitude (per-component std dev)."""
    @amplitude.setter
    def amplitude(self, value: float) -> None: ...

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "AWGN": ...

    def __exit__(self, *args: object) -> None: ...
