"""NATS sources against a plain list: seeded schedules, every read checked."""

import numpy as np
import pytest

import doppler.stream
from doppler.buffer import F32Buffer
from doppler.specan.source import PullSource, SocketSource
from doppler.specan.tests.test_nats_sources import (
    FS,
    HDR,
    _FakePull,
    _FakeTransport,
)
from doppler.stream import WORK_QUEUE_MAX_ACK_PENDING

ADDR = "nats://127.0.0.1:1/test"
MAX_BEHIND = WORK_QUEUE_MAX_ACK_PENDING


class _ScheduledSocket(_FakeTransport):
    """Lends a fresh copy of each frame, poisoned at the next recv."""

    def __init__(self, address):
        super().__init__(address)
        self._lent = None

    def recv(self, timeout_ms=None):
        if self._lent is not None and self._lent.dtype == np.complex64:
            self._lent[...] = np.nan  # a view kept past this recv reads NaN
        data, hdr = super().recv(timeout_ms)
        self._lent = data.copy()
        return self._lent, dict(hdr)


class _ScheduledPull(_FakePull):
    """_FakePull that poisons each frame at its ack and bounds the receives."""

    MAX_DELIVERIES = 10**9  # schedules re-send sequence numbers on purpose
    budget = 0  # a source that never acks is cut off after this many recvs

    def __init__(self, address):
        super().__init__(address)
        self.calls = 0

    def recv(self, timeout_ms=None):
        self.calls += 1
        if self.calls > self.budget:
            raise AssertionError("received past the schedule: never acked")
        return super().recv(timeout_ms)

    def ack(self, samples):
        super().ack(samples)
        if samples.dtype == np.complex64:
            samples[...] = np.nan  # a copy made after the ack reads NaN


def _frame_event(array, sequence, rate):
    hdr = {**HDR, "sample_rate": rate, "sequence": sequence}
    return ("frame", array, hdr)


def _schedule(rng, *, pull, length):
    """Seeded events: ("frame", array, header), ("timeout",), ("eos",)."""
    # starts far along, so a restart is a jump past MAX_BEHIND
    events, seq = [], int(rng.integers(2000, 10**6))
    for _ in range(length):
        roll = rng.random()
        if roll < 0.05:
            events.append(("timeout",))
            continue
        if roll < 0.055:  # rare, so a sequence climbs past MAX_BEHIND first
            events.append(("eos",))
            seq = 0  # the next producer counts from 0
            continue
        size = int(rng.integers(1, 5001)) if rng.random() > 0.03 else 0
        rate = FS if rng.random() < 0.8 else FS / 2
        frame = (
            rng.standard_normal(size) + 1j * rng.standard_normal(size)
        ).astype(np.complex64)
        if not pull:
            events.append(_frame_event(frame, seq, rate))
        elif roll < 0.10 and seq > 0:  # a redelivery of a recent frame
            behind = int(rng.integers(1, min(seq, MAX_BEHIND) + 1))
            events.append(_frame_event(frame, seq - behind, rate))
        elif roll < 0.12:  # frames lost upstream: a gap
            seq += int(rng.integers(1, 4))
            events.append(_frame_event(frame, seq, rate))
            seq += 1
        elif roll < 0.14:  # a new producer, from 0
            seq = 0
            events.append(_frame_event(frame, seq, rate))
            seq += 1
        elif roll < 0.16:  # no decoder takes this dtype
            u16 = rng.integers(0, 2**16, size, dtype=np.uint16)
            events.append(_frame_event(u16, seq, rate))
            seq += 1
        elif roll < 0.17:  # no sequence at all
            events.append(_frame_event(frame, None, rate))
        else:
            events.append(_frame_event(frame, seq, rate))
            seq += 1
    return events


def _items(events):
    """The fake transport's queue for these events."""
    out = []
    for ev in events:
        if ev[0] == "frame":
            out.append((ev[1], ev[2]))
        elif ev[0] == "timeout":
            out.append(TimeoutError("recv timed out"))
        else:
            out.append(EOFError("eos"))
    return out


