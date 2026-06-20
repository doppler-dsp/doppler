"""doppler.cvt.iq — CF32 → interleaved IQ16 ADC model.

ADCIQ wraps a single ADC instance and exploits the fact that a complex64
array is already laid out as interleaved float32 (I0 Q0 I1 Q1 …) in memory.
Passing the flat view to ADC.steps() quantises both channels in one SIMD
pass; the int64 output is then narrowed to int16.  The result is a standard
interleaved int16 IQ buffer compatible with USRP, RTL-SDR, and most SDR
hardware interfaces.

Restriction: bits must be ≤ 16 so the quantised values fit in int16.
For higher bit depths use two ADC objects directly.
"""

from __future__ import annotations

import numpy as np

from .cvt import ADC


class ADCIQ:
    """CF32 → interleaved IQ16 ADC model.

    Quantises a complex float32 stream through a single
    :class:`~doppler.cvt.ADC` instance operating on the flat I/Q interleaved
    representation.  Output is a contiguous int16 array with the standard
    interleaved layout ``I0 Q0 I1 Q1 …``.

    Parameters
    ----------
    bits : int
        ADC resolution, 1–16.  Values above 16 raise ``ValueError`` because
        the int16 output cannot represent them without truncation.
    dbfs : float
        Full-scale input level in dBFS.  A sinusoid at amplitude
        ``10**(dbfs/20)`` fills the ADC range exactly.
    dithering : int
        ``0`` = no dither; non-zero = TPDF dither before rounding.  The
        same dither stream is applied to both I and Q channels (correlated
        dither); use two separate :class:`ADC` instances if independent
        per-channel dither is required.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.cvt import ADCIQ
    >>> adc = ADCIQ(bits=12, dbfs=-10.0)
    >>> x = np.exp(
    ...     1j * np.linspace(
    ...         0, 2 * np.pi, 64, endpoint=False, dtype=np.float32
    ...     )
    ... ) * 10 ** (-10 / 20)
    >>> iq = adc.steps(x)
    >>> iq.dtype
    dtype('int16')
    >>> iq.shape
    (128,)
    >>> iq[0::2].dtype  # I channel
    dtype('int16')
    """

    def __init__(
        self,
        bits: int = 16,
        dbfs: float = -10.0,
        dithering: int = 0,
    ) -> None:
        if bits > 16:
            raise ValueError(
                f"ADCIQ output is int16; bits={bits} would silently truncate. "
                "Use two ADC instances for bits > 16."
            )
        self._adc = ADC(bits=bits, dbfs=dbfs, dithering=dithering)

    # ── properties ───────────────────────────────────────────────────────────

    @property
    def scale(self) -> float:
        """Scale factor: divide int64 ADC output by this to recover float."""
        return self._adc.scale

    @property
    def bits(self) -> int:
        """ADC resolution."""
        return self._adc.bits

    @property
    def clipped(self) -> bool:
        """True if any sample saturated since the last reset()."""
        return self._adc.clipped

    # ── lifecycle ────────────────────────────────────────────────────────────

    def reset(self) -> None:
        """Reset state: clear sticky clipped flag, re-seed dither PRNG."""
        self._adc.reset()

    def destroy(self) -> None:
        """Release the underlying ADC resources."""
        self._adc.destroy()

    def __enter__(self) -> ADCIQ:
        return self

    def __exit__(self, *args: object) -> None:
        self.destroy()

    # ── processing ───────────────────────────────────────────────────────────

    def step(self, x: complex) -> tuple[int, int]:
        """Quantise one complex sample.

        Processes the real part then the imaginary part through the shared
        ADC state; returns ``(I, Q)`` as a pair of Python ints (int16 range).

        Parameters
        ----------
        x : complex
            Input sample.

        Returns
        -------
        tuple[int, int]
            ``(I, Q)`` quantised values in ``[-(2**(bits-1)),
            2**(bits-1)-1]``.
        """
        s = complex(x)
        i = int(np.int16(self._adc.step(float(s.real))))
        q = int(np.int16(self._adc.step(float(s.imag))))
        return (i, q)

    def steps(self, x: np.ndarray) -> np.ndarray:
        """Quantise a block of complex samples.

        The complex64 array is reinterpreted as a flat float32 view
        (``I0 Q0 I1 Q1 …``) and passed to :meth:`ADC.steps` in a single
        SIMD call.  The int64 output is narrowed to int16 and returned.

        Parameters
        ----------
        x : array-like, complex64
            Input array of *N* complex samples.

        Returns
        -------
        np.ndarray, int16, shape (2N,)
            Interleaved IQ output: ``out[0::2]`` = I channel,
            ``out[1::2]`` = Q channel.
        """
        flat = np.asarray(x, dtype=np.complex64).view(np.float32)
        return self._adc.steps(flat).astype(np.int16)
