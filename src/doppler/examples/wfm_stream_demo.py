"""wfm_stream_demo.py — stream a capture to and from a BLUE file.

Every other container example writes a whole array in one call and reads the
whole capture back in one call. Real captures do not work that way: a source
delivers whatever size buffer it happens to have, the total length is not known
when the file is opened, and a reader has to drain to end-of-file without being
told how much to expect. That path exercises three things a one-shot write
never touches:

* **``total=0``** — the length is unknown at open, so the BLUE header goes down
  with a placeholder ``data_size`` that ``close()`` overwrites with the count
  actually written. (The patch is unconditional, so a wrong ``total`` is
  corrected too — but with ``total=0`` it is the *only* thing standing between
  you and a header claiming an empty capture.)
* **draining to EOF** — ``read()`` is bounded by the payload the header
  declares, so the loop stops at the last sample rather than continuing into
  whatever follows it. To make that bound do visible work, this demo appends
  stand-in bytes after the payload, occupying the region where a real BLUE file
  keeps its extended header: an unbounded reader hands them back decoded as IQ
  (64 samples of garbage, here), which is silent corruption rather than an
  error. They are only stand-ins — a genuine extended header would also be
  pointed to by the HCB and decoded into ``Reader.keywords``, which the Python
  ``Writer`` currently cannot produce (the C API has ``add_keyword``; there is
  no binding for it yet).
* **``close()`` reporting** — the ``data_size`` patch happens at close, after
  the samples, so a failure there means the file on disk is wrong. ``close()``
  raises rather than returning quietly, and ``with`` propagates it.

Both directions re-block through :class:`doppler.buffer.F32Buffer`, a lock-free
SPSC ring with a double-mapped window: producer chunks go in at whatever size
they arrive, fixed-size blocks come out, and a block that straddles the wrap is
still returned as one contiguous zero-copy view. That is the whole reason to
reach for it here — the obvious alternative, keeping a numpy leftover array and
``np.concatenate``-ing each arrival, copies the tail on every chunk and
allocates as it goes. Measured on this shape it is worth about 1.4x (0.90 ms vs
1.26 ms per 1.18 Msamp) and, more usefully, zero allocations in the loop.

The ring's ``wait(n)`` *spins* until ``n`` samples arrive. It releases the GIL,
so a producer on another thread is the case it is built for, but it has no
timeout and no short return, so asking for one sample more than was written
hangs forever. The loops below size every block from ``ring.available`` and so
can never ask for a sample that has not landed.

Run::

    python src/doppler/examples/wfm_stream_demo.py     # → stream.blue
"""

from __future__ import annotations

import os

import numpy as np

from doppler.buffer import F32Buffer
from doppler.wfm import Reader, Writer, qpsk

FS = 1e6  # sample rate, Hz
PATH = "stream.blue"
TX_BLOCK = 4096  # fixed block size handed to the writer
RX_BLOCK = 1024  # fixed block size handed to the consumer

# A source that does not respect block boundaries: 40 arrivals of between 700
# and 5000 samples, deterministic so the run is reproducible.
_rng = np.random.default_rng(7)
ARRIVALS = [int(n) for n in _rng.integers(700, 5000, size=40)]


# --8<-- [start:write]
# The producer streams: each call continues the modulator's state, so the
# concatenation of the chunks is one continuous QPSK signal.
synth = qpsk(sps=8, snr=30.0, fs=FS, seed=1)

tx_ring = F32Buffer(1 << 15)  # power of two; read .capacity for what you got
sent = []  # keep a reference copy to verify the round-trip
n_written = 0

