"""A cast array outlives the C call that reads it (doppler#1477).

A module function's binding converts its array argument to the C type with
``PyArray_FROM_OTF``. When the caller's array is already that type the result
is the caller's own array; when it is not -- a ``bool`` array handed to a
``uint8_t[]`` parameter -- it is a TEMPORARY holding the only reference. The
generated binding freed that temporary before calling the kernel
(just-makeit#1490), so the kernel read freed memory: an access violation one
run in three on Windows CI, and a silent wrong answer on Linux.

The fix is jm's (adopted at 0.87.0). This pins doppler's symptom so a future
regression reads as what it is. glibc's ``malloc.perturb`` fills freed memory
with a fixed nonzero byte, so a read after the free sees every flag set and
dates the lock at 0 -- deterministic, not a heap-layout lottery. The test
runs in a subprocess because the tunable must be set before the process
allocates anything.
"""

from __future__ import annotations

import os
import platform
import subprocess
import sys

import pytest

_PROBE = """
import numpy as np
from doppler.ber import ber_lock_symbol
flags = np.zeros(200_000, dtype=bool)
flags[150_000:] = True          # locks at 150000, sustained to the end
print(ber_lock_symbol(flags, 200, 0.9),
      ber_lock_symbol(flags.astype(np.uint8), 200, 0.9))
"""


@pytest.mark.skipif(
    platform.libc_ver()[0] != "glibc", reason="needs glibc's malloc.perturb"
)
def test_a_bool_array_is_read_before_its_cast_is_freed():
    env = {
        **os.environ,
        "GLIBC_TUNABLES": "glibc.malloc.perturb=165",
        "PYTHONMALLOC": "malloc",
    }
    r = subprocess.run(
        [sys.executable, "-c", _PROBE],
        env=env,
        capture_output=True,
        text=True,
        check=True,
    )
    cast, direct = r.stdout.split()
    assert direct == "150000", r.stdout
    assert cast == "150000", (
        f"bool input dated the lock at {cast}, uint8 at {direct}: the "
        "binding read its cast temporary after freeing it (doppler#1477)"
    )
