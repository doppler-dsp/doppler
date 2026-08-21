#!/usr/bin/env bash
# Start a NATS JetStream broker on 127.0.0.1:4222 and wait until it accepts
# connections. The stream suite's nats:// tests self-skip when the port is
# unreachable, so CI starts a broker to actually exercise them (a GitHub
# service container can't pass the `-js` JetStream flag, so it runs explicitly).
# One definition, shared by every ci.yml job that needs the broker.
#
# TWO WAYS TO GET A BROKER, and the binary is preferred deliberately. CI now
# runs its Linux jobs INSIDE a container (deploy/docker/Dockerfile.ci), where
# there is no docker daemon to run a sibling container with -- so a
# docker-only script would leave 127.0.0.1:4222 unreachable and the nats://
# tests would SELF-SKIP. That is the bad failure: the suite stays green while
# silently dropping the whole nats path, and the coverage number drops with
# it. The image therefore carries nats-server, and this prefers it.
#
# The docker path stays for a dev box without the binary, so the script's
# contract -- "a JetStream broker is listening on 4222 when I exit 0" -- is
# the same either way. A box with neither now says so and fails, rather than
# leaving the caller to discover an absent broker as a skipped test.
set -euo pipefail

PIDFILE="${NATS_PIDFILE:-${TMPDIR:-/tmp}/doppler-nats.pid}"

_listening() { (exec 3<>/dev/tcp/127.0.0.1/4222) 2>/dev/null; }

# Someone is already serving 4222 -- a dev box running a system nats-server,
# or a previous run that was not torn down. Reuse it and say so.
#
# This case is not hypothetical and it is why the check is here: this script
# was verified on a box with a system broker on 4222, where the spawn below
# could not bind, exited immediately, and the port check still passed because
# it was reading SOMEONE ELSE'S listener. The report said ready, and it was
# ready -- just not because of anything this script did. A start script that
# cannot tell those apart cannot be trusted to fail either.
if _listening; then
  echo "NATS broker already listening on 127.0.0.1:4222 — reusing it"
  exit 0
fi

if command -v nats-server >/dev/null 2>&1; then
  # -a 127.0.0.1 keeps it off every other interface: this is a test fixture,
  # and on a shared runner an all-interfaces bind is a listening service
  # nobody asked for. -sd under TMPDIR because JetStream needs a store dir
  # and the checkout is bind-mounted -- writing stream state into it would
  # show up as untracked files in the tree the gates then lint.
  store="${TMPDIR:-/tmp}/doppler-nats-store"
  # What this needs is an EMPTY store, not one particular unlink succeeding.
  # A previous broker that is still shutting down keeps writing stream state
  # for a while after it stops listening, so an `rm -rf` here can lose the
  # same race `nats-down` does and fail with "Directory not empty" -- and
  # under `set -e` that aborts the start outright. Retry, then insist.
  rm -rf "$store" 2>/dev/null || { sleep 1; rm -rf "$store" 2>/dev/null; } || :
  if [ -e "$store" ]; then
    echo "start-nats: $store could not be cleared -- something is still" >&2
    echo "  writing to it. Run 'make nats-down' and try again." >&2
    exit 1
  fi
  mkdir -p "$store"
  nats-server -a 127.0.0.1 -p 4222 -js -sd "$store" \
      >"${TMPDIR:-/tmp}/doppler-nats.log" 2>&1 &
  echo $! >"$PIDFILE"
  how="nats-server $(nats-server --version | awk '{print $NF}') (binary)"
elif command -v docker >/dev/null 2>&1; then
  docker run -d --name nats -p 4222:4222 nats:2.10 -js
  how="nats:2.10 (docker)"
else
  echo "start-nats: neither nats-server nor docker is available, so no" >&2
  echo "  broker can be started. The nats:// tests would self-skip, which" >&2
  echo "  reads as success -- failing here instead." >&2
  exit 1
fi

for _ in $(seq 1 50); do
  _listening && break || sleep 0.2
done
# Fail-closed: the loop above exits after 50 tries whether or not anything is
# listening, so without this a broker that never came up still printed
# "ready" and every nats:// test then self-skipped.
if ! _listening; then
  echo "start-nats: nothing is listening on 127.0.0.1:4222 after 10s" >&2
  [ -f "${TMPDIR:-/tmp}/doppler-nats.log" ] \
      && tail -20 "${TMPDIR:-/tmp}/doppler-nats.log" >&2
  exit 1
fi
echo "NATS JetStream broker ready on 127.0.0.1:4222 — $how"
