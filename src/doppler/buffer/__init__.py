"""Sample buffering: lock-free ring buffers for handing IQ blocks between producer and consumer stages.

F32Buffer (complex64), F64Buffer (complex128) and I16Buffer (a record of
two int16, i and q) are single-producer single-consumer rings whose
storage is mapped twice, so a read is always one contiguous zero-copy
window across the wrap. Any capacity from 1 up holds exactly that many
samples on every machine; only the address space behind it is rounded. A
producer thread writes and a consumer thread `wait`s; a caller that is its
own producer uses `write_some` and `peek` instead. The Ring Buffers guide
walks both.

Examples
--------
>>> import numpy as np
>>> from doppler.buffer import F32Buffer
>>> b = F32Buffer(16)
>>> b.write(np.ones(4, np.complex64))
True
>>> b.wait(4).shape
(4,)

"""

# buffer/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .buffer import F32Buffer, F64Buffer, I16Buffer  # noqa: E402

__all__ = ["F32Buffer", "F64Buffer", "I16Buffer"]
