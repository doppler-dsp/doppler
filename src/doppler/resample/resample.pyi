# resample/resample.pyi — type stubs for the resample C extension.
import numpy as np
from numpy.typing import NDArray

class Resampler:
    """Create a Resampler with the built-in 4096×19 Kaiser bank.

    Parameters
    ----------
    rate : float, default 0.0
        rate constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.resample import Resampler
    >>> obj = Resampler(0.0)

    """
    def __init__(self, rate: float = ...) -> None: ...

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Resample x(0..x_len-1) into out(0..n_out-1).

        out must be at least Resampler_execute_max_out() samples wide.

        Returns the number of output samples written.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def execute_ctrl(self, x: NDArray[np.complex64], ctrl: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Resample with per-sample rate deviations.

        rate_i = base_rate + crealf(ctrl(i)).  ctrl and x must be the

        same length.  Returns number of output samples written.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.
        ctrl : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def reset(self) -> None:
        """Zero delay line and phase accumulator.  Rate and bank preserved.
        """

    @property
    def rate(self) -> float:
        """Rate."""
    @rate.setter
    def rate(self, value: float) -> None: ...

    @property
    def num_phases(self) -> int:
        """Num phases."""

    @property
    def num_taps(self) -> int:
        """Num taps."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Resampler": ...

    def __exit__(self, *args: object) -> None: ...

class Halfbanddecimator:
    """Create a HalfbandDecimator.

    Parameters
    ----------
    h : NDArray[np.float32], default ...
        h constructor parameter.

    """
    def __init__(self, h: NDArray[np.float32] = ...) -> None: ...

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Decimate x(0..x_len-1) by 2 into out(0..n_out-1).

        out must be at least HalfbandDecimator_execute_max_out() samples.

        Returns actual output count (roughly x_len / 2).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def reset(self) -> None:
        """Zero delay lines.  Coefficients preserved.
        """

    @property
    def rate(self) -> float:
        """Always returns 0.5."""

    @property
    def num_taps(self) -> int:
        """Returns the FIR branch length passed to create."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Halfbanddecimator": ...

    def __exit__(self, *args: object) -> None: ...

class CIC:
    """Create a CIC decimation filter.

    Parameters
    ----------
    R : int, default 16
        R constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.resample import CIC
    >>> obj = CIC(16)

    """
    def __init__(self, R: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def reconfigure(self, R: int) -> None:
        """Change the decimation ratio in place; resets all filter state.

        Silently ignores invalid R (non-power-of-two, out of range).

        Parameters
        ----------
        R : int
            New decimation ratio.  Same constraints as cic_create().
        """

    def decimate(self, x: complex) -> NDArray[np.complex64]:
        """Decimate."""

    @property
    def R(self) -> int:
        """R."""

    @property
    def shift(self) -> int:
        """Shift."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "CIC": ...

    def __exit__(self, *args: object) -> None: ...

class RateConverter:
    """Create a rate converter for the given output/input rate ratio.

    Parameters
    ----------
    rate : float, default 1.0
        rate constructor parameter.
    compensate : int, default 0
        compensate constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.resample import RateConverter
    >>> obj = RateConverter(1.0, 0)

    """
    def __init__(self, rate: float = ..., compensate: int = ...) -> None: ...

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Convert n_in samples and write results to out.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Number of output samples written.
        """

    def reset(self) -> None:
        """Zero all sub-stage filter memories.

        Rate and stage structure are preserved.  Processing from a reset state

        produces the same output as a freshly created converter.
        """

    @property
    def rate(self) -> float:
        """Return the current rate ratio."""
    @rate.setter
    def rate(self, value: float) -> None: ...

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "RateConverter": ...

    def __exit__(self, *args: object) -> None: ...

def ciccompmf(N: int, R: int, M: int) -> NDArray[np.float64]:
    """Ciccompmf."""

def kaiser_beta(atten: float) -> float:
    """Kaiser beta."""

def kaiser_num_taps(num_phases: int, atten: float, pb: float, sb: float) -> int:
    """Kaiser num taps."""
