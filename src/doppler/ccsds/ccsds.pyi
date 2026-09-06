# ccsds/ccsds.pyi — type stubs for the ccsds C extension.
import numpy as np
from numpy.typing import NDArray
def asm_bits() -> NDArray[np.uint8]:
    """The CCSDS Attached Sync Marker, 0x1ACFFC1D, as 32 unpacked bits —
    `out[0]` is the first bit on the wire (the top of 0x1A). Pass it to
    `doppler.detection.SyncFinder` to acquire a CADU in a bit stream; it is
    NOT randomised, so it reads the same in every frame and in exactly one
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
    Python one: it delegates to `ccsds_tm_asm_bits`, which is where the
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