# total=0: the length is NOT known when the header goes down. close() patches
# the BLUE data_size from the count actually written.
with Writer(PATH, file_type="blue", sample_type="cf32", fs=FS, total=0) as w:
    for arrival in ARRIVALS:
        chunk = synth.steps(arrival)
        sent.append(chunk)
        assert tx_ring.write(chunk), "ring overflow — grow the capacity"

        # Drain whole blocks only. `available` is what makes wait() safe:
        # it never reports a sample that has not landed, and wait() would
        # spin forever on one that has not.
        while tx_ring.available >= TX_BLOCK:
            block = tx_ring.wait(TX_BLOCK)  # contiguous even across the wrap
            n_written += w.write(block)
            tx_ring.consume(TX_BLOCK)

    # The stream ended mid-block; flush the remainder.
    if tail := tx_ring.available:
        n_written += w.write(tx_ring.wait(tail))
        tx_ring.consume(tail)
# close() ran here: data_size is now the real count, not the placeholder.
# --8<-- [end:write]

# A real BLUE file usually carries an extended header after the samples. Append
# stand-in bytes so "stop at the payload" is a claim with something to prove --
# without the bound these decode as ~64 extra samples of garbage.
with open(PATH, "ab") as fh:
    fh.write(bytes(range(256)) * 2)
trailing = os.path.getsize(PATH) - (512 + n_written * 8)

# --8<-- [start:read]
rx_ring = F32Buffer(1 << 15)
received = []

with Reader(PATH) as r:
    declared = r.num_samples  # recovered from the patched header
    while True:
        got = r.read(TX_BLOCK)  # short on the final block, then empty
        if len(got) == 0:
            break
        assert rx_ring.write(got), "ring overflow — grow the capacity"

        while rx_ring.available >= RX_BLOCK:
            block = rx_ring.wait(RX_BLOCK)
            received.append(np.array(block))  # copy: consume() invalidates it
            rx_ring.consume(RX_BLOCK)

    if tail := rx_ring.available:
        received.append(np.array(rx_ring.wait(tail)))
        rx_ring.consume(tail)

    # The read loop stopped at the declared payload, not at end-of-file.
    at_end = r.read(TX_BLOCK)
# --8<-- [end:read]

reference = np.concatenate(sent)
readback = np.concatenate(received)
size = os.path.getsize(PATH)

print(
    f"{len(ARRIVALS)} ragged arrivals "
    f"({min(ARRIVALS)}–{max(ARRIVALS)} samples) → {n_written} samples\n"
    f"  → {PATH} ({size} bytes = 512 header + payload + {trailing} trailing)\n"
    f"  → drained in {TX_BLOCK}-sample reads, re-blocked to {RX_BLOCK}"
)

# ── validate ─────────────────────────────────────────────────────────────────
# Re-blocking is lossless in both directions: what the ragged producer emitted
# is exactly what came back, sample for sample.
assert n_written == len(reference), (
    f"wrote {n_written} of {len(reference)} produced"
)
assert len(readback) == len(reference), (
    f"read back {len(readback)}, wrote {len(reference)}"
)
assert np.array_equal(readback, reference), "round-trip is not bit-exact"

# The header was opened with total=0 and patched at close. If close() had not
# run, or had run without seeking, this would still be the placeholder.
assert declared == len(reference), (
    f"header declares {declared} samples, wrote {len(reference)}"
)

# Draining stopped at the payload, with real bytes sitting after it. A reader
# that ran to end-of-file instead would return those decoded as IQ -- silent
# corruption rather than an error.
assert trailing > 0, "the trailing-bytes probe did not append anything"
assert len(at_end) == 0, (
    f"read past the payload returned {len(at_end)} samples; "
    f"{trailing} trailing bytes were not excluded"
)

# Nothing was dropped: every arrival fit, so the ring never rejected a write.
assert tx_ring.dropped == 0, f"tx ring dropped {tx_ring.dropped}"
assert rx_ring.dropped == 0, f"rx ring dropped {rx_ring.dropped}"

# The blocks handed to the writer were uniform except the flushed tail, which
# is the point of re-blocking a ragged source.
assert len(reference) % TX_BLOCK != 0, (
    "test shape is degenerate: the stream happens to be a whole number of "
    "blocks, so the tail-flush path never ran"
)

print(
    f"validated: {len(reference)} samples bit-exact both ways, "
    f"header data_size patched at close ({declared}),\n"
    f"  drain stopped at the payload, 0 dropped"
)
