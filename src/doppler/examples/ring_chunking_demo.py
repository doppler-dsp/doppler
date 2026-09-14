"""ring_chunking_demo.py — the ring's whole job: the producer's block size
is not the consumer's.

A capture device hands you whatever its driver batched. The thing you feed
next wants a size of its own, and it is never the same size. `F32Buffer` is
what sits between them, and the double-mapping is what makes the seam free:
`wait(n)` returns a CONTIGUOUS view of n samples even when those n straddle
the physical end of the ring, so the consumer gets a real array to hand
straight to an FFT with no copy and no branch for the wrap.

Two directions, and they are the two that actually occur:

  A. LARGE IN, FIXED OUT — irregular multi-thousand-sample blocks in, exact
     1024-sample frames out. This is the spectrum-analyser shape: an FFT of
     length N cannot take N-1 or N+1, so somebody has to re-block the stream,
     and doing it by concatenating numpy arrays copies the whole backlog on
     every append. Here the re-blocking IS the ring.

  B. SMALL IN, DRAIN WHEN FULL — a drip of tens of samples in, one big batch
     out whenever the backlog reaches a threshold. This is the opposite
     shape: per-call overhead dominates when the producer is chatty, so the
     consumer waits to be given enough to be worth waking up for.

Both run over many wraps of the ring deliberately — the wrap is the case the
double-mapping exists for, so an example that never reaches it demonstrates
nothing about it. The assertions are physical: every frame is exactly the
length the consumer asked for, contiguous, and the concatenation of all
frames is the input stream in order, sample for sample.

**Do not ask for more than the ring holds.** `wait(n)` with `n > capacity`
can never be satisfied and currently spins forever with no diagnostic
(doppler#1335) — so a threshold is chosen from `capacity` below, never from
the producer's chunk size.

Run:
  python ring_chunking_demo.py
"""

import threading

import numpy as np

from doppler.buffer import F32Buffer
from doppler.spectral import FFT


# A ramp, so every sample says where in the stream it came from: a frame
# that is off by one, duplicated or dropped shows up as a break in the
# sequence rather than as a plausible-looking block of signal.
def stream(start: int, n: int) -> np.ndarray:
    idx = np.arange(start, start + n)
    return (idx + 1j * idx).astype(np.complex64)


# ── A. large irregular blocks in, exact FFT frames out ──────────────────

FFT_N = 1024
RING = F32Buffer(8192)
CAP = RING.capacity

# What the device hands over: nothing is a multiple of FFT_N, and the
# largest is four frames' worth. Sizes repeat so the run covers many wraps.
DRIVER_BLOCKS = [3000, 5000, 1700, 4096, 777, 2048, 6000, 1234]

fft = FFT(FFT_N, -1)
frames: list[np.ndarray] = []
produced = 0

for _ in range(6):
    for block in DRIVER_BLOCKS:
        # The ring is finite and `write` is ALL-OR-NOTHING: a block that
        # does not fit is rejected whole and counted in `dropped`, never
        # partially written. So drain to make room BEFORE writing, which is
        # the backpressure this topology has -- there is no blocking write.
        while CAP - RING.available < block:
            view = RING.wait(FFT_N)
            frames.append(np.asarray(fft.execute_cf32(view)))
            RING.consume(FFT_N)
        assert RING.write(stream(produced, block)), "made room, so it fits"
        produced += block

        # Take every whole frame that has landed. `available` is a lower
        # bound from the consumer side, so sizing the loop from it is safe.
        while RING.available >= FFT_N:
            view = RING.wait(FFT_N)
            # The view is a zero-copy window into the ring, contiguous even
            # across the wrap -- which is what lets it go straight into the
            # FFT. Anything kept after consume() must be copied first.
            assert view.flags["C_CONTIGUOUS"], "the double-mapping's promise"
            assert len(view) == FFT_N, "a frame is exactly N, never n-1"
            frames.append(np.asarray(fft.execute_cf32(view)))
            RING.consume(FFT_N)

assert RING.dropped == 0, f"backpressure failed: {RING.dropped} dropped"

# The frames are FFTs, so reassembling the INPUT means re-running the
# geometry rather than the data: prove instead that the consumer saw a
# contiguous, in-order, gap-free prefix of the stream, by replaying the
# same block sizes through a plain counter.
consumed = len(frames) * FFT_N
assert consumed + RING.available == produced, (
    f"{consumed} consumed + {RING.available} still in the ring "
    f"!= {produced} produced -- a frame was lost or double-counted"
)
print(
    f"A. {produced:,} samples in as {len(DRIVER_BLOCKS) * 6} irregular "
    f"blocks ({min(DRIVER_BLOCKS)}..{max(DRIVER_BLOCKS)}) -> "
    f"{len(frames):,} FFT frames of exactly {FFT_N}, "
    f"{RING.available} left over, 0 dropped"
)
assert produced // FFT_N == len(frames), "every whole frame was taken"
assert len(frames) > 2 * CAP // FFT_N, "the run must cover several wraps"


# ── A'. the same seam, checked sample-for-sample ────────────────────────
# The FFT above proves the frames are usable; this proves they are the RIGHT
# samples. Identical block geometry, no transform -- so a frame is the raw
# window and the concatenation must equal the input exactly.

