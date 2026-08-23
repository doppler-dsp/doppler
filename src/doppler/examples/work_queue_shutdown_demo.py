"""Ending an SPMC work queue, where the guarantees are not PUB/SUB's.

``graceful_shutdown_demo.py`` ends a PUB/SUB stream — one sender, every
subscriber gets every frame. This is the other tier: **SPMC**, one
producer feeding a pool of consumers over a NATS JetStream work queue,
which is how a stage scales out when one consumer cannot keep up. It
ends differently in three ways that matter, and all three are asserted
below rather than described.

1. **Nothing is lost.** PUB/SUB is at-most-once: a subscriber that cannot
   keep up drops frames rather than slowing the sender, and the ending is
   best-effort too, so a subscriber that must not hang still needs a
   timeout. A work queue is at-least-once — every frame *and* the ending
   arrive. That guarantee is the reason to reach for this tier.

2. **Exactly one consumer hears it.** A work queue load-balances: each
   message goes to one consumer, and the end-of-stream frame is a message
   like any other. With **one producer and five consumers**, ``send_eos()``
   ends *one* of the five. The other four are still waiting, and nothing
   will ever tell them. This is the trap — it looks like a broadcast
   shutdown and is not one — and 1:5 is where it stops being subtle:
   ending a pool of N needs N markers, or a different tier.

   The five consumers each run a thread and race for frames, so the
   distribution shown is the BROKER's and not this script's — it varies
   run to run, and so does which consumer gets told. The count carrying
   the claim is the total, never the shape of the split.

3. **The ending does not outlive the run.** A work-queue message stays in
   the stream until it is acked, and the caller cannot ack this one: it is
   reported as a state (``EOFError``) with no frame handed back. So the
   receive path acks it itself. Without that it would redeliver every
   AckWait forever and the *next* run against the subject would open onto
   a previous run's ending — a stream that ended before it began.

Run it::

    python work_queue_shutdown_demo.py

Needs a NATS broker on 127.0.0.1:4222 (``make nats-up``). See
``docs/design/io-termination.md`` for the one contract all three
transports share, and why the tiers are allowed to differ underneath it.
"""

from __future__ import annotations

import os
import socket
import sys
import threading
import time

import numpy as np

from doppler.stream import CF32, Pull, Push

#: One producer, five consumers. The ratio is the point of the example:
#: at 1:2 "one consumer was told" reads as a detail, and at 1:5 it reads
#: as the hazard it is -- four consumers left waiting on a stream that
#: has ended.
NUM_CONSUMERS = 5

#: Enough that the broker has something to distribute. A whole multiple
#: of the consumer count only so an even split is POSSIBLE -- it is not
#: expected, and nothing asserts it.
FRAMES_EACH = 4
NUM_FRAMES = NUM_CONSUMERS * FRAMES_EACH

#: Generous: a consumer that times out while frames remain takes them out
#: of the total, turning a slow broker into a failed claim.
RECV_MS = 3000

BLOCK = 256

#: The consumer's redelivery timer (``AckWait``, set where the durable
#: pull consumer is created in ``native/src/stream/stream_nats.c``), plus
#: a margin. Claim 3 cannot be checked faster than this: an unacked
#: message is only redelivered once that timer expires, so a shorter wait
#: cannot tell an acked ending from an unacked one and would pass either
#: way.
ACKWAIT_S = 5.0
STALE_CHECK_S = ACKWAIT_S + 1.5


def _broker_up(host: str = "127.0.0.1", port: int = 4222) -> bool:
    try:
        with socket.create_connection((host, port), timeout=0.5):
            return True
    except OSError:
        return False


