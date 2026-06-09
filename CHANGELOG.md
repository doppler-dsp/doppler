# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

______________________________________________________________________

> **API stability notice** — doppler is pre-1.0. Minor releases may
> make breaking changes. Check this file before upgrading.

## [Unreleased]

## [0.8.0] — 2026-06-08

The headline is the **C composer subsystem, now in Python** — the multi-segment
waveform engine behind the `wfmgen` CLI, exposed as an ergonomic class API whose
output is byte-identical to the CLI.

### Added

- **`doppler.wfmgen.compose`** — the Python face of the C `wfmgen` composer /
    writer / sink subsystem (~18 C functions), a hand-written `no_generate`
    CPython module (the `ddc_fn` pattern: opaque PyCapsules, GIL released around
    the kernels):

    - **`Segment` + `Composer`** — multi-segment streams (per-segment on-time and
        trailing gap), `repeat` / `continuous`, streaming `execute()` / one-shot
        `compose()`, and a JSON spec round-trip (`to_json` / `from_json` /
        `from_file`).
    - **`Writer`** — raw / CSV / BLUE type-1000 / SigMF containers, pairing with
        `read_iq`; plus `sigmf_meta()` and `write_blue_header()` helpers.
    - **`ZmqSink`** (POSIX) — ZeroMQ PUB with the wfmgen fs/fc framing, decodable
        by `doppler.stream.Subscriber`.
    - **`rrc_taps`, `dsss_spread`, `mls_poly`** free functions.

    Output is byte-identical to the `wavegen` / `wfmgen` CLIs — proven by 15 md5
    byte-parity tests (5 waveform types × 3 sample types) against the CLI, plus a
    `ZmqSink`→`Subscriber` loopback, BLUE/SigMF container tests, and JSON /
    Writer↔`read_iq` round-trips (33 pytests, doctests on every public symbol).

### Changed

- The `nogil` execute bindings (`ddc`, `ddcr`) and the new composer binding are
    GNU-formatted with `Py_BEGIN/END_ALLOW_THREADS` treated as block macros, so
    clang-format keeps each on its own line (synced to just-makeit's canonical
    `.clang-format`).

## [0.7.0] — 2026-06-08

### Added

- **`I32ToF32` / `I8ToF32` converters** (`doppler.cvt`) — int32→float32 and
    int8→float32 with configurable full-scale, round-trip tested against the
    F32→int writers.
- **`read_iq()`** (`doppler.wfmgen.readback`) — read an interleaved-I/Q capture
    back into a complex NumPy array: cf32/cf64 as a zero-copy complex view,
    ci8/16/32 rescaled through the fast int→f32 converters, `raw=True` for the
    raw `(N, 2)` view. Documented alongside the interleaved-I/Q view-vs-copy
    table in `docs/types.md`.
- **Comprehensive docstrings + doctests across all 16 modules** — every public
    class, method, function, and property now carries a full numpy-style docstring
    with a verified, runnable `Examples` doctest (884 doctest lines, CI-gated via
    `pytest --doctest-glob`), synthesized from the C-header Doxygen by
    just-makeit 0.18.0 (`@code` → `Examples`).

### Changed

- just-makeit pin → **0.18.0** (header-derived docstrings, `@code` doctests,
    built-in `step`/`steps`/`reset` deriving from the header `@brief`).

### Tooling

- **pre-commit** — ruff (lint + format), clang-format (pinned v19), mdformat,
    and hygiene hooks, enforced by a CI `pre-commit` job. jm-generated glue is
    excluded (owned by the `jm status --check` manifest-drift gate).

______________________________________________________________________

## [0.6.0] — 2026-06-07

The headline is a new **waveform generator** — a C-first synthesis engine with
two command-line tools and a Python API — plus a substantial throughput pass on
the signal-source primitives and a refreshed brand and docs site.

### Added

- **Waveform generator (`doppler.wfmgen`)** — five waveform types (tone, noise,
    PN, BPSK, QPSK) from one declarative C engine, exposed three ways:
    - **`wavegen`** — single-shot generator with three byte-identical faces
        (C binary, console script, PEP 723), `--sample_type cf32|cf64|ci32|ci16|ci8`,
        `--file_type raw|csv|blue|sigmf`, `--endian le|be`, and `--record`.
    - **`wfmgen`** — multi-segment composer: JSON specs via `--from-file` /
        `--record` (byte-exact round-trip), off-time gaps, repeat / continuous, and
        a ZMQ PUB sink (`--output zmq://…`).
    - Python API: `Synth` (the engine) and `PN` (raw LFSR m-sequence).