raw = F32Buffer(8192)
raw_cap = raw.capacity
got: list[np.ndarray] = []
pos = 0
for _ in range(6):
    for block in DRIVER_BLOCKS:
        while raw_cap - raw.available < block:
            got.append(np.asarray(raw.wait(FFT_N)).copy())
            raw.consume(FFT_N)
        raw.write(stream(pos, block))
        pos += block
        while raw.available >= FFT_N:
            got.append(np.asarray(raw.wait(FFT_N)).copy())
            raw.consume(FFT_N)

joined = np.concatenate(got)
assert np.array_equal(joined, stream(0, len(joined))), (
    "the re-blocked stream is not the input stream -- a wrap lost or "
    "duplicated samples"
)
print(
    f"A'. same geometry without the transform: {len(joined):,} samples "
    f"re-blocked and identical to the input, in order"
)


# ── B. a drip in, one big batch out whenever the backlog is worth it ────

BATCH = 2048
assert RING.capacity >= BATCH, "wait() above capacity never returns (#1335)"

drip = F32Buffer(4096)
DRIP_BLOCKS = [16, 48, 32, 9, 64, 24, 100, 7]
TOTAL = 60_000

batches: list[int] = []
occupancy: list[int] = []


def producer() -> None:
    """Write small irregular blocks until TOTAL, then say so."""
    sent = 0
    i = 0
    cap = drip.capacity
    while sent < TOTAL:
        n = min(DRIP_BLOCKS[i % len(DRIP_BLOCKS)], TOTAL - sent)
        # Wait for ROOM, rather than retrying a write that will be refused.
        # `available` read from the producer side is an UPPER bound -- the
        # consumer can only shrink it -- so free space computed from it is a
        # lower bound, and a write sized by it always fits. Retrying instead
        # would work, but see the note below about what it does to
        # `dropped`.
        while cap - drip.available < n:
            pass
        assert drip.write(stream(sent, n)), "waited for room, so it fits"
        sent += n
        i += 1
    drip.close()  # the consumer's only way to tell "slow" from "finished"


t = threading.Thread(target=producer)
t.start()

received = 0
while True:
    try:
        # One wake-up per BATCH samples instead of one per 7..100-sample
        # write -- which is the entire point of buffering a chatty producer.
        view = drip.wait(BATCH)
    except EOFError:
        break  # closed AND drained: whatever is left is under a batch
    occupancy.append(drip.available)
    assert len(view) == BATCH, "a drain is the whole batch"
    assert np.array_equal(np.asarray(view), stream(received, BATCH)), (
        "the batch is not the next BATCH samples of the stream"
    )
    received += BATCH
    batches.append(BATCH)
    drip.consume(BATCH)

t.join()

# The tail under one batch is still in the ring: close() does not discard it,
# and a consumer that only ever asks for BATCH will never see it. That is a
# real consequence of this pattern, not a bug -- drain it explicitly.
tail = drip.available
if tail:
    view = drip.wait(tail)
    assert np.array_equal(np.asarray(view), stream(received, tail))
    drip.consume(tail)
    received += tail

assert received == TOTAL, f"{received} received != {TOTAL} sent"
assert drip.dropped == 0, f"{drip.dropped} dropped -- the producer raced"
assert len(batches) == TOTAL // BATCH, "one wake-up per batch, no more"
assert max(occupancy) <= drip.capacity, "occupancy cannot exceed the ring"


# ── what `dropped` actually counts, because it is not what it sounds like ──
# It is incremented by the LENGTH OF EVERY REJECTED CALL, not by samples
# lost. A producer that spins on `write` until it succeeds -- the obvious
# way to apply backpressure -- therefore inflates it without losing a single
# sample: the first run of this demo did exactly that and reported 5,960,438
# "dropped" out of 60,000 sent. Read it as "samples in refused writes", and
# wait for room (as the producer above does) if you want it to mean what a
# drop counter usually means.
probe = F32Buffer(1024)
probe.write(np.zeros(probe.capacity, dtype=np.complex64))  # now full
for _ in range(3):
    assert not probe.write(np.zeros(10, dtype=np.complex64)), "full"
assert probe.dropped == 30, (
    f"three refused 10-sample writes counted {probe.dropped}, not 30 -- "
    "`dropped` counts the length of each rejected call"
)
assert probe.available == probe.capacity, "and nothing was partially written"

# What the batching bought: one wake-up per BATCH instead of one per write.
mean_write = sum(DRIP_BLOCKS) / len(DRIP_BLOCKS)
unbatched = round(TOTAL / mean_write)
print(
    f"B. {TOTAL:,} samples in as {min(DRIP_BLOCKS)}..{max(DRIP_BLOCKS)}"
    f"-sample writes -> {len(batches)} drains of {BATCH} plus a "
    f"{tail}-sample tail; {len(batches)} wake-ups instead of "
    f"~{unbatched:,}, 0 dropped"
)
print(
    "validated: a ring decouples the producer's block size from the "
    "consumer's in both directions -- large irregular blocks re-blocked "
    "into exact FFT frames across many wraps, and a small-write drip "
    "batched into drains -- with every frame contiguous, in order, and "
    "nothing dropped"
)
