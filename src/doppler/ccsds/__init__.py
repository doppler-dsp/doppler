"""CCSDS 131.0-B TM Synchronization and Channel Coding — the literals the standard picked, beside the general layer rather than inside it.

`asm_bits()` is the Attached Sync Marker as bits, which is what a receiver acquires on: pair it with `doppler.detection.SyncFinder` to find where a CADU starts in a bit stream. The standard's *transforms* are not here and are not coming here — describe a CADU with `doppler.wfm.FrameDesc` and the outer code, the randomiser and the inner code run through the general assembler.

So the rule for this module is narrow: a literal a mission copies out of the
Blue Book belongs here; a transform does not.

The marker is also the only thing in a CADU that can report a 180-degree
carrier ambiguity. It is not randomised (131.0-B 10.4: "The ASM was not
randomized and is not derandomized"), so it reads the same in every frame
and in exactly one polarity, while everything else in the frame can be
complemented and still pass: Reed-Solomon is linear and the all-ones vector
is itself a codeword, so a global flip lands on another codeword.
`SyncFinder`'s `inverted` is what sees it.

Examples
--------
>>> from doppler.ccsds import asm_bits
>>> b = asm_bits()
>>> int("".join(map(str, b.tolist())), 2) == 0x1ACFFC1D
True

It is what a receiver acquires on:

>>> import numpy as np
>>> from doppler.detection import SyncFinder
>>> stream = np.random.default_rng(1220).integers(0, 2, 500, dtype=np.uint8)
>>> stream[173 : 173 + b.size] = b
>>> f = SyncFinder(np.asarray(b))
>>> hit = f.find(stream, max_errors=f.max_errors_for(96, pfa=1e-3))
>>> (hit.found, hit.offset, hit.inverted)
(1, 173, 0)

"""

# ccsds/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .ccsds import asm_bits  # noqa: E402

__all__ = ["asm_bits"]
