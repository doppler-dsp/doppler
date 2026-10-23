"""AsyncDsssPool through the binding: construction and its refusals, the
status record, symbols, the threads mutator and reset.

The controller's rules (exclusions, ranks, duplicates, releases) and their
sabotages are the C suite's, ``native/tests/test_async_dsss_pool_core.c``;
the state round trip is in the shared matrix,
``src/doppler/tests/test_state_serialization.py``. This file checks what the
binding adds: the Python face reaches each of those C entry points.
"""

import numpy as np
import pytest

from doppler.dsss import AsyncDsssPool
from doppler.wfm import Gold

CODE = np.asarray(Gold().generate(1023)).astype(np.uint8)
SPEC = {
    "chip_rate": 3.069e6,
    "symbol_rate": 2700.0,
    "spc": 2,
    "doppler_uncertainty": 500.0,
}


def _pool(**kw):
    return AsyncDsssPool(CODE, **{**SPEC, "threads": 1, **kw})


def test_defaults():
    p = AsyncDsssPool(CODE, **SPEC)
    assert p.n_receivers == 3
    assert 1 <= p.threads <= 3
    assert p.samples == 0
    assert p.released_lost == 0 and p.released_duplicate == 0
    assert p.released_ghost == 0
    assert p.coherent_bins == 1


@pytest.mark.parametrize(
    "kw",
    [
        {"n_receivers": 0},
        {"n_receivers": 17},
        {"threads": -1},
        {"lost_confirm_s": -1.0},
    ],
)
def test_refuses(kw):
    with pytest.raises(ValueError, match="AsyncDsssPool"):
        _pool(**kw)


def test_push_status_symbols():
    p = _pool(n_receivers=2)
    assert p.push(np.zeros(2046, dtype=np.complex64)) == 0
    assert p.samples == 2046
    st = p.status(0)
    assert st.state == 0 and st.state_samples == 2046
    assert type(st).__name__ == "PoolReceiverStatus"
    assert p.status(2).state == -1  # past the pool
    assert p.symbols(0).size == 0
    assert p.symbols(5).size == 0


def test_threads_and_reset():
    p = _pool()
    p.set_threads(2)
    assert p.threads == 2
    with pytest.raises(ValueError, match="threads refused"):
        p.set_threads(-1)
    assert p.threads == 2
    p.push(np.zeros(1000, dtype=np.complex64))
    p.reset()
    assert p.samples == 0


def test_telemetry_one_worker():
    from doppler.telemetry import Telemetry

    p = _pool(n_receivers=2, threads=2)
    tlm = Telemetry(1 << 12)
    with pytest.raises(ValueError, match="set_telemetry failed"):
        p.set_telemetry(tlm, "p")
    p.set_threads(1)
    p.set_telemetry(tlm, "p")
    assert {"p.r0.state", "p.r1.state"} <= set(tlm.probe_names)
    with pytest.raises(ValueError, match="threads refused"):
        p.set_threads(2)
    # The pool keeps the context alive: drop ours and keep pushing.
    del tlm
    p.push(np.zeros(4092, dtype=np.complex64))
    p.set_telemetry(None, "p")
    p.set_threads(2)
    assert p.threads == 2
