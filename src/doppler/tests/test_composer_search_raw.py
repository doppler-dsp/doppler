"""The four composers refuse a search grid the same way (#2752).

BurstCapture, DsssBurstReceiver, DsssReceiver and AsyncDsssReceiver forward
``configure_search_raw`` to the Correlator they embed. Each declares the
same two refusals in its manifest: ``DP_ERR_INVALID`` raises ``ValueError``
with a sentence naming every rule a grid must meet, and ``DP_ERR_MEMORY``
raises ``MemoryError`` with the engine keeping its prior grid. No message
carries a bare return code.

The out-of-memory case needs a grid larger than any memory the process
already holds, or malloc serves it without asking the kernel and the cap
never bites; a grid shrunk and grown again reuses what the shrink freed.
So the two burst composers are built with a wide grid, shrunk to (1, 1)
first, and grown by ``doppler_bins`` (up to ``reps``) past what their
construction held; the two continuous receivers (whose ``doppler_bins``
cannot exceed 1) are built at one look and grown by ``n_noncoh``, whose
non-coherent surface is sized by a 32767-chip code and never existed.
"""

import functools

import pytest

from doppler.tests._oom import linux_only, run_capped

_GRID = (
    "configure_search_raw: doppler_bins must lie in 1..reps, n_noncoh in "
    "1..DP_CORRELATOR_N_NONCOH_SAFETY_CEILING, and non-coherent looks "
    "(n_noncoh > 1) need rate_hypotheses 1"
)
#: The burst composers add the look-back refine can reach.
_BURST_GRID = (
    _GRID + "; and n_noncoh x doppler_bins must not exceed the code periods "
    "refine reaches back (k_lo)"
)
_MEMORY = (
    "configure_search_raw: rebuilding the engine's grid could not "
    "allocate; the engine keeps its prior grid"
)

_BURST_CAPTURE = """
import numpy as np
from doppler.acquire import BurstCapture
from doppler.acquire.tests._preamble import code_preamble
from doppler.wfm import PN

code = np.asarray(PN(poly=0, seed=1, length=10).generate(1023), np.uint8) & 1
obj = BurstCapture(code_preamble(code, 4), burst_len=1 << 16, reps=32,
                   fs=4e6)
"""
_DSSS_BURST_RECEIVER = """
import numpy as np
from doppler.dsss import DsssBurstReceiver
from doppler.wfm import Frame

rng = np.random.default_rng(0)
acq = rng.integers(0, 2, 1023).astype(np.uint8)
data = rng.integers(0, 2, 8).astype(np.uint8)
sync = rng.integers(0, 2, 13).astype(np.uint8)
frame = Frame(sync=sync, payload=np.zeros(32, np.uint8), crc="crc16")
obj = DsssBurstReceiver(acq_code=acq, data_code=data, frame=frame, reps=32,
                        spc=4, chip_rate=1.0e6, cn0_dbhz=50.0)
"""
_LONG_CODE = """
import numpy as np
from doppler.wfm import PN

code = np.asarray(PN(poly=0, seed=1, length=15).generate(32767), np.uint8) & 1
"""
_DSSS_RECEIVER = (
    _LONG_CODE
    + """
from doppler.dsss import DsssReceiver

obj = DsssReceiver(code, chip_rate=3.2767e6, symbol_rate=50.0,
                   sample_rate=6.5534e6)
"""
)
_ASYNC_DSSS_RECEIVER = (
    _LONG_CODE
    + """
from doppler.dsss import AsyncDsssReceiver

obj = AsyncDsssReceiver(code, chip_rate=3.2767e6, symbol_rate=50.0,
                        sample_rate=6.5534e6, doppler_uncertainty=500.0)
"""
)

_SHRINK = "\nobj.configure_search_raw(1, 1)\n"

#: (setup, the refusal sentence, what runs before the cap, the grid grown
#: under it).
COMPOSERS = {
    "BurstCapture": (_BURST_CAPTURE, _BURST_GRID, _SHRINK, (32, 1)),
    "DsssBurstReceiver": (_DSSS_BURST_RECEIVER, _BURST_GRID, _SHRINK, (32, 1)),
    "DsssReceiver": (_DSSS_RECEIVER, _GRID, "", (1, 256)),
    "AsyncDsssReceiver": (_ASYNC_DSSS_RECEIVER, _GRID, "", (1, 256)),
}


@functools.cache
def _build(name: str):
    """One object per composer for the in-process tests: a refused grid
    leaves it as it was, so the tests can share it."""
    ns: dict = {}
    exec(COMPOSERS[name][0], ns)
    return ns["obj"]


@pytest.mark.parametrize("name", COMPOSERS)
@pytest.mark.parametrize(
    "grid", [(0, 1), (1, 0), (1, 1 << 20), (1 << 20, 1)], ids=str
)
def test_a_bad_grid_raises_the_declared_sentence(name: str, grid) -> None:
    obj = _build(name)
    with pytest.raises(ValueError) as e:
        obj.configure_search_raw(*grid)
    assert str(e.value) == COMPOSERS[name][1]


@pytest.mark.parametrize("name", ["BurstCapture", "DsssBurstReceiver"])
def test_a_grid_past_the_look_back_is_refused(name: str) -> None:
    """Within the engine's own rules, but more periods than refine reaches
    back: the composer's own refusal, under the same sentence."""
    obj = _build(name)
    with pytest.raises(ValueError) as e:
        obj.configure_search_raw(32, 256)
    assert str(e.value) == _BURST_GRID


@linux_only
@pytest.mark.parametrize("name", COMPOSERS)
def test_a_grid_that_cannot_allocate_raises_memoryerror(name: str) -> None:
    setup, _, before, (bins, looks) = COMPOSERS[name]
    call = f"obj.configure_search_raw({bins}, {looks})"
    r = run_capped(
        setup + before,
        call,
        headroom=64 << 10,
        after=call + "\nprint('regrid after the cap')",
    )
    assert r.returncode == 0, r.stderr[-2000:]
    assert r.stdout.splitlines() == [
        f"MemoryError: {_MEMORY}",
        "regrid after the cap",
    ]
