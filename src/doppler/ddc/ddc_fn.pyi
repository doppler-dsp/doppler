# ddc/ddc_fn.pyi — type stubs for the ddc_fn C extension.
#
# Hand-written, like the module itself (no_generate): jm only manages the
# CMake add_subdirectory for ddc_fn, so this stub is maintained by hand
# against ddc_fn_ext.c's PyMethodDef table.  The functional DDCR API holds its
# state in an opaque PyCapsule rather than a Python type.
from typing import Any

import numpy as np
from numpy.typing import NDArray

# Opaque DDCR state handle (a PyCapsule); created by ddcr_create, consumed by
# the other ddcr_* functions.  Treated as an unintrospectable token.
DDCRState = Any

def ddcr_create(norm_freq: float, rate: float) -> DDCRState:
    """Allocate a DDCR state handle.

    Parameters
    ----------
    norm_freq : float
        Fine NCO frequency at the intermediate rate (``fs_in / 2``).  To tune
        a real tone at ``f_carrier`` (normalised to ``fs_in``) to DC, use
        ``norm_freq = -(2 * f_carrier + 0.5)``.
    rate : float
        Total output / input rate.  Must be in ``(0, 0.5)``.

    Returns
    -------
    state : capsule
        Opaque handle.  Pass to :func:`ddcr_execute` / :func:`ddcr_reset` /
        :func:`ddcr_destroy`.  Released by the GC if :func:`ddcr_destroy` is
        never called.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.ddc import ddcr_create, ddcr_execute, ddcr_destroy
    >>> state = ddcr_create(0.0, 0.25)
    >>> x = np.zeros(64, dtype=np.float32)
    >>> out = np.empty(64, dtype=np.complex64)
    >>> y = ddcr_execute(state, x, out)
    >>> y.dtype
    dtype('complex64')
    >>> y.shape[0] <= 64
    True
    >>> ddcr_destroy(state)

    """
    ...

def ddcr_execute(
    state: DDCRState,
    x: NDArray[np.float32],
    out: NDArray[np.complex64],
) -> NDArray[np.complex64]:
    """Process a block of real float32 samples.

    Writes into the caller-supplied buffer and returns a zero-copy view of the
    filled slice (``out[:n_out]``).  The view lifetime is tied to ``out``, not
    to ``state``.

    Parameters
    ----------
    state : capsule
        Handle returned by :func:`ddcr_create`.
    x : ndarray[float32]
        Real input samples (C-contiguous).
    out : ndarray[complex64]
        Caller-owned, writable output buffer.  Capacity ``>= len(x)`` is
        always sufficient (DDCR is always decimating).

    Returns
    -------
    ndarray[complex64]
        Zero-copy view ``out[:n_out]``.
    """
    ...

def ddcr_reset(state: DDCRState) -> None:
    """Zero halfband, LO phase, and resampler history.

    Equivalent to destroy + create with the same parameters, but cheaper.
    """
    ...

def ddcr_destroy(state: DDCRState) -> None:
    """Release C resources immediately.

    The GC also frees correctly if this is never called.  Any further call on
    the same ``state`` after destroy raises ``RuntimeError``.  Live views of
    previous :func:`ddcr_execute` output remain valid because they reference
    the caller's buffer, not the state.
    """
    ...

def ddcr_get_norm_freq(state: DDCRState) -> float:
    """Return the current fine NCO normalised frequency (at ``fs_in / 2``)."""
    ...

def ddcr_set_norm_freq(state: DDCRState, norm_freq: float) -> None:
    """Retune the fine NCO without resetting halfband or resampler history.

    Phase-continuous across block boundaries.
    """
    ...

def ddcr_get_rate(state: DDCRState) -> float:
    """Return the total configured rate (``fs_out / fs_in``).

    Read-only; changing the rate requires destroy + re-create.
    """
    ...
