"""doppler.nco — numerically-controlled oscillator.

Wraps :c:type:`dp_nco_t`.  The NCO generates complex or raw-phase
samples at a normalised frequency *f* (cycles per sample, range
``[0, 1)``).  A 2¹⁶-entry sine LUT gives ~96 dBc SFDR; phase
accumulates in a 32-bit unsigned integer.

Classes
-------
Nco
    Wraps :c:type:`dp_nco_t`.

Examples
--------
Basic complex tone generation:

>>> from doppler.nco import Nco
>>> osc = Nco(0.1)          # 0.1 cycles/sample → period 10 samples
>>> s = osc.execute_cf32(4)
>>> len(s)
4

Raw phase accumulator (useful for polyphase branch selection):

>>> osc = Nco(0.25)
>>> phases, overflows = osc.execute_u32_ovf(8)
>>> len(phases)
8
"""

from ._nco import Nco

__all__ = ["Nco"]
