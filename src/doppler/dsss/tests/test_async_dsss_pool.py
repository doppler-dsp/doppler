"""AsyncDsssPool through the binding (design section 8.2): the C test owns
the lifecycle; this is the Python face -- construction, the slot record,
the symbols by slot, the event log attached through its capsule, and the
composition's state round trip. The searcher's false alarms are part of
the lifecycle (a noise peak seeds a free slot and is released one
interval later), so every expectation is about the EMITTER's slot."""

import json

import numpy as np
import pytest

from doppler.dsss import AsyncDsssPool
from doppler.telemetry import EventLog
from doppler.wfm import Gold

SF = 1023
CHIP_RATE = 5.0e6
SYM_RATE = 2700.0
SPC = 2
FS = CHIP_RATE * SPC
TE = SF * SPC
TSYM = FS / SYM_RATE
CN0 = 47.0
LOST_S = 0.3
N_SYM = 2700
PRE = 3

CODE = np.asarray(Gold().generate(SF)).astype(np.uint8)
_CSIGN = np.where(CODE & 1, -1.0, 1.0)


# The searcher deep enough that a seed lands inside the receivers' carrier
# pull-in (D = 16, 305 Hz rows against the 391 Hz the C header allows), on
# a waveform whose code-only window holds those epochs at any chip phase
# (20 symbols of every 270).
CELL_EPOCHS = 31
CELL_W_SYM = 20
CELL_F_SYM = 270


