# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

______________________________________________________________________

> **API stability notice** — doppler is pre-1.0. Minor releases may
> make breaking changes. Check this file before upgrading.

## [Unreleased]

### Changed

- **Richer type stubs, derived from the C headers (jm 0.33.12).** The `.pyi`
    stubs for the `Ddcr` handle (`doppler.ddc`) and the `Plan` handle
    (`doppler.wfm` save/restore surface), plus the `SampleClock`/`StreamSink`
    standalones, now carry full docstrings — class summary, `Parameters`,
    method docs, and (for `Ddcr.execute`) a runnable header-derived doctest —
    synthesized from each object's backing-header Doxygen instead of the old
    generic `"<Type> handle."` template. `Plan.save`/`dump`/`render`/`at` and
    the `PlanFromBlob`/`PlanFromFile` factories are now properly documented at
    the type-checker surface. No runtime/behavior change.

## [0.37.1] — 2026-07-24

### Fixed

- **`PSD.band_power` / `total_band_power` were scaled by the window's
    equivalent noise bandwidth (ENBW).** Band-*integrated* power normalised by
    the coherent gain `cg²` — correct for reading a single tone's peak bin in
    `psd_db`, but wrong for integrating power across a band, where the leaky
    main lobe must be normalised by the noise-power gain `nfft·s²`. The two
    differ by exactly the window's ENBW, so an absolute band power read
    **1.5× too high for a Hann window, 2× for Blackman-Harris**, and with
    zero-padding a further `nfft/n` on top (6× / 8× at `pad=4`) — i.e. it
    tracked the window instead of the signal. Now normalised by `nfft·s²` (the
    same per-bin calibration `nprmeas` already applied): a full-scale tone
    integrates to 0 dBFS and a noise band to its true variance, window- and
    pad-invariant (Parseval). *Relative* band powers (channel-to-channel
    ratios, ACLR, `snr`, `occupied_bw`) are unchanged — the error was a common
    factor that cancelled in any ratio. The raw per-bin accessors
    (`power_onesided`/`power_twosided`) keep their documented `cg²` convention.

## [0.37.0] — 2026-07-23

### Added

- **`wfm.Plan` save / restore** — a prepared `Plan` (the prepare-once stimulus
    engine) can now be serialized so its one-time DSP is paid once across
    processes or machines. `plan.save()` returns the cache as `bytes`,
    `plan.dump(path)` writes it to a file, and the module factories
    `PlanFromBlob(blob)` / `PlanFromFile(path)` reconstruct a `Plan` without
    re-running `prepare()`. The blob carries a build-time DSP-source
    fingerprint, so a stale cache transparently rebuilds rather than returning
    wrong samples. Restore of a large scene is a `memcpy` instead of the full
    `build_synth` DSP (55 s → milliseconds in the WCDMA case).
