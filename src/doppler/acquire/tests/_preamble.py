"""A PN code as preamble SAMPLES, for the burst objects' tests.

``BurstAcquisition``, ``BurstCapture`` and ``PersistentBurstCapture`` take
their preamble as its samples, one period at ``fs`` (doppler#1470). A PN
code is one such preamble, built the way a caller builds it: the chips
mapped by :func:`doppler.cvt.bin_to_nrz` (the library's one chip -> +-1
rule) and each held ``spc`` samples, at ``fs = chip_rate * spc``.

One builder, so a test cannot quietly grow its own mapping. The C tests'
twin is ``native/tests/dp_preamble_test.h``.

Examples
--------
>>> import numpy as np
>>> code_preamble(np.array([0, 1, 1], np.uint8), spc=2)
array([ 1.+0.j,  1.+0.j, -1.+0.j, -1.+0.j, -1.+0.j, -1.+0.j],
      dtype=complex64)
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import numpy as np

from doppler.cvt import bin_to_nrz

if TYPE_CHECKING:
    from numpy.typing import NDArray


def code_preamble(code: NDArray[np.uint8], spc: int) -> NDArray[np.complex64]:
    """One period of ``code`` as samples: ``bin_to_nrz``, held ``spc``.

    Parameters
    ----------
    code : NDArray[np.uint8]
        PN chips, 0/1 (any non-zero byte is a 1, as ``bin_to_nrz`` reads it).
    spc : int
        Samples per chip.

    Returns
    -------
    NDArray[np.complex64]
        ``len(code) * spc`` samples.
    """
    chips = np.asarray(code, dtype=np.uint8)
    nrz = np.zeros(chips.size, dtype=np.float32)
    bin_to_nrz(chips, nrz)
    return np.repeat(nrz, spc).astype(np.complex64)
