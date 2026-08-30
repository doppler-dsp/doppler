#!/usr/bin/env python3
"""The `DP_WORK_*` JetStream streams doppler leaves behind: count, or delete.

Every `Push(endpoint)` provisions a durable work-queue stream named
`DP_WORK_<endpoint>`, and nothing has ever removed one. `make nats-down`
cleans up a broker THIS repo started -- it kills the pidfile's process,
removes the container, and deletes its own temp store -- but
`scripts/start-nats.sh` deliberately REUSES a broker that is already
listening on 4222, and on that path there is no pidfile, no container and
no temp store, so `nats-down` does nothing at all and the streams stay.

CI never noticed, because a CI runner's broker is discarded with the job.
A dev box keeps the same broker for months. Measured on one 2026-08-30:
**5,236 leftover streams, 4,888 consumers, 40.8 GiB**, the oldest frames
dating from 2026-07-08 and carrying the retired `SGIS` wire magic that no
current build can parse.

That is not merely disk. A work queue is keyed by endpoint, and endpoints
repeat -- `dp-chain-5601` is whatever compose allocated port 5601 to, this
run and every previous one. So a new run opens onto the PREVIOUS era's
queue: `DP_WORK_dp-chain-5601` held 174,133 unreadable frames, and
`test_three_block_chain_spawns_and_moves_data` failed instantly and
permanently on this machine while passing in CI. The library is not at
fault -- `nats_recv_signal` rejects the frame and Terms it, exactly as
designed -- but one recv per run against a 174k backlog never gets to a
good frame.

Two modes, one home for the logic:

    nats_streams.py --check [--max N]   count them; fail above N
    nats_streams.py --delete            delete every DP_WORK_* stream

`--delete` is prefix-guarded: anything not named `DP_WORK_*` is reported
and left alone, so a stream belonging to something else on a shared
broker cannot be removed by accident.

Stdlib only, speaking the NATS wire protocol over a socket -- there is no
NATS client in this project's dependency tree and adding one to clean up
after ourselves would be its own kind of debt.
"""

from __future__ import annotations

import argparse
import contextlib
import json
import socket
import sys
import time

HOST = "127.0.0.1"
PORT = 4222

#: Every stream doppler creates is named for its endpoint under this
#: prefix (`nats_ensure_stream` in native/src/stream/stream_nats.c).
PREFIX = "DP_WORK_"

#: What `--check` tolerates by default, derived from a measurement rather
#: than picked: one full `make test-python` leaves **20** streams behind
#: (counted 2026-08-30, immediately after a 3,298-test run). So 500 is
#: about 25 suite runs without a `make nats-down` in between -- loose
#: enough that ordinary work never trips it, tight enough that it fires
#: an order of magnitude before the 5,236 streams / 40.8 GiB that this
#: exists to prevent.
#:
#: Not zero, and not 64: a threshold that fires every third suite run is
#: read as a false alarm and then ignored, which is worse than no gate.
#: The number to watch is runaway growth, not the streams of the run in
#: flight.
DEFAULT_MAX = 500


class NatsError(RuntimeError):
    """The broker could not be reached or spoke unexpectedly."""


class Nats:
    """A minimal NATS request/reply client, enough for the JetStream API."""

    def __init__(
        self, host: str = HOST, port: int = PORT, timeout: float = 10.0
    ):
        try:
            self._sock = socket.create_connection(
                (host, port), timeout=timeout
            )
        except OSError as e:
            raise NatsError(f"no broker on {host}:{port}: {e}") from e
        self._sock.settimeout(30.0)
        self._buf = b""
        self._readline()  # the server's INFO greeting
        self._sock.sendall(b'CONNECT {"verbose":false,"pedantic":false}\r\n')
        self._sock.sendall(b"SUB _INBOX.dpstreams 1\r\n")

    def close(self) -> None:
        with contextlib.suppress(OSError):
            self._sock.close()

    def __enter__(self) -> Nats:
        return self

    def __exit__(self, *exc: object) -> None:
        self.close()

    def _fill(self) -> None:
        chunk = self._sock.recv(1 << 20)
        if not chunk:
            raise NatsError("broker closed the connection")
        self._buf += chunk

    def _readline(self) -> bytes:
        while b"\r\n" not in self._buf:
            self._fill()
        line, _, self._buf = self._buf.partition(b"\r\n")
        return line

    def _read(self, n: int) -> bytes:
        while len(self._buf) < n:
            self._fill()
        out, self._buf = self._buf[:n], self._buf[n:]
        return out

    def request(self, subject: str, payload: str = "{}") -> dict | None:
        """One JetStream API request; None if no reply arrives in time."""
        body = payload.encode()
        self._sock.sendall(
            f"PUB {subject} _INBOX.dpstreams {len(body)}\r\n".encode()
            + body
            + b"\r\n"
        )
        deadline = time.monotonic() + 30.0
        while time.monotonic() < deadline:
            line = self._readline()
            if line.startswith(b"PING"):
                self._sock.sendall(b"PONG\r\n")
                continue
            if line.startswith(b"MSG "):
                n = int(line.split()[-1])
                data = self._read(n)
                self._read(2)  # the payload's trailing CRLF
                return json.loads(data)
        return None

    def stream_names(self) -> list[str]:
        """Every stream on the broker, following the API's paging."""
        names: list[str] = []
        offset = 0
        while True:
            page = self.request(
                "$JS.API.STREAM.NAMES", json.dumps({"offset": offset})
            )
            if not page:
                raise NatsError("$JS.API.STREAM.NAMES did not reply")
            got = page.get("streams") or []
            if not got:
                break
            names.extend(got)
            offset += len(got)
            if offset >= (page.get("total") or 0):
                break
        return names

    def account(self) -> dict:
        return self.request("$JS.API.INFO") or {}


