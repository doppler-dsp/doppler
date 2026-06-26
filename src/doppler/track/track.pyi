# track/track.pyi — type stubs for the track C extension.
from typing import Literal
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
            Loop noise bandwidth, normalized cycles/sample (>= 0).
        zeta : float
            Damping factor (typically 0.707).
        t : float
            Update period in samples (> 0).
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

class Costas:
    """Costas component.

    Parameters
    ----------
    bn : float, default 0.05
        bn constructor parameter.
    zeta : float, default 0.707
        zeta constructor parameter.
    init_norm_freq : float, default 0.0
        init_norm_freq constructor parameter.
    tsamps : int, default 64
        tsamps constructor parameter.
    bn_fll : float, default 0.0
        bn_fll constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import Costas
    >>> obj = Costas(bn=0.05, zeta=0.707, init_norm_freq=0.0, tsamps=64, bn_fll=0.0)

    """
    def __init__(self, bn: float = ..., zeta: float = ..., init_norm_freq: float = ..., tsamps: int = ..., bn_fll: float = ...) -> None: ...

    def steps(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """De-rotate a cf32 block with the integer-NCO carrier, coherently integrate over each tsamps-sample symbol, run the decision-directed Costas discriminator, and emit one complex prompt symbol per symbol.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def configure(self, bn: float, zeta: float) -> None:
        """Recompute the loop gains for a new (bn, zeta); preserves the frequency/phase estimate.

        Parameters
        ----------
        bn : float
            Input.
        zeta : float
            Input.
        """

    def reset(self) -> None:
        """Re-seed the loop to the create-time frequency/phase; preserve config.
        """

    @property
    def bn(self) -> float:
        """Bn."""
    @bn.setter
    def bn(self, value: float) -> None: ...

    @property
    def norm_freq(self) -> float:
        """Norm freq."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def lock_metric(self) -> float:
        """Lock metric."""

    @property
    def last_error(self) -> float:
        """Last error."""

    @property
    def bn_fll(self) -> float:
        """Bn fll."""
    @bn_fll.setter
    def bn_fll(self, value: float) -> None: ...

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Costas": ...

    def __exit__(self, *args: object) -> None: ...

class Dll:
    """Create a DLL instance (COPIES @p code).

    Parameters
    ----------
    code : NDArray[np.uint8], default ...
        code constructor parameter.
    sps : int, default 2
        sps constructor parameter.
    init_chip : float, default 0.0
        init_chip constructor parameter.
    bn : float, default 0.01
        bn constructor parameter.
    zeta : float, default 0.707
        zeta constructor parameter.
    spacing : float, default 0.5
        spacing constructor parameter.

    """
    def __init__(self, code: NDArray[np.uint8] = ..., sps: int = ..., init_chip: float = ..., bn: float = ..., zeta: float = ..., spacing: float = ...) -> None: ...

    def steps(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Correlate a carrier-wiped cf32 block against the local code with early/prompt/late taps, run the non-coherent (|E|-|L|)/(|E|+|L|) discriminator each code period, steer the code NCO, and emit one prompt symbol per period.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def configure(self, bn: float, zeta: float) -> None:
        """Recompute the loop gains for a new (bn, zeta); preserves the code phase/rate.

        Parameters
        ----------
        bn : float
            Input.
        zeta : float
            Input.
        """

    def reset(self) -> None:
        """Re-seed the loop to the create-time code phase; preserve config.
        """

    @property
    def bn(self) -> float:
        """Bn."""
    @bn.setter
    def bn(self, value: float) -> None: ...

    @property
    def code_phase(self) -> float:
        """Code phase."""

    @property
    def code_rate(self) -> float:
        """Code rate."""

    @property
    def last_error(self) -> float:
        """Last error."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Dll": ...

    def __exit__(self, *args: object) -> None: ...

class Channel:
    """Create a tracking channel (COPIES @p code).

    Parameters
    ----------
    code : NDArray[np.uint8], default ...
        code constructor parameter.
    sps : int, default 4
        sps constructor parameter.
    init_norm_freq : float, default 0.0
        init_norm_freq constructor parameter.
    init_chip : float, default 0.0
        init_chip constructor parameter.
    bn_carrier : float, default 0.05
        bn_carrier constructor parameter.
    bn_code : float, default 0.005
        bn_code constructor parameter.
    bn_fll : float, default 0.0
        bn_fll constructor parameter.
    zeta : float, default 0.707
        zeta constructor parameter.
    spacing : float, default 0.5
        spacing constructor parameter.
    nav_period : int, default 1
        nav_period constructor parameter.

    """
    def __init__(self, code: NDArray[np.uint8] = ..., sps: int = ..., init_norm_freq: float = ..., init_chip: float = ..., bn_carrier: float = ..., bn_code: float = ..., bn_fll: float = ..., zeta: float = ..., spacing: float = ..., nav_period: int = ...) -> None: ...

    def steps(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Track carrier + code and despread a cf32 block: per sample wipe the carrier (Costas) and correlate early/prompt/late against the code (DLL), update both loops each code period, and emit one complex prompt symbol per period.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def bits(self, x: NDArray[np.complex64]) -> NDArray[np.uint8]:
        """Same tracking kernel as steps(), but bit-sync the per-period prompts into hard data bits: nav_period prompts are coherently summed across each detected bit boundary and one 0/1 bit is emitted per data bit.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.uint8]
            Output.
        """

    def reset(self) -> None:
        """Re-seed both loops to the create-time frequency/phase; preserve config.
        """

    @property
    def norm_freq(self) -> float:
        """Norm freq."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def code_phase(self) -> float:
        """Code phase."""

    @property
    def code_rate(self) -> float:
        """Code rate."""

    @property
    def lock_metric(self) -> float:
        """Lock metric."""

    @property
    def bit_phase(self) -> int:
        """Bit phase."""

    @property
    def bn_carrier(self) -> float:
        """Bn carrier."""
    @bn_carrier.setter
    def bn_carrier(self, value: float) -> None: ...

    @property
    def bn_code(self) -> float:
        """Bn code."""
    @bn_code.setter
    def bn_code(self, value: float) -> None: ...

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Channel": ...

    def __exit__(self, *args: object) -> None: ...

class SymbolSync:
    """SymbolSync component.

    Parameters
    ----------
    sps : int, default 4
        sps constructor parameter.
    bn : float, default 0.01
        bn constructor parameter.
    zeta : float, default 0.707
        zeta constructor parameter.
    order : Literal["linear", "parabolic", "cubic"], default "cubic"
        order constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import SymbolSync
    >>> obj = SymbolSync(sps=4, bn=0.01, zeta=0.707, order="cubic")

    """
    def __init__(self, sps: int = ..., bn: float = ..., zeta: float = ..., order: Literal["linear", "parabolic", "cubic"] = "cubic") -> None: ...

    def steps(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Recover symbol timing from an oversampled cf32 baseband block: a Gardner timing-error detector drives an integer timing NCO whose post-wrap value gives the interpolation fraction for free, and a Farrow interpolator emits one symbol-rate sample per recovered symbol instant.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def configure(self, bn: float, zeta: float) -> None:
        """Recompute the loop gains for a new (bn, zeta); preserve the timing estimate.

        Parameters
        ----------
        bn : float
            Input.
        zeta : float
            Input.
        """

    def reset(self) -> None:
        """Re-seed the timing loop to its nominal rate and zero phase.
        """

    @property
    def bn(self) -> float:
        """Bn."""
    @bn.setter
    def bn(self, value: float) -> None: ...

    @property
    def timing_error(self) -> float:
        """Timing error."""

    @property
    def rate(self) -> float:
        """Rate."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "SymbolSync": ...

    def __exit__(self, *args: object) -> None: ...
