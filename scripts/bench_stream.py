#!/usr/bin/env python3
"""Transport (P0) benchmark: NATS throughput + status-plane latency.

The DSP benchmarks measure the kernels; this measures the *wire*. It drives the
hand-written ``bench_stream`` C harness (``native/benchmarks/bench_stream.c``)
over NATS and records one JSON snapshot:

* **firehose** — PUSH/PULL of a large CF32 block, the data-plane figure of
  merit (throughput), over the durable JetStream work-queue tier
  (synchronous server-acked publish + explicit-ack pull). JetStream is
  store-and-forward, so the harness's one-way latency degenerates to queue
  residency and is not meaningful here (see the status-plane RTT instead).
* **reqrep** — the status/control plane: unloaded small-message REQ/REP
  round-trip latency.

The harness reads ``DP_BENCH_FIREHOSE_EP`` / ``DP_BENCH_REQREP_EP`` for the
``nats://`` endpoint. We lean on large blocks only (DSP pipelines always do)
rather than a block-size sweep.

Requires ``nats-server`` on PATH — started here on an isolated port + temp
store and torn down after, so the run is self-contained and reproducible.

Historical note: releases through v0.27.0 also measured a ZMQ backend
(brokerless TCP loopback), recorded under a "zmq" key in the older
``benchmarks/published/*/stream.json`` snapshots. ZMQ was removed in favor of
NATS; this script and its JSON output are NATS-only going forward.

Usage::

    make bench-stream                       # scratch JSON, prints a table
    python scripts/bench_stream.py --publish 0.21.0   # -> published/v0.21.0/
    python scripts/bench_stream.py --block 65536 -k 5 --pings 4000
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench_report import collect_meta

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BIN = os.path.join(REPO, "build", "native", "benchmarks", "bench_stream")
NATS_PORT = 4223  # isolated from a developer's default-4222 broker


def _build() -> None:
    subprocess.run(
        ["cmake", "--build", "build", "--target", "bench_stream"],
        cwd=REPO,
        check=True,
        stdout=subprocess.DEVNULL,
    )


def _port_open(port: int, host: str = "127.0.0.1") -> bool:
    try:
        socket.create_connection((host, port), timeout=0.3).close()
        return True
    except OSError:
        return False


def _run(args, env_extra=None) -> list[str]:
    """Run bench_stream; return the data row (2nd TSV line) split on tabs."""
    env = dict(os.environ)
    if env_extra:
        env.update(env_extra)
    out = subprocess.run(
        [BIN, *args],
        cwd=REPO,
        env=env,
        capture_output=True,
        text=True,
        timeout=180,
    )
    rows = [ln for ln in out.stdout.splitlines() if ln and "\t" in ln]
    if len(rows) < 2:
        raise RuntimeError(
            f"bench_stream {args} produced no data row:\n"
            f"stdout={out.stdout!r}\nstderr={out.stderr!r}"
        )
    return rows[1].split("\t")


def _firehose(block, blocks, ep=None):
    env = {"DP_BENCH_FIREHOSE_EP": ep} if ep else None
    f = _run([str(block), str(blocks)], env)
    # block_sz num_blocks tput_mss tput_mbs lat_min lat_mean lat_p99 lat_max
    return {
        "tput_msa_s": float(f[2]),
        "tput_mb_s": float(f[3]),
        "lat_mean_us": float(f[5]),
        "lat_p99_us": float(f[6]),
    }


def _reqrep(pings, ep=None):
    env = {"DP_BENCH_REQREP_EP": ep} if ep else None
    r = _run(["reqrep", str(pings)], env)
    # msg_bytes pings rtt_min rtt_mean rtt_p99 rtt_max oneway_mean
    return {
        "rtt_mean_us": float(r[3]),
        "rtt_p99_us": float(r[4]),
        "oneway_mean_us": float(r[6]),
    }


def _best(samples, key, prefer_max):
    """Keep the least-interfered sample by `key` (max throughput / min RTT)."""
    return (max if prefer_max else min)(samples, key=lambda s: s[key])


def measure(block, blocks, pings, passes, nats_url):
    """{transport: {firehose, reqrep}}; `nats_url` is required."""
    fh, rr = [], []
    for i in range(passes):
        # Fresh subject base per pass -> a clean JetStream stream each run,
        # no cross-pass residue (matches the test suite convention).
        tag = f"{os.getpid()}_{i}"
        f = _firehose(block, blocks, f"{nats_url}/fh{tag}")
        f["lat_mean_us"] = None  # store-and-forward: latency is residency
        f["lat_p99_us"] = None
        fh.append(f)
        rr.append(_reqrep(pings, f"{nats_url}/rr{tag}"))
    return {
        "nats": {
            "firehose": _best(fh, "tput_msa_s", True),
            "reqrep": _best(rr, "rtt_mean_us", False),
        }
    }


def _print_table(transports, cfg):
    def g(sec, k):
        v = transports.get("nats", {}).get(sec, {}).get(k)
        return "—" if v is None else f"{v:,.1f}"

    print(
        f"\nTransport P0 — NATS  (block={cfg['block_size']} CF32, "
        f"{cfg['num_blocks']} frames, {cfg['passes']} passes best-of)\n"
    )
    print(f"  {'metric':<28}{'NATS (JS)':>12}")
    print(f"  {'-' * 40}")
    rows = [
        ("firehose tput  MSa/s", "firehose", "tput_msa_s"),
        ("firehose tput  MB/s", "firehose", "tput_mb_s"),
        ("reqrep RTT mean  µs", "reqrep", "rtt_mean_us"),
        ("reqrep RTT p99   µs", "reqrep", "rtt_p99_us"),
    ]
    for label, sec, k in rows:
        print(f"  {label:<28}{g(sec, k):>12}")
    print()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "--block",
        type=int,
        default=65536,
        help="CF32 samples per firehose frame (default 65536)",
    )
    ap.add_argument(
        "--blocks",
        type=int,
        default=600,
        help="firehose frames per pass (default 600)",
    )
    ap.add_argument(
        "--pings",
        type=int,
        default=2000,
        help="reqrep round trips per pass (default 2000)",
    )
    ap.add_argument(
        "-k",
        "--passes",
        type=int,
        default=3,
        help="passes per measurement; best is kept (default 3)",
    )
    ap.add_argument(
        "--publish",
        metavar="VERSION",
        default=None,
        help="write benchmarks/published/v<VERSION>/stream.json",
    )
    ap.add_argument("--out", default=None, help="explicit output path")
    a = ap.parse_args()

    _build()

    nats_bin = shutil.which("nats-server")
    if not nats_bin:
        print(
            "error: nats-server not found on PATH -- required (ZMQ was "
            "removed, NATS is the only transport)",
            file=sys.stderr,
        )
        return 1

    proc, store, nats_url = None, None, None
    if not _port_open(NATS_PORT):
        store = tempfile.mkdtemp(prefix="dp-bench-natsjs-")
        proc = subprocess.Popen(
            [
                nats_bin,
                "-js",
                "-sd",
                store,
                "-a",
                "127.0.0.1",
                "-p",
                str(NATS_PORT),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        for _ in range(50):
            if _port_open(NATS_PORT):
                break
            time.sleep(0.1)
        nats_url = f"nats://127.0.0.1:{NATS_PORT}"
    else:
        nats_url = f"nats://127.0.0.1:{NATS_PORT}"  # reuse a running broker

    try:
        transports = measure(a.block, a.blocks, a.pings, a.passes, nats_url)
    finally:
        if proc:
            proc.terminate()
            proc.wait(timeout=10)
        if store:
            shutil.rmtree(store, ignore_errors=True)

    cfg = {
        "block_size": a.block,
        "num_blocks": a.blocks,
        "reqrep_pings": a.pings,
        "passes": a.passes,
    }
    commit = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"],
        cwd=REPO,
        capture_output=True,
        text=True,
    ).stdout.strip()
    snapshot = {
        "doppler_meta": collect_meta({}, "", "", commit, ""),
        "config": cfg,
        "transports": transports,
    }

    _print_table(transports, cfg)

    dst = a.out
    if a.publish:
        ver = "v" + a.publish.lstrip("v")
        d = os.path.join(REPO, "benchmarks", "published", ver)
        os.makedirs(d, exist_ok=True)
        dst = os.path.join(d, "stream.json")
    if dst:
        with open(dst, "w") as fh:
            json.dump(snapshot, fh, indent=1)
            fh.write("\n")
        print(f"wrote {os.path.relpath(dst, REPO)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
