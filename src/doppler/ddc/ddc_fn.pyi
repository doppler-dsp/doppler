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
    """Allocate and initialise a DDCR down-converter state handle.

    The DDCR (real-input Digital Down-Converter) implements the chain::

        float in (fs_in)
          → halfband R2C decimator (2:1, with an embedded fs/4 shift)
          → fine NCO mixer at the intermediate rate (fs_in/2)
          → RateConverter (polyphase + optional CIC + halfband)
          → complex64 out (fs_out)

    RateConverter selects the cheapest cascade for the requested rate at
    create time.  The handle is an opaque PyCapsule; all subsequent
    operations take it as their first argument.

    Parameters
    ----------
    norm_freq : float
        Fine NCO frequency at the *intermediate* rate (``fs_in / 2``).
        The halfband R2C stage applies a fixed ``+fs/4`` shift before
        the NCO, so to park a real tone at ``f_carrier`` (normalised to
        ``fs_in``) at DC, use::

            norm_freq = -(2 * f_carrier + 0.5)

        For example, a carrier at 0.1·fs_in requires
        ``norm_freq = -0.7``.
    rate : float
        Total output-to-input rate ratio (``fs_out / fs_in``).  Must
        be in the open interval ``(0, 0.5)`` — DDCR always decimates.

    Returns
    -------
    state : capsule
        Opaque handle wrapping the C ``ddcr_state_t``.  Pass to
        :func:`ddcr_execute`, :func:`ddcr_reset`, :func:`ddcr_destroy`,
        :func:`ddcr_get_norm_freq`, :func:`ddcr_set_norm_freq`, and
        :func:`ddcr_get_rate`.  If :func:`ddcr_destroy` is never called,
        the GC releases the C resources when the capsule is collected.

    Raises
    ------
    MemoryError
        If the underlying ``ddcr_create`` C call returns ``NULL`` (OOM
        or invalid arguments).

    Examples
    --------
    Create a DDCR that tunes a real carrier at 0.1·fs to DC and
    decimates by 4 (rate = 0.25):

    >>> import numpy as np
    >>> from doppler.ddc import ddcr_create, ddcr_execute, ddcr_destroy
    >>> f_carrier = 0.1
    >>> norm_freq = -(2 * f_carrier + 0.5)   # -0.7
    >>> state = ddcr_create(norm_freq, 0.25)
    >>> x = np.zeros(64, dtype=np.float32)
    >>> out = np.empty(64, dtype=np.complex64)
    >>> y = ddcr_execute(state, x, out)
    >>> y.dtype
    dtype('complex64')
    >>> y.shape[0]
    16
    >>> ddcr_destroy(state)

    """
    ...

def ddcr_execute(
    state: DDCRState,
    x: NDArray[np.float32],
    out: NDArray[np.complex64],
) -> NDArray[np.complex64]:
    """Process a block of real float32 samples through the DDCR.

    Applies the full DDCR chain (halfband R2C → NCO → RateConverter) to
    the input block and writes the complex baseband output into the
    caller-supplied buffer ``out``.  Returns a zero-copy view
    ``out[:n_out]`` — the view lifetime is tied to ``out``, not to
    ``state``.

    The GIL is released across the pure-C kernel.  A thread-per-shard
    worker — each thread owning its own capsule and ``out`` buffer —
    therefore scales across CPU cores without serialising on the GIL.
    Safety relies on the one-capsule-per-stream contract: the kernel
    touches only this stream's state and the caller-owned buffers, never
    any Python object or shared mutable state.

    Processing is **phase-continuous** across calls: feeding a signal as
    successive blocks through the *same* state is bit-identical to
    processing the entire signal in one shot, because the halfband
    history, LO phase, and resampler banks are all preserved in the
    capsule between calls.

    Parameters
    ----------
    state : capsule
        Handle returned by :func:`ddcr_create`.  Must not have been
        passed to :func:`ddcr_destroy`.
    x : ndarray[float32]
        Real input samples, C-contiguous.  Any length; the output
        count scales by ``rate`` (plus any residual in the filter
        history).
    out : ndarray[complex64]
        Caller-owned, writable, C-contiguous output buffer.  A
        capacity of ``len(x)`` elements is always sufficient because
        the DDCR always decimates (``rate < 0.5``).  The buffer may
        be reused across calls — nothing is cached inside the capsule.

    Returns
    -------
    ndarray[complex64]
        Zero-copy view ``out[:n_out]``.  The data are owned by
        ``out``; the view remains valid as long as ``out`` is alive,
        regardless of the state of ``state``.

    Raises
    ------
    RuntimeError
        If ``state`` has already been passed to :func:`ddcr_destroy`.
    TypeError
        If ``out`` is not a writable ``ndarray[complex64]``.

    Examples
    --------
    Tune a cosine at 0.1·fs to DC and decimate 4×; confirm the peak
    of the output spectrum lands at bin 0 (DC):

    >>> import numpy as np
    >>> from doppler.ddc import ddcr_create, ddcr_execute, ddcr_destroy
    >>> N = 4096
    >>> f_carrier = 0.1
    >>> state = ddcr_create(-(2 * f_carrier + 0.5), 0.25)
    >>> t = np.arange(N, dtype=np.float32)
    >>> x = np.cos(2 * np.pi * f_carrier * t)
    >>> out = np.empty(N, dtype=np.complex64)
    >>> y = ddcr_execute(state, x, out)
    >>> y.shape[0]           # output length = N * rate = 1024
    1024
    >>> np.shares_memory(y, out)    # zero-copy view of out[:1024]
    True
    >>> spectrum = np.abs(np.fft.fft(y))
    >>> int(np.argmax(spectrum))    # peak at bin 0 (DC)
    0
    >>> ddcr_destroy(state)

    Phase continuity — two halves equal one shot:

    >>> state = ddcr_create(-(2 * f_carrier + 0.5), 0.25)
    >>> y_whole = ddcr_execute(state, x, out).copy()
    >>> ddcr_destroy(state)
    >>> state = ddcr_create(-(2 * f_carrier + 0.5), 0.25)
    >>> y0 = ddcr_execute(state, x[:N//2], out).copy()
    >>> y1 = ddcr_execute(state, x[N//2:], out).copy()
    >>> ddcr_destroy(state)
    >>> float(np.max(np.abs(np.concatenate([y0, y1]) - y_whole)))
    0.0

    """
    ...

