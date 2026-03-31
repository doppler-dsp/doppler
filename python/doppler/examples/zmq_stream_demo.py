#!/usr/bin/env python3
"""
IQ signal streaming demo using the doppler Python extension.

Demonstrates:
- PUSH/PULL sockets for producer-consumer pipeline
- PUB/SUB sockets for multi-consumer broadcast
- CF64 complex I/Q sample streaming
- TimeoutError handling on recv
"""

import threading
import time

import numpy as np

from doppler import CF64, Publisher, Pull, Push, Subscriber


# ---------------------------------------------------------------------------
# Signal generation
# ---------------------------------------------------------------------------


def generate_chirp(
    duration_sec: float,
    sample_rate: float = 1e6,
    f_start: float = 1e6,
    f_end: float = 10e6,
) -> np.ndarray:
    """Return a complex chirp sweeping from f_start to f_end."""
    n = int(duration_sec * sample_rate)
    t = np.arange(n) / sample_rate
    phase = 2 * np.pi * (f_start * t + (f_end - f_start) * t**2 / (2 * duration_sec))
    return np.exp(1j * phase).astype(np.complex128)


# ---------------------------------------------------------------------------
# PUSH / PULL demo
# ---------------------------------------------------------------------------


def producer(endpoint: str, num_batches: int = 50, batch_size: int = 1024):
    """Send CF64 batches over a PUSH socket."""
    sock = Push(endpoint, CF64)
    print(f"[Producer] Bound to {endpoint}")
    signal = generate_chirp(duration_sec=1.0, sample_rate=1e6)

    try:
        for batch_id in range(num_batches):
            start = (batch_id * batch_size) % len(signal)
            batch = signal[start : start + batch_size]
            if len(batch) < batch_size:
                batch = np.concatenate([batch, signal[: batch_size - len(batch)]])
            sock.send(batch, sample_rate=1_000_000, center_freq=0)
            if (batch_id + 1) % 10 == 0:
                print(f"[Producer] Sent batch {batch_id + 1}/{num_batches}")
            time.sleep(0.002)
    finally:
        print("[Producer] Done")
        sock.close()


def consumer(endpoint: str, timeout_sec: float = 30.0):
    """Receive CF64 batches from a PULL socket."""
    sock = Pull(endpoint)
    print(f"[Consumer] Connected to {endpoint}")

    deadline = time.time() + timeout_sec
    batches = 0
    total = 0
    powers = []

    while time.time() < deadline:
        try:
            samples, _hdr = sock.recv(timeout_ms=500)
            power = float(np.mean(np.abs(samples) ** 2))
            powers.append(power)
            total += len(samples)
            batches += 1
            if batches % 10 == 0:
                avg_db = 10 * np.log10(np.mean(powers[-10:]))
                print(f"[Consumer] {batches} batches  avg power = {avg_db:.2f} dB")
        except TimeoutError:
            break
        except Exception as exc:
            print(f"[Consumer] Error: {exc}")
            break

    print(f"[Consumer] Received {batches} batches, {total} samples total")
    if powers:
        avg_db = 10 * np.log10(np.mean(powers))
        print(f"[Consumer] Overall average power = {avg_db:.2f} dB")
    sock.close()


def demo_push_pull():
    """PUSH/PULL (producer-consumer pipeline)."""
    print("\n" + "=" * 60)
    print("Demo 1: PUSH/PULL (Producer-Consumer)")
    print("=" * 60 + "\n")

    endpoint = "tcp://127.0.0.1:15555"
    connect = endpoint.replace("127.0.0.1", "localhost")

    prod = threading.Thread(target=producer, args=(endpoint, 50, 1024), ddpmon=False)
    cons = threading.Thread(target=consumer, args=(connect, 30.0), ddpmon=False)

    prod.start()
    time.sleep(0.3)
    cons.start()

    prod.join(timeout=60)
    cons.join(timeout=35)


# ---------------------------------------------------------------------------
# PUB / SUB demo
# ---------------------------------------------------------------------------


def broadcaster(pub_endpoint: str, num_messages: int = 50):
    """Publish CF64 batches to all subscribers."""
    sock = Publisher(pub_endpoint, CF64)
    print(f"[Broadcaster] Bound to {pub_endpoint}")
    print("[Broadcaster] Waiting 2 s for subscribers to connect...")
    time.sleep(2.0)

    signal = generate_chirp(duration_sec=0.5, sample_rate=1e6)

    for msg_id in range(num_messages):
        start = (msg_id * 100) % len(signal)
        batch = signal[start : start + 100]
        if len(batch) < 100:
            batch = np.pad(batch, (0, 100 - len(batch)), mode="wrap")
        sock.send(batch, sample_rate=1_000_000, center_freq=0)
        if (msg_id + 1) % 10 == 0:
            print(f"[Broadcaster] Published {msg_id + 1}/{num_messages}")
        time.sleep(0.01)

    # Send empty sentinel so subscribers can exit cleanly
    for _ in range(3):
        sock.send(np.array([], dtype=np.complex128), sample_rate=0, center_freq=0)
        time.sleep(0.05)

    print("[Broadcaster] Done")
    sock.close()


def subscriber_worker(
    sock: Subscriber,
    worker_id: int,
    stop_event: threading.Event,
    timeout_sec: float = 15.0,
):
    """Receive from a shared Subscriber socket until stop or timeout."""
    print(f"[Sub-{worker_id}] Started")
    deadline = time.time() + timeout_sec
    received = 0

    while time.time() < deadline and not stop_event.is_set():
        try:
            samples, _hdr = sock.recv(timeout_ms=200)
            if len(samples) == 0:
                print(f"[Sub-{worker_id}] Received shutdown sentinel")
                break
            power = 10 * np.log10(np.mean(np.abs(samples) ** 2) + 1e-10)
            received += 1
            print(
                f"[Sub-{worker_id}] msg {received}: "
                f"{len(samples)} samples  power = {power:.2f} dB"
            )
        except TimeoutError:
            continue
        except Exception:
            break

    print(f"[Sub-{worker_id}] Received {received} messages")


def demo_pub_sub():
    """PUB/SUB (broadcast to multiple subscribers)."""
    print("\n" + "=" * 60)
    print("Demo 2: PUB/SUB (Broadcast)")
    print("=" * 60 + "\n")

    endpoint = "tcp://127.0.0.1:15556"
    connect = endpoint.replace("127.0.0.1", "localhost")

    stop_event = threading.Event()
    sub_socks = [Subscriber(connect) for _ in range(2)]
    sub_threads = [
        threading.Thread(
            target=subscriber_worker,
            args=(sub_socks[idx], idx, stop_event, 15.0),
            ddpmon=False,
        )
        for idx in range(len(sub_socks))
    ]

    for t in sub_threads:
        t.start()
    time.sleep(1.0)

    pub_thread = threading.Thread(target=broadcaster, args=(endpoint, 50), ddpmon=False)
    pub_thread.start()
    pub_thread.join()

    stop_event.set()
    for s in sub_socks:
        try:
            s.close()
        except Exception:
            pass
    for t in sub_threads:
        t.join(timeout=3.0)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    print("╔══════════════════════════════════════════════════════════╗")
    print("║  doppler IQ Signal Streaming Demo                        ║")
    print("╚══════════════════════════════════════════════════════════╝")
    try:
        demo_push_pull()
        demo_pub_sub()
        print("\n" + "=" * 60)
        print("All demos completed successfully!")
        print("=" * 60 + "\n")
    except KeyboardInterrupt:
        print("\nInterrupted by user")
    except Exception as exc:
        print(f"Error: {exc}")
        import traceback

        traceback.print_exc()
