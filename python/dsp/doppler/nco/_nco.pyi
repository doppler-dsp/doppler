"""Type stubs for the dp_nco C extension."""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

class Nco:
    """Numerically Controlled Oscillator wrapping dp_nco_t.

    Parameters
    ----------
    norm_freq:
        Normalised frequency f/fs (cycles per sample).
        Typical range [−0.5, 0.5).
    """

    def __init__(self, norm_freq: float) -> None: ...
    def get_freq(self) -> float:
        """Return the current normalised frequency (f/fs).

        Returns the value last passed to ``__init__`` or :meth:`set_freq`.
        """
        ...

    def get_phase(self) -> int:
        """Return the raw 32-bit phase accumulator value.

        Normalised: ``get_phase() / 2**32`` gives the fractional
        phase in [0, 1).  Useful for polyphase branch selection:
        ``get_phase() >> (32 - log2(num_phases))``.
        """
        ...

    def get_phase_inc(self) -> int:
        """Return the phase increment (fixed-point frequency).

        This is the uint32 added to the accumulator each sample:
        ``phase_inc = round(norm_freq × 2**32)``.

        Useful for polyphase branch selection::

            branch = phase >> (32 - log2(num_phases))

        where ``phase`` advances by ``get_phase_inc()`` each tick.
        """
        ...

    def set_freq(self, norm_freq: float) -> None:
        """Update the normalised frequency without resetting the phase."""
        ...

    def reset(self) -> None:
        """Reset the phase accumulator to zero."""
        ...

    # ------------------------------------------------------------------
    # Complex output
    # ------------------------------------------------------------------

    def execute_cf32(self, n: int = 1) -> NDArray[np.complex64]:
        """Generate *n* complex tone samples (float32 I/Q)."""
        ...

    def execute_cf32_ctrl(
        self, ctrl: NDArray[np.float32] | int
    ) -> NDArray[np.complex64]:
        """Generate complex tone samples with per-sample FM control.

        Parameters
        ----------
        ctrl:
            1-D float32 array of per-sample normalised-frequency
            deviations, **or** an integer *n* to generate *n* samples
            with zero control (equivalent to :meth:`execute_cf32`).

        Returns
        -------
        NDArray[np.complex64]
            Same length as *ctrl* (or *n* samples if int).
        """
        ...

    # ------------------------------------------------------------------
    # Raw phase accumulator output
    # ------------------------------------------------------------------

    def execute_u32(self, n: int = 1) -> NDArray[np.uint32]:
        """Return *n* raw 32-bit phase accumulator values."""
        ...

    def execute_u32_ctrl(self, ctrl: NDArray[np.float32] | int) -> NDArray[np.uint32]:
        """Raw phase accumulator values with per-sample FM control.

        Pass an integer *n* for zero control (free-running).
        """
        ...

    def execute_u32_ovf(
        self, n: int = 1
    ) -> tuple[NDArray[np.uint32], NDArray[np.uint8]]:
        """Phase accumulator values plus per-sample overflow flags.

        Returns
        -------
        phases:
            1-D uint32 array of length *n*.
        overflows:
            1-D uint8 array; element is 1 when the accumulator wrapped
            (i.e. one input period elapsed), 0 otherwise.
        """
        ...

    def execute_u32_ovf_ctrl(
        self, ctrl: NDArray[np.float32] | int
    ) -> tuple[NDArray[np.uint32], NDArray[np.uint8]]:
        """Phase + overflow with per-sample FM control.

        Pass an integer *n* for zero control (free-running).
        """
        ...

    def destroy(self) -> None:
        """Release the underlying C resource (also called on GC)."""
        ...

    def __enter__(self) -> "Nco": ...
    def __exit__(self, *args: object) -> None: ...
