"""Signal-to-noise estimation: data-aided and moment-based (M2M4) SNR / Es-N0 estimators for a recovered symbol stream.

Two stateless estimators over a block of complex baseband samples, shared
by every receiver that needs one rather than each growing its own formula.
Each has a sliding-window `_series` sibling for watching the estimate drift
along a stream instead of reading one block average.

- `snr_data_aided_db` knows the transmitted symbols: it strips the known
  sign, then Es/N0 = (mean signal amplitude)^2 / (mean residual power).
  It needs ground truth (or trusted decisions), and is unbiased,
  scale-invariant and polarity-invariant.
- `snr_m2m4_db` is blind: the second- and fourth-moment estimator of
  Pauluzzi & Beaulieu (IEEE Trans. Commun. 48(10), 2000) for a
  constant-modulus signal (BPSK, QPSK, M-PSK) in circular complex AWGN.

Examples
--------
>>> import numpy as np
>>> from doppler.snr import snr_m2m4_db
>>> rng = np.random.default_rng(0)
>>> sym = (2 * rng.integers(0, 2, 20000) - 1).astype(np.complex64)
>>> sym += 0.1 * (rng.standard_normal(20000)
...              + 1j * rng.standard_normal(20000)).astype(np.complex64)
>>> bool(15 < snr_m2m4_db(sym) < 25)
True

On one block the two agree to a few tenths of a dB, and only the
data-aided one needs the bits:

>>> from doppler.snr import snr_data_aided_db
>>> bits = (rng.random(5000) > 0.5).astype(np.uint8)
>>> sign = np.where(bits, -1.0, 1.0).astype(np.complex64)
>>> noise = (0.3 * (rng.standard_normal(5000)
...          + 1j * rng.standard_normal(5000))).astype(np.complex64)
>>> soft = (sign + noise).astype(np.complex64)
>>> round(float(snr_data_aided_db(soft, bits)), 1)
7.6
>>> round(float(snr_m2m4_db(soft)), 1)
7.4

"""

# snr/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .snr import (  # noqa: E402
    snr_data_aided_db,
    snr_data_aided_db_series,
    snr_m2m4_db,
    snr_m2m4_db_series,
)

__all__ = [
    "snr_data_aided_db",
    "snr_m2m4_db",
    "snr_data_aided_db_series",
    "snr_m2m4_db_series",
]