- **64-bit PN/LFSR** with a verified primitive-polynomial table for **every
    length 2..64** (`--pn_length`, `--pn_poly`); auto-MLS when `--pn_poly 0`.
- **Fibonacci LFSR** alongside Galois — `--lfsr galois|fibonacci`
    (`lfsr=` on `Synth`/`PN`); same polynomial and period, different realization.
- **BLUE type-1000** output: a complete 512-byte Header Control Block, **attached
    or detached** (`--detached` → `<out>.hdr` + `<out>.det`).
- **SigMF** output (`--file_type sigmf`): `.sigmf-data` + `.sigmf-meta` with one
    annotation per composer segment.
- **`ddc_fn`** — the functional down-converter API promoted to first-class
    (re-exported from `doppler.ddc`, with a gallery walkthrough).
- Brand kit (wordmark, favicon, social / app icons) and a Python API-reference
    page for `wfmgen`, plus a runnable `examples/python/pn_codes.py`.

### Changed

- **AWGN AVX-512 is now runtime-dispatched.** The 8-wide generator is selected
    at run time via `__builtin_cpu_supports`, so the distribution-safe wheel uses
    it on capable CPUs and falls back to scalar on older hardware — no new runtime
    dependency (libmvec is referenced through a weak symbol). ~2.6× noise
    throughput on AVX-512 machines.
- **The synth engine is fully batched.** The LO carrier and AWGN are generated a
    block at a time (vectorized), PN chips come from the block generator, clean
    waveforms (`snr ≥ 100`) skip AWGN entirely, and baseband (`freq 0`) skips the
    LO — byte-identical output, with the PN/LFSR path reaching ~1 GSa/s.
- Documentation site migrated from Zensical to **mkdocs-material**.

### Fixed

- Benchmark CI now captures the **C (`jm_bench`) suite** to the `benchmarks`
    branch (previously Python-only); the benchmarking docs were corrected to match,
    and the advisory perf-regression gate's just-makeit pin was aligned to 0.17.1.
- `zensical.toml` is no longer re-committed by `jm apply` (gitignored).

### Tests & docs

- Comprehensive waveform-generator **user guide** and **gallery**; the
    benchmarking guide now matches the workflows. PN/Fibonacci/64-bit coverage at
    the C, CLI, and Python layers (1046 Python tests, 41 C tests).

## [0.5.5] — 2026-06-04

### Fixed

- **Runtime `__doc__` now matches the derived docs — for real this time.**
    0.5.4 derived numpy docstrings into the `.pyi` stubs, but the C runtime docs
    (`help(DDC.execute)`, `DDC.__doc__`, property docs) still showed the stale
    scaffold fallback ("Zero-copy view…", "DDC type."), because those strings
    live in the sacred per-object `<mod>_ext_<obj>.c` binding fragments that
    `jm apply` does not regenerate. Upgraded just-makeit to **0.14.12** (the
    doc-slot refresh landed in 0.14.11; 0.14.12 is the first published build to
    carry it), whose `apply` transplants the derived docstrings into the
    fragment's `PyMethodDef`
    / `PyGetSetDef` / `tp_doc` slots — but **only** where the slot still holds
    the scaffold form or is empty. Hand-written docstrings and bindings the
    manifest can't express are preserved untouched: `RateConverter`'s rich class
    doc and `stages` accessor, and the `cvt` `clipped` getters, are unchanged.
    Now `DDC.execute.__doc__` really is "Mix input block with LO, then
    rate-convert." and `DDC.norm_freq.__doc__` is "Return the current LO
    normalised frequency.".

______________________________________________________________________

## [0.5.4] — 2026-06-04

### Added