def ddcr_reset(state: DDCRState) -> None:
    """Zero all DDCR filter history without freeing or recreating the state.

    Resets the halfband R2C decimator taps, the LO phase accumulator,
    and the RateConverter (polyphase / CIC / halfband) history buffers
    to zero.  The configured ``norm_freq`` and ``rate`` are preserved.
    Equivalent to ``ddcr_destroy`` + ``ddcr_create`` with the same
    parameters, but cheaper — no allocation or deallocation occurs.

    Use this before processing a new, independent signal segment through
    an existing state when filter transients from the previous segment
    must not bleed into the new one.

    Parameters
    ----------
    state : capsule
        Handle returned by :func:`ddcr_create`.  Must not have been
        passed to :func:`ddcr_destroy`.

    Raises
    ------
    RuntimeError
        If ``state`` has already been passed to :func:`ddcr_destroy`.

    Examples
    --------
    Processing the same block twice through a reset state produces
    identical output each time:

    >>> import numpy as np
    >>> from doppler.ddc import ddcr_create, ddcr_execute, ddcr_reset, ddcr_destroy
    >>> state = ddcr_create(-0.7, 0.25)
    >>> x = np.ones(64, dtype=np.float32)
    >>> out = np.empty(64, dtype=np.complex64)
    >>> y1 = ddcr_execute(state, x, out).copy()
    >>> ddcr_reset(state)
    >>> y2 = ddcr_execute(state, x, out).copy()
    >>> bool(np.array_equal(y1, y2))
    True
    >>> ddcr_destroy(state)

    """
    ...

def ddcr_destroy(state: DDCRState) -> None:
    """Release DDCR C resources immediately, without waiting for the GC.

    Frees the ``ddcr_state_t`` struct and all heap members (halfband
    taps, polyphase banks, CIC state, etc.) allocated by
    :func:`ddcr_create`.  After this call the capsule is marked
    *destroyed*: any subsequent call that accepts it as ``state`` raises
    ``RuntimeError``.

    If ``ddcr_destroy`` is never called, the GC will also free the
    resources correctly when the capsule is collected — the capsule
    destructor is registered at creation time.  Explicit destruction is
    preferred in long-running processes or when managing many streams,
    because it releases memory deterministically rather than waiting for
    a collection cycle.

    Live views returned by earlier :func:`ddcr_execute` calls remain
    valid after this call, because they reference the caller-supplied
    ``out`` buffer, not the state.

    Parameters
    ----------
    state : capsule
        Handle returned by :func:`ddcr_create`.  Must not have been
        passed to :func:`ddcr_destroy` already.

    Raises
    ------
    RuntimeError
        If ``state`` has already been destroyed.

    Examples
    --------
    Explicit destruction is safe; subsequent calls raise RuntimeError:

    >>> import numpy as np
    >>> from doppler.ddc import ddcr_create, ddcr_destroy, ddcr_get_rate
    >>> state = ddcr_create(-0.7, 0.25)
    >>> ddcr_destroy(state)
    >>> try:
    ...     ddcr_get_rate(state)
    ... except RuntimeError as exc:
    ...     print(exc)
    ddcr_state has already been destroyed

    Output views outlive the state:

    >>> state = ddcr_create(-0.7, 0.25)
    >>> x = np.zeros(64, dtype=np.float32)
    >>> out = np.empty(64, dtype=np.complex64)
    >>> from doppler.ddc import ddcr_execute
    >>> y = ddcr_execute(state, x, out).copy()  # copy owns its data
    >>> ddcr_destroy(state)
    >>> y.shape[0]   # view data still accessible
    16

    """
    ...

