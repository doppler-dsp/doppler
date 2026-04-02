# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

> **API stability notice** — doppler is pre-1.0. Minor releases may
> make breaking changes. Check this file before upgrading.

## [Unreleased]

### Added

- **`doppler-cli`** (`python/cli/`): `doppler compose` pipeline
  orchestrator — `init`, `up`, `down`, `ps`, `inspect`, `logs`
  subcommands; ships as a separate pip-installable package
- **Dopplerfile** (`doppler_cli/dopplerfile.py`): YAML-defined
  custom pipeline blocks; `uv run --with` dep isolation; discovery
  from `~/.doppler/blocks/` or CWD; zero Python required to write
  a new block
- **Named compose chains**: `--name` flag on `doppler compose init`;
  name used as the pipeline ID and filename
- **`doppler logs`**: redirects per-block stdout/stderr to dated log
  files; `doppler ps` shows log paths
- **`doppler-source` entry point**: standalone IQ source block for
  use in compose pipelines
- **PUSH/PULL pipeline transport** between CLI blocks; blocks
  discover upstream/downstream ports automatically
- **Timestamped health logging**: all blocks print a startup banner
  with endpoint, PID, and timestamp
- **`web_host` config** in `SpecanConfig` (default `127.0.0.1`);
  allows binding the spectrum analyzer web server to a non-loopback
  address
- **specan: chirp sweep source** — synthetic linear chirp across the
  full display bandwidth; rate and depth configurable
- **specan: max-hold trace** — persists per-bin peak magnitude; can
  be toggled and reset from the UI
- **Recorded chirp demo** (`docs/specan/chirp_frames.json`, 150
  frames, ~470 KB): pre-captured WS frames served by the static
  docs demo without a live server
- **`scripts/capture_specan.py`**: captures live spectrum analyzer
  WS frames to JSON for use as a static demo recording
- **DPMFS resampler Python bindings** (`doppler.resample`):
  `Resampler`, `ResamplerDpmfs`, `HalfbandDecimator`; 40 new pytest
  tests; `fit_dpmfs` / `optimize_dpmfs` design tools
- **Accumulator module** (`doppler.accumulator`): `F32Accumulator`,
  `CF64Accumulator`; typed `.pyi` stubs
- **Delay line module** (`doppler.delay`): `DelayLine`; typed stubs
- **`doppler-specan` standalone package** (`python/specan/`):
  separately pip-installable (`pip install doppler-specan`); serves
  live FFT frames via WebSocket + static HTML UI
- **CONTRIBUTING.md**: mandatory checklist — benchmarks, examples,
  NumPy docstrings, typed stubs, cross-language test chain

### Changed

- **`python/doppler/` → `python/dsp/`**: Python source tree renamed
  to avoid collision with the installed `doppler` package name
- **`SpEcan` → `Specan`** throughout (`SpecanEngine`,
  `SpecanConfig`): removed non-standard capitalisation
- **specan: demo controls hidden** when source is not `demo`,
  reducing UI clutter for real-signal use
- **specan: 100dvh layout** — fixes mobile viewport cutoff on
  iOS/Android browsers
- **`doppler compose up`** defaults to the most recently created
  compose file when `--file` is omitted
- **`docs/examples/`**: split `examples.md` into
  `examples/{index,c,python,streaming}.md` for easier navigation
- **Docs index**: rewrote introduction for clarity; added complete
  feature matrix

### Fixed

- **specan: inverted Gaussian** in synthetic chirp magnitude frame
  (peak appeared as a trough)
- **specan: socket source** — CLI option parsing and noise floor
  visibility corrected
- **specan: stale chirp state** not cleared when switching sources

---

## [0.2.3] — 2026-04-02

### Added

- **cibuildwheel** (`release.yml`, `pyproject.toml`): builds proper
  platform wheels for Linux (manylinux_2_28 x86_64) and macOS
  (arm64 via `macos-14`, x86_64 via `macos-13`); replaces the
  single ubuntu-latest wheel build
- **`scripts/retag_wheel.sh`**: retags the `py3-none-any` wheel
  produced by `uv_build` to the correct `cpXYZ-cpXYZ` ABI tag,
  then runs `auditwheel` (Linux) or `delocate` (macOS) to bundle
  shared-lib dependencies
