"""Integration tests for doppler.telemetry.Telemetry.

The C-level contract (registry, decimation, wraparound, overrun, SPSC
threading) is covered by native/tests/test_telemetry_core.c; these tests
cover the Python binding surface: the structured-array read, the
name/id/decim plumbing, error translation, the capsule attach point, and
the producer/consumer split across real Python threads.
"""

import threading

import numpy as np
import pytest

from doppler.telemetry import Telemetry

REC_DTYPE = np.dtype(
    [("n", "<u8"), ("value", "<f4"), ("probe", "<u2"), ("flags", "<u2")]
)


def test_create_and_capacity():
    tlm = Telemetry(1 << 12)
    assert tlm.capacity >= 1 << 12
    # capacity is a power of two (page rounding preserves this)
    assert tlm.capacity & (tlm.capacity - 1) == 0
    assert tlm.dropped == 0
    assert tlm.probe_count == 0


def test_create_rejects_bad_sizes():
    with pytest.raises(ValueError):
        Telemetry(0)
    with pytest.raises(ValueError):
        Telemetry(-4)
    with pytest.raises(MemoryError):
        Telemetry(3)  # not a power of two


def test_probe_registry():
    tlm = Telemetry(1 << 12)
    a = tlm.probe("agc.gain_db")
    b = tlm.probe("sync.e", decim=4)
    assert (a, b) == (0, 1)
    assert tlm.probe("agc.gain_db", decim=8) == a  # idempotent
    assert tlm.probe_count == 2
    assert tlm.probe_id("sync.e") == b
    assert tlm.probe_names() == {"agc.gain_db": 0, "sync.e": 1}
    with pytest.raises(KeyError):
        tlm.probe_id("nope")
    with pytest.raises(ValueError):
        tlm.probe("x" * 64)  # overlong name
    with pytest.raises(ValueError):
        tlm.probe("x", decim=0)


def test_read_dtype_and_roundtrip():
    tlm = Telemetry(1 << 12)
    gid = tlm.probe("agc.gain_db")
    tlm.set_now(1000)
    tlm.emit(gid, 1.5)
    tlm.set_now(2000)
    tlm.emit(gid, -3.25)

    recs = tlm.read()
    assert recs.dtype == REC_DTYPE
    assert recs.dtype.itemsize == 16
    assert recs.shape == (2,)
    assert recs["n"].tolist() == [1000, 2000]
    assert recs["value"].tolist() == [1.5, -3.25]
    assert recs["probe"].tolist() == [gid, gid]
    assert recs["flags"].tolist() == [0, 0]
    assert tlm.emitted(gid) == 2

    # Drained: empty (but correctly-typed) array, non-blocking.
    empty = tlm.read()
    assert empty.shape == (0,)
    assert empty.dtype == REC_DTYPE


def test_read_max_records_partial():
    tlm = Telemetry(1 << 12)
    pid = tlm.probe("x")
    for i in range(10):
        tlm.emit(pid, float(i))
    first = tlm.read(max_records=3)
    rest = tlm.read()
    assert first["value"].tolist() == [0.0, 1.0, 2.0]
    assert len(rest) == 7
    assert rest["value"].tolist() == [float(i) for i in range(3, 10)]


def test_decimation():
    tlm = Telemetry(1 << 12)
    pid = tlm.probe("x", decim=3)
    for i in range(10):
        tlm.emit(pid, float(i))
    # First event emits (phase primed), then every third.
    assert tlm.read()["value"].tolist() == [0.0, 3.0, 6.0, 9.0]


def test_overrun_drops_and_counts():
    tlm = Telemetry(256)
    pid = tlm.probe("x")
    cap = tlm.capacity
    for i in range(cap + 50):
        tlm.emit(pid, float(i))
    assert tlm.dropped == 50
    assert tlm.emitted(pid) == cap
    # The ring keeps the FIRST cap records (drop-new, never overwrite).
    recs = tlm.read()
    assert len(recs) == cap
    assert recs["value"][-1] == float(cap - 1)


def test_multi_probe_series_split():
    """The exemplary use case: two signals, split by probe id."""
    tlm = Telemetry(1 << 12)
    gid = tlm.probe("agc.gain_db")
    eid = tlm.probe("sync.e")
    for i in range(8):
        tlm.set_now(i * 100)
        tlm.emit(gid, -float(i))
        tlm.emit(eid, float(i) / 10)

    recs = tlm.read()
    gain = recs[recs["probe"] == tlm.probe_id("agc.gain_db")]
    err = recs[recs["probe"] == tlm.probe_id("sync.e")]
    assert gain["value"].tolist() == [-float(i) for i in range(8)]
    assert err["n"].tolist() == [i * 100 for i in range(8)]
    np.testing.assert_allclose(
        err["value"], np.arange(8, dtype=np.float32) / 10, rtol=1e-6
    )


def test_emit_unknown_probe_raises():
    tlm = Telemetry(1 << 12)
    with pytest.raises(ValueError):
        tlm.emit(0, 1.0)  # nothing registered yet


def test_destroy_then_use_raises():
    tlm = Telemetry(1 << 12)
    tlm.destroy()
    tlm.destroy()  # idempotent
    with pytest.raises(RuntimeError):
        tlm.read()
    with pytest.raises(RuntimeError):
        tlm.probe("x")


def test_capsule_attach_point():
    tlm = Telemetry(1 << 12)
    cap = tlm._capsule
    assert type(cap).__name__ == "PyCapsule"