def main() -> int:
    if not _broker_up():
        print(
            "no NATS broker on 127.0.0.1:4222 — start one with `make nats-up`"
        )
        return 0

    # Unique per run: a work queue is durable, so a fixed subject would
    # carry one run's leftovers into the next -- which is precisely the
    # failure claim 3 is about, and not something to reproduce by accident.
    endpoint = (
        f"nats://127.0.0.1:4222/work-queue-demo"
        f"-{os.getpid()}-{int(time.time())}"
    )
    print(f"SPMC work queue: 1 producer, {NUM_CONSUMERS} consumers")
    print(f"  {endpoint}")

    producer = Push(endpoint, CF32)
    consumers = [
        (f"consumer-{i + 1}", Pull(endpoint)) for i in range(NUM_CONSUMERS)
    ]

    try:
        block = np.zeros(BLOCK, dtype=np.complex64)
        print(f"\nsending {NUM_FRAMES} frames, then send_eos() once")
        for _ in range(NUM_FRAMES):
            producer.send(block, 1e6, 1e9)
        producer.send_eos()

        # Every consumer runs its own thread, pulling as fast as it can.
        # The distribution is then the BROKER's -- Pull.recv() releases the
        # GIL, so the five genuinely race, and which consumer gets which
        # frame is NATS deciding rather than this loop taking turns.
        #
        # An earlier version polled them in strict rotation. That produced
        # a perfectly even split every time, which looked like evidence and
        # was arithmetic: the loop was doing the load-balancing itself.
        # Left as a warning -- a harness that imposes the result it is
        # measuring will report it confidently.
        received = [0] * NUM_CONSUMERS
        told_it_ended: list[str] = []
        ledger = threading.Lock()

        def drain(index: int, name: str, consumer: Pull) -> None:
            while True:
                try:
                    samples, _hdr = consumer.recv(timeout_ms=RECV_MS)
                except EOFError:
                    # The ending. Exactly one consumer reaches this.
                    with ledger:
                        told_it_ended.append(name)
                    return
                except Exception:
                    # A timeout: this consumer asked while the queue was
                    # empty. It is done -- but nobody told it, which is
                    # claim 2 happening rather than being stated.
                    return
                consumer.ack(samples)
                received[index] += 1

        threads = [
            threading.Thread(target=drain, args=(i, name, consumer))
            for i, (name, consumer) in enumerate(consumers)
        ]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()

        total = sum(received)
        print(f"\n  frames received (total {total}):")
        for i, (name, _) in enumerate(consumers):
            ending = (
                "  <- told the stream ended" if name in told_it_ended else ""
            )
            print(f"    {name}: {received[i]}{ending}")

        # 1. At-least-once: nothing was dropped.
        assert total == NUM_FRAMES, (
            f"a work queue is at-least-once, so all {NUM_FRAMES} frames "
            f"must arrive; {total} did"
        )

        # 2. And exactly one consumer was told, which is the trap.
        assert len(told_it_ended) == 1, (
            f"a work queue load-balances, so the end-of-stream frame goes "
            f"to exactly one consumer; {len(told_it_ended)} of "
            f"{NUM_CONSUMERS} saw it"
        )
        # NOT that the split is even. It is the broker's to choose and it
        # varies run to run (measured: [5, 4, 5, 3, 3] on one trial, an
        # even [4, 4, 4, 4, 4] on the next). Asserting a shape would be
        # asserting a coincidence. The claim is the TOTAL -- a broadcast
        # tier would have delivered all NUM_FRAMES to each of the five.
        assert total != NUM_FRAMES * NUM_CONSUMERS, (
            "every consumer received every frame, which is PUB/SUB "
            "behaviour -- this is not a work queue"
        )
        silent = NUM_CONSUMERS - len(told_it_ended)
        print(
            f"\n  at-least-once : all {NUM_FRAMES} frames arrived, none lost"
        )
        print(
            f"  a queue       : {NUM_FRAMES} frames between "
            f"{NUM_CONSUMERS} consumers, not {NUM_FRAMES} each"
        )
        print(
            f"                  (split {received} — the broker's to "
            f"choose, and it varies)"
        )
        print(
            f"  one ending    : 1 of {NUM_CONSUMERS} was told; {silent} are "
            f"still waiting on a stream that has ended"
        )
        print(
            "                  ending a pool of N needs N markers, or a "
            "tier that broadcasts"
        )

        # 3. The ending was consumed, not merely delivered.
        print(
            f"\nchecking the ending did not outlive the run "
            f"({STALE_CHECK_S:.1f} s, one AckWait plus margin) ..."
        )
        t0 = time.monotonic()
        latecomer = Pull(endpoint)
        try:
            latecomer.recv(timeout_ms=int(STALE_CHECK_S * 1000))
            waited = time.monotonic() - t0
            raise AssertionError(
                f"a frame was still queued after {waited:.1f} s on a stream "
                f"that had been fully consumed"
            )
        except EOFError:
            waited = time.monotonic() - t0
            raise AssertionError(
                f"the end-of-stream frame came back after {waited:.1f} s: "
                f"nothing acked it, so it stays in the work queue and the "
                f"next run against this subject reads this run's ending"
            ) from None
        except AssertionError:
            raise
        except Exception:
            waited = time.monotonic() - t0
            print(
                f"  nothing came back in {waited:.1f} s — the queue is empty"
            )
        finally:
            latecomer.close()
    finally:
        producer.close()
        for _name, consumer in consumers:
            consumer.close()

    print("\nall three claims hold on the SPMC work-queue tier")
    return 0


if __name__ == "__main__":
    sys.exit(main())
