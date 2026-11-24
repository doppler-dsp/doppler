# ccsds/ccsds.pyi — type stubs for the ccsds C extension.
import numpy as np
from numpy.typing import NDArray

def asm_bits() -> NDArray[np.uint8]:
    """The CCSDS Attached Sync Marker, 0x1ACFFC1D, as 32 unpacked bits —
    `out[0]` is the first bit on the wire (the top of 0x1A). Pass it to
    `doppler.detection.SyncFinder` to acquire a CADU in a bit stream; it is NOT
    randomised, so it reads the same in every frame and in exactly one
    polarity, which is what makes it the thing that reports a 180-degree
    carrier ambiguity.

    `out[0]` is the first bit on the wire — figure 9-1 of 131.0-B numbers
    the marker's bit 0 as the most significant bit of 0x1A. One bit per
    byte, the convention every frame path here passes around.

    The thing a Python receiver ACQUIRES on: pair it with
    `doppler.detection.SyncFinder` to find where a CADU starts in a bit
    stream, then slice and `Frame.check()` it. The marker is deliberately
    NOT randomised (10.4's NOTE: "The ASM was not randomized and is not
    derandomized"), so it reads the same in every frame and in exactly one
    polarity — which is what makes it the only thing in a CADU that can
    report a 180-degree carrier ambiguity.

    A function rather than a constant a caller expands, because an
    MSB-first expansion written out twice is a transcription that can
    disagree with itself. This tree's own doctests were the second copy
    until doppler#900, and this alias exists so the third copy is not a
    Python one: it delegates to `dp_ccsds_tm_asm_bits`, which is where the
    expansion is written and where `test_ccsds_tm_asm` holds it to the
    published pattern.

    Returns
    -------
    NDArray[np.uint8]
        Output.

    Examples
    --------
    >>> from doppler.ccsds import asm_bits
    >>> b = asm_bits()
    >>> b.size, b[:8].tolist()          # 0x1A, first bit at the top
    (32, [0, 0, 0, 1, 1, 0, 1, 0])
    >>> int("".join(map(str, b.tolist())), 2) == 0x1ACFFC1D
    True

    """

def ccsds415_chip_rate(fc: float) -> float:
    r"""Chips a second at carrier `fc` (Hz) at the CCSDS 415.1-B-1 point: F_fwd
    x 31 / (221 x 96), table 4-1. Computed in C with fast math off, so every
    compiler and this function give the same bits.

    >>> from doppler.ccsds import ccsds415_chip_rate
    >>> ccsds415_chip_rate(2.2e9)
    3214555.052790347

    An alias over dp_ccsds_415_chip_rate(), which owns the arithmetic; the
    point's other numbers and the scene helper are
    doppler/ccsds/ccsds_415.h.

    Parameters
    ----------
    fc : float
        The carrier, F_fwd, Hz.

    Returns
    -------
    float
        The chip rate, chips/s.

    Examples
    --------
    printf ("%.6f\n", dp_ccsds415_chip_rate (2.8e9));   // 4091251.885370

    """

def ccsds415_cn0(esn0_db: float) -> float:
    r"""C/N0 in dB-Hz of an Es/N0 (dB) at the CCSDS 415.1-B-1 point's 3,000
    symbols a second: `esn0_db + 10 log10(3000)`.

    >>> from doppler.ccsds import ccsds415_cn0
    >>> round(ccsds415_cn0(4.0), 4)
    38.7712

    Parameters
    ----------
    esn0_db : float
        Es/N0, dB.

    Returns
    -------
    float
        C/N0, dB-Hz.

    Examples
    --------
    printf ("%.4f\n", dp_ccsds415_cn0 (4.0));   // 38.7712

    """
