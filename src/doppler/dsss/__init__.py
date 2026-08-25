"""Direct-sequence spread-spectrum: the full chain from acquisition (Acquisition, BurstAcquisition) through despreading (Despreader, BurstDespreader) and polynomial-phase estimation to end-to-end receivers (DsssReceiver, AsyncDsssReceiver).

Examples
--------
>>> import numpy as np
>>> from doppler.dsss import Despreader
>>> rng = np.random.default_rng(0)
>>> code = rng.integers(0, 2, 31).astype(np.uint8)
>>> csign = np.where(code & 1, -1.0, 1.0)
>>> tx = np.repeat(np.tile(csign, 20), 2).astype(np.complex64)
>>> bool(Despreader(code, sps=2).steps(tx).size >= 19)
True"""

# dsss/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .dsss import BurstDespreader, Acquisition, PolynomialPhaseEstimator, BurstDemod, Despreader, DsssReceiver, BurstAcquisition, AsyncDsssReceiver, bin_to_signed, DsssBurstReceiver  # noqa: E402

__all__ = ["BurstDespreader", "Acquisition", "PolynomialPhaseEstimator", "BurstDemod", "Despreader", "DsssReceiver", "BurstAcquisition", "AsyncDsssReceiver", "bin_to_signed", "DsssBurstReceiver"]