- **`make wheel` target**: local equivalent of the CI wheel build —
  runs `uv build --wheel` then `uvx auditwheel repair` via a new
  CMake `wheel` target (Linux only)
- **Release workflow — all three packages**: `release.yml` now
  builds and publishes `doppler-dsp`, `doppler-specan`, and
  `doppler-cli` to PyPI via a matrix over Linux and macOS;
  `verify-version` checks all three `pyproject.toml` files against
  the tag
- **`make bump-version`** now updates `python/specan/pyproject.toml`
  and `python/cli/pyproject.toml` in addition to the root,
  `Cargo.toml`, and `CMakeLists.txt`

### Changed

- **`docs/build.md`**: corrected all `pip install doppler` →
  `pip install doppler-dsp`; added install instructions for
  `doppler-specan` and `doppler-cli`; fixed from-source commands

---

## [0.2.0] — 2026-03-26

### Added

- **Rust FFI — NCO bindings** (`ffi/rust/src/nco.rs`): Full Rust
  wrapper for the C NCO API — `Nco::new`, `execute_cf32`,
  `execute_cf32_ctrl`, `execute_u32`, `execute_u32_ovf`, `reset`,
  `set_freq`, `get_freq`; 13 new Rust unit tests
- **Rust FFI — FIR bindings** (`ffi/rust/src/fir.rs`): Full Rust
  wrapper for `dp_fir_t` — `FirFilter::lowpass_f32`,
  `execute_cf32`; included in Rust test suite
- **NCO Rust example** (`ffi/rust/examples/nco_demo.rs`): prints
  IQ samples, FM control-port demo, raw phase accumulator, overflow
  detection
- **`make rust-examples` target**: builds all Rust examples and
  prints their paths (cross-platform, handles `.exe` on Windows)
- **Windows / MSYS2 UCRT64 support** for Rust FFI: static link to
  `libdoppler.a`, `fftw3_threads`, correct MinGW `stdc++`; full
  build + test verified on Windows
- **Release workflow** (`.github/workflows/release.yml`):
  tag-triggered CI that verifies version consistency across
  `pyproject.toml`, `Cargo.toml`, and `CMakeLists.txt`, builds
  Python wheel, publishes to PyPI via OIDC trusted publishing, and
  creates a GitHub Release with auto-generated notes
- **`make bump-version VERSION=x.y.z`**: atomically updates the
  three version locations
- **`make tag-release VERSION=x.y.z`**: commits the version bump,
  creates an annotated tag, and pushes
- **Zensical documentation**: migrated from mkdocs/Material to
  Zensical (`uv run zensical build --clean`); docs job updated in
  CI; `make docs-build` / `make docs-serve` targets added
- **`make specan` target**: launches live spectrum analyzer in
  browser via `uv run doppler-specan`
- **Windows build guide** in `docs/build.md`: step-by-step MSYS2
  UCRT64 instructions covering all dependencies, cmake, and Rust
  FFI testing

### Changed

- **CI — Windows MSYS2 environment**: switched from `MINGW64` to
  `UCRT64` to match the rest of the toolchain; added
  `mingw-w64-ucrt-x86_64-rust` to the MSYS2 package list
- **CI — added `make rust-test` steps** to all four OS matrix
  entries (Ubuntu 22.04, 24.04, macOS, Windows)
- **`ffi/rust/build.rs`**: platform-split link strategy — dylib +
  rpath on Linux/macOS, static + `fftw3_threads` + `stdc++` on
  Windows; MinGW LTO workaround removed (handled in CMake)
- **CMakeLists.txt**: LTO disabled on MinGW (`if(NOT MINGW)` guard)
  to prevent `plugin needed to handle lto object` errors when Rust
  links the static archive
- **`python/ext/` renamed to `python/src/`** for clarity (no longer
  looks like a Maturin/Rust extension directory)
- **`docs/build.md`**: added Rust FFI section, UCRT64 Windows
  guide, updated artifact table

### Fixed

- **Rust static link on Windows**: `libdoppler.dll` loaded beyond
  the 2 GB boundary causing pseudo-relocation overflows; fixed by
  linking statically on Windows
