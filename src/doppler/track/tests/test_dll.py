"""Integration tests for doppler.track.Dll (code-tracking loop)."""

import numpy as np
import pytest

from doppler.track import Dll

SF, SPS = 63, 4


def _code(seed=1):
    return np.random.default_rng(seed).integers(0, 2, SF).astype(np.uint8)


def _signal(code, delta, nper, seed=9):
    """Carrier-free spread signal at code rate (1+delta), BPSK data/period."""
    rng = np.random.default_rng(seed)
    n = SF * SPS * nper
    rx = np.empty(n, np.complex64)
    cph = 0.0
    for p in range(nper):
        data = 1 if rng.integers(0, 2) else -1
        for i in range(SF * SPS):
            idx = int(cph % SF)
            rx[p * SF * SPS + i] = data * (-1.0 if code[idx] & 1 else 1.0)
            cph += (1 + delta) / SPS
    return rx


def test_create_and_guard():
    assert Dll(_code(), SPS, 0.0, 0.01, 0.707, 0.5) is not None


def test_context_manager_and_destroy():
    with Dll(_code(), SPS, 0.0, 0.01, 0.707, 0.5):
        pass
    d = Dll(_code(), SPS, 0.0, 0.01, 0.707, 0.5)
    d.destroy()


def test_properties():
    d = Dll(_code(), SPS, 0.0, bn=0.01, zeta=0.707, spacing=0.5)
    assert d.bn == pytest.approx(0.01)
    assert d.code_rate == pytest.approx(1.0)
    d.bn = 0.005
    assert d.bn == pytest.approx(0.005)


def test_one_prompt_per_period():
    code = _code()
    rx = _signal(code, 0.0, 100)
    d = Dll(code, SPS, 0.0, 0.005, 0.707, 0.5)
    sym = d.steps(rx)
    assert sym.dtype == np.complex64
    assert len(sym) == 100


def test_tracks_code_doppler():
    code = _code(11)
    delta = 5e-4
    rx = _signal(code, delta, 1500, seed=9)
    d = Dll(code, SPS, 0.0, 0.005, 0.707, 0.5)
    d.steps(rx)
    # the replica speeds up to match the incoming chip rate
    assert d.code_rate == pytest.approx(1.0 + delta, abs=1e-4)
    assert abs(d.last_error) < 0.1


def test_pulls_in_static_offset():
    code = _code(13)
    rx = _signal(code, 0.0, 800, seed=17)
    d = Dll(code, SPS, 0.4, 0.005, 0.707, 0.5)  # 0.4-chip offset
    d.steps(rx[: SF * SPS * 3])
    assert abs(d.last_error) > 0.05  # starts misaligned
    d.steps(rx[SF * SPS * 3 :])
    assert abs(d.last_error) < 0.05  # pulled in


def test_reset_reproducible():
    code = _code(21)
    rx = _signal(code, 3e-4, 300, seed=5)
    d = Dll(code, SPS, 0.0, 0.005, 0.707, 0.5)
    d.steps(rx)
    r1, e1 = d.code_rate, d.last_error
    d.reset()
    d.steps(rx)
    assert (r1, e1) == (d.code_rate, d.last_error)
