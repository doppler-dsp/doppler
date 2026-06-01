# ddc/ddc.pyi — type stubs for the ddc C extension.
import numpy as np
from numpy.typing import NDArray
from typing import Any

class DDC:
    """DDC component.

    Parameters
    ----------
    norm_freq : float, default 0.0
        norm_freq constructor parameter.
    rate : float, default 0.25
        rate constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.ddc import DDC
    >>> obj = DDC(0.0, 0.25)

    """
    def __init__(self, norm_freq: float = ..., rate: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Execute."""

    @property
    def norm_freq(self) -> float:
        """Norm freq."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def rate(self) -> float:
        """Rate."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "DDC": ...

    def __exit__(self, *args: object) -> None: ...

class DDCR:
    """DDCR component.

    Parameters
    ----------
    norm_freq : float, default 0.0
        norm_freq constructor parameter.
    rate : float, default 0.25
        rate constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.ddc import DDCR
    >>> obj = DDCR(0.0, 0.25)

    """
    def __init__(self, norm_freq: float = ..., rate: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def execute(self, x: NDArray[np.float32]) -> NDArray[np.complex64]:
        """Execute."""

    @property
    def norm_freq(self) -> float:
        """Norm freq."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def rate(self) -> float:
        """Rate."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "DDCR": ...

    def __exit__(self, *args: object) -> None: ...

# ------------------------------------------------------------------ #
# Functional DDCR API — state passed explicitly                        #
# ------------------------------------------------------------------ #
#
# State is an opaque capsule allocated by ddcr_create.  The buffer for
# execute output is cached inside the capsule and grows on demand; it
# is freed by ddcr_destroy or the GC, whichever comes first.
#
# Usage:
#   state = ddcr_create(-0.7, 0.25)
#   y     = ddcr_execute(state, x)
#   ddcr_destroy(state)          # optional: GC also frees correctly

# Opaque state handle returned by ddcr_create.
DDCRState = Any

def ddcr_create(norm_freq: float, rate: float) -> DDCRState:
    """Allocate a DDCR state handle.

    Parameters
    ----------
    norm_freq : float
        Fine NCO frequency at the intermediate rate (fs_in/2).
    rate : float
        Total output/input rate.  Must be in (0, 0.5).

    Returns
    -------
    DDCRState
        Opaque capsule.  Pass to ddcr_execute / ddcr_reset / ddcr_destroy.
    """

def ddcr_execute(
    state: DDCRState,
    x: NDArray[np.float32],
    out: NDArray[np.complex64],
) -> NDArray[np.complex64]:
    """Write output into caller-supplied buffer; return a view of the filled slice.

    Parameters
    ----------
    state : DDCRState
        Handle returned by ddcr_create.
    x : NDArray[np.float32]
        Real input samples.
    out : NDArray[np.complex64]
        Caller-owned output buffer.  Must be writable complex64 with capacity
        >= len(x) (a decimating DDC never produces more samples than the input).

    Returns
    -------
    NDArray[np.complex64]
        Zero-copy view ``out[:n_out]``.  Valid as long as ``out`` is alive;
        the state handle has no ownership of ``out``.
    """

def ddcr_reset(state: DDCRState) -> None:
    """Zero halfband, LO phase, and resampler history."""

def ddcr_destroy(state: DDCRState) -> None:
    """Release C resources immediately.

    Further calls on the same state raise RuntimeError.
    The GC also frees correctly if destroy is never called.
    """

def ddcr_get_norm_freq(state: DDCRState) -> float:
    """Return the current fine NCO normalised frequency."""

def ddcr_set_norm_freq(state: DDCRState, norm_freq: float) -> None:
    """Retune the fine NCO without resetting state."""

def ddcr_get_rate(state: DDCRState) -> float:
    """Return the total configured rate (fs_out / fs_in)."""
