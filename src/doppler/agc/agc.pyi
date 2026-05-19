# agc/agc.pyi — type stubs for the agc C extension.
import numpy as np
from numpy.typing import NDArray

class AGC:
    """AGC component.

    Parameters
    ----------
    ref_db : float, default 0.0
        ref_db constructor parameter.
    loop_bw : float, default 0.0025
        loop_bw constructor parameter.
    alpha : float, default 0.05
        alpha constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.agc import AGC
    >>> obj = AGC(0.0, 0.0025, 0.05)

    """
    def __init__(self, ref_db: float = ..., loop_bw: float = ..., alpha: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def step(self, x: complex) -> complex:
        """Process one input sample."""

    def steps(self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Process a samples array."""

    @property
    def gain_db(self) -> float:
        """Gain (dB) the loop currently commands."""

    @property
    def applied_gain_db(self) -> float:
        """Gain (dB) actually applied to the most recently processed sample."""

    @property
    def ref_db(self) -> float:
        """Ref db."""
    @ref_db.setter
    def ref_db(self, value: float) -> None: ...

    @property
    def loop_bw(self) -> float:
        """Loop bw."""
    @loop_bw.setter
    def loop_bw(self, value: float) -> None: ...

    @property
    def alpha(self) -> float:
        """Alpha."""
    @alpha.setter
    def alpha(self, value: float) -> None: ...

    @property
    def decim(self) -> int:
        """steps() envelope decimation factor (default 8; use 8, 16 or 32)."""
    @decim.setter
    def decim(self, value: int) -> None: ...

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "AGC": ...

    def __exit__(self, *args: object) -> None: ...
