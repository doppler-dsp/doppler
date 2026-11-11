"""Acquisition: the searches that find a signal before anything tracks it. The code-phase x Doppler search itself is doppler.correlation's Correlator (framing='continuous' or 'burst'); BurstCapture finds a burst of any repeated complex preamble (a PN code, Zadoff-Chu, a chirp or a QPSK sequence) and captures it, and PersistentBurstCapture keeps the capture's history in a file. CarrierAcquisition is a coarse frequency/phase search that seeds a carrier tracking loop. bin_to_signed is the Doppler-bin fold convention the searches and their hand-offs share.

BurstCapture stops at the burst's samples: demodulating is
`doppler.dsss.BurstDemod`'s job, and `DsssBurstReceiver` is the two
together. Reach for it directly when you want the bursts themselves -- a
recorder, an offline corpus, a second consumer fanned out from one stream.
Its look-back is nearly the whole checkpoint (16.84 MB for a 511-chip code,
5 repetitions and an 8029-symbol frame, all but 135 kB of it history), so
PersistentBurstCapture backs the ring with a file: the blob drops to the
engine's 135 kB and the history survives a restart.

CarrierAcquisition's two modes mirror the FrequencyAcquisition reference
design: `sequential` tests every block, otherwise it waits a fixed
`dwell_target`.

Examples
--------
>>> import numpy as np
>>> from doppler.acquire import CarrierAcquisition
>>> ca = CarrierAcquisition(sample_rate_hz=8000.0, symbol_rate_hz=1000.0,
...                          resolution_hz=5.0)
>>> ca.ready
False

`bin_to_signed` reads a Doppler bin the way numpy's `fftfreq` does, the
even grid's Nyquist bin included:

>>> from doppler.acquire import bin_to_signed
>>> [bin_to_signed(b, 8) for b in (0, 1, 4, 7)]
[0, 1, -4, -1]

"""

# acquire/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .acquire import (  # noqa: E402
    BurstCapture,
    CarrierAcquisition,
    PersistentBurstCapture,
    bin_to_signed,
)

__all__ = [
    "CarrierAcquisition",
    "BurstCapture",
    "PersistentBurstCapture",
    "bin_to_signed",
]
