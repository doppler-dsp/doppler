#!/usr/bin/env bash
# LEGACY: pre-split-TOML bootstrap history. Objects now live in objects/*.toml.
# To regenerate: edit objects/<obj>.toml then: jm apply objects/<obj>.toml
# stream_scaffold.sh — declare the stream module interface via just-makeit.
#
# Run from: doppler-jm/ (project root, must contain just-makeit.toml)
#
# What stream is
# --------------
# ZMQ-based IQ streaming over named endpoints.  Supports five patterns:
#   PUB/SUB  — broadcast one-to-many
#   PUSH/PULL — pipeline (work queue)
#   REQ/REP  — request-reply
#
# Wire format: ZMQ two-frame multipart (dp_header_t frame + data frame).
# Three sample types: CI32, CF64, CF128 (int32 IQ, cf64, cf128).
#
# Types exported
# --------------
#   Publisher(endpoint, sample_type)  — ZMQ PUB, send()
#   Subscriber(endpoint)              — ZMQ SUB, recv() → (array, header)
#   Push(endpoint, sample_type)       — ZMQ PUSH, send()
#   Pull(endpoint)                    — ZMQ PULL, recv() → (array, header)
#   Requester(endpoint, sample_type)  — ZMQ REQ, send() + recv()
#   Replier(endpoint, sample_type)    — ZMQ REP, recv() + send()
#
# Module-level constants: CI32, CF64, CF128
# Module-level function:  get_timestamp_ns() -> int
#
# Dependencies
# ------------
# vendor/libzmq — vendored static ZMQ (no runtime libzmq.so dep).
# CMakeLists.txt must declare zmq_vendor_static before this subdirectory.
#
# Generated (skeleton only — hand-restore stream_ext.c + stream_core.c from git):
#   native/src/stream/CMakeLists.txt  (overwrites — can copy from root)
#   src/doppler/stream/__init__.py
#   src/doppler/stream/stream.pyi
#   src/doppler/stream/tests/test_stream.py
#   CMakeLists.txt                    (add_subdirectory entry)
#   just-makeit.toml                  (updated)
#
# No --impl possible
# ------------------
# stream_ext.c is a monolithic 934-line file; there is no stream_core.h/c
# with extractable function bodies (stream_core.c provides dp_msg_t and
# ZMQ socket wrappers that are called by stream_ext.c internals, not public
# functions suitable for --impl lifting).
#
# Hand-edit after scaffolding
# ---------------------------
# stream_ext.c: fully hand-crafted.  The jm skeleton provides struct shells;
#   restore the full file from git history.
# stream_core.c: restore from git history.
# CMakeLists.txt: link zmq_vendor_static + stdc++ + m + pthread.
#   Ensure add_subdirectory(native/src/stream) comes AFTER the
#   zmq_vendor_static cmake block in root CMakeLists.txt.

set -euo pipefail

JM="${JUST_MAKEIT:-just-makeit}"

# ── Module ────────────────────────────────────────────────────────────────────
$JM module stream

# ── Publisher — ZMQ PUB socket ───────────────────────────────────────────────
$JM object publisher \
    --module stream \
    --class-name Publisher \
    --no-state \
    --no-step \
    --init-param "endpoint:char *" \
    --init-param "sample_type:int"

$JM method publisher send \
    --module stream \
    --param "samples:float _Complex[]" \
    --param "sample_rate:double" \
    --param "center_freq:double"

$JM method publisher close \
    --module stream

# ── Subscriber — ZMQ SUB socket ──────────────────────────────────────────────
$JM object subscriber \
    --module stream \
    --class-name Subscriber \
    --no-state \
    --no-step \
    --init-param "endpoint:char *"

# recv() returns (ndarray, header_dict) — cannot express as jm return type.
# Hand-edit the generated stub to return a tuple.
$JM method subscriber recv \
    --module stream \
    --param "timeout_ms:int" \
    --return-type "float _Complex" \
    --variable-output  # placeholder; hand-edit to return (ndarray, dict)

$JM method subscriber close \
    --module stream

# ── Push — ZMQ PUSH socket ───────────────────────────────────────────────────
$JM object push \
    --module stream \
    --class-name Push \
    --no-state \
    --no-step \
    --init-param "endpoint:char *" \
    --init-param "sample_type:int"

$JM method push send \
    --module stream \
    --param "samples:float _Complex[]" \
    --param "sample_rate:double" \
    --param "center_freq:double"

$JM method push close \
    --module stream

# ── Pull — ZMQ PULL socket ───────────────────────────────────────────────────
$JM object pull \
    --module stream \
    --class-name Pull \
    --no-state \
    --no-step \
    --init-param "endpoint:char *"

$JM method pull recv \
    --module stream \
    --param "timeout_ms:int" \
    --return-type "float _Complex" \
    --variable-output  # placeholder; hand-edit to return (ndarray, dict)

$JM method pull close \
    --module stream

# ── Requester — ZMQ REQ socket ───────────────────────────────────────────────
$JM object requester \
    --module stream \
    --class-name Requester \
    --no-state \
    --no-step \
    --init-param "endpoint:char *" \
    --init-param "sample_type:int"

$JM method requester send \
    --module stream \
    --param "samples:float _Complex[]" \
    --param "sample_rate:double" \
    --param "center_freq:double"

$JM method requester recv \
    --module stream \
    --param "timeout_ms:int" \
    --return-type "float _Complex" \
    --variable-output  # placeholder; hand-edit to return (ndarray, dict)

$JM method requester close \
    --module stream

# ── Replier — ZMQ REP socket ─────────────────────────────────────────────────
$JM object replier \
    --module stream \
    --class-name Replier \
    --no-state \
    --no-step \
    --init-param "endpoint:char *" \
    --init-param "sample_type:int"

$JM method replier recv \
    --module stream \
    --param "timeout_ms:int" \
    --return-type "float _Complex" \
    --variable-output  # placeholder; hand-edit to return (ndarray, dict)

$JM method replier send \
    --module stream \
    --param "samples:float _Complex[]" \
    --param "sample_rate:double" \
    --param "center_freq:double"

$JM method replier close \
    --module stream

echo
echo "stream scaffold done."
echo "Restore native/src/stream/stream_ext.c and stream_core.c from git."
echo "Hand-edit all recv() methods to return (ndarray, dict) tuples."
echo "Verify zmq_vendor_static block precedes add_subdirectory(native/src/stream)"
echo "in root CMakeLists.txt."
