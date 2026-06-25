# track/track.pyi — type stubs for the track C extension.
import numpy as np
from numpy.typing import NDArray

class LoopFilter:
    """LoopFilter component.

    Parameters
    ----------
    bn : float, default 0.01
        bn constructor parameter.
    zeta : float, default 0.707
        zeta constructor parameter.
    t : float, default 1.0
        t constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import LoopFilter
    >>> obj = LoopFilter(bn=0.01, zeta=0.707, t=1.0)

    """
    def __init__(self, bn: float = ..., zeta: float = ..., t: float = ...) -> None: ...

    def step(self, x: float) -> float:
        """Advance the loop one update with error @p x; return the control.

        `integ += ki*x; return integ + kp*x`.

        Parameters
        ----------
        x : float
            Loop error.

        Returns
        -------
        float
            Control value (integ + kp*x).
        """

    def steps(self, x: NDArray[np.float64], out: NDArray[np.float64] | None = None) -> NDArray[np.float64]:
        """Run a block of errors through the loop.

        Parameters
        ----------
        x : NDArray[np.float64]
            Input.

        Returns
        -------
        NDArray[np.float64]
            Output.
        """

    def configure(self, bn: float, zeta: float, t: float) -> None:
        """Recompute the loop gains for a new (bn, zeta, t); preserves the integrator.

        Parameters
        ----------
        bn : float
            Input.
        zeta : float
            Input.
        t : float
            Input.
        """

    def reset(self) -> None:
        """Zero the integrator; keep the configured gains.
        """

    @property
    def kp(self) -> float:
        """Kp."""

    @property
    def ki(self) -> float:
        """Ki."""

    @property
    def integ(self) -> float:
        """Integ."""
    @integ.setter
    def integ(self, value: float) -> None: ...

    @property
    def bn(self) -> float:
        """Bn."""

    @property
    def zeta(self) -> float:
        """Zeta."""

    @property
    def t(self) -> float:
        """T."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "LoopFilter": ...

    def __exit__(self, *args: object) -> None: ...