def ddcr_get_norm_freq(state: DDCRState) -> float:
    """Return the current fine NCO normalised frequency.

    The returned value is the NCO frequency at the *intermediate* rate
    (``fs_in / 2``), as set by :func:`ddcr_create` or most recently
    updated by :func:`ddcr_set_norm_freq`.  Calling :func:`ddcr_reset`
    does not alter this value.

    Parameters
    ----------
    state : capsule
        Handle returned by :func:`ddcr_create`.  Must not have been
        passed to :func:`ddcr_destroy`.

    Returns
    -------
    norm_freq : float
        Current NCO frequency in cycles/sample at the intermediate rate.

    Raises
    ------
    RuntimeError
        If ``state`` has already been passed to :func:`ddcr_destroy`.

    Examples
    --------
    >>> from doppler.ddc import ddcr_create, ddcr_get_norm_freq, ddcr_destroy
    >>> state = ddcr_create(-0.7, 0.25)
    >>> ddcr_get_norm_freq(state)
    -0.7
    >>> ddcr_destroy(state)

    The value persists through :func:`ddcr_reset`:

    >>> from doppler.ddc import ddcr_reset
    >>> state = ddcr_create(-0.7, 0.25)
    >>> ddcr_reset(state)
    >>> ddcr_get_norm_freq(state)
    -0.7
    >>> ddcr_destroy(state)

    """
    ...

def ddcr_set_norm_freq(state: DDCRState, norm_freq: float) -> None:
    """Retune the fine NCO frequency without disturbing filter history.

    Updates the LO phase increment so subsequent :func:`ddcr_execute`
    calls use the new ``norm_freq``.  The halfband R2C, polyphase, CIC,
    and halfband decimator histories are untouched — processing resumes
    phase-continuously from the current state, with no filter transient
    at the retune boundary.

    To shift a real carrier at ``f_new`` (normalised to ``fs_in``) to
    DC after retuning::

        ddcr_set_norm_freq(state, -(2 * f_new + 0.5))

    Parameters
    ----------
    state : capsule
        Handle returned by :func:`ddcr_create`.  Must not have been
        passed to :func:`ddcr_destroy`.
    norm_freq : float
        New fine NCO frequency in cycles/sample at the intermediate
        rate (``fs_in / 2``).

    Raises
    ------
    RuntimeError
        If ``state`` has already been passed to :func:`ddcr_destroy`.

    Examples
    --------
    Retune from carrier 0.18·fs to 0.14·fs:

    >>> from doppler.ddc import (
    ...     ddcr_create, ddcr_set_norm_freq,
    ...     ddcr_get_norm_freq, ddcr_destroy,
    ... )
    >>> state = ddcr_create(-(2 * 0.18 + 0.5), 0.25)
    >>> ddcr_get_norm_freq(state)
    -0.86
    >>> ddcr_set_norm_freq(state, -(2 * 0.14 + 0.5))
    >>> ddcr_get_norm_freq(state)
    -0.78
    >>> ddcr_destroy(state)

    """
    ...

def ddcr_get_rate(state: DDCRState) -> float:
    """Return the total configured output-to-input rate ratio.

    Returns ``fs_out / fs_in`` exactly as passed to :func:`ddcr_create`.
    The rate is read-only; changing it requires
    :func:`ddcr_destroy` + :func:`ddcr_create` with a new value.

    Parameters
    ----------
    state : capsule
        Handle returned by :func:`ddcr_create`.  Must not have been
        passed to :func:`ddcr_destroy`.

    Returns
    -------
    rate : float
        Configured ``fs_out / fs_in`` ratio.  Always in ``(0, 0.5)``.

    Raises
    ------
    RuntimeError
        If ``state`` has already been passed to :func:`ddcr_destroy`.

    Examples
    --------
    >>> from doppler.ddc import ddcr_create, ddcr_get_rate, ddcr_destroy
    >>> state = ddcr_create(-0.7, 0.25)
    >>> ddcr_get_rate(state)
    0.25
    >>> ddcr_destroy(state)

    """
    ...