- **`Plan.prepare()` runs across cores** — the per-source DSP for a
    many-signal segment is now fanned out over a bounded pthread pool
    (`dp_parallel.h`, doppler's first C-level threading), gated on signal
    count and sample size so tiny scenes stay serial. Bit-for-bit identical to
    the serial result; ~9× on a 20-core host for a crowded band. See the
    [A Crowded Band](docs/gallery/crowded-band.md) gallery example.

### Removed

- **`doppler.wfm.read_iq`** (and the `doppler.wfm.readback` module) — the
    pure-Python interleaved-I/Q reader is retired in favour of the C
    [`doppler.wfm.Reader`](docs/api/python-wfmgen.md), which supersedes it: same
    `(path, sample_type, endian)` arguments, the wire-type → unit-scale
    `complex64` deinterleave/rescale done in C rather than NumPy, plus
    container auto-detection (BLUE / SigMF / CSV / raw) `read_iq` never had.
    This removes the last pure-Python DSP logic from `doppler.wfm`. Migrate:

    ```python
    # before: y = read_iq(path, sample_type)
    with Reader(path, sample_type=sample_type) as r:
        y = r.read(r.num_samples)
    ```

    The one behaviour `Reader` does not reproduce is `read_iq(..., raw=True)`'s
    zero-copy `(N, 2)` on-disk-dtype view — that is a plain
    `numpy.fromfile(path, dtype).reshape(-1, 2)` with no doppler logic.

## [0.36.0] — 2026-07-22

Three silent data-corruption bugs in the BLUE reader, found by auditing the
Header Control Block parse against the Midas BLUE 1.1 specification after the
first one turned up. Each returned wrong samples with no error, correct-looking
`file_type`/`fs`, and no way for a caller to notice.

### Fixed

- **A detached BLUE capture opened by its header returned the 512-byte HCB as
    IQ.** The reader inferred "detached" from the `.det` extension and never
    read the HCB's `detached` field (offset 12), so a header file parsed its
    HCB, seeked to `data_start` — 0 for a detached capture — and handed back
    the header itself as 64 cf32 "samples", the first being the ASCII
    `BLUEEEEI` magic as two floats. Per BLUE §3.1.1.4 the header is the normal
    entry point (`<base>.tmp`/`<base>.prm`; doppler writes `<base>.hdr`) and
    the payload is the collocated `<base>.det`, so the extension must not
    decide — `detached` does. All four entry points now yield the same capture.
- **Scalar BLUE captures were read at the complex stride.** The `format` field
    (HCB bytes 52–53) is a `[mode][type]` pair and only the type half was
    parsed. A valid `S` (scalar) capture was walked as interleaved I/Q: every
    second real sample became a phantom Q, the capture came back at half its
    length, and `num_samples` under-reported 2x. Mode is now parsed — `S` reads
    one component per sample with `imag == 0`, `C` is unchanged, and every
    other Midas mode (`V`/`Q`/`M`/`T`/…, three or more components per sample)
    is **rejected at open** rather than strided as if it were I/Q.
- **Reads ran past the declared payload.** BLUE states its data size, but the
    reader streamed to EOF, so draining a capture that carries an extended
    header returned keyword bytes decoded as IQ. Reads are now bounded by the
    declared sample count.
- The async DSSS spec-demo's prose described its Doppler inputs in the wrong
    units (they are Hz and Hz/s, converted internally to ppm of carrier).

### Added

- **BLUE extended-header keywords**, read and written (Midas BLUE 1.1 §3.3.1,
    Table 26). `ext_start`/`ext_size` were previously never parsed and hardcoded
    to zero on write, so a file's entire metadata region was invisible in both
    directions. One codec — `native/src/wfm/wfm_keywords.c` — serves both
    sides, so encode and decode cannot drift apart. Types `B`/`I`/`L`/`X`,
    `F`/`D`, `A` (a variable-length string in keyword context) and the
    deprecated `T`; unrecognised types are stepped over rather than aborting
    the walk, per §3.3.1. C API: `wfm_writer_add_keyword()`,
    `wfm_reader_num_keywords()`/`_keyword()`/`_find_keyword()`. A Python
    surface will follow with the Reader/Writer object migration.
- **`Reader.mode`** — `"complex"` or `"scalar"`, parsed from the BLUE `format`
    mode designator; other containers are complex.
- **`wfm_reader_reset()`** rewinds to the first sample of the capture (512
    bytes into an attached BLUE file, byte 0 of a `.det` or raw/SigMF payload),
    leaving the container metadata and decoded keywords intact.
- **`doppler.wfm.Reader.keywords`** now appears in the type stub as
    `dict[str, Any]`, and `close()`/`destroy()` on both `Reader` and `Writer`
    now appear too — they were runtime-only before.
- **`doppler.wfm.Writer.add_keyword(tag, type, value)`** — the write half of
    the Python keyword surface, completing what `Reader.keywords` reads.
    `value` is a `str` (type `"A"`), a single `int`/`float`, or a sequence of
    them; keywords are buffered and written at `close()`. Its C→Python value is
    data-dependent on the type code, so — like `Reader.keywords`' value builder
    — the marshaling is one hand-written binding method rather than generated.
- **`F32Buffer`/`F64Buffer`/`I16Buffer.available`** — samples written but not
    yet consumed; the largest `n` for which `wait(n)` returns without spinning.
    `wait()` has no timeout, so a consumer that over-counts hangs; read from the
    consumer side, `available` is a safe lower bound to size blocks from.

### Changed

- **C API (breaking for C consumers).** The reader/writer cores moved onto the
    standard object layout and their lifecycle functions were renamed:

    | before                        | after                                     |
    | ----------------------------- | ----------------------------------------- |
    | `#include "wfm/wfm_reader.h"` | `#include "wfm_reader/wfm_reader_core.h"` |
    | `#include "wfm/wfm_writer.h"` | `#include "wfm_writer/wfm_writer_core.h"` |
    | `wfm_reader_open()`           | `wfm_reader_create()`                     |
    | `wfm_reader_close()`          | `wfm_reader_destroy()`                    |
    | `wfm_writer_open_path()`      | `wfm_writer_create()`                     |

    `wfm_writer_open(FILE *, …)` keeps its name — it is a secondary
    constructor, not the object's ctor. `wfm_reader_t`/`wfm_writer_t` still
    work as aliases for the new `*_state_t` names. Python is unaffected.

- **`doppler.wfm.Writer.reset()` is removed.** A writer has nothing coherent to
    reset (its samples are on disk and the written count drives the BLUE
    `data_size` patch), so the method is now absent — `w.reset()` raises
    `AttributeError` rather than the previous `NotImplementedError`. Construct a
    new `Writer` for a new capture.

- Tooling: the just-makeit pin moves to 0.33.6, picking up the doppler-filed
    features behind the Reader/Writer object migration and its fully-declarative
    follow-up — gh-514/515/519/521/523 (path ctor args, meaningful `create()`
    failures, enum-valued properties, out-of-range enum guards, object-module
    packaging) and gh-541/542/543/544 (fallible/renamable destructors,
    `no_reset`, dict-valued properties). gh-521 fixes a memory-safety bug in the
    enum getters; doppler's own paths clamp every one of those fields before
    storing it, so the out-of-range read was not reachable here.

## [0.35.0] — 2026-07-21

Continuous asynchronous DSSS, end to end: a spread-spectrum signal can now be
synthesised, driven through coupled clock Doppler, and acquired/tracked back to
bits by packaged receiver objects.

### Added

- **`doppler.dsss.AsyncDsssReceiver`** — packaged continuous asynchronous DSSS
    receiver (acquire → refine → track) with carrier→code aiding for coupled
    clock Doppler. `steps()` accepts any block size with state carried across
    calls, so one epoch per call equals one big call.
- **`doppler.dsss.DsssReceiver`** — packaged burst DSSS receiver over the same
    acquisition/tracking primitives.
- **`doppler.dsss.BurstAcquisition`** — 2-D (code phase × Doppler) burst DSSS
    acquisition.
- **`doppler.acquire.CarrierAcquisition`** — PSDMF carrier-frequency estimator
    (a C port of the periodogram-sum-of-differences maximum-of-forward
    estimator).
- **`doppler.impairment.DopplerChannel`** — coupled clock Doppler modelled as
    time dilation plus a coherent carrier, ppm-parameterised (a starting offset
    and a linear rate).
- **Gold-code generation** — maximal-length Gold code pairs for spreading.
- **Continuous DSSS synthesis** in `doppler.wfm.Synth` (`symbol_rate > 0`): an
    endlessly repeating spreading code carrying data at a symbol rate
    independent of the code epoch (non-integer chips/symbol), with code-only,
    user-payload, or PRBS data sources.
- **Interpolation primitives** (`interp`, `interp_table`).
- State serialization (the `state_bytes`/`get_state`/`set_state` bytes triplet)
    for the new stateful objects.

### Changed

- **`nco_norm_to_inc` (the shared LO / NCO / DLL phase-increment conversion)
    now truncates toward zero** — the standard fixed-point / DDS
    phase-accumulator convention — instead of rounding to nearest. This makes
    the increment bit-identical across hosts and removes an arm64 NCO-overflow
    edge case; the realised frequency is now at most one quantization step
    (`fs / 2^32`) low, never high. Generated carriers (`Synth` tones, `LO`,
    `NCO`) shift by at most that one step.
- **`Acquisition.push()`'s 6th tuple field is now `cn0_dbhz_est` (dB-Hz),
    replacing the linear `snr_est`.** The old field reported a per-sample
    amplitude ratio backed out of the CFAR test statistic
    (`test_stat / sqrt(2*pi) / sqrt(2*n)`) — bandwidth-dependent ("per-
    sample" really meant "normalised by the sample rate"), not portable
    across `spc`/`reps` configurations, and gave no legible sense of link
    margin: a rock-solid detection (`test_stat` in the dozens) could still
    report a small, flat linear ratio, reading as broken even when it
    wasn't. `cn0_dbhz_est` inverts the same statistic back through the
    engine's own C/N0-to-amplitude-SNR sizing transform, so it's directly
    comparable to the `cn0_dbhz` the engine was constructed with — it
    tracks true C/N0 while AWGN dominates the CFAR noise estimate, and
    saturates at the code's own autocorrelation-sidelobe floor once C/N0
    exceeds what the code/geometry can resolve (a real ceiling, not a
    bug). Verified against known-C/N0 injected AWGN to within ~1 dB.
    `doppler.dsss.orchestrator.Detection.snr_est` is renamed
    `cn0_dbhz_est` to match.

### Fixed

- **`dll_init` now initializes `rate_aid`.** The in-place (stack-embedded)
    `Dll` init left the carrier-aiding rate bias uninitialized; on a host whose
    stack held a NaN there (macOS/arm64) it made the steered phase increment
    degenerate and cast to zero, freezing the code NCO permanently. Benign on
    hosts whose stack held 0 (Linux/x86). Any object embedding a `Dll` by value
    was affected.
- **`marcum_q` underflow and CarrierAcquisition threshold calibration** in the
    detection / acquisition path.

## [0.34.0] — 2026-07-14

Four issues closed and one bug found and fixed along the way, all from a
single "clear the open-issue backlog" pass.

### Added

- **`Plan` sweeps multi-segment / repeated / ranged-gap scenes.** The
    prepare-once stimulus cache now supports any number of finite segments,
    `repeats=N` bounded instancing, ranged `off_samples`/`delay_samples`
    (redrawn per instance from the Plan's seed), and a lone bundled noisy
    source (its AWGN reconstructed via a per-instance noise synth rather
    than an external multiply) — the canonical 5-burst DSSS train can now
    be swept in place instead of re-composed per point. Still out of scope:
    a ranged on-time or any ranged per-source field (both would invalidate
    the cached signal render). Fixes #410.
- **`doppler.filter.design_lowpass`** — a one-call Kaiser-windowed-sinc
    lowpass FIR design helper (`fpass`/`fstop` Nyquist-normalised band
    edges, `atten_db` stopband target); `n_taps` is auto-sized via
    `doppler.resample.kaiser_num_taps`. Closes the last reason a doppler
    example would reach for `scipy`. Fixes #453.

### Fixed

- **specan web UI: negative tone-frequency wrap.** `DemoSource` wrapped a
    signed tone offset into `[0, 1)` via a plain `% 1.0`, so a tone set
    below center (or the negative half of a chirp sweep) silently reported
    a near-Nyquist alias instead — the web UI marker vanished or jumped.
    Now wraps into `(-0.5, 0.5]`. Fixes #457.
- **specan web UI: WebSocket receive desync.** `websocket_endpoint()`
    interleaved outgoing frames and incoming commands via a
    timeout-cancelled `receive_text()`; cancelling it mid-flight desynced
    Starlette's receive state, so a rapid slider drag silently dropped
    every subsequent command — including unrelated controls — for the
    rest of the connection. Split into two independent concurrent tasks
    (a plain, uncancelled receive loop; a separate send loop) so a
    receive is never cancelled mid-flight. Found while manually testing
    the #457 fix. Fixes #475.

### Investigated

- **Acquisition blind-sweep false-alarm rate — settled as normal
    variance, not a calibration gap.** A single 471-dwell overlapping
    blind sweep once measured 3 false alarms against a naive
    `pfa * n_dwells` estimate of ~0.47. A follow-up 2.34M-dwell
    Monte-Carlo study across four overlap fractions (0%/50%/75%/87.5%)
    found every condition within ±1.8 std devs of the naive estimate,
    with no trend toward inflation as overlap increases — ordinary
    Poisson variance (`P(X>=3 | lambda=0.47) ~ 1.5%`), not a per-dwell
    calibration or composability gap. No engine changes. Fixes #394.

## [0.33.5] — 2026-07-13

Stream-component ergonomics, prompted by a real aarch64 user hitting
`undefined reference to dp_pub_create` hours after 0.33.4 — the
two-component core/stream split stays (its rationale held), and the
remaining friction is gone and gated.

### Added

- **`doppler_stream.pc`** — one pkg-config name for the whole streaming
    link line: `pkg-config --cflags --libs doppler_stream` emits both
    libraries in the right order (`Requires: doppler`), relocatable like
    `doppler.pc`. Ships in the release tarballs and any
    `cmake --install` tree.
- **The three consumer faces, CI-verified** — one consumer (the FFT
    example plus an optional `dp_pub_*` call) is built via bare `cc`
    (static archives), CMake (`find_package` + `doppler::stream`), and
    pkg-config, and the three binaries' output is asserted identical —
    continuously in CI against a fresh install prefix, and in the
    post-release smoke against the published tarball on linux-x86_64,
    linux-aarch64, and macos-arm64.
- **C Quick Start page** (`quickstart-c.md`, top-level nav) — get the
    library via `jbx get-doppler`, the consumer, and the three faces as
    tabs; every snippet is included from the tested files, so the shown
    commands are the smoke commands.
- **C snippet gate: `no-run=` and `broker=` markers** — a complete
    program that only can't *run* headless keeps its full `-Werror`
    compile check (`skip=` used to drop it); `broker=` additionally runs
    the snippet wherever a NATS broker is reachable (CI provides one).

### Fixed

- The streaming examples page never showed a link command — it now
    opens with the two-archive link line and names the exact
    `undefined reference to dp_pub_create` symptom (`dp_pub_*` lives in
    the optional `libdoppler_stream`, not the core).
- `install/c.md`'s "System install" led with `cmake --install build`,
    presupposing a source tree — it now leads with
    `jbx get-doppler --prefix /usr/local` and keeps the from-source
    variant second.

## [0.33.4] — 2026-07-13

A docs-quality release, culminating in a six-phase campaign
(#462–#469): every doc surface is now generated-or-gated — single source
of truth everywhere, every code example executed and self-validating in
CI. One real API-behavior fix (`CorrDetector`/`CorrDetector2D` `dwell`
default, below) and one build-requirement fix (no C++ toolchain needed,
ever) ride along. Prompted by a hands-on pass over the quickstart,
README, and the docs/design + docs/dev trees — several examples had
never actually been run against a live install, and a batch of
design/contributing docs had drifted behind shipped work.

### Added

- **Fail-closed doc + example gates** — three fence gates run every
    python, C, and shell code block under
    `docs/` (the shell gate parse-validates every documented
    `doppler`/`doppler-specan` invocation against the CLIs' real argparse
    parsers and executes safe `wfmgen` fences end-to-end); every
    `src/doppler/examples/*.py` is glob-discovered and run by
    `test_examples.py` and must **self-validate** with physical asserts
    (measured SNR/ENOB vs theory, loop lock, BER thresholds, byte-exact
    round-trips) — 41 scripts gained them. Escape hatches
    (`skip=`/`no-exec=`/`broker=`) all require reasons, meta-enforced.
    The docs build is now `--strict` and `scripts/check_site_links.py`
    fails CI on any broken internal link/anchor in the built site.
- **`scripts/gen_readme.py`** — `README.md`'s entire body below the
    badges is generated from `docs/index.md` (admonitions rewritten to
    GitHub alert syntax, relative links `docs/`-prefixed). Closed the gap
    that let the two drift repeatedly — including README nav links that
    404'd on GitHub and a Performance/Licensing drift that sat exactly
    outside the first, quickstart-only sync block. Wired into
    `make docs-relink` + a CI drift-check.
- **`scripts/gen_install_scripts.py`** — the per-distro
    `tests/install/build-*-deps.sh` (rendered into the install docs) are
    generated projections of `jb.toml`'s `[dev.*]` lists, ending the
    three-copy drift that had the dnf/zypper docs demanding a `gcc-c++`
    the SSOT never listed. CI drift-check included.
- **`scripts/check_version_strings.py`** — CI fails if the current
    release version is hand-typed into README/docs prose (such claims go
    stale at the very next release; one already had).

### Fixed

Real bugs found by actually running the quickstart/example code against
live installs, rather than trusting it by inspection:

- **`CorrDetector`/`CorrDetector2D` ignored their documented `dwell`
    default** — the binding fragments initialized an omitted `dwell` to 0
    instead of the manifest's 1, producing a detector that could never
    int-dump (`push()` never returned a result). Found the moment
    `corr_demo.py` gained self-validation: its `CorrDetector2D` section
    had silently never run. Fixed in both fragments + regression tests.
- **No C++ toolchain is needed anywhere in the build — now actually
    true.** Vendored `nats.c`'s bare `project(cnats)` enabled CMake's
    default C+CXX, so configuring the stream component probed for a C++
    compiler despite compiling zero C++ sources (and `gcc-c++` had crept
    into the install docs to match). Patched to `LANGUAGES C`; verified
    with a full build with every C++ compiler removed from PATH.
- **`hbdecim_q15_demo`'s "frequency response" panel was a flat line** —
    the impulse landed in the halfband's pure-delay polyphase branch (a
    2:1 decimator computes `y[m] = h[2m−d]`, so one impulse samples every
    other tap). Replaced with a two-phase impulse reconstruction; the
    committed gallery figure now shows the real response (±0.011 dB
    passband, −58 dB stopband).
- **Missing `fs=` on `Segment.sum`** in two wfm demos *and* the
    wfm-composition gallery page silently resolved scenes at the default
    `fs=1.0`, aliasing their annotated "+200 kHz"/"+120 kHz" tones to DC
    — the committed composition figure was regenerated from before this
    class of fix and had shown the interferer at DC.
- **`dsss_despread_demo` dropped a symbol** whenever a block dumped two
    (`soft.append(s[0])` → `soft.extend(s)`), desynchronizing the
    demodulated stream after ~50 symbols.
- **`measure_imd_npr_demo`'s TOI fit** included below-analysis-floor
    points, skewing the IM3 slope to 2.8 and the extrapolated intercept
    by 4.7 dB.
- **Two `architecture.md` commands that never existed** — `doppler   compose ps` (it's `doppler ps`) and `doppler specan` (the binary is
    `doppler-specan`) — caught by the new shell-fence gate's first run.
- **Python 3.9 compatibility** in an example's runtime type hints
    (PEP 604 `X | None` → `Optional[X]`), caught by the example gate
    running on the full version matrix (#469).
- **`Publisher`/`Subscriber` streaming example** — `Publisher(endpoint)`
    with no `sample_type` defaults to `CF64`, but the example sent
    `complex64` (`CF32`) samples — guaranteed `TypeError` on `.send()`.
    `Subscriber.recv()` returns a `(samples, header_dict)` tuple, not an
    object with `.samples`/`.sample_rate`/`.seq` attributes — both the
    "Subscriber (Python)" and "C transmitter → Python subscriber"
    sections treated it as the latter. Verified the fix against a real
    `nats-server`, including building and running the actual C
    `transmitter` binary end to end.
- **Pipeline CLI example** — `doppler compose init --name X` was missing
    the required positional `BLOCK` args; `compose up --file X` doesn't
    exist (`up` takes a positional `FILE`); `doppler logs` needs a
    positional chain ID. Verified the corrected sequence against a real
    running chain.
- **wfmgen streaming example** — showed `--output zmq://tcp://*:5555`,
    dead since the ZMQ→NATS transport migration; `--output` only accepts
    `FILE|-|nats://HOST:PORT/SUBJECT` today. Confirmed the old form
    fails (`error: cannot open output`) and the `nats://` fix streams
    cleanly. Also fixed a stale "ZMQ sink" cross-link in
    `docs/gallery/wfm-io.md` (the actual example already uses NATS).
- **Bench throughput summary namespacing** (`conftest.py`) — the
    terminal `pytest_terminal_summary` hook printed the raw
    pytest-benchmark test name; every `bench_*.py` file draws case names
    from the same small vocabulary (`test_bench_step`,
    `test_bench_steps_64k`, ...), so 24 different modules share one name
    and the summary was full of indistinguishable duplicate lines.
    Reused `scripts/bench_report.py`'s existing `module::case`
    disambiguation logic.
- Quickstart's "FIR filter" and "Resample" sections silently depended on
    `x` defined three sections earlier (the "page is one notebook" gate
    convention) — fine for the automated gate, but a `NameError` trap for
    a reader who jumps straight to either section. Both are now
    self-contained.
- The FIR filter example imported `scipy.signal.firwin`, the only
    non-numpy dependency in an otherwise numpy-only quickstart. Replaced
    with a windowed-sinc design using doppler's own
    `doppler.spectral.kaiser_window` + `kaiser_beta_for_sidelobe`.
- `architecture.md`'s layer diagram and Layer 1 description both listed
    6 modules (NCO/FIR/FFT/DDC/Resampler/Buffer) as if that were the
    whole DSP library — it's actually 40 modules.
- The stale `[unreleased]` compare link at the bottom of this file
    pointed at `v0.33.1...HEAD` instead of the actual latest tag.

### Changed

- **Gallery pages single-source their code** — 19 gallery pages'
    substantial code blocks are now `--8<--` includes of marked regions
    in the CI-run example scripts, so page, tested script, and committed
    figure are one artifact (the conversion itself surfaced and fixed a
    half-dozen page-only drift bugs: prose describing figures that don't
    exist, inline parameters contradicting the committed PNGs).
- **Contributing docs separated by audience** — maintainer plumbing
    (release, build-internals, coverage) nests under a labeled
    "Maintainer internals" nav group; four completed historical records
    moved to `docs/dev/archive/` (out of nav, banners intact).
- Hand-maintained counts in prose ("40 modules", "40+ examples") became
    resilient quantifiers — exact numbers now appear only where something
    generates or checks them.

### Docs

A sweep of `docs/design/` (14 pages) and `docs/dev/` (11 pages, `+`
`wfmgen/api.md`) against actual current code turned up several pages
whose status/roadmap sections had fallen behind shipped work:

- **`docs/dev/adding-a-module.md`** — rewritten to match the current
    `jm` workflow: added the `--preset blockwise`/`generator` path (the
    primary route for the whole block-I/O object class — resampler/FFT/
    decimator/generator — which wasn't documented at all), the pinned
    `uvx --from 'just-makeit==0.28.11' jm ...` invocation, the mandatory
    state-serialization step, and manifest registration. Fixed three
    nonexistent Makefile targets used throughout (`make bench-python`,
    `make bench-c`, `make docs-build`) to the real `make bench`/`make docs`.
- **`docs/dev/wfmgen/api.md`** — reframed as the historical decision
    record it actually is (the 0.11.0 API cleanup + 0.23.0 addendum),
    with a banner pointing to the actively-maintained
    `docs/guide/wfmgen/` for the current surface, instead of
    hand-maintaining a second, increasingly-stale copy of it.
- **Five `docs/design/` pages** (`api-taxonomy.md`, `RESAMPLER.md`,
    `dsss-acquisition.md`, `corr2d-interpolated-inverse.md`, `mpsk.md`,
    plus `acq-fn.md`) described shipped work as still-proposed,
    never-built, or open — corrected against git history and current
    `native/src/`.
- **`docs/dev/error-convention.md`** — the entire error-code table was
    wrong (values didn't match `clib_common.h` at all, one code missing
    entirely) and described a two-header split that no longer exists.
- **`docs/dev/module-layout.md`** — added the `<module>_ext_<component>.c`
    hand-owned fragment pattern and the state-serialization requirement,
    both load-bearing per this repo's conventions but absent from the
    page entirely.
- **`docs/dev/benchmarking.md`**, **`docs/dev/release.md`**,
    **`docs/dev/build-internals.md`** — fixed the same stale bench-naming
    guidance as the `conftest.py` fix above, and documented the aarch64
    build leg's SVE portability gate and `publish-container` job, both
    added to the release pipeline without the docs catching up.
- **`docs/dev/wfm-validation-findings.md`** — added a status banner
    (all three findings were already individually resolved) matching its
    sibling historical-record pages.
- Restructured the quickstart's "Build from source" section: `jbx   get-doppler` (already covered above, for the C library alone) is now
    clearly the fast path; this section is scoped to what actually needs
    a build (examples, Rust FFI, tests, contributing); `make install-deps`
    replaces the bare per-distro package-manager commands as the primary
    path, with those preserved in a collapsed "install by hand" block.
    Also fixed the "No C++ compiler needed" callout, which read as "no
    compiler needed at all" to anyone unfamiliar with the project's
    C++-avoidance history.

## [0.33.3] — 2026-07-12

### Added

- **`jbx get-doppler`** — a one-command installer for the pre-built C
    library (`scripts/get-doppler.sh`): resolves the latest release,
    downloads the platform-appropriate tarball, and extracts it to a
    prefix — no toolchain, no cloning/building doppler itself. Good
    defaults (`$HOME/doppler`) with `--prefix`/`--version` for advanced
    users. A previous install at the same prefix is moved aside to
    `PREFIX/.get-doppler-backup` before a new one is extracted, restored
    automatically if the new one fails a sanity check, and restorable any
    time with `--restore`. Documented in `install/c.md` ahead of the
    existing manual curl/tar steps, which stay as the fallback.
- **Linux aarch64 C library tarball** (`doppler-X.Y.Z-linux-aarch64.tar.gz`)
    — built natively on `ubuntu-24.04-arm` (the same runner already used for
    the aarch64 Python wheel leg), closing the gap that left arm64 Linux
    users with no pre-built C library to download via `get-doppler`.
    `tests/install/release-smoke.sh` gained a matching `Linux/aarch64` case
    (it had the same OS-only detection bug `get-doppler.sh` originally did)
    and now smoke-tests all three published platforms. Verified against a
    real build on native ARM64 hardware before merging: a genuine `ARM   aarch64` `libdoppler.so`, all expected files present, C++-free static
    lib, and a real consumer program compiled and ran against the
    installed prefix.

### Fixed

- `get-doppler.sh`'s platform detection only checked `uname -s` (OS), never
    `uname -m` (CPU architecture) — an arm64 Linux box silently downloaded
    the x86_64 tarball instead of erroring. Now switches on `OS/ARCH` and
    errors clearly for any combination with no published tarball.

### Docs

- The homepage (`docs/index.md`) had no markdown H1, so mkdocs-material
    filled the gap with a synthesized heading using the page's title
    (falling back to the filename, "Index") — visible above the wordmark and
    in the sticky header-topic bar when scrolled. The wordmark image is now
    the H1 itself (a common mkdocs-material logo-as-title idiom): the
    browser title and header-topic resolve cleanly with no visible or
    duplicate text, and the page shows just the wordmark, tagline, and
    badges below, as intended.

### Changed

- **jm pin 0.28.9 → 0.28.11.** Picks up gh-468 (filed this cycle): jm's
    decl-injector had no case for a non-static, header-only, self-defining
    module function, so it injected a redundant, malformed forward
    declaration for `square_clip` after it was made non-static (to fix a
    GCC `-Wstatic-in-inline` warning from a non-static `always_inline`
    caller referencing a static callee). `native/inc/util/util_core.h`
    comes off `status_allow` — the underlying gap is fixed, so the
    workaround is no longer needed.
- `pre-commit autoupdate`: ruff-pre-commit v0.15.18 → v0.15.21,
    clang-format v22.1.5 → v22.1.8.

## [0.33.2] — 2026-07-12

### Added

- **C snippet testing gate** (`src/doppler/tests/test_c_doc_snippets.py`) —
    every ```` ```c ```` fence under `docs/` is now compiled and run in CI
    against `build/libdoppler.a` (+ `build/libdoppler_stream.a` when a
    snippet needs the NATS wire layer), mirroring the existing Python
    doc-snippet gate's fail-closed, discovered-not-registered philosophy.
    Motivated by the homepage's own C "Quick start" snippet sitting broken
    for a release: missing `#include <complex.h>`, undeclared arrays,
    top-level statements outside `main()`. Shares include-resolution and
    marker-parsing logic with the Python gate via a new
    `_docs_snippet_common.py`.
- **`jbx get-doppler`** — a one-command installer for the pre-built C
    library (`scripts/get-doppler.sh`): resolves the latest release,
    downloads the platform-appropriate tarball, and extracts it to a
    prefix — no toolchain, no cloning/building doppler itself. Good
    defaults (`$HOME/doppler`) with `--prefix`/`--version` for advanced
    users. Documented in `install/c.md` ahead of the existing manual
    curl/tar steps, which stay as the fallback.

### Fixed

- **`square_clip`/`dp_tlm_emit`/`dp_buffer_write` linkage warnings.**
    `agc_step()` (non-static, `JM_FORCEINLINE`) called `square_clip()`
    (`static`), tripping GCC's `-Wstatic-in-inline`; `dp_tlm_emit()` had
    the same issue with its own callee, `DECLARE_DP_BUFFER`'s generated
    `write()`. Both now carry `JM_FORCEINLINE`'s `always_inline` guarantee
    instead of plain non-static `inline` — matching the pattern already
    used by the codebase's other shared header-only helpers — which also
    avoids the "needs an out-of-line definition somewhere" C99 pitfall a
    plain `inline` fix would hit.
- Running the new C-snippet gate against the real docs surfaced genuine
    drift beyond the homepage snippet, fixed here: `fir_execute()` called
    with 5 args (real signature takes 4); `fft_execute_inplace_cf32()`
    called with 3 args (real signature takes 4, needs a separate output
    buffer); `docs/api/python-ddc.md` referenced entirely fictional
    `hbdecim_cf32_*` functions; `docs/dev/error-convention.md`'s `awgn()`
    example was missing its include; `docs/quickstart.md`'s transmitter
    command used an unsupported `cf32` sample type (real choices: `ci32`,
    `cf64`); `examples/c/hbdecim_demo.c` passed an `int` for a `%zu` format
    specifier.

### Docs

- Homepage/README/quickstart "Quick start" C example reworked so its
    prose and its actual compile command agree: both now lead with the
    pre-built tarball (`$PREFIX/include`/`$PREFIX/lib`), with
    build-from-source called out as the alternative rather than the
    default — the compile command previously only worked after a full
    from-source build, exactly the friction the prose was trying to help
    readers avoid. `README.md` and `docs/index.md` are kept from
    diverging on "install before use" for both Python and C.

### Changed

- **jm pin 0.28.8 → 0.28.9.** Picks up gh-464
    (just-buildit/just-makeit#465): `jm bench`'s display table and
    Δ-vs-prev comparison now qualify Python benchmark names with their
    component prefix, fixing ambiguous same-named benches colliding
    across components (verified against a real 7-way collision on
    `test_bench_steps_64k`).

## [0.33.1] — 2026-07-12

### Added

- **`make install-deps`** bootstraps `jbx` (installs to `$HOME/.local/bin`
    via the `get-jb.sh` installer, the same mechanism CI uses) if it isn't
    already on `PATH`, then runs `jbx install-deps`. `README.md` and
    `docs/index.md`'s Build sections now lead with it.

### Docs

- **Docs navigation/discoverability unification** — five phases plus two
    follow-ons closing the "how do I find anything" gap: a CI-enforced
    nav-index coverage gate (`scripts/check_nav_index.py`) for `design/`,
    `dev/`, and a new `docs/gallery/index.md`; a homepage + new
    `docs/start-here.md` entry-point fix; a generated `## Related pages`
    cross-link block on every `docs/api/*.md` page
    (`scripts/gen_related_pages.py`, backtick/link-text-scoped matching to
    avoid false positives from class names that double as common English
    words); a one-line `docs/c-api/index.md` ↔ `api/index.md` link-back;
    and a new `docs/dev/docs-conventions.md` contributor guide explaining
    what's generated vs. hand-owned under `docs/`.
- **All 37 pre-existing `zensical build` warnings fixed** — two dead anchor
    links, and ~33 `native/inc/*.h` Doxygen comments where bracket syntax
    (`[a / b / c]*`, range notation, array-index expressions) misparsed as
    broken markdown links once converted to markdown; fixed by
    backtick-wrapping, matching the codebase's own already-working
    convention elsewhere (e.g. `adc_core.h`). Regenerating `docs/c-api/` in
    the process also caught it had drifted badly out of sync with the
    actual header set — missing pages for `burst_despreader`, `lockdet`,
    `telemetry`, `tlm_sink`, `snr`, and `crc16`, plus a stale page for a
    `channel_core.h` that no longer exists.
- Tagline refresh: "Dead-simple, ultra-fast" → "Practical, portable,
    performant" (`README.md`, docs homepage, `pyproject.toml`,
    `mkdocs.yml`).
- The release checklist (`docs/dev/release.md`) no longer has step 1
    re-run the full test suite locally — `main`'s required CI already
    gates every merge, and the checklist's own later steps already said as
    much.

## [0.33.0] — 2026-07-12

### Added

- **Lock-detector consistency pass across `doppler.track`/`doppler.dsss`.**
    Every continuous tracking loop now carries the same `lockdet_core.h`
    verify-counted decision (level + time hysteresis) behind a
    `configure_lock`-family setter and a `.locked`-family getter:
    - `SymbolSync` gets its **first-ever lock detector** — a Gardner-style
        eye-opening ratio (`lock_signal`), block-averaged and sized from a
        closed-form `(pfa, pd)` derivation (`configure_lock`), plus a raw
        escape hatch (`configure_lock_raw`) for direct control of the
        averaging depth, threshold, and verify counts. Empirically validated
        by a 500,000-trial Monte Carlo harness
        (`native/validation/symsync_lock.c`, gated in CI).
    - `Dll` gains `configure_lock_raw()`, exposing the same raw geometry
        control `Costas` already had, for a caller composing `Dll`+`Costas`
        directly instead of through a higher-level object.
    - `CarrierNda` gets `configure_lock()`/`.locked`, wrapping its existing
        lock-signal EMA in a verify-counted decision (default `n_up=64`,
        set from direct Monte Carlo against noise-only input — a smaller,
        seemingly-reasonable default false-locked at a real, measured rate
        because the underlying EMA is autocorrelated across looks).
    - `MpskReceiver` gains a post-construction `configure_lock()` to re-tune
        its acquisition↔tracking handover detector (previously fixed at
        construction time only).
    - `Despreader` gains `configure_carrier_lock()`/`configure_code_lock()`,
        thin forwarders onto its embedded `Costas`/`Dll` loops.
    - New guide: [Lock Detection Across `doppler.track`](https://doppler-dsp.github.io/doppler/guide/lock-detection/)
        — the consistency table, which of the two `configure_lock` entry
        points (derived `pfa`-style vs. raw geometry) to reach for, and two
        standing lessons this pass surfaced (verify-count independence can
        silently fail on a fast/correlated statistic; a magic number that
        lands safe by accident still needs replacing and empirically
        re-verifying, not trusted on algebra alone).
    - New gallery page: [Full-Chain Lock-Up](https://doppler-dsp.github.io/doppler/gallery/receiver-lock/)
        — a real, cold-started `Dll(segments=K) -> Costas -> SymbolSync`
        chain with one shared `Telemetry` context, plotting all three loops'
        `.locked` traces on one timeline (the real acquisition cascade:
        code locks first, then carrier, then symbol timing).
    - A new end-to-end test suite
        (`src/doppler/track/tests/test_async_dsss_receiver.py`) proves the
        `Dll(segments=K) -> Costas -> SymbolSync` composition recovers bits
        blind (no genie carrier/timing knowledge) at a real GPS-scale link
        budget (2.046 Mcps chip rate, 1023-chip code, 1800 bps data).

## [0.32.0] — 2026-07-11

### Added

- **Linux aarch64 wheels on PyPI.** The release workflow now builds
    manylinux_2_28 **aarch64** wheels (cp39–cp314) natively on GitHub's
    arm64 runners, alongside the existing x86_64 set — `pip install   doppler-dsp` works out of the box on Graviton, Raspberry Pi, and
    Docker-on-Apple-Silicon Linux. The published container installs
    these wheels on **both** architectures, retiring the ~19-minute
    QEMU-emulated source build the `linux/arm64` image layer needed
    before (release wall-clock drops from ~30 min to ~10). An SVE
    portability gate (the aarch64 analogue of the AVX2/AVX-512 scan)
    guards the new wheels against `-mcpu=native` leaks.

- **Acquisition sizing is straddle-aware — and averages Pd, not
    amplitude.** `Acquisition` now sizes the search grid and reports
    `pd_predicted` as the **average Pd over the straddle priors**
    (slow-time Doppler scalloping, uncompensated intra-segment rotation —
    the band-edge `sinc(1/2)` = −3.9 dB effect, clamped to a tight
    `doppler_uncertainty` — and code-phase sample offset), computed by
    quadrature at setup. Previously both used the on-grid best case, so
    an engine sized near threshold silently missed its `pd` in operation
    (the gap the Monte-Carlo characterization has always measured); and
    Pd at the *mean amplitude* would still overstate the true average by
    ~0.11 at a marginal design point (Jensen — caught in review). The
    non-coherent look count escalates on the same averaged criterion.
    The new `straddle_loss` property exposes the mean amplitude derating
    as a diagnostic.

- **Calibrated whole-burst lock test + `detection.det_threshold_f`.**
    `BurstDespreader` gains `lock_stat` — `R = sqrt(stat_n · ΣRe² /   ΣIm²)`, the one-shot analog of the tracking loops' verify-counted
    detectors. Because the noise reference is estimated from as many
    samples as the signal sum, the exact H0 law is `R² = stat_n ·   F(stat_n, stat_n)` — a chi-square gate would realize **25–41× the
    priced pfa** (caught in review). The new `det_threshold_f(pfa, n)`
    helper (regularized-incomplete-beta quantile of F(n, n), exact for
    every n, odd included) prices the gate:
    `R > sqrt(stat_n · det_threshold_f(pfa, stat_n))`. Only payload
    prompts fold into the statistics (preamble prompts have a different
    code length and pull-in transients — mixing them would break both
    the H0 law and the SNR calibration). Plus `stat_n`, and a CI-tested
    H1/H0 gate check at an honest 1e-3.

- **Acquisition-handoff verify/reject recipe.** A false acquisition cell
    no longer means a tracker spinning on noise: the despreader's live
    `code_locked` plus `det_verify_delay` give a bounded-time
    accept/reject window with every constant derived from (pfa, pd)
    budgets — documented in the DSSS guide/gallery and CI-tested
    (true cell accepts, false cell rejects inside the window).

### Changed

- **Burst statistics are cumulative, not EMA (burst blob v2).** A burst
    is one-shot, and the old fixed-α=0.1 EMAs were warmup-dominated for
    an entire ~20-period burst. `lock_metric` is now the mean of
    |Re P|/|P| over every prompt; `snr_est` is accumulate-then-ratio
    `(ΣRe² − ΣIm²)/ΣIm²` — replacing the heavy-tailed per-symbol
    `Re²/Im²` EMA (a reciprocal chi-square, biased high with enormous
    variance). `snr_est` reads as the **effective post-loop SNR**
    (residual tracking jitter included) — the quantity that predicts
    demodulation performance. `BURST_DESPREADER_STATE_VERSION` 1 → 2.

- **jm pin 0.28.1 → 0.28.2.** jm#441's `status_allow` fix: `jm apply` /
    `regenerate` no longer regenerate allowlisted files, retiring
    doppler's 8-file post-apply restore drill (verified: a bare apply
    now leaves the hand-maintained `.pyi` stubs untouched).

- **jm pin 0.28.2 → 0.28.3.** jm#428's `# jm:hand` member-level `.pyi`
    merge retires most of the hand-merge toil the 0.28.2 bump left
    behind: `dsss.pyi`'s ten hand-maintained members (despreader/
    burst_despreader `steps`/`bits` + their `*_max_out()`,
    `burst_demod.demod` + `demod_max_out()`) are now individually
    marked, and the file is off `status_allow` entirely — `jm apply`
    preserves the marked spans and refreshes everything else, so new
    generated API no longer silently rots behind the old whole-file
    skip. jm#440's additive fragment splice also landed: a
    manifest-derived method/property missing from a sacred fragment
    now gets spliced in without disturbing existing hand content (see
    Fixed, below).

### Fixed

- **`execute_max_out()`/`generate_max_out()`/`steps_u32*_max_out()` now
    actually exist on `DDC`, `HalfbandDecimator`, `HalfbandDecimatorQ15`,
    `AWGN`, and `NCO`.** Their `.pyi` stubs promised these `out=`
    sizing-helper methods, but the C binding was never generated (jm's
    delete-to-adopt fragment mechanic meant adding one meant losing
    unrelated hand content, so it kept getting deferred) — calling any
    of them raised `AttributeError`. Fixed by the jm 0.28.3 bump above:
    the additive splice fills in exactly the missing binding.

- **The DLL lock detector now runs in composition.** The
    `dll_accumulate`/`dll_update` inline composition helpers never fed
    the always-on code-lock detector (no offset noise tap, no looks), so
    a `Despreader`'s `code.lock` / `code.locked` / `noise_est` — getters
    *and* telemetry probes — were dead zeros. New composition faces
    `dll_lock_accumulate()` (per-sample offset tap, force-inline) and
    `dll_lock_look()` / `dll_lock_epoch()` (per-look/per-epoch, out of
    line) are shared by `dll_steps` (bit-exact refactor — its loops now
    call the same helpers) and the despreader, so the CFAR detector is
    genuinely always-on everywhere. **Throughput note:** the fix adds
    the fourth (noise) correlator tap to the despreader's per-sample
    loop — `Despreader.steps()` measures ~25% lower throughput
    (order-alternated interleaved bench, ~141 → ~106 MSa/s on the dev
    machine). This is parity, not regression: `Dll.steps` has carried
    the identical always-on cost since the detector shipped, and the
    tap cannot be decimated without breaking the CFAR calibration (the
    noise reference must integrate the same sample count as the
    prompt). `dll_steps` itself is unchanged within noise (~1%).

### Added

- **Verify-counted carrier lock on the Costas loop (blob v3;
    despreader v4).** `Costas` embeds a `lockdet` stepped on the
    |Re P|/|P| lock-metric EMA each dumped symbol: `locked` property,
    `configure_lock(up_thresh, down_thresh, n_up, n_down)`, and a
    `"<prefix>.locked"` telemetry probe beside `.lock` (four records per
    symbol). The default rule (0.85 / 0.78, 8 up / 32 down) derives from
    the metric's no-carrier statistics: under H0 the metric is
    |cos θ| — mean 2/π ≈ 0.637, EMA-smoothed std ≈ 0.071 — so the
    declare threshold sits ~3σ above the no-carrier mean.
    `Despreader` exposes both decisions (`carrier_locked`,
    `code_locked`) and forwards `"<prefix>.car.locked"` (eight probes
    per attach). `COSTAS_STATE_VERSION` 2 → 3,
    `DESPREADER_STATE_VERSION` 3 → 4.

- **Not adopted (by design): burst objects and acquisition.**
    `BurstDespreader` / `burst_demod` are burst-scoped — a verify-counted
    latch would spend most of a short burst warming up; the decision
    belongs to the caller (use `detection.LockDet` standalone).
    `Acquisition` emits one-shot per-dwell CFAR detections at a
    configured (pfa, pd); a verify-across-dwells confirm needs
    cell-association across dwells and is deferred to an acquisition-
    strategy design.

## [0.31.0] — 2026-07-10

### Added

- **`lockdet` — a portable lock detector, and `detection.LockDet`.** The
    decision rule every loop needs, factored out once as an embeddable
    C leaf (`lockdet_core.h`: pointer-free POD, force-inline step):
    separate declare/drop thresholds (level hysteresis) plus
    consecutive-look verify counters (time hysteresis) — `n_up` straight
    hits above `up_thresh` declare, `n_down` straight misses below
    `down_thresh` drop, and a metric inside the band is sticky both
    ways, so a statistic grazing a threshold cannot chatter the flag.
    Exposed as `detection.LockDet` (serializable; truth-table tested in
    both harnesses).

- **`detection.det_verify_count` / `detection.det_verify_delay`** —
    verify-count sizing: consecutive looks compound (`p^n`), so
    `det_verify_count(p_look, p_target)` returns the smallest run length
    meeting a compound budget (one function serves both sides: declare
    from the per-look pfa, drop from the per-look miss rate), and
    `det_verify_delay(p_look, n)` prices it — the mean looks to the
    first length-`n` run. Together with `det_threshold_*`, `det_pd_*`
    and `det_ema_alpha`, the full chain C/N0 → thresholds, verify
    counts, and smoothing bandwidth is now derived, not guessed.

- **`detection.det_ema_alpha(snr_in_db, snr_out_db)`** — probabilistic
    EMA sizing: treat the smoothed quantity as a DC level in noise with a
    per-sample estimator SNR (mean²/variance), request the output SNR the
    decision needs, and the coefficient follows from the EMA's variance
    reduction `(2−α)/α`. Given C/N0 (hence per-look SNR), this sizes any
    lock-metric smoother to a target decision SNR.

- **The lock decisions are telemetry probes.** `Dll.set_telemetry` now
    registers `"<prefix>.locked"` (the verify-counted lockdet decision,
    0/1) alongside the `.lock` CFAR statistic — four records per code
    epoch — and `MpskReceiver.set_telemetry` registers
    `"<prefix>.tracking"` (the two-way handover state) alongside the
    `.lock` carrier metric — nine probes per attach. `Despreader`
    forwards the DLL's new probe as `"<prefix>.code.locked"` (seven per
    period). Statistic and decision stream side by side, so a consumer
    sees exactly where the declare/drop rule fired without re-deriving
    thresholds. No new hot-path cost (the emits ride the existing
    out-of-line per-epoch/per-symbol flushes) and no blob change (the
    ids fill the attachment structs' padding).

### Changed

- **The DLL's code-lock latch is verify-counted (state blob v3).**
    `Dll.locked` now runs through an embedded `lockdet`: it flips up only
    after `det_verify_count(pfa, pfa·1e-3)` consecutive above-threshold
    N-look decisions (2 for the default pfa = 1e-3 — the false-declare
    rate compounds three decades under the per-decision pfa) and drops
    only after 2 consecutive below-threshold ones. `configure_lock`'s
    signature is unchanged (the counts derive from `pfa`); the C-only
    `dll_configure_lock_raw` grows the full lockdet geometry
    (`up/down_thresh`, `n_up/n_down`). `DLL_STATE_VERSION` 2 → 3 and
    `DESPREADER_STATE_VERSION` 2 → 3 (the embedded struct grew; old
    blobs are rejected, per the unreleased-format policy).

- **The M-PSK receiver's handover is two-way (state blob v4).** With
    `acq_to_track` enabled, a verify-counted `lockdet` steps on the
    carrier lock metric each recovered symbol: 8 consecutive
    above-`lock_thresh` symbols hand the carrier to the decision-directed
    discriminator (was: a single comparison, latched forever), and 32
    consecutive symbols below the 0.8× drop threshold now fall **back**
    to the NDA acquisition steer — the shared NCO carries the frequency
    estimate both ways, so a drop-back is a discriminator swap, not a
    cold restart. `MPSK_RECEIVER_STATE_VERSION` 3 → 4.

- **`Dll.configure_lock` is C-first and probabilistic.** The pfa→CFAR
    threshold policy moved from the hand-owned Python binding into
    `dll_configure_lock()` in the C core (the old raw form remains as
    `dll_configure_lock_raw()`), fixing a C/Python asymmetry — C
    composers can now configure by `(pfa, n_looks)` — and collapsing two
    defaults into one: the create-time default now computes the precise
    `det_threshold_noncoherent(1e-3, 20)` instead of a baked constant the
    Python constructor silently overrode. The noise-reference EMA
    bandwidth is sized via `det_ema_alpha` with a new `ref_snr_db`
    parameter (default 0 = auto, which reproduces the classic
    `1/α = max(1024, 32·N)` heuristic exactly — now as a consequence of
    holding the reference's std to an eighth of the statistic's intrinsic
    H0 spread, floored at ~33 dB). `track_ext_dll.c` is now 100%
    jm-generated — the last hand-owned binding logic in the repo's
    generated fragments is gone.

- **jm pin 0.28.0 → 0.28.1; the hand copy-out exceptions are retired.**
    jm gh-437 (just-makeit#438) makes the generated `variable_output`
    view default safe across same-size calls (a still-referenced buffer
    is retired, never reused in place), so `MpskReceiver.steps`/`bits`
    and `Dll.steps` regenerate to the declarative default — no
    hand-patched `PyArray_SimpleNew` + memcpy, and accumulate-chunks
    callers now get zero-copy views instead of copies. Verified:
    `test_block_size_invariance` plus explicit accumulated-views ==
    per-call-copies checks on both objects; the drain-immediately fast
    path still reuses the buffer in place.

## [0.30.0] — 2026-07-10

### Added

- **NATS telemetry egress (`tlm_sink` + `TLM16` wire frames)** — the
    `dp_tlm_sink_*` helper (`telemetry/tlm_sink.h`, in the optional
    `libdoppler_stream` component) drains a `dp_tlm_t` ring and publishes
    the records on a NATS subject as `TLM16` frames — a new
    `dp_sample_type_t` (appended, wire value 6) whose payload is packed
    16-byte `dp_tlm_rec_t` rows. `doppler.stream.Subscriber.recv()`
    decodes a TLM16 frame into the exact structured array
    `Telemetry.read()` returns, and `Publisher(ep, TLM16).send(recs)`
    publishes one from Python. The stream package also now re-exports the
    full sample-type constant set (`CI8`/`CI16`/`CF32` were missing).
    telemetry_core stays dependency-free; the pump runs on the ring's
    consumer thread and the path is lossy end-to-end by design. See
    docs/design/telemetry.md §Egress.

- **Costas + DLL + CarrierNda + Despreader telemetry instrumentation** —
    every tracking loop now speaks `set_telemetry(tlm, prefix, decim=1)`:
    Costas registers `"<prefix>.lock"` / `.e` / `.freq` (per dumped
    symbol), the DLL `"<prefix>.e"` / `.rate` / `.lock` (per code epoch,
    in both the coherent and partial-correlation loops), CarrierNda
    `"<prefix>.lock"` / `.e` / `.freq` **plus a forwarded attach to its
    embedded arm AGC** (`"<prefix>.agc.gain_db"`) — a sample-rate loop, so
    `decim` is the throttle — and the DSSS Despreader forwards to both of
    its loops (`"<prefix>.car.*"`, `"<prefix>.code.*"`, per code period).
    MpskReceiver's forward now also reaches its embedded carrier loop
    (`"<prefix>.car.*"` incl. the arm AGC — eight probes per attach).
    Detached cost stays benchmarked at parity via the hoisted-split /
    literal-parameter patterns (docs/design/telemetry.md §Instrumenting).
    Blob versions bump (costas v2, dll v2, carrier_nda v3, despreader v2,
    mpsk_receiver v3): telemetry attachments are zeroed in blobs —
    including the embedded-AGC attachment inside carrier_nda snapshots —
    and live attachments survive `set_state`.

- **SymbolSync + MpskReceiver telemetry instrumentation** — the timing
    loop registers `"<prefix>.e"` / `"<prefix>.freq"` / `"<prefix>.rate"`
    (TED error, NCO rate control, tracked samples/symbol), and the MPSK
    receiver adds its own `"<prefix>.lock"` plus a forwarded attach to the
    embedded timing loop (`"<prefix>.sync.*"`) — one record set per
    recovered symbol, `decim`-thinned, fully jm-declarative
    (`set_telemetry(tlm, prefix, decim=1)`). Serialization stays safe
    (`DP_DEFINE_POD_STATE_TLM`; symsync blob v3, mpsk_receiver v2 — the
    embedded child grew). **Detached cost is benchmarked at parity with
    the untouched baseline**: emission lives in out-of-line flush
    functions behind attachment checks hoisted to block-loop entry, so the
    per-sample hot loops carry no telemetry call sites at all (an extern
    call inside the loop forces the compiler to spill the register-cached
    loop state — measured ~20% slower even when never taken; the pattern
    is documented in docs/design/telemetry.md).

- **`track.SymbolSync` gains a second, selectable timing-error detector**:
    `ted="gardner"` (default, unchanged behavior) or `ted="dttl"` — a
    decision-directed sign-sign Data Transition Tracking Loop (M.K. Simon).
    DTTL reuses the same transition-gate/on-time samples Gardner already
    computes, so no new strobe machinery is added; it's valid for BPSK/QPSK
    only (not 8PSK/QAM). `MpskReceiver` is unaffected — it stays hardcoded
    to Gardner. See [Symbol Timing Recovery](docs/gallery/symsync.md).

## [0.29.0] — 2026-07-09

### Added

- **AGC telemetry instrumentation** — the first `dp_tlm`-instrumented
    object: `AGC.set_telemetry(tlm, "agc", decim=1)` (fully jm-declarative
    via the new capsule-param + status-return binding, jm gh-432) registers
    an `"<prefix>.gain_db"` probe recording the loop-filter integrator once
    per gain-update event in both `step()` and the decimated `steps()`
    paths. Detached cost is one predicted-not-taken branch per event;
    blobs stay deterministic (attachment zeroed) and a live attachment
    survives `set_state` (`DP_DEFINE_POD_STATE_TLM`; AGC state blob v3).
    The jm pin moves to **0.28.0**; the five hand-written `.pyi` symbols
    (`FFT.execute_ci16`/`execute_ci8` and the three hand-added `*_max_out`
    siblings) gain `manual_stub = true` manifest presence so the new
    gh-426 DROPPED gate passes.
- **`dp_tlm` telemetry taps** (`native/inc/telemetry/telemetry.h`) — a
    lightweight C99 primitive for observing scalar internals of running DSP
    objects (tracking-loop stress, AGC gain, lock metrics) as time series.
    Detached cost is one predicted-not-taken branch per *event*; attached
    cost is a per-probe decimation check plus one 16-byte record into a
    lock-free VM-mirrored SPSC ring (drop-on-overrun — the producer never
    blocks). Named probe registry, caller-stamped sample index, non-blocking
    `dp_tlm_read` drain, `-DDP_TLM_DISABLE` compile-out. The new
    `DP_DEFINE_POD_STATE_TLM` macro (`dp_state.h`) keeps instrumented objects
    serialization-safe: the attachment is zeroed in state blobs and preserved
    across restore. Design doc: `docs/design/telemetry.md`. The Python
    face is `doppler.telemetry.Telemetry` (hand-owned `no_generate`
    module): numpy structured-array `read()`, probe-name map, per-probe
    `emitted()` and `dropped` accounting, and the `_capsule` attach point
    instrumented objects bind to. API page: `docs/api/python-telemetry.md`.

### Fixed

- **The build tree is now a valid CMake package prefix** (#380).
    `doppler-config.cmake` was generated into `build/` but
    `doppler-targets.cmake` only materialised at install time — so pointing
    `doppler_DIR` (or `CMAKE_PREFIX_PATH`) at a raw build tree found the
    config and then failed at configure on the missing targets include. An
    `export(EXPORT doppler-targets ...)` now emits the same targets file
    into the build tree at generate time, so
    `find_package(doppler)` against an uninstalled `build/` works.

## [0.28.1] — 2026-07-09

### Added

- **Every release now publishes a container image**,
    `ghcr.io/doppler-dsp/doppler:X.Y.Z` (+ `:latest`), for `linux/amd64` and
    `linux/arm64` — `doppler`, `doppler-fir`, `doppler-source`,
    `doppler-specan`, and `wfmgen` are all on `PATH`, with the `cli` and
    `specan-web` extras pre-installed:
    ```sh
    docker pull ghcr.io/doppler-dsp/doppler:latest
    docker run --rm ghcr.io/doppler-dsp/doppler wfmgen --help
    ```
    `linux/amd64` installs the exact wheel already published to PyPI;
    `linux/arm64` (no manylinux wheel exists yet) builds from source at
    image-build time. See [Docker](docs/install/docker.md#published-container).

### Fixed

- **`doppler compose` with a `fir` chain stage was completely broken** —
    `FirBlock` referenced a `doppler-fir` console script that was never
    registered, so `doppler compose init tone fir specan && doppler compose up`
    (the literal example in the CLI docs and quickstart guide) failed with
    `FileNotFoundError: 'doppler-fir'`. Implements the missing script (a
    `Pull` → `FIR.execute()` → `Push` chain block, matching `doppler-source`'s
    structure) and registers it in `pyproject.toml`.

### Removed

- **Dead `fftw` system dependency dropped everywhere** — doppler's FFT has
    been fully vendored (pocketfft + PFFFT) for a while; every
    fftw/libfftw3/fftw-devel package declaration across `jb.toml` (the
    project's system-deps source of truth), both Dockerfiles, and both CI
    workflows was leftover dead weight. Also removed two straggler
    `zeromq-devel`/`zeromq` entries in the release workflow that the earlier
    ZMQ-removal work missed.

## [0.28.0] — 2026-07-08

### Changed

- **BREAKING:** naming-axis survey renames (`docs/design/api-taxonomy.md`) —
    no behavior changes, only identities:
    - `doppler.filter.HBDecimQ15` moves to `doppler.resample.HalfbandDecimatorQ15`
        (sits next to `HalfbandDecimator`, its CF32 sibling).
    - `doppler.dsss.Despreader` is renamed to `doppler.dsss.BurstDespreader`.
    - `doppler.track.Channel` is renamed **and moved** to
        `doppler.dsss.Despreader` (taking the name freed up above); its
        `nav_period` constructor kwarg is renamed to `periods_per_bit`.
    - `doppler.dsss.PolyPhaseEstimator` is renamed to
        `doppler.dsss.PolynomialPhaseEstimator`.
    - `MpskReceiver`'s `auto_handover` constructor kwarg is renamed to
        `acq_to_track`.
- **BREAKING:** `ZmqSink` (Python, `doppler.wfm`) and `wfm_zmq_sink_*` (C)
    are renamed to `StreamSink` / `wfm_stream_sink_*`. `wfmgen --output` no
    longer takes a `zmq://<endpoint>` double-prefix; pass the real endpoint
    directly, e.g. `wfmgen --output nats://127.0.0.1:4222/iq`.
- `examples/c/pipeline_demo.c` (and its Python counterparts) now require a
    running `nats-server` (was a brokerless ZMQ ipc/tcp pipeline).

### Removed

- **BREAKING: the ZMQ transport backend and vendored libzmq are gone.**
    `doppler.stream` / `stream.h`'s `dp_pub/sub/push/pull/req/rep_*` API is
    unaffected — NATS (`nats://`) is now the only backend, with full
    JetStream support and no `tcp://`/`ipc://`/`inproc://` fallback. This
    also removes doppler's only C++ dependency: the entire build, including
    the optional `libdoppler_stream` component, now needs only a C compiler
    on every platform (the CI C++-free gate now checks both).

## [0.27.0] — 2026-07-05

### Added

- **`out=` parameter + a `<method>_max_out()` sizing helper** on every
    block/streaming method whose default return is a zero-copy view into a
    buffer the object reuses on the next call: `BurstDemod.demod`,
    `Despreader.steps`/`bits`, `Specan.execute`, `DelayCf64.ptr`/`push_ptr`,
    `CarrierMpsk`/`CarrierNda`/`Costas`/`Channel`/`Symsync`'s `steps`/`bits`,
    `ImdMeas`/`NprMeas`/`ToneMeas.spectrum_dbfs`, `FFT`/`FFT2D`'s `execute*`
    family, `PSD.band_power`, `LO.steps_ctrl`, `PN.generate`,
    `AccTrace.value`, `RateConverter.execute`, `Resampler.execute`/
    `execute_ctrl`, and `Farrow.delay`. Pass `out=` with a buffer sized to
    `max(<method>_max_out(), len(x))` to get the result written directly
    into your own array instead of the default reused view — useful when
    you need to hold onto more than one call's result at a time without
    copying it yourself.
- `BurstDemod.payload_len` property, exposing the decoded payload length
    directly (previously only inferable from the returned array's size).

### Changed

- The default (no `out=`) return value of every method listed above is now
    consistently documented as a zero-copy view reused on the object's next
    call — this was already the real behavior for most of them but was
    previously undocumented, so a caller holding onto a result across two
    calls could be silently handed the same, now-overwritten, buffer.
    `Detector`/`Detector2D.last_corr` (a property, which can't take `out=`
    at all) gets the same documentation treatment.

### Fixed

- **Use-after-free in `RateConverter.execute()`.** Both the output-buffer
    growth path and the `rate` setter used to call `free()` on the buffer
    immediately, with no protection for a previously-returned numpy array
    still holding a view into it — growing the buffer or changing `rate`
    after calling `execute()` could silently corrupt or crash on an
    already-returned result. Both paths now defer the free to the object's
    own destruction, matching the pattern already used elsewhere in the
    codebase for this exact hazard.

## [0.26.1] — 2026-07-03

### Fixed

- **`type="symbols"` scenes now survive a JSON round-trip** (gh #331). The
    composer's JSON serializer (`wfm_json.c`) predated the `symbols` waveform
    (shipped 0.24.0) and never learned it: its `TYPE_NAMES` table had no
    `"symbols"` entry (so `type` index 7 fell out of range and mis-serialized as
    `"tone"`), and the complex constellation array was neither written nor
    parsed. Any path that round-trips a scene through JSON — `wfm.prepare()` /
    `Plan`, `wfmgen --from-file`, `--record` — therefore silently reverted a
    symbols source to a bare tone, corrupting the waveform (wrong PAPR/EVM/
    spectrum). `compose()`, which uses the in-memory scene directly, was
    unaffected, which is why it surfaced first through `Plan` (the first feature
    to round-trip through JSON). The serializer now emits `"symbols"` and a flat
    interleaved `[re, im, …]` constellation array, and the parser restores it —
    so `prepare(scene).render()` is once again bit-for-bit identical to
    `scene.compose()` for symbols scenes. The same stale table in `wfm_writer.c`
    (SigMF annotation labels) was an out-of-bounds read for symbols sources and
    is fixed too. Regression-tested in both harnesses (C `test_wfm_compose`
    symbols round-trip over the inline + `sum` serializer paths; Python
    `test_symbols_json_roundtrip` over single/multi-user, the exact #331 scene,
    and RRC pulse shaping).

## [0.26.0] — 2026-07-01

### Added

- **`wfm.Plan` — a "prepare once, materialize many" stimulus engine.** A
    composed multi-source scene is a linear form `Σ gainₖ·signalₖ + noise`, and
    the expensive DSP (spreading, RRC pulse shaping, the LO) lives entirely in
    the signal terms — invariant across a parameter sweep. `prepare(scene)`
    renders and caches each source once, then `Plan.render(…)` / `Plan.at(snr,   seed)` re-materialize any variation as a cheap re-weighted sum, **bit-for-bit
    identical to a full compose**. v1 axes: per-source `gains`/`phases`/`enable`,
    global `snr` (noise floor), and Monte-Carlo `seed`; `sweep()` / `monte_carlo()`
    generators drive detection/BER campaigns. The stimulus for evaluating a
    system (a detector, demod, or synchroniser) that re-runs one scene at many
    operating points. C-first (`native/src/wfm/wfm_plan.c`, over a shared
    `wfm_compose_build_synth` that guarantees the cache matches the composer);
    Python is a thin wrapper over the generated `kind="handle"` binding. See the
    [gallery walkthrough](https://doppler-dsp.github.io/doppler/gallery/plan/).

### Changed

- **just-makeit pinned to 0.25.0** (from 0.24.0) — adds the `kind="handle"`
    `type="string"` argument and `out_len_fn` array-out method shape that the
    `wfm_plan` binding is built on. Pure tooling; no codegen drift elsewhere.

## [0.25.0] — 2026-07-01

### Added

- **Fail-closed doc-snippet drift gate.** Every `python` fence in the docs
    is now executed (or `>>>` output-checked) in CI, discovered — not registered
    — so a new page is gated the moment it exists. This closes the hole that let
    the quickstart's `HalfbandDecimator()` example rot silently. Escapes are
    visible and reviewed (an inline `skip=REASON` marker, or a shrinking
    `docs/.doc-snippet-ignore` burn-down backlog). Contributor policy lives in
    `docs/dev/doc-examples.md`. The gate resolves `--8<--` snippet includes, so a
    fence can pull a region from a CI-tested `examples/*.py` and the shown code
    *is* the tested code (the gold standard for drift-proof examples). It has
    already surfaced and fixed a raft of real drift the docs had accumulated —
    the wfmgen **Scenes**/composition guides' non-existent
    `Composer(fs=…).add(tone(…))` fluent API and `gap`/`headroom=`/`.write`
    surfaces (→ the real `Segment`/`Timeline`/`Composer` form);
    `Despreader.set_acq(reps=…)` → `acq_reps`; `AccF32.push`/`add` → `step`/`steps`;
    a non-existent `F32Buffer.available`; `FFT.execute` → `execute_cf32`;
    `LO.step()` (block-only); `ToneMeasure`/`IMDMeasure` phantom `window=`/`beta=`
    kwargs; `Resampler(rejection=…, passband=…)` (the Python ctor takes only
    `rate`); `HalfbandDecimator()` now requiring caller-supplied taps `h`;
    `Detector.execute` → `push`; and a stale `pd_predicted >= pd` assertion. **The
    whole `docs/` tree is now gated — the burn-down backlog is empty.** The
    `Farrow.delay` keyword/`.pyi` mismatch it surfaced is now fixed (just-makeit
    #412 — adopted via the 0.24.0 pin bump; see **Fixed**).
- **"Bring Your Own Constellation" gallery page + `symbols_demo.py`.** A worked
    showcase of `wfm` `type="symbols"`: pi/4-QPSK and 16-QAM built from arbitrary
    complex streams (modulations no enum provides), rect vs RRC pulses, and the
    envelope floor that gives pi/4-QPSK its ~0.5 dB lower PAPR. The `wfmgen`
    guide's `--type` reference now documents `symbols` and `--symbols-file`.
- **API reference completeness.** Documented the remaining undocumented public
    symbols — `filter.HBDecimQ15`, `resample.kaiser_beta`/`kaiser_num_taps`, and
    the `wfm` `bpsk_map`/`qpsk_map`/`wfm_awgn_amplitude`/`wfm_ebno_to_snr_db`
    helpers — and added `type="symbols"` to the `Synth` API page (correcting the
    stale "seven-type" wording). The `scripts/check_api_docs.py` coverage baseline
    (`docs/api/.api-coverage-ignore`) is paid down from 37 lines to 1: every
    public `__all__` symbol is now documented, and the CI gate keeps it that way.

### Changed

- **wfmgen guide restructured** into a concepts-first multi-page section
    (`docs/guide/wfmgen/`). The former 736-line single page becomes nine focused
    pages — Overview, **Concepts** (the Synth / Segment / Timeline / Composer
    object model, stated plainly for the first time), Waveforms (now including
    `symbols`), Levels & SNR, Output & containers, Scenes, Streaming, Python API,
    and Recipes. Inbound links repointed; no content dropped.
- **Examples & Gallery merged into one domain-grouped section.** The flat
    37-entry Gallery and the separate Examples section become a single
    **Examples** section grouped by DSP domain (Sources & Waveforms, Filters &
    Resampling, DDC, Detection & Acquisition, Synchronization Loops,
    Constellations & Receivers, Measurement, Quantization & Fixed-Point, Gain
    Control, Streaming, Fundamentals). Six duplicate `examples/python-*.md`
    walkthroughs (agc, awgn, corr, detection, detection2d, rate-converter) were
    folded into their gallery counterparts, which carry the figure. Off-nav
    orphans adopted or archived: `adc`/`hbdecim_q15` gallery pages and the
    `RESAMPLER`/`SPECAN`/`acq-fn`/`STATIC_VS_DYNAMIC`/NATS-transport design docs
    are now in the nav; two superseded design notes moved to `design/archive/`.
    `make gallery` now regenerates the AWGN and waveform-I/O figures too.

### Fixed

- **`Farrow.delay(x, mu=…)` keyword call (just-makeit #412).** Adopting
    just-makeit **0.24.0** (pin bumped from 0.23.0; `jm apply` clean, no codegen
    drift) makes a `variable_output` method with named params generate a
    keyword-capable binding that matches its already-keyword-shaped `.pyi`.
    `f.delay(x, mu=0.3)` now works — it previously raised `TypeError` although
    the stub advertised `mu`; positional calls are unchanged. The gallery and
    API-reference Farrow examples use the keyword form. (The other positional-only
    param methods — `LO.steps`, `Costas.steps`, `RateConverter.execute` — adopt
    the same way when next regenerated.)

## [0.24.0] — 2026-06-30

### Added

- **`wfm` `type="symbols"` — arbitrary complex-symbol streams.** Feed the synth
    a complex64 constellation directly instead of picking a fixed modulation:
    each symbol *is* the output point (no bit→symbol map), oversampled by `sps`,
    cycled, and RRC-shaped through the matched FIR. Generalises every modulation
    into "compute the symbols, pass them in" — pi/4-QPSK is the QPSK points with
    every other rotated by pi/4; QAM/APSK/custom likewise. Available across all
    three faces, byte-identical: `_SynthEngine(type="symbols").set_symbols(iq)`,
    the composer `Synth(type="symbols", symbols=iq)` (jm 0.23.0 `complex` field),
    and the CLI `wfmgen --type symbols --symbols-file iq.cf32`.
- **Synth/Segment field docstrings** — every `wfm` composer source/segment field
    now carries a description in the generated `.pyi`, and ranged numeric
    defaults render bare (`freq`/`level`/`f_end` → `default 0.0`, not `"0.0"`).
    `level`'s doc notes it only applies when summed in a Segment/Composer.
    (jm 0.22.0 field-`doc` key.)

### Fixed

- **`clib_common.h` include guard namespaced** (`CLIB_COMMON_H` →
    `DOPPLER_CLIB_COMMON_H`) so a jm-scaffolded C consumer's own `clib_common.h`
    can no longer shadow doppler's `DP_OK`/`DP_ERR_INVALID` defines.

## [0.23.1] — 2026-06-30

### Fixed

- **`wfm` bits input now honours RRC pulse shaping.** `--type bits --pulse rrc`
    (and the `bits(..., pulse="rrc")` API) silently emitted rectangular pulses:
    the RRC FIR was gated off for the `bits` waveform at four layers
    (`wfm_synth_set_rrc`, the standalone bridge, and both the per-sample and
    block generation paths). The bit stream is now shaped by the same matched
    FIR as `pn`/`bpsk`/`qpsk` — verified byte-identical to a symbol-rate impulse
    train convolved with the sqrt(sps)-scaled taps, chunk-invariant across
    `step()`/`steps()`.

## [0.23.0] — 2026-06-29

### Added

- **`dsss.BurstDemod`** — feedforward BPSK DSSS burst/frame demodulator. Takes a
    coarse `(Doppler, chirp-rate)` prior from acquisition, refines it with a
    feedforward 2-D estimate over the preamble partials, sample-rate dechirps,
    despreads, frame-syncs on a sync word, and CRC-checks the payload. Handles
    near-static Doppler **and** high-rate (LEO) chirped bursts.
- **`dsss.PolyPhaseEstimator`** — coherent 2-D (frequency × chirp-rate)
    estimator (2-lag HAF) with a `max_rate` knob: `0` collapses to a single
    zero-padded FFT (Doppler only), non-zero adds the rate axis. The transform is
    4× zero-padded for a finer frequency grid plus parabolic peak interpolation.
- **Ranged numeric fields in the `wfm` composer** — `freq`, `f_end`, `snr`,
    `level`, `num_samples`, and `off_samples` each accept either a scalar **or** a
    `[lo, hi]` pair (`Synth(freq=(lo, hi))` / `Segment(...)` / JSON `[lo, hi]` /
    CLI `--freq lo:hi`) drawn **uniformly per segment repeat**. The draw is a
    stateless splitmix64 hash of `(seed, repeat, segment, source, field)`, so
    `--record` stores the *range* and `--from-file` replays byte-for-byte —
    powering per-burst Doppler and code-phase variation in a looping scene.
- **`wfm` `seed_advance` (`none` / `noise` / `all`)** — per-repeat seed policy
    for looped / `--continuous` streams (CLI `--seed-advance`, JSON
    `seed_advance`). `none` (default) repeats byte-identically; `noise` re-rolls
    only the AWGN seed (signal bit-identical — BER/detection curves over one
    fixed waveform); `all` advances the whole seed (code, data, and noise). Pass
    0 is always the unmodified seed, so a finite single-pass run stays
    byte-reproducible.
- **Realtime DSSS demod example**
    (`doppler.examples.dsss_realtime_file_demod`) — tails a growing
    `wfmgen --continuous` capture and decodes each burst as it lands (DDC →
    Acquisition → BurstDemod), each with a fresh Doppler offset, code phase, and
    noise realization.

## [0.22.0] — 2026-06-25

### Added

- **`wfmgen --version` / `-V`** prints `wfmgen (doppler) <version>`.
- **Clearer NATS streaming errors** — a `Push` / `Requester` / `Replier` frame
    larger than the broker `max_payload` now raises a `ValueError` that names the
    limit (backed by a new `DP_ERR_TOO_LARGE` code in the C ABI) instead of an
    opaque "send failed". The durable PUSH/PULL work-queue does not chunk frames
    (unlike PUB/SUB, whose chunks would scatter across load-balanced workers), so
    a work-queue frame must fit one message. Raise the broker `max_payload` to
    stream larger durable frames — which is also faster (bigger frames amortize
    the per-publish fsync); see `deploy/nats/values.yaml`.

### Changed (breaking)

- **`wfmgen` flags are now hyphenated**: `--sample_type` → `--sample-type`,
    `--file_type` → `--file-type`, `--snr_mode` → `--snr-mode`,
    `--pn_length` → `--pn-length`, `--pn_poly` → `--pn-poly`,
    `--f_end` → `--f-end`. The underscore spellings are removed (no aliases).
    The `--from-file`/`--record` JSON keys are unchanged (still `sample_type`,
    etc.), as are the Python keyword arguments.
- **`wfmgen` friendly defaults**: `--fs` defaults to `1.0` (so `--freq`/`--f-end`
    are **normalised**, cycles/sample, unless `--fs` is given), `--sps` to `1`,
    `--seed` to `0`, `--pn-length` to `15`. The Python `Synth`/`Composer`
    defaults move in lockstep, so `wfmgen` and `Synth()` stay byte-identical.
    Pass explicit flags to restore the old behavior (e.g. `--fs 1e6 --sps 8`).

### Fixed

- **`wfmgen` no longer segfaults** when a value-taking flag is given without a
    value (e.g. `wfmgen --freq`); it now reports a usage error and exits 2.
- `wfmgen --rrc-beta` is validated against its documented `(0, 1]` range; an
    unknown flag prints one terse line instead of the whole usage; over-long
    `--output` paths are rejected instead of silently truncated.
- `wfmgen --help` now states the real defaults (the old text mis-documented
    `--fs`, `--sps`, `--seed`, `--pn-length`, and `--headroom`).

## [0.21.0] — 2026-06-23

### Changed (breaking)

- **Measurement suite is now auto-windowed** — `ToneMeasure` / `IMDMeasure` /
    `NPRMeasure` no longer take `window` / `beta` / `pad`. State the **dynamic
    range** you need (directly via `dynamic_range_db`, or implied by the ADC
    `bits`) and the analyser auto-selects the Kaiser window so its sidelobes sit
    below that range — operators think in resolution bandwidth and dynamic
    range, not Kaiser shape. The realised RBW is reported in each result.
    Callers passing `window=`/`beta=`/`pad=` must drop them (use
    `bits=`/`dynamic_range_db=` instead). `measure_min_samples` is likewise
    dynamic-range driven and defaults `target_rbw` to span/1000.

### Fixed

- **SFDR no longer capped by window leakage** — the worst-spur search excluded
    only the main-lobe null-to-null half-width around the fundamental, so the
    first eligible bin sat on the fundamental's own first sidelobe and SFDR read
    the *window's* leakage rather than the DUT's. A wider, window-aware
    `spur_guard_bins` keep-out (still integrating power over the main lobe) fixes
    it across ToneMeasure/IMDMeasure/NPRMeasure. The auto window uses the Kaiser
    *window*-sidelobe design formula (not the FIR-filter one), so a B-bit ADC's
    true SFDR is no longer leakage-limited.

### Added

- **Python API reference pages** for `arith`, `cvt`, `agc`, and `util` — the
    four C-extension modules that previously had no docs page — plus a
    rebuilt per-pattern streaming page, and a `spectral.kaiser_beta_for_sidelobe`
    window-design helper.
- **Runnable, CI-gated docstring examples** across the suite: every public
    method/free function with an example now has it exercised by the
    `--doctest-glob='*.pyi'` gate (extended to `docs/api/*.md` for the curated
    free-function pages). Corrected several wrong documented values along the way
    (notably the detection `Pd`/Marcum-Q numbers and the amplitude-vs-power SNR
    framing).

### Tooling

- **just-makeit pin → 0.19.32** — brings the gh-384/gh-385 fixes upstream
    (this project drove both): module free-function and inline-function header
    `@code` now synthesize into the generated `.pyi` docstrings, and a
    `variable_output` block method renders an `NDArray` input. Re-applying
    enriches the `arith`/`detection`/`measure`/`resample`/`spectral`/`wfm` stubs
    and gives `CIC.decimate` its real docstring + correct `x: NDArray` signature.

## [0.20.0] — 2026-06-22

### Added

- **Canonical wfmgen JSON schema** (`docs/schema/wfmgen.schema.json`,
    JSON Schema 2020-12) — covers both segment forms (inline
    single-source and multi-source `sum`), all source fields (`f_end`,
    `lfsr`, `level`, `modulation`/`pattern`, `pulse`/`rrc_*`,
    `headroom`), with `additionalProperties: false` so unknown keys
    are caught. Replaces the partial sketch in the C header comment.
- **Schema validation test suite** (`test_schema.py`, 36 tests) — 18
    live `wfmgen --record` invocations covering every waveform type,
    SNR mode, pulse shape, and LFSR convention; plus `json-template`
    and `--from-file` round-trip; plus 8 static valid and 8 static
    invalid cases. Caught a real gap: `sps`/`pn_length` are
    zero-initialised in the noise source injected by
    `wfm_resolve_noise`. Adds `jsonschema>=4.18` to dev deps.
- **`wfm_json_demo.py` example + gallery page** — end-to-end
    demonstration of `Composer.to_json()` → `Composer.from_json()`
    byte-identical round-trip, with spectrogram and inline JSON panel.

### Changed

- **wfmgen JSON `version` field is now the integer `1`** (was the
    string `"wfmgen-1"`). The parser ignores the field so existing
    `--from-file` specs continue to work; new `--record` output and
    `to_json()` emit the integer form.

## [0.19.1] — 2026-06-21

### Added

- **`wfmgen --help` rewrite** — grouped sections (WAVEFORM TYPE, SIGNAL
    PARAMETERS, NOISE/SNR, PULSE SHAPING, BITS INPUT, PN SEQUENCE,
    AMPLITUDE & CLIPPING, OUTPUT, COMPOSITION, REAL-TIME), per-flag
    descriptions with defaults, pipe-separated choices (`auto | fs | ebno | esno`), and copy-paste EXAMPLES.
- **Build internals doc** (`docs/dev/build-internals.md`) — CMake layer,
    just-buildit PEP 517 hook, manylinux/auditwheel pipeline, CI and release
    pipeline job tables, troubleshooting guide, local wheel replication steps.
- **CLI vs Python API side-by-side** in the wfmgen guide multi-segment
    section — tabbed `--from-file` JSON spec and `Composer` equivalent showing
    byte-identical output from the same C engine.
- **Python examples shipped in the wheel** (`src/doppler/examples/`) — all
    37 example scripts now install with the package.

## [0.19.0] — 2026-06-21

### Added

- **Two new Python examples:** `dsss_burst_demo.py` (DSSS burst generation with
    PN code spreading) and `wfm_write_demo.py` (waveform I/O round-trip with
    SigMF-style metadata), both bundled under `examples/python/`.
- **Docs section overview pages** for all left-nav sections (Install, Examples,
    Guides, Design, Contributing, API Reference), making section headers
    clickable in the material theme via `navigation.indexes`.
- **Waveform Write gallery page** (`docs/gallery/wfm-write.md`).

### Changed

- **jm pin → 0.19.30** — resolves incomplete docstrings on `Synth`, `Segment`,
    `Composer` (jm#375), wfm handle generator objects (`wfm_reader`,
    `wfm_writer`, `wfm_sink`, `sample_clock`) (jm#374), and `size_t`
    init-param defaults (jm#377). All `.pyi` stubs regenerated.
- **CI: release.yml `verify-ci` gate** now polls the tagged SHA for the
    `CI passed` aggregator check instead of re-running the full suite on tag
    push, cutting release cycle time.
- **Docs build:** `make docs` no longer depends on `gen-c-api`; use
    `--clean` instead of `--strict`. C API docs remain pre-generated in
    `docs/c-api/`.
- **Docs build warnings → 0:** fixed bracket notation in 32 `native/inc/`
    Doxygen headers that zensical was parsing as link references.
- **README:** added Navigate / API Reference quick-link block; fixed dead
    Rust badge link.

## [0.18.0] — 2026-06-21

### Added

- **Waveform composer is now a first-class generated surface.** The transport
    and composition types — `Synth`, `Segment`, `Timeline`, `Composer`,
    `Writer`, `Reader`, `ZmqSink`, `SampleClock` — are generated into the C
    extension and re-exported verbatim from `doppler.wfm` (no Python wrapper
    layer). New serializers: `Composer.to_json`/`from_json`/`from_file`,
    `Composer.to_sigmf` (SigMF 1.0 sidecar with one annotation per source), and
    the `Synth(bits=…)` arbitrary-bit-pattern waveform.
- **`spectral`: `blackman_harris_window`** plus an extended PSD window enum.
- **Exhaustive `wfm`/`wfmgen` validation.** Analytic DSP-correctness and full
    API-surface test suites (`src/doppler/wfm/tests/`), three new examples
    (`wfm_receiver_ber`, `wfm_rrc_response`, `wfm_realtime_stream`),
    documentation gap-closure (SigMF sidecar schema, a worked SNR/level
    walkthrough, RRC, real-time streaming, and BLUE-detached sections), and a
    Python 3.9 end-to-end container (`deploy/validation/`) that validates the
    published wheel with no build toolchain.

### Fixed

- **`doppler.stream` decodes all six `dp_sample_type_t` wire types.** The
    receivers (`Subscriber`/`Pull`/`Requester`/`Replier`) previously handled
    only `CI32`/`CF64`/`CF128`; `cf32` (the default `ZmqSink` type), `ci16`, and
    `ci8` frames now round-trip to a Python subscriber. (#193)
- **`PN()` auto-selects a maximal-length polynomial** when `poly` is omitted or
    `0`, matching `Synth(pn_poly=0)` — previously it ran with no feedback and
    emitted a degenerate sequence. (#191)
- **`wfmgen --output -` writes to stdout** instead of creating a file literally
    named `-`. (#192)

### Changed

- **Performance:** `fft2d` uses per-row/column scratch instead of a whole-array
    CF32 promote.
- **Toolchain:** standardized on zensical for docs, aligned Makefile targets and
    mcp-store configs, expanded ruff to the full ruleset, split docs deps into
    their own group (wheel builds skip the docs toolchain), and added
    `make setup` with lint-hook auto-install. Vendored FFT backends moved to
    `vendor/`.

## [0.17.0] — 2026-06-15

### Added

- **`doppler.analyzer.Specan` — a natural-parameter spectrum analyzer.** Drive a
    streaming spectrum display with the instrument knobs an operator already
    knows — **center, span, RBW, reference level** — instead of window length,
    Kaiser beta and zero-pad. It composes the existing `DDC` (tune + decimate)
    and the averaging-PSD core in C, so the natural-parameter → DSP mapping lives
    in C exactly once. `doppler.specan`'s engine is re-based onto it (the
    pure-Python DDC→window→FFT→dB chain is gone, so the app can no longer drift
    from the C ABI).
- **`bits` dBFS scale option** on `spectral.PSD`, the three measurement analyzers
    and `Specan`: `bits>0` sets the 0-dBFS reference to `2**(bits-1)`, defined
    once in the PSD core. `bits=B` is identical to `full_scale=2**(B-1)` — one
    source of truth for dBFS, no more hand-computing the ADC full scale.
- **`spectrum_dbfs()` on `IMDMeasure` and `NPRMeasure`** (mirrors `ToneMeasure`):
    the same averaged-PSD dBFS trace the metrics use, for a display backdrop.

### Changed

- **Renamed `spectral.Welch` → `spectral.PSD`** (C `welch_*` → `psd_*`,
    `native/{inc,src}/welch` → `…/psd`). The shared averaging-PSD core's public
    name; no behaviour change — every metric, spectrum and test is identical.
- **`Specan`'s additive dB offset is `offset_db`** (applied on top of the dBFS
    reference, e.g. a dBm calibration); the dBFS reference itself comes from the
    PSD core's `bits`/`full_scale`.
- The `doppler.measure` result structseqs now report
    `__module__ == "doppler.measure"` (was the C component name, e.g.
    `"tonemeas"`), so `repr(type(r))` reads
    `<class 'doppler.measure.ToneMetrics'>` — the import path, not the internal
    component. Field access / unpacking are unchanged (jm `record_module`,
    gh-261).
- The `measure_demo` / `measure_imd_npr` gallery demos are now doppler-native:
    tones via `source.LO`, noise via `source.AWGN`, transforms via
    `spectral.FFT`, the spectrum backdrop via the analyzers' `spectrum_dbfs`,
    ADC dBFS via `bits` — no hand-rolled periodogram, FFT or RNG.

## [0.16.2] — 2026-06-14

### Changed

- **`measure` drops its hand-written structseq fragments for declarative
    `single = true` (jm gh-244/gh-259, pin → 0.19.9).** `ToneMeasure`/`NPRMeasure`/
    `IMDMeasure`'s `analyze`/`analyze_complex`/`time_stats` are now generated from
    the manifest: `single = true` emits the by-value `PyStructSequence` binding,
    `record_name` preserves the public type names (`ToneMetrics`/`NPRMetrics`/
    `IMDMetrics`/`TimeStats`), `NPRMeasure.analyze`'s geometry params are declared
    (with `guard_hz = 0.0` now an optional keyword), and `nogil = true` releases
    the GIL across the kernel (jm gh-261, 0.19.9). The metric kernels in
    `*_core.c` were reconciled to **return the record by value**, and
    `measure.pyi` is now jm-generated (dropped from `status_allow`). The public
    API is unchanged (`r.enob`, tuple-unpacking, the same fields) **except** the
    result types' `__module__` is now the C component (`tonemeas`/…) rather than
    `doppler.measure` (cosmetic; `repr` only).

### Docs

- **Measurement-suite docs round out.** A new IMD/NPR gallery example
    (`measure_imd_npr_demo.py`) — two-tone IMD/TOI and notched-noise NPR with the
    measured curve against the ideal-quantiser model (ADI MT-005). The
    measurement-suite design guide now **renders its LaTeX** (added MathJax via
    `pymdownx.arithmatex`).

## [0.16.1] — 2026-06-14

### Changed

- **The 11 `cvt` converters and `agc` drop their hand-written `steps(x, out)`
    bindings for jm-generated ones (jm gh-222/gh-240).** Each `_ext_<obj>.c`
    fragment hand-rolled the dual-path block method (allocate-or-fill-`out=`);
    jm now generates that natively, so the fragments were deleted and
    regenerated. The regenerated `steps()` is a strict improvement — `out=` is
    now a **keyword** (`obj.steps(x, out=buf)`), where the hand version only
    accepted it positionally. Signatures are otherwise unchanged. The four
    `F32To*` converters' sticky `clipped` flag, previously a hand-patched getset,
    is now a declared `[[<obj>.properties]]` (with its docstring preserved via the
    `doc =` key). The accumulator objects (`acc_f32`/`acc_cf64`/`acc_trace`) are
    **not** included — they expose bespoke methods (`madd`/`add2d`/`accumulate`/…)
    that aren't a generated block-`steps` shape.

- **All composing modules link cross-module cores declaratively (jm gh-225).**
    Every hand-maintained module `extra_link_libs` core list is gone (only the
    non-component libm `m` remains, on `ddc` and `resample`); each composing
    object owns its link line via `depends_on = [{ name = "…", link = true }]`:
    `welch` → `acc_trace`; `tonemeas` → `fft`/`spectral`; `wfm_synth` →
    `lo`/`awgn`/`fir`; `ddc` →
    `lo`/`RateConverter`/`resamp`/`hbdecim`/`hbdecim_r2c`/`cic`/`fir`/`resample`;
    `resample`'s `RateConverter` → `resamp`/`fir` and `HalfbandDecimator` →
    `hbdecim`/`hbdecim_r2c` (the latter for the `HalfbandDecimatorR2C` extra
    type). `jm status --check` now covers the link and no hand-edited
    `target_link_libraries` remains. Generated link lines are byte-identical
    except `resample`, which additionally **drops a redundant duplicate
    `resample_core`** (the module's own core is auto-linked). `ddc` is a
    *collocated* module-object (module name == object name) whose `.so` and C
    test/bench share one regenerated CMakeLists; that relies on `link=true` being
    **additive** there (jm gh-254, shipped in the 0.19.7 pin bump below) so the
    composed cores stay on `test_ddc_core`/`bench`.

- **jm pin 0.19.6 → 0.19.7** (`just-makeit.toml` + `ci.yml` +
    `perf-regression.yml`). Picks up gh-254 (additive collocated `link=true`,
    above); no codegen drift (`jm apply` reconciled nothing but the hand-owned
    `measure.pyi`).

- **Module-level functions are now keyword-capable** (via the jm 0.19.6 pin bump).
    Free functions such as `doppler.spectral.kaiser_window(w=…, beta=…)` and
    `doppler.measure.measure_min_samples(fs=…, target_rbw=…, …)` accept keyword
    arguments; the per-sample `step()`/`steps()` hot path stays positional.

### Fixed

- **Latent use-after-free in `FIR.execute`.** Its grow-on-demand output buffer
    was returned as a NumPy *view* (`SimpleNewFromData`), which could dangle after
    a later, larger `execute()` reallocated the buffer (the gh-219 class of bug
    that `DDC`/`DDCR`/`HalfbandDecimator` were already hardened against). `execute`
    now returns an independent NumPy-owned array per call.

## [0.16.0] — 2026-06-14

### Added

- **`doppler.measure` — single-tone ADC / spectral measurement suite.** A new
    module of IEEE Std 1241 windowed-tone analysers that own their window +
    zero-padded FFT and turn a time-domain capture into figures of merit, with
    each component's power integrated over its window **main lobe** (so a
    full-scale tone reads ~0 dBFS regardless of sub-bin placement):

    - **`ToneMeasure`** — SNR, SINAD, THD, THD+N, SFDR (dBc + dBFS), ENOB
        (+ full-scale-corrected), noise floor and worst spur from one
        `analyze()` (real or complex), plus `time_stats()` and the accuracy /
        resolution metadata (RBW vs bin spacing, processing gain, uncertainty).
    - **`NPRMeasure`** — notched-noise Noise Power Ratio.
    - **`IMDMeasure`** — two-tone IMD2 / IMD3 and second/third-order intercepts.
    - Capture-planning helpers: `measure_min_samples`, `measure_rec_nfft`,
        `measure_proc_gain`, and `dp_coherent_freq` (nearest leakage-free
        coherent frequency). Results are named tuples (`r.enob`, `r.sfdr_dbc`).
        See the [Measurement Suite](design/measurement-suite.md) design guide.

- **`wfmgen json-template [FILE]`** — a subcommand that dumps a ready-to-edit
    example spec in the canonical `--from-file` (`wfmgen-1`) schema, to a file or
    stdout. The template (an inline tone, an RRC-shaped QPSK-from-bits burst with
    a trailing gap, and a two-source `sum` mix) is generated through the same
    serialiser as `--record`, so it is valid by construction and round-trips
    through `--from-file` unchanged — a working starting point, not just docs.

## [0.15.1] — 2026-06-13

### Fixed

- **`wfmgen` no longer prints binary garbage to a terminal.** With no `--output`
    it defaults to raw IQ on stdout; on an interactive terminal that dumped binary
    bytes. It now refuses (with a usage message) when stdout is a tty and the
    format is binary — piping/redirecting (`wfmgen … > out.raw`, `wfmgen … | …`)
    and the text `--file_type csv` are unaffected.
- **Use-after-free in `DDC`/`DDCR`/`HalfbandDecimator` (q15) `execute()`** — the
    grow-on-demand output buffer was `realloc`'d in place, so a previously returned
    array (which pins `self`, not the buffer) could alias freed memory after a
    later, larger `execute()` grew it. Each call now returns an independent
    numpy-owned array, matching the source objects (`lo`/`nco`/`awgn`) and the
    upstream just-makeit fix (gh-219). (Also plugs an input-array refcount leak on
    the allocation-failure path.)

### Changed

- **just-makeit pin → 0.19.3.** Picks up gh-197: the generated `kaiser_window`/
    `hann_window` bindings now take a writable output buffer
    (`NPY_ARRAY_WRITEABLE`) instead of `const float *`.

## [0.15.0] — 2026-06-13

### Added

- **Integer-IQ FFT methods `FFT.execute_ci16` / `execute_ci8`** (C:
    `fft_execute_ci16`/`ci8`). Transform interleaved **int16/int8** I/Q directly to
    CF32, folding the int→float scale (v/32768, v/128 — the `cvt` full-scale ±1.0
    convention) into the FFT's input read. So an SDR/ADC integer stream FFTs in one
    fused pass on the native-float PFFFT backend — **bit-identical** to
    `i16_to_f32` then `execute_cf32`, at the same speed (the convert is free), and
    ~10× a scalar int16 FFT.
- **Native single-precision FFT via vendored PFFFT (Pommier/FFTPACK, BSD).** cf32
    transforms on PFFFT-friendly sizes (multiple of 16, 5-smooth — all powers of
    two) now run on a SIMD float kernel (SSE/NEON, scalar fallback) instead of
    promoting to double — **~2.2–3.1× faster** for 1-D cf32 across 1024–65536, and
    the 2-D cf32 path too, at float accuracy (~1e-7 vs the double result). Other
    sizes (e.g. odd 2× Gold-code lengths) transparently keep the promote-to-double
    pocketfft path. cf64 is unchanged. Closes most of
    [#139](https://github.com/doppler-dsp/doppler/issues/139). The core stays
    C++-free and `-lm`-only (PFFFT is pure C, 128-bit SIMD only).

### Changed

- **2-D FFT: recover the cf64 performance regressed by the C99 port.** For
    power-of-two column strides (the common FFT sizes, the worst case for the
    strided column pass) the 2-D transform now runs both passes contiguously via a
    cache-blocked transpose instead of a per-column gather/scatter — ~+20%
    throughput at 64×64, ~1.5–2× at 256²–2048². Non-power-of-two strides keep the
    gather path (where the double transpose wouldn't pay). cf64 numerics unchanged;
    the cf32 2-D path still pays the promote-to-double cost (a native single-
    precision FFT, tracked in [#139](https://github.com/doppler-dsp/doppler/issues/139),
    is the remaining lever).

## [0.14.1] — 2026-06-13

### Fixed

- **macOS: a downstream can statically link `libdoppler.a` with just `-lm`
    again.** 0.14.0's weak-`import` seam linked the core dylib but not a
    consumer that statically links the core archive into its own executable
    (ld64 rejected the undefined `wfm_zmq_sink_*` references). The core now ships
    pure-C weak **stub definitions** for those symbols, so the archive is
    self-contained and links on every platform with no special flags;
    `libdoppler_stream` still provides the strong overrides. The downstream
    static-link is now smoke-tested in CI (incl. macOS) to catch this pre-release.
    Python wheels were unaffected.

## [0.14.0] — 2026-06-13

### Changed

- **The core `libdoppler` is now C++-free — it links only `-lm`.** pocketfft was
    ported from the vendored header-only **C++** implementation to the upstream
    **pure-C99** pocketfft (libm-only) behind the unchanged C wrapper API; the cf32
    path promotes to double internally (cf64 numerics unchanged). The C++
    ZMQ/stream layer was split out of the core (see Added). A downstream can now
    link `libdoppler.a -lm` with **no libstdc++** at link or runtime — previously
    the archive dragged in libstdc++/CXXABI symbols stamped at doppler's build
    toolchain version. A CI gate enforces that the core carries no
    libstdc++/CXXABI symbols.
    - *Performance note:* the **2-D** FFT is slower with the 1-D-only C core
        (cf64 ≈ +47%, cf32 ≈ +157% via the double-promote); 1-D FFT is unchanged.
        Tracked in [#139](https://github.com/doppler-dsp/doppler/issues/139).

### Added

- **`libdoppler_stream` — an optional ZMQ/stream component** (`doppler::stream` /
    `doppler::stream-static`). It carries the `dp_pub_*`/`dp_sub_*` wire layer and
    the wfm ZMQ sink and embeds the vendored C++ libzmq statically (no runtime
    `libzmq.so`). `wfmgen` stays in the core via a weak `wfm_zmq_sink_*` seam: its
    `--output zmq://` path works when `libdoppler_stream` is linked and reports a
    clear "requires the stream component" error otherwise.

## [0.13.2] — 2026-06-12

### Changed

- **`doppler::doppler-static` is now a first-class CMake export target.** The
    static library joins `install(EXPORT)` (its folded-in objects are baked into
    the archive, not export-time dependencies), so `find_package(doppler)`
    generates a fully **relocatable** imported target for it — the previous
    hand-rolled path computation in `doppler-config.cmake.in` is gone, so it
    survives any `lib`/`lib64`/multiarch install layout.

## [0.13.1] — 2026-06-12

### Fixed

- **C-library release tarball installs to `lib/`** (was `lib64/` on Linux, the
    manylinux/RHEL default). `find_package(doppler)` via `CMAKE_PREFIX_PATH`
    searches `lib/` on every distro but `lib64/` only where the platform opts
    in, so a Debian/Ubuntu consumer of the manylinux tarball could not find the
    package. Both the Linux and macOS tarballs now use one `lib/` layout. (Found
    by the new post-release C smoke test on its first run.)

## [0.13.0] — 2026-06-12

### Added

- **First-class consumer integration — `find_package` + pkg-config, static and
    shared.** `find_package(doppler)` now offers a **`doppler::doppler-static`**
    target alongside `doppler::doppler`; the self-contained static archive links
    with only the C/C++ runtime (no zmq). `doppler.pc` is now **relocatable**
    (its prefix derives from the file's own location, so an extracted release
    tarball works wherever it lands) and carries `Libs.private`, so
    `pkg-config --static` reports the right link line. A buildable
    [`examples/consumer/`](https://github.com/doppler-dsp/doppler/tree/main/examples/consumer)
    project exercises both link modes, and a **post-release smoke test**
    (`tests/install/release-smoke.sh`, wired into `release.yml`) downloads the
    published tarball and verifies all four consumer paths build, run, and carry
    no `libzmq` dependency.

### Fixed

- **`find_package` shared target is now `doppler::doppler`** (was the
    undocumented `doppler::doppler_lib`, so the `doppler::doppler` shown in the
    docs never resolved). Set via `EXPORT_NAME`.

## [0.12.1] — 2026-06-12

### Changed

- **`libdoppler.a` is now self-contained** — the vendored `libzmq.a` is folded
    into the static archive (via an `ar`/`libtool` merge), so a downstream
    linking the static library needs only `-ldoppler` plus the C/C++ runtime
    (`-lstdc++ -lpthread -lm`) and never an external `-lzmq`. Previously the
    archive recorded the zmq requirement but didn't carry its objects, forcing
    static consumers to supply zmq themselves. The shared `libdoppler.so` was
    already self-contained (zmq linked in); this brings the `.a` to parity.

## [0.12.0] — 2026-06-11

### Added

- **Chirp (LFM) waveform type** — `Synth(type="chirp", freq=f_start, f_end=…)`
    and the `chirp(f_start, f_end)` builder generate a linear-FM sweep whose
    instantaneous frequency ramps from `freq` (the start) to `f_end` over the
    generated length, then holds at `f_end`; `f_end < freq` is a down-chirp. The
    phase is continuous across `steps()`/segments, so concatenated chirps join
    seamlessly (radar pulse compression, SAR, sonar, frequency-response tests).
    Exposed on every face: the `wfmgen --type chirp --freq … --f_end …` CLI, the
    JSON spec (`"type":"chirp"`, `"f_end"`), `Segment`/`Composer` (the sweep
    spans the segment's `num_samples`), and SigMF annotations (the
    `f_start..f_end` occupied band). Byte-identical CLI ⇄ Composer ⇄ standalone,
    and the C `wfm_synth_step()`/`wfm_synth_steps()` paths agree bit-for-bit.
    (#113)
- **User bit-pattern waveform type (`bits`)** — `Synth(type="bits",   pattern=…, modulation=…)` and the `bits(pattern, modulation)` builder play
    back a specific bit sequence (preambles, sync words, test vectors). The
    pattern is a 0/1 string (`"10110101"`), a hex string (`"0xAA55"`, MSB
    first), or any array-like of 0/1; `modulation` maps it to symbols
    (`"none"` → 0/1 amplitude, `"bpsk"` → ±1, `"qpsk"` → two bits/symbol,
    Gray-coded). Each bit is held `sps` samples and the pattern **cycles** to
    fill the requested length (one pass is `Synth.n_samples`). On every face:
    the `wfmgen --type bits --bits/--bits-hex/--bits-file --modulation …` CLI,
    the JSON spec (`"pattern"` + `"modulation"`), `Segment`/`Composer` (incl.
    `.sum` scenes), and SigMF. Byte-identical CLI ⇄ Composer ⇄ standalone, and
    the C `wfm_synth_step()`/`wfm_synth_steps()` paths agree bit-for-bit.
    (#114)
- **RRC pulse shaping for the PSK carriers** — `pulse="rrc"` (with `rrc_beta` /
    `rrc_span`) on a `pn` / `bpsk` / `qpsk` `Synth` replaces the rectangular
    sample-and-hold with **root-raised-cosine** shaping, so a band-limited
    carrier (e.g. WCDMA QPSK at roll-off 0.22) comes straight from the generator
    instead of being hand-filtered. The symbol-rate impulse train is run through
    the existing `fir` core with `wfm_rrc_taps`, scaled for unit transmit power;
    the FIR delay line carries across blocks so the per-sample and block paths
    agree bit-for-bit. Default `pulse="rect"` is byte-stable. On every face: the
    `wfmgen --pulse rrc --rrc-beta … --rrc-span …` CLI, the JSON spec, and
    `Segment`/`Composer` (incl. `.sum`). Byte-identical CLI ⇄ Composer ⇄
    standalone. (#115)
- **`wfmgen` exposed as a callable in libdoppler** — the composer CLI is now
    the library function `doppler_wfmgen(int argc, char **argv)` (declared in
    `wfm/wfmgen.h`), archived into `libdoppler.a`/`.so`, so a C program that
    links the library can drive the full generator in-process without shelling
    out. The standalone `wfmgen` binary is a one-line `main` shim over it, so
    the two are the exact same code path (byte-identical output). The zmq sink
    is statically linked, so there is **no runtime `libzmq` dependency** (the
    `.so`'s dynamic-dep list is unchanged); the cost is binary size
    (`libdoppler.a` +~132 KiB, `libdoppler.so` +~1.2 MiB incl. embedded zmq).

### Fixed

- **`source` heap overflow on large single-call generation** (#116) —
    `LO.steps(n)`, `NCO.steps_u32`/`steps_u32_scaled`/`steps_u32_ovf`, and
    `AWGN.generate(n)` sized their output buffer to a fixed internal cap
    (`*_MAX_OUT = 65536`) but then wrote `n` samples, overflowing the heap for
    `n > 65536` — silently corrupting memory, and segfaulting once `n` ran past
    a page (e.g. `LO.steps(393216)`). The bindings now allocate a NumPy-owned
    output of exactly `n` per call (the same pattern `Synth.steps` uses), which
    also makes each returned array independent: concatenating or holding results
    across calls is now correct (the old shared reuse buffer aliased/overwrote
    earlier results). Also fixes a leak of the `LO`/`AWGN` reuse buffers at
    dealloc.

## [0.11.0] — 2026-06-11

The **waveform composer** and the **`wfm` API cleanup**. doppler can now build a
multi-source *scene* — a signal of interest, interferers, and a noise floor mixed
at one sample rate — and sequence scenes into a timeline, with full amplitude
bookkeeping (per-source level, headroom, clip detection). Alongside it, the whole
waveform subsystem is unified under one `wfm` name: one Python package
(`doppler.wfm`), one engine object (`Synth`), and one CLI (`wfmgen`).

> **Breaking (pre-1.0):** the Python import path, the `Synth`/`Source` model, two
> builder/method parameters, and the C symbol prefix all changed. See **Changed**
> / **Removed** below and the migration table in the
> [Waveform Generator guide](https://doppler-dsp.github.io/doppler/guide/wfmgen/).

### Added

- **Waveform composition** — mix and sequence waveforms into a scene:
    - `Segment.sum(*synths, num_samples=…)` mixes several `Synth` at the same time
        over **one resolved noise floor** — computed once, in C, so the Python /
        JSON / `wfmgen --from-file` faces are byte-identical. (#99, #100, #101)
    - `Segment.add(*segments)` and `Timeline` sequence segments back-to-back in
        time. (#102)
    - Per-source **`level`** (dBFS), **`--headroom`** (SNR-invariant output
        scaling so peaks fit full-scale), and **clip detection** (`peak_dbfs` /
        `clip_fraction`; `--clip-report` / `--clip-error`). (#96, #97, #98, #103)
    - The JSON spec gains a `"sum"` array; SigMF emits one annotation per source.
- **`wfm_io_demo`** + a Waveform I/O gallery page: write one capture to all four
    containers (raw / CSV / BLUE / SigMF) and read each back, showing which
    metadata each recovers. (#104, #110)

### Changed

- **The waveform subsystem is unified under `wfm`** (the API cleanup):
    - **Python package `doppler.wfmgen` → `doppler.wfm`**, with **one import
        path** — `from doppler.wfm import …` re-exports the whole surface.
        **(breaking)** (#106, #109)
    - **One waveform object `Synth`** that both generates (`.steps()` / `.step()`
        / `.reset()`) and composes (passed straight into `Segment.sum`). `Source`
        is gone; the builders `tone()` / `bpsk()` / `qpsk()` / `pn()` / `noise()`
        return `Synth`. **(breaking)** (#109)
    - **One CLI, `wfmgen`** — a single waveform from flags *or* a multi-segment
        scene from `--from-file`, into raw / CSV / BLUE / SigMF, to file / stdout
        / `zmq://`. The wheel now ships the `wfmgen` binary + a console-script
        shim that `exec`s it. (#107)
    - Parameter renames: **`noise(level=)`** (was `nf=`) and
        **`Segment.sum(num_samples=)`** (was `n=`). **(breaking)** (#109)
    - C symbols **`synth_*` → `wfm_synth_*`**; the C sources moved under
        `native/{src,inc}/wfm/`. **(breaking, C ABI)** (#106)

### Removed

- **The single-shot `wavegen` tool** — the C binary, the PEP 723 script, and the
    Python CLI. A one-segment `wfmgen` run is byte-for-byte identical to it, so
    `wfmgen` is now the only CLI. **(breaking)** (#107)
- `Synth`'s `get_*` / `set_*` engine accessors are no longer on the public object
    (internal engine state). **(breaking)** (#109)

## [0.10.2] — 2026-06-10

Build, tooling, and documentation release — the importable API and C ABI are
**identical to 0.10.1** (no functional library change). Ships the build cleanup
from adopting just-makeit 0.19.0 and a new representative Benchmarks page.

### Added

- **Dedicated Benchmarks page** (`docs/benchmarks.md`) with representative,
    hand-measured numbers committed under `benchmarks/published/`. Each release
    is measured in two builds — **portable** (the PyPI wheel) and **native**
    (`-DDOPPLER_NATIVE=ON`) — run **interleaved** to denoise the comparison;
    throughput is reported in MSa/s, and every snapshot carries full
    reproducibility metadata (CPU + scaling governor, compiler + flags,
    glibc/NumPy versions, commit, timestamp). (#78–#89)

### Removed

- **Unused Windows MinGW CMake boilerplate** from every component. doppler has
    been Linux/macOS-only since 0.10.1; adopting **just-makeit 0.19.0** — which
    gates the per-component Windows DLL-copy block behind `[project] platforms`,
    resolving the doppler-filed
    [jm#213](https://github.com/just-buildit/just-makeit/issues/213) — drops
    ~200 lines of dead build scaffolding. The wheel is unaffected. (#90)

### Changed

- Pin just-makeit **0.18.0 → 0.19.0** (manifest-drift gate + benchmark tooling).
- Developer tooling: a `.clangd` config that silences diagnostics the compile
    database can't resolve; CMake now always exports `compile_commands.json`;
    and a PR CI gate that flags leaked AVX instructions in the portable build.
    (#73, #74, #83)

## [0.10.1] — 2026-06-09

Bug-fix release: two macOS arm64 correctness issues surfaced by the new macOS
test gate added in 0.10.0. Both are pre-existing — not 0.10.0 regressions.

### Fixed

- **VM-mirrored ring buffers (`F32Buffer` / `F64Buffer` / `I16Buffer`) were
    unusable on 16 KiB-page systems (macOS arm64).** `create()` rejected any
    sub-page request — `F32Buffer(1024)` is 8 KiB, below one 16 KiB page — so
    the double-mapping could not be constructed. Sizes are now rounded **up** to
    the smallest power-of-two that spans a whole page; read the real size back
    from `.capacity` (it may exceed the request). 4 KiB-page (Linux x86-64)
    behaviour is unchanged. The Windows path aligns to the 64 KiB allocation
    granularity for the same reason. (#66)
- **The Python `Composer` diverged byte-for-byte from the `wavegen` CLI for
    `qpsk` / `cf32` on arm64.** The composer pulled samples through the scalar
    `synth_step()` while the CLI uses the block `synth_steps()`; under
    `-ffast-math` those contract fused multiply-adds differently, and QPSK's
    irrational ±1/√2 symbol leg exposed the ULP gap (other waveforms' ±1/0 legs
    are exact, so they were immune; it also rounded away under `ci16`/`ci8`,
    leaving only `cf32` affected). The composer now drives the **same**
    `synth_steps()` the CLI uses, so both faces are byte-identical by
    construction. (#67)

## [0.10.0] — 2026-06-09

The headline is **broadened Python support: 3.9 – 3.14** (the floor drops from
3.11). doppler's C extensions were already 3.9-clean — no `Py_NewRef`-era API,
no runtime PEP-604 unions, no `match`/`case` — so the floor was set entirely by
NumPy (2.1 dropped 3.9, 2.3 dropped 3.10), not by doppler. Lowering it is
packaging plus two small fixes flushed out by the new CI rows.

### Changed

- **Supported Python is now 3.9 – 3.14** (`requires-python = ">=3.9"`). Each
    interpreter resolves a compatible dependency set via PEP 508
    `python_version` environment markers — NumPy is capped per-version
    (`<2.1` on 3.9, `<2.3` on 3.10) in both the runtime deps and the build
    backend, so the cp39 wheel builds against NumPy 2.0.x and runs against any
    later 2.x via the stable ABI. The dev group caps `scipy` / `matplotlib` /
    `pytest` the same way. CI gains 3.9 / 3.10 / 3.11 test rows and the release
    wheel matrices gain cp39 – cp311 (manylinux_2_28 x86_64 + macOS arm64).

### Fixed

- **`SpecanConfig` pydantic fields** use `Optional[float]`, not PEP-604
    `float | None`. pydantic force-evaluates field annotations via
    `get_type_hints` at class-definition time, which raises on Python 3.9
    (`from __future__ import annotations` does not help — pydantic resolves the
    deferred strings), breaking CLI test collection. Functionally identical;
    3.9-safe.
- **`test_missing_extras`** reproduces a stdlib-only "bare" install with an
    in-process import-blocker instead of a nested `venv.create()` interpreter.
    A venv created under a uv-managed python-build-standalone interpreter (the
    new 3.9 / 3.10 CI rows) cannot bootstrap its own stdlib and died before the
    CLI's install-hint could print; blocking the extras' imports in the working
    interpreter tests identically on every version.

## [0.9.0] — 2026-06-08

The headline is **real-time pacing + a container reader**: a C-first sample
clock that emits and timestamps samples at their true rate, and the `Reader`
dual of `Writer`.

### Added

- **`Reader` — the dual of `Writer`** (`wfm_reader`, C-first; Python bind-only)
    — reads a capture back to `complex64`, **auto-detecting** the container
    (BLUE `"BLUE"` magic / `.sigmf-meta` sidecar / `.csv` / raw). Self-describing
    containers (BLUE, SigMF) recover sample type, byte order, `fs` and `fc` from
    metadata; headerless raw/CSV take hints. All detection, header parsing and
    wire→unit conversion live in C; `doppler.wfmgen.compose.Reader` is thin glue
    (`.read()` / `.read_all()` + `file_type` / `sample_type` / `fs` / `fc` /
    `num_samples`). Round-trips every container against the writer.

- **Benchmarks** for the new subsystems: `bench_timing_core`,
    `bench_wfm_writer_core`, `bench_wfm_reader_core` (C, via `make bench`) and
    `bench_timing` / `bench_compose` (Python, pytest-benchmark).

- **Real-time sample-clock pacing + timestamping** (`timing_core`, C-first) —
    a `dp_sample_clock_t` that paces a producer to `fs` on a drift-free
    `epoch + n/fs` schedule (mimicking a hardware sample clock) and stamps
    blocks with their ideal UNIX-epoch-ns time. Exposed both ways:

    - **CLI**: `wfmgen --realtime` throttles the emit loop to `fs` (zmq or
        file); `--realtime-resync` re-anchors on underrun. Pacing is
        byte-transparent and reports an underrun summary at exit.
    - **Python**: `doppler.wfmgen.compose.SampleClock(fs, resync=...)` with
        `pace()` / `stamp()` / `reset()` / `resync()` and `underruns` /
        `max_lateness` telemetry; the `pace()` sleep releases the GIL.

    POSIX only (mirrors the ZMQ sink). Drift-free because each deadline is
    recomputed from the cumulative sample count, not summed sleeps.

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
[0.10.0]: https://github.com/doppler-dsp/doppler/compare/v0.9.0...v0.10.0
[0.10.1]: https://github.com/doppler-dsp/doppler/compare/v0.10.0...v0.10.1
[0.10.2]: https://github.com/doppler-dsp/doppler/compare/v0.10.1...v0.10.2
[0.11.0]: https://github.com/doppler-dsp/doppler/compare/v0.10.2...v0.11.0
[0.12.0]: https://github.com/doppler-dsp/doppler/compare/v0.11.0...v0.12.0
[0.12.1]: https://github.com/doppler-dsp/doppler/compare/v0.12.0...v0.12.1
[0.13.0]: https://github.com/doppler-dsp/doppler/compare/v0.12.1...v0.13.0
[0.13.1]: https://github.com/doppler-dsp/doppler/compare/v0.13.0...v0.13.1
[0.13.2]: https://github.com/doppler-dsp/doppler/compare/v0.13.1...v0.13.2
[0.14.0]: https://github.com/doppler-dsp/doppler/compare/v0.13.2...v0.14.0
[0.14.1]: https://github.com/doppler-dsp/doppler/compare/v0.14.0...v0.14.1
[0.15.0]: https://github.com/doppler-dsp/doppler/compare/v0.14.1...v0.15.0
[0.15.1]: https://github.com/doppler-dsp/doppler/compare/v0.15.0...v0.15.1
[0.16.0]: https://github.com/doppler-dsp/doppler/compare/v0.15.1...v0.16.0
[0.16.1]: https://github.com/doppler-dsp/doppler/compare/v0.16.0...v0.16.1
[0.16.2]: https://github.com/doppler-dsp/doppler/compare/v0.16.1...v0.16.2
[0.17.0]: https://github.com/doppler-dsp/doppler/compare/v0.16.2...v0.17.0
[0.18.0]: https://github.com/doppler-dsp/doppler/compare/v0.17.0...v0.18.0
[0.19.0]: https://github.com/doppler-dsp/doppler/compare/v0.18.0...v0.19.0
[0.19.1]: https://github.com/doppler-dsp/doppler/compare/v0.19.0...v0.19.1
[0.2.0]: https://github.com/doppler-dsp/doppler/compare/v0.1.0...v0.2.0
[0.2.3]: https://github.com/doppler-dsp/doppler/compare/v0.2.0...v0.2.3
[0.2.5]: https://github.com/doppler-dsp/doppler/compare/v0.2.3...v0.2.5
[0.2.6]: https://github.com/doppler-dsp/doppler/compare/v0.2.5...v0.2.6
[0.2.7]: https://github.com/doppler-dsp/doppler/compare/v0.2.6...v0.2.7
[0.2.8]: https://github.com/doppler-dsp/doppler/compare/v0.2.7...v0.2.8
[0.2.9]: https://github.com/doppler-dsp/doppler/compare/v0.2.8...v0.2.9
[0.22.0]: https://github.com/doppler-dsp/doppler/compare/v0.21.0...v0.22.0
[0.23.0]: https://github.com/doppler-dsp/doppler/compare/v0.22.0...v0.23.0
[0.23.1]: https://github.com/doppler-dsp/doppler/compare/v0.23.0...v0.23.1
[0.24.0]: https://github.com/doppler-dsp/doppler/compare/v0.23.1...v0.24.0
[0.25.0]: https://github.com/doppler-dsp/doppler/compare/v0.24.0...v0.25.0
[0.26.0]: https://github.com/doppler-dsp/doppler/compare/v0.25.0...v0.26.0
[0.26.1]: https://github.com/doppler-dsp/doppler/compare/v0.26.0...v0.26.1
[0.27.0]: https://github.com/doppler-dsp/doppler/compare/v0.26.1...v0.27.0
[0.28.0]: https://github.com/doppler-dsp/doppler/compare/v0.27.0...v0.28.0
[0.28.1]: https://github.com/doppler-dsp/doppler/compare/v0.28.0...v0.28.1
[0.3.1]: https://github.com/doppler-dsp/doppler/compare/v0.2.9...v0.3.1
[0.3.2]: https://github.com/doppler-dsp/doppler/compare/v0.3.1...v0.3.2
[0.3.3]: https://github.com/doppler-dsp/doppler/compare/v0.3.2...v0.3.3
[0.3.4]: https://github.com/doppler-dsp/doppler/compare/v0.3.3...v0.3.4
[0.3.5]: https://github.com/doppler-dsp/doppler/compare/v0.3.4...v0.3.5
[0.3.6]: https://github.com/doppler-dsp/doppler/compare/v0.3.5...v0.3.6
[0.3.7]: https://github.com/doppler-dsp/doppler/compare/v0.3.6...v0.3.7
[0.33.0]: https://github.com/doppler-dsp/doppler/compare/v0.32.0...v0.33.0
[0.33.1]: https://github.com/doppler-dsp/doppler/compare/v0.33.0...v0.33.1
[0.33.2]: https://github.com/doppler-dsp/doppler/compare/v0.33.1...v0.33.2
[0.33.3]: https://github.com/doppler-dsp/doppler/compare/v0.33.2...v0.33.3
[0.33.4]: https://github.com/doppler-dsp/doppler/compare/v0.33.3...v0.33.4
[0.33.5]: https://github.com/doppler-dsp/doppler/compare/v0.33.4...v0.33.5
[0.34.0]: https://github.com/doppler-dsp/doppler/compare/v0.33.5...v0.34.0
[0.35.0]: https://github.com/doppler-dsp/doppler/compare/v0.34.0...v0.35.0
[0.36.0]: https://github.com/doppler-dsp/doppler/compare/v0.35.0...v0.36.0
[0.37.0]: https://github.com/doppler-dsp/doppler/compare/v0.36.0...v0.37.0
[0.37.1]: https://github.com/doppler-dsp/doppler/compare/v0.37.0...v0.37.1
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
[0.9.0]: https://github.com/doppler-dsp/doppler/compare/v0.8.0...v0.9.0
[unreleased]: https://github.com/doppler-dsp/doppler/compare/v0.37.1...HEAD
