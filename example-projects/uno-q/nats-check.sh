#!/usr/bin/env bash
# nats-check.sh — the uno-q NATS transport, both patterns, end to end.
#
#   nats-check.sh BUILD_DIR [NATS_URL]      (default nats://127.0.0.1:4222)
#
# Needs a JetStream broker at NATS_URL (`nats-server -js`; in doppler's CI,
# `make nats-up`). A broker that is not there is a FAILURE, not a skip: a
# check that could not run has not passed.
#
# 1. PUB/SUB (the default pattern). The receiver subscribes first and says
#    so; the publisher then sends one second of the self-test scene, paced
#    at real time, and an end-of-stream frame. The receiver must get every
#    frame (none lost, none repeated, starting at sequence 0) and hold the
#    data to the self-test's own checks.
# 2. PUSH/PULL (JetStream). The publisher sends first -- the broker keeps
#    it, so no subscriber has to be listening -- and the receiver pulls it
#    all with a deliberate stall mid-stream. Every frame must still arrive,
#    once: at-least-once delivery may REPEAT a frame, and the receiver's
#    duplicate count must stay 0 on this clean run.
set -euo pipefail

bin=${1:?usage: nats-check.sh BUILD_DIR [NATS_URL]}
url=${2:-nats://127.0.0.1:4222}
fs=2400000
frame=32768
# ceil(fs * 1 s / frame): the frames one second of the scene makes.
frames=$(( (fs + frame - 1) / frame ))
tag="unoq_check_$$"
log=$(mktemp -d)
trap 'rm -rf "$log"' EXIT

for exe in uno_q_shared uno_q_pub; do
    [ -x "$bin/$exe" ] || {
        echo "nats-check: $bin/$exe missing -- was the doppler install built" \
             "with its stream component (doppler::stream)?"
        exit 1
    }
done

echo "── pub/sub: $url/${tag}_sub, $frames frames expected ──"
"$bin/uno_q_shared" --nats "$url/${tag}_sub" --check \
    --expect-frames "$frames" > "$log/sub.out" 2>&1 &
rx=$!
# The subscriber must be listening before anything is published, or the
# first frames were never offered to it: wait for it to say so.
for _ in $(seq 100); do
    grep -q '^ready:' "$log/sub.out" 2>/dev/null && break
    kill -0 "$rx" 2>/dev/null || break
    sleep 0.1
done
grep -q '^ready:' "$log/sub.out" || {
    cat "$log/sub.out"; echo "nats-check: subscriber never became ready"
    kill "$rx" 2>/dev/null || true; exit 1
}
"$bin/uno_q_pub" --nats "$url/${tag}_sub" --synthetic 1 --fs "$fs" \
    --frame "$frame"
rc=0; wait "$rx" || rc=$?
cat "$log/sub.out"
[ "$rc" -eq 0 ] || { echo "nats-check: pub/sub FAILED"; exit 1; }

echo "── push/pull: $url/${tag}_pull, stall 1500 ms mid-stream ──"
"$bin/uno_q_pub" --nats "$url/${tag}_pull" --pattern push --synthetic 1 \
    --fs "$fs" --frame "$frame"
"$bin/uno_q_shared" --nats "$url/${tag}_pull" --pattern pull --check \
    --expect-frames "$frames" --stall-ms 1500 \
    || { echo "nats-check: push/pull FAILED"; exit 1; }

echo "nats-check: OK -- both patterns, no frame lost or repeated"
