"""Convert an :class:`~doppler.dsss.Acquisition` hit into a
:class:`~doppler.track.Dll` seed.

``Acquisition.push()`` reports ``code_phase`` as a correlation *lag*: how
far the local code reference has to roll forward to align with the
capture. ``Dll(init_chip=...)`` wants the opposite quantity — the code's
own instantaneous phase at the hand-off instant. The two are related by a
sign inversion modulo the spreading factor, not by direct equality;
seeding ``Dll`` with the raw ``code_phase`` despreads to noise instead of
the intended symbol stream.
"""

__all__ = ["dll_init_chip_from_acq"]


def dll_init_chip_from_acq(code_phase: float, spc: int, sf: int) -> float:
    """Acquisition ``code_phase`` (a correlation lag) -> ``Dll`` ``init_chip``
    (the code's own phase).

    Parameters
    ----------
    code_phase : float
        ``code_phase`` from an :class:`~doppler.dsss.Acquisition` hit
        (samples, ``0 <= code_phase < sf * spc``).
    spc : int
        Samples per chip — the same ``spc`` the ``Acquisition`` and the
        downstream ``Dll`` are both built with.
    sf : int
        Chips per code period (the spreading factor) — the same ``sf``
        (``len(code)``) both objects share.

    Returns
    -------
    float
        The value to pass as ``Dll(..., init_chip=...)``, in chips,
        ``0 <= init_chip < sf``.

    Examples
    --------
    >>> from doppler.dsss.handoff import dll_init_chip_from_acq
    >>> dll_init_chip_from_acq(code_phase=0.0, spc=4, sf=1023)
    0.0
    >>> dll_init_chip_from_acq(code_phase=16.0, spc=4, sf=1023)  # 4-chip lag
    1019.0
    >>> dll_init_chip_from_acq(code_phase=4092.0, spc=4, sf=1023)  # full wrap
    0.0
    """
    return (sf - code_phase / spc) % sf