class _ListModel:
    """The pre-change PullSource rules over a plain list: the reference."""

    def __init__(self, events, *, pull):
        self._events = events
        self._at = 0
        self._pull = pull
        self.held = []  # samples read and not yet taken
        self.next_seq = None
        self.fs = self.cf = 0.0
        self.duplicates = self.gaps = self.restarts = self.undecodable = 0
        self.acked = []

    def counters(self):
        return (self.duplicates, self.gaps, self.restarts, self.undecodable)

    def _next(self):
        if self._at == len(self._events):
            return ("timeout",)  # an exhausted schedule times out, as the fake
        self._at += 1
        return self._events[self._at - 1]

    def read(self, n):
        while len(self.held) < n:
            ev = self._next()
            if ev[0] == "timeout":
                break
            if ev[0] == "eos":
                self.next_seq = None
                break
            self._frame(ev[1], ev[2])
        out = self.held[:n]
        del self.held[:n]
        return np.array(out, dtype=np.complex64), self.fs, self.cf

    def _frame(self, frame, hdr):
        if not self._pull:
            self._decoded(frame, hdr)
            return
        self.acked.append(hdr["sequence"])
        if hdr["sequence"] is None:  # int(None) fails before any state moves
            self.undecodable += 1
            return
        seq = hdr["sequence"]
        if self.next_seq is not None:
            behind = self.next_seq - seq
            if 0 < behind <= MAX_BEHIND:
                self.duplicates += 1
                return
            if behind > MAX_BEHIND:
                self.restarts += 1
            else:
                self.gaps -= behind
        self.next_seq = seq + 1
        if frame.dtype != np.complex64:  # no decoder: after next_seq moved
            self.undecodable += 1
            return
        self._decoded(frame, hdr)

    def _decoded(self, frame, hdr):
        self.held.extend(frame.tolist())
        self.fs, self.cf = hdr["sample_rate"], hdr["center_freq"]


def _acked(src):
    """The sequence numbers the source's Pull has acked; none for a socket."""
    pull = getattr(src, "_pull", None)
    return [] if pull is None else pull.acked


@pytest.mark.parametrize("seed", range(4))
@pytest.mark.parametrize("pull", [False, True], ids=["socket", "pull"])
def test_every_read_matches_a_list_model(monkeypatch, seed, pull):
    rng = np.random.default_rng(seed)
    events = _schedule(rng, pull=pull, length=300)
    tiny = rng.integers(0, 64, 250)
    big = rng.integers(0, 9000, 250)
    reads = np.where(rng.random(250) < 0.3, tiny, big).tolist()

    items = _items(events)
    if pull:
        fake = type(
            "Fake",
            (_ScheduledPull,),
            {"frames": items, "budget": len(items) + len(reads) + 1},
        )
        monkeypatch.setattr(doppler.stream, "Pull", fake)
        src = PullSource(ADDR, timeout_ms=10)
    else:
        fake = type("Fake", (_ScheduledSocket,), {"frames": items})
        monkeypatch.setattr(doppler.stream, "Subscriber", fake)
        src = SocketSource(ADDR, timeout_ms=10)

    model = _ListModel(events, pull=pull)
    try:
        for n in reads:
            got, fs, cf = src.read(n)
            want, want_fs, want_cf = model.read(n)
            np.testing.assert_array_equal(got, want)
            assert (fs, cf) == (want_fs, want_cf)
            if pull:
                assert (
                    src.duplicates,
                    src.gaps,
                    src.restarts,
                    src.undecodable,
                ) == model.counters()
                # acked before read() returned, not at some later read
                assert _acked(src) == model.acked
        # every frame taken exactly once, duplicates and undecodable too
        assert _acked(src) == model.acked
    finally:
        src.close()


def _pull_source(monkeypatch, frames):
    fake = type("Fake", (_ScheduledPull,), {"frames": frames, "budget": 99})
    monkeypatch.setattr(doppler.stream, "Pull", fake)
    return PullSource(ADDR, timeout_ms=10)