- **Rich Python docstrings, derived from the C headers.** Upgraded the
    just-makeit toolchain to 0.14.7, which synthesizes numpy-style docstrings
    for every class, method, and property from the hand-written Doxygen
    (`@brief`/`@param`/`@return`) already in each `<obj>_core.h`. The generated
    `.pyi` stubs (and the C bindings' `__doc__`) now carry real documentation
    instead of `"""Execute."""` — e.g. `help(DDC.execute)` shows "Mix input
    block with LO, then rate-convert." with its parameters. The header stays the
    single source of truth; `jm apply` regenerates the docs.
- **Doctest gate in CI.** The synthesized `.pyi` examples are run against the
    freshly built extensions (`pytest --doctest-glob='*.pyi'`), so a docstring
    that drifts from the API fails CI.

### Fixed

- Hand-written `stream.pyi` doctests that perform live socket I/O are marked
    `# doctest: +SKIP` (they need a running peer); the `get_timestamp_ns`
    example gains the blank line doctest-as-text parsing requires.

## [0.5.3] — 2026-06-03

### Fixed

- **`doppler-specan` no longer hangs on "Waiting for signal…".** The specan
    engine was never updated after the ddc/spectral/jm migrations and referenced
    five renamed/removed extension APIs; every per-block failure was swallowed by
    the display's DSP loop, turning a hard error into a silent forever-hang.
    Restored: `DDC` (was `Ddc`), the 2-arg `DDC(norm_freq, rate)` constructor,
    the `norm_freq` property setter (was `set_freq`), a specan-local
    `kaiser_beta_for_enbw` (deleted in the migration) adapted to the in-place
    `kaiser_window`, and `FFT.execute_cf32` (was `FFT.execute`).
- The terminal DSP loop now records and **displays** processing errors instead
    of silently retrying — a persistent failure shows a diagnostic panel.

### Added

- First integration tests for `doppler.specan` — drive the full demo → DDC →
    Kaiser → FFT → peak-detection chain through the real C extensions, so an
    extension-API drift can no longer ship undetected.

______________________________________________________________________

## [0.5.2] — 2026-06-03

### Fixed

- **Published Linux wheels are now portable.** The 0.5.1 manylinux wheel was
    built with `-march=native`, baking the CI build host's AVX-512 instructions
    into the binary; on any CPU without AVX-512 it crashed at first use with
    `Illegal instruction (core dumped)` (e.g. `doppler-specan --source demo`).
    The build's `-march` portability guard keyed on a `CIBUILDWHEEL` env var that
    the release workflow (`python -m build`, not cibuildwheel) never set, so the
    guard never fired.

### Changed

- **`-march` policy inverted to safe-by-default.** All builds, including every
    release/wheel path, now target a portable baseline (`x86-64-v2` on x86-64);
    `-march=native` is strictly opt-in via `-DDOPPLER_NATIVE=ON` (`make blazing`)
    for local dev/bench only. Correctness no longer depends on any CI tool
    exporting an env var.
- The release workflow now disassembles every bundled extension and **fails the
    release if any AVX2/AVX-512 (`%ymm`/`%zmm`) instruction is present**, so a
    non-portable wheel can never be published again.

______________________________________________________________________

## [0.5.1] — 2026-06-03

### Added

- **`doppler.arith`** — Q8/Q15 fixed-point arithmetic module. `AccQ15` /
    `AccQ8` saturating accumulators, plus elementwise `add`/`sub`/`mul`/`dot`/
    `shl`/`shr` for Q15 and Q8 arrays (`add_q15`, `mul_q8`, `dot_q15`, …) and
    `shl_i64`/`shr_i64`. Two's-complement saturation with round-half-up.
- "Getting Started with Fixed-Point Arithmetic" guide.

### Changed

- Upgraded the just-makeit toolchain to 0.14.4. `ddc` / `ddcr` /
    `RateConverter` now declare `pass_capacity`, so jm generates their 5-arg
    `*_execute(..., out, max_out)` signature directly (no hand-patched header).
    `ddc_fn` is declared as a `no_generate` module so its CMake wiring is
    jm-managed.
- CI gained a native manifest-drift gate (`jm status --check`); the generated
    glue is asserted in sync with `just-makeit.toml` on every run.
- Vendored cJSON 1.7.19 under `vendor/cjson/` (drop-in, not yet wired into the
    build).

### Fixed

- **`doppler.source` import failure on some Linux toolchains**
    (`source.so: undefined symbol: _ZGVdN8v_logf`). GCC auto-vectorises
    `awgn`'s `logf` into a libmvec call; the extension now links `libmvec` on
    Linux.
- `spectral` `kaiser_window` / `hann_window` header drift — `w` is now a
    writable out-param, so `jm apply` no longer adds a spurious `const`.

______________________________________________________________________

## [0.5.0] — 2026-06-02

### Added

- **`doppler.cvt.ADC`** — signed N-bit (1–64) two's-complement ADC model.
    Configurable full-scale level (`dbfs`), optional TPDF dither, sticky
    `clipped` flag, and `steps()` with SIMD float-scale path. Accepts any
    bit depth; uses `double` precision scale for bits > 23.
- **`doppler.cvt.ADCIQ`** — CF32 → interleaved IQ int16 wrapper around `ADC`.
    Exploits the complex64 memory layout (I₀ Q₀ I₁ Q₁ …) to process both
    channels in a single SIMD call. Restricted to bits ≤ 16 so output fits
    int16.
- **`doppler.filter.HBDecimQ15`** — fixed-point halfband 2:1 decimator for
    interleaved IQ int16 (ADCIQ output format → 2:1 decimated IQ int16).
    AVX2 inner loop uses a two-pass `_mm256_madd_epi16` strategy (left side +
    right side reversed) that avoids computing the symmetric fold as int16,
    eliminating saturation at any valid input level. I and Q run as two
    independent madd chains on the same coefficient vector — free ILP on any
    superscalar core. Scalar fallback for non-AVX2 targets.
- **Functional DDCR API** (`doppler.ddc.ddcr_create` / `ddcr_execute` /
    `ddcr_reset` / `ddcr_destroy` / `ddcr_get_norm_freq` / `ddcr_set_norm_freq`
    / `ddcr_get_rate`) — state passed explicitly as an opaque capsule rather
    than bound to a Python object; suited for multi-pipeline use cases.
- **Gallery examples**: ADC quantisation staircase (3–8 bits, time + spectrum)
    and HBDecimQ15 (frequency response Q15 vs float32, input/output spectra
    showing −60 dB stopband suppression).

### Changed

- **`doppler.ddc` build layout**: `ddc_fn_ext.c` moved to
    `native/src/ddc_fn/` with its own `CMakeLists.txt`, isolated from
    `just-makeit` regeneration so `jm apply` can no longer clobber the
    functional DDCR API.

### Fixed

- **HBDecimQ15 SIMD**: replaced `_mm256_adds_epi16` (saturating fold) with
    two-pass `_mm256_madd_epi16`; the saturating add clipped fold values above
    −6 dBFS and destroyed the stopband cancellation at frequencies where
    adjacent delay-line samples are in-phase (e.g. f = 0.45 with a 60 dB
    halfband design gives fold ≈ 38 044 > 32 767, turning a theoretical
    −82 dBFS null into −29 dBFS leakage).

______________________________________________________________________

## [0.4.6] — 2026-05-28

### Fixed

- Release workflow now publishes all artifact types: sdist, Linux x86_64 and
    macOS arm64 C library tarballs (headers + static + shared libs), and OCI
    packages on GitHub Container Registry (`ghcr.io`).

______________________________________________________________________

## [0.4.1] — 2026-05-26

### Added

- **`F32ToUQ15` / `UQ15ToF32`** — offset-binary uint16 converters in
    `doppler.cvt`. Encode: `v_Q15 + 32768 → uint16` (−1.0 → 0, 0.0 → 32768,
    ~+1.0 → 65535). `F32ToUQ15` has a sticky `clipped` property identical to
    `F32ToI16`. 13 new tests; roundtrip error ≤ 0.5 LSB.
- **`docs/design/QUANTIZATION.md` — §7.1 UQ15 definition**: formal
    encode/decode formulas, code-point table, and reference to the new cvt
    converters. The document is now reachable from the website nav under
    **Design → Quantization**.
- **`docs/types.md` — quantization schemes table**: Q15, I16U32, I16U64,
    UQ15, and UQ16 listed with container type, zero-code, and one-line
    description; links to QUANTIZATION.md.

### Fixed

- **Stream module test coverage**: 28 tests covering all six socket patterns
    (PUSH/PULL, PUB/SUB, REQ/REP) with CI32, CF64, and CF128 types; context
    manager and timeout tests added.
- **cvt gallery decode example** (`docs/gallery/cvt-quantization.md`):
    corrected snippet to show the `I16ToF32` decode step and the `clipped`
    property; demo signal amplitudes rescaled to stay within Q15 full-scale.
- **CIC gallery snippet** (`docs/gallery/cic.md`): added missing `f_jammer`
    variable and `_tone` helper so the example is copy-pasteable.
- **CIC alias comment** (`examples/python/cic_demo.py`): `aliases to 48 kHz`
    corrected to `aliases to -48 kHz` (208 kHz − 2×128 kHz = −48 kHz).

______________________________________________________________________

## [0.4.0] — 2026-05-26

### Changed

- **Breaking**: `CIC` constructor and `reconfigure()` now take only `R`
    (decimation ratio). `N` and `M` are fixed constants (`N=4`, `M=1`) and
    are no longer accepted as arguments. The `N`, `M`, `input_scale`, and
    `output_scale` properties are removed; `shift` (`= 4 * log2(R)`) is
    added.
- CIC internal encoding switched from sign-extended Q15 to offset-binary
    UQ16 (`v_Q15 + 32768 → uint64`). All integrator/comb arithmetic is now
    purely unsigned, eliminating the C99 implementation-defined
    `(int16_t)(uint16_t)v` cast in the output path.
- Zero CF32 input now produces a non-zero transient for the first `N=4`
    output periods (the DC offset bias ramps the integrators before the comb
    chain fills); output is exactly `0+0j` from index 4 onward.

### Added

- `F32ToI16`, `F32ToI16U32`, `F32ToI16U64`: sticky `clipped` property
    (`bool`) — reads `True` if any sample has been saturated since the last
    `reset()`.
- `docs/design/QUANTIZATION.md`: full C99 cast-chain analysis for both
    UQ16 encode and decode paths.
- Spectral purity roundtrip tests for all three cvt encoder/decoder pairs
    (−80 dBc threshold, `src/doppler/cvt/tests/test_roundtrip_spectral.py`).
- Gallery: Q15 vs UQ15 quantization demo (`examples/python/q15_uq15_demo.py`)
    and cvt quantization noise comparison (`examples/python/cvt_quantization_demo.py`).

______________________________________________________________________

## [0.3.7] — 2026-05-24

### Changed

- CI now uses `jbx install-deps` to install system dependencies from
    `jb.toml`, replacing inline `apt-get`/`brew install` blocks.
- `jb.toml` is the single system-deps manifest; the standalone
    `install-deps.sh` shim and `jb-deps.toml` are removed.
- Benchmark workflow runs automatically on release tags only; opt-in
    via `workflow_dispatch` otherwise.
- Benchmark snapshots are capped at 512 KB to prevent `stats.data`
    arrays from bloating the repository.

______________________________________________________________________

## [0.3.6] — 2026-05-22

### Added

- **`Resampler` custom filter bank** — `Resampler` accepts an optional
    `bank=` keyword argument to supply a pre-computed polyphase filter
    bank, routing to `resamp_create_custom` internally.
- **`HalfbandDecimatorDp` / `HalfbandDecimatorR2C`** — two new Python
    types wrapping the double-precision and real-to-complex halfband
    decimator variants; both are exported from `doppler.resample`.
- **Gallery examples** — detection/correlation and AGC plot-generating
    examples added; `make gallery` target regenerates all gallery images.
- **C examples** — `docs/examples/c.md` filled with working C snippets
    covering AGC, FIR filter, delay, source, accumulator, and resample.

### Fixed

- **FIR heap corruption** — `FIR.execute` used a pre-allocated output
    buffer sized by `fir_execute_max_out()`, which returns 0 at
    construction time. `malloc(0)` produced a zero-byte allocation that
    every `execute` call silently overflowed. Output is now allocated
    fresh per call with `PyArray_SimpleNew`.
- **FIR real-tap dispatch** — `FIR.__init__` now inspects the tap
    array dtype: `float32` routes to `fir_create_real`; `complex64`
    routes to `fir_create`. Previously only the complex path was wired.
- **`Delay.ptr()` default length** — the default `n` for `ptr()` was
    hardcoded to 1; it now defaults to `handle->num_taps` (the full
    delay line), matching the expected no-argument behaviour.
- **`HalfbandDecimator` argument order** — `HalfbandDecimator_create`
    was called with `(ptr, h_len)` instead of the correct `(h_len, ptr)`.
- **`Resampler.execute_ctrl` guard** — added a length check requiring
    the control array to be at least as long as the input array.
- **Docs build** — `spectral.pyi` used `in` (a Python keyword) as a
    parameter name, causing griffe to silently drop the
    `doppler.spectral.spectral` submodule and fail with
    `AliasResolutionError` on every `zensical build`. Fixed parameter
    names and added missing `from typing import Any`.

______________________________________________________________________

## [0.3.5] — 2026-05-22

### Added

- **`Corr` / `Corr2D`** — cross-correlation components backed by
    `corr_state_t` / `corr2d_state_t`; exported from `doppler.spectral`.
- **`Detector` / `Detector2D`** — CFAR detectors on top of the
    correlators; configurable noise mode (`mean`, `median`, `min`, `max`)
    and per-dwell threshold; exported from `doppler.spectral`.
- **`detection` module** — Marcum Q function, envelope detector, and
    power detector; exported from `doppler.detection`.
- **Python 3.14** — added to the release wheel build matrix.

### Changed

- **`dp_sample_type_t` enum values** — `DP_` prefix dropped
    (e.g. `DP_CF32` → `CF32`). Breaking for C callers using the old names.
- **Stream / pub-sub type names** — `_t` suffix added to
    `dp_pub`, `dp_sub`, `dp_push`, `dp_pull`, `dp_req`, `dp_rep`,
    `dp_f32`, `dp_f64`, `dp_i16`. Breaking for C callers.
- **`DECLARE_DP_BUFFER` macro** — generated typedef now carries the
    `_t` suffix to match the convention above.

______________________________________________________________________

## [0.3.4] — 2026-05-19

### Added

- **`util` module** — a new `doppler.util` extension module. Its first
    function, `square_clip`, clips a complex sample's real and imaginary
    parts independently to a [-lin, lin] box. It is header-only and
    inline, so the AGC and any other module share one definition.
- **AGC output clipping** — `AGC` gains a writable `clip_db` parameter.
    The output is square-clipped to `10^(clip_db/20)` per component,
    applied after the power detector so the control loop is unperturbed.
    Defaults high enough to be effectively off.

### Fixed

- **Linux wheels** — the `agc` extension failed to import on glibc 2.31+
    with `undefined symbol: __exp_finite`. `-ffast-math` emitted glibc's
    removed `__*_finite` math aliases when built in the manylinux image;
    the SIMD flags now include `-fno-finite-math-only`.
- **Wheel CPU portability** — released x86-64 wheels were built with
    `-march=native`, baking in the CI runner's instruction set. Wheels
    built under cibuildwheel now target a portable `x86-64-v2` baseline;
    local builds keep `-march=native`.

______________________________________________________________________

## [0.3.3] — 2026-05-19

### Added

- **AGC** — a log-domain automatic gain control component. Feedback
    loop with a 1st-order log-domain loop filter, an EMA power detector,
    and linear-in-dB gain, so settling time is independent of input
    level. `agc_steps()` runs a decimated control loop with a
    first-order-hold gain ramp and an explicit-SIMD power reduction;
    exposes `applied_gain_db` (the gain the signal actually saw)
    alongside the commanded `gain_db`.

### Changed

- Benchmarking now delegates to `just-makeit bench`, which writes
    trimmed, dated snapshots to `benchmarks/history/`. Raw per-iteration
    timing arrays are dropped, keeping committed snapshots small.

______________________________________________________________________

## [0.3.2] — 2026-05-18

### Fixed

- `doppler.__version__` now resolves correctly under the `doppler-dsp`
    PyPI distribution name.

______________________________________________________________________

## [0.3.1] — 2026-05-18

### Added

- **C examples** (`examples/c/`): seven self-contained, runnable C programs
    — `nco_demo`, `fir_demo`, `hbdecim_demo`, `fft_demo` (DSP demos) and
    `transmitter`, `receiver`, `pipeline_demo`, `spectrum_analyzer`
    (streaming demos using ZMQ PUB/SUB and PUSH/PULL). All link
    `doppler_lib_static` so they run without a system `libdoppler.so`.
- **Python examples** (`examples/python/`): `fir_demo.py`,
    `transmitter.py`, `receiver.py` — end-to-end scripts exercising the
    Python bindings and streaming API.
- **`doppler.__version__`**: the installed package version is now
    available as `doppler.__version__` (via `importlib.metadata`);
    resolves to `"unknown"` when the package is not installed.
- **Docker Compose demo**: `docker compose up` now starts a full
    streaming pipeline — transmitter, two receivers, and a spectrum
    analyzer — as separate containers; all example binaries are included
    in the runtime image.
- **`make test-examples`**: runs the C example binaries as smoke tests
    (FFT, FIR, NCO, halfband decimator); part of `make test-all`.
- **`make test-examples-python`**: runs Python example smoke tests;
    part of `make test-all`.

### Changed

- **PyPI distribution**: the package is published as `doppler-dsp`. The
    former separate `doppler-cli` and `doppler-specan` packages are now
    optional extras of `doppler-dsp` rather than standalone distributions.
- **CMake install**: `doppler_lib_static` (`libdoppler.a`) is now
    installed without being added to the `doppler-targets` CMake export
    set. The shared library (`doppler::doppler`) remains the primary
    CMake-integrated target; users who want static linking can link
    `libdoppler.a` directly alongside `-lstdc++ -lpthread -lm`.

### CI

- **glibc 2.28 verification** (`glibc-228` job): builds and runs the
    C examples on Debian Buster (glibc 2.28); verifies `libdoppler.so`
    contains no glibc symbols newer than 2.28.

______________________________________________________________________

## [0.2.9] — 2026-05-09

### Fixed

- **CMake version**: `project(VERSION …)` now always receives a
    numeric `X.Y.Z` string; `bump-version` strips Python pre-release
    suffixes (e.g. `a0`) before writing to `CMakeLists.txt`
- **Cargo version**: same suffix-stripping applied to `Cargo.toml`;
    Cargo requires SemVer and rejected `0.2.9a0`
- **`just-build` target**: corrected env-var names from
    `JUST_BUILD_OUTPUT_DIR/PYTHON` to `JUST_BUILDIT_OUTPUT_DIR/PYTHON`;
    empty `mkdir -p` had been failing the Release workflow
- **Specan staleness CI check**: narrowed watched path from
    `python/specan/` to `python/specan/doppler_specan/` so version-bump
    commits no longer falsely trigger the guard

______________________________________________________________________

## [0.2.8] — 2026-05-09

### Added

- **CF32 FFT** (`dp_fft1d_execute_cf32`, `dp_fft1d_execute_inplace_cf32`,
    `dp_fft2d_execute_cf32`, `dp_fft2d_execute_inplace_cf32`): single-
    precision (float complex) FFT variants backed by FFTW `fftwf_*` and
    pocketfft; ~1.9–2.6× faster than CF64 across 1K–16K sizes
- **Python dtype dispatch**: `execute1d`, `execute2d`, `execute`, `fft`
    now auto-route on input dtype — `complex64` → CF32 path with
    `complex64` output; `complex128` → CF64 path unchanged

### Changed

- **CMake**: `libfftw3f` and `libfftw3f_threads` added as FFTW-backend
    dependencies

### Fixed

- **Rust FFI (Windows)**: `build.rs` now links `fftw3f` and
    `fftw3f_threads` on Windows; static `libdoppler.a` requires all
    `fftwf_*` symbol providers to be listed explicitly

______________________________________________________________________

## [0.2.7] — 2026-04-08

### Added

- **Architecture docs** (`docs/architecture.md`): new page explaining
    the four-layer stack (DSP library → transport → pipeline CLI →
    apps); HTML stack diagram with DSP layer highlighted; Mermaid
    compose flow diagram; added to nav between Quick Start and Overview
- **`doppler compose up` status lines**: prints the specan web URL
    immediately after startup (`specan → http://127.0.0.1:8080`);
    extensible via `Block.status_lines()` hook
- **`record_demo` warmup** (`--warmup N`, default 5): discards the
    first N frames before recording so the static demo starts clean;
    fixes startup glitch without patching the player

### Changed

- **`docs/specan/frames.json`** regenerated with warmup; demo player
    no longer needs the `slice(1)` workaround
- **Polyphase docstrings** expanded (`kaiser_beta`, `kaiser_taps`,
    `kaiser_prototype`); `matlab_optimization.py` removed

### Fixed

- **`record_demo`**: removed stale `beta` argument that was rejected
    by `SpecanConfig` after the field was dropped

### CI

- **Python 3.14** added to the test matrix
- **Specan demo staleness check**: new `specan-demo` job fails when
    `python/specan/` changes without a corresponding update to
    `docs/specan/frames.json`

______________________________________________________________________

## [0.2.6] — 2026-04-02

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

### Build

- **Switched to `just-buildit` PEP 517 backend**: replaces
    `uv_build` + `scripts/retag_wheel.sh`; `just-buildit` calls
    `make just-build`, detects platform from the `.so` suffix, tags
    the wheel correctly, and runs `uvx auditwheel repair` / `uvx delocate-wheel` — all in one `python -m build` invocation
- **macOS arm64 wheels**: `macos-14` runner added to the release
    matrix for Python 3.12 and 3.13

______________________________________________________________________

## [0.2.5] — 2026-04-02

### Changed

- **Default build type `Release`**: `make build` now compiles at
    `-O3 + LTO`, matching the performance numbers in published
    benchmarks (previously `RelWithDebInfo` / `-O2`)

### Fixed

- **`pipeline_demo` missing `pthread`**: LTO resolves all symbols
    directly at link time; `pipeline_demo` used pthreads transitively
    but didn't declare it, causing an undefined reference to
    `pthread_join` under GCC 14 in the manylinux container
- **`python/CMakeLists.txt`: manylinux Python extension build**:
    replaced `uv run python` with `${Python3_EXECUTABLE}` for NumPy
    include-dir and `EXT_SUFFIX` discovery — uv is not present inside
    the manylinux container; added `pip install numpy` to cibuildwheel
    `before-build` so cmake can locate the NumPy headers (numpy is a
    project dep, not a build dep, and is not installed before the
    build step runs)
- **`python/CMakeLists.txt`: macOS Python extension linking**:
    replaced the `string(FIND suffix "so" ...)` hack with
    `if(NOT (UNIX AND NOT APPLE))` — the old check incorrectly treated
    macOS `.cpython-312-darwin.so` as Linux and skipped linking
    `Python3::Python`, causing `_PyExc_ImportError` undefined-symbol
    errors with cibuildwheel's isolated Python on macOS
- **`release.yml`**: removed `macos-14` from the cibuildwheel build
    matrix (macOS arm64 wheels can be added back once the macOS
    extension build is validated end-to-end)
- **`make pyext`**: passes `-DPython3_EXECUTABLE` from the uv venv
    so extension suffixes always match the active interpreter; fixes
    `ModuleNotFoundError` when running pytest under Python 3.13

______________________________________________________________________

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

______________________________________________________________________

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

______________________________________________________________________

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
    - Windows matrix; Python 3.12/3.13 pytest job; Docker build + smoke-
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

[0.1.0]: https://github.com/doppler-dsp/doppler/releases/tag/v0.1.0
[0.2.0]: https://github.com/doppler-dsp/doppler/compare/v0.1.0...v0.2.0
[0.2.3]: https://github.com/doppler-dsp/doppler/compare/v0.2.0...v0.2.3
[0.2.5]: https://github.com/doppler-dsp/doppler/compare/v0.2.3...v0.2.5
[0.2.6]: https://github.com/doppler-dsp/doppler/compare/v0.2.5...v0.2.6
[0.2.7]: https://github.com/doppler-dsp/doppler/compare/v0.2.6...v0.2.7
[0.2.8]: https://github.com/doppler-dsp/doppler/compare/v0.2.7...v0.2.8
[0.2.9]: https://github.com/doppler-dsp/doppler/compare/v0.2.8...v0.2.9
[0.3.1]: https://github.com/doppler-dsp/doppler/compare/v0.2.9...v0.3.1
[0.3.2]: https://github.com/doppler-dsp/doppler/compare/v0.3.1...v0.3.2
[0.3.3]: https://github.com/doppler-dsp/doppler/compare/v0.3.2...v0.3.3
[0.3.4]: https://github.com/doppler-dsp/doppler/compare/v0.3.3...v0.3.4
[0.3.5]: https://github.com/doppler-dsp/doppler/compare/v0.3.4...v0.3.5
[0.3.6]: https://github.com/doppler-dsp/doppler/compare/v0.3.5...v0.3.6
[0.3.7]: https://github.com/doppler-dsp/doppler/compare/v0.3.6...v0.3.7
[0.4.0]: https://github.com/doppler-dsp/doppler/compare/v0.3.7...v0.4.0
[0.4.1]: https://github.com/doppler-dsp/doppler/compare/v0.4.0...v0.4.1
[0.5.0]: https://github.com/doppler-dsp/doppler/compare/v0.4.1...v0.5.0
[0.5.1]: https://github.com/doppler-dsp/doppler/compare/v0.5.0...v0.5.1
[0.5.2]: https://github.com/doppler-dsp/doppler/compare/v0.5.1...v0.5.2
[0.5.3]: https://github.com/doppler-dsp/doppler/compare/v0.5.2...v0.5.3
[0.5.4]: https://github.com/doppler-dsp/doppler/compare/v0.5.3...v0.5.4
[0.5.5]: https://github.com/doppler-dsp/doppler/compare/v0.5.4...v0.5.5
[0.6.0]: https://github.com/doppler-dsp/doppler/compare/v0.5.5...v0.6.0
[0.7.0]: https://github.com/doppler-dsp/doppler/compare/v0.6.0...v0.7.0
[0.8.0]: https://github.com/doppler-dsp/doppler/compare/v0.7.0...v0.8.0
[unreleased]: https://github.com/doppler-dsp/doppler/compare/v0.8.0...HEAD
