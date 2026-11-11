"""Propagation impairments: what transforms a signal between transmitter and receiver, as distinct from doppler.source, which generates one.

`DopplerChannel` applies Doppler as physics rather than as a frequency
offset. Relative motion rescales the whole received time base, so it
resamples the stream by 1/(1+d) -- every clock in the signal, carrier,
chip, symbol and frame, moves together -- and turns the same dilation into
the carrier offset, both read from one integral so they cannot drift
apart. Doppler is given in ppm of the nominal time base: at a 2.5 GHz
carrier and 3.069 Mcps, 20 ppm is both +50 kHz and +61.4 chips/s.
`carrier_hz` is DSP input here, not metadata: it is the only thing that
turns ppm into Hz, and leaving it 0 dilates the clocks but leaves the
carrier still. Noise is `doppler.source.AWGN`'s, not this module's.

Examples
--------
>>> import numpy as np
>>> from doppler.impairment import DopplerChannel
>>> ch = DopplerChannel(fs=1e6, carrier_hz=1e5, doppler_ppm=10.0)
>>> bool(ch.execute(np.ones(64, np.complex64)).size > 0)
True

Fewer samples come out than go in -- the missing ones are the dilation --
and blocks fed one after another match one large call:

>>> x = np.ones(65536, np.complex64)   # the most one call takes
>>> ch = DopplerChannel(fs=6.138e6, carrier_hz=2.5e9, doppler_ppm=20.0,
...                     doppler_rate_ppm_s=0.2)
>>> whole = ch.execute(x)
>>> len(whole) < len(x)
True
>>> ch.reset()
>>> parts = np.concatenate([ch.execute(x[i:i + 4096])
...                         for i in range(0, len(x), 4096)])
>>> bool(np.allclose(parts, whole))
True

"""

# impairment/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .impairment import DopplerChannel  # noqa: E402

__all__ = ["DopplerChannel"]
