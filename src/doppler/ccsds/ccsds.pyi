# ccsds/ccsds.pyi — type stubs for the ccsds C extension.
from typing import final

import numpy as np
from numpy.typing import NDArray

@final
class Ccsds415Scene(tuple[float, float, float, float]):
    """A scene's derived numbers at the CCSDS 415.1-B-1 point, at one carrier:
    chip_rate (chips/s), ratio (the rate converter's, from the emitter's
    samples a chip to the capture), start_ppm and rate_ppm_s (the Doppler's
    start and rate, ppm of the carrier).

    Attributes
    ----------
    chip_rate : float
        Chips a second, dp_ccsds_415_chip_rate().
    ratio : float
        The rate converter's ratio from an emitter at `spc` samples a chip to the capture: DP_CCSDS_415_FS / (spc x chip_rate).
    start_ppm : float
        The Doppler at the first sample in ppm of the carrier, the unit dp_doppler_channel_create() takes: start_hz / fc x 1e6.
    rate_ppm_s : float
        R_env in ppm of the carrier a second: DP_CCSDS_415_R_ENV_HZ_S / fc x 1e6.
    """

    @property
    def chip_rate(self) -> float:
        """Chips a second, dp_ccsds_415_chip_rate()."""

    @property
    def ratio(self) -> float:
        """The rate converter's ratio from an emitter at `spc` samples a chip
        to the capture: DP_CCSDS_415_FS / (spc x chip_rate).
        """

    @property
    def start_ppm(self) -> float:
        """The Doppler at the first sample in ppm of the carrier, the unit
        dp_doppler_channel_create() takes: start_hz / fc x 1e6.
        """

    @property
    def rate_ppm_s(self) -> float:
        """R_env in ppm of the carrier a second: DP_CCSDS_415_R_ENV_HZ_S / fc x
        1e6.
        """

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

def ccsds415_scene(fc: float, spc: float, start_hz: float) -> Ccsds415Scene:
    r"""The numbers a scene at the CCSDS 415.1-B-1 point is built from, at
    carrier `fc` (Hz), for an emitter at `spc` samples a chip whose Doppler
    starts at `start_hz`: the chip rate, the rate converter's ratio to the 11
    MSa/s capture, and the Doppler's start and rate in ppm of the carrier (the
    unit `DopplerChannel` takes). Computed in C with fast math off, so this and
    `dp_ccsds_415_scene()` give the same bits.

    >>> from doppler.ccsds import ccsds415_scene
    >>> s = ccsds415_scene(2.2e9, 2.0, -13.75)
    >>> s.chip_rate, s.ratio
    (3214555.052790347, 1.7109677419354838)
    >>> s.start_ppm, s.rate_ppm_s
    (-0.0062499999999999995, 0.005)

    Parameters
    ----------
    fc : float
        The carrier, F_fwd, Hz.
    spc : float
        The emitter's samples a chip, > 0.
    start_hz : float
        The Doppler at the scene's first sample, Hz.

    Returns
    -------
    Ccsds415Scene
        The scene's numbers, a dp_ccsds_415_scene_t.

    Examples
    --------
    dp_ccsds_415_scene_t s = dp_ccsds415_scene (2.2e9, 2.0, -13.75);
    printf ("%.17g\n", s.ratio);   // 1.7109677419354838

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
