"""doppler.ddc — Digital Down-Converter.

Wraps :c:type:`dp_ddc_t`.  Chains an NCO (frequency shift to DC) with
an optional DPMFS polyphase resampler, using built-in M=3 N=19
Kaiser-DPMFS coefficients — no filter design required.

Classes
-------
Ddc
    Wraps :c:type:`dp_ddc_t`.

Examples
--------
4× decimating DDC; shift a tone at +0.1·fs to baseband:

>>> import numpy as np
>>> from doppler.ddc import Ddc
>>> ddc = Ddc(-0.1, 4096, 0.25)
>>> x = np.zeros(4096, dtype=np.complex64)
>>> y = ddc.execute(x)
>>> y.dtype
dtype('complex64')
>>> len(y) <= ddc.max_out
True
"""

from ._ddc import Ddc

__all__ = ["Ddc"]
