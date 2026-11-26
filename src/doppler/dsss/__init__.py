"""Direct-sequence spread-spectrum: despreading (Despreader, BurstDespreader) and end-to-end receivers (DsssReceiver, AsyncDsssReceiver, DsssBurstReceiver). The receivers compose the acquisition engines of doppler.acquire.

Examples
--------
>>> import numpy as np
>>> from doppler.dsss import Despreader
>>> rng = np.random.default_rng(0)
>>> code = rng.integers(0, 2, 31).astype(np.uint8)
>>> csign = np.where(code & 1, -1.0, 1.0)
>>> tx = np.repeat(np.tile(csign, 20), 2).astype(np.complex64)
>>> bool(Despreader(code, sps=2).steps(tx).size >= 19)
True

"""

# dsss/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .dsss import (  # noqa: E402
    ASYNC_DSSS_RX_STATE,
    AsyncDsssPool,
    AsyncDsssReceiver,
    BurstDemod,
    BurstDespreader,
    Ccsds415Pool,
    Ccsds415Receiver,
    Despreader,
    DsssBurstReceiver,
    DsssReceiver,
    PoolReceiverStatus,
    ReceiverStatus,
)
from .orchestrator import Acquirer, CoarseChannel, Detection  # noqa: E402

__all__ = [
    "BurstDespreader",
    "BurstDemod",
    "Despreader",
    "DsssReceiver",
    "AsyncDsssReceiver",
    "DsssBurstReceiver",
    "ReceiverStatus",
    "AsyncDsssPool",
    "PoolReceiverStatus",
    "ASYNC_DSSS_RX_STATE",
    "Acquirer",
    "CoarseChannel",
    "Detection",
    "Ccsds415Receiver",
    "Ccsds415Pool",
]