def test_producer_consumer_threads():
    """SPSC across real threads: emit in one, read in another."""
    tlm = Telemetry(1 << 14)
    pid = tlm.probe("x")
    n_events = 20000

    def produce():
        for i in range(n_events):
            tlm.set_now(i)
            tlm.emit(pid, float(i % 1000))

    got = []
    t = threading.Thread(target=produce)
    t.start()
    while t.is_alive():
        got.append(tlm.read())
    t.join()
    got.append(tlm.read())  # tail after the join

    recs = np.concatenate(got)
    assert len(recs) + tlm.dropped == n_events
    # In-order delivery: stamped sample indices strictly increase.
    assert np.all(np.diff(recs["n"].astype(np.int64)) > 0)


# ── read_dict / set_decim / stats ────────────────────────────────────────────
# The shaping and accounting every consumer of read() was rebuilding by hand.


def test_read_dict_groups_by_name_and_keeps_every_probe():
    """One drain, grouped by name -- and a key per REGISTERED probe, not per
    probe seen in this batch, so the dict's shape is stable while draining
    block by block."""
    tlm = Telemetry(1 << 12)
    a, b = tlm.probe("loop.e"), tlm.probe("loop.lock")
    for i in range(4):
        tlm.emit(a, i / 2)

    d = tlm.read_dict()
    assert sorted(d) == ["loop.e", "loop.lock"]
    assert list(d["loop.e"]) == [0.0, 0.5, 1.0, 1.5]
    assert d["loop.e"].dtype == np.float32
    assert d["loop.lock"].size == 0  # registered, silent, still a key
    assert tlm.read().size == 0  # ...and it really drained the ring
    del b


def test_read_dict_index_carries_the_sample_index():
    """`index=True` is what makes a real time axis possible: the caller
    divides n by fs and gets seconds, instead of plotting an ordinal."""
    tlm = Telemetry(1 << 12)
    e = tlm.probe("sync.e")
    for blk, n in enumerate((0, 256, 512)):
        tlm.set_now(n)
        tlm.emit(e, float(blk))

    n, v = tlm.read_dict(index=True)["sync.e"]
    assert list(n) == [0, 256, 512]
    assert list(v) == [0.0, 1.0, 2.0]
    assert n.dtype == np.uint64 and v.dtype == np.float32


def test_read_dict_matches_read_exactly():
    """It is the same drain, only reshaped -- so it must agree with the
    hand-rolled filter it replaces, value for value."""
    tlm = Telemetry(1 << 14)
    ids = {name: tlm.probe(name) for name in ("a", "b", "c")}
    rng = np.random.default_rng(3)
    order = rng.integers(0, 3, 300)
    for k, which in enumerate(order):
        tlm.set_now(k)
        tlm.emit(ids["abc"[which]], float(k))

    tlm2 = Telemetry(1 << 14)  # a second run, drained the OLD way
    ids2 = {name: tlm2.probe(name) for name in ("a", "b", "c")}
    for k, which in enumerate(order):
        tlm2.set_now(k)
        tlm2.emit(ids2["abc"[which]], float(k))
    recs = tlm2.read()

    d = tlm.read_dict()
    for name, pid in ids2.items():
        assert np.array_equal(d[name], recs[recs["probe"] == pid]["value"])


def test_read_dict_max_records_leaves_the_rest():
    tlm = Telemetry(1 << 12)
    e = tlm.probe("e")
    for i in range(10):
        tlm.emit(e, float(i))
    assert list(tlm.read_dict(max_records=4)["e"]) == [0.0, 1.0, 2.0, 3.0]
    assert list(tlm.read_dict()["e"]) == [4.0, 5.0, 6.0, 7.0, 8.0, 9.0]


def test_set_decim_thins_one_probe_only():
    """Per-probe decimation was already possible (probe() is idempotent by
    name and updates decim) but only as a side effect of 're-registering'.
    This names it -- and the sibling probe must be untouched."""
    tlm = Telemetry(1 << 12)
    e, lk = tlm.probe("sync.e"), tlm.probe("sync.lock")
    tlm.set_decim("sync.e", 4)
    for i in range(8):
        tlm.emit(e, float(i))
        tlm.emit(lk, 1.0)

    d = tlm.read_dict()
    assert list(d["sync.e"]) == [0.0, 4.0]  # every 4th
    assert d["sync.lock"].size == 8  # untouched


def test_set_decim_rejects_unknown_and_zero():
    tlm = Telemetry(1 << 12)
    tlm.probe("known")
    with pytest.raises(KeyError):
        tlm.set_decim("nope", 2)
    with pytest.raises(ValueError):
        tlm.set_decim("known", 0)


def test_stats_reconciles_emitted_against_dropped():
    tlm = Telemetry(1 << 12)
    a, b = tlm.probe("a"), tlm.probe("b")
    for _i in range(5):
        tlm.emit(a, 1.0)
    tlm.emit(b, 2.0)

    s = tlm.stats()
    assert s["emitted"] == {"a": 5, "b": 1}
    assert s["dropped"] == 0
    assert s["probes"] == 2
    assert s["capacity"] == tlm.capacity
    del a, b


def test_stats_sees_a_real_overrun():
    """An overrun must show up as dropped, and emitted must count only what
    actually landed -- that is the whole point of reconciling them."""
    tlm = Telemetry(8)  # tiny ring, rounded up to the page minimum
    p = tlm.probe("flood")
    n = tlm.capacity * 4
    for i in range(n):
        tlm.emit(p, float(i))

    s = tlm.stats()
    assert s["dropped"] > 0
    assert s["emitted"]["flood"] + s["dropped"] == n
