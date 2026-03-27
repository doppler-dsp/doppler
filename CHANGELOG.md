# Changelog

Development history moved from CLAUDE.md for readability.

## Test Coverage Expansion

- **C streaming tests** (`c/tests/test_stream.c`): Expanded from
  8 to 26 tests covering:
  - Utilities: sample types, timestamps, error codes
  - dp_msg_t: NULL safety for all accessors
  - PUB/SUB: CF64/CI32/CF128 roundtrips, multiple subscribers,
    sequence numbering
  - SUB timeout: verifies DP_ERR_TIMEOUT return
  - PUSH/PULL: CF64/CI32/CF128 roundtrips, multiple frames in
    order, timeout
  - REQ/REP raw bytes: request-reply roundtrip with string data
  - REQ/REP signal frames: CF64/CI32/CF128 full roundtrips
    (request + reply)
  - Error handling: NULL endpoints, invalid send args,
    destroy(NULL) safety, invalid recv args
  - Header validation: all fields (magic, version, protocol,
    stream_id, flags, sequence, timestamp, sample_rate,
    center_freq, num_samples, reserved[4])
  - **26/26 pass**, 2/2 CTest pass
- **Removed stale `c/tests/test_doppler.c`** — used
  pre-refactor API, not registered in CTest
- **Python FFT tests** (`python/doppler/tests/test_fft.py`):
  Replaced demo script with 20 proper pytest tests:
  - 1D FFT: impulse, DC, cosine tone energy, round-trip,
    matches NumPy
  - 1D in-place: matches NumPy, verifies array mutation
  - 2D FFT: impulse, round-trip, matches NumPy fft2
  - 2D in-place: matches NumPy fft2
  - Dispatcher: execute/execute_inplace for 1D and 2D,
    ValueError for 3D
  - One-shot fft(): 1D, 2D, inverse round-trip
  - **20/20 pass**, full suite **54/54 pass**
    (20 buffer + 20 FFT + 14 streaming)
- **Rust FFI tests** (`ffi/rust/src/lib.rs`): Added
  `#[cfg(test)]` module with 11 tests:
  - SIMD: c16_mul basic, identity, zero, conjugate,
    matches num-complex
  - Version: non-empty string containing a dot
  - 1D FFT: impulse, round-trip (forward+inverse), cosine
    energy at correct bins
  - 2D FFT: impulse, round-trip (forward+inverse)
  - **11/11 unit tests + 2/2 doc-tests pass**
- **Rust build.rs fix**: fallback search path
  `../../build` -> `../../build/c`

## Zero-Copy Streaming Refactor

- **Phase 1 -- Header API** (`c/include/dp/stream.h`):
  - `dp_header_t` expanded: added `protocol`
    (dp_protocol_t), `stream_id`, `flags` fields for
    DIFI/VITA 49 future-proofing
  - Added `dp_protocol_t` enum
    (DP_PROTO_SIGS=0, DP_PROTO_DIFI=1)
  - Added `dp_msg_t` opaque handle + 5 accessors
  - Changed recv signatures to return `dp_msg_t` instead
    of malloc'd buffer
  - Added signal-frame REQ/REP send/recv functions
  - Added timeout setters for all socket types
  - Removed `dp_sub_free_samples` (replaced by
    `dp_msg_free`)
  - Bumped version to 2.0.0
- **Phase 2 -- C Implementation**
  (`c/src/stream.c`, ~380 lines, was 675):
  - `dp_msg_t` struct wraps `zmq_msg_t` + metadata
  - De-duplicated socket creation: single `ctx_create()`
  - Zero-copy recv via `zmq_msg_recv`
  - `DP_ERR_TIMEOUT` on `EAGAIN`
- **Phase 3 -- C Callsites** (4 files, ~12 recv/free pairs):
  - Updated test_stream.c, receiver.c, pipeline_demo.c,
    spectrum_analyzer.c
  - **2/2 CTest pass**
- **Phase 4 -- Python Extension Rewrite**
  (`python/ext/dp_stream.c`, 540 lines, was 1693):
  - Thin wrapper over libdoppler (no direct zmq calls)
  - `dpMsgObject` wraps `dp_msg_t*` for zero-copy lifetime
  - Shared `do_send()`/`do_recv()` helpers
  - GIL release on all blocking C calls
  - `python/CMakeLists.txt`: links `doppler` shared lib
    instead of vendored `libzmq-static`
  - **Wire format compatible**: Python and C share the
    same `dp_header_t`
  - **34/34 pytest pass**

## Python Streaming C Extension + Vendoring

- **C Extension** (`python/ext/dp_stream.c`): All 6 socket
  types (Publisher, Subscriber, Push, Pull, Requester,
  Replier) as zero-copy Python C extension (1692 lines)
  - GIL release on all blocking zmq calls
  - Zero-copy recv on all receiver types
- **Static libzmq**: vendored libzmq 4.3.5 linked statically
  - Symbol hiding: only `PyInit_dp_stream` exported
  - No runtime libzmq dependency
- **Removed ctypes client.py** (310 lines)
- **Vendoring policy**: `VENDORED.md` with checksums,
  licensing, update process

## CI + Coverage

- `pyproject.toml`: pytest-cov, coverage config
- Coverage baseline: 76% overall on 26 tests
- CI uploads coverage.xml artifact

## Makefile Restore + CI Fixes

- Hand-written project wrapper Makefile recreated
- Root cmake artifacts cleaned up
- CI jobs use `make build` + `make test`

## Install Test Fixes

- `c/CMakeLists.txt`: full install wiring (GNUInstallDirs,
  cmake package config, pkg-config)
- `c/tests/test_install.sh`: 9-check post-install verification

## Buffer Python Extension

- `c/include/dp/buffer.h`: ARM compatibility
- `_buffer` module: F32Buffer, F64Buffer, I16Buffer
- 20 tests, all passing

## ARM / CI Fixes

- `simd.c`: guarded x86 intrinsics, ARM scalar fallback
- `-Ofast` -> `-O3 -ffast-math`
- CI: fixed Python executable matching for extensions

## Docs + API Reference

- mkdocs Material theme with dark/light toggle
- mkdoxy for C API docs, mkdocstrings for Python
- Full docstrings on all Python FFT functions
- `mkdocs build --strict` 0 warnings

## Python Streaming Bindings (ctypes, later replaced)

- `client.py`: Publisher, Subscriber, Push, Pull,
  Requester, Replier via ctypes
- test_pubsub.py: 6 tests

## Docker + CI + Build System

- Dockerfile overhauled: multi-stage, 130 MB image
- docker-compose.yml modernized
- CI: Ubuntu + macOS matrix, Docker job
- CMakeLists.txt: CTest registration, static library target
- NumPy ABI compatibility fix (1.x vs 2.x)

## Initial Integration

- Merged unified `doppler_*` API
- Python C extension separated from pure C
- Unified streaming: PUSH/PULL merged into doppler