- **`make rust-examples` empty output on Windows**: `grep -v '[.\-]'`
  excluded `.exe` files; fixed with `grep -E '^[a-z_]+(\.exe)?$'`

---

## [0.1.0] — 2025-01-01

### Added

- **NCO** (`c/include/dp/nco.h`, `c/src/nco.c`): 32-bit phase
  accumulator, 2^16-entry sine LUT (~96 dBc SFDR), FM control port
  (`dp_nco_execute_cf32_ctrl`); 59 CTest unit tests
- **FIR filter** (`c/include/dp/fir.h`, `c/src/fir.c`): real and
  complex taps, AVX-512 / scalar paths, CI8/CI16/CI32/CF32 inputs;
  `dp_fir_create`, `dp_fir_execute_*`
- **Lock-free ring buffer** (`c/include/dp/buffer.h`): SPSC ring
  buffer; Python `_buffer` module (`F32Buffer`, `F64Buffer`,
  `I16Buffer`); 20 pytest tests
- **Python FFT tests** (`python/dsp/doppler/tests/test_fft.py`): 20
  pytest tests covering 1D/2D FFT, impulse response, round-trip,
  NumPy parity, dispatcher, one-shot `fft()`
- **Python streaming C extension** (`python/src/dp_stream.c`): all
  6 socket types (Publisher, Subscriber, Push, Pull, Requester,
  Replier) as a zero-copy Python C extension; GIL release on all
  blocking calls; replaces ctypes `client.py`
- **Python buffer C extension** (`python/src/dp_buffer.c`): thin
  wrapper exposing the lock-free ring buffer to Python
- **Rust FFI** (`ffi/rust/`): initial bindings — version, SIMD
  `c16_mul`, 1D/2D FFT; 11 unit tests + 2 doc-tests; `fft_demo`,
  `simd_demo`, `fft_bench` examples; `build.rs` with rpath baking
- **C streaming tests** (`c/tests/test_stream.c`): 26 tests
  covering all socket types (PUB/SUB, PUSH/PULL, REQ/REP), zero-
  copy `dp_msg_t`, timeouts, header validation, error handling
- **Post-install verification** (`c/tests/test_install.sh`): 9
  checks for pkg-config, headers, and linkage
- **Docker**: multi-stage Dockerfile, 130 MB image; `docker-compose.yml`
- **CI** (`.github/workflows/ci.yml`): Ubuntu 22.04/24.04 + macOS
  + Windows matrix; Python 3.12/3.13 pytest job; Docker build + smoke-
  test job; coverage upload
- **Makefile**: project wrapper with `build`, `test`, `rust-test`,
  `install`, `install-test`, `pyext`, `python-test`, `test-all`,
  `docker`, `docker-test`, `debug`, `release`, `blazing`, `clean`,
  `help` targets
- **Documentation**: `docs/` site with build guide, API reference,
  quickstart, examples, and design docs

### Changed

- **Zero-copy streaming refactor**: `dp_header_t` expanded with
  `protocol` (`dp_protocol_t`), `stream_id`, `flags` fields;
  `dp_msg_t` opaque handle replaces malloc'd buffers; version
  bumped to 2.0.0; Python extension rewritten from 1693 → 540 lines
- **Static libzmq replaced with system libzmq**: Python extension
  now links the system `libzmq` shared library; `VENDORED.md`
  documents vendoring policy
- **`-Ofast` replaced with `-O3 -ffast-math`** for standards
  compliance
- **SIMD**: x86 intrinsics guarded; ARM scalar fallback added in
  `c/src/simd.c`

### Fixed

- **ARM CI**: guarded x86 intrinsics in `simd.c`
- **NumPy ABI**: compatibility fix for 1.x vs 2.x
- **cmake scatter**: all build artifacts confined to `build/`;
  root-level cmake artifacts cleaned up
- **Python executable matching** in CI for C extension builds

[Unreleased]: https://github.com/doppler-dsp/doppler/compare/v0.2.3...HEAD
[0.2.3]: https://github.com/doppler-dsp/doppler/compare/v0.2.0...v0.2.3
[0.2.0]: https://github.com/doppler-dsp/doppler/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/doppler-dsp/doppler/releases/tag/v0.1.0
