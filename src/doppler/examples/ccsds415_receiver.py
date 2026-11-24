"""ccsds415_receiver.py -- the async DSSS receiver at the CCSDS 415.1-B-1
point, built in one line.

:class:`~doppler.dsss.Ccsds415Receiver` and
:class:`~doppler.dsss.Ccsds415Pool` are the receiver and the pool at the
specification point the receiver is designed for: 3,000 symbols a second
asynchronous to a 1,023-chip code whose chip rate is the carrier times
31/21216, 11 MSa/s, a +/-1,500 Hz search, a Doppler rate of R_env, the
code-only opening of each frame as the search's gap, and a carrier lock
designed at the Es/N0 4 dB floor. The point's numbers are the library's
(``doppler/ccsds/ccsds_415.h``); only the code (the mission's) and the
carrier are given.

What the run shows:

- **The view IS the full receiver.** At both ends of the band its state is
  byte for byte the :class:`~doppler.dsss.AsyncDsssReceiver` spelled out
  with the point's numbers, the chip rate from
  :func:`doppler.ccsds.ccsds415_chip_rate` and the floor's C/N0 from
  :func:`doppler.ccsds.ccsds415_cn0`.
- **The carrier sets the chip rate.** 2.2 GHz is 3.21 Mcps, 2.8 GHz 4.09.
- **A code that is not 1,023 chips is refused,** with a message saying so.
- **The pool is the same receiver, N times.**
"""

from __future__ import annotations

import numpy as np

from doppler.ccsds import ccsds415_chip_rate, ccsds415_cn0
from doppler.dsss import (
    AsyncDsssPool,
    AsyncDsssReceiver,
    Ccsds415Pool,
    Ccsds415Receiver,
)
from doppler.wfm import Gold

# --8<-- [start:views]
#: The mission's code: here the CCSDS Gold code, 1,023 chips.
CODE = np.asarray(Gold().generate(1023), np.uint8)

for fc in (2.2e9, 2.8e9):
    rx = Ccsds415Receiver(CODE, carrier_freq_hz=fc)
    pool = Ccsds415Pool(CODE, n_receivers=4, carrier_freq_hz=fc)
    print(
        f"{fc / 1e9:.1f} GHz: {ccsds415_chip_rate(fc) / 1e6:.4f} Mcps, "
        f"designed at C/N0 {ccsds415_cn0(4.0):.2f} dB-Hz, "
        f"{pool.n_receivers} receivers in the pool"
    )
# --8<-- [end:views]

# --8<-- [start:validate]
# The view is the full receiver with the point's numbers, byte for byte.
for fc in (2.2e9, 2.8e9):
    point = {
        "chip_rate": ccsds415_chip_rate(fc),
        "symbol_rate": 3000.0,
        "sample_rate": 11e6,
        "cn0_dbhz": ccsds415_cn0(4.0),
        "doppler_uncertainty": 1500.0,
        "carrier_freq_hz": fc,
        "code_only_s": 275 / 3000.0,
        "doppler_rate": 11.0,
    }
    full = AsyncDsssReceiver(CODE, **point)
    assert Ccsds415Receiver(CODE, carrier_freq_hz=fc).get_state() == (
        full.get_state()
    ), fc
    assert Ccsds415Pool(
        CODE, n_receivers=2, carrier_freq_hz=fc, threads=1
    ).get_state() == (
        AsyncDsssPool(CODE, n_receivers=2, threads=1, **point).get_state()
    ), fc

# The carrier sets the chip rate: F_fwd x 31 / 21216 (415.1-B-1 table 4-1).
assert abs(ccsds415_chip_rate(2.2e9) - 3214555.0528) < 1e-3
assert abs(ccsds415_chip_rate(2.8e9) - 4091251.8854) < 1e-3

# The point's code is 1,023 chips; any other length is refused.
try:
    Ccsds415Receiver(CODE[:511])
except ValueError as e:
    assert "1,023 chips" in str(e)
else:
    raise AssertionError("a 511-chip code was accepted")
print("OK")
# --8<-- [end:validate]