def partition(
    names: list[str], prefix: str = PREFIX
) -> tuple[list[str], list[str]]:
    """Split into (ours, everyone else's). The safety guard, isolated so it
    can be tested without a broker.

    A plain prefix, never a substring: `backup-DP_WORK_x` is somebody's
    backup, not ours.
    """
    ours = [n for n in names if n.startswith(prefix)]
    theirs = [n for n in names if not n.startswith(prefix)]
    return ours, theirs


def narrowed(prefix: str) -> str:
    """Validate a caller-supplied prefix, which may only NARROW the default.

    `--prefix` exists so a test can operate on its own streams while the
    rest of the suite runs in parallel against the same broker -- an
    unscoped `--delete` would remove the work queues of whatever else is
    mid-flight. Since the whole point of the guard is that this tool
    cannot reach another owner's data, a prefix that does not itself start
    with the default is refused rather than honoured.
    """
    if not prefix.startswith(PREFIX):
        raise ValueError(
            f"--prefix must start with {PREFIX!r} (got {prefix!r}); it may "
            "narrow the guard, never widen it"
        )
    return prefix


def _report(nc: Nats) -> tuple[int, float]:
    info = nc.account()
    gib = (info.get("storage") or 0) / (1024**3)
    return int(info.get("streams") or 0), gib


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--check",
        action="store_true",
        help="count leftovers; fail above --max",
    )
    mode.add_argument(
        "--delete", action="store_true", help=f"delete every {PREFIX}* stream"
    )
    ap.add_argument("--max", type=int, default=DEFAULT_MAX)
    ap.add_argument(
        "--prefix",
        default=PREFIX,
        help=f"narrow the guard below {PREFIX}* (a test scoping itself so a "
        "parallel suite's queues are untouched); may not widen it",
    )
    ap.add_argument(
        "--quiet-when-absent",
        action="store_true",
        help="exit 0 when no broker is listening (teardown after a run that "
        "never started one)",
    )
    args = ap.parse_args()

    try:
        prefix = narrowed(args.prefix)
    except ValueError as e:
        print(f"nats-streams: {e}", file=sys.stderr)
        return 2

    try:
        nc = Nats()
    except NatsError as e:
        if args.quiet_when_absent:
            print(f"nats-streams: no broker — nothing to do ({e})")
            return 0
        print(f"nats-streams: {e}", file=sys.stderr)
        return 2

    with nc:
        try:
            names = nc.stream_names()
        except NatsError as e:
            # JetStream disabled is not the same as "clean", and saying
            # "OK" here would be the absent-output-is-a-pass mistake.
            print(f"nats-streams: cannot list streams: {e}", file=sys.stderr)
            return 2

        ours, theirs = partition(names, prefix)
        _total, gib = _report(nc)

        if args.check:
            print(
                f"nats-streams: {len(ours)} {prefix}* stream(s), "
                f"{len(theirs)} other, {gib:.1f} GiB total"
            )
            if len(ours) > args.max:
                print(
                    f"\n{len(ours)} leftover work queues (limit {args.max}). "
                    "Nothing removes these:\n"
                    "  `make nats-down` only cleans a broker this repo "
                    "started, and start-nats.sh\n"
                    "  reuses one already listening on 4222. A reused "
                    "endpoint then opens onto the\n"
                    "  PREVIOUS run's queue -- which is how a compose test "
                    "came to fail on 174,133\n"
                    "  frames in a retired wire format. Run `make "
                    "nats-purge`."
                )
                return 1
            return 0

        if theirs:
            print(
                f"nats-streams: leaving {len(theirs)} non-{prefix} "
                f"stream(s) alone: {theirs[:10]}"
            )
        if not ours:
            print(f"nats-streams: no {prefix}* streams to delete")
            return 0

        print(f"nats-streams: deleting {len(ours)} stream(s) ({gib:.1f} GiB)")
        ok = 0
        failed: list[str] = []
        for name in ours:
            reply = nc.request(f"$JS.API.STREAM.DELETE.{name}")
            if reply and reply.get("success"):
                ok += 1
            else:
                failed.append(name)

        after, after_gib = _report(nc)
        print(
            f"nats-streams: deleted {ok}, failed {len(failed)}; "
            f"{after} stream(s) / {after_gib:.1f} GiB remain"
        )
        if failed:
            print(f"  failed: {failed[:10]}", file=sys.stderr)
            return 1
        return 0


if __name__ == "__main__":
    sys.exit(main())