def _emitter(doppler_hz, delay, seed):
    """One emitter: async BPSK on the code at a fixed carrier offset with
    the fixture's code-only window (the first CELL_W_SYM symbols of every
    CELL_F_SYM are +1, the synth's rule), starting `delay` samples in,
    AWGN from CN0 (dp_dsss_windowed_capture's shape)."""
    rng = np.random.default_rng(seed)
    n = int(N_SYM * TSYM) + 4 * TE
    idx = np.arange(n)
    data = (rng.integers(0, 2, N_SYM + 4) * 2 - 1).astype(float)
    data[(np.arange(N_SYM + 4) % CELL_F_SYM) < CELL_W_SYM] = 1.0
    si = np.clip(np.floor(idx / TSYM).astype(int), 0, len(data) - 1)
    cph = (idx // SPC) % SF
    sig = data[si] * _CSIGN[cph] * np.exp(2j * np.pi * doppler_hz / FS * idx)
    sigma = 1.0 / np.sqrt(10.0 ** (CN0 / 10.0) / FS)
    total = PRE + delay + n
    noise = (sigma / np.sqrt(2.0)) * (
        rng.standard_normal(total) + 1j * rng.standard_normal(total)
    )
    x = np.concatenate([np.zeros(PRE + delay), sig]) + noise
    return x.astype(np.complex64)


def _two(a, b):
    """Two emitters on one channel: the sum, to the shorter's length."""
    n = min(len(a), len(b))
    return a[:n] + b[:n]


def _noise(n, seed):
    rng = np.random.default_rng(seed)
    sigma = 1.0 / np.sqrt(10.0 ** (CN0 / 10.0) / FS)
    return (
        (sigma / np.sqrt(2.0))
        * (rng.standard_normal(n) + 1j * rng.standard_normal(n))
    ).astype(np.complex64)


def _pool(n_slots=3, threads=1, **kw):
    args = {
        "chip_rate": CHIP_RATE,
        "symbol_rate": SYM_RATE,
        "spc": SPC,
        "cn0_dbhz": CN0,
        "pfa": 1e-3,
        "doppler_uncertainty": 6000.0,
        "code_only_epochs": CELL_EPOCHS,
        "max_peaks": 4,
        "n_slots": n_slots,
        "threads": threads,
        "lost_confirm_s": LOST_S,
    }
    args.update(kw)
    return AsyncDsssPool(CODE, **args)


def _feed(pool, x):
    last = 0
    for pos in range(0, len(x) - TE + 1, TE):
        last = pool.push(x[pos : pos + TE])
    return last


def _truth_chip(delay, sample):
    """The code phase an emitter started `delay` samples after PRE has at
    stream position `sample`."""
    return ((sample - (PRE + delay)) / SPC) % SF


def _slots_of(pool, doppler_hz, delay):
    """The slots holding the emitter seeded at `doppler_hz` (within the
    searcher's row) at that emitter's code phase (within a chip) -- a
    false alarm in the same row is at another phase."""
    out = []
    for i in range(pool.n_slots):
        r = pool.status(i)
        if not r.assigned:
            continue
        if abs(r.seed_doppler_hz - doppler_hz) > pool.doppler_res_hz:
            continue
        dc = abs(r.seed_chip_phase - _truth_chip(delay, r.seed_sample))
        if min(dc, SF - dc) <= 1.0:
            out.append(i)
    return out


def test_create_defaults_and_read_back():
    p = _pool()
    assert (p.n_slots, p.n_assigned, p.dropped, p.events) == (3, 0, 0, 0)
    assert p.samples_consumed == 0
    assert p.coherent_bins == 16
    assert 0.0 < p.doppler_res_hz < 391.0
    r = p.status(0)
    assert (r.slot, r.assigned, r.state) == (0, 0, 3)  # idle
    assert p.status(7).state == -1  # no such slot: a zero record
    assert len(p.symbols(0)) == 0


def test_one_emitter_is_assigned_once_tracked_released_and_reassigned():
    x = _emitter(1500.0, 40, 100)
    p = _pool()
    tail = 8 * TE
    assert _feed(p, x[:-tail]) >= 1
    assert p.dropped == 0
    (slot,) = _slots_of(p, 1500.0, 40)
    # An epoch carries half a symbol, so the symbols are summed over the
    # last eight pushes, read after each.
    n_syms = 0
    for pos in range(len(x) - tail, len(x) - TE + 1, TE):
        p.push(x[pos : pos + TE])
        syms = p.symbols(slot)
        assert syms.dtype == np.complex64
        n_syms += len(syms)
    assert n_syms > 0
    r = p.status(slot)
    assert r.state == 2 and r.code_locked == 1  # tracking
    assert abs(r.seed_doppler_hz - 1500.0) <= p.doppler_res_hz
    assert abs(r.doppler_hz - 1500.0) < 100.0
    assert r.assigned_samples > 0
    for i in range(3):
        if i != slot and not p.status(i).assigned:
            assert len(p.symbols(i)) == 0
    assert p.events >= 2
    # Off the air: released by the rule; back on: assigned again.
    _feed(p, _noise(int(2 * LOST_S * FS), 200))
    assert _slots_of(p, 1500.0, 40) == []
    assert _feed(p, x) >= 1
    (again,) = _slots_of(p, 1500.0, 40)
    assert p.status(again).state == 2
    p.reset()
    assert (p.n_assigned, p.events, p.samples_consumed) == (0, 0, 0)


def test_two_emitters_two_slots_and_a_full_pool_counts_the_drop():
    x = _two(_emitter(1500.0, 40, 101), _emitter(-3500.0, 900, 102))
    p = _pool(4)
    assert _feed(p, x) >= 2
    (a,) = _slots_of(p, 1500.0, 40)
    (b,) = _slots_of(p, -3500.0, 900)
    assert a != b
    assert abs(p.status(a).doppler_hz - 1500.0) < 100.0
    assert abs(p.status(b).doppler_hz + 3500.0) < 100.0
    assert p.dropped == 0
    one = _pool(1)
    assert _feed(one, x) == 1
    assert one.n_assigned == 1 and one.dropped >= 1


def test_threads_give_the_same_records():
    x = _two(_emitter(1500.0, 40, 101), _emitter(-3500.0, 900, 102))
    p1, p3 = _pool(4, 1), _pool(4, 3)
    assert _feed(p1, x) == _feed(p3, x) >= 2
    assert (p1.events, p1.dropped) == (p3.events, p3.dropped)
    for i in range(4):
        np.testing.assert_array_equal(p1.symbols(i), p3.symbols(i))


def test_event_log_attached_through_its_capsule(tmp_path):
    path = str(tmp_path / "run.events")
    log = EventLog(path)
    p = _pool(2)
    p.set_event_log(log)
    x = _emitter(1500.0, 40, 103)
    _feed(p, x)
    _feed(p, _noise(int(2 * LOST_S * FS), 201))
    p.set_event_log(None)
    log.close()
    with open(path) as f:
        rows = [json.loads(line) for line in f]
    labels = [r["core:label"] for r in rows]
    # The emitter's transitions, in order, with a false alarm's own
    # seeded/tracking/lost/released possibly interleaved.
    want = iter(["seeded", "tracking", "lost", "released"])
    nxt = next(want)
    for lab in labels:
        if lab == nxt:
            nxt = next(want, None)
    assert nxt is None
    assert len(rows) == p.events
    assert rows[0]["doppler:slot"] in (0, 1)
    assert all(
        r["doppler:reason"] == "lost"
        for r in rows
        if r["core:label"] == "released"
    )
    assert rows[0]["doppler:state"] == 1  # refining, at the seed
    assert all(r["core:sample_start"] <= p.samples_consumed for r in rows)


def test_state_round_trip_resumes_bit_exact_and_rejects():
    x = _emitter(1500.0, 40, 104)
    live, cold = _pool(2), _pool(2)
    half = (len(x) // TE // 2) * TE
    _feed(live, x[:half])
    assert len(_slots_of(live, 1500.0, 40)) == 1
    blob = live.get_state()
    assert len(blob) == live.state_bytes()
    cold.set_state(blob)
    assert cold.n_assigned == live.n_assigned
    assert cold.samples_consumed == live.samples_consumed
    for pos in range(half, len(x) - TE + 1, TE):
        live.push(x[pos : pos + TE])
        cold.push(x[pos : pos + TE])
        for i in range(2):
            np.testing.assert_array_equal(live.symbols(i), cold.symbols(i))
    assert cold.events == live.events
    bad = bytearray(blob)
    bad[0] ^= 0xFF
    with pytest.raises(ValueError):
        cold.set_state(bytes(bad))
    with pytest.raises(ValueError):
        _pool(3).set_state(blob)  # another slot count
    with pytest.raises(TypeError):
        cold.set_state("not bytes")


def test_create_refuses_a_searcher_a_cell_receiver_cannot_take():
    # No refine to floor: neither the method nor its read-back.
    pool = _pool()
    assert not hasattr(pool, "set_refine_min_blocks")
    assert not hasattr(pool, "refine_min_blocks")
    with pytest.raises(TypeError):
        _pool(refine_n_fft=64)
    # Refused: no searcher timing (D = 1), a row past the carrier loop's
    # pull-in (D = 12 -> 407 Hz), and a receiver argument out of range.
    for bad in (
        {"code_only_epochs": 1},
        {"code_only_epochs": 23},
        {"gain": 0.0},
    ):
        with pytest.raises(ValueError, match="AsyncDsssPool"):
            _pool(**bad)