def test_a_read_that_raises_acks_the_frames_it_took(monkeypatch):
    """A transport error after two frames: both were taken, both acked."""
    frames = [
        (np.ones(4, np.complex64), {**HDR, "sequence": 0}),
        (np.ones(4, np.complex64), {**HDR, "sequence": 1}),
        RuntimeError("link down"),
    ]
    src = _pull_source(monkeypatch, frames)
    with pytest.raises(RuntimeError, match="link down"):
        src.read(64)
    assert src._pull.acked == [0, 1]


def test_the_receive_budget_turns_a_hang_into_a_failure(monkeypatch):
    """A source that never acks is cut off, not left to redeliver forever."""
    frames = [
        (np.ones(4, np.complex64), {**HDR, "sequence": k}) for k in (0, 1)
    ]
    fake = type("Fake", (_ScheduledPull,), {"frames": frames, "budget": 1})
    monkeypatch.setattr(doppler.stream, "Pull", fake)
    src = PullSource(ADDR, timeout_ms=10)
    with pytest.raises(AssertionError, match="never acked"):
        src.read(64)


def test_a_read_that_fails_mid_packet_still_acks_that_frame(monkeypatch):
    """Frame 1 is held, unacked, when the ring fails: its ack still runs."""
    frames = [
        (np.ones(4, np.complex64), {**HDR, "sequence": k}) for k in range(3)
    ]
    src = _pull_source(monkeypatch, frames)
    fit = src._fit
    calls = []

    def fit_then_fail(n, p):
        calls.append(p)
        if len(calls) == 2:
            raise MemoryError("injected")
        return fit(n, p)

    monkeypatch.setattr(src, "_fit", fit_then_fail)
    with pytest.raises(MemoryError, match="injected"):
        src.read(64)
    assert src._pull.acked == [0, 1]


def test_read_leaves_its_packet_generator_finished(monkeypatch):
    """A generator still suspended after read() holds an unacked frame."""
    frames = [
        (np.ones(4, np.complex64), {**HDR, "sequence": k}) for k in range(3)
    ]
    src = _pull_source(monkeypatch, frames)
    packets = src._packets
    made = []

    def recording():
        gen = packets()
        made.append(gen)
        return gen

    monkeypatch.setattr(src, "_packets", recording)
    got, _, _ = src.read(8)  # fills from two frames: ends mid-stream
    assert len(got) == 8
    assert made and all(gen.gi_frame is None for gen in made)
    assert src._pull.acked == [0, 1]


def test_a_rebuild_with_samples_held_keeps_them_in_order(monkeypatch):
    """A 50-sample backlog, then a 5000-sample packet: the ring is rebuilt."""
    frames = [
        (np.arange(50, dtype=np.complex64), {**HDR, "sequence": 0}),
        (np.arange(50, 5050, dtype=np.complex64), {**HDR, "sequence": 1}),
    ]
    src = _pull_source(monkeypatch, frames)
    got, _, _ = src.read(100)
    np.testing.assert_array_equal(got, np.arange(100, dtype=np.complex64))
    assert src._ring.capacity >= 100 + 5000  # the packet fit, once rebuilt
    rest, _, _ = src.read(4950)
    want = np.arange(100, 5050, dtype=np.complex64)
    np.testing.assert_array_equal(rest, want)


def test_a_refused_write_is_an_error_not_a_silent_loss(monkeypatch):
    frames = [(np.ones(4, np.complex64), {**HDR, "sequence": 0})]
    src = _pull_source(monkeypatch, frames)
    ring = F32Buffer(1)  # too small for the 4-sample packet
    monkeypatch.setattr(src, "_fit", lambda n, p: ring)
    try:
        with pytest.raises(RuntimeError, match="refused"):
            src.read(64)
    finally:
        ring.destroy()


def test_a_negative_read_is_refused():
    with pytest.raises(ValueError, match="negative"):
        PullSource(ADDR, timeout_ms=10).read(-1)
