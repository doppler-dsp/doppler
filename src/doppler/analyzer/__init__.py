"""Spectrum analysis: Specan renders a windowed, averaged power spectral density (dBFS) over a chosen span and resolution bandwidth, mirroring a hardware spectrum analyser's span / RBW / averaging controls.

Specan takes the instrument parameters an operator already knows -- center,
span, RBW and reference level -- instead of the DSP knobs under them
(transform length, Kaiser beta). It composes the DDC tuner/decimator and the
PSD averaging core, so the chain lives in C once and the `doppler.specan`
application is a display shell over it. Moving the center is a cheap
`retune` that keeps the filter history; a new span or RBW changes the
decimation rate and window, so it is a new Specan. Peaks are not computed in
the core: compose `doppler.spectral.find_peaks_f32` on the returned dB band.

Examples
--------
>>> import numpy as np
>>> from doppler.analyzer import Specan
>>> sp = Specan(fs=1.024e6, span=200e3, rbw=2e3, src_center=0.0,
...             center=0.0, offset_db=0.0, full_scale=1.0, bits=0,
...             navg=1)
>>> sp.execute(np.ones(4096, np.complex64)).shape
(401,)

"""

# analyzer/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .analyzer import Specan  # noqa: E402

__all__ = ["Specan"]
