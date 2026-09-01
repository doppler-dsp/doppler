"""BurstCapture through its Python face.

The C suite (`native/tests/test_burst_capture_core.c`) owns the algorithm's
claims. What is asserted here is what only the BINDING can get wrong: that
the windows come back as an array a caller can index, that the event record's
fields land in the right columns, and that the state triplet round-trips
through `bytes`.
"""

import numpy as np
import pytest

from doppler.dsss import BurstCapture
from doppler.wfm import PN

ACQ_SF, DATA_SF, REPS, SPC = 31, 8, 4, 4
PAYLOAD_SYMS = 61
BURST_LEN = (REPS * ACQ_SF + PAYLOAD_SYMS * DATA_SF) * SPC
CHIP_RATE = 1.0e6


def acq_code() -> np.ndarray:
    """An m-sequence, not an arithmetic pattern.

    A code whose worst autocorrelation sidelobe is near its peak sets the CFAR
    reference from its own structure rather than from noise, which costs
    roughly half the burst offsets — measured. The library's own generator is
    the way to get one.
    """
    return (
        np.asarray(
            PN(poly=0, seed=1, length=5).generate(ACQ_SF), dtype=np.uint8
        )
        & 1
    )


def data_code() -> np.ndarray:
    return np.array([(i >> 1) & 1 for i in range(DATA_SF)], dtype=np.uint8)


def build_burst() -> np.ndarray:
    """REPS preamble repetitions, then a spread payload."""
    acode = 1.0 - 2.0 * acq_code().astype(np.float64)
    dcode = 1.0 - 2.0 * data_code().astype(np.float64)
    pre = np.repeat(np.tile(acode, REPS), SPC)
    syms = np.array(
        [1.0 if (j % 2) == 0 else -1.0 for j in range(PAYLOAD_SYMS)]
    )
    payload = np.repeat(np.outer(syms, dcode).ravel(), SPC)
    return np.concatenate([pre, payload]).astype(np.complex64)


def build_capture(n: int, at: list[int], sigma: float = 0.02, seed: int = 7):
    rng = np.random.default_rng(seed)
    x = (rng.normal(0.0, sigma, n) + 1j * rng.normal(0.0, sigma, n)).astype(
        np.complex64
    )
    burst = build_burst()
    for a in at:
        m = min(burst.size, n - a)
        x[a : a + m] += burst[:m]
    return x


def make() -> BurstCapture:
    return BurstCapture(
        acq_code(),
        burst_len=BURST_LEN,
        reps=REPS,
        spc=SPC,
        chip_rate=CHIP_RATE,
        cn0_dbhz=55.0,
    )


def test_geometry_is_derived_not_asked_for():
    """The look-back is not a constructor knob: a caller asked to size one is
    a caller handed a way to lose bursts silently. Both spans are read back
    instead, because duplicating `(k_lo+k_hi+reps)*code_period` in a caller is
    the drift this repo forbids."""
    cap = make()
    assert cap.burst_len == BURST_LEN
    assert cap.refine_span > 0
    assert cap.retain_span == cap.refine_span + BURST_LEN
    assert cap.doppler_res_hz == 0.0  # nothing captured yet


def test_a_bad_parameter_names_itself():
    """create_error turns the NULL into a ValueError naming the constraint,
    not the blanket MemoryError an unnamed failure would surface as."""
    with pytest.raises(ValueError):
        BurstCapture(acq_code(), burst_len=0, reps=REPS, spc=SPC)
    with pytest.raises(ValueError):
        BurstCapture(
            acq_code(), burst_len=BURST_LEN, reps=REPS, spc=SPC, pd=1.0
        )


def test_push_returns_windows_and_a_matching_event():
    at = 9000
    x = build_capture(80_000, [at])
    cap = make()
    win = cap.push(x)

    assert win.dtype == np.complex64
    assert win.size == BURST_LEN
    assert cap.preamble_start == at

    ev = cap.events()
    assert len(ev) == 1
    # A structured array, so fields are read by NAME. Field ORDER is what a
    # record binding gets wrong, and naming each one is what catches a
    # transposition the offsets would otherwise hide.
    assert ev["preamble_start"][0] == cap.preamble_start == at
    assert ev["doppler_res_hz"][0] == cap.doppler_res_hz > 0.0
    assert ev["cn0_dbhz_est"][0] == cap.cn0_dbhz_est
    assert ev["refine_margin"][0] == cap.refine_margin
    assert ev["doppler_hz_est"][0] == cap.doppler_hz_est
    # The margin's floor is (reps-1)/reps, never a constant: it RISES with
    # depth, so comparing against 0.9 at reps=16 would assert nothing.
    assert ev["refine_margin"][0] < (REPS - 1) / REPS + 0.1

    assert cap.dropped == 0
    assert cap.n_bursts == 1
    assert cap.pending == 0


def test_several_bursts_come_back_as_rows():
    at = [9000, 60_000, 120_000]
    x = build_capture(200_000, at)
    cap = make()
    win = cap.push(x)

    assert win.size == len(at) * BURST_LEN
    ev = cap.events()
    assert list(ev["preamble_start"]) == at
    # Row i of the return IS burst i: the concatenation is the contract, so a
    # caller slicing by burst_len must land on the window the event describes.
    for i in range(len(at)):
        assert win[i * BURST_LEN : (i + 1) * BURST_LEN].size == BURST_LEN


def test_reset_keeps_working():
    """reset() returns to searching, and the SAME stream is found again.

    The assertion the C receiver's own reset test stopped one step short of —
    the ring's head/tail are absolute counters, so a reset that emptied it
    without rewinding left every later position unreachable (doppler#1169)."""
    x = build_capture(80_000, [9000])
    cap = make()
    assert cap.push(x).size == BURST_LEN

    cap.reset()
    assert cap.pending == 0
    assert cap.preamble_start == 0
    assert cap.n_bursts == 1  # lifetime, survives reset

    assert cap.push(x).size == BURST_LEN
    assert cap.preamble_start == 9000
    assert cap.dropped == 0


def test_state_round_trips_through_bytes():
    at = 60_000
    x = build_capture(200_000, [at])
    cut = at + 2 * ACQ_SF * SPC  # inside the preamble: nothing emitted yet

    a = make()
    assert a.push(x[:cut]).size == 0

    b = make()
    blob = a.get_state()
    assert isinstance(blob, bytes)
    assert len(blob) == a.state_bytes()
    b.set_state(blob)

    # The restored capture finds the burst `a` was still holding, which is
    # what proves the retained look-back travelled in the blob.
    assert b.push(x[cut:]).size == BURST_LEN
    assert b.preamble_start == at


def test_state_rejects_a_blob_it_did_not_write():
    cap = make()
    blob = cap.get_state()
    with pytest.raises(ValueError):
        cap.set_state(bytes(len(blob)))
    with pytest.raises(ValueError):
        cap.set_state(blob[:-1])
    with pytest.raises(TypeError):
        cap.set_state("not bytes")


def test_context_manager_releases_it():
    with make() as cap:
        assert cap.burst_len == BURST_LEN
