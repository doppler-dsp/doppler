# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

______________________________________________________________________

> **API stability notice** — doppler is pre-1.0. Minor releases may
> make breaking changes. Check this file before upgrading.

## [Unreleased]

### Added

- **`Interleaver` is certified** — `src/doppler/coding/tests/validation/interleaver/`,
    20 limits and 8 findings, plus `coding`'s first
    `test_validation_limits.py`. The permutation is scored against numpy's
    reshape-transpose rather than against doppler's own inverse, and the
    burst claim is measured end to end over RS(255,223): the corrigible
    burst is `E * depth` octets and not one octet more.

### Fixed

- **The `Interleaver` object's own permutation was unpinned in C.**
    Transposing its three calls into `dp_interleave.h` left the entire
    155-test C suite green — a transposed block interleave is still a
    permutation, still inverts, and still permutes each block within
    itself, so every existing assertion survived it. `test_interleaver_core.c`
    now compares the object against the kernel it wraps, checks
    `interleaver_create_rx` (which had no coverage at all), measures the
    one-hit-per-codeword invariant through the object, and pins
    statelessness. Six sabotages, six reds.

- **`interleave_burst_demo.py` demonstrates the silent-mismatch hazard.** A
    receiver holding a different geometry raises nothing and returns the
    right number of bits with half of them wrong.

## [0.44.0] — 2026-08-24

### Breaking

- **`CF128` is removed from the streaming wire protocol.** It is 32 bytes on
    both x86-64 and aarch64 but a different *representation*, so every field a
    receiver checks agreed and the samples decoded to nonsense — across the
    multi-arch images doppler itself publishes. Use `CF64`. Wire value 2 is
    retired, not reused; `dp_sample_type_is_valid()` now derives from
    `dp_sample_size()` so a type with no size is not a type.

- **The streaming wire format is v2:** a 64-byte header (down from 96),
    `DPSTREAM` magic, BLUE format codes, a `payload_bytes` the receiver
    actually validates, and `TLM16` as a frame *kind* rather than a sample
    format. v1 and v2 are unrecognisable at byte 0, so both ends must move
    together. `recv()` headers gain `kind`/`flags`/`payload_bytes`/`version`,
    and `sample_type` becomes `format`. Layout and rationale:
    [`docs/design/streaming.md`](https://doppler-dsp.github.io/doppler/design/streaming/).

- **`wfm_spec_to_json()` takes a `seed_advance` argument**, inserted before
    `headroom` because it describes the stream rather than the writer.
    `WFM_SEED_ADVANCE_NONE` reproduces the old output byte for byte. Fixes
    [#978](https://github.com/doppler-dsp/doppler/issues/978).

### Added

- **`dp_stream_interrupt()` — a blocking receive you can actually stop.** The
    library checks a process-wide flag *inside* the wait, so `*_recv` returns
    `DP_ERR_INTERRUPTED` instead of parking forever once a sender goes quiet.
    Python gets `interrupt_on_sigint()` (a context manager) plus
    `interrupt()`/`resume()`/`interrupted()`; an unblocked `recv()` raises
    `KeyboardInterrupt`.

- **A sender can say it has finished, and a receiver hears it.**
    `dp_pub_send_eos()` / `Push.send_eos()` publish a zero-payload
    `DP_KIND_EOS` frame; `*_recv` reports `DP_ERR_EOF` and Python raises
    `EOFError`. A timeout only ever answered "nothing *yet*". Delivery follows
    the tier — at-most-once on PUB/SUB, at-least-once on PUSH/PULL.

- **The ring buffer can say it is finished, and its wait can end.**
    `dp_<t>_close()` / `dp_<t>_closed()`, and `dp_<t>_wait()` returns `NULL`
    on drained-and-closed or on interrupt instead of spinning forever at 100%
    CPU with no flag to read. Contract change for `wait()`'s callers.

- **A follow read is stoppable from Python.** `Reader.read_follow()` installs
    `dp_interrupted` at construction; before this it had no escape at all.
    A stop still does not by itself end the read — `follow_grace_ms` defaults
    to forever, so shutdown propagates *through* the file.

- **`dp_pub_flush()` and `dp_stream_drain()`** — "does the server have it?"
    and "stop cleanly", neither of which a sender could previously ask.
    `drain()` waits for CLOSED, so a process exiting on its return does not
    abandon the work; a send afterwards returns `DP_ERR_CLOSED`, a state
    rather than a transport failure.

- **`dp_mean_power()` / `doppler.stream.mean_power()`** — mean power of a
    complex block normalised to full scale, so `10*log10()` is dBFS whatever
    the wire carried. Replaces four private copies of the same formula. Also
    `doppler.stream.format_name()` and `data_rep` in the `recv()` header.

- **One wait contract for network, memory and disk** —
    [`docs/design/io-termination.md`](https://doppler-dsp.github.io/doppler/design/io-termination/).
    The three transports had the same defect three times: "no data now" was
    indistinguishable from "no data ever". Written before the implementation
    so the vocabulary could not diverge across them.

- **`docs/design/streaming.md`** — the transport contract, which had no home:
    the wire byte by byte, chunking and reassembly, the subjects derived from
    an endpoint, buffer ownership, and what the layer does not promise.
    Writing it produced #956, #958 and #959.

- **A worked example of stopping a stream cleanly**
    (`graceful_shutdown_demo.py`), driving the real `wfmgen` unpaced. Asserts
    the three previously unanswerable things: the run can be interrupted, the
    consumer *learns* the stream ended, and the tail arrived. Measured: the
    producer stops in ~31 ms while saturated at ~3 GB/s.

- **`work_queue_shutdown_demo.py`** — the SPMC case, one producer and five
    consumers, asserting the trap: a work queue load-balances, so `send_eos()`
    ends *one* consumer and the other four wait forever. Ending a pool of N
    needs N markers.

- **The two-process streaming examples are RUN now, in pairs.**
    `transmitter`/`receiver`, `replier`/`requester` and
    `pipeline_send`/`pipeline_recv` move from `.examples-skip` to a
    `.examples-pairs` registry whose evidence regex means a pair that exits 0
    having exchanged nothing still fails. Gate: 76 passed / 7 skipped → 80 / 1.

- **`costas`'s pull-in is measured, not assumed** —
    `native/validation/costas_pullin.c`. The loop an entire receiver's carrier
    recovery rests on had only a jitter validator, which its own MPSK twin
    calls "not the question". Acquires at and inside `bn/m` on both signs, and
    the bound scales with `bn`. Symmetric, so [#982](https://github.com/doppler-dsp/doppler/issues/982)'s
    asymmetry is not in this loop.

- **`async_dsss_receiver_core.h` no longer states its pull-in as prose.** The
    header claimed "~tens of Hz" of residual; it is **−54..+375 Hz**, 2–6× the
    loop's own bound, so the receiver asks its carrier loop to acquire from
    outside its range on most draws.

### Changed

- **The version sites have one declaration, one reader and one writer**
    (`scripts/version_sites.py`). Two lists — one to read each site, one to
    write it — encoded *jointly impossible* rules: the writer stripped a
    pre-release suffix that the reader then failed to match, so
    `make bump-version VERSION=X.Y.Zrc1` produced a tree that could not pass
    `release.yml`'s own check. Pre-releases are refused with that reason;
    supporting them needs [just-buildit#30](https://github.com/just-buildit/just-buildit.github.io/issues/30).

- **Re-vendored `standard.mk`: a bare exported `VERSION` can no longer arm a
    release target.** `VERSION=9.9.9 make bump-version`, with no argument
    typed, rewrote every manifest. Fixed upstream
    ([just-buildit#29](https://github.com/just-buildit/just-buildit.github.io/pull/29))
    rather than copied here: `PROJECT = DOPPLER` makes
    `JUST_BUILDIT_DOPPLER_VERSION` the spelling that carries intent.

- **The CI image fingerprint hashes the image's inputs, not two whole files.**
    `cat bootstrap.toml Dockerfile.ci | sha256sum` counted a `[project]   version` no layer reads, which is what had kept doppler's version out of
    that file. Now parsed, with `[project]` excluded — one reasoned exclusion
    beats an include-list that stops covering a table added later.

- **The interrupt primitive moved to the core library** as
    `native/inc/dp_interrupt.h`. It lived in the optional `libdoppler_stream`
    while two of its three users are core, and never had a NATS dependency.
    The `dp_stream_*` spellings still forward and are **deprecated**.

- **The DSSS examples describe their burst to wfmgen instead of assembling
    it.** Three examples hand-built `[preamble | sync | payload | CRC]`, so
    the framing rules lived in four places — including a second private
    `_crc16()`. Decoded bursts are bit-identical after the move. Ground truth
    now comes from `bpsk_map`, which fixes a sign inversion both files had.

- **The DSSS examples measure with doppler's objects instead of hand-rolled
    numpy**, and two of those copies were wrong.
    `dsss_burst_demo`'s repetition sweep is `BurstAcquisition` now: the hand
    correlator matched one code period regardless of `acq_reps` and so
    measured +0.6 dB across an 8× change while claiming +9
    ([#980](https://github.com/doppler-dsp/doppler/issues/980)).

- **`dsss_burst_demo.py`'s payload is now actually spread, and a gate says
    so.** Its headline claim was false — the "payload" was a BPSK segment at
    one pulse per bit, filling 0.010 of the band against the preamble's 0.69.
    Now 0.53 against 0.69, with a ratio assertion beside it.

- **The async DSSS example measures its acquisition rate** instead of
    asserting one lucky draw: 5/8 (TCA) and 6/8 (±50 kHz) over fixed noise
    draws, decoding cleanly whenever it acquires. Why that rate:
    [#982](https://github.com/doppler-dsp/doppler/issues/982).

- **Both receiver examples show the same dashboard** — wire header, power in
    dBFS, sustained MS/s/MB/s, and one-way latency from `timestamp_ns`, which
    nothing had been using. Power comes from one shared C primitive, so the
    two faces cannot disagree about a frame.

- **The examples now live where their language does.** `examples/c/` →
    `native/examples/`, and the two stand-alone consumer projects →
    `example-projects/`, which says what they are. All pure renames.

- **just-makeit pin 0.63.3 → 0.68.0**, across four bumps. 0.67.1 for
    `process_global` (the mechanism [#976](https://github.com/doppler-dsp/doppler/issues/976)
    needs); 0.67.3 retiring both workarounds it shipped with, plus jm gh-1140
    which doppler filed after finding a generated contract header that `apply`
    never rewrote and `status` never compared; 0.68.0 for gh-1141/gh-1142,
    whose new version gate doppler passes **only** because the `0.1.0` below
    was fixed first.

### Fixed

- **Two `[project] version` fields had never been bumped, and nothing was
    watching either.** `bootstrap.toml` sat at `0.3.7`, `just-makeit.toml` at
    `0.1.0` — the value from the *initial commit* — while doppler shipped to
    0.43.2. `make version-check` now probes five files instead of three.

- **A stop requested in one extension module now reaches a wait in another**
    ([#976](https://github.com/doppler-dsp/doppler/issues/976)). Each `.so`
    links the interrupt primitive statically and CPython imports `RTLD_LOCAL`,
    so the flag `doppler.interrupt` set was not the flag `doppler.buffer` read
    — invisible because the only setter and the only tested wait shared a
    module.

- **The runtime image's own documented first command failed** with
    `FileNotFoundError` ([#954](https://github.com/doppler-dsp/doppler/issues/954)).
    `awgn_demo.py` wrote to a repo-relative path. Two gates were watching and
    neither could see it: the example harness pre-created the directory, and
    the image smoke checked `import doppler` rather than what the image is
    for. Both fixed.

- **The C receiver example ignored Ctrl+C once the transmitter stopped.** No
    receive timeout, so `dp_sub_recv` parked inside the NATS client and the
    SIGINT flag was never re-read — invisible while traffic flows. 0.26 s to
    exit with the fix. Also found: CI ran `make nats-down` *before* the
    examples step, so every broker-dependent example silently self-skipped.

- **34 example docstrings told you to run a path that has not existed for
    releases.** `check_doc_paths.py` read an example's docstring as code
    rather than documentation; examples are in scope now, bare paths included.

- **A feature-test macro in a public header does not work, and two of ours
    were there** ([#986](https://github.com/doppler-dsp/doppler/issues/986)).
    glibc latches the feature set on the first `features.h` include, so
    `#define _GNU_SOURCE` in a header is a no-op for most translation units.

- **`wfmgen --continuous` could not be stopped without losing its output**,
    and **a `--record` capture now replays the run it recorded** —
    `seed_advance` was written to the record but never read back.

- **`ddc_fn_scaling.py` read one point of its scaling curve** and called an
    ordinary cloud VM a GIL regression
    ([#990](https://github.com/doppler-dsp/doppler/issues/990)). 1.02× at two
    threads with **1.90× at four** is a clock ceiling, not a held GIL, which
    caps every count near 1×. It reads the whole curve now.

- **`spool_follow_demo` waited for a Ctrl+C nobody could send** and hung the
    example gate.

- **The stimulus gate's `evm` marker was defeated by a one-word prefix.**

- **An end-of-stream frame on the work queue is acked**, so it does not
    redeliver forever, and **`dp_frame_parse` checks an EOS frame's `format`**
    rather than accepting any.

- **`F64Buffer.close()` and `I16Buffer.close()` documented themselves with
    the wrong type's name.**

## [0.43.2] — 2026-08-22

### Fixed

- **The release's PyPI-index wait could only ever time out, and took the
    container images and the GitHub Release down with it — twice.** The
    `publish-container` job waited for the new version to be resolvable before
    building images, using

    ```sh
    if pip index versions doppler-dsp 2>/dev/null | grep -q "$VERSION"; then
    ```

    That job has **no `setup-python`**. A bare `pip` was never guaranteed to
    exist on the runner, and `2>/dev/null` turned its absence into "not
    indexed yet" — forever. So the loop had no reachable success path: v0.43.0
    timed out at 5 minutes and v0.43.1 at 15, both while the version was live
    on PyPI the entire time, and in both releases `Create GitHub Release` was
    skipped because it needs this job.

    Widening the budget from 5 to 15 minutes was treating the symptom, and
    v0.43.1 is the measurement that says so: the same failure, 900 seconds
    later. **A check that cannot succeed is not a slow check.**

    It now reads the **simple index** — the thing pip actually reads — with
    `curl`, which is always present, and matches `doppler_dsp-<version>[-.]`
    so that `0.43.1` cannot match `0.43.10` (a wheel is
    `doppler_dsp-<v>-…whl`, an sdist `doppler_dsp-<v>.tar.gz`). On timeout it
    now prints the versions the index *does* list, so the next failure says
    what it saw rather than only that it gave up.

    Verified against the live index in both directions, which is the property
    the old check never had: a real version is found, a fabricated one is not.

## [0.43.1] — 2026-08-22

### Fixed

- **The release's C-library job ran half its script on the runner, and
    v0.43.0 shipped without tarballs, containers or a release page.** The step
    passed its script to the manylinux container as a double-quoted argument,
    and a COMMENT inside it carried an unbalanced `"`:

    ```
    # than assumed: without the line `git archive` prints "detected
    # dubious ownership" and writes nothing, and with it the export
    ```

    That closed the argument early, so every line after it was parsed by the
    **runner** instead of the container — and `make package-starter-tarball`
    ran as the runner user against a `build/` that cmake had just created as
    **root** inside the container. It surfaced as

    ```
    mkdir: cannot create directory 'build/starter-pkg': Permission denied
    ```

    a permissions error with no permissions bug behind it. The backticks in
    those comments were command-substituted on the runner too, which is where
    the log's `safe.directory: command not found` came from — the tell that
    the wrong shell was reading the script. Only two of the four commands ever
    reached the container.

    It surfaced now because `package-starter-tarball` is new since v0.42.0, so
    v0.43.0 was its first release. `publish-python` had already succeeded, so
    the wheels are on PyPI and 0.43.0 is installable; `github-release` needs
    the C jobs and was skipped.

    The script now reaches the container on **stdin through a quoted
    heredoc**, so the runner performs no expansion on it at all. Verified by
    delivering it to a stand-in for `docker`: four commands arrive where two
    did.

- **Every workflow `run:` block is parsed, by `make workflow-syntax-check`.**
    GitHub does not parse them, so a shell syntax error does not announce
    itself as one — the shell runs what it managed to parse and the step fails
    somewhere else entirely. `bash -n` reports the defect above in under a
    millisecond, naming the job and the step. `${{ }}` is substituted first,
    since GitHub does that before any shell sees it, and a `shell:` naming
    python/pwsh is skipped rather than guessed at.

- **The container publish no longer fails on PyPI's index lag.** It waited 30
    x 10s for `pip index versions` to show the new release and gave up at
    exactly five minutes; v0.43.0 took longer, so a job whose dependency had
    genuinely succeeded failed anyway and took the images with it. The budget
    is the whole gate — there is nothing to retry against a dependency that is
    simply still propagating — so it is now 15 minutes, inside the job's
    45-minute cap, and the success line reports how long it actually took.

## [0.43.0] — 2026-08-22

### Highlights

248 entries since v0.42.0, and this section is the shape of them. Nothing
here replaces an entry, and where a change was made and then superseded inside
this same release the **net** effect is what is stated.

**Act on these first.**

- **`nda_tap` is gone from both M-PSK receivers**, along with the
    acquisition/tracking handover, `ContinuousMpskReceiver`, `warmup_syms`,
    `acq_to_track`, the `tracking` property and `configure_lock()`. One
    M-th-power NDA discriminator now steers the LO from the first strobe to
    the last. (`"mf_all"` was renamed to `"mf_out"` earlier in this cycle;
    that rename no longer applies to anything.)
- **`MpskReceiverR` is a VIEW over `MpskReceiver`, not a second type**, so
    the two can no longer drift apart in what they derive or accept.
- **`HalfbandDecimator_create()` takes `(h, h_len)`, not `(num_taps, h)`.**
    C callers must swap; the types differ so it fails to compile rather than
    misbehaving, and the Python face is unchanged.
- **Rust: `Lo::steps_ctrl` takes `&[f64]`.** It declared its control port
    `*const f32` while the C took `const double *`, so C read twice the
    buffer from safe Rust ([#911](https://github.com/doppler-dsp/doppler/issues/911)).
- **`DsssReceiver` refuses `sps < 2`** instead of `SIGABRT`ing the
    interpreter with no traceback.
- **`MPSK_RX_LOOPS_STATE_VERSION` 6 → 7**; frame flags on a waveform that
    cannot carry one now refuse rather than being ignored.

**Forward error correction, end to end.**

- `doppler.viterbi` is now **`doppler.coding`**, and it encodes as well as
    decodes: the CCSDS rate-1/2 K=7 convolutional code, Reed-Solomon
    (255,223) that **corrects** rather than only detects, symbol
    interleaving, and node synchronisation.
- **`doppler.wfm.Frame`** describes a frame as a field list and a stage
    list — one description read from both ends, so the assembler and the
    receiver cannot disagree about a bit layout. A CCSDS CADU is a
    configuration, not a code path.
- The **CCSDS receive chain**, a link demo that runs it end to end, and
    coding gain measured through a real receiver.
- **`wfmgen` can generate a coded waveform**, and a Python receiver can
    ACQUIRE a frame rather than only check one it was handed.

**The M-PSK receiver.**

- **`MpskReceiver.locked`** and **`lock_time`** — the binary lock indicator
    and the acquisition time, as numbers, in Python.
- **`mpsk.mpsk_soft_demap`** — per-bit log-likelihood ratios.
- Receiver telemetry reaches the front-end AGC; the receiver publishes the
    recovered **symbol**, not just loop state.
- **`track.BpskReceiver`** — the receiver asked for in the units a capture
    actually comes in.

**Measurement you can trust.**

- **14 objects are certified** under the object-validation campaign, each
    with a claim inventory mapping its header's prose onto tests that are
    proven by sabotage — `MpskReceiver`, `LoopFilter`, `CarrierNda`,
    `RateSync`, the M-PSK constellation, `conv` and `ccsds_tm` among them.
- **75 assertions across 20 C tests could not fail**, and now can. The
    `dp_*_test.h` family — the assertion foundation 97 C test files sit on —
    has self-tests of its own.
- **The C tests' randomness has one home**, `native/tests/dp_rng_test.h`,
    and a gate that keeps it there.
- **Every benchmark records a measurement**, enforced by
    `scripts/check_bench_coverage.py`; several were writing results under a
    name nothing read.
- **`FrameMeter`**, `lock_time`, and a receiver instrument that runs one
    harness across every receiver.

**Faster, and honest about it.**

- The Python example gate runs in parallel in two passes: **376.6 s →
    20.6 s**.
- Every Linux CI job runs inside a **baked toolchain image**; the C core
    compiles through **ccache**.
- **`make coverage` stopped discarding pytest's exit code** — twice, in two
    different places — and the coverage run no longer borrows the
    developer's machine.
- **`make test-tsan`** runs the threaded C tests under ThreadSanitizer,
    where a data race in the CCSDS Reed-Solomon table derivation was found.

**Packaging and consumers.**

- `libdoppler.a` declares `-lpthread`; the C tarball could previously be
    built **empty** and would have shipped.
- Two installed public headers declared an API the library does not export,
    and an installed header can no longer do so.
- `examples/downstream-jm` ships as a **starter tarball with doppler
    inside**.

**Under the hood.** The just-makeit pin moved 0.51.0 → 0.63.3 across eleven bumps and now has one source of truth. A changelog entry is a **file**
now (`changelog.d/`), because a shared line does not scale past a few open
PRs.

### Breaking

- **BREAKING: the Costas arm filter is gone, and `nda_tap = "lo_arm"` with
    it.** A discriminator tap does not need an arm filter where the chain
    already filters ahead of it — and the one place the receiver added its own,
    a free-running half-symbol boxcar on the raw post-LO sample, was also the
    only tap whose node had no filtering ahead of it at all. `mf_in` is that
    tap done properly: DEC's filters have already band-limited it and the AGC
    has already levelled it. (The matched filter may itself BE a boxcar — I&D
    and CIC both are — and that is untouched; this is about the extra arm.)

    It did not work anyway. Measured with `native/validation/rx_nda_tap.c`,
    fraction of a known carrier offset removed: 0.1352 at sps=8, 0.0002 at
    sps=200, 0.0000 at sps=10000, unchanged with the modulation removed or the
    timing loop disabled — the only tap of the four that failed, and it failed
    everywhere. The published `0.090*Rs` pull-in came from a hand-tuned
    `bn_carrier` absorbing gh-765 at sps=8, which stops working as the ratio
    grows. Closes gh-768.

- **BREAKING: `warmup_syms` is gone from both receivers.** The handover now
    rests on its lock detector and nothing else. That detector already carries
    both hysteresis axes — a split declare/drop threshold pair and a
    consecutive-symbol count each way — so the warmup counter was a second,
    cruder de-chatterer for the same job.

    It was guarding a condition gh-657 made impossible. Its comment claimed the
    pre-lock lock EMA reaches 0.9-1.7 against 0.62 settled; `carrier_nda_disc()`
    now divides by `|z|` at the FIRST squaring, so every later value is a unit
    vector and `lock` is an EMA of something bounded in [-1, 1] — it cannot
    exceed 1. Measured on the shipped receiver (QPSK, sps=8, 20 dB, 5 seeds):
    pre-lock peak 0.900-0.916, settled 0.947-0.968. BOTH numbers in that
    comment described the pre-normalisation detector. Note `acq_to_track`
    already defaults to 0, so the default receiver never consulted it.

- **BREAKING: `nda_tap = "mf_all"` is now `"mf_out"`.** "all" read as *both*
    sides of the matched filter, which is the one thing it never meant — the
    tap is the MFR's output. Paired with `mf_in` it is now a symmetric
    input/output naming, and `strobe` keeps its own name because that name is
    standard. C enum `MPSK_RX_NDA_TAP_MF_OUT`. This is the only shipped
    spelling that changes; callers passing `nda_tap="mf_all"` get the usual
    `ValueError` listing the valid names.

- **Frame flags on a waveform that cannot carry one now REFUSE instead of
    being silently ignored.** `--type bpsk|qpsk|pn` (and the Python
    equivalents) source their symbols from the PN LFSR, so there is no length
    to bound a payload; they now exit 2 with a message naming the replacement,
    where before they exited 0 and produced an unframed waveform. `--type bits`
    with frame flags but no `--bits` refuses for the same reason. This is the
    defect being fixed, not a new restriction — see below.

- **`DsssReceiver` refuses `sps < 2` instead of aborting the process.**
    `DsssReceiver(sps=1)` used to **SIGABRT the interpreter** — exit 134, no
    exception, no traceback, nothing on stderr. `sps` is a documented
    constructor parameter and `1` is not obviously out of range from the
    Python face, so a caller had no way to see it coming and no way to
    recover.

    The guard validated `sps < 1`, so `sps = 1` reached
    `mpsk_receiver_create()`, which rejects it (that constructor requires
    `sps >= m_out`, and the smallest legal `m_out` is 2). Its argument-error
    NULL then went through `dp_xnn()`, an **abort-on-OOM** helper — and an
    argument error is not an allocation failure.

    Two is not arbitrary: below it there is no receiver to build, which is
    the range `mpsk_rx_derive_m_out()` already documented. The guard and the
    guarantee simply did not meet. The object now also declares
    `create_error = "ValueError"`, so the refusal names the parameter set
    instead of surfacing as a blanket `MemoryError`.

    **Behaviour change:** `sps` of 0 or 1 now raises `ValueError` where it
    previously killed the process. Nothing in the tree passed either.

- **`HalfbandDecimator_create()` takes `(h, h_len)`, not `(num_taps, h)`** —
    the array first, matching its own manifest and every other array
    constructor in the tree. **C callers must swap the two arguments.** The
    types differ, so a call that does not swap fails to compile rather than
    misbehaving; the Python face is unchanged.

    `objects/HalfbandDecimator.toml` declared **one** constructor parameter
    while the C took **two**, in the opposite order. Nothing broke today,
    because jm never rewrites that prototype — but the manifest is what
    `jm regenerate` and every future reconciliation read, so the divergence
    was a trap for whoever regenerated the object next, and it hid the real
    signature from the generated stub.

    The issue proposed declaring both parameters instead, as the smaller
    change. Rendered, that turns out worse: jm sorts the array first and
    makes the count optional, giving
    `__init__(self, h, num_taps: int = ...)` — a redundant Python argument
    duplicating `len(h)`, and the C order still not what the manifest says.
    So the fix converges the C on jm's shape, which is also what
    `fir`, `corr`, `corr2d`, `detector` and `detector2d` already do; the
    `hbdecim` family were the only three `(len, ptr)` constructors in the
    tree, and the one-line adapter now does the swap.

    Not gated here, deliberately: verifying it needs jm's manifest→C type
    renderer, and reimplementing that downstream is the duplication jm
    exists to remove. `jm status --check` cannot see it — every file jm owns
    was self-consistent, and the only disagreeing file was the sacred
    `_core.c`. Filed as just-makeit#1076, which asks jm to verify the
    `_core.h` declaration it already injects.

- **The MPSK acquisition/tracking handover is gone, and so is
    `ContinuousMpskReceiver`.** `acq_to_track`, the `tracking` property, the
    `<prefix>.tracking` telemetry probe, `MpskReceiver.configure_lock()` and
    the whole decision-directed arm are removed
    ([#877](https://github.com/doppler-dsp/doppler/issues/877)). One M-th-power
    NDA discriminator steers the LO from the first strobe to the last.

    **It was measured before it was deleted**, because `acq_to_track` had to be
    shown *not* inert rather than assumed so. The same record through
    `MpskReceiver` twice, at operating points where the meter is not saturated
    (SER 1.5e-2 to 7.1e-2, BPSK/QPSK/8PSK):

    | axis                          | result                                |
    | ----------------------------- | ------------------------------------- |
    | recovered symbols that differ | 19764–19928 of 19998 (~99%)           |
    | largest difference            | 0.30–0.48 (unit-radius constellation) |
    | SER over 10 engaged cells     | mean ratio **0.9999**, t = 0.28       |
    | cells where the handover won  | 6 of 10                               |

    So it perturbed essentially the whole sample path and moved the decisions
    by scatter with no sign. At the one operating point where it shipped
    enabled — 8PSK in `mpsk_receiver_ber.c`, on the ±π/8 decision-margin
    argument — the validator already carried its own number: turning it off
    costs **0.09 dB** (0.44 → 0.53 dB of loss) against a settling window it
    more than doubled, since the handover fired around symbol 8500.

    `ContinuousMpskReceiver` existed only to pin the handover off. With nothing
    left to pin it was a duplicate of `MpskReceiver`, which is now the
    continuous receiver under its own name. `configure_lock()` went for a
    second reason: it retuned only the handover's detector and never the lock
    indicator's, so it desynced the two detectors it appeared to configure.

- **`ber_settle_from()` loses its `handover` argument** (C and Python). Its
    only producer was `acq_to_track`, so it could now only ever be passed -1.
    `dp_ber_settle()` in the test harness loses the matching `tracking` array.

- **`MPSK_RX_LOOPS_STATE_VERSION` 6 → 7.** The blob drops the handover
    detector's `cnt`/`locked` pair and the `tracking` flag bit; `have_prev_idx`
    moves to bit 0. A v6 blob is rejected at the envelope, not reinterpreted.
    `MPSK_RX_HANDOVER_*` are renamed `MPSK_RX_LOCK_*`, since what they size is
    the surviving lock indicator.

- **`Lo::steps_ctrl` read twice its buffer from safe Rust**
    ([#911](https://github.com/doppler-dsp/doppler/issues/911)). The extern
    declared the control port `*const f32`; the C takes `const double *`, so
    C walked `8 * ctrl_len` bytes off a `4 * ctrl_len`-byte allocation, with
    the values reinterpreted regardless. **Breaking**: `steps_ctrl` now takes
    `&[f64]`. `examples/nco_demo.rs` was calling it with a *non-zero*
    `Vec<f32>`, so it over-read on every run.

    The existing test passed throughout because it used `vec![0.0_f32; 4]` —
    all-zero is the one input whose bit pattern is identical at both widths.
    The new test uses a real deviation and asserts the measured phase advance;
    against the original declarations it reports the deviation as **entirely
    ignored** (0.0999… where 0.15 was expected), since `0.05_f32` read as
    `f64` is a denormal.

    `ffi/rust/` is the one binding jm does not generate, so nothing compared
    it to the C — and this had happened once before (`cce1792f`). New
    `scripts/check_rust_abi.py` (`make lint-rust-abi`, and a pre-commit hook)
    checks every `extern "C"` declaration against the header: the name exists,
    the arity matches, and each parameter's element width agrees. 45
    declarations check clean; three sabotages caught.

### Added

- **Every benchmark in the tree now records a measurement — the
    `HOLLOW_ALLOW` ratchet is EMPTY**
    ([#891](https://github.com/doppler-dsp/doppler/issues/891)). The last
    ten off it were the library's hottest kernels: `fir`, `fft`, `nco`,
    `ddc`, `ddcr`, `corr`, `fft2d`, `detector`, `detector2d` and
    `hbdecim_q15`. Each had a `main()` with a `TODO` and no recording call,
    so it built, ran, exited 0 and wrote `"benchmarks": []` — present in
    every build and absent from every snapshot. `buffer` came off the
    sibling `ALLOW` list the same way: its waiver said the ring's push/pop
    "is a real hot path and should be measured", which is an argument for
    writing the file rather than for holding the waiver.

    These ten had survived because their kernels already reach
    `docs/benchmarks.md` through a Python benchmark, so the empty C file
    cost a row rather than the measurement. That row is the face where
    per-call overhead is not folded into the number, and it is where the
    questions a signature cannot answer live. Each file names one and
    answers it:

    |                           | the question the API does not answer                                     |
    | ------------------------- | ------------------------------------------------------------------------ |
    | `fir`                     | a complex tap against a real one, same coefficients, three lengths       |
    | `nco`                     | what `scaled` / `ovf` / `ctrl` each cost over the bare phase accumulator |
    | `fft`                     | whether N log N holds, and what an ADC's `ci16`/`ci8` format costs       |
    | `fft2d`                   | whether a 2-D transform is priced by its bin count or its SHAPE          |
    | `corr`                    | the dump call against the accumulate call — what a dwell buys            |
    | `ddc` / `ddcr`            | what the one-sample form a closed loop must use costs over a block call  |
    | `detector` / `detector2d` | whether the caller's chunk size, and emitting a detection, are free      |
    | `hbdecim_q15`             | whether the SIMD tap padding is what you pay for                         |
    | `buffer`                  | whether a batch straddling the wrap costs more than one that does not    |

    Three answers are worth stating here. **A 2-D FFT is not priced by its
    bin count**: at a constant 65536 bins, every elongated shape costs
    ~1.6–1.8x the square one, and the penalty is symmetric in the two
    elongations, so it is not the column stride on its own. **The
    per-sample `execute_ctrl_push` form — the only one a carrier or timing
    loop can use, since it computes each correction from outputs already
    emitted — costs 4.5x the block call** on `ddc`, while steering itself
    (`execute_ctrl` against `execute`) is free at 1.02x; so a tracking loop
    pays for its granularity, not for its steering. And **two answers are
    "free"**: the detector's chunk size (1.01x across a 256:1 range) and
    the ring buffer's wrap position (1.01x), the latter being the double
    mapping's whole design claim, now pinned by a number rather than
    asserted in a header.

    **`wfm_compose` came off the sibling `ALLOW` list on the same
    argument**, and it closes #891: its waiver said the composer's segment
    assembly is "benchmarked indirectly through `wfm_synth` only", which
    measures the engine underneath and says nothing about the layer on top
    — the header is explicit that a one-segment spec is byte-identical to
    calling `synth` directly, so everything the composer adds happens at a
    boundary a one-segment spec never crosses. Holding the sample count
    fixed and varying only how many boundaries produce it prices one at
    **~30–40 ns, or 64–86 samples-equivalent**, agreeing across three
    independent segment lengths. A burst schedule with segments shorter
    than that spends more on starting waveforms than on emitting them.
    `ALLOW` is now one entry: `hbdecim`, the one case where "measured
    elsewhere" is true rather than a euphemism.

    Two predictions written into the files were refuted by their own
    measurements and the files were corrected rather than the readings —
    `hbdecim_q15`'s cost is not the padded SIMD width (34 and 64 taps pad
    identically and differ by 1.4x), and `fft`'s fixed-point multiple rises
    with N rather than falling, because per-call overhead amortises faster
    than the convert does.

- **The seven function-only modules are measured, in C, for the first
    time** ([#891](https://github.com/doppler-dsp/doppler/issues/891)).
    `arith`, `detection`, `filter`, `measure`, `resample`, `spectral` and
    `wfm` carry their surface as free functions with no object of their
    own, so `jm bench` never saw them and neither did anything else. Seven
    new `native/benchmarks/bench_<module>_core.c`, each answering a
    question rather than printing a rate, and each settling the clock once
    before interleaving its configurations — the shape
    `bench_viterbi_core.c` established, now shared through a new
    doppler-owned `native/benchmarks/dp_bench.h` rather than copied.

    Three of them found something the code does not say:

    - **`measure_min_samples` costs ~25 µs**, 10 000x its neighbours in the
        same "planning helper" family and more than the FFT it is sizing —
        it designs a window to hit a dynamic-range target rather than doing
        arithmetic. Invisible once per capture; it *is* the sweep when a
        planner calls it per candidate.
    - **`dp_coherent_freq` is priced by N's factorisation, not N's size**:
        2.64x more at N = 30030 than at N = 4096, from the walk for a J
        coprime with N. A capture length chosen for the FFT's sake is
        choosing this too.
    - **The Q8 shift kernels are slower than their Q15 twins** — `shr_q8`
        3.02x, `shl_q8` 1.36x — while `add`/`sub`/`mul`/`dot` all go the
        right way at 0.50x/0.50x/0.64x/0.20x. Same source transliterated
        between widths, so it is a codegen result rather than an
        algorithmic one. Filed as
        [#905](https://github.com/doppler-dsp/doppler/issues/905), not
        fixed here: a behaviour change does not belong inside a measurement
        change. The Python face for these functions is
        [#906](https://github.com/doppler-dsp/doppler/issues/906).

    `detection` and `wfm` leave `check_bench_coverage.py`'s ALLOW ratchet.
    That gate's docstring had claimed since it was written that it fails on
    a stale entry, and for ALLOW it did not — the check existed only for
    `HOLLOW_ALLOW`, so the list was free to rot in the one direction that
    matters. It now fails on an ALLOW entry whose component has started
    measuring, and on one naming a component that no longer exists. Rule 3
    also learned `dp_bench_record`, and re-derives from `dp_bench.h` on
    every run that the helper really does call `jm_bench_add` — an alias
    that stopped recording would otherwise hand every caller a way to look
    measured while writing an empty array, which is the exact failure the
    file exists to catch, one level down.

- **`examples/downstream-jm` ships as a starter tarball with doppler inside
    it**, published with every release as
    **`doppler-starter-<ver>-<plat>.tar.gz`** for all three platforms.
    `make package-starter-tarball VERSION=x.y.z` builds the project from `git archive` (never the
    working tree, which carries `build/` and a `compile_commands.json` full of
    one machine's absolute paths) plus the C SDK installed into
    `third_party/doppler/`. Extract it and build — no doppler checkout, no
    install step, no `CMAKE_PREFIX_PATH`, no network. `find_package(doppler)`
    resolves against the bundle because `CMakeLists.txt` prepends it, above
    the manifest-owned section, and an explicit `-Ddoppler_DIR` still wins.

    Its README loses the "from source, or against a doppler build tree"
    section and the "For doppler developers" framing that went with it — that
    was doppler's CI mechanism documented in a page written for someone
    consuming doppler.

    `make test-starter-tarball` is the gate, and it is deliberately not a
    duplicate of `test-example-downstream`: it unpacks the shipped archive
    **outside this repo** and runs the README's own commands, so a packaging
    mistake fails there and nowhere else.

    It is built in the same release job as the SDK it bundles, so doppler is
    compiled once rather than twice, and it rides the existing
    `dist/doppler-*-<plat>.tar.gz` upload glob. The whole recipe was run in
    the manylinux container before being wired in, which settled three things
    reading could not: `git archive` needs
    `git config --global --add safe.directory /project` or it prints
    "detected dubious ownership" and writes **nothing**; `git` must *not* be
    added to the `dnf` list, because the image already ships 2.55.0 at
    `/usr/local/bin/git` and installing it adds a second, older one at
    `/usr/bin/git` that PATH never reaches; and the artifact the container
    produces extracts, configures with no flags, builds and passes its test. The asset is named
    `doppler-starter-` and not `iqtools-` because on a release page beside
    `doppler-<ver>-<plat>.tar.gz` it has to say what it is; the directory
    inside stays `iqtools`, which is the project's own name and what you
    rename first.

- **The starter provisions its Python half in-tree, from its own manifest.**
    `make setup` creates `.venv/` and runs `pip install -e ".[test]"`. No
    root, nothing written outside the extracted directory, and no second copy
    of the dependency list: NumPy is already declared in `[build-system]   requires`, `[project] dependencies` and `find_package(Python3 ... NumPy)`,
    and the new `[test]` extra carries `pytest` and `doppler-dsp`.

    Measured, and contrary to the obvious assumption: the only thing a stock
    Python is missing here is **NumPy's headers** — `Python.h` ships with the
    interpreter, so no system `-dev` package is in the critical path.
    `jbx install-deps` still installs the `bootstrap.toml` names, but it is
    `sudo <package-manager>` by construction with no notion of a prefix, so it
    is for CI images rather than for someone who just unpacked a tarball.

- **`track.BpskReceiver` — the receiver asked for in the units a capture comes
    with.** Two required arguments, `sample_rate_hz` and `symbol_rate_hz`,
    against `MpskReceiver`'s seventeen parameters. `m` is carried by the class
    name; `sps` is `fs / Rs`, a ratio the library computes for its own use in
    planning a cascade, so a caller never states it; and `carrier_freq_hz`
    defaults to 0 for complex baseband. Nothing on the signature is normalised
    to anything ([#831](https://github.com/doppler-dsp/doppler/issues/831)).

    It is a **view** over the same core, not a second type — the rule being
    that a difference in constructor is a flavor — and a test asserts it
    produces bit-identical symbols to the equivalent `MpskReceiver`, because
    one core is the whole claim. `MpskReceiver` is unchanged.

    `m_out` deriving rather than being pinned is not cosmetic: `m_out=4`
    against the default I&D pulse is measured **3.11 dB** off the coherent
    bound where the derived 8 is 0.41 dB off. A parameter nobody needed was a
    way to lose most of a link's margin quietly, and the telemetry demo had
    been doing exactly that.

- **The receiver publishes the recovered SYMBOL, not just its loop state.**
    `sym.i` and `sym.q` join the fourteen existing probes
    ([#846](https://github.com/doppler-dsp/doppler/issues/846)). Every probe
    before them was an internal, so a filed capture held the scene around a
    number and not the thing the number is computed from — no constellation,
    and no error rate recomputable from the evidence. A telemetry record
    carries one `float`, so a complex value cannot be one probe; the pair
    lands on the sample index the format already stamps, and
    `sym.i + 1j*sym.q` reconstructs `steps()` output exactly.

- **`conv` is certified — the first component with no Python face to be.**
    The campaign's evidence layer is a Python validator, and `conv` has no
    binding: a binding built only to be measured is one nobody calls, and the
    campaign would then be certifying an artifact of its own process. So the
    substitution is **C measures, Python renders**.
    `native/validation/conv_certify.c` runs the sweeps — bits from `pn`,
    symbols from `mpsk_map`, noise from `awgn`, soft decisions from
    `mpsk_soft_demap` — and emits CSV;
    `src/doppler/tests/validation/conv/validate.py` parses it and
    characterises, reviews and asserts through the same `Report` every other
    object uses, so the format cannot drift between the two kinds. Nothing in
    the C decides whether a number is acceptable; nothing in the Python
    computes one. `docs/dev/contributing/validation.md` and
    `docs/dev/contributing/adding-algorithms.md` carry the track, both gates found it by
    glob with no registration, and the validation log reads **12 objects
    certified**.

    **What the report says a caller should do.** Ship traceback depth 60
    rather than the textbook `5*K = 35`, which sits 17 % above the achievable
    floor where 60 is within 1.2 %. Feed the decoder soft decisions or do not
    code at all — below Eb/N0 ~3.5 dB a hard-decision Viterbi is **worse than
    an uncoded link**, because the rate costs 3.01 dB of Eb that a two-level
    input does not buy back. Size a node-sync window for the job it has: the
    phase decision holds at 250 bits, while the in-sync statistic only becomes
    a channel estimate — to within 25 % of the delivered symbol error rate —
    at a thousand bits or more.

    **And one design number is corrected.** `docs/design/viterbi.md` §4
    quoted `5*K` at 33 % above the floor (0.04178 against 0.03137), from a
    prototype that was explicitly throwaway and uncommitted. Measured over
    four times the bits at the same Eb/N0: 17 %, with every level ~30 %
    higher — about what a fraction of a dB of Es/N0 convention is worth on a
    curve that steep. The page now carries the table the gated harness
    produces and says the prototype's is superseded; the decision it drove is
    unchanged.

- **`conv` — convolutional codes, both directions, on one code description.**
    A rate-1/n code is four numbers: a constraint length, an output count, a
    generator polynomial per output, and which outputs are inverted.
    `conv_code_t` holds them, `conv_outputs()` is the only place in the tree
    that says what the family emits, and **both** `conv_encode` and the new
    `viterbi_decode` read it. An encoder that computed the outputs and a
    decoder that computed them again would be two implementations of one
    primitive — and the inversion is exactly the detail that drifts between
    them, invisibly, because a matched pair decodes itself perfectly and
    interoperates with nothing.

    **CCSDS is now a configuration, not an implementation.** `CCSDS_TM_CONV`
    is `{k=7, n=2, {0171, 0133}, invert G2}` and `ccsds_tm`'s bespoke encoder is
    gone; the frame assembler encodes through `conv_encode`. Point the same
    objects at the deep-space rate-1/6 code, a K = 9 experiment, or anything a
    caller brings.

    The decoder is streaming and maximum-likelihood: 2^(k-1) states, branch
    metrics computed once per step from the `2^n` distinct output words rather
    than per state, path metrics renormalised so a stream cannot overflow them,
    and a traceback ring. It consumes the LLR convention `mpsk_soft_demap`
    produces, so it agrees with `mpsk_demap` on hard decisions by construction
    — and since scaling every branch metric cannot move the winning path, a
    caller with no SNR estimate may pass unscaled values. **Depth 60** is the
    measured default for CCSDS's code: `5·K = 35`, the textbook number, sits
    33 % above the achievable BER (`docs/design/viterbi.md` §4).

    **The external truth is the impulse response**, which is what a generator
    polynomial means — drive a 1 followed by zeros and output `j` traces
    `poly[j]`, inverted where the code says so. That is checkable for every
    configuration rather than only the familiar one, and it is not a round
    trip. Seven codes from K = 3 to K = 9 and rate 1/1 to 1/3 decode exactly;
    sabotage-proven four ways, each in the section that should catch it:
    reversing the state convention and dropping the inversion both redden the
    impulse response, while breaking the butterfly's input bit or shortening
    the traceback by one redden the decode.

    Also fixed on arrival, the gh-747 class: `conv_core` reached no library, so
    a C consumer could include the installed header and link none of its 11
    out-of-line symbols. jm 0.62.0's wiring check is what named it.

- **`dp_mf_test.h` and `dp_dsss_test.h` have self-tests — and doppler#689 is
    a gate instead of a paragraph.** These were the last two entries on
    `docs/design/rx-test.md` §5.2's list; the section is now closed, and it
    records what writing the tests found rather than only that they exist.

    `dp_dsss_test.h` carries a **known defect in its own docstring**: the
    noise line scales `dp_cgauss` by `sigma/sqrt(2)`, the factor for the other
    complex-Gaussian convention, so every capture is **3.01 dB quieter than it
    claims**. It is deliberately unfixed — removing the `/sqrt(2)` makes an
    async BER sweep go non-monotonic (6 dB decodes, 8 dB fails, 10 dB
    decodes), which is acquisition succeeding or failing per point rather than
    a threshold, so correcting it is a receiver investigation. Until now the
    defect was held in place by prose alone.

    The self-test makes it a **characterization**: the 3.01 dB is measured and
    asserted, reproducing the header's own `E|n|^2 = 0.4996` against a target
    of 1.0. So the magnitude is a fact rather than a recollection, the level
    cannot drift further unnoticed, and a one-character "fix" turns the test
    **red on purpose** — forcing the investigation #689 asks for instead of
    quietly re-tuning two BER sweeps that have been passing on 3 dB of noise
    they never had. Proven: removing the `/sqrt(2)` reads −0.02 dB against the
    pinned −3.01.

    `dp_mf_test.h`'s `mf_evm_db()` takes a **min over strobe alignment** — the
    shape `dp_ber_test.h` calls "the historic footgun", legitimate here
    because the loop is open and the strobe phase is genuinely arbitrary, but
    exposed to the false-PASS that footgun names. So it is measured rather
    than argued: a stream carrying an unrelated sequence must read badly at
    every alignment the search tries, or every EVM this header has reported is
    a search result rather than a measurement. Also pinned: the fitted complex
    gain normalises out rotation *and* scale (an 80 dB swing when removed),
    which is what lets one number score both the complex and the real chain,
    and both traps the header documents get an assertion each.

- **`dp_sym_test.h`, `dp_tx_test.h` and `dp_state_test.h` have self-tests, and
    the state macro gains the FIDELITY half it was missing.**

    **`DP_STATE_ROUNDTRIP_TEST` is 12 lines and 31 test files call it** — the
    only evidence most serializable objects have that their state interface
    works. It asserted what `set_state` **returns** (`DP_OK` on a good blob,
    `DP_ERR_INVALID` on a clobbered one) and never that the restored object
    **carries** the state, so a `set_state` that validated the envelope,
    returned `DP_OK` and restored nothing passed at all 31 sites. The
    project's claim is bit-exact resume; every object meeting it did so in its
    own test, by hand, while the shared macro a new object reaches for first
    proved only the envelope.

    The generic fix is three lines and needs no knowledge of the object:
    restore into `b`, re-serialize `b`, compare to `a`'s blob. The state
    standard is what makes it well defined — a blob carries only RUNNING
    fields, config is restored by `create()`, and `b` must be a fresh object
    of the same config. **No object was cheating**: everything passes with it
    in, and all 32 call sites pass distinct `a`/`b`, so it is exercised rather
    than trivially satisfied.

    Finding it needed a fake object, because the macro pastes a PREFIX and so
    cannot be exercised against a real one without also testing that object.
    `test_dp_state.c` builds one over the real `dp_state.h` envelope whose
    `set_state` switches between correct and three broken implementations.

    **`dp_sym_test.h`** is thin, so what is load-bearing is the numbers its
    docstrings state — other files write fixed thresholds against them. Those
    are pinned at the source, and the half a closed form cannot establish is
    measured: a stream at uniformly random phase lands on the scatter floor
    and **passes** the `< -12.0 dB` assertion the header records as live in
    `test_mpsk_receiver_r_core.c` until 2026-07-27, while the identical stream
    fails it at BPSK. It also found the short-stream floor is **39, not the 20
    the guard names** — both back-half forms score `ceil(n_syms/2)`, which the
    layer beneath rejects below 20. Docstrings corrected.

    **`dp_tx_test.h`** is the file `check_stimulus_sources.py` structurally
    cannot police (§5.4): it *is* the test layer's stimulus, so the gate has
    nowhere to point. The conventions it would have checked are asserted
    instead. `DP_TX_RC` makes the central one exact — a full raised cosine is
    Nyquist, so at symbol centres the sample **is** `amp * symbol`, and "amp
    is the SYMBOL amplitude, never a peak" stops being a comment. Proven by
    sabotage: reading `tau` as samples, and dropping the `rate` scaling from
    the lead-in, each take exactly the one assertion written for them red.

- **`dp_test.h` has a self-test — the assertion foundation 97 C test files
    include, and nothing tested it.** That is the worst place in the tree for
    an untested thing, because the failure mode is not a red suite but a
    **green** one: a `DP_CHECK` that stops recording failures turns all 97
    files into programs that run to completion and report success, and `ctest`
    says 100%. The header replaced 90 hand-rolled `CHECK` macros in six
    variants — one with its condition inverted, twenty whose failure gate had
    drifted so that 75 checks printed FAIL and their tests still exited 0 —
    and nothing had been watching the replacement.

    `test_dp_test.c` resolves the circularity by observing `dp_test.h`'s own
    counters rather than its exit status, so it can assert that a check FAILS
    without failing itself, and it captures stderr so a deliberate failure
    never puts a fake `FAIL` line into a passing test's log. Capturing rather
    than discarding also pins the DIAGNOSTIC — file, line, stringified
    condition, and both values plus the tolerance for `DP_CHECK_NEAR`.

    `DP_TEST_END` returns, so its three exit paths cannot be tested
    in-process; `test_dp_test_end.c` gives each one a process and CTest
    asserts the status (`WILL_FAIL` for two). The path that matters is
    **`nothing`**: the zero-assertion floor is the only guard between this
    suite and a test whose body never ran reading as a pass forever, and
    nothing had ever run a zero-assertion program to confirm the floor fires.

    Writing it found a documentation defect on the first run. `dp_cnearf` /
    `dp_cnear` are component-wise and their comment called that "the stricter
    of the two" — it is the **looser**: the component test accepts a square of
    side `2*tol`, the magnitude test a disc of radius `tol` that sits strictly
    inside it, so a diagonal error of `(0.4, 0.4)` passes at `tol = 0.5`
    despite a magnitude of 0.566. That is the shape of every carrier-phase
    error the suite measures, so the semantics are now asserted rather than
    described.

- **The CCSDS rate-1/2 K=7 convolutional code** (131.0-B-3 section 3.3), with
    the symbol inversion on the G2 output path that 3.3.1(5) requires. That
    inversion is invisible to a round trip — a matched Viterbi inverts whatever
    it was handed — so it is pinned against the impulse response, where C1 must
    trace `G1 = 1111001` and C2 the complement of `G2 = 1011011`, and against
    an all-zero input, which must emit C1 all zeros and C2 all ones.

- **The frame assembler, and the ASM** (131.0-B-3 section 9) — the element the
    other four exist for. `ccsds_tm_frame_encode` takes a Transfer Frame as packed
    octets and returns unpacked channel symbols, which makes it the one place
    the packed/unpacked boundary between `ccsds_tm_rs.h` and `ccsds_tm.h` is
    crossed rather than a convention each kernel assumes about its caller.

    What it adds over running the four kernels in order is that **the stages do
    not all cover the same bits.** The marker enters third and the stages
    disagree about whether they reach over it: the outer code must not
    (9.5.1), the randomiser must not (10.3.4 note 1), and the inner code must
    (3.2.1, 9.2.1.4) — 9.2.1.5 states two of the three in a single sentence.
    So `ccsds_tm_frame_layout` reports a **span per stage** rather than a stage
    order: an order is the representation that cannot express this, being
    right at three boundaries and wrong at the fourth, in the direction that
    still encodes, still decodes against a receiver of one's own construction,
    and syncs to nothing.

    All three rows are asserted against something outside the assembler: the
    marker byte for byte as figure 9-1 prints it, the randomiser's published
    40-bit prefix positioned *after* it — which fails in both halves if the
    randomiser reached back, since the marker would come out XORed with
    `FF 48 0E C0` and the block would begin at sequence bit 32 — and
    `ccsds_tm_rs_codeword_ok`, which needs no decoder and refuses the 255 symbols
    taken from the marker rather than from behind it. Nine sabotages were run
    against the finished test, one per claim.

    Virtual fill (4.4.2's shortened codeblock) is not implemented, so a frame
    that is not exactly `223 * I` octets is refused rather than padded (#813).

    `ccsds_tm_core` now reaches `libdoppler.so` and `libdoppler.a`. It never
    had, for as long as the component has existed: `native/inc/ccsds_tm/*.h`
    are installed
    headers whose every function is out-of-line, and `nm` found 13 defined
    symbols in the core against zero occurrences of the component's prefix in
    the library — so
    a C consumer could include the header and link none of it. Python was
    unaffected the whole time, because the extension links each core directly,
    which is exactly why nobody noticed. jm 0.62.0's wiring check is what
    named it.

- **Reed-Solomon symbol interleaving** (131.0-B-3 sections 4.3.5 and 4.4.1),
    depths `I = 1, 2, 3, 4, 5, 8`, plus `ccsds_tm_rs_codeword_ok` — a syndrome check
    that says whether 255 symbols form a codeword without decoding them. The
    interleaver is what makes the outer code burst-tolerant: a contiguous burst
    of `B` symbols lands as `ceil(B/I)` errors per codeword, so depth buys a
    `I`-fold longer correctable burst at no cost in rate. Tested in both
    directions — a 40-symbol burst stays inside `E=16` at depth 5 and exceeds
    it at depth 1 — because an interleaver that merely copied would pass the
    first half alone.

- **`ccsds_tm` — CCSDS TM channel coding, starting with the pseudo-randomiser.**
    doppler encoded nothing before this: no convolutional code, no
    Reed-Solomon, no interleaver and no randomiser anywhere in the tree, which
    is the gap between a test-vector generator and a link waveform. The first
    element of the CCSDS TM slice (131.0-B-3, section 10) lands as its own
    `c_dep` rather than a library inside `wfmcompose`, because both ends want
    it — wfmgen encodes and `frame`/`ber_meter` will decode, and a receiver
    should not carry the waveform-composition chain on its link line for the
    sake of a randomiser.

    `ccsds_tm_randomise` is its own inverse, so both ends call one function.
    The published 40-bit prefix is what pins it: a first cut transcribed the
    taps from the polynomial's exponents rather than from the recurrence they
    stand for, drove the register to the all-zero fixed point, and **passed
    both a round trip and both period checks** — a dead sequence repeats with
    period 255 and matches no earlier one. The period test now also asserts
    the 128-ones balance a maximal generator must have, which no degenerate
    sequence does.

- **CCSDS Reed-Solomon (255,223)**, the E=16 outer code (131.0-B-3 section
    4.3), with the three conventions a textbook implementation gets wrong and
    a round trip cannot catch: the field is `x^8 + x^7 + x^2 + x + 1` (4.3.3)
    rather than the habitual one, the generator's roots are powers of `a^11`
    rather than consecutive powers of `a` (4.3.4), and symbols travel in the
    **dual (Berlekamp) basis** (4.3.9.1). Verified against Annex G, which
    publishes all 33 coefficients of `g(x)` — reproducing them exercises the
    field and the root stride together — and against 4.3.9.3's two basis
    matrices, required to invert each other across all 256 symbols so a single
    mis-transcribed bit cannot pass.

- **The CCSDS receive chain, and a link demo that runs it end to end.**
    `ccsds_tm_frame_decode` is the mirror of `ccsds_tm_frame_encode` — it reads the same
    `ccsds_tm_frame_cfg_t` and the same span table, so the two directions cannot
    come to disagree about which stage covered what, which is the failure
    `ccsds_tm_frame.h` opens by describing. It skips the marker, re-applies the
    involutive randomiser over the block span, packs back to octets MSB-first,
    and checks each interleaved codeword; `ccsds_tm_frame_rx_t` reports what it
    found. `ccsds_tm_asm_find` correlates for the marker at every offset in
    **both polarities**, because a BPSK carrier recovered through a 180-degree
    ambiguity delivers the stream complemented and the marker is the only part
    of a CADU that can say so — the randomiser deliberately does not cover it.

    **The outer code is a check, not a correction.** `ccsds_tm_rs_codeword_ok` is a
    syndrome test; Berlekamp-Massey, Chien and Forney are still
    `docs/design/fec-receive.md` §7 step 2. So `rs_ok < rs_codewords` means the
    returned frame is wrong in a way the function knows about, which is why it
    is reported rather than folded into the return value.

    **The inner decode is deliberately outside it.** A Viterbi is streaming and
    emits decision `i` only after `depth` further bits, and the marker that
    says where a CADU *starts* is only readable once the inner code is undone —
    so a function taking channel symbols would have to own a decoder, a search
    window and a buffer. That is a streaming receiver object; this is the pure
    per-frame chain it will call. It matches the encoder, where `conv_enc_t`
    belongs to the caller for the same reason: the inner code is continuous and
    the frame is not.

    `examples/c/ccsds_link_demo.c` runs the whole thing — R-S, randomiser, ASM,
    K=7 rate-1/2, BPSK, AWGN, soft demap, Viterbi, sync, and back — and prints
    an Es/N0 sweep plus the recovered text. It measures rather than asserts:
    the channel's own symbol error rate comes out at **7.90 % at 0 dB against
    Q(sqrt 2) = 7.86 %**, the inner code takes that to **zero bit errors in
    40092 from 2 dB up**, and the outer code reports 7 of 10 codewords surviving
    at 0 dB where the Viterbi did not clear the channel. The capture starts
    1554 symbols late on purpose, so the sync has to find the frame rather than
    be told: it reports the marker at bit 9455, which is exactly
    `10232 - 777`.

    Nine guards, each proven by sabotage. Two of them were GREEN on the first
    attempt and the tests were what changed: a rotated de-interleave is
    **invisible against a zeros payload**, because R-S of all-zeros has
    all-zero parity and every interleaved column is then identical — the zeros
    this file uses to keep a missing randomiser visible were hiding a different
    defect, so that section now pays for it with structured data. And the ASM
    search was never exercised at the last offset a marker can occupy, so a
    loop bound of `<` instead of `<=` passed everything while losing exactly
    the frame flush against the end of a capture.

- **The frame assembler carries the inner encoder across frames, and takes a
    capacity.** `ccsds_tm_frame_encode` now reads
    `(cfg, conv, frame, frame_len, out, max_out)`. Both parameters close a hole
    that a single-frame test cannot see, and the prototype's actual use case is
    a *stream* of frames.

    **`conv`** is the caller's `conv_enc_t`, carried from one frame to the
    next. 3.3.2 fixes the inner code's output as one uninterrupted symbol
    sequence with no per-frame flush, and `ccsds_tm.h` says so outright — the
    state is a struct precisely so a chunked caller can carry it, "or introduce
    a discontinuity every chunk boundary that no decoder expects." The
    assembler was that caller, and it called `conv_enc_init` per frame.

    Measured at depth 1, two frames: restarting differs from the continuous
    encoding in **6 of 8288 symbols**, all within the first 7 symbols of frame
    2 — the `K - 1 = 6` bits of register memory, landing on the ASM a receiver
    correlates against. A matched Viterbi absorbs it, which is what makes this
    the same class as the inversion on G2 and the dual basis: self-consistent,
    decodes against a receiver of one's own construction, and not what the
    standard says. `NULL` is still the single-frame form, and now means "this
    frame stands alone" rather than "I forgot".

    **`max_out`** is a capacity rather than a comment, because the CADU is
    assembled in the TAIL of the output buffer: a short buffer was a write past
    the end, not a truncated answer. Its sibling `wfm_frame_bits` already took one.

    Both are proven by sabotage. Restoring the per-frame `conv_enc_init`
    reddens *"a stream of frames must equal one continuous encode of the same
    CADUs"* — asserted against the two CADUs run through one `conv_encode`,
    which is external truth built from the uncoded assembler and the kernel,
    neither of which knows what a frame is. Dropping the capacity check reddens
    both the refusal and *"it must not have written anything"* — the guard is
    preventing a real out-of-bounds write, not a theoretical one.

    The constraint length is public, since `K - 1` is the quantity a caller
    reasons with at both ends: how far a restarted register stays wrong, and
    how much of a stream a decoder needs before its state is the data's rather
    than its own. It reads `CCSDS_TM_CONV.k` now that the code is a
    description rather than a constant.

- **`docs/design/viterbi.md` and `docs/design/fec-receive.md` — the design
    for the FEC receive half.** The decoder gets its own page; the chain page
    owns the synchronization and the lock detector.
    Planned, not built. `ccsds_tm/` encodes today and decodes nothing, so nothing
    can measure a coded link and coding gain is unquotable.

    The prototype decoded symbols dumped from the **shipped** `conv_encode`
    rather than from a re-derivation of it, and **refuted two things a first
    sketch had assumed**:

    - **The code is transparent, so the decoder cannot resolve polarity.** Both
        generators have odd weight (5), so inverting the input inverts both
        outputs and an inverted stream is an exact codeword of the inverted
        bits — measured: `decode(-llr)` returns exactly `~bits`. The sketch had
        node sync searching phase × polarity and picking the best; measured,
        the two polarities read 0.1193 and 0.1197, i.e. it would have been
        choosing from noise. **Polarity belongs to the ASM search**, which must
        correlate for the marker and its complement. Phase does separate, ~2–4×.
    - **The node-sync statistic is the plain disagreement COUNT, settled by
        measurement.** As a comparator over the two phase hypotheses it picks
        correctly in 1000 of 1000 trials at the 500-symbol operating point from
        2 dB up, and is already at 0.9 % single-shot over 64 decoded bits. A
        soft-decision statistic is not needed at this window —
        Mengali, Pellizzoni & Spalvieri (IEEE T-COMM 43(9), 1995) is the
        Mengali, Pellizzoni & Spalvieri (IEEE T-COMM 43(9), 1995) matters
        where a synchronizer must declare on far less data than this one has.
        Three ad-hoc comparators measured **within noise of each other**, and
        the count is the cheapest: no multiplies, and the same number already
        serves as the lock statistic and the channel quality readout.

    Also measured: **`5·K` traceback is 33 % above the BER floor** (0.04178 vs
    0.03137 at 1 dB); depth 60 is within 3 %. And the detector is sized for
    **both** error probabilities, including the one nobody sizes —
    **P_false_unlock**, dropping lock on a working link. At a 0.30 threshold a
    Gaussian puts it at 5.17e-5 where the measurement says 2.50e-3: **48×
    optimistic**, in the direction that promises a link that does not drop.

    **The lock operating point is recorded with what it buys**: window = 500
    channel symbols, threshold = 100 (20 %). The in-sync statistic tracks the
    theoretical channel symbol error rate to within a count (66.6 against 65.5
    predicted at 1 dB; 18.8 against 18.8 at 5 dB), so "sync metric, lock
    statistic and channel quality" really are one quantity. Against false
    *unlock* the threshold is excellent — 1.5 % at 1 dB, below 1e-3 from 2 dB
    up. Against false *lock* a single window is marginal at 12.7–18.8 %,
    because the out-of-sync distribution is ~125 ± 25 and the threshold sits
    about 1σ below its mean; that is the argument for `lockdet`'s hysteresis
    rather than against the threshold.

    The measurement home is settled too, and it is **not a new sweep**: the
    receiver instrument built on `docs/design/rx-test.md` already owns the
    stimulus, the statistics and the frame outcomes, so a coded link is an
    adapter and an operating point — the way `ContinuousMpskReceiver` was.

- **A validation report now files the TRAJECTORIES behind its numbers, not
    just the numbers** ([#846](https://github.com/doppler-dsp/doppler/issues/846)).
    `Report.capture()` attaches telemetry, captures the run, and writes the
    capture and its self-describing sidecar into the object's own `data/`
    folder beside the CSVs — committed like every other artifact there, so the
    scene behind an EVM or a lock time is re-openable without re-running
    anything.

    **`Report` owns it because it already owns the folder.** It is the single
    writer of `results.md`, the plots and `data/*.csv`; a per-validator
    convention would let the layout drift between objects the way the report
    format would without `_validation_common`. `write=False` still captures —
    every measurement runs, as the limits gate expects — and files nothing,
    because a test must never write into the repo.

    What is encapsulated is the ORDER, for the same reason `dp_ber_measure()`
    exists on the C side: probes must be registered BEFORE the capture opens,
    because the ring is sized from the probe table. Attach, then `arm`, then
    run — and getting it wrong raises rather than silently truncating.

    `scripts/plot_capture.py` is the one utility that reads them, replacing
    one script per subject: it takes a filed capture and an optional probe
    selection, resolves names against the capture's own table, and computes
    nothing. `--list` prints the probe table, because the capture describes
    itself. `MpskReceiver` is the first object to file one — 14 probes, 1792
    records, byte-stable across runs.

- **The general assembler, and a CCSDS CADU built from a description.**
    `wfm_frame_assemble()` materialises a `wfm_frame_desc_t`: it writes every
    field in wire order, then runs each stage over the span the layout gave it
    — over that span and no other, which is the whole content of the coverage
    table a standard's framing turns out to be. `wfm_frame_bits()` is now that
    function over the four-field configuration.

    **The kernels arrive as a table, and that is a layering requirement rather
    than a taste.** `ccsds_tm` depends on `wfm/wfm_frame.h` to describe a
    CADU, so `wfm_frame.c` must not call `ccsds_tm`'s kernels — the two would
    form a cycle. A `wfm_frame_ops_t` carries them instead, looked up by stage
    kind and extending the built-in CRC-16 rather than replacing it. A stage
    whose kind is in neither table is a **refusal**, never a silent skip: a
    stage that quietly did not run produces a frame that still assembles,
    still decodes against itself, and syncs to nothing.

    `ccsds_tm_frame_describe()` expresses 131.0-B-3 section 9 as data — three
    fields and three stages — and `ccsds_tm_frame_ops()` supplies the outer
    code, the randomiser and the inner code. They are the *same functions*
    `ccsds_tm_frame_encode()` calls, so the two paths cannot come to disagree
    about what a stage does, only about which bits it is handed, and that is
    what the description states.

    **The falsification is complete.** Across five configurations, the
    described CADU equals `ccsds_tm_frame_encode()`'s output **byte for
    byte** — including a carried `conv_enc_t` across two frames, where 3.3.2's
    continuous symbol sequence would expose an assembler that quietly owned
    its own register. That check inherits everything the shipped encoder is
    already falsified against: figure 9-1's marker, the randomiser's published
    prefix, Annex G's generator, the inner code's impulse response. A
    generalization that agreed only with itself would prove nothing.

    One rule makes a single in-place kernel signature serve a CRC, an outer
    code and a randomiser alike: **a derived field is the last field of its
    stage's cover**, so the kernel reads information at the head of its span
    and writes check symbols into the tail. Descriptions that break it are
    refused. Tracking: gh-853.

- **Scoring a coded frame, and why an outer code beats a CRC at it.**
    `wfm_frame_check()` is the receive mirror of `wfm_frame_assemble()`: it
    undoes each stage over the span the *same description* gives it, in the
    opposite order to the one they were applied in, correcting in place and
    reporting what it found. `Frame.check()` / `FrameDesc.check()` expose it,
    returning a `FrameCheck` record.

    It needs the description and the received bits and **no payload truth at
    all**, so it works on a real capture — which is what makes a truth-free
    frame error rate possible on a coded link.

    **A CRC reports one bit; an outer code reports what it cost.** Both frames
    below "pass", and only one of them is healthy:

    ```text
    clean          passed=1  units=6 ok=6  corrected=0  symbols=0
    80-symbol burst passed=1  units=6 ok=6  corrected=5  symbols=80
    E+1 in one cw   passed=0  units=6 ok=5  corrected=0  symbols=0
    ```

    A margin being spent is visible long before it is lost, and a failure
    names how much of the frame went with it rather than condemning the whole
    thing.

    **A stage the receiver does not reverse is reported as NOT CHECKED, never
    as passed** — `checked < stages` says so. The inner code is the case: a
    Viterbi is streaming and emits its decisions `depth` bits late, so frame
    checking begins after the inner decode and after frame synchronisation,
    and a frame checker never sees channel symbols. Likewise a description
    with no reversible stage at all returns "not checked" rather than "passed"
    — an FER conflating the two would score every unprotected frame as
    perfect.

- **Gallery page and worked example for the whole slice.**
    [A CCSDS CADU](docs/gallery/ccsds-link.md) walks the description end to
    end: the three fields, the three covers, why a pipeline cannot express the
    asymmetry between them, the `wfmgen` flags that reach the same thing, and
    what the receive side reports. Its numbers are executed rather than
    transcribed — the `pycon` fences run under the docs gate and the plot
    comes from `src/doppler/examples/ccsds_link_demo.py`, which self-validates
    with physical asserts (the interleaver must carry exactly a `depth * E`
    symbol burst and refuse one symbol more).

- **A frame can be described from Python, CCSDS included.**
    `doppler.wfm.FrameDesc` is `Frame`'s deferred flavor: the same
    constructor arguments, but it stops before materialising, so the four
    fields `wfm_frame_t` names are a starting point a caller extends with
    `add_field` / `add_stage` before `build()`. Empty arrays for all three
    begin from nothing.

    That is what makes the CCSDS coding reachable from Python at all.
    `ccsds_tm` has no binding and is not getting one, so a caller meets the
    outer code, the randomiser and the inner code by **describing** a CADU —
    three fields and three covers — rather than through a CCSDS entry point
    bolted onto this object. The covers are 131.0-B-3's coverage table: the
    inner code reaches over the marker and neither of the other two does.

    `Frame` is unchanged and is now visibly one configuration of the general
    description: `n_fields()`, `n_stages()`, `field_off()`, `field_bits()`,
    `stage_first()` and `stage_bits()` read it too, and agree with the named
    `layout()` field for field.

    A view rather than a second type, by the rule the `ddc` module already
    follows — a difference in CONSTRUCTOR is a flavor; a difference in METHOD
    SIGNATURE is a separate type. Every method is shared verbatim.

    Two carve-outs, both filed. `add_field`/`add_stage` take `kind` as an int
    rather than one of the enum's names, because a method parameter cannot yet
    be a string enum
    ([just-makeit#1021](https://github.com/just-buildit/just-makeit/issues/1021)
    — the `enum` key is accepted on a method parameter and silently ignored,
    which is the half worth fixing). And `layout()`'s named view reports
    nothing for a description, on purpose: it would go stale the moment a
    fifth field is appended, and a stale offset is worse than an absent one.

- **A frame is a field list and a stage list, not a pipeline.**
    `wfm_frame_desc_t` describes a frame as an ordered list of **fields** —
    what appears on the wire, in order — and an ordered list of **stages**,
    each carrying the **span it covers**. `wfm_frame_desc_layout()` derives
    every field offset, every stage span and both lengths from the two.

    `wfm_frame_t` is now a *configuration* of it rather than a rival:
    `wfm_frame_layout()` builds the description through
    `wfm_frame_describe()` and reads the general layout back, so there is one
    implementation of the arithmetic and the two cannot drift.

    **`cover` is the load-bearing part.** `ccsds_tm_frame.h` already predicts
    the failure of leaving it out — *"any chain of optional transforms applied
    to 'the frame' is right at three stage boundaries and wrong at the fourth,
    and wrong in the direction that still encodes, still decodes against
    itself, and syncs to nothing."* A stage that inherited "whatever ran
    before me" would be that chain. CCSDS is the case that proves it: the
    marker is covered by the inner code and by neither the outer code nor the
    randomiser.

    Two rules fell out of prototyping and both simplify the model. A derived
    field **is** a field, which removes any need for a stage to expand what it
    covers and makes R-S check symbols and a CRC trailer one concept rather
    than two. And "emits a new unit" is a distinct property — the inner code
    consumes the assembled CADU and emits channel symbols — which is exactly
    why `ccsds_tm_frame_layout_t` reports `cadu_bits` and `out_bits` as two
    numbers.

    Checked against both shipped framers: the existing frame's layout is
    unchanged, and a description configured as CCSDS reproduces
    `ccsds_tm_frame_layout()`'s four spans and both lengths exactly, across
    three configurations. That is the layout half of the falsification;
    byte-for-byte output needs a general assembler and is not claimed yet.
    Design: `docs/design/frame-description.md`. Tracking: gh-853.

- **The generated tree is checked at the commit that changes it, not in CI.**
    Two pre-commit hooks close the gap `make lint` structurally cannot see —
    lint checks *sources*, while `docs/c-api` is mkdoxy output and the docstring
    ratchet reads generated stubs.

    `gen-c-api-drift` runs `make gen-c-api-check` when a staged file matches the
    Doxyfile's own `INPUT` and `FILE_PATTERNS`, so editing a header and
    forgetting the regenerated tree fails the commit that caused it. It is
    narrowed rather than unconditional because it costs 19 s and shells out to
    a pinned doxygen container on any box whose doxygen is not 1.9.8.

    `docs-invariants` runs the thirteen fast checks `docs-check` runs before its
    site build — API-doc coverage, the docstring-coverage ratchet, nav index,
    doc/face parity, version strings, generator drift — in **1.6 s**, on every
    commit. It replaces the narrower `docs-drift` hook, whose four checks it
    contains, and iterates `DOCS_CHECK_PRE_CMDS` directly so the hook and the
    gate cannot disagree about what an invariant is.

    Both run the same commands CI runs, so a local pass means a CI pass.

- **`docs/dev/issues.md` — the whole backlog, tiered by the kind of harm each
    issue does.** Six tiers, from *breaks for a user* down to *convergence and
    hygiene*, with a summary and status per issue. Most open issues carry no
    label at all, so this ordering **is** the triage rather than a view onto
    one that already existed.

    Generated, because a hand-maintained list of eighty-odd issues is the
    shape this repo has had to delete before: it rots in both directions at
    once, missing what was filed and still naming what was closed.
    `make issues` reconciles the committed tier map against the live issue
    list and **fails** on either — an open issue with no tier is untriaged and
    says so; a tier entry whose issue is closed is a stale row and says so.

    The judgement half lives in `docs/dev/issue-tiers.toml`, committed so a
    tier assignment is reviewed like code rather than living in someone's
    head. The rendering half is gated offline: `gen_issue_tracker.py --check`
    re-renders from that map and diffs the page, so a hand-edit is caught by
    `docs-drift-check` like every other generated region.

    `make issues` is deliberately **not** in CI and not in `docs-relink`. It
    is the only generator whose input is off the machine — it reads GitHub
    through `gh` — so a CI job built on it would fail on a rate limit rather
    than on the tree. Freshness cannot be checked offline, so the page states
    the date it was derived and the command that derives it, which is the
    date-plus-derivation this repo requires of any recorded live value.

- **The lock thresholds a decision actually used are readable.**
    `MpskReceiver` and its views gain `lock_drop_thresh`, `sync_lock_thresh`
    and `sync_lock_drop_thresh`, joining the `lock_thresh` that was already
    there. Anything reading a lock statistic against its decision needs both
    edges of the pair, and the carrier drop level (`0.8 ×` declare) and the
    timing loop's levels were previously reachable only by retyping them —
    a second copy of a rule the object owns, free to drift from the decision
    it claims to describe.

    The timing pair reading `0.311 / 0.311` is information rather than a
    defect: the timing loop carries no *level* hysteresis, its hysteresis
    living in the verify counts instead. Its threshold is also not the
    carrier's number and is not derived the same way — symsync sizes block
    length and threshold together from (rolloff, esno_min, pfa, pd).

- **`loop_filter_wn(bn, zeta)` is public** — the natural frequency
    `8*zeta*bn / (4*zeta^2 + 1)`, which at `zeta = 0.707` is `1.8857*bn`.
    Every closed form about a second-order loop is written in `wn`, and the
    one that matters for measurement is the steady-state phase lag under a
    frequency **ramp**, `2*pi*r / wn^2` — the disturbance a type-2 loop does
    *not* null, and therefore the only one against which a gain can be
    checked. A frequency step is nulled regardless of gain and cannot.

    Extracted from `loop_filter_init()` and **deliberately unguarded**, so the
    behaviour of that function changes by exactly nothing — including the
    non-finite case its own docstring documents. `loop_filter_create()` is the
    boundary that rejects the domain, and the test pins it doing so.

    `native/validation/rx_nda_tap.c`'s private `rx_nda_wn` is gone.

    **Three of the five "copies" turned out not to be copies**, which is worth
    recording because the grep that found them looked convincing.
    `loop_filter_bandwidth_demo.py` and `validate.py` share only the
    `(4*zeta^2 + 1)` denominator — their `excess_law` is
    `16*zeta^2 / (4*zeta^2 + 1)^2`, a different closed form. And
    `test_loop_filter_core.c`'s `theta = 4*zeta*bn*t / (4*zeta^2 + 1)` is
    `wn*t/2` written out **independently on purpose**: its own comment says
    re-typing the implementation's formula beside it "would prove only that
    the file had been copied correctly". Repointing either would have deleted
    the independence that makes them evidence.

    The new test exploits that instead: it ties `loop_filter_wn()` to the
    test's existing independent derivation (`wn*t/2 == theta`) rather than to
    a copy of its own expression, and pins the quoted `1.8857*bn`, linearity
    in `bn`, and that the gains `loop_filter_init()` produces still match.
    Sabotage: `8.0 -> 8.1` takes six assertions red.

- **`MpskReceiver.locked` — the binary carrier-lock indicator reaches Python.**
    `mpsk_receiver_get_locked()` has always existed in C, and `lock_time`'s own
    docstring referred to "polling `locked` in a loop", but the property was
    never declared, so the Python face had only the raw `lock` metric. Adding
    it also gives the Python tests the observable that replaces `tracking`.

    Its docstring states what the C header states and the API reference now
    repeats: it is an **indicator and nothing else**. It steers no loop and
    gates no output, so a wrong reading costs a caller their measurement window
    and costs the demodulator nothing. Its statistic's H1 mean is a function of
    Es/N0 alone, so the instant it declares carries no information about how
    converged the carrier estimate is — `lock_time` plus a settling budget is
    the question that asks about convergence.

- **Two more `mpsk_receiver_core.h` claims get C tests, both sabotage-proven**
    ([#814](https://github.com/doppler-dsp/doppler/issues/814)).

    **§14 — `bn_carrier` is normalised to the SYMBOL rate.** The `@warning`'s own
    headline: *"at the old default `sps = 8` the same number is now an 8x wider
    loop"*. Nothing measured it, so a regression to input-rate normalisation
    would have read correct at `sps = 8` — the rate every other test in the file
    uses — and been wrong everywhere else. Measured as settling time **in
    symbols** at one `bn` across a 4x span of `sps`, with the offset held
    constant in symbol-rate units: 320 symbols at `sps` 8, 16 and 32. An
    input-rate `bn` would scale that by 4, which is what makes the invariance the
    discriminator. Read off `get_norm_freq` rather than the lock detector, whose
    own EMA and verify counts made an earlier attempt useless (955 symbols at
    `sps = 4` against 51 at 16). Sabotage: scaling `bn_carrier` by `lo_sps` takes
    it red.

    **§15 — never pair `m_out = 2` with `MPSK_RX_PULSE_IANDD`.** The header says
    *never* and construction permits it anyway. Pinned as the **degeneracy**
    rather than the failure rate — *"fails about half the time"* is a
    distribution over seeds, not an assertion — and the mechanism is
    deterministic: **+11 dB** of EVM excess against `m_out = 8`'s +0.5. The test
    also asserts that `lock` stays above the declare threshold at both
    geometries, because it does: a caller who pairs 2 with I&D and watches
    `lock` sees nothing wrong, which is why the header's "never" could not be
    left to runtime.

- **Two `mpsk_receiver_core.h` claims that nothing tested now have C tests**,
    both proven by sabotage ([#814](https://github.com/doppler-dsp/doppler/issues/814)).

    **§4b — the handover carries the frequency estimate across, both ways.** The
    header's claim is not that the flip happens (§4 pins that, along with the
    drop-back and the re-declare) but that *"the shared loop filter carries the
    frequency estimate across it in both directions, so a drop-back is a
    discriminator swap rather than a cold re-acquisition"*. §4 cannot test it, and
    not merely by omission — it calls `mpsk_receiver_set_norm_freq()` right after
    the drop-back, **overwriting the very quantity the claim is about**, so a
    receiver that cleared its filter on every mode change would pass §4 unchanged.

    §4b steps in one-symbol chunks so the measurement straddles each transition
    rather than bracketing it at block boundaries, and checks both: forward, the
    estimate is already the offset before the flip *and* undisturbed by it;
    reverse, the drop-back preserves it too, deliberately without §4's re-seed.
    The failure it guards is silent and expensive — a cold re-acquisition still
    reaches lock, so SER recovers and `tracking` returns to 1; it just pays the
    pull-in again, and at a marginal `bn` it slips instead.

    **§12 — the verify counts are time hysteresis.** *"Both directions are
    verify-counted (8 symbols up / 32 down)"* was documented and tested nowhere,
    and `carrier_nda`'s certification found the analogous count mattered a great
    deal — its `n_up = 8` false-declared 18/60 at one geometry. Measured as
    behaviour rather than by reading the count back: on one record, `n_up = 2`
    must declare strictly earlier than `n_up = 64`, and by at least the extra
    symbols asked for. Sabotage: making `configure_lock` ignore `n_up` takes both
    assertions red, where a count wired to nothing would give the same instant
    twice.

- **`docs/design/mpsk-refactor.md` — the design for collapsing `MpskReceiver`
    and `MpskReceiverR` into one object with three faces.** Planned, not built.
    The argument is a measurement: the two differ only in a front-end pointer
    and one rate convention, and `mpsk_receiver_r_core.c` is 372 lines of which
    16 functions are pure delegations — but the cost of the split is not the
    duplication, it is that their shared 784-line `mpsk_rx_loops.h` **has no
    test home**, so its claims are pinned only where one twin's tests happen to
    reach them. "The LO runs at half the input rate" is pinned by neither, and
    that is where the gh-765 `freq_scale` bug lived.

- **`mpsk.mpsk_soft_demap` — per-bit log-likelihood ratios from the M-PSK
    constellation.** The module docstring has promised *"hard and **soft**"*
    since the module shipped, and there was no soft anything in it. There is
    now, and the caller it was built for is the CCSDS inner decoder: a
    rate-1/2 K=7 Viterbi fed hard bits gives up roughly 2 dB of the coding
    gain it exists to deliver.

    The convention, because every consumer has to agree with it:
    `L_i = log( P(bit i = 0) / P(bit i = 1) )`, so **positive means bit 0**
    and the hard decision is `L < 0`. That is not a second decision rule — it
    is the same one seen differently, and *"the LLR's sign reproduces
    `mpsk_demap`'s label"* is asserted at every M across Es/N0 from −3 dB
    (where the decision is nearly a coin toss) to +20 dB. Sabotage: flipping
    the sign reddens it at every point.

    **One general path, no per-M fast paths.** BPSK and QPSK have exact
    closed forms — `4·Re{y}/N0`, and `4·(1/√2)·{Re,Im}{y}/N0` for QPSK, whose
    `phi0 = pi/4` grid is axis-separable — but shipping them beside the
    general path would be two implementations of one primitive, which is the
    thing that drifts. They are worth more as **test assertions**: they prove
    the general path right, to 2e-4, and pin the grid that makes the
    separability true rather than coincidental.

    Also pinned, each sabotage-proven: exact linearity in `1/N0` (so a caller
    without an SNR estimate may pass 1.0 and rescale, and a Viterbi may ignore
    it entirely); that the origin — equidistant from every point — reads
    exactly zero on every bit, and that confidence grows strictly along the
    ray to a constellation point, which is what a demapper returning the hard
    decision as ±1 would fail; and four refusals (short buffer, unsupported M,
    zero and negative `N0`) verified against a poisoned buffer, since silence
    is this function's whole contract.

    `llr` is a caller-provided out-param rather than a returned array because
    the output expands by `log2(M)` and jm sizes a function's output array 1:1
    with its input — `kaiser_window` is the existing precedent for that shape.
    What max-log costs at 8PSK **in dB** is deliberately not claimed: it is an
    Eb/N0 offset on a decoded BER curve and cannot be measured until the
    decoder exists (`docs/design/mpsk-soft.md` §5).

- **`docs/design/mpsk-soft.md` — the design for LLR output from the M-PSK
    constellation.** Planned, not built. `doppler.mpsk`'s module docstring has
    said *"M-ary PSK mapping: hard and **soft**"* since the module shipped, and
    there is no soft anything in it — five functions, all hard-decision. The
    caller that makes this urgent is the CCSDS inner decoder: a rate-1/2 K=7
    Viterbi fed hard bits gives up roughly **2 dB** of the coding gain it
    exists to deliver, which is more than the difference between having the
    convolutional code and not having it.

    Three things the throwaway prototype settled before any C was written, all
    measured against the shipped `mpsk_map`/`mpsk_demap` rather than a numpy
    re-derivation:

    - **The sign convention reproduces `mpsk_demap` exactly** — zero
        mismatches over 20 000 symbols at each of M ∈ {2, 4, 8} × Es/N0 ∈
        {−3, 0, +6, +20} dB, including where the decision is nearly a coin
        toss. That matters because the repository has exactly one decision
        rule and a soft demapper that disagreed anywhere would be a second.
    - **BPSK and QPSK have exact closed forms and need no search.** With
        `phi0 = pi/4` the QPSK grid is axis-separable, so its two bits are
        independent BPSK decisions: `4·Re{y}/N0` and `4·(1/√2)·{Re,Im}{y}/N0`,
        agreeing with the general path to 1e-14. Two of the three
        constellations are one multiply per bit.
    - **max-log is free at M = 2 and M = 4 and costs something at M = 8** —
        identical for the first two (with one point per bit subset per axis,
        the maximum *is* the sum), and 3–14 % median LLR error at 8PSK.

    What that costs **in dB** is named as an unknown rather than quoted from
    literature: it is an Eb/N0 offset on a decoded BER curve and cannot be
    measured until the decoder exists, so the header will not claim a figure
    until phase 7 does.

- **Node synchronization is the library's job now, in `conv`.**
    `node_sync_score` decodes a window, **re-encodes the decisions**, and
    counts where the result disagrees with the received hard decisions;
    `node_sync_scan` runs that over all `n` alignments of a rate-1/n code and
    reports the winner with its margin. It references no truth, no marker and
    no training sequence — only the decoder's own input and output — so it
    works on a live capture, which is what makes it a receiver's statistic
    rather than a simulation's. Closes
    [#834](https://github.com/doppler-dsp/doppler/issues/834).

    In sync the count **is** the channel symbol error rate. Out of sync it is
    not a half, and the difference is worth stating because a half is what a
    coin-flip argument predicts: the decoder is a maximum-LIKELIHOOD search,
    so it finds whatever codeword agrees best with the misaligned stream.
    Measured on clean streams: **24 %** of symbols for CCSDS K=7 r=1/2, 23 %
    uninverted, 18 % for a K=5 r=1/3 — against 0 % for the right alignment.

    Two things the tests found rather than confirmed. The head of a window
    must be discarded by the DECODER's traceback depth, not the encoder's
    `k-1`: two cold starts overlap there and the decoder's all-zero prior is
    the larger, so skipping only `k-1` left three disagreements in 1598
    symbols on a clean stream and broke the polarity equality. And the scan
    must be scored over the window it is about to decode — at Es/N0 = +1 dB a
    slip early in a record made a whole-record scan prefer the phase that was
    right for the tail, and frame sync then found no marker at the head.

    `native/validation/rx_coding_gain.c` was picking the phase by which parity
    put an ASM where an ASM could be — the harness doing the library's job
    with a statistic that exists only because CCSDS supplies a marker.
    Swapping it onto the re-encoding metric changed **no measured number**:
    same frames, same bits, same bound. That is the evidence the
    general statistic is at least as good as the special one.

- **The standard test harness gained the two helpers this needed**, rather
    than a private copy in one file: `dp_bit_distance` (bits differing between
    two packed-octet buffers — what `ber_meter` answers for a symbol stream
    and cannot be pointed at two byte arrays) and `dp_rx_duty` (the share of a
    window where a per-symbol flag is set). The standard record
    (`dp_rx_result_t`) now carries `lock_duty` and `lock_stat_duty`, printed
    with every battery row.

- **`doppler.coding.ReedSolomon` — a caller can name their own block code, in
    both directions.** `rs_encode`, `rs_syndromes` and `rs_codeword_ok` were
    reachable from Python only through `FrameDesc`'s Reed-Solomon stage, which
    binds to `ccsds_tm_frame_ops` and carries an interleaving depth rather than
    a code — so Python could run exactly **one** Reed-Solomon code, CCSDS's,
    and only inside a frame. This is the third and last of the family slices
    [gh-900](https://github.com/doppler-dsp/doppler/issues/900) named, after
    `ConvEncoder` and `SyncFinder`, and it takes the same shape: `rs` owns the
    code, the object owns the binding of a code to its tables, and every method
    calls the matching kernel rather than reimplementing it.

    `encode` answers in whole systematic codewords because that is the unit
    every other method takes, and it may encode **in place** — the call a frame
    assembler makes, and the reason `rs_encode` stays exposed as the
    parity-only primitive. `decode` corrects the caller's own array and returns
    the count, with **-1** for "too far from every codeword to name one" and
    **-2** for a word that was not `n` symbols long, because those are
    different kinds of fact: one is the channel's answer and one is the
    caller's mistake.

    **Two of the five constructor arguments are validated rather than
    trusted**, and it has to be there or nowhere: a non-primitive `field_poly`
    and a `root_stride` sharing a factor with `n` each produce arithmetic that
    encodes and decodes against itself perfectly, so no round trip can ever
    catch them. `generator()` is exposed for the matching reason — standards
    publish those coefficients, so it is how a caller checks that they read the
    five numbers correctly against the *document* rather than against this
    implementation.

    Sixteen mutations of the object were each confirmed to turn the C suite
    red, and two of them found real gaps first: nothing had pinned that
    `first_root` reaches the arithmetic at all (a codec that silently used the
    textbook root set would produce a perfectly good RS(255,223) that no CCSDS
    receiver can decode), and nothing had pinned the `*_max_out` bounds — an
    off-by-one in `generator_max_out` is a heap overflow, because the binding
    allocates from it. Both now have sections of their own.

    The Python face is checked against `math.comb` arithmetic written out from
    the definition, not against the kernel: an oracle that shares its
    arithmetic with its subject proves only self-consistency, which every wrong
    Reed-Solomon also has.

- **A runnable example and a gallery page for the whole `doppler.coding`
    family.** `src/doppler/examples/coding_demo.py` runs an outer code, a
    marker, an inner code, a real AWGN channel, acquisition and both decoders,
    on a configuration that is nobody's standard — and it closes a gap the
    previous three slices each left: `ConvEncoder`, `Viterbi` and `SyncFinder`
    all shipped with no example exercising them, which
    [the lifecycle page](https://github.com/doppler-dsp/doppler/blob/main/docs/dev/contributing/adding-algorithms.md)
    owes unconditionally.

    It also measures something the tree stated and never showed: **past its
    radius a bounded-distance decoder can be silently wrong**, returning a
    positive count for a word that passes `codeword_ok` because it *is* a
    codeword — just not the one that was sent. The sweep runs on a deliberately
    weak RS(15,11), because the miscorrection probability is about
    `sum(C(n,i)(q-1)**i for i<=E) / q**(n-k)` — 2e-05 for RS(255,239) and 0.36
    for RS(15,11) — and a sweep of the strong code would have been reporting
    its own sample size. The measured rate converges onto that closed form,
    which the example asserts.

- **A release can no longer be too large to publish, and the same number says
    when one is due.** GitHub caps a release body at 125,000 characters, and
    `release.yml`'s `github-release` job lists `publish-python` in its
    `needs` — so an oversized body fails *after* the version is on PyPI, and
    PyPI refuses a re-upload, which removes the ordinary "rerun the release"
    recovery. Measured on 2026-08-22: `[Unreleased]` plus `changelog.d/`
    projected to **410,891** characters, 3.3x the cap, against 19,296 for
    v0.42.0's real notes.

    `scripts/check_release_notes_size.py` runs on `make lint` and in `gates`,
    and measures what would actually be **published** rather than what
    CHANGELOG.md holds: a version section may carry a `### Highlights` block,
    and `release-notes.sh` publishes that, with a link to the full section,
    when the whole section will not fit. CHANGELOG.md keeps its full depth —
    the record is the point of it.

    The cadence half is the same measurement, not a second mechanism.
    Deferring a release is exactly what makes the body grow, so the gate warns
    at half the budget ("a release is due") and fails at the cap. A "days
    since the last tag" rule would have been a rule about the clock, and the
    clock is not what breaks: a quiet fortnight ships fine, and one like this
    one does not.

- **Reed-Solomon is a description now, and it CORRECTS.** `rs/rs_core.h` is a
    general Reed-Solomon code over `GF(2^J)` — a symbol width, a field
    polynomial, a parity count, a first root and a root stride — with the
    encoder, the syndromes and a Berlekamp-Massey / Chien / Forney decoder all
    reading the same `rs_code_t`. **Nothing in it is CCSDS.** 131.0-B-3's
    (255,223) `E = 16` is `CCSDS_TM_RS`, five numbers in `ccsds_tm/ccsds_tm_rs.h`,
    beside the two things the standard adds that are not properties of the
    code: the dual-basis symbol representation (4.3.9) and the interleaver
    (4.4.1). Its own `c_dep`, for the reason `conv` is one — a caller who
    wants a Reed-Solomon code should not link a channel-coding standard.

    This closes [#826](https://github.com/doppler-dsp/doppler/issues/826): the
    outer code checked and could not correct, so all the concatenated coding
    gain past the Viterbi was unavailable. In `examples/c/ccsds_link_demo.c`
    at `Es/N0 = 0 dB`, where the inner code does not clear the channel, the
    three symbol errors it lets through used to cost **three of ten
    codewords** and a frame that was wrong-but-known-wrong; they are now
    repaired and all ten codewords are good, frames byte-exact.
    `ccsds_tm_frame_rx_t` grew `rs_corrected` and `rs_symbols` to report the repair
    work, and `rs_ok` now counts codewords valid **after** decoding.

    **Two offsets a textbook will not warn about**, both of which produce a
    decoder that decodes its own encoder perfectly and interoperates with
    nothing — the failure this slice keeps finding in new guises. First: the
    syndromes are a power sum only after substituting `Yt = Y * X^j0`, so
    running Berlekamp-Massey while assuming `j0 = 1` finds the right error
    *positions* and magnitudes wrong by `X^(j0-1)`, and every syndrome still
    checks out against the decoder's own model. Second: Chien iterates the
    **position exponent** rather than field elements, which is what makes a
    root stride of 11 cost nothing — a search over `a^e` has to invert the
    stride to learn where the error is. Both are derived in
    `docs/design/reed-solomon.md`, which owns the outer code the way
    `docs/design/viterbi.md` owns the inner one.

    **Validated rather than trusted:** `rs_init` refuses a non-primitive field
    polynomial (the table must visit every nonzero element exactly once) and
    `rs_code_valid` refuses a root stride sharing a factor with `n`, because
    both produce arithmetic that is entirely self-consistent — CCSDS 4.3.4
    states the second as a note about `a^11`, and for a general implementation
    a note is a condition to check.

    The external truth is the code's own distance rather than a round trip:
    `E` symbol errors corrected exactly, the sent word **never** recovered at
    `E+1`, refusal as often as `~1/E!` says, and — provable, and proved —
    a decode either refuses or returns a codeword, never a third thing, with a
    refused word left untouched. Checked at three configurations (textbook
    RS(255,223), the CCSDS-shaped one, and RS(15,11) where every single-symbol
    error at every position and value is swept). Every guard proven by
    sabotage, including one that was NOT: a zero-magnitude refusal branch that
    survived 600k adversarial patterns without ever executing, deleted rather
    than left as a claim nothing runs.

- **The receiver battery measures the ramp law.** `dp_rx_result_t`'s
    `ramp_lag_rad`/`ramp_law_rad` had been declared since the instrument
    landed and filled by nothing. Filling them found two reasons the
    `doppler` point could not have measured anything, and both were in the
    measurement rather than the receiver.

    **The point was three and a half decades below the answer.** It carried
    0.02 ppm/s, which `2*pi*r/wn^2` puts at a settled lag of 2.2e-4 rad
    against a linear range of `pi/4` — so every number it produced was
    byte-identical to `anchor`'s, and a point that reproduces the reference
    is not measuring the thing it is named for. The rate is now sized from
    the answer it has to make observable: **9.2 ppm/s**, predicting ~0.1 rad,
    an eighth of the range, so the *law* is checked rather than its
    breakdown.

    **The estimator was reading its own noise.** The discriminator series was
    reduced to `fabs()` at capture, and at these Es/N0 the discriminator's
    noise dwarfs the lag — `|e|` has an RMS of 0.54 against a lag of 0.1, so
    a mean of it is nearly identical at every point in the battery. The
    series is now **signed** and the magnitude taken of the **mean**: the
    loop's own integrator forces that mean to the value which sustains the
    ramp, so the noise averages out of it. Measured **0.1006 rad against
    0.09989 predicted, +0.7%** — the agreement `rx_nda_tap.c` gets on a
    *noiseless* tail, here at 6.79 dB Es/N0.

    Gated at the 10% tolerance `rx_nda_tap.c` established, and only where a
    ramp exists: a type-2 loop nulls a frequency step regardless of gain, so
    an unimpaired point has a law of exactly zero and nothing to check.
    `wn` comes from the new `loop_filter_wn()`, and the damping it needs is
    **read back from the constructed receiver** through a new `zeta` entry on
    `dp_rx_iface_t` — the adapter passes `0` and asks the receiver to derive
    it, so restating the default would have been a copy of the number the
    receiver is free to change.

    Sabotage, both ways: scaling `freq_scale` by 2 reads +101.4% and fails,
    by 1.25 reads -19.4% and fails — **and the trio stays green through
    both** (SER 1.09e-3, EVM -7.35 dB, unmoved). A carrier loop running at
    twice its stated bandwidth is invisible to every other number the
    instrument produces.

    **What it does not cover, structurally.** gh-765 itself was `freq_scale`
    missing its `* upd` — the filter's output taken as radians per *update*
    rather than per symbol. Every battery point runs `nda_tap = 0` (STROBE),
    whose update rate is exactly 1, so that factor *is* 1 and removing it
    leaves this gate byte-identical and green. `rx_nda_tap.c` catches it, on
    the taps whose update rate is not 1 (`mf_out` 2.0, `mf_in` 1.5625) and
    never on `strobe`. A battery point at a non-unity tap is what would close
    it, and that is entangled with the open `nda_tap` question (#791).

- **The receiver battery is a complete suite, and it runs on two receivers.**
    Four operating points added (`qpsk`, `psk8`, `irrational` at
    `sps = 17.33389`, `rate_odd` at `sps = 31.7`), and a second adapter for
    `ContinuousMpskReceiver` — which is `docs/design/rx-test.md` goal 6 cashed
    rather than asserted: the second receiver costs one function and reuses the
    other ten interface entries unchanged. Each M is read at **its own**
    SER=1e-3 Es/N0 (6.79 / 10.35 / 15.68 dB from `ber_esn0_db_for_ser`),
    because holding one Es/N0 across M compares constellations rather than
    receivers.

    What it found immediately, and none of it was visible to the Python
    harness it replaces:

    - **`nda_tap = mf_in` refuses on all 9 points** of the battery, under
        either pulse, on the BASE receiver — isolated by changing only that
        one argument. Diagnosed under Changed: the tap acquires everywhere,
        and it is its LOCK STATISTIC that falls under the detector's
        threshold, so the refusals are the settle gate reading a degraded
        indicator rather than a loop that never moved.
    - **Implementation loss grows with irrational oversampling**: 0.07 dB at
        `sps = 8`, **4.34 dB** at 17.33389, **7.41 dB** at 31.7 — a trend, not
        a cliff, and defensible because the harness's four gates passed.
    - `qpsk`/`psk8` **refuse** on frame geometry (the frame's bit count does
        not divide into whole symbols at M = 4/8). A refusal is a result: the
        frame set is BPSK-shaped and the M sweep needs a length that divides.

- **Coding gain, measured through a real receiver.**
    `native/validation/rx_coding_gain.c` runs the whole CCSDS chain in both
    directions with a demodulator in the middle — R-S, randomiser, ASM, K=7
    r=1/2, BPSK, RRC, AWGN, `MpskReceiver`, soft demap, node sync, Viterbi,
    ASM search, derandomise, R-S **decode** — and reports what each stage
    saw. It is an adapter and an operating point rather than a second
    harness: the receiver adapters moved to `native/tests/dp_rx_mpsk.h` and
    are shared with `rx_battery.c`, and the point is `DP_RX_ANCHOR` with one
    field changed, so a difference from the battery's numbers is the coding
    or the Es/N0 and cannot be the geometry.

    At Es/N0 = +2 dB (Eb/N0 5.59 dB), with the channel putting **one symbol in
    25** wrong before decoding, the link delivered **46 of 46 frame slots,
    every one byte-exact, 0 payload errors in 410 320 bits** — a coding gain
    of **≥ 4.1 dB**, where the bound is the run length rather than the code:
    zero errors is not a rate, so it is the exact 95 % upper limit
    (`ber_confidence`) turned into the Eb/N0 an uncoded link would have
    needed (`ber_esn0_db_for_ser`), minus the Eb/N0 this link ran at. The
    rate is charged first — R = 1/2 × 223/255, so 3.59 dB of redundancy
    before any gain is claimed.

    **Three things only a receiver-in-the-loop run could say**, all now in
    `docs/design/fec-receive.md` §8. The uncoded lock detector is not a
    usable gate for a coded link: the binary `locked` flag reads 23 % at 0 dB
    while the loops track throughout and every delivered frame is byte-exact,
    so the window is the settling budget and the evidence of lock is that the
    marker appears. Slips are real at these Es/N0 — frame sync loses the
    marker where it expected it 4 times over ~46 slots at 0 dB, falling to
    zero only at the clean point — and a measured slip moved the stream by an
    ODD number of symbols, which flips the `(C1, C2)` parity and makes every
    subsequent bit noise, so node sync cannot be a one-shot at start of
    stream. And the outer code **never miscorrected**: a CADU that
    decodes, reports every codeword good and matches no transmitted frame is
    the failure `rs_core.h` warns is possible past `E`, and across the whole
    sweep including the points where nothing synchronised there were zero.

    Five gates, each proven by sabotage: disabling R-S correction, sweeping
    only an easy link (which fires the channel-SER, gain-bound and
    waterfall-span gates at once), and forcing the node-sync hypothesis test
    to a tie.

- **A Python receiver can ACQUIRE a frame, not just check one it was handed.**
    `doppler.detection.SyncFinder` correlates a known marker against every bit
    offset of a stream, in both polarities, and reports the first offset
    within a tolerance. `doppler.wfm.ccsds_asm_bits()` hands it CCSDS's
    `0x1ACFFC1D`. Together they close the gap
    [gh-900](https://github.com/doppler-dsp/doppler/issues/900) named: the
    frame checker `Frame.check()` had always existed, and nothing could find
    a frame for it to score, so everything the `ccsds_tm` certification
    measured about that detector described a function Python could not call.

    **The search is general and CCSDS is a configuration of it.** The kernel
    moved to a header-only `native/inc/dp_syncword.h`, and
    `ccsds_tm_asm_find` is now two lines over it — the same relationship
    `CCSDS_TM_CONV` has to `conv_code_t`. A standard picks a pattern; the
    correlation is not the standard's. `SyncFinder` therefore takes any
    marker, at any length, and `detection` stays free of any one document's
    picks.

    **`max_errors` is answered, not warned about.**
    [gh-897](https://github.com/doppler-dsp/doppler/issues/897) found that the
    threshold has to be chosen against the SEARCH WINDOW and that nothing said
    so: half of 32 is 16, so 8 "sounds safe", and at `t = 8` the marker is
    found at its true offset only 58 % of the time on a stream with no channel
    errors at all — each preceding offset is an independent chance to
    false-hit first. `SyncFinder.pfa(t)` is the per-offset false-alarm
    probability and `SyncFinder.max_errors_for(window_bits, pfa)` inverts it
    through `1 - (1 - pfa)**W`, returning the largest tolerance that still
    holds. They sit beside the search the way `det_threshold` sits beside
    `det_pd`, and they answer *for the marker being searched*, so a threshold
    and the thing it thresholds cannot come from two declarations.

    Checked against three independent oracles rather than against itself: an
    exhaustive enumeration of all 2^8 windows in C, `math.comb` in Python, and
    — the one that matters — **the search itself**, whose measured accept rate
    over 20000 random windows tracks the formula it is meant to size. Ten
    mutations of the kernel and three of the binding were each confirmed to
    turn the suite red.

    **A finding that was prose until now runs.** A complemented CADU passes
    its own outer code, cleanly, with nothing corrected: Reed-Solomon is
    linear and the all-ones vector is itself a full-length codeword, so a
    global flip lands on another codeword and the randomiser carries it
    straight through. A receiver that acquired at the right offset and ignored
    the reported polarity would score a clean PASS on a frame whose every
    payload bit is wrong. The headers have said the marker is "the only thing
    in a CADU that can say so" since the component was written; nothing had
    ever run it.

    The marker also stopped being transcribed. `0x1ACFFC1D` was expanded
    MSB-first by hand in six live places — two `frame_core.h` doctests that
    generated four more in `wfm.pyi`, two tests, a docs page and the link
    demo — which is exactly the hazard `ccsds_tm_asm_bits` exists to remove.
    All six now call `ccsds_asm_bits()`.

- **`make test-tsan` — the threaded C tests under ThreadSanitizer, where a
    data race fails rather than prints.** The companion to `make test-ubsan`,
    and the gate that holds the CCSDS RS fix above.

    Scoped to the threaded tests by a **ctest `-R` pattern, not a hand list
    of binaries**, so a new threaded test named for what it is gets picked up
    with no edit here — the same reasoning `check_bench_coverage` applies to
    benchmarks, for the same reason: a list maintained by hand goes stale
    silently. It fails when the pattern matches nothing, because an empty
    result set is not a pass — the trap the glibc and tarball gates were both
    caught by. `TSAN_OPTIONS=halt_on_error=1` for the reason `UBSAN_OPTS`
    already gives: without it the sanitizer prints and the suite still
    passes.

    `native/tests/test_ccsds_tm_rs_race.c` is its first case, and is a
    separate binary on purpose — the race is on the FIRST call, so a process
    that has already encoded anything cannot reach it. Eight threads released
    from one barrier must all derive the same `g(x)` and the same parity.
    Reverting the fix makes TSan report `data race ... in rs_init` and the
    target go red.

    It earned its keep immediately: the first run caught `bench_buffer_core`
    linking no `libm`, which a Release build hides by folding the `sqrt` away
    and a Debug build does not.

- **`track` gains a characterization tree, and the M-PSK pull-in envelope
    becomes a curve anybody can re-run**
    ([#849](https://github.com/doppler-dsp/doppler/issues/849)).
    `src/doppler/track/tests/characterization/pull_in/` sweeps the success
    fraction against multiples of each loop's own acquisition bound, across
    every constellation order and two oversampling ratios, composed entirely
    from the shipped harness.

    It existed as dated prose in three docstrings before this, and re-derived
    by nothing — which is how two findings came to be filed against the
    receiver for behaviour that was really a test seeded past the bound
    (#843, both retracted). The docstrings now cite the subject instead of
    quoting numbers.

    Two things the sweep establishes beyond the shoulders: the collapse
    multiple does **not** move with `sps` (identical rows at 8 and 16), which
    is the check that the bound really is stated in cycles per symbol; and it
    barely moves with `m` (4/4/3), which is the `1/m` being carried correctly
    — a missing `m` would spread the collapse fourfold across the orders.

- **The Viterbi decoder resumes from a blob, like every other stateful
    object.** `viterbi_state_bytes` / `viterbi_get_state` / `viterbi_set_state`
    carry the path metrics, the traceback ring and the cursor into it, so a
    decode split anywhere and restored into a *fresh* decoder produces the same
    bits as one uninterrupted pass. A decoder is a link in a chain — behind the
    receiver, in front of the R-S decoder — and a chain is checkpointable only
    if every link is; one that is not is enough to make elastic resume
    unavailable for everything it sits between.

    **`fill` travels, because it is part of the answer rather than
    bookkeeping.** A decoder resumed inside its first `depth` bits still owes
    its traceback and must emit nothing for bits it has not earned;
    `viterbi_decode_max_out` reads `fill` to say so. A blob that dropped it
    resumes a decoder that invents them, and a split taken in steady state
    cannot see the difference — so the test takes two, one on each side of
    `depth`.

    **A size match is not a configuration match.** The code and the depth are
    configuration, restored by `viterbi_create` rather than carried in the
    payload — but they are *stamped* in it and compared, because two codes with
    the same `k` and `n` differing only in a polynomial or in `invert` produce
    blobs of identical length. The envelope's own size check sees no
    difference between them, and reinterpreting one as the other yields a
    decoder that is confidently wrong instead of one that refuses.

    Eight guards, each proven by sabotage and each reddening in the section
    meant to catch it: dropping `fill` shortens the split stream, dropping the
    cursor or either buffer breaks bit-exactness, and removing the code
    comparison lets an uninverted-CCSDS blob restore into a CCSDS decoder.
    Closes #824.

- **wfmgen can generate a coded waveform, and a CCSDS CADU is a configuration
    of it.** Four new flags on `--type bits` — `--rs-depth I`, `--randomise`,
    `--asm` and `--conv` — apply channel coding as **stages over the frame's
    fields**, and they do not all cover the same bits.

    That asymmetry is the point rather than a detail. A marker, a preamble and
    a sync word are things a receiver *finds*, so all three must look the same
    in every frame: the outer code and the randomiser reach over the data
    group only, and the inner code reaches over everything. CCSDS states that
    rule for its own ASM (10.3.4: *"The ASM was not randomized"*), and the
    reason it gives is exactly as true of doppler's preamble and sync word —
    so it generalises rather than being special-cased.

    Set all four with a 223·I-octet payload and no preamble or sync word and
    the result is a **CCSDS CADU**. It is a configuration these flags reach,
    not a mode they switch into.

    Verified against the shipped encoder rather than against itself: the CLI's
    output is bit-identical to a description assembled through
    `wfm_frame_assemble`, which is byte-identical to `ccsds_tm_frame_encode`
    across five configurations — so it inherits everything that encoder is
    falsified against (figure 9-1's marker, the published randomiser prefix,
    Annex G's generator, the inner code's impulse response).

    **The record carries it**, and that was a defect worth catching before it
    shipped: a record is what makes a capture reproducible, so a stage the
    record dropped would be a capture nobody could rebuild — and the omission
    would read as a plain uncoded waveform rather than as missing
    information. `--record` now emits the four keys (only when set, so an
    uncoded record is unchanged), `--from-file` reads them back, the schema
    declares them, and a round trip is asserted end to end.

    Refusals rather than surprises: `--rs-depth` outside 4.3.5.1's
    `{1,2,3,4,5,8}` is rejected, and a payload off the `223·I` grid is refused
    rather than padded — virtual fill is not implemented
    ([gh-813](https://github.com/doppler-dsp/doppler/issues/813)), and a
    silently padded codeblock is the wrong length for the receiver it was
    aimed at. Any coding flag also *frames* the waveform, as `--sync` does: a
    CADU carries neither a preamble nor a sync word, so a source coded but
    unframed would have emitted its payload with no coding at all.

- **[Measuring a Receiver](docs/dev/contributing/measuring-a-receiver.md) — the missing
    HOW.** `docs/design/rx-test.md` says why the receiver harness is shaped as
    it is, `native/tests/README.md` is the reference for the family, and
    `docs/dev/contributing/validation.md` is the certification process — but nothing said
    *"you have a receiver, here is how you get a number you can defend"*.

    The page is the task path: give the battery an adapter rather than writing
    a harness (eleven entries, all of which a receiver worth measuring already
    exposes); read the four metrics **together**, because they fail
    differently and the disagreement is the diagnostic; read a refusal as a
    result rather than a failure, including the case where it is about your
    lock indicator rather than your receiver (#791); gate on an interval's
    limit and never on a point estimate; and check that your gate can fail,
    because one that has never been seen to is not evidence (#796). It ends
    with the seven traps that were each paid for once, naming the header that
    owns each so a reader lands on the reasoning rather than on a rule.

    `native/tests/README.md` gains the two family members its table was
    missing (`dp_frame_test.h`, `dp_rx_test.h`) and a section on the family
    now testing itself — including the two techniques worth copying: assert
    that an assertion FAILS by observing the counters rather than an exit
    status, and capture stderr rather than muting it so the diagnostic is
    pinned too.

- **`MpskReceiver` is certified — the 11th object.** 33/33 limits hold, 5
    findings, 2 still open (F4, F5). The report is
    `src/doppler/track/tests/validation/mpsk_receiver/results.md`, generated by
    its own `validate.py` and reachable from
    [the validation log](docs/dev/contributing/validation-log.md).

    What a caller gets from it: BPSK and QPSK track `ber_theory_ser` within
    10× from 8 dB Es/N0 and EVM stays within 6 dB of `-(Es/N0)` without ever
    beating it (asserted in **both** directions — an EVM under the bound is a
    broken measurement, not a good receiver); **8PSK does not reach the bound
    below ~16 dB** (F5); the certified rate envelope is **`sps <= 24`**,
    because at 31.7 the receiver locks and still misses the bound (F4); an
    irrational `sps` costs nothing inside that envelope; and a false lock at
    `Δf = k·F/M` is invisible to every metric the receiver computes, so defend
    against it with an external reference or a sync word rather than with
    `lock` (F2).

    It scores through **`BerMeter`**, the shipped meter, rather than a
    hand-rolled estimator — the difference being that `BerMeter` can *refuse*.
    The estimator it replaced searched M rotations × 81 lags for the minimum
    error rate, so a record with no alignment at all still produced the best
    of 324 tries: a measurement that cannot refuse will invent a finding
    rather than decline one. Two earlier results did not survive the swap and
    are now printed as refusals (8PSK at 8 and 12 dB, and `sps = 31.7`).

- **`MpskReceiver`'s claim inventory, and the C tests for what it found
    absent** (`docs/dev/contributing/validation.md` step 1→2). Reading the header as the SSOT
    and grepping `test_mpsk_receiver_core.c` for each prose claim turned up
    five with **zero** mentions anywhere in the file. Four now have tests:

    - **An irrational `sps`.** The header's headline claim — a modem "at
        **any** input rate", where "17.33389 is equally valid" — and every test
        ran at `sps = 8.0`, so no symbol boundary had ever fallen between
        samples. Now measured at the header's own 17.33389: the output count
        tracks the integral of the rate to 2%, `sps` round-trips exactly, and
        it locks and recovers. Sabotage: truncating `sps` to an integer inside
        `create()` reddens both the round-trip **and the lock**, so this
        exercises the irrational path rather than merely accepting a double.
    - **`num_phases` a power of two, `bn_agc_ratio` in (0, 1).** Neither had a
        reject case on this twin, though the real twin pinned the ratio.
    - **The stable false lock at `Δf = k·F/M`** — what `design/mpsk.md` §2.1
        calls Mode 1's "one quiet failure". Pinned as *behaviour*, not asserted
        away: the test requires a healthy lock statistic to **coexist** with a
        completely wrong tracked frequency, which is precisely what no
        self-referenced metric can separate.

    One result worth recording: **deleting the receiver's `num_phases`
    power-of-two guard changes nothing** — `RateConverter_core.c:830` carries
    the same check and the front end is built first, so the receiver's copy is
    fail-fast (it produces the named `create_error_message` rather than a bare
    NULL from a composed core), not the enforcement. A reject test is exactly
    where that hides, because the assertion is true either way; only the
    sabotage distinguishes "this guard works" from "something else catches
    it". The `bn_agc_ratio` guard, by contrast, is the sole enforcer and
    reddens when removed.

- **`track.ContinuousMpskReceiver` — the continuous flavor, and nothing
    waits.** A **view** over `MpskReceiver`, not a second type: same core, same
    state, the identical 25-member surface, and only the constructor differs —
    which is the axis that makes something a flavor in this project (a
    difference in *method signature* is what makes the real-input twin a
    separate type instead). Nothing is removed; `MpskReceiver` still reaches
    every knob.

    ```python
    ContinuousMpskReceiver(m=2, sps=8.0)      # BPSK, continuous
    ```

    **There is no handover, no warmup, no lock gate and no timing gate.** The
    NDA M-th-power error steers the LO from the first output to the last. That
    is a reliability argument rather than a simplicity one: there is no state
    in which the receiver can be wrong about which mode it is in, because
    there is one — no declaring on garbage, no drop-back that never fires, no
    metric that has to be trusted before the loop may act
    (`docs/design/mpsk.md` §2.1).

    What it pins, and why none of them is a choice here: `acq_to_track = 0`
    (the handover **is** the gate this flavor removes); `nda_tap = strobe`,
    the only tap whose lock statistic means what the derived `lock_thresh`
    says; `agc = 1`, which is load-bearing rather than optional; and the five
    gh-644 parameters as `0`, which is a *request* for the derived answer, not
    an omission.

    **The tap was `mf_in` in an earlier draft of this entry, and measurement
    moved it.** `mf_in` is timing-independent, which is a real argument for a
    receiver with no gating — but its update rate is `bank_sps`, faster than
    `Rs`, and `docs/design/mpsk.md` §2.1 lists exactly what follows: the lock
    EMA's `α` is per-*update*, so at a fast tap the metric's memory is shorter
    in symbols and its looks are correlated, breaking the independent-look
    assumption its threshold is derived from. §2.1 claims this flavor cannot
    have that defect; pinning `mf_in` is what gave it one. Measured
    ([#791](https://github.com/doppler-dsp/doppler/issues/791)): `mf_in`
    demodulates on the matched-filter bound — EVM −7.32 dB against `strobe`'s
    −7.31, within 0.07 dB at `sps` 4/8/16/32 — while its lock statistic
    settles at 0.23–0.51 against a `lock_thresh` of 0.4999, so the receiver
    worked and its own lock readout said otherwise.

    A flavor whose headline is that *no metric has to be trusted before the
    loop may act* must not ship the one metric it still reports in a state
    where it cannot be. At `strobe` the discriminator's clock **is** the
    symbol clock, so §2.1's defects collapse rather than being repaired — the
    C test asserts that as `updates_per_symbol == 1.0`, a property an enum
    check would not have caught. What is given up is timing independence:
    `strobe` couples the discriminator's input quality to the timing loop's
    convergence. That is a **coupling, not a gate** — it steers from its first
    strobe whether or not timing has declared — and the timing-independent
    taps stay one `MpskReceiver(...)` call away. Normalising the lock
    statistic for the tap's update rate is what would make `mf_in` pinnable
    here.

    `lock_thresh` is **excluded rather than defaulted**: with no handover it
    gates nothing, so it is telemetry. It stays readable — `rx.lock_thresh`
    reports the derived `0.4999` — and stops being settable, which is the
    honest shape for a number that no longer controls anything.

    Evidence is C-first and non-vacuous. The C test pins the construct-time
    values **and** runs the receiver, because "there is no handover" is a claim
    about a receiver that has run; a handover-enabled twin on the *same*
    stimulus is carried as the control, without which `tracking == 0` would be
    equally satisfied by a signal that never locked. Proven by sabotage:
    pinning `strobe` instead of `mf_in` takes the tap and `tap_timed`
    assertions red, and pinning `acq_to_track = 1` takes both the construction
    check and the post-run `tracking == 0` red.

- **The receiver instrument — one harness, every receiver.**
    `native/tests/dp_rx_test.h` measures a receiver; `native/validation/rx_battery.c`
    is the standard battery run on `MpskReceiver`, and the adapter in it is the
    entire receiver-specific fork. A second receiver design costs an adapter
    and nothing else, which is `docs/design/rx-test.md` goal 6 — two receivers
    comparable **by construction** rather than by hoping two harnesses agree.

    Five operating points (anchor, acquire, doppler, runburst, oversampled),
    each reporting SER, EVM, blind M2M4 SNR, implementation loss against the
    matched-filter bound, the timing error's peak and rms, and acquisition time
    in units of the loop's own bandwidth.

    **A refusal is not a failure.** `dp_rx_run()` declining to report a number
    it cannot defend is the design working, and it prints rather than counting
    against the gate. `--check` gates that every point *claiming* to be
    measurable produces a defensible record — including the `sane` check that
    catches an EVM beating the bound — and deliberately does **not** gate the
    loop numbers as values: a pull-in ceiling moves with the record length
    allowed, so pinning one would pin the observation window rather than the
    receiver.

    The battery's five construction parameters are `0` on purpose: it states
    the link and lets the object derive what it already knows (gh-644).

- **`MpskReceiver` derives the construction parameters that are not design
    axes, on both faces** (gh-644, `docs/design/mpsk.md` §8.1). Five of its
    seventeen parameters are a constant, a false-alarm probability and two
    rates the object already knows, so it computes them and reports them back:

    ```python
    MpskReceiver(m=4, sps=8.0, bn_carrier=0.01, bn_timing=0.01)
    # m_out 8 · zeta 0.7071067811865476 · num_phases 64
    # lock_thresh 0.4999 · bn_agc_ratio 0.05
    ```

    **Zero means derive**, which is what keeps it additive: every one of these
    validators previously REJECTED zero, so no working call site can be relying
    on it, and a caller who wants to pin one still passes a value. The
    derivation runs before the validation, so a derived answer faces the same
    guards a supplied one does. `zeta`, `num_phases`, `lock_thresh` and
    `bn_agc_ratio` join `m_out` as readbacks — without them, `0` is an
    instruction whose result nobody can see.

    `num_phases` moves 1024 → **64**, the measured saturation point: a 16×
    bank for no measurable gain, and it changed nothing across 117 C and 334
    Python tests, which is the point.

    §8's real-twin `m_out` rule was wrong where deriving helps most — it gave
    `min(8, 2·floor(sps/4))`, which `mpsk_receiver_r_create()` **rejects** at
    `sps = 8` and `sps = 16` against its `sps > 2·m_out` bound. The rule is now
    stated against the CONSTRAINT rather than the rate, and both twins call one
    function instead of keeping two in step.

    §8.1's minimal call is a **complete program** the C doc-fence gate compiles
    `-Werror` and runs, and it checks the five derived numbers itself rather
    than printing them for a reader to trust; the Python face states the same
    five as a doctest. A section whose whole claim is "these are computed for
    you" should not assert the results in prose — both were proven to go red on
    a wrong number before this landed.

    The five readbacks are pinned in **C** as well, on both twins, at their
    derived values and at supplied ones — the derivation is a fallback, not a
    policy that overrides a caller. The expectations are literals rather than a
    second call to `mpsk_rx_derive_m_out()`, since an expectation computed by
    the code under test agrees with it by construction. That the object merely
    CONSTRUCTS is not the test: zero used to be a rejection, so "it built" is
    equally satisfied by a receiver that quietly kept the zero. The real twin
    also pins the **refusal**: behind the halfband, `sps = 4` leaves a strict
    bound of 2, no even `m_out ≥ 2` fits under it, and `create()` returns NULL
    rather than clamping to a receiver whose detector has nothing to detect
    with.

- **`mf_in` now works on `MpskReceiverR` too — the restriction was unwired
    code, not architecture.** `ddcr_state_t` carries the same
    `RateConverter`, so `ddcr_get_bank_sps()` is a delegate and the tap
    threads through `ddcr_execute_ctrl_push_tap2()` exactly as on the complex
    path. Measured, `bank_sps` is IDENTICAL on both types at every rate ratio
    (2.0000 / 1.5625 / 2.4414 at sps 8 / 200 / 10000) — it is symbol-relative,
    so the halfband's 2:1 is absorbed by the plan; only `lo_sps` differs,
    which is why that was always a separate parameter. The real receiver now
    acquires on `mf_in` at every ratio (0.987 / 1.003 / 1.000 of a known
    offset). `mpsk_rx_config_carrier()` is public for the same reason it is
    called twice on the complex path: the bank rate arrives after
    `mpsk_rx_loops_init()` has already sized the filter.

- **`rx_nda_tap.c` characterises the carrier loop's DYNAMICS, not just its
    acquisition.** Two additions, both gated against closed forms rather than
    recorded numbers — a gate fitted to its own output cannot fail for the
    right reason.

    **Frequency ramps.** The steady-state phase lag under a Doppler rate,
    checked against `2*pi*r/wn^2` at three ramp rates on every tap. This is
    the only gate here that can rank loops which all acquire, and it is what
    found gh-765.

    **Cold-start time.** `lock_time` against the loop filter's own `5/bn`
    settling budget, with a floor as well as a ceiling — a detector declaring
    at symbol 0 would otherwise "beat" the budget while reporting nothing.

    Also gated now: joint phase/timing acquisition from cold (a carrier offset
    AND a half-symbol timing offset), including with the data modulation
    removed entirely and with the timing loop disabled — the case the
    timing-independent taps exist for, since the Gardner TED needs transitions
    and has none.

- **`lock_time` — the acquisition time, as a number.** Symbols from reset to
    the FIRST carrier-lock declaration, or -1 if the receiver has not locked;
    on both receiver types. Dated by the same hysteretic detector `locked`
    reports, so the two cannot disagree, and only the first declaration is
    dated — a drop and re-acquire does not restamp it. In SYMBOLS, not seconds:
    `bn_carrier` and `bn_timing` are both normalised to the symbol rate, so a
    settling budget quoted in symbols is comparable across every input rate.
    Previously a caller had to poll `locked` in a loop to learn this.

- **Receiver nomenclature is fixed, and the architecture figure follows it.**
    `docs/design/mpsk.md` now names one block one way — LO, MIX, DEC, AGC,
    MFR, PED, PLF, TED, TLF — with a glossary, and its figure groups by
    **clock** (the rate the plan fixed, vs the rate the timing loop stretches)
    rather than by component. LO and MIX are drawn apart for the first time:
    the LO's *rate* is fixed and it is its *frequency* PLF steers, which one
    box hid.

- **The pre-matched-filter tap is `nda_tap = "mf_in"`, not `"preterm"`.** It
    names the node — the MFR's input — instead of a region, which is what
    "pre-terminal" and "pre-MF" both were: the LO's output and DEC's output
    are equally "before the matched filter". It also makes the three taps one
    taxonomy instead of a list, `mf_in` / `mf_out` being the two sides of the
    matched filter and `strobe` the gated subset of `mf_out`. The C enum is
    `MPSK_RX_NDA_TAP_MF_IN`; `mpsk_rx_push_preterm` is `mpsk_rx_push_mf_in`
    and the `pre_sps` field is `mf_in_sps`. `RateConverter` keeps *pre-terminal*
    for the same node, which is correct at that layer — a plain cascade's
    terminal stage is not a matched filter, so it has no `mf` to be the input
    of. Renamed before release, so no shipped spelling changes.

- **`nda_tap = "mf_in"` — the pre-matched-filter NDA carrier tap.** Reads the
    M-th-power discriminator from the cascade's pre-terminal node: after every
    integer stage and after the AGC, but ahead of the matched filter, so it
    needs no symbol timing and carries none of the matched filter's group delay.
    `docs/design/mpsk.md` §3.3 specified this tap and named the existing
    `lo_arm` as a hand-rolled approximation of it. Complex-input only —
    `MpskReceiverR` publishes no bank rate, so it has no such node and
    `create()` refuses the tap rather than silently mis-sizing the loop.

    Additive on every existing path: `RateConverter_execute_ctrl_push_tap()`
    publishes the node and the old `_push` becomes a NULL-tap wrapper, so every
    current caller is bit-identical.

- **`native/validation/rx_nda_tap.c` — the four NDA taps, measured.** Nothing
    in the tree exercised any tap but `strobe`: every C and Python test
    constructed `MPSK_RX_NDA_TAP_STROBE`, so three of the four were prose. This
    harness scores each tap on the fraction of a **known** frequency offset it
    actually removes, across rate ratios from sps=8 to sps=10000.

    The metric is deliberate. Reading `norm_freq` back at the design centre —
    the obvious experiment — **cannot rank these taps and ranks them
    confidently anyway**: at zero offset the correct answer is zero, so a
    carrier loop that never steers scores better than one that works. The
    harness reproduces that table as a labelled counter-example next to the
    real one.

- **just-makeit pin 0.60.2 → 0.61.0.** Adopted for **gh-998**, the only one of
    the three that moves a file here: a composer source's project-written
    straight-C seams (`[module.X.source.generates] bridge_fn` and each
    `[[module.X.source.computed]] fn`) were declared *only* as `extern` lines
    inside the generated binding, so no other translation unit could see them —
    a C test or benchmark could reach a signature jm already owns only by
    writing a second copy of it, which is the duplication the generated
    declaration exists to prevent. They are published now in a generated
    `native/inc/wfm_compose/wfm_compose_bridge.h`, included by
    `wfm_compose_ext.c` instead of re-declared there, so there is exactly one
    of each. The definitions are untouched: still doppler's hand-written C.

    **gh-994 and gh-996 produced no codegen change here, which is the
    interesting part.** gh-994 is the one this repo provoked: an object's
    Python surface is written as `[[<obj>.methods]]` entries, doppler names one
    of them `reset` in 28 places (27 `objects/*.toml` fragments plus one in
    `just-makeit.toml`), and that used to emit the built-in's body *and* the
    method's stub into a create-only `_core.c` — `redefinition of   '<obj>_reset'` on a brand-new object, at the moment a contributor is
    trusting the generator rather than reading it.

    Checked rather than assumed, because the guess went the wrong way: all 27
    are the **naming** kind, not the replacing kind — `arg_type = "void"`,
    `return_type = "void"`, no `params`, so the prototype they imply is
    byte-identical to the built-in's and jm keeps the built-in, whose body
    restores the declared defaults. They carry a `doc` and nothing else, which
    is exactly why they were written. So the fix changes nothing for the
    existing objects and protects the next new one — which is where the
    redefinition actually bit.

- **`doppler.wfm.Frame` — the frame descriptor, reachable from Python.** The
    measurement half of the frame story shipped first (`wfm_frame_t` in C, and
    `doppler.ber.FrameMeter` with a Python face); the descriptor half did not,
    so a caller with a capture could accumulate frame outcomes but had no way
    to *produce* one. `Frame` closes that: `bits()` materialises
    `[preamble x reps | sync | payload | CRC]`, `layout()` returns the field
    offsets as a `FrameLayout` record, and `crc_ok(rx_bits)` scores a received
    frame using **no payload truth at all** — which is what makes a frame error
    rate measurable on a real capture, and what still catches a false lock that
    EVM and M2M4 read as clean.

    It is the **same descriptor the generator uses**, which is the point: a
    framed `Segment` and a `Frame` built from the same fields now agree symbol
    for symbol, so a receiver is scored against the frame that was actually
    sent rather than one reassembled from parts. Every layout decision stays in
    `wfm_frame.c`; this object is lifecycle and delegation only.

    Each field is a literal array **or** a PN/Gold descriptor a receiver can
    regenerate — truth for a long record without a long array. The three
    `wfm_seq_t` cannot nest across the C ABI, so they are flattened with a name
    prefix (`preamble_*`, `sync_*`, `payload_*`), and a field with an empty
    array and zero length is **absent** — `wfm_seq_t`'s own convention. An
    unbuildable descriptor (empty geometry, a literal with no array, a PN with
    no register width) raises `ValueError` at construction rather than
    producing a frame with a hole in it.

- **just-makeit pin 0.60.1 → 0.60.2.** Pure bugfix upstream, and three of the
    four fixes are about doppler's own wiring report — this repo is where they
    were found. `jm status` now reads
    `add_library(NAME SHARED|STATIC $<TARGET_OBJECTS:X>)`, which is how
    `libdoppler_stream{,_static}` gets its cores, so a core shipped in a
    library other than `doppler_lib` is no longer reported UNWIRED
    (just-buildit/just-makeit#991). It also validates wiring keys in
    `status_allow` instead of skipping them: a `CMakeLists.txt:<core>` entry is
    a FINDING key, not a path, and 0.60.1 called those exemptions stale while
    printing `[status_allow]` beside the finding they were suppressing — advice
    that pointed at the blanket `CMakeLists.txt` spelling gh-984 exists to
    avoid.

- **A frame is now a waveform property, not a DSSS one.** `--acq-code` /
    `--acq-reps` / `--sync` / `--crc` describe the bit layout
    `[preamble x reps | sync | payload | CRC-16]`, and they now reach the
    samples for `--type bits` (with `--modulation bpsk|qpsk`) on all three
    faces — the `wfmgen` CLI, `doppler.wfm.Synth`, and `Segment`/`Composer` —
    through the one construction path they already share. The layout comes from
    `wfm_frame_bits()`, the same descriptor the DSSS assembler and the receiver
    read, so TX and RX cannot drift.

    The frame **cycles** to fill the requested length, exactly as a plain
    pattern does: one description yields a multi-frame record and the repeat
    count stays out of the descriptor. (A DSSS burst keeps its intrinsic
    length.)

    `--record` carries the frame for an unspread source too, so `--from-file`
    rebuilds it byte for byte. `add_dsss_fields` was type-gated in both
    directions, which meant a framed `bits` record wrote no frame at all and
    then read back as a different waveform from a file that looked complete.

- **The named starter frame set, and one receiver measured end to end on it.**
    `native/tests/dp_frame_test.h` ships the five frames of the design's §7.4 —
    `RX_FRAME_NONE` / `BURST` / `CONT` / `GOLD` / `ACQ` — as `wfm_frame_t`
    values. One struct, five configurations: no per-name code path, no second
    layout, five rows of a table materialised by the one `wfm_frame_bits()`.

    `native/validation/rx_frame_fer.c` (`validate_rx_frame_fer`, in `ctest`)
    then runs the whole §8 sequence: a named frame → `wfm_frame_bits()` →
    `wfm_synth` → `MpskReceiver` → `ber` + `snr` + `frame_meter`. **It is the
    first FER measured on a receiver rather than on synthetic outcomes, and the
    first run to produce BER, EVM, M2M4 and FER together.** It owns no pulse,
    no estimator, no level convention and no random number generator at all —
    the noise is `wfm_synth`'s AWGN at a requested Es/N0. At the SER = 1e-3
    anchor the receiver measures 0.11–0.15 dB of implementation loss on a
    framed, library-generated stimulus.

    This closes §5.3, the inventory finding that the framed generator and the
    frame-aware measurer had never met: `dp_ber_marker_t` has modelled a
    PERIODIC marker since it was written and nothing had ever supplied one,
    because the only thing that could emit a frame was the DSSS spreader. The
    record alignment is now the sync word repeating at the frame period — which
    also means a periodic marker costs no leading block, since every occurrence
    is excluded from scoring uniformly instead of 256 symbols being given up.

    **The §6 open question is measured**, and the answer is not "13 is too
    short". At Es/N0 6.8 dB a per-frame Barker-13 confirmation misses 0.845 of
    frames [0.755, 0.942] while PN-127 misses none of 120 [0, 0.038] — yet
    the SAME Barker-13 acquires the record without difficulty, because there it
    is periodic and ~130 occurrences combine non-coherently. A short sync word
    is an acquisition aid, not a per-frame confirmation.

    Two configurations are REFUSED rather than reported: `RX_FRAME_ACQ` before
    a single burst runs (a preamble has no payload, so there is no BER, no EVM
    window and no CRC — and 60 bursts would have ended in a *settling* verdict,
    the wrong diagnosis), and `RX_FRAME_NONE`'s FER as `n/a` rather than 0.0,
    because an unprotected stream having no truth-free error detector is the
    gap the frame closes.

    Sabotage found two gates that would have been vacuous. Hard-wiring
    `sync_ok = 1` left every gate green — an invented miss rate is still
    self-consistent with the FER computed from it — so the run now asserts the
    detector was observed both to accept and to refuse. And the FER anchor at a
    ×1.5 tolerance still PASSED with the CRC check sabotaged to always fail, so
    it is asserted on `frame_meter`'s own lower limit at ×1.15, and skipped
    with a printed reason when most of the predicted FER is a sync miss the
    harness would merely be handing back to itself.

- **`FrameMeter` — the fourth metric, and the only truth-free one that catches
    a false lock.** `doppler.ber.FrameMeter` accumulates frame outcomes across
    a record and reports a frame error rate and a sync MISS rate, each with the
    same exact Gamma/chi-square interval `BerMeter` uses.

    What it is FOR is the gap the previous commit measured: EVM and M2M4 need
    no truth and read a stationary-but-wrong constellation as clean, BER sees
    it but needs truth and a trustworthy alignment, and a CRC-checked frame
    needs no payload truth at all. It either checks or it does not.

    The counting rules are the content, because each is a convention that
    fails silently. A frame whose sync was never detected IS an error — score
    only the frames you managed to find and the FER *improves* as the receiver
    gets worse at finding them. A frame carrying no CRC is NOT an error when
    its sync was found, or the number measures the frame format rather than
    the receiver (`crc = -1` is exactly what `wfm_frame_crc_ok()` returns, so
    it passes straight through). And the two failure modes stay separately
    countable, because "the sync is too short at this Es/N0" and "the
    demodulator makes bit errors" are different repairs.

    It stops on an ERROR target, like `ber_meter`, and that is not cosmetic:
    `ber_confidence` is exact for inverse-binomial sampling, so a
    fixed-frame-count stopping rule would be the wrong model for the interval
    it hands back. The header says so rather than leaving it to be assumed.
    Mutation-tested both rules; the state triplet round-trips a record across
    a process boundary and rejects a clobbered envelope, from C and Python.

- **A frame descriptor: one bit layout, read from both ends.** `wfm_frame_t`
    (`native/inc/wfm/wfm_frame.h`) describes a frame as
    `[preamble × reps | sync | payload | crc]` in BITS — spreading, pulse
    shaping, oversampling and SNR stay `wfm_synth`'s job — with
    `wfm_frame_layout()` / `wfm_frame_nbits()` / `wfm_frame_bits()` /
    `wfm_frame_crc_ok()`. Every field is a `wfm_seq_t`, so a Gold sync is a
    configuration rather than a feature, and the generated kinds call
    `pn_create()` / `gold_create()` rather than adding a generator: a receiver
    regenerates a long record's truth from a handful of numbers instead of
    carrying the array.

    `wfm_frame_crc_ok()` is the point of the exercise. It needs the layout and
    the received bits and **no payload truth at all**, which makes a frame
    error rate the one metric usable on a real capture that still catches a
    false lock — the failure the previous commit measured EVM and M2M4 going
    blind to.

    `wfm_frame_dsss_chips()` keeps its signature and now builds its frame
    through the descriptor and spreads it, so the layout (and the CRC's
    position, width and bit order) stops being expressed twice. The existing
    DSSS round-trips are the regression test the design asked them to be —
    `test_wfm_dsp`, `test_burst_demod_core`, `test_async_dsss_receiver_core`
    and 703 Python wfm/dsss tests all pass unchanged.

    **The build split follows the FUNCTION, not the file.** The DSSS burst
    assembler moved out of `wfm_dsp.c` into `wfm_frame.c`, because assembling a
    frame is what it does — and because `wfm_dsp_core` is spreading and RRC
    taps, which every receiver links for a matched filter and which must not
    start dragging in a Gold LFSR. Measured before choosing: folding them
    together put `gold_create` into eight targets, four of them jm-generated.
    The four consumers that genuinely assemble frames declare it in the
    manifest (`depends_on … link = true`), not by hand.

- **The M-PSK harness reports the whole trio, and a false lock proves why it
    must.** `symbol_metrics` returned SER and EVM; M2M4 was never computed, so
    the trio the design asks for was two of three. It now returns a
    `SymbolMetrics` record (`evm_db`, `ser`, `lag`, `m2m4_db`) built from the
    library's own estimators over one window, and the seven call sites name the
    field they mean. The EVM had been recomputed in numpy — it agreed with
    `ber_evm_db` to four decimals, which is what a duplicate looks like right
    up until one of them changes, and it was invisible to
    `check_stimulus_sources.py` because that marker looks for a function while
    this was inline.

    The new test is the argument for truth-referenced measurement, measured
    rather than asserted. A stable false lock at `df = k*Rs/M` (the M-th power
    discriminator sees `M*df = k*Rs`, which aliases to zero, so the loop
    parks) at Es/N0 15 dB:

    | M   | alias | EVM honest | EVM false | penalty | truth-referenced |
    | --- | ----- | ---------- | --------- | ------- | ---------------- |
    | 2   | Rs/2  | -13.43 dB  | -9.88 dB  | 3.56 dB | **refused**      |
    | 4   | Rs/4  | -13.57 dB  | -12.52 dB | 1.05 dB | **refused**      |
    | 8   | Rs/8  | -14.83 dB  | -14.61 dB | 0.21 dB | **refused**      |

    The receiver declares LOCK at every order; the alignment refuses at every
    order; and the penalty the truth-free pair shows **shrinks as M rises**,
    because the alias is `Rs/M`. So the metric that can half-see this at BPSK
    goes blind exactly where the margin is thinnest. The test pins that shape,
    not a single point.

- **`test_snr_core.c` and `test_ber_core.c` — the measurement primitives had
    no C tests.** `snr_m2m4_db`, `snr_data_aided_db` and `ber_evm_db` are
    called by two harness headers, by `test_ratesync_core.c` and by
    `test_async_dsss_receiver_core.c`, which exercises them as tools without
    asserting one thing any of them claims; the `snr` module had no C test at
    all. Every receiver number this project reports rests on them.

    Each claim is taken from the declaration and pinned by known answer —
    QPSK/BPSK at a constructed Es/N0, so the reference is the construction and
    not another estimator — drawing its randomness from `dp_rng_test.h`, which
    `make tests-ssot` requires and which caught the hand-rolled xorshift and
    Box-Muller these files started with. Then proven by mutation. Seven mutations, each
    watched going red: `log10`→`log2` and a wrong fourth-moment weight in
    M2M4; assuming unit amplitude and skipping the sign-strip in the
    data-aided form; `20log10`→`10log10` (the I-only convention, worth 3 dB),
    not estimating the constellation rotation, and relaxing the 20-symbol
    no-lock floor in `ber_evm_db`.

- **`test_ber_meter_core.c` was a 24-line jm scaffold** — create, reset,
    destroy — for the object that ships the entire alignment decision, and
    `ber_align_detect` appeared in exactly one place tree-wide. It now pins
    what the primitive claims: a planted lag and ABSOLUTE rotation recovered
    exactly across four lags and four noise levels (the gate estimates its
    floor from the off-peak lags, so it needs no Es/N0); `ok = 0` rather than
    a plausible lag for a marker too short to identify one; repeats combined
    non-coherently buying that gain back; a peak on the edge of the search
    refused as saturated; an unrelated truth sequence refused outright; and
    marker symbols excluded from `score()`, landing in `skipped`. Removing the
    false-alarm gate or shifting the detected lag by one turns it red.

- **The self-referenced EVM flatters at low Es/N0, and now says so.** Writing
    the known-answer test asserted `EVM[dB] == -(Es/N0)[dB]` at 6 dB and it
    FAILED, reading -7.06. The estimator is right: it scores each symbol
    against the stream's own hard decision, so a misdecided symbol is measured
    against a nearer constellation point and contributes too small an error
    vector. The bias tracks the SER — measured on QPSK, 2.45 dB at 3 dB Es/N0,
    1.06 at 6, 0.44 at 9, 0.20 at 12, 0.11 at 15, 0.04 at 21.

    This contradicted a load-bearing sentence in the M-PSK harness ("an EVM
    that BEATS the bound means the measurement is wrong, never that the
    receiver is good"), which is true only above ~12 dB. Both the test and the
    harness docstring now carry the measured table, and the identity is pinned
    tightly from 12 dB up — where every EVM assertion in the tree actually
    runs — with the flattery pinned separately as its own monotone property.

- **just-makeit pin 0.59.1 → 0.60.1, and 38 C symbols across 6 components
    became linkable.** The mpsk certification found `mpsk_map`/`mpsk_demap`
    absent from `libdoppler.a` and root-caused it upstream: jm emitted the
    `target_sources(<pkg>_lib{,_static} …)` wiring **per object, never per
    component**, so a component contributing only module-level functions was
    folded into no library at all. A function-only module was wired only if
    some unrelated object happened to name it in `depends_on` — which is why
    `snr` shipped and `mpsk` did not
    ([just-makeit#981](https://github.com/just-buildit/just-makeit/issues/981)).

    The scope was **four times** what the original report said. Hand analysis
    found 2 components by reasoning about function-only modules; jm 0.60.0's
    new detector found 17 unwired cores, of which 6 actually defined
    out-of-line symbols that no library carried — `arith` (14), `wfm` (10),
    `mpsk` (5), `measure` (4), `util` (4), `filter` (1). `libdoppler.a` went
    from 1227 to 1265 exported symbols on the bump, and the C reproduction
    that used to fail to link now runs.

    Among them were `saturate` and `ema_step` — the shared primitives the
    validation campaign has spent months consolidating call sites onto. The
    C face of that reuse story was unlinkable the whole time. Python was
    unaffected throughout, because each extension links its own core
    directly, which is exactly why nothing noticed.

    0.60.0 also shipped the **detector** for it
    ([gh-984](https://github.com/just-buildit/just-makeit/issues/984)):
    `jm status` reports `UNWIRED` and `DANGLING` components, gated through
    `make drift-check`. **0.60.1 is the patch that made it usable** — 0.60.0's
    readers were anchored at column 1 and scanned only the root, so a core
    declared inside an `if()` was invisible, and `jm apply` *deleted* its
    correct wiring. doppler has all three shapes it missed and is what found
    it ([gh-988](https://github.com/just-buildit/just-makeit/issues/988)).
    Nothing was lost here: the pair it deleted duplicated one
    `native/src/wfmcompose/CMakeLists.txt` already emits under the same
    guard, and all seven of `timing_core`'s exports stayed in the archive
    throughout. The pair is restored.

    doppler keeps **five** deliberate exclusions, each scoped to its
    component rather than to `CMakeLists.txt` — naming the file would exempt
    every core and re-open the hole above. Four (`wfm_compose`, `wfm_sink`,
    `wfm_sink_stub`, `wfmgen`) export nothing out-of-line, so no consumer can
    be missing anything. The fifth is worth stating: `stream_core_obj`
    exports 75 symbols, and all 75 are in `libdoppler_stream.a` — the
    optional second library the stream layer ships as by design. jm's check
    reads only `doppler_lib`/`doppler_lib_static`, so it cannot see a
    project's second library. Every one of the five was checked with `nm`
    before being suppressed.

- **A validation report can no longer record an open finding without
    filing it, or invent a verdict.** Two checks inside
    `Report._self_check`, so `make validate`, `make validate-check` and
    every module's limits test enforce them without naming them.

    A verdict was just a string on the way in while `open_findings`
    matched exact ones, so `"Gap"`, `"GAP "` or an invented `"OPEN"` was
    accepted and then counted as **not** open — a typo made a real defect
    invisible in the executive summary and in the validation log's
    `still open` column at once. The vocabulary had three homes that
    disagreed; it now has one.

    And an open finding must cite the issue tracking it. That is the
    repo's carve-out rule applied where carve-outs actually get recorded:
    a gap living only inside a report is invisible to everyone not
    reading that report. Nine of the eleven open findings already cited
    one, so this codifies a convention rather than inventing a demand;
    the two that did not are now filed rather than allowlisted
    ([#750](https://github.com/doppler-dsp/doppler/issues/750), the AGC
    applying 59.9 dB to a noise floor, and
    [#751](https://github.com/doppler-dsp/doppler/issues/751), the DTTL's
    low-SNR claim), so the gate ships with **zero** exceptions.

    The citation rule is also what catches a *positive* result filed
    under an open verdict — the mpsk report did that for one commit and
    advertised three open findings against one real one — because a
    result that holds has no issue to cite. If it is a result rather than
    a problem, it is a limit, not a finding.

- **The M-PSK constellation is certified — the decision rule every M-PSK
    consumer routes through had no C tests at all.** `mpsk_slice` is the
    library's one hard decision: `mpsk_demap`, `mpsk_diff_demap`,
    `mpsk_receiver_core.c` and `mpsk_rx_loops.h` all decide through it, and
    there was no `native/tests/test_mpsk_core.c`. The only coverage was
    Python, which cannot reach the six `JM_FORCEINLINE` helpers — including
    the decision `ahat` whose `Im(y·conj(ahat))` is the decision-directed
    carrier error a receiver steers on.

    Worse than absent: `test_carrier_mpsk_core.c` carried a private O(M)
    correlation search instead of calling `mpsk_slice`, so the carrier-loop
    test scored against **its own slicer**, free to disagree with the
    library's with no gate able to notice. It now delegates, and the
    equivalence it had silently assumed — nearest in phase equals nearest by
    Euclidean distance on the unit circle — is proven rather than assumed.

    The new C test is built inside-out and every section is a property, not
    a spot value: Gray labelling as a **cyclic** one-bit-per-neighbour
    invariant (the 0/M−1 seam is where a near-miss labelling breaks, and the
    transition a noisy symbol is most likely to make), the slicer against an
    external truth that builds its points from `cos`/`sin` so a broken
    constellation cannot excuse a broken slicer, buffer canaries for the two
    length claims, and the memoryless/sequential pair as duals. Ten
    mutations were applied and each took it red.

- **`~2x` for differential M-PSK is an asymptote, not a constant.** The
    header stated the penalty and nothing measured it, which left a caller
    trading a known carrier-phase ambiguity for an unknown cost.
    `native/validation/mpsk_diff_penalty.c` measures it paired over one
    shared noise realisation — so the seed's luck cancels out of the ratio —
    and anchors the coherent path to closed-form theory, because two paths
    sharing a defect still divide to a plausible 2.0. Measured, 8PSK pays
    **1.44x at 4 dB** Es/N0 and 2.03x by 14 dB, while BPSK and QPSK reach
    the asymptote by ~8 dB. The correction is carried back into the header,
    both Python faces and the design doc; a caller sizing a link at low
    Es/N0 is charged less than the round number suggests.

    `docs/design/mpsk.md` gains **§9**, which specifies the constellation
    primitive the receiver reuses — Gray labelling, the one decision rule,
    why QPSK alone carries a π/4 offset, and what differential mode costs.

- **`LoopFilter` is certified, and `bn` is now measured rather than
    asserted.** The second-order PI filter is embedded by value in seven
    objects — costas, carrier_mpsk, carrier_nda, dll, symsync, ratesync,
    burst_despreader — and every one of them sizes its settling and its
    jitter off the promise that `bn` is the loop's noise bandwidth. Nothing
    in the repository had ever closed the loop and asked what bandwidth came
    out. It does now, two independent ways that share no arithmetic: Parseval
    on the impulse response of the real loop (`Bn = 0.5·Σh[n]²`, exact, no
    RNG and no fitting) and a numerical integral of the derived `|H(f)|²`.
    They agree to six figures across 108 cells.

    The promise holds, with a correction that is a **law** rather than a
    scatter. The delivered bandwidth is always slightly *wide* — never
    narrow, so a caller sizing noise off `bn` is conservative — by a
    fractional excess that collapses onto the single group `bn·t`, with the
    update period dropping out entirely, and whose coefficient has a closed
    form: `Bn/(bn·t) − 1 ≈ 16·zeta²/(4·zeta²+1)²·(bn·t)`. That is not
    fitted; it reproduces the measured coefficient to five decimals at every
    damping. **Solved for the budget a caller wants: keep `bn·t ≤ 0.0112` at
    zeta 0.707 and the bandwidth is within 1%** — every configuration
    shipped in this library already sits inside it. The rule is now in the
    header, in `docs/design/loop-filter.md`, and in
    `src/doppler/examples/loop_filter_bandwidth_demo.py`.

    Settling follows from the same number and is flat in **loop constants**:
    2.25–2.30 to ±5% across a 50× range of bandwidth, which is the first
    direct measurement behind the `5/bn` rule the rest of the campaign sizes
    its windows with, and says the rule is comfortable rather than tight.

    Evidence: `native/tests/test_loop_filter_core.c` grows from one
    undifferentiated block to eleven `§` sections, every one sabotage-proven,
    covering seven claims that previously had no test at all — `steps()` had
    neither a C caller nor a C test, `destroy(NULL)` was never called,
    `reset` checked `kp` but never `ki`, and the "`init` does not touch
    `integ`" contract that all seven embedders depend on (two of them
    *positively*, seeding it to a known carrier offset) was asserted
    nowhere. The gain check no longer re-types the implementation's own
    expression beside it — it asserts Rice's parameterisation, which is
    algebraically identical and textually independent, so a sign or factor
    error has to be made the same way twice to survive.
    `native/validation/loop_filter_noise_bw.c` is the new C sweep and
    `src/doppler/track/tests/validation/loop_filter/` the report: 25 limits,
    10 findings.

- **Every validation report now opens with an executive summary** — a
    derived status line (CERTIFIED / REGRESSED, the limit tally, which
    findings are still open) and three to six authored key takeaways aimed
    at a caller who will read nothing else. Written last, because the status
    counts limits and findings that do not exist until every phase has run,
    and rendered first: `Report.executive()` accumulates into the report's
    `head`, so ordering is a property of `render()` rather than of call
    order. Status is derived so it cannot drift from the body the way a
    hand-written abstract does; takeaways are authored because "what matters
    here" is judgement and no counter produces it. Added to all seven
    reports, specified in `docs/dev/contributing/validation.md`, and enforced against the
    rendered file by `scripts/check_validation_reports.py` — presence,
    position and both parts, each proven by mutation. The section is
    deliberately unnumbered: numbering it `1.` would renumber everything
    below and invalidate the `§2.x` cross-references in seven reports, the C
    tests citing them and the issues filed against them.

- **CarrierNda is certified under the object-validation campaign**, and the
    certification produced a number the composing receiver needs: the
    **seeding rule**. `src/doppler/track/tests/validation/carrier_nda/` holds
    43 asserted limits and 12 findings, and its §2.5 establishes that
    `bn/M` is the scale the loop's whole acquisition behaviour collapses onto
    — nine (M, bn) pairs spanning 4x in each land on one curve of settling
    time against `u = |Δf|·M/bn`, with M dropping out to the third decimal.
    Two regimes meet at `u = 1`: inside it settling does not depend on the
    offset at all (1.76–1.92 loop time constants, since a linear second-order
    loop settles in a fixed number of them whatever the step size), and
    outside it the beat-note term takes over at quadratic cost. Inverted:

    ```
      seed within |Δf| ≤ bn/M  →  settled within 2/bn samples
    ```

    so a coarse acquisition ahead of this loop needs a frequency bin no wider
    than `2·bn/M`. Measured worst case `u = 1.42` noiseless and `0.85` at
    6 dB Es/N0. The rule is carried by `test_carrier_nda_core.c` §16 (three
    claims — the budget, the offset-independence inside the window, and the
    far slower regime outside it that stops the first two passing vacuously)
    and demonstrated end-to-end by the new
    `src/doppler/examples/carrier_nda_seeding_demo.py`, which sizes a
    coarse M-th-power FFT search from the rule and locks in 1.4/bn against
    234/bn cold. Five findings are filed rather than carried in prose:
    gh-732 (the contract needs the seeding number, and does not name the
    arm's absolute pull-in wall), gh-733 (the half-symbol arm's
    `1/2 + 1/(M+1)` coherent gain does not reproduce), gh-734 (the lock
    statistic's H0 spread is M-independent per look and not through the
    arm, which is why the shipped `n_up = 64` — not the threshold — carries
    the false-alarm budget), gh-735 (`carrier_nda_get_nco_freq` is
    unreachable from Python and telemetry), gh-736 (the doc-face parity gate
    compares Examples/Raises only).

- **RateSync is certified under the object-validation campaign.** It owns
    `src/doppler/track/tests/validation/ratesync/`, and `track` gains the
    `test_validation_limits.py` gate the campaign requires, so its 23-claim
    envelope is now asserted by pytest rather than only written down. The
    design rationale it assumes is written off main as
    `docs/design/ratesync-timing.md` — the last residual stranded in the
    abandoned PR #647, rewritten rather than cherry-picked because the TED
    normaliser has since become a construct-time constant (loop state v2) and
    the draft's central argument no longer describes the code.

    The order was header first: 40 prose claims enumerated from
    `ratesync_core.h`, each mapped to `test_ratesync_core.c` as pinned,
    pinned-only-at-literals, or absent. **Twelve new C sections (§8–§19)**
    cover the absent ones — the prime countdown and where its length comes
    from, that one input really can complete two terminal outputs, the DTTL
    detector (previously executed by nothing), the timing loop driven over a
    hand-owned cascade, the `ctrl`-referenced-to-terminal-rate consequence,
    `clipped` in both directions, the `m >= 4` rectangle rule, the loop's own
    state envelope, atomic telemetry attach, `configure` semantics, `max_out`,
    and the `sps == m` edge.

    Every one was proved by sabotaging the implementation and watching it go
    red. **Two of the thirteen sabotages initially stayed GREEN** and the
    sections were rewritten until they failed: the DTTL section passed because
    the two detectors also differ by `ted_scale`, so a dispatch collapsing to
    Gardner still diverged (it now isolates the discriminator with `ted_scale`
    held equal), and the loop-state section passed because with `m = 2` a lost
    strobe phase lands on the right parity half the time (it now runs at both
    parities).

    Measured for the first time: the TED S-curve carries **exactly two zeros
    per symbol with opposite slope**, confirming the header's T/2 parity
    argument, and its normalised slope at lock is **1.015**, confirming the
    construct-time `ted_scale`.

- **`telemetry.Capture` / `MemoryCapture` accept `clock=None`**, which states
    that there is no time base. The C has always read `NULL` here that way —
    the sidecar then omits `fs` and `epoch_real_ns` rather than fabricating a
    rate into a file that outlives the process — and only the Python face could
    not say it. It now can, and the sidecar is verified to omit both keys when
    told `None` and to carry them when given a clock.

    The manifest needed no change: just-makeit 0.53.0 (gh-805 §H) honours the
    `required` key that was already there, and `clock` never set it. **Do not
    add `required = true` to it** — that now restores the refusal.

    Reaching doppler took deleting the two sacred `_ext` fragments and
    re-applying: jm only ever *adds* missing members to a sacred fragment, so a
    changed constructor body stays as written and the fix was stranded upstream
    until the fragments were refreshed.

    **The argument is still not omittable** — `MemoryCapture(tlm, block)` raises.
    Accepting `None` and being omittable are different axes; the stub says
    `clock: Any = ...` and the binding disagrees, which is
    [just-makeit#845](https://github.com/just-buildit/just-makeit/issues/845)
    and stays in `scripts/.init-param-optionality-ignore`. A test pins the
    current behaviour so it fails when that is fixed.

- **`MpskReceiver`/`MpskReceiverR` telemetry reaches the front-end AGC —
    `rx.agc.gain_db` and `rx.agc.level_db`, so the probe set is thirteen
    rather than eleven.** That AGC was the receiver's third loop and the only
    one emitting nothing, which is the awkward part: by `mpsk_rx_agc_bn()` it
    is the SLOWEST of the three by construction, so it — not the carrier or
    timing loop — sets how long the receiver takes to become usable. Its
    settling had to be inferred.

    The forward was not one hop. The AGC lives inside the front end's
    `RateConverter`, which had **no telemetry surface at all**, so the missing
    layer is added: `RateConverter_set_telemetry()` forwards to its one
    instrumented child, and `ddc_set_telemetry()`/`ddcr_set_telemetry()`
    forward to that. Deliberately C-only — `RateConverter_enable_agc()` is not
    exposed to Python either, so a Python `set_telemetry` there could only ever
    register nothing.

    `agc_set_telemetry()` now registers **two** probes. `level_db` is the
    detector's measured level, `10*log10(p_avg)` — the loop's *input*, which
    the integrator drives to `ref_db`. That is the point: it is
    zero-referenced, so settling is readable without knowing the true input
    level, where `gain_db` alone settles to an offset that depends on how loud
    the signal happens to be. `AGC_STATE_VERSION` is unchanged — the new probe
    id fills what was padding, so the blob's size and meaning do not move.

    Two behaviours worth knowing. The AGC probes are **not on the symbol
    grid**: the tap is pre-terminal in the cascade, ahead of the stage the
    timing loop steers (deliberately, so its bandwidth is not coupled to the
    loop stretching the symbol grid), and it emits per gain-update event — so
    compare AGC records against loop records by time, never by index. And with
    `agc = 0` there is no third loop, so the attach registers eleven probes and
    still returns success: a caller should not have to know how the receiver
    was constructed to avoid an error.

    The attachment is held as a *request*, which fixes a latent trap rather
    than only adding one: `_agc_build()` destroys and rebuilds the AGC on a
    re-plan, so a `set_rate()` would otherwise have silently stopped the gain
    trajectory being recorded with no error anywhere to say so. A C test pins
    that, plus the attach-before-`enable_agc` ordering and the whole-attach
    rollback when only the AGC forward cannot fit the probe table. Every claim
    was proved by sabotage.

- **`make glibc-gate` — the glibc floor is now answerable on a dev box.**
    `glibc-check` is pure inspection, so it was only ever as good as the `.so`
    it was pointed at, and only CI could produce an old-glibc one: on any
    modern distro the local build legitimately references newer symbols and
    the check failed by design. It sat in `GATES_DEPS` as a gate no laptop
    could pass, and the answer arrived one push at a time.

    The fix supplies the missing *input*, and does not touch the assertion.
    `deploy/docker/Dockerfile.glibc228` is Debian 10 (glibc 2.28 — the floor
    itself, which is why raising `GLIBC_MAX` means moving the base image, not
    editing a number) plus a build toolchain: no doppler source baked in, and
    never published. `glibc-gate` bind-mounts the checkout into it as the
    invoking user, builds out-of-tree into `build-glibc228/`, smoke-runs the C
    examples, and runs the existing `glibc-check` against the result.

    Its own build dir, deliberately: sharing `build/` would leave a
    Buster-compiled CMake cache in the tree and abort the next local
    `cmake -B build` on the changed compiler. `STANDALONE_BUILD_DIR` is
    overridden for the same reason — it is the one path `test-examples`
    reaches that is not already derived from `BUILD_DIR`. Verified rather than
    assumed: after a run, the host's `examples/standalone/build` still holds
    its own host-path cache and the container's sits under
    `build-glibc228/standalone`.

    The `glibc-228` CI job is now a single `make glibc-gate`. The archive-apt
    sources and the Kitware cmake tarball it used to hand-roll inline in
    `ci.yml` live in the Dockerfile, so CI and a dev box run one definition
    instead of two that could drift. Measured on CachyOS (glibc 2.43): the
    gate build tops out at **2.27**, the host build reaches **2.43**.

- **`native/tests/dp_test.h` — the `dp_*_test.h` family gets the foundation it
    was already standing on.** `dp_state_test.h` had documented its dependency
    for as long as it existed — *"Requires (already present in every
    `test_*_core.c`): the `CHECK` macro"* — and that requirement was met by
    **90 copies of `CHECK` in six mutually incompatible variants**: two arities,
    two failure semantics, and one whose condition was inverted. A shared
    harness resting on a macro every includer redefines is a contract with
    nobody. The compiler said so on the first build: `dp_state_test.h` *calls*
    `CHECK` and stopped compiling the moment the copies went away.

    Both failure semantics are kept, under the names the wider testing world
    uses for the same split, because both were legitimate and converting either
    into the other would not have been a refactor — the checks that follow a
    `DP_REQUIRE` exist precisely because the pointer is known-good by then:

    | macro                           | on failure            | files |
    | ------------------------------- | --------------------- | ----- |
    | `DP_CHECK` / `DP_CHECK_MSG`     | count it, carry on    | 79    |
    | `DP_REQUIRE` / `DP_REQUIRE_MSG` | report and `return 1` | 9     |

    Plus `DP_CHECK_NEAR` — promoted from `test_dp_ber.c`, the one file whose
    numeric failures already printed both values and the tolerance — and
    `dp_nearf` / `dp_near` / `dp_cnearf` / `dp_cnear`, which retire 18 private
    copies of `|a-b| <= tol` and their 36 wrapper macros. Tolerance stays an
    **argument** throughout: `TOL` is defined five times in the suite with five
    deliberately different values (`1e-3f` through `1e-12`), because a CF32
    round-trip and a double-precision spectral estimate do not share an epsilon.
    The comparison was duplicated; the constant was not.

    The counter lives in the header at file scope rather than as a local in
    `main`, because 20 tests call checks from helper functions — which is why
    15 of them had already promoted it by hand.

    89 files, **-994 net lines**. `native/tests/README.md` documents the family
    and where a new helper goes. Only `examples/downstream-jm/native/tests/`
    keeps its own `CHECK`: it is a separate downstream project, which is the
    point of it.

- **`make lint` runs `tests-ssot`.** Two rules, because the convention was
    *already written down* while 90 copies accumulated under it. No test may
    re-define an assertion `dp_test.h` provides or roll its own
    `CHECK`/`REQUIRE`/`EXPECT`/`ASSERT` — the forbidden set is **derived from
    `dp_test.h` on every run**, so a macro added there is covered without
    touching the checker. And no `native/tests/*.c` may end up with **fewer
    assertions** than `$(ASSERT_BASE)` has.

    That second rule is not theoretical. This work was branched before
    `feat(telemetry)` reached `main` and would have dropped **43 assertions
    across three files** — `test_RateConverter_core.c` -20,
    `test_mpsk_receiver_core.c` -12, `test_agc_core.c` -11 — with a completely
    green suite: the files compile, the survivors pass, `ctest` reports 100%.
    Review caught the one file it knew; the other two were only ever going to
    be found by counting. Read the count, not the percentage.

    Deliberate removals go in `native/tests/.assertion-ratchet-ignore` with a
    reason. It is empty, and the intent is that it stays that way — the
    regression above was fixed by rebasing, not by an entry.

- **`dp_tlm_demux()` and `dp_tlm_demux_counts()` split a record array by
    probe in one pass** (C API; `dp_tlm/dp_tlm_core.h`). Records come out of a
    context interleaved — every probe's emissions in emission order — and every
    consumer that wanted one probe's trace wrote the same
    `recs[recs["probe"] == id]["value"]` filter, once per probe, over the whole
    array. The pair does it as a sizing pass and a fill pass, placing each
    record on the visit that reads it: **O(n)** rather than O(n × probes).

    Both take a plain array rather than a context, so the same call serves
    `dp_tlm_read()`'s output, `dp_tlm_capture_records()`, and a `.tlm16` file
    read straight off disk. Probe ids are registry slots, so `counts` and the
    destination tables are indexed directly by id and need no map; ids beyond
    the caller's table are skipped rather than treated as an error, because a
    blob from another context may legitimately carry more probes than the one
    asking about it. Writes stop at each probe's capacity, so a buffer sized
    from a stale count truncates instead of overrunning.

- **`read_dict()` on `Telemetry`, `MemoryCapture` and `Capture`** — the same
    records the existing readers return, grouped by probe **name**:
    `{name: values}`, or `{name: (n, values)}` with `index=True`, where `n` is
    the sample index each value was stamped with. That last part is the point
    of the index flag — a real time axis is `n / fs`, where the alternative
    people actually wrote was `np.arange(v.size)` labelled "symbol index",
    which is only the same thing when every probe fires on every sample.

    `Telemetry.read_dict()` **drains**, exactly as `read()` does, and shares the
    ring with it — calling both in one loop splits the records between them.
    The capture flavours' does **not** consume: it reads the accumulated
    capture and can be called as often as you like. Every *registered* probe
    gets a key either way, including one that emitted nothing on this drain, so
    a plotting loop does not break on the first block where a probe happens not
    to fire.

    The regrouping is **not** in the binding — it is `dp_tlm_demux()` above, so
    a C consumer reading a `.tlm16` off disk gets the identical split, and the
    hand-written part is marshalling only. It is hand-written because the shape
    has no manifest spelling: jm's `dict` property binds scalar values, not
    arrays. Both bindings call **one** `tlm_build_read_dict()`, which is also
    where two latent defects are fixed once rather than twice — a duplicate
    probe name is now rejected by name instead of being a use-after-free
    (the second insert dropped the dict's only reference to an array still
    being written through), and the probe bound is clamped where both calls
    originate, so a `nprobes` past `DP_TLM_MAX_PROBES` can no longer hand
    Python arrays that `dp_tlm_demux` never fills and `PyArray_SimpleNew` never
    zeroed.

    `dp_tlm_capture_context()` is new alongside it, exposing the borrowed
    context so a capture resolves its own ids into names rather than being
    handed the context separately and keeping the two associated by hand.

    The MPSK telemetry capture demo and its gallery page are rewritten on this:
    the ring-size guess, the `chunks + np.concatenate`, the per-probe filter,
    the `{v: k}` id-to-name inversion and the fake sample axis are all gone,
    the drain is four lines, and the x-axis is real milliseconds — so the
    carrier pull-in lines up across `rx.car.freq`, `rx.car.locked` and
    `rx.tracking` by inspection.

- **`make characterize` — a category for the long sweeps, so they stop taxing
    every push.** A **characterization** answers "how does this object behave
    across its whole envelope" by sweeping C/N0, Doppler, sample rate and seed
    until the answer is statistically meaningful. Two of them were living in
    `src/doppler/examples/`, where the smoke gate ran them on every push:
    measured, **164.6 s + 117.7 s + 18.9 s against ~58 s for every other
    example combined** — three sweeps were the overwhelming majority of a
    376 s gate. Shortening a 300-trial Monte-Carlo to fit a smoke gate spends
    the statistical confidence that is its entire point, so they moved instead
    of shrinking, and the example gate fell to **68.8 s** on that change
    alone.

    Three subjects under `src/doppler/dsss/tests/characterization/`:
    `dsss_receiver`, `acquisition` (the continuous `Acquisition`) and
    `burst_acquisition` (`BurstAcquisition`'s Pd/Pfa vs Es/N0),
    laid out like the validation tree — one directory per subject, one fixed
    filename, artifacts beside the script, discovered by glob — so a new
    subject is covered the moment its folder exists. **Not** in `GATES_DEPS`
    and not in `ci.yml`: it is run on purpose, when the envelope is the
    question.

    **Validation was the wrong home for these, for a mechanical reason.** A
    validator runs every measurement on every push *twice* —
    `test_validation_limits.py` executes its `build(write=False)` and
    `make validate-check` re-renders its report — so parking a 280 s sweep
    there would have moved the cost rather than removed it. The two categories
    now answer different questions, and `docs/dev/contributing/validation.md` says which is
    which.

    What guards the code per-push is each subject's **fast twin** under
    `tests/`, which imports its helpers and runs a handful of trials, plus
    **`make characterization-check`** (in `lint`): it fails a subject with no
    `__main__` block or no twin at all — the two ways a sweep becomes a silent
    no-op. Stated rather than implied: the twin proves the helpers still run,
    **not** that the envelope still holds. A regression that moves a pull-in
    boundary without breaking an import waits for the next `make characterize`.
    A subject's plot is a **working artifact**, written beside its
    `characterize.py` and gitignored; the findings go to stdout. The one
    subject with a published gallery page is the exception and goes through
    the **gallery pipeline** instead: `make gallery` re-runs it with the
    asset path as its argument, so the page's figure is regenerated by a
    target like every other gallery image, and is committed in exactly one
    place. That closed a real gap —
    `docs/assets/dsss_acq_characterization.png` had been hand-committed on
    2026-07-11 with **no target refreshing it** (the script was never in
    `GALLERY_SCRIPTS`), and regenerating it moved 105,324 bytes to 122,397:
    the published figure was a month behind its own code. Note what is not
    claimed — there is still no staleness gate on a characterization, and
    there cannot be one without re-running the sweep. `make gallery` makes
    the figure reproducible, not automatically fresh.

- **One canonical exponential moving average, certified — `ema_step` and
    `ema_alpha_decim` in `native/inc/util/util_core.h`.** The library had
    written the first-order EMA out four times in two different algebraic
    forms, with no shared statement of what the recursion guarantees. The
    form was chosen by measurement rather than taste: scored against a
    40-digit reference computed from neither form, the incremental
    `state + alpha*(x - state)` beats `alpha*x + (1-alpha)*state` everywhere
    the library operates, and the margin widens as the average lengthens
    (2.7e-17 against 5.4e-15 at `alpha = 1e-5`). `ema_alpha_decim` computes
    `1-(1-alpha)^d` through `expm1`/`log1p`, because the direct expression is
    catastrophic cancellation at `d == 1` — 26865 ulps off at `alpha = 1e-5`,
    where the answer must be `alpha` itself.

    It is the sixth object certified under the validation campaign and the
    first that is a primitive rather than a DSP block: 15/15 limits in
    `src/doppler/util/tests/validation/ema/`, C evidence in
    `test_util_core.c` §1–§8 with every section sabotage-proven, rationale in
    `docs/design/ema.md`. Two laws nothing previously checked are now pinned
    — the `(2-alpha)/alpha` noise reduction that `det_ema_alpha` has been
    *inverting* all along to size a coefficient, and the memory crossing
    `1 - 1/e` on exactly sample `ceil(-1/ln(1-alpha))`.

    Recorded here rather than with the change: the primitive merged (#700)
    with no CHANGELOG entry, which `changelog-check` cannot catch — it fails
    only when `[Unreleased]` is entirely empty, so anything lands green under
    any other bullet. The gate's gap is #705.

### Changed

- **The starter's Makefile is what a newcomer reads first, so it was cut back
    to what it teaches.** `coverage` and `docs` are gone: both named tools
    nothing declared — `lcov`, `genhtml`, `doxygen`, `zensical`, `pytest-cov`
    — so in a freshly unpacked starter `make coverage` spent a full Debug
    rebuild and then said `lcov: command not found`. Neither ran in CI nor
    appeared in any doc. What survives is `setup`, `build`, `test`, `bench`,
    `clean`, `help`, and one `dev` extra in `pyproject.toml` that makes all of
    them work after a single command.

    `help` is now derived from the `## ` comment beside each target instead of
    a hand-written list — the list it replaces had already drifted, still
    advertising coverage as "C (lcov)".

- **`make bench` measures something.** Both benchmarks were unfilled jm
    scaffolds: the C one recorded nothing (`EMPTY bench_capture_core: no   measurements recorded`) and the Python one called `Capture(...)` with a
    literal `Ellipsis` and printed a string. `pytest-benchmark` was missing
    too, and `python_files` never included `bench_*.py`, so pytest collected
    no benchmark at all. All four are fixed, and the starter now demonstrates
    the thing it exists to demonstrate — reading a capture at **483 MSa/s** in
    C, with the Python binding costing about **4%** over it (135.6 µs vs
    141.2 µs for the same file).

- **The starter's Makefile drops its Windows branches.** The manifest has
    declared `platforms = ["linux", "macos"]` all along, so `SHELL = cmd.exe`,
    the MinGW generator override, the `2>nul` twin of every probe and the
    `.pyd` clean-up were dead code. `PYTHON` collapses with them: it used to
    run the interpreter to ask for `pathlib.Path(sys.executable).as_posix()`,
    which existed only to turn Windows backslashes into forward slashes, and
    to fall back to a bare `python` that can now only resolve to Python 2.
    It is `command -v python3`. The root `CMakeLists.txt`'s unconditional
    `if(WIN32 ...)` libwinpthread copy is gone too — `jm status --check`
    confirms its absence is not drift.

- **just-makeit pinned to 0.63.3, and the tree builds on it.** The bump
    closes nine doppler-filed issues:
    [gh-1023](https://github.com/just-buildit/just-makeit/issues/1023)
    (`jm bench` runs what the tree BUILDS, not what the manifest declares),
    [gh-1034](https://github.com/just-buildit/just-makeit/issues/1034) (a
    function-only module gets a C test and a benchmark),
    [gh-1042](https://github.com/just-buildit/just-makeit/issues/1042) (every
    parameter in a generated signature gets a `Parameters` entry),
    [gh-1046](https://github.com/just-buildit/just-makeit/issues/1046) (jm
    does not emit a CMake target name the project already declares),
    [gh-1051](https://github.com/just-buildit/just-makeit/issues/1051) (a
    `count_default` method's `.pyi` no longer hardcodes `count: int = 1`),
    [gh-1052](https://github.com/just-buildit/just-makeit/issues/1052) (a
    header-derived `*_max_out` doc is one paragraph, not one per source
    line), and the three below.

    **0.63.0 through 0.63.2 could not build this tree**, which is why the
    pin lands three patches downstream of the minor. gh-1034 gave every
    module with free functions a `test_`/`bench_<name>_core` pair, and each
    release uncovered the next defect behind the last:

    - **0.63.0/0.63.1 — the pair did not configure.** For a *collocated
        module-object* (a module whose `objects` list carries its own name)
        the object already emits `test_<obj>_core` into the same
        `CMakeLists.txt`, so `cmake` refused outright:
        `add_executable cannot create target "test_agc_core"`. Fixed by
        [gh-1055](https://github.com/just-buildit/just-makeit/issues/1055),
        and under it
        [gh-1057](https://github.com/just-buildit/just-makeit/issues/1057) —
        the reason nothing caught it. `_targets.from_manifest()` accumulated
        into a `set`, so a name emitted twice was indistinguishable from one
        emitted once; measured on this tree at filing, 365 names emitted
        against a set of 356, **nine produced twice and the set reporting
        zero**.
    - **0.63.2 — the pair configured, then did not compile or link.**
        Getting past `cmake` is what let anything reach the compiler for the
        first time. A function declared with `out_type` got a call one
        argument short of the prototype jm had generated in the same run
        (`(void)ccsds_asm_bits()` against `void ccsds_asm_bits(uint8_t *)`),
        and the targets linked `<module>_core m` and nothing else, so a
        module function calling a sibling core did not resolve. Both are
        doppler-filed —
        [gh-1060](https://github.com/just-buildit/just-makeit/issues/1060)
        and
        [gh-1061](https://github.com/just-buildit/just-makeit/issues/1061),
        the latter being gh-254's lesson, which gh-1034 introduced a second
        target pair without inheriting.

    Six generated module tests land with the bump — `arith`, `filter`,
    `measure`, `resample`, `spectral`, `wfm`. They are jm scaffolds and say
    so, reporting `PASSED (0 checks)`; what they do today is prove every
    symbol links and every all-scalar function survives a call. That is not
    nothing — see the entry below. **They are real tests**, not the
    scaffolds jm generated: `dp_test.h`'s `DP_TEST_END` fails a test that
    checked nothing, which is doppler's own rule refusing a scaffold
    outright, so the six were written against the 43 functions' actual
    properties ([#914](https://github.com/doppler-dsp/doppler/issues/914)).
    The seven module *benchmarks* get no
    scaffold at all: they exist and are declared in this project's own
    `CMakeLists.txt`, so gh-1046 stands jm down.

- **A repo path named in prose is now gated, and three dead citations
    fixed.** `check_site_links.py` holds every LINK in the built site and
    cannot see a path written as a code span, so a page could cite a file
    that had never existed with every docs gate green. New
    `scripts/check_doc_paths.py` (on `make lint` via `docs-invariants`)
    scans **headers as well as docs** — the worst instance was written in a
    C header and reached the site only because mkdoxy copied it, so gating
    the generated page would have been gating the copy. Cross-repo
    citations are excluded by naming the sibling project, derived from the
    text rather than an allowlist. 587 paths across 876 files check clean.

    What it would have caught: `dsss_receiver_core.h` sent readers to
    `docs/gallery/dsss-acq-async-data.md` and `dsss-despread-async-data.md`,
    neither ever committed under those names; and the NATS archive page
    closed its "read X for the current state" note with
    `docs/dev/streaming-roadmap.md`, which has never existed in this repo at
    all.

- **`docs/design/mpsk.md` no longer tells readers they cannot link
    `mpsk_map`.** §9.6 listed "`mpsk_core` is in no library" as *open*
    against [#747](https://github.com/doppler-dsp/doppler/issues/747) —
    which is closed, and both `mpsk_core.c.o` and `util_core.c.o` are in
    `libdoppler.a` today with `mpsk_map`/`mpsk_demap` defined. The bullet is
    rewritten as resolved, and the stale "84 component cores" figure (104
    now) is gone rather than corrected, since a count restated in prose is a
    number with nothing keeping it true.

- **`doppler.coding.ConvEncoder` — doppler can encode now**
    ([#900](https://github.com/doppler-dsp/doppler/issues/900)). Until this,
    **no class in the library exposed an `encode()` at all.** `Viterbi`
    accepts any rate-1/n code, and Python could produce symbols for exactly
    one of them — CCSDS's, and only inside a `FrameDesc`, because that path's
    stage kinds bind to `ccsds_tm_frame_ops` and carry a depth rather than a
    polynomial. A decoder whose matching encoder cannot be reached is a
    decoder that can only be tested against itself.

    ```python
    from doppler.coding import ConvEncoder, Viterbi

    sym  = ConvEncoder([0o171, 0o133], k=7, invert=0x2).encode(bits)
    back = Viterbi([0o171, 0o133], k=7, depth=35).decode(llr)
    ```

    `conv` keeps the CODE — the description, the trellis arithmetic and the
    kernel — and `conv_enc` is the stateful encoder over one, the same split
    `viterbi` established. It is not a second implementation:
    `conv_enc_encode` calls `conv_encode`, and the C benchmark measures the
    wrapper at **1.001x** the raw kernel.

    Two measurements worth recording. Encoding is **flat in `k`** — 2.92 ns
    per information bit at every `k` from 3 to 9, because the encoder
    computes `n` parities per input bit and never walks a trellis — against
    the decoder's 331 to 1699 µs over the same range, where `2**(k-1)` states
    set the price. And the register carrying between calls is the whole
    reason the encoder is an object: a chunked encode is bit-identical to one
    call, while an encoder restarted per block emits `k - 1` wrong symbols at
    every boundary.

- **`doppler.viterbi` is now `doppler.coding`** — **BREAKING**, though it
    breaks nothing released: `Viterbi` shipped in no tag, and is
    `[Unreleased]` above. A module named after the DECODING algorithm cannot
    hold the encoder without reading wrong, and jm's `reexports` names
    submodules of the same package rather than a sibling, so a re-export
    could not bridge them. `doppler.coding` is the home for the general code
    families a standard configures; `rs` follows.

- **`ccsds_tm` certified — the 14th object, and the last of the three with
    no Python face** ([#894](https://github.com/doppler-dsp/doppler/issues/894)
    context;
    `src/doppler/tests/validation/ccsds_tm/results.md`). 12 limits, 5
    findings, 1 open.

    Its claim inventory came out better than any before it — **every public
    entry point was already pinned against a published value**, because the
    component was built that way. So the certification measured the three
    things it adds *on top of* the codes it configures, none of which a
    single codeword can show:

    - **The interleaver's guarantee is exact and has no tail.** A contiguous
        burst of `depth * 16` symbols is corrected and `depth * 16 + 1` is
        not — 200 blocks of 200 either way, at every allowed depth. It is a
        cliff rather than a curve, because the pigeonhole argument is exact;
        size the depth from the longest burst the channel produces, not from
        a probability.
    - **B-6's reason for demoting the 255-bit randomiser is measurable, and
        it is 91 dB.** On constant data `CCSDS_TM_RAND_LEGACY` puts a line
        that far above its own noise floor, at 1/255 of the symbol rate
        exactly as 10.4.2 warns, where the default 131071-bit sequence puts
        nothing (1.9 dB, the ordinary fluctuation of an averaged
        periodogram). Measured through `psd_core`, the shipped meter.
    - **A looser ASM threshold is a worse detector, not a more sensitive
        one.** At `max_errors = 8` the marker is found at its right offset
        only 58 % of the time **with no channel errors at all**, because
        `asm_find` reports the FIRST match and each of the 96 preceding bits
        is another chance to beat it. `t = 4` survives both tails. The
        false-alarm rate itself tracks the binomial closed form to within
        20 % across five decades.

    Two things the inventory turned up that no gate could:
    **`ccsds_tm_randomise`'s docblock described the LEGACY generator** — 8
    stages, all-ones preset, 255-bit period, three facts each belonging to
    the other randomiser, which the same header states correctly two
    declarations above. Fixed here. And **`max_errors` has to be chosen
    against the search window rather than the marker length**, which no
    interface says
    ([#897](https://github.com/doppler-dsp/doppler/issues/897)).

- **`doppler.viterbi.Viterbi` — the soft-decision convolutional decoder now
    has a Python face**
    ([#893](https://github.com/doppler-dsp/doppler/issues/893)). It lived
    inside the `conv` `c_deps` directory, one level below anything jm
    modelled, so it had no binding, no generated CMake target for its test or
    its benchmark, and a component string typed by hand. Declaring it as an
    object in a collocated `viterbi` module produced all four, plus the
    `.pyi`, for one `objects/viterbi.toml` and one `[module.viterbi]` block:

    ```python
    v = Viterbi([0o171, 0o133], k=7, depth=35)   # CCSDS 131.0-B-3 §3
    bits = v.decode(llr)                          # soft in, hard out
    ```

    The manifest cannot express `viterbi_create(const conv_code_t *, size_t)`,
    so the declared constructor takes the generator polynomials directly — the
    array *is* the code, and its length gives `n`, following `fir`'s `taps`.
    The struct form survives as `viterbi_create_code` for callers that already
    hold one, and a test asserts the two agree. State carries across calls and
    serializes, so `Viterbi` joins the shared round-trip matrix; a code the
    decoder refuses now raises `ValueError` with what it wanted rather than a
    blanket `MemoryError`. `conv` keeps the code description and the encoder,
    the same split `rs` and `ccsds_tm` already have.

    The C benchmark moved with it, and now **interleaves** its two traceback
    depths instead of measuring them one after the other. It had to: the
    first configuration in a process pays the clock ramp that the 0.25 s
    settle does not finish absorbing, and back-to-back runs of the same
    binary reported depth=35 at 229 ns/bit once and 162 ns/bit twice —
    turning a real **1.42x** cost for depth 96 into 1.03x, and once into
    0.99x, which reads as a 95-step traceback being cheaper than a 34-step
    one. Interleaved, three consecutive runs agree to 1.42x, and the Python
    face measures 1.38x independently.

- **Eight C benchmarks, and a gate that keeps them honest.** An audit of what
    landed since v0.42.0 found five new components with C tests and no
    benchmark at all — `conv` (convolutional encode/Viterbi decode), `rs`
    (Reed-Solomon), `ccsds_tm`, `mpsk` (the per-symbol map/demap/soft-demap
    kernels) and `ber` — plus `snr`. All six now have one, and the two
    benchmarks that shipped with `frame` and `frame_meter` as unfilled jm
    scaffolds are filled in.

    Three things the new measurements say, none of which the tree had
    recorded: at the CCSDS k=7 rate-1/2 code, Viterbi decode runs at
    **6.2 Mbit/s against the encoder's 342**, so decoding is the expensive
    direction by more than fifty times; RS(255,223) costs
    **1.33x** from a clean
    codeword to a fully-loaded one, because the 32 syndromes dominate the
    correction they gate; and in the CCSDS chain the inner code is **40%** of
    a full `frame_encode` while `asm_find` — the only stage that scales with
    the sample rate rather than the frame rate — scans at **1.0 ns/bit**.

- **`scripts/check_bench_coverage.py`, on `make lint`.** Four rules, all
    derived from the tree rather than from a list: a component with C tests
    has a benchmark; a non-component benchmark has a CMake target; a
    benchmark records a measurement; and it writes its JSON under the name a
    collector opens. Both allowlists are ratchets that may only shrink, and
    the gate fails if an entry is left behind after its benchmark starts
    measuring.

- **`native/benchmarks/` is now honest about what runs.** `jm bench` walks
    jm's *component* list, so a `c_deps` entry or a function-only module is
    invisible to it — `util`, `timing`, `hbdecim` and `resamp` had been
    compiled by every build and run by nothing for months, appearing in no
    published snapshot (`just-makeit bench util` answers
    `unknown component(s): util`). Ten benchmarks are in that state, including the six
    added here; they are run by hand until
    [just-makeit#1023](https://github.com/just-buildit/just-makeit/issues/1023)
    ships, rather than papered over with a local runner the fix would retire.
    The gate holds everything checkable without running them.

- **The algorithm lifecycle now names what a change owes a reader.** Phase 9 of
    [Adding an Algorithm](https://doppler-dsp.github.io/doppler/dev/adding-algorithms/)
    was one paragraph about carrying findings back; it is now five deliverables
    with the gate behind each — a `@code` example on every public function
    (executed by `make test-stubs`), a markdown guide when prose outgrows a
    docstring, C **and** Python benchmarks, a runnable example, and a gallery
    page when there is a figure worth seeing.

    Phase 3 also states the part that was implicit: a header's `@code` blocks
    **are tests**, flowed into the `.pyi` by jm and executed, so they must be
    pinned against a real run rather than reasoned about.

- **The published benchmark page moves to v0.43.0**, five releases after it
    last did. `benchmarks/published` had stalled at v0.37.3 because `make bench`
    was the one unpinned jm call site and had been silently resolving to a
    version too old to understand the manifest — fixed earlier in this cycle,
    and this is the first publish since.

    Measured with `make bench-interleaved` (five alternating passes per build,
    per-benchmark best kept) on `030b7679`, so the two columns are free of the
    cross-run drift the old two-pass `bench-publish` picked up.

    The new `syncword` rows are the most native-sensitive thing on the page:
    the marker search is an XOR-and-sum over every offset, which vectorises,
    so `-march=native` buys **+45 % at a 32-bit marker and +97 % at 256** over
    the portable wheel build. `max_errors_for` is flat at +2 %, as a scalar
    log-space sum should be.

- **Timing the Python microbenchmarks is `make bench-python` now, not part of
    `make test-python`.** Tests run constantly; benchmarks run occasionally.
    They were one target doing both — a second, *serial* pass over
    `src/doppler/*/benchmarks/` so pytest-benchmark would measure instead of
    disabling itself under xdist.

    Measured in CI before changing anything: that pass was **139 s of a 268 s
    step, on all six Python versions** — roughly 14 minutes a run spent timing
    code on a shared runner and throwing the numbers away. Runner timings are
    not trustworthy in the first place; that is what
    [#543](https://github.com/doppler-dsp/doppler/issues/543) deleted
    `perf-regression.yml` over, and why `make bench-interleaved` exists.

    The benchmark files still run in `make test-python` — as **tests**, with
    `--benchmark-disable`, so a broken benchmark script still fails where it
    should. Nothing stopped being asserted: the suite went from 2730 tests
    plus a separate 138 to **2868 in one pass**, 66 s locally.

    `--benchmark-disable` is explicit rather than relied upon. pytest-benchmark
    *also* self-disables whenever xdist is active, which would leave the test
    run's behaviour depending on `-n auto` — and `PYTEST_ARGS="-n 0"` is a
    documented override, under which timing would quietly switch back on inside
    the step everyone runs. (`-p no:benchmark` is the wrong knob and was
    measured as such: it removes the fixture and every benchmark test errors
    with "fixture 'benchmark' not found".)

    Python line coverage now runs on **3.12 only** — the version whose
    `coverage.xml` is uploaded. The other five computed it and discarded it,
    which is just a slower test run; the number the patch gate reads has always
    come from the `coverage` job's single instrumented C ∪ Python ∪ Rust build.

- **The C core is compiled through ccache, and a developer can now run the
    gates in CI's own container.** Two halves of the same idea: stop repeating
    work, and stop guessing whether the environment matches.

    Measured first. On one run the core was built **seven times with the same
    compiler and the same flags** — once per Python job (only the ~11 s
    extension build differs per ABI) plus the ubuntu-24.04 leg — at 59–80 s
    each, about 7 minutes of identical work per run. `ccache` is now in every
    dev group and reaches every configure step, including the coverage tree's.

    What is *not* duplicated stays that way: the ubuntu-22.04 leg is a
    different glibc and compiler, the coverage build is clang with
    `-fprofile-instr-generate`, and `glibc-228` is the floor's toolchain.
    ccache hashes the compiler and the full flag set, so those land in
    separate entries by construction rather than by our being careful about
    it. `make ccache-stats` runs after each build so the hit rate is in the
    log — a cache that stops hitting has no other symptom than builds slowly
    getting longer.

    ccache rather than build-once-and-share: an artifact would make the six
    Python jobs *wait* on a build job that nothing waits on today, and it
    cannot span runs. The cache can, which is the case a developer feels — the
    second push to a branch.

    **Running it like CI is now a target, not a hope.** `make ci-run   TARGET='build test-rust'` runs any goals inside the *pinned digest* CI
    uses, `make ci-shell` opens a shell in it, and `make ci-gates` runs the
    whole gate set there — `gates` is already "every gate CI runs" (enforced
    by `gates-check`), so this only adds the environment.

    Both of today's CI-only failures would have been caught by it before a
    push: a cargo too old to read the repo's own lockfile (invisible on a box
    with rustup), and a host build tree handed to the container failing on
    `atan2f@GLIBC_2.43`. That second one is why `ci-run` builds into its own
    `build-ci/` and sets **both** `BUILD_DIR` and `DOPPLER_BUILD_DIR` —
    `ffi/rust/build.rs` locates the library itself and defaults to the host
    tree, so missing the second variable fails in a way that reads as a code
    bug. Same separation, same reason, as `glibc-gate`'s `build-glibc228`.

    Deliberately not a pre-push hook: `gates` includes `coverage` at ~10
    minutes, and a hook that slow is one people pass `--no-verify` to.

- **The pseudo-randomiser follows 131.0-B-6, which changed the default.**
    §10.4.1 makes the **131071-bit** sequence (`h(x) = x¹⁷ + x¹⁴ + 1`, preset
    `11000111000111000`) the requirement; §10.4.2 keeps the 255-bit one
    doppler shipped *"for backward compatibility with legacy systems"*. Both
    are now available as configurations — `CCSDS_TM_RAND` and
    `CCSDS_TM_RAND_LEGACY` — over one generator, and the default is B-6's.

    `wfmgen --randomise` takes an optional generator: bare means `ccsds`,
    and `--randomise legacy` selects the old one. `--record` carries **which**
    rather than a bare `true`, because the two are not interchangeable on the
    air — only the matching receiver derandomises a given waveform, so a
    record naming neither could not rebuild its own capture. A boolean is
    still read on input, as the default, so older records load.

    The frame decoder now **steps** the generator alongside the pack instead
    of indexing a 255-entry table. That table was free at the old period and
    would be 128 KB at the new one — and longer than any CADU, so it would
    never wrap.

- **The reported coding-gain bound moves 6.1 → 4.1 dB, and the reason is the
    measurement's shape rather than the receiver's.**
    `validate_rx_coding_gain`'s cleanest point moves Es/N0 0.0 → 2.0 dB on the
    same chain, same seeds, same code, so the gate is re-baselined to 4.0 dB
    with the measurement recorded beside it. The ≥6.1 dB half of that pair was
    only ever measured on the legacy waveform, so it is the figure this
    release does **not** ship, and `docs/design/fec-receive.md` §8 reports the
    B-6 sweep throughout.

    B-6 changed the sequence deliberately, to remove the 255-bit one's
    spectral lines at 1/255 of the symbol rate and its ITU power-flux-density
    problem. A maximal-length sequence of degree *D* has a maximum run of
    exactly *D*, so the legacy randomiser guaranteed a transition every ≤ 8
    symbols and B-6's only every ≤ 17 — but both have the **same 50.00 %
    transition density** and the same run distribution below 8, and the whole
    difference is ~20 events per CADU, or 0.2 % of symbols.

    Isolated, that costs about **0.02 dB** of implementation loss, not 2 dB
    (gh-866, closed with the data). What moves the reported clean point two
    whole grid steps is a concatenated code on its cliff amplifying a ~3 %
    relative change in channel SER, read on a 1 dB sweep grid — B-6 at +1 dB
    was already at 1.08e-3 payload BER, so the true threshold shift is well
    under 2 dB.

- **A changelog entry is a FILE now, because a shared line does not scale.**
    Every pull request appended to the same place in `CHANGELOG.md`. Measured
    with twelve PRs in flight: **all twelve** touched it, all near the top of a
    2625-line `[Unreleased]`, so **every merge knocked the other eleven to
    `CONFLICTING`** — `O(N^2)` hand-resolutions in one file, none of them about
    the code. That is a property of the layout, not of anyone's discipline, and
    no amount of care fixes it.

    An entry now goes in `changelog.d/<section>/<slug>.md`. The directory **is**
    the `### Heading`, so the section is never declared twice and cannot
    disagree with itself; the content is the entry verbatim, moved and not
    templated. Two PRs touch different files, so git has nothing to resolve.

    **`changelog-check` is folded, not replaced** — it asks exactly what it
    asked before (a branch changing code must say what changed) and now accepts
    either a fragment or a direct `CHANGELOG.md` edit, counting fragments
    toward `[Unreleased]` being non-empty so a release cannot be cut with the
    notes still sitting unassembled. `make changelog-assemble` promotes them
    once per release, in Keep a Changelog order, and deletes them as it goes,
    so a second run is a no-op by construction.

    One consumer had to learn about them: `gen_jm_pin.py` harvests the jm pin
    from the changelog TEXT, so a bump recording its pin in a fragment would
    have been reported as a pin nothing documents. It reads
    `changelog.d/*/*.md` too now.

- **Every Linux CI job runs inside a baked toolchain image instead of
    apt-installing one.** `deploy/docker/Dockerfile.ci` carries the dev and
    docs groups, and `.github/ci-images.env` pins it by digest.

    The provisioning step it replaces was ~112 MB of archives per job, ten
    jobs a run — and most of it was already on the runner under a different
    owner: cmake 3.31.6 and cargo 1.97.1 live in `/usr/local`, outside dpkg,
    so apt did not know they were there and fetched the distro copies anyway.
    That download was the entire exposure to mirror weather. On 2026-08-19 it
    stalled five runs of one PR, one job trickling 21m30s against a
    25-minute ceiling ([#885](https://github.com/doppler-dsp/doppler/issues/885)).

    **The image has no package list of its own.** It copies `bootstrap.toml`
    and runs the same two `jbx install-deps` commands `make install-deps` and
    `make install-docs-deps` run, so the image and a dev box provision from
    one file. A second list is exactly what `bootstrap.toml` exists to
    prevent, and it would rot in the way that is hardest to notice — the
    image would keep working while no longer being what a developer gets.

    **Two bases, and the pair is load-bearing.** `build-and-test` runs
    ubuntu-22.04 and ubuntu-24.04 on purpose — two glibcs — so one image for
    both legs would leave that matrix naming two environments while testing
    one. `BASE` is a build argument and both are published.

    **The glibc 2.28 floor is untouched.** It is still answered where it
    always was: `make glibc-gate` builds the tree in Debian 10 and runs
    `glibc-check` over the `.so` and every example binary, and release wheels
    are still built in `manylinux_2_28`. A modern base cannot answer a floor
    question, and this image does not try to.

    Two things fall out of running in a container. `make nats-up` shelled out
    to `docker run`, which does not exist inside a container job — and the
    nats:// tests **self-skip** when 4222 is unreachable, so that would have
    quietly dropped the whole nats path rather than failing. The image now
    carries `nats-server` and `scripts/start-nats.sh` prefers the binary,
    falling back to docker, so a dev box without docker gains those tests
    instead of skipping them. And the script no longer reports success on
    someone else's broker: it detects a listener it did not start and says so.

    Freshness is a question asked nightly, not an image consumed nightly.
    `ci-image.yml` rebuilds, compares the package fingerprint baked into the
    image, and publishes and opens a repin PR only when the content actually
    moved — so a run stays reproducible while drift still surfaces within a
    day. `make ci-image-check` fails offline when `bootstrap.toml` or the
    Dockerfile move without a repin, and refuses any `container:` naming a
    mutable tag.

- **`ContinuousMpskReceiver` pins `nda_tap = strobe`, measured on its own
    waveform.** It shipped pinning `mf_in`, and every harness that had touched
    that tap shared the same gap: `rx_battery` runs RRC with dense transitions
    throughout (the **burst** flavor's signal), `rx_nda_tap` sweeps NRZ but
    **noiseless**, and the C test runs I&D at 30 dB. `docs/design/mpsk.md` §0
    calls the continuous flavor *NRZ BPSK, periods of data modulation off but
    carrier on*, and nothing measured that.

    New: **`native/validation/rx_dynamics.c`** — NRZ BPSK, I&D, `m_out = 4`,
    DTTL, 12 dB Es/N0, half the record with modulation **off** (carrier on, so
    the TED has no edge and timing cannot close), then dense transitions as a
    step, all under a Doppler ramp through `doppler_channel` so the carrier and
    every clock move together. It captures every telemetry probe
    (`--out DIR`), and `make plot-rx-dynamics` renders
    `docs/assets/rx-dynamics.png` from that capture — the figure plots the
    receiver's own records, never a Python re-derivation.

    | tap          | lock, modulation OFF | min at the onset | end    |
    | ------------ | -------------------- | ---------------- | ------ |
    | **`strobe`** | +0.935               | **+0.860**       | +0.920 |
    | `mf_out`     | +0.934               | +0.478           | +0.802 |
    | `mf_in`      | +0.761               | +0.417           | +0.714 |

    **`strobe`'s timing dependency costs nothing in the half where timing is
    impossible**, because an unmodulated NRZ carrier is *sampling-phase
    invariant* — every sample is the same constellation point, so the
    M-th-power discriminator does not care which one the timing loop would have
    nominated. Timing closure gates demodulation, not carrier acquisition.
    `mf_out` takes the largest hit the moment transitions exist (its ISI bias,
    on schedule). The **TED** is the largest single effect on the page: the
    same record through Gardner deepens `strobe`'s onset dip from 0.075 to
    0.306, four times, on its own.

- **`docs/dev/` is organised around its spine, and CI has a page.** The
    thirteen pages [Adding an
    Algorithm](https://doppler-dsp.github.io/doppler/dev/contributing/adding-algorithms/)
    links to now live under `docs/dev/contributing/`, leaving `docs/dev/`
    holding the index and the maintainer-internals pages. The spine was
    already the entry point and `index.md` already listed its members
    separately from maintainer plumbing; the directory now says the same
    thing the index did.

    New: [Continuous Integration](https://doppler-dsp.github.io/doppler/dev/ci/)
    — what CI is made of and how to run it yourself. The pinned toolchain
    image and why it has no package list of its own; the three things
    installed outside `bootstrap.toml` and why a cross-distro list cannot
    express them; digest pinning and what "refresh nightly" does and does not
    mean; the compiler cache and its measured hit rate; and the four gates
    that watch CI itself.

    It leads with `make ci-shell` / `ci-run` / `ci-gates`, because the point
    of baking the toolchain is that "works on my machine" and "works in CI"
    become the same sentence — including the warning that container and host
    build trees must not be mixed, which is a link error that reads as a code
    bug.

    The move itself was gated end to end: `check_nav_index` for the index
    bullets, the strict site build and `check_site_links` for 183 pages of
    internal links, `check_doc_targets` for every `make` target the new page
    names, and `gen_validation_log`'s output path — a generated page, so its
    generator moved with it rather than being left pointing at a path that no
    longer exists.

- **The `m_out` rule has one implementation again.** gh-644 gave it a home in
    `mpsk_rx_derive_m_out()`; `dsss_receiver_core.c` was never migrated, so
    the tree carried two implementations of one rule — the thing CLAUDE.md's
    *never reimplement existing logic* exists to prevent, and which the
    retired copy's own comment was a monument to having gone wrong once
    already.

    They disagreed below `sps = 2`: the shared rule refuses with `0`, the
    local one floored at `2`. Neither value builds a receiver at `sps = 1`,
    so this moves *which* rule refuses rather than whether one does —
    recorded because a silent value difference between two copies of a single
    rule is how the first drift happened.

- **just-makeit pin 0.62.0 → 0.62.1**, for a fix this repo drove and could not
    work around: **just-makeit#1018**. A gh-1012 signature override's `.pyi`
    documented the PARENT's C symbol while the runtime face documented the
    override's, so `MpskReceiverR.steps` — whose whole point is that it takes
    `float32` where its parent takes `complex64` — was stubbed with a doctest
    that constructs `MpskReceiver` and hands it a complex array.

    `scripts/check_doc_face_parity.py` refused it, correctly, and **no manifest
    or header configuration made both faces right**: removing the override's
    `@code` only swapped the runtime face onto jm's synthesized example while
    the stub kept the parent's authored one. That is what made it an upstream
    blocker rather than a local carve-out, and why the collapse waited on a jm
    release instead of on an exemption.

    The fix keys a member's doc block on the symbol it BINDS
    (`C.method_c_symbol`) rather than on `<component>_<member>`, and judges the
    scaffold sentinel against the member NAME — jm writes its skeleton
    `@brief` from the Python name while the parser recognises a scaffold by the
    name derived from the C symbol, the same string for every method until
    `fn` made them differ.

    Adopting it reconciled two `.pyi` files and eight sacred fragments, with no
    `_core` and no signature drift. `Doc face parity` reports 213 methods compared and 0 divergent.

- **What `mf_in` costs is excess noise bandwidth, not processing gain.** A
    Nyquist-sampled band-limited signal loses nothing by being sampled fast.
    Measured at the node with the AGC off so the path is linear, `mf_in` sits
    **6.01 dB** below Es/N0 at `bank_sps = 4` while the terminal node sits
    1.7 dB below it — and `10·log10(4) = 6.02 dB`, *identical at 6.79, 12 and
    20 dB Es/N0*, which is the signature of a pure bandwidth ratio rather than
    an SNR-dependent effect. DEC band-limits to **its own** Nyquist,
    `±bank_sps·Rs/2`, while the signal occupies ~`±Rs`, and the terminal filter
    — the first thing in the cascade matched to the signal — is downstream of
    this tap.

    So the cost is **bounded by the plan** (`bank_sps` is a planner outcome:
    still 8 at `sps = 64`, so 9.0 dB there, not 18) — and it is the tap's
    **stated price rather than a defect**. Band-limiting the node (an arm
    filter, or the 2-sps decimation `docs/design/mpsk.md` §3.3 considers)
    would recover most of it and is **declined**: both cost serialized state
    on every object carrying the tap, and `strobe` already reads the node
    matched to the signal for free — and measures better on this flavor's own
    waveform. So `mf_in`'s trade is stated, not repaired: `bank_sps/(2M)` of
    pull-in range for `10·log10(bank_sps)` dB of lock sensitivity. The loop
    acquires at every operating point measured; what degrades is the
    M-th-power lock statistic, which is an SNR measure.

- **`MpskReceiver`'s certification is re-based on measurements that can carry
    it** — 33 limits to 60, with a claim inventory (§1.1) mapping all 28
    header claims onto the C test, a report section, or C-ONLY.

    The Es/N0 grid is now **derived per M** from the bound and the record
    length. One grid across all three orders put BPSK where it makes no errors
    at all: six of nine cells could not bound anything, and one of them
    reported the receiver *beating* the matched-filter bound off three errors.
    Each M is now measured where its own bound predicts enough errors in the
    symbols actually **scored**, so the grid moves with the record length
    instead of being retyped, and section 4 asserts only over cells clearing a
    stated error floor.

    Four sections are new. **§2.8** halves `m_out` at fixed Es/N0: ~2.7 dB of
    EVM at every M, reproducing the header's QPSK figure and **not** its
    M-dependent ordering, which is anchored at an SER this record length
    cannot reach. **§2.9** measures the AGC's level law. **§2.10** covers
    lifecycle, telemetry and state, with the resume checked against a *warm*
    instance so the blob has to determine the continuation rather than merely
    not contradict it. **§2.7** grows a true-lock control and all three
    metrics, which turns the invisibility claim into evidence — and
    corroborates `docs/design/rx-test.md` §8.6's independent measurement to
    0.05 dB.

    Two findings are rewritten on the corrected bound, and both, plus
    [#781](https://github.com/doppler-dsp/doppler/issues/781), turn out to be
    the same mechanism: a loop that recovers the symbol rate to 2 ppm against
    a record the meter cannot align. **8PSK's implementation loss is
    unmeasurable by any per-push sweep**, because its measurable window
    (≤14.5 dB) and its working window (≥17 dB) do not overlap. A new finding
    collects six header claims a binding reaches that nothing measures
    ([#814](https://github.com/doppler-dsp/doppler/issues/814)).

    Every characterisation section now closes with what its table *means*,
    which is the half `CarrierNda`'s report had and this one did not.

- **`MpskReceiverR` is now a VIEW over `MpskReceiver`, not a second type — one
    object, three faces.** The Python surface is unchanged for every existing
    caller: same class name, same constructor signature and defaults, same
    `steps()`/`bits()` taking `float32`, same properties. What it GAINS is the
    four §8.1 read-backs the separate type had in C and never bound (`zeta`,
    `num_phases`, `lock_thresh`, `bn_agc_ratio`), which are shared from the
    parent verbatim. In C, `mpsk_receiver_r_*` is gone: the constructor is
    `mpsk_receiver_create_real()` and the block API is
    `mpsk_receiver_steps_real()` / `mpsk_receiver_bits_real()`, over one
    `mpsk_receiver_state_t` carrying `union { ddc_state_t *c; ddcr_state_t *r; }   fe` and an `int real`. `native/{inc,src}/mpsk_receiver_r/` and
    `objects/mpsk_receiver_r.toml` are deleted.

    The argument was never the duplication. `mpsk_receiver_r_core.c` was 372
    lines of which 16 functions were pure delegations, but the cost of the
    split was that their shared 784-line `mpsk_rx_loops.h` **had no test
    home** — so its claims were pinned only where one of the two tests
    happened to reach them, and the two did not overlap. `set_telemetry` was
    asserted seven times on the complex side and zero on the real one; **"the
    LO runs at half the input rate" was pinned by neither**, which is exactly
    where the gh-765 `freq_scale` defect lived. `test_mpsk_receiver_r_core.c`
    is folded into `test_mpsk_receiver_core.c` (§15-23) rather than deleted,
    and every row of that table now has one owner.

    §23 is the claim nothing asserted, in two halves, each proven by sabotage:
    the loop GAIN against `θ_ss = 2πr/wn²` under a **ramp** on both faces
    (`lo_sps = sps` on the real face reads 2.00× the law at both ramp rates),
    and the frequency READBACK against a known offset (dropping the 0.5 in
    `mpsk_rx_lo_to_input()` is off by exactly `df`, five times the tolerance).
    Each sabotage leaves the other half green. The stimulus for the first has
    to be a ramp: a type-2 loop nulls a frequency step regardless of gain,
    which is how gh-765 survived every test in the tree. The estimator is the
    **signed mean** of the discriminator output, not the mean of `|e|` — under
    a ramp the lag is a constant the loop holds, so the signed mean averages
    the jitter out while `mean|e|` carries a bias that read 44% high and failed
    a correct receiver.

    The state blob is unchanged on both faces and neither version moved, but
    the envelope MAGIC is keyed on the face (`MPSK`/`MPSR`) so a blob from one
    is refused by the other by name rather than reinterpreted. `MpskReceiverR`
    keeps its declared defaults verbatim, including the five it pins where the
    parent derives — three of which disagree with the derivation. Fixing that
    is a behaviour change and is gh-829; the collapse changed no behaviour, and
    `make validate-check` reports the report unchanged.

    Made possible by just-makeit#1012 (jm 0.62.0): a view method restating a
    parent's Python name may declare its own signature when it binds its own C
    symbol via `fn`. The type/flavor rule — a difference in constructor is a
    flavor, a difference in method signature is a separate type — is unchanged;
    what changed is that jm can now express the answer.

- **The certification's level-diagnostic finding is now gated by `ctest` and by
    the examples suite, not only stated in a report.** `docs/dev/contributing/validation.md`
    step 9 — whatever a certification establishes goes back into the C test and
    into an example, because those are what keep it true and what put it in front
    of someone.

    **The C test's assertion is strictly stronger.** §11 checked that the AGC's
    gain *moved* by more than 10 dB per 4× level step. It now checks the **law**:
    each step moves it by `20·log10(4) = 12.0412 dB`, measured 12.0412 and
    12.0412. That is the difference between a trend and an absolute level
    estimate. Sabotage shows the coverage this adds — a 2% scale error on
    `agc_gain_db` passes the old `> 10 dB` check and fails the new one.

    **The example carries the blind spot**, because that is what a caller gets
    wrong: `lock` reads 0.936 / 0.948 / 0.950 across a 16× level change and
    **cannot see a level error at all** — it is the M-th-power carrier statistic
    and `carrier_nda_disc` divides out its own `|z|^M`. `mpsk_receiver_demo.py`
    now asserts both halves (the gain law, and that `lock` stays put), so a change
    making `lock` level-sensitive fails the examples gate instead of surprising
    someone in the field. The gallery page includes that region under
    "Diagnosing a level problem — read `agc_gain_db`, never `lock`".

    Only the robust half was carried. The report's §2.9 also shows the `A²`
    timing under-drive in `timing_rate`, and that one is **not** monotone in
    level — at 25 dB and amplitude 0.25 the un-levelled receiver reads better —
    so it is reported and deliberately not asserted anywhere.

- **A level problem is invisible to `MpskReceiver.lock`, and the report now
    says where to look instead.** With `agc = 0` the recovered symbol rate
    degrades from 17 to 172 ppm as the input level falls — the `A²`
    under-drive the header describes, arriving on the timing loop — while
    `lock` reads 0.96–0.97 throughout and is not even monotone in level,
    because `carrier_nda_disc` divides out its own `|z|^M` and is immune to
    the level by construction.

    So the receiver publishes two health readouts with disjoint blind spots,
    and the one a caller reaches for first is the blinder of the two.
    Diagnose with `agc_gain_db` and `timing_rate` instead. The gain law is
    exact: the readback plus `20·log10(amp)` is constant to under 0.01 dB
    across a 32× amplitude span, so the number is an absolute level estimate
    and not just a trend. Neither `lock` nor, at 20 dB Es/N0, the error rate
    can see a level error at all. Measured in the report's §2.9.

- **`MpskReceiverR` derives the five parameters its complex twin derives.**
    `m_out`, `zeta`, `lock_thresh`, `num_phases` and `bn_agc_ratio` now
    default to `0` — *derive it* — instead of carrying pinned values.
    `MpskReceiver` and `MpskReceiverR` are one object with two constructors,
    and both now read back the same numbers:

    ```pycon
    >>> MpskReceiverR(m=4, sps=32.0).num_phases
    64
    ```

    which was `1024`. Three of the five were not merely redundant:
    `num_phases = 1024` was the legacy bank against the **measured**
    saturation point of 64, `lock_thresh = 0.5` a round number against the
    derived `sigma_H0 * eta(Pfa)` = 0.4999, and `zeta = 0.707` a typed-out
    constant against `1/sqrt(2)`. `m_out = 8` happened to equal the
    derivation at the default `sps = 32` and stopped doing so anywhere else.

    The real face never adopted `docs/design/mpsk.md` §8.1 because the
    collapse that created it carried the defaults across **unchanged** — a
    refactor and a retune in one commit is a diff nobody can bisect. This is
    the retune, on its own, with the evidence.

    **Nothing measurable moved.** `validate_mpsk_receiver_real_ber` reports
    implementation loss against the coherent bound at each M's own SER=1e-3
    anchor, and every figure is identical before and after — 0.54 dB at
    M=2, 0.51 dB at M=4, 0.92 dB at M=8. So the saturation measured on the
    complex face holds behind the R2C halfband too, and the 16x bank was
    paying about **40 kB per instance** (measured over 100 instances) for
    resolution the receiver cannot use.

    Pass a value to pin one, exactly as before; only the defaults moved.

- **One `mpsk.md`.** The M-PSK design lived on three pages —
    `mpsk.md`, `mpsk-refactor.md` and `mpsk-soft.md` — and two of them
    specified the *same constructor differently*. They are now one page:
    the refactor's API surface merged into §8 with the disagreements resolved,
    its collapse record kept as §12, and the soft-decision design folded under
    §9 as §9.7, rewritten to describe what shipped rather than what was
    proposed.

    Where the two constructors disagreed, §8.2 is the answer: `nda_tap` and
    `acq_to_track` are **gone** rather than derived, and `differential` moves
    to `bits()`. §8.2 also now states the whole target — **exactly two required
    arguments**, `sample_rate_hz` and `symbol_rate_hz`, with `m` defaulting to
    2 — and the three-constraint rule that derives `bn_carrier`.

- `rs` is certified. The Reed-Solomon header's claims were enumerated and
    mapped onto `test_rs_core.c`, which gained seven sections for the ones
    nothing ran: the derived sizes and the declared range, the consequence of
    the root-stride rule, the parity as a remainder by long division, the
    syndrome closed form, the packed-symbol convention at `J < 8`, every error
    count up to `E`, and that the description carries no running state. The
    evidence layer is
    [`src/doppler/tests/validation/rs/results.md`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/tests/validation/rs/results.md),
    measured by `native/validation/rs_certify.c` on the C-only track, and it
    establishes the number a caller sizes parity by: a failure past `E` is
    **silent** with probability `V(E)/q^(n-k)` — 0.99 at `E = 1`, 2.6e-14 at
    CCSDS's `E = 16` — and that probability does not fall as the damage grows.

- `rs_core.h` no longer offers RS(204,188) as a code to point the file at.
    `n` is `2^J - 1` by construction, so DVB's code is a *shortened*
    RS(255,239) and needs the virtual fill of
    [#813](https://github.com/doppler-dsp/doppler/issues/813); both the header
    and `docs/design/reed-solomon.md` now name the mother code.

- **`rx_battery --check` still does not gate a receiver that refuses EVERY
    point**, and the file now says so where the gate would go. A per-point
    refusal is a result and stays uncounted — a `qpsk`/`psk8` frame-geometry
    refusal is the harness working — but nine of them is a receiver that does
    not work, which is precisely how the `mf_in` pin exited 0 while measuring
    nothing. Closing it needs a run-level rather than a per-point gate, so it
    is being added to this same loop as `dp_rx_witness_t` by doppler#794
    rather than raced here.

- **`libdoppler.a` now declares `-lpthread`, and a static consumer needs it
    on the link line.** The core has needed pthread since `rs.c` moved to
    `pthread_once`, but nothing said so: each component that needs pthread
    carries `Threads::Threads` PUBLIC on its own target, and every component
    is folded into the archive as `$<TARGET_OBJECTS:...>` — objects, not a
    link edge — which drops the usage requirement on the floor. So the
    archive's link interface named only `-lm` while one of its members called
    `pthread_once`.

    Nothing failed on a modern box, because glibc ≥ 2.34 folds pthread into
    libc. On glibc < 2.34 it is a separate library and the symbol has to be
    named: `examples/c/ccsds_link_demo` failed to link in the Debian 10 job
    with `undefined reference to pthread_once`.

    `Threads::Threads` is now PUBLIC on `doppler_lib_static`, so
    `doppler::doppler-static` and `pkg-config --static doppler` both carry it
    and a `find_package`/pkg-config consumer needs no change. A consumer that
    spells the link line by hand should add `-lpthread` beside `-lm`.

- **The telemetry capture demo reads every panel against the number it is
    supposed to hit.** Each trace now carries its reference: decision
    thresholds in red, read off the receiver rather than retyped, and the
    actual quantity an estimator is estimating in green. `sym.i` and `sym.q`
    share one axis, because what matters is their relative size — I settles
    into two ±1 bands while Q collapses onto zero, and separate autoscaled
    panels render a Q of pure noise exactly like a Q carrying signal. The
    example asserts it (mean|I| ≈ 17× mean|Q|) rather than leaving it to
    the eye.

    The demo also stops passing `acq_to_track=1`: **there is no handover.**
    One NDA discriminator steers the LO from the first output to the last,
    which is Mode 1 in `docs/design/mpsk.md`, and the demo had been running
    the superseded design against a view whose own manifest pins that gating
    at 0. The parameter remains on the shipped constructor — measured, it
    still changes 456 of 3998 symbols — so retiring it belongs to
    [#831](https://github.com/doppler-dsp/doppler/issues/831).

- **The telemetry capture example's transmitter is `wfmgen`, not numpy.**
    `mpsk_telemetry_capture_demo.py` shaped its own rectangular pulse
    (`np.repeat`), applied its own carrier offset, and computed its own noise
    level from `sqrt(8 / (2 * 10**(20/10)))` — an Es/N0 convention written out
    by hand, where the `8` is `sps` and the `2` is the complex-noise factor and
    neither is named. It is now one `Composer([Segment(...)])`, the same path
    the CLI and a JSON record drive, with `snr_mode="esno"` stating the level
    once. numpy stays for analysis of the captured series.

    The point is not tidiness: this example exists to show what a receiver's
    telemetry can prove, and a demo that re-derives the transmitter cannot
    catch a transmitter bug — it would agree with itself while both halves
    drifted from the shipped generator.

    `scripts/check_stimulus_sources.py` is the gate for this class and passed
    on the old code, because its `pulse` marker matches a defined `rrc`/`rc`
    function and its `level` marker matches peak-fraction normalisation.
    Widening it is [gh-871](https://github.com/doppler-dsp/doppler/issues/871):
    the hand-computed sigma shape appears in 14 files, so it wants a ratcheted
    marker rather than a drive-by.

    The docstring also claimed the attach registers "all 13 probes" while the
    real figure is 16. It now names the probe families and points at the
    assert that pins the set (`set(series) == set(tlm.probe_names)`), a count
    nobody read back being exactly what went stale. Among the 16 are
    `rx.sync.lock` and `rx.sync.locked` — the timing loop's Gardner
    eye-opening ratio and its de-chattered flag, which no C accessor exposes.

- **Four examples and the M-PSK gallery page build their stimulus with
    `wfmgen` instead of numpy.** Each one shows a different face of the
    generator rather than repeating one recipe, because the examples are where
    a reader learns which face their own problem wants:

    | example                    | what it now teaches                                                                                                                                                              |
    | -------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
    | `mpsk_receiver_demo`       | the `symbols` source — an arbitrary constellation, which is how ONE function serves BPSK, QPSK **and** 8PSK when `modulation` reaches only the first two; plus `snr_mode="esno"` |
    | `costas_demo`              | `snr_mode="fs"` — SNR per SAMPLE, the question a Costas loop actually asks, and a 12 dB different number from `esno` at sps=16                                                   |
    | `symsync_demo`             | `rc_h` and `wfm_awgn_amplitude` — the primitives for a stimulus the composer *cannot* build                                                                                      |
    | `dll_demo`                 | `PN` — a real maximal-length code                                                                                                                                                |
    | `gallery/mpsk-receiver.md` | `level` in dBFS, replacing a bare `* 0.5`                                                                                                                                        |

    Every conversion was verified numerically rather than assumed, because the
    conventions are the part that goes wrong silently: with noise off the
    `symbols` source is **byte-identical** to `np.repeat`, `snr_mode="esno"`
    and the hand-written `sqrt(sps / (2 * 10**(esn0/10)))` both measure
    10.0 dB at the matched-filter output, `snr_mode="fs"` reproduces its hand
    sigma to five digits, and `wfm_awgn_amplitude` matches to eight decimals.

    **`dll_demo` got a real fix, not just a refactor.** Its 127-chip code was
    `default_rng(1).integers(0, 2, 127)` — a coin flip. 127 is 2^7 - 1, so
    `PN(length=7)` fills the period exactly, and the difference is the
    property a delay lock loop runs on: an m-sequence's off-peak
    autocorrelation is a flat **-1** at every non-zero lag, while the random
    code wandered between **-13 and +3**. The discriminator reads that
    sidelobe structure directly, so the demo had been handing itself a worse
    S-curve than any real spreading code would produce.

    **`symsync_demo` is the one that stays hand-placed, and says why.**
    `Segment.sps` is an integer, so no scene can express "the symbol clock
    runs 1.004x fast and starts 1.7 samples late" — which is the question a
    `SymbolSync` exists to answer. Its pulse and its level are now library
    primitives even though its clock cannot be; `rc_h` is documented for
    exactly this, being the analytic raised cosine at arbitrary non-grid
    times, and the private `rc_pulse` it replaces was a transcription of that
    formula.

- **Batch 2 of the numpy-to-`wfmgen` sweep**, four more examples, again a
    different face each:

    | example                | what it now teaches                                                                               |
    | ---------------------- | ------------------------------------------------------------------------------------------------- |
    | `telemetry_fanin_demo` | a **multi-segment scene** — clean, outage, clean as one `Composer`, not three arrays concatenated |
    | `receiver_lock_demo`   | **continuous async DSSS** (`symbol_rate` > 0), the waveform type this demo was hand-indexing      |
    | `rate_converter_demo`  | **`Composer.from_json`** — the face a `--record` document has                                     |
    | `ber_awgn_demo`        | `wfm_awgn_amplitude` at sps = 1, where Es/N0 and per-sample SNR are the same number               |

    `receiver_lock_demo` is the one worth reading. wfmgen has its waveform as
    a first-class type: `--type dsss` with a `symbol_rate` selects the
    continuous form, where the code repeats forever and data rides it at a
    rate that is deliberately not a whole number of chips. That
    asynchronicity is the entire subject of the demo, and it had been
    expressed as index arithmetic
    (`si = floor((idx - 0.37*TE) / tsym)`). All three loops — DLL, Costas
    and SymbolSync — still acquire from a cold start on the generated
    stimulus, which is the assertion that file already carried.

    Two codes stopped being coin flips. `receiver_lock_demo`'s 127-chip code
    and `telemetry_fanin_demo`'s 31-chip one are both `2^n - 1`, so
    `PN(length=7)` and `PN(length=5)` fill their periods exactly. The second
    replaces a hand-written five-stage LFSR whose own docstring explained why
    low autocorrelation sidelobes matter to a CFAR reference — and then
    generated the sequence by hand rather than asking the library.

    **A trap found and documented**: the CLI's `--fs` default of 1.0 does
    **not** reach a JSON scene. `{"type": "tone", "freq": 0.08}` renders at
    DC with no error; `{"type": "tone", "freq": 0.08, "fs": 1.0}` renders at
    0.08. Found because `rate_converter_demo` failed its own frequency check
    with the tone 1245 bins off, which is the gate doing its job.

- **just-makeit pin 0.61.0 → 0.62.0.** Adopts the two doppler-filed fixes that
    the `MpskReceiver`/`MpskReceiverR` collapse
    (`docs/design/mpsk-refactor.md` §6) was blocked on, skipping 0.61.1 (a
    release-notes repair with no template change).

    **jm gh-1012** lets a `[[<obj>.views.methods]]` entry restating a parent's
    name declare its own `arg_type`/`return_type`, bound to its own C symbol
    via `fn` — so two objects differing in one method's dtype and nothing else
    can become one object and one view, under the *same* Python name.
    Previously the only way to express it was a differently-named method
    (`steps_r`), which gives the two faces an asymmetric public surface —
    most of the way back to being a second type. **jm gh-1011** is the fix for
    what made that silent: such a declaration was accepted, written to the
    manifest, then discarded, because the replay copied the parent's entry
    wholesale and kept only `doc`. It is now honoured (with `fn`) or refused
    by name. A doc-only override — same name, same signature — is unchanged.

- **One SNR conversion, not two.** `wfm_snr_over_fs()` (the composer's
    per-segment noise floor, and what `Plan` recomputes a swept SNR through)
    carried its own copy of the mode→SNR-over-fs arithmetic, with a comment
    saying it "mirrors the conversion in `wfm_synth_core.c` … single source of
    truth — no drift". It was a second implementation, and the claim held only
    by inspection: nothing in the tree compared them, `wfm_snr_over_fs` had no
    test and no binding, and the consequence of a divergence is silent — the
    same requested SNR meaning two different noise powers depending on how many
    sources happen to share a segment.

    The arithmetic now lives once, as `wfm_synth_snr_over_fs()` beside the
    generator that owns it, with `wfm_synth_bps()` for the bits-per-symbol rule
    both callers need. What legitimately differs stays an ARGUMENT rather than

- **`wfmgen`, `Synth` and `Segment` silently ignored the frame flags for every
    unspread type** ([#755](https://github.com/doppler-dsp/doppler/issues/755)).
    They were parsed, stored in `wfm_source_t` and readable back — `s.sync`
    returned the sync word — and applied only on `type="dsss"`. Measured before
    the fix:

    ```console
    $ wfmgen --type bpsk --sync 1111100110101 --crc crc16 \
             --acq-code 10101010 --acq-reps 4 --count 256 --output a.dat
    $ wfmgen --type bpsk --count 256 --output b.dat
    $ cmp a.dat b.dat && echo IDENTICAL
    IDENTICAL
    ```

    So a caller who asked for a framed waveform got an unframed one, at exit 0,
    with no warning — a plausible result from a state nobody can defend, which
    is the exact failure `docs/design/rx-test.md` exists to stop.

    One gate caused it (`wfm_synth_bridge.c`'s `if (src->type !=   WFM_SYNTH_DSSS) return 0;`) and nothing else consumed the fields. **What
    let it ship is that no test asserted a frame kwarg CHANGES the waveform** —
    the DSSS path was covered and the unspread path had nothing to be wrong
    about, because nothing looked. The recurrence gate is therefore
    behavioural, at each face: `test_wfm_compose.c` checks the framed stream IS
    `wfm_frame_bits()` of its own descriptor symbol for symbol,
    `test_frame_source.py` checks the composer, the CLI and the record
    round-trip, `test_wfm_synth.py` checks `Synth`, and the flag matrix pins a
    framed `bits` record. Four sabotages, each red on target: reverting the
    attach, silencing the record emitter, silencing the reader, and removing
    the refusal.

    `crc` is deliberately NOT read as intent to frame — it defaults to `crc16`
    on every source, so doing so would have appended a trailer to every
    unframed pattern anyone has ever generated. A preamble or a sync word is
    what says "framed".

    a second formula: the composer resolves `auto` to Es/No for a DSSS source
    and passes the true symbol span, while the generator resolves the same
    source to fs because at `create()` time the codes have not attached yet and
    the spreading factor is unknown.

    `test_wfm_compose` gains the end-to-end check that was missing: for seven
    (type, mode, sps) combinations, the noise a composed source actually
    carries against a HAND-DERIVED expected power. The first version computed
    that expectation by calling `wfm_snr_over_fs()` — which, now that both
    sides share one conversion, moved with it: dropping the bits-per-symbol
    term from the Eb/No branch left the test green. A known answer cannot
    follow the code it checks. With literals it fails by 3.06 dB at QPSK, and
    the Es/No span term turns out to have been guarded all along by the DSSS
    byte-identity assertion in the same file.

- **The M-PSK receiver harness generates with `wfm.Synth` and aligns with
    `BerMeter`, instead of with its own numpy.** Two duplicates, both of which
    had invented a convention:

    `make_signal()` oversampled by `np.repeat`, mixed its own carrier and
    scaled its own AWGN variance under two Es/N0 conventions. It now asks
    `Synth(type="symbols")` — the same `lo`/`awgn`/sample-and-hold the
    receivers under test are built against. **Verifying `snr_mode="esno"` on
    the way through found nothing wrong with it**, which is worth saying
    plainly: it delivers the Es/N0 it claims to within 0.04 dB across m 2/4/8,
    sps 1..16 and Es/N0 0..20 dB, read back with `snr.snr_data_aided_db` at the
    matched-filter output and cross-checked by subtracting the same-seed clean
    run. Nothing had ever checked it, and the check now runs in CI.

    `symbol_metrics()` searched its lag by minimising the error count over
    ±200 — an optimisation over the answer, which false-passes on a lucky
    alignment over garbage and false-floors when the true lag falls outside the
    span. It now detects with `BerMeter.align()` and REFUSES to return a number
    when `align_ok` is false, which is what two of its seven call sites were
    approximating by hand with `abs(lag) < 190`. `coherent_errors()` likewise
    stopped computing a post-marker scoring window by hand: `score()` excludes
    marker symbols itself and reports them in `skipped`.

    No output-rate invariant was added, though the design called for one. The
    detection already refuses every case it would have caught — half rate
    detects at −2.5 dB of margin, double rate at −inf, `m_out` outputs mistaken
    for symbols at −5.6 dB, against +10.5 dB healthy — so a count check would
    have been a second convention for a question `ber` answers. It was written
    twice, once in Python and once as a C module function with its own
    tolerance constants, before being measured; both are reverted.

- **just-makeit pin 0.59.0 → 0.59.1, which retires a doppler-local edit rather
    than adding one.** The whole adoption is deleting nine lines.

    `native/inc/jm_simd.h` carried a local `@code` fence around its usage
    example, plus a note explaining why. Unfenced, that example's `coeffs[k]`
    reaches markdown as prose, CommonMark reads `[k]` as a shortcut link
    reference with no definition, and `zensical build --strict` aborts —
    partially writing `site/`, which is how one real cause produced 864 broken
    links. doppler had been carrying the fence; the 0.58.0 re-vendor destroyed
    it, and CI's Docs job caught the fallout. Filed as
    [just-makeit#968](https://github.com/just-buildit/just-makeit/issues/968)
    and fixed there in 0.59.1, so **the fence is jm's now** and doppler's copy
    is byte-identical to the shipped template again — verified with `cmp`, not
    by eye. `jm_simd.h` therefore leaves the `OUTDATED` list on its own, which
    is the report doing exactly what it exists for.

    Nothing else in 0.59.1 reaches doppler. Its other seven template changes
    are the `jm app` scaffolds (gh-962 — an edit to a generated app is no
    longer silently discarded), and doppler declares no `[app]`:
    `native/src/app/wfmgen.c` is hand-written, and says so in its own banner.

    Two behavioural notes, both pre-checked so neither reads as a regression:
    `.clang-format` now *can* be reported `OUTDATED` (gh-960) and duly is —
    benign, since jm's template is 2 lines against doppler's 21 and
    `c_style`/`c_format_command` mean jm formats generated C with **doppler's**
    committed file, not its own. And a `CMakeLists.txt` that has lost a jm
    splice anchor is now reported as `UNANCHORED` and **gates** (gh-975);
    doppler passes, carrying both `# ── Components` and `# ── Modules`. That
    one is worth knowing about: those read like decorative section headers in
    a hand-owned file, and `# ── Modules` is where every module's
    `add_subdirectory` line is appended. Losing it makes a module silently not
    build while `jm status` reports OK.

- **just-makeit pin 0.57.0 → 0.59.0, and the create-only headers 0.58.0 ships
    are re-vendored by hand.** Everything in 0.58.0 that doppler wants lands in
    *create-only* files — `jm apply` never rewrites those, so
    `make drift-check` stays green whether the migration happens or not, and
    an existing project receives none of it until someone copies the files
    over. That is why this is a manual re-vendor of `native/inc/jm_perf.h`,
    `native/inc/jm_simd.h` and `native/benchmarks/jm_bench.h`, rather than a
    bump that carried itself.

    **0.59.0 adds nothing to re-vendor** — it changes no template at all
    (verified by diffing the two tags' `templates/` trees, which are
    byte-identical), so it is pure tooling on top of the migration below. It
    is taken in the same commit rather than as a follow-up because landing
    0.58.0 first would ship a pin that was already stale, and the whole point
    of the three-site pin gate is that the number means something.

    What 0.59.0 carries that doppler cares about: **`jm status` now names
    which create-only files are behind, as `OUTDATED`** (gh-949) — the direct
    answer to the "not compared: create-only files" line this bump would
    otherwise have left standing. It is *reported, never counted*, because
    `apply` cannot fix a create-only file and gating on it would fail CI with
    no command that clears it; it is suppressible via `status_allow`, which is
    what doppler's `jm_test.h` entry below already does for both its `MISSING`
    and its `OUTDATED` classification. Measuring it also showed the blind set
    was far larger than 0.58.0's own note claimed — `status` cannot see drift
    in **28 of a plain project's 32** manifest-owned files, not five.

    **The first `OUTDATED` report caught doppler's own formatter, not jm.**
    `native/benchmarks/jm_bench.h` was named on a tree whose content is
    byte-identical to jm's — strip all whitespace from both and the hashes
    match. doppler's pre-commit had rewritten it from jm's K&R to GNU, purely
    because `C_EXCLUDE_RE` reached `native/inc/` and nothing under
    `native/benchmarks/` or `native/tests/`. Its siblings `jm_perf.h` and
    `jm_simd.h`, same vintage and same source, were absent from the same
    report — the control that identifies the cause. `jm_bench.h` and
    `jm_test.h` join the exclusion and are restored to jm's render, which is
    what makes the new signal mean something: left formatted, `OUTDATED` fires
    forever on a file nobody needs to act on, and a real upstream change would
    arrive into a line already being ignored. Main tree 9 → 8, downstream
    4 → 2.

    **`jm_*.h` joins `COV_IGNORE`, which had been missing it all along.** The
    re-vendor brought in jm's `jm_dot_f32` / `jm_dot_f64` inline helpers —
    22 lines doppler does not call — and `coverage-gate` failed at **18.5%** on
    a header doppler does not write. The exclusion list already held `vendor/`,
    the `_ext` glue and the test/bench harnesses under a stated rule of *only
    first-party `_core.c` counts*; jm's headers are that same category and were
    simply never listed. It stayed invisible because the patch gate only sees
    lines a branch CHANGES, and until now no branch had changed one. The `jm_`
    prefix is deliberately narrow — `native/inc/` is otherwise doppler's own
    and those inline bodies stay measured, and the SIMD macros doppler really
    uses are unaffected, since a macro is attributed to the `.c` that expands
    it.

    The **9 that remain are deliberate and stay visible**, un-suppressed:
    `.clang-tidy`, `.gitignore`, `Doxyfile`, `Makefile`, `bootstrap.toml`, the
    two `cmake/*.in`, and `native/inc/clib_common.h` — every one a file
    doppler authored and will not adopt jm's version of (`clib_common.h`
    carries the whole `DP_OK`/`DP_ERR_*` vocabulary against jm's 14-line
    stub). `status_allow` would silence them, and that is exactly the wrong
    move: jm ships real improvements to these, and harvesting them is how
    doppler got `make tidy` and `compile-commands` in the first place. An
    advisory line about a config jm has moved on is the notification, not
    noise.

    Also in 0.59.0, and the reason it is worth taking promptly: **`jm method` /
    `property` / `warning` / `error` silently did the wrong thing on a module
    object when `--module` was omitted** (gh-963). They consulted the flag and
    nothing else, so its absence selected the *standalone* path: the verb wrote
    the C stub, never touched the module's binding fragment, printed `Done!`
    and exited 0 — leaving a tree that builds and imports with the member
    missing from the class. doppler's documented add-an-object workflow drives
    exactly those verbs against module objects (`ddc`, `wfm`), so this is a
    trap doppler was standing next to. The owning module is now inferred from
    the manifest, which has always recorded it.

    **`.clang-tidy`'s `ExcludeHeaderFilterRegex` comes off in the same
    commit** — that is the point of the bump here. jm 0.58.0
    (just-makeit#947) renames all 16 reserved identifiers across the shipped
    headers (`_JM_LIKELY_`, `_jm_hsum256_f32`, …) with the public spelling
    unchanged, so the carve-out that had `native/inc/jm_.*` outside the gate
    is no longer buying anything. Measured rather than assumed, and with one
    variable moved: the tidy backlog is **118 findings with the exclusion and
    118 without**, none of them in `native/inc/jm_*`. Every header under
    `native/inc` is in scope again at no cost. The 118 are the pre-existing
    backlog tracked as #723, untouched by this.

    **`native/tests/jm_test.h` is the one create-only file doppler does NOT
    take**, and `make tests-ssot` is what said so — four violations, on its
    `CHECK`, its `REQUIRE` and both retired `ALMOST_EQ` spellings. jm ships
    it because one downstream had accumulated 90 copies of `CHECK` in 6
    incompatible variants; that downstream was this repo, which had
    already fixed it here as `native/tests/dp_test.h` — `DP_CHECK` /
    `DP_REQUIRE` / `dp_nearf`, 90 test files on it, and a gate holding it as
    the single definition. Vendoring jm's harness beside doppler's would put
    two peer assertion harnesses in one directory, so the file is dropped
    and `native/tests/jm_test.h` is `status_allow`-ed as permanently
    missing, which is how the drift gate is told the absence is a decision
    rather than a lapse. The live consequence — a test scaffolded by jm
    0.58.0+ includes `jm_test.h` and must be converted to `DP_CHECK` before
    it compiles, with tests-ssot failing it on the way past — is filed as
    #730 rather than left in this paragraph.

    Two more things checked and deliberately **not** adopted. The scaffold's
    own `.clang-tidy` and `make tidy` are 0.58.0 additions for freshly
    scaffolded projects; doppler's are older, stricter, and scoped to the
    library rather than to the compile database, so they stay. And
    `compile_commands.json` was already settled — it is a symlink into
    `build/`, not a tracked hand-rolled file, so 0.58.0's "delete yours and
    run `make compile-commands`" migration step is a no-op here.

    `examples/downstream-jm` takes **both** files, unlike the main tree: it
    exists to show what jm actually generates, so its copies are the point.

    The headers also lose their `@file` blocks upstream, which is not a
    regression: measured at CI's doxygen 1.9.8, the generated tree builds
    with **0 warnings**, anchors intact and all 40 inbound links resolving.

    `examples/downstream-jm` is a jm project in its own right and is gated by
    the same `status --check`, so its pin moves too.

- **The C tests' randomness has one home, `native/tests/dp_rng_test.h`, and
    one of the copies it replaced was broken.** This is a BEHAVIOUR change to
    `test_costas_core.c`, not a refactor — read the second paragraph before
    reviewing the diff as a mechanical migration.

    The xorshift32 step was written out **twenty times** across `native/tests`
    (inlined nine, wrapped eleven), `prbs` six times under two different
    contracts sharing one name, `cgauss` five times byte-identically, `uni`
    twice, and `gauss` five times in two implementations. Nineteen of the
    twenty files are a bit-exact migration: the shifts, their order, the
    `(x + 1) / (2^32 + 1)` uniform mapping and the draw order are all
    preserved, proved by running each retired implementation against the
    shared one over 12.9 million draws and comparing bit patterns, not
    tolerances.

    The twentieth was `test_costas_core.c`, whose `gauss` was a half-finished
    edit that had been sitting in the tree: it computed `u1`, voided it with
    `(void)u1`, then drew both Box-Muller uniforms from a two-shift recurrence
    that is neither xorshift32 nor full-period, with the second word one
    shift-xor from the first. Measured over 2e6 draws against the N(0,1) it
    claimed to be — mean **+0.056**, variance **1.115**, kurtosis **3.62**,
    two-sigma tail **0.063** against 0.0455. Its one AWGN test therefore ran
    0.47 dB hot on biased, heavy-tailed noise, and a Costas phase detector
    reads a mean offset as signal. Nothing ever failed, because the assertion
    had margin. Re-verified after the fix: tracked frequency 0.00150647 →
    0.00152291 against a 5e-4 tolerance, lock metric 0.9697 → 0.9560 against a
    0.7 floor, bit errors 0 → 0.

    Two further findings fell out of doing it:

    - `test_dp_ber.c`'s Gaussian drew its two uniforms from **unsequenced**
        operands of one `*` expression, so which drew first was the compiler's
        choice and the two orders give different noise (6.3417 against 2.3548
        at the same seed). Now sequenced explicitly, in the order gcc -O2
        happened to pick, so the stream is preserved and stops being a
        compiler's opinion.
    - `test_costas_core.c` asserted `be == 0` on a noise test where the
        expected error count is ~0.1 — a ~90%-per-seed outcome that returns 1
        error at two of seeds 2024..2043. It passed only because the seed is
        pinned, and would have flaked the first time a runner's libm rounded
        differently. Now a bound.

    `make_signal` is deliberately NOT consolidated. It is defined in eight
    files and only two were one function written twice — the byte-identical
    DSSS pair, now `dp_dsss_test.h`. The other six build genuinely different
    signals sharing a name, and folding them together would delete the
    differences each test exists to drive.

    `tests-ssot` now derives its forbidden set from every `dp_*.h` here rather
    than from `dp_test.h` alone, and rejects a new private generator outright:
    an inline xorshift, a hand-written Box-Muller, or either uniform mapping.
    `check_stimulus_sources.py` declines that check at repo scale, where
    hand-rolled noise hits 72 files and a ratchet that large is noise; inside
    `native/tests` the count is now zero, so it is a rule. `test_dp_rng.c`
    pins the integer streams bit-for-bit and measures the distributions —
    including the exact statistics above, so the defect that motivated the
    header is one the gate now rejects.

- **`jb.toml` is now `bootstrap.toml`.** The old name pointed at the wrong
    tool: `jb` reads as just-buildit, the PEP 517 build backend, which has
    never opened this file — it reads `pyproject.toml` like every other
    backend. The file is read by `jbx`/`install-deps` instead, and it declares
    what must exist *before* the language ecosystem's own package manager can
    run. Nothing inside the file changed; `git mv` and the references.

    `standard.mk` re-vendored, since canonical's three references moved with
    it (just-buildit.github.io#20). The old name is still read, and warns, so
    the merge order between this and the canonical publish does not matter.

    **The just-makeit pin moves 0.55.3 -> 0.57.0 in the same commit, because
    the rename is not doppler's alone to make.** jm *generates* this file into
    every scaffolded project, so `jb.toml` is a manifest-owned path: under
    0.55.3 the rename read as `MISSING (1) — jm apply will create: + jb.toml`
    and `make drift-check` exited 1. jm 0.57.0 (just-makeit#935) renames the
    template, which is what makes the rename expressible here at all. It
    carries 0.56.0 as well — a `jm status` NOTE for a `pass_capacity` opt-in
    that cannot take effect (gh-921), and fixes that do not touch the codegen
    doppler exercises. Verified: zero codegen drift across the bump.

    `examples/downstream-jm` is a jm project in its own right and is gated by
    the same `status --check`, so its own copy of the file is renamed with it.

- **Four more CI/release steps became make targets, after an audit prompted by
    the `glibc-gate` work above.** Same rule each time: a step only CI can run
    is a step only CI can debug.

    - **`make package-c-tarball VERSION=x.y.z`** — the released C library
        tarball. It was three hand-written `tar -czf` lines in `release.yml`,
        one per platform, wrapped in two more hand-written manylinux
        `docker run` blocks, and **no target produced it at all**: `package-c`
        stopped at installing to a prefix, and the only `tar` in the Makefile
        was coverage staging. So `release-smoke` could test a *published*
        tarball that nothing could build locally. The platform string is now
        derived from `uname` instead of hard-coded per copy — a leg can no
        longer label itself an arch it did not build on — and the two Linux
        jobs collapse into one matrixed `build-c-linux`, mirroring
        `build-python`'s existing arch matrix.

    - **`make nats-up` / `make nats-down`** — the JetStream broker the
        `nats://` stream tests need. It was `bash scripts/start-nats.sh` plus
        an inline `docker rm -f nats` written two different ways, and
        `gates-check` only scans for `make <target>` — so the one step
        deciding whether a whole transport is exercised was invisible to the
        gate that exists to catch CI-only steps. Both are in `GATES_PROVISION`
        now. `nats-up` is also idempotent, which the script was not: with a
        broker already running it exits **125** (`docker run --name nats`
        refuses the duplicate), fine on a fresh runner and wrong on a dev box.

    - **`make smoke-image KIND=… IMAGE=…`** — the per-arch published-image
        smoke, written out three times in `release.yml`. They already shared
        `smoke-image.sh`; the loop around it was the copy. This file's own
        comments record two bugs that lived in exactly such a re-typed copy
        (the SDK check degrading to a version print, and the #601 quoting
        `SyntaxError`).

    - **`make print-jm-version`** — `release.yml` re-derived the jm pin with
        its own `grep`/`sed` while `JM_VERSION` already read the same file, so
        one pin had two extractions that agreed only by luck.

- **just-makeit pin 0.55.1 → 0.55.2.** Both fixes in it were surfaced by
    doppler's own 0.55.1 bump, and both are docs/formatting only — no
    signature, no behaviour, and every non-whitespace line in the 85 changed
    generated files falls inside a comment or a docstring.

    **gh-917** retires this repo's most recent standing workaround: `c_style`
    now reaches a member reconciled in place, so `jm apply` no longer leaves a
    fragment holding two C styles at once and `make lint-clang-format` is no
    longer a required follow-up. Two independent causes, either sufficient on
    its own — jm's formatter glob predated gh-729's per-object
    `<module>_ext_<obj>.c` fragments and never matched them, and `apply`
    formatted only the throwaway scaffold it diffs against while the
    member-level reconciliation wrote the real tree afterwards. Verified on
    adoption: `make lint-clang-format` changes nothing after `jm apply`.

    **gh-915** makes generated prose name the class you can actually import.
    An object overriding its Python class name (`--class-name DDC`) got the
    class right and every sentence *about* it wrong, so `class DDC:` carried
    "Lets a `Ddc` be used in a `with` statement" and a `Returns` annotation
    naming a type that does not exist.

- **just-makeit pin 0.51.0 → 0.52.0.** Zero codegen drift: `jm apply`
    re-renders six binding fragments in jm's K&R and clang-format returns every
    one of them byte-identical, so only the pins and `uv.lock` change here.

    The bump carries **gh-806**, which doppler filed after being bitten by it
    twice in one component. Renaming a component moves its manifest section and
    native directories, but its C test and benchmark keep their old filenames —
    so `jm apply` materialises scaffolds under the new names and re-renders the
    CMake that builds *those*, leaving the author's real files on disk compiled
    by nothing. The scaffold passes. `ctest` prints "100% tests passed" with the
    real suite missing from the denominator. jm now reports `UNBUILT` (a
    `test_*_core.c` / `bench_*_core.c` no build file compiles) and gates on it
    unconditionally, because a finding that did not fail the gate would
    reproduce the exact silence it exists to break. **doppler is clean on it** —
    the two displaced files were re-homed when Capture went declarative.

    Also `UNPARSEABLE` (gh-785): a `.pyi` that does not parse has no members for
    jm to find, so a render used to replace every hand-owned one in silence.
    doppler has none.

- **The just-makeit pin now has one source of truth.** It is stated in three
    files — `just-makeit.toml`, `pyproject.toml`'s dev group, and
    `examples/downstream-jm/just-makeit.toml` — and nothing checked that they
    agreed. Mutation-verified: reverting the downstream pin alone left
    `make drift-check` exiting **0**, because jm's version-skew notice is a
    `warning:` line in a wall of advisory output and fails nothing. So a bump
    that missed a site shipped green, and `drift-check`'s own comment claiming
    the example "cannot silently document a jm version doppler is not on" was
    an intent rather than a mechanism.

    `scripts/gen_jm_pin.py` derives the other two from `just-makeit.toml` and
    gates the agreement; it runs first in `drift-check`, ahead of the sync, so
    one jm cannot be installed while the manifest names another.
    `scripts/check_version_strings.py` never covered this — it guards
    *doppler's* release version against being hand-typed into docs.

- **A capture's `with` exit finalizes instead of freeing, so the capture
    outlives its block.** `__exit__` on `MemoryCapture` and `Capture` now calls
    `close()`: it drains the tail, joins the writer thread, writes the sidecar
    and detaches from the context — and stops there. The object stays usable,
    so the records and the drop verdict are readable **after** the `with`
    block, which is when a caller wants them. Nothing is freed twice:
    `tp_dealloc` still destroys, exactly once, at collection.

    Before this, a capture you intended to read back had exactly one shape —
    an explicit `close()` and every read from *inside* the block, because exit
    destroyed and the memory flavour only completes on the final drain. Both
    halves of that ceremony are gone; the demo above drops its `close()` call
    and reads after the block.

    Code that relied on the memory being released at exit now releases it at
    collection instead. `close()` remains public and repeat-safe — a second
    call returns the verdict the first computed, which is exactly how the
    teardown avoids closing a capture the block already finalized.

    One line of manifest — `[dp_tlm_capture.destroy] exit = "close"` — and the
    duplicated `error_message` on that table is deleted with it, because the
    teardown inherits the finalizer's diagnostic once the two calls split. A
    hole in the capture raises `ValueError` carrying that message from
    `close()`, `destroy()` and `with` exit alike.

- **`scripts/check_doc_face_parity.py` compares `Raises`, not only
    `Examples`.** The gate asserted one section and was being read as asserting
    the invariant. The inherited-error divergence above reached the raise and
    the runtime `__doc__` as `ValueError` while the `.pyi` said `RuntimeError`
    — one manifest input rendered two ways, one of them wrong — and the gate
    reported "0 divergent" throughout it. It was found by reading, which is the
    thing the gate exists to make unnecessary.

    Both faces are static text the checker already parses: the runtime side is
    the `PyMethodDef` literal in the sacred fragment, the stub side is the
    `.pyi`. No build, no import, no new target — the same script in the same
    wave, comparing one more section, with the section in the key so a method
    can diverge in one and not the other. The failure text now names which fix
    applies, because they are opposite: a divergent `Examples` means a header
    `@code` was edited and the hand-owned fragment did not follow (patch the
    fragment), while a divergent `Raises` means jm rendered one input two ways
    (a codegen bug, which hand-patching the faces into agreement would hide).
    202 methods compared, 0 divergent (12 fragment methods have no stub
    counterpart).

- **just-makeit pin 0.55.2 → 0.55.3.** 0.55.3 is the fix for
    [jm gh-920](https://github.com/just-buildit/just-makeit/issues/920), which
    this repo filed an hour earlier, so the hand-restored two-line block in
    `source_ext_lo.c` and `source_ext_nco.c` comes back out and the carve-out
    is **retired rather than carried**.

    The release's own diagnosis is sharper than the issue's and worth keeping:
    jm gh-607 shipped exact allocation together with a `max_out(state, n)`
    prototype that can *see* the call, and doppler sits in the seam — opted
    into `pass_capacity` while its headers still declare the call-independent
    `max_out(state)`. jm had extended exactness to a value that provably
    cannot depend on `n`, and **a call-independent cap read as a per-call
    bound is a silent truncation**. A state-only prototype keeps the clamp on
    both faces.

    Verified on adoption: the fragments regenerate with the growing form and
    no hand-patch, `NCO.steps_u32(393_216)` returns all 393216 samples again,
    and the #116 large-n tests are green from jm's own output rather than from
    a local patch.

    The `out=` half of gh-920 is unchanged, and correctly so: a request-sized
    buffer is accepted only where the bound is declared per-call, which
    `*_max_out(state)` cannot claim. That is doppler's header shape rather
    than jm's codegen, and the comment in `nco_core.h` now says so instead of
    pointing at the issue.

    Recorded here because it was not: the bump (`4f1eb86b`) moved all three
    pin sites and touched no CHANGELOG, and `changelog-check` cannot see that
    — it fails only on an empty `[Unreleased]`. `gen_jm_pin.py --check` now
    asserts the pinned version appears on a jm-pin line in this file, which is
    the gate that would have caught it.

- **The Python example gate runs in parallel, in two passes: 376.6 s → 20.6 s.**
    `-n auto` on the bulk (each example is already an independent subprocess in
    a throwaway cwd), which took the post-move 68.8 s down to ~13 s — 20.6 s
    for the whole target, downstream example included, with nothing skipped
    and no example weakened.

    **A single `-n auto` would have shipped a broken gate.**
    `ddc_fn_scaling.py` asserts a 2-thread speedup (`su2 > 1.25`) to prove
    `execute()` releases the GIL; under eight xdist workers the workers already
    own every core, so it measured **1.15x** and reported the contention as
    "execute appears GIL-bound". A gate must not fail merely for running, and
    equally must not be weakened to fit its harness — so examples whose
    assertion *is* a timing get a second **serial** pass. Same split, same
    reason, that `make test-python` already applies to the benchmark
    directories.

    Which ones is declared in `src/doppler/examples/.examples-serial`
    (`script.py: reason`, reasons mandatory, same shape as `.examples-skip`),
    and the `examples_serial` marker is applied *from* that registry — so
    **neither pass names a script** and the registry stays the only place a
    name appears. Three meta-tests pin it: an entry with no reason, an entry
    naming a deleted script, and an entry in both registries at once (which
    would read as "runs, carefully" while the example never ran).

    Sublinear scaling is expected and not worth chasing: several examples
    drive `Plan.prepare()`'s own pthread parallel-for, so workers oversubscribe
    by design. What matters is that no single item sets the floor any more —
    the longest example is now `detector2d_acq_demo.py` at ~6.2 s with a smooth
    tail behind it, which is the shape xdist is actually good at. While one
    164.6 s sweep remained, Amdahl put the floor at that sweep no matter how
    many workers were thrown at it.

- **One ruff config, and a lint rule for the Python floor.** There was a root
    `ruff.toml` *and* a `[tool.ruff]` block in `pyproject.toml`. Ruff does not
    merge them — first found wins — so the pyproject block was **dead**, and
    only looked effective because both files happened to agree on line-length
    and excludes. Proved rather than argued: `ruff check --show-settings`
    printed `Settings path: ".../ruff.toml"`. A rule added to pyproject did
    nothing at all, which is what made this worth fixing rather than tidying.

    Consolidated into `pyproject.toml`, which is where CLAUDE.md already says
    it belongs when a project has one. Nothing referenced `ruff.toml` by name
    (no `--config` in the Makefile, pre-commit, CI or scripts).
    Behaviour-preservation is proved by diffing the full 975-line
    `--show-settings` output across the move: the only changes are the settings
    path and the rule below.

    **`FA102` is now selected**, and `target-version = "py39"` is stated
    explicitly rather than inferred from `requires-python`. FA102 fails a
    module that writes `X | Y` without `from __future__ import annotations` —
    valid *syntax* on 3.9 and a `TypeError` the moment the annotation is
    evaluated. That is not hypothetical: it shipped in this branch, every local
    gate passed because they all run on 3.12, and only CI's 3.9 job caught it,
    at import time. The floor is declared, so the linter enforces it instead of
    the version matrix discovering it. Sabotage-proven both directions, and on
    a real 3.9.25 interpreter rather than by argument.

    Not the whole `FA` set: its sibling FA100 is cosmetic ("you could simplify
    `typing.Optional`") and selecting it would rewrite an unrelated example as
    a drive-by. A correctness rule and a style campaign are separate decisions.

- **A committed test artifact left the repo root.** `agc_step_response.csv`,
    206 KB, added by `f79ac33f` — the very commit that introduced the
    fail-closed example gate on the principle that an example must not pollute
    the repo. Nothing reads it: `build/examples/c/agc_demo` writes it into
    whatever cwd it runs from, and `docs/examples/c.md` says to run it from the
    root. Removed, with `/*.csv` added to `.gitignore` beside the existing
    `/*.png` and `/bench_*.json`. Root-scoped, so the validation trees'
    deliberately committed `*/data/*.csv` are untouched.

- **`async_dsss_receiver`'s lock EMAs run the shared primitive —
    bit-identical.** First of the sites in #698, and deliberately the one that
    cannot change behaviour: the power-weighted `lock_num`/`lock_den` pair was
    already the incremental form, and `lock_alpha` is `1/DWELL` with the dwell
    fixed at 30, so `ema_step`'s `alpha >= 1.0` branch is unreachable here.

    Bit-identity was measured, not argued: a 200-symbol run at CN0 90 dB-Hz /
    Doppler 40 Hz / seed 3, `lock_metric` sampled every 4096 samples (489
    chunks, 475 nonzero), packed to raw doubles and compared byte for byte.
    **And the comparison was proven live before being believed** — a signature
    that never changes is exactly what a stale build produces, so halving
    `lock_alpha` inside the migrated call was checked to move it first.

- **`ACC_TRACE_EXP` runs the shared EMA, and gets more accurate.** It was the
    library's only `alpha*x + (1-alpha)*acc` site; migrating it to `ema_step`
    moves it to the incremental form, which `docs/design/ema.md` §3 measures
    as the more accurate of the two.

    Measured on the real consumer, in C, because Python cannot see it:
    `acc_trace_value()` returns **float32** and the two forms differ at
    ~1e-16, so a Python before/after reads ~1e-7 at every alpha — pure
    quantisation — and would have shown "no change" while proving nothing.
    Against a `long double` reference over 4000 frames, the error improves at
    every coefficient and **most where the design predicted**: 43x at
    `alpha = 1e-5` (6.5e-13 → 1.5e-14), 2.7x at 1e-3, 1.8x at 0.01, 3.7x at
    0.2. A spectrum trace at 1e-5 is exactly the long-average case the old
    form was worst for. That is an independent confirmation of the design's
    §3 table, which was measured on the primitive in isolation.

    `ACC_TRACE_MEAN` (Welford) is untouched — a different recursion with its
    own `1/count` weighting, not an EMA — as are `MAXHOLD`/`MINHOLD`. The
    float32 readback means no downstream consumer can observe the change; it
    is an accuracy improvement in the accumulator's own state, which is where
    a long trace's error actually accumulates.

- **The AGC detector runs the shared EMA, and its `decim == 1` pole becomes
    exact.** Recorded as finding F2 of the EMA's validation report. Both
    paths move onto the primitive: `agc_steps`'s chunk pole was a repeated
    multiply of `(1 - alpha)` followed by `1 - ac` — catastrophic
    cancellation at `d == 1`, where the answer must be `alpha` itself:
    **6 ulps off at `alpha = 0.05`, 2556 at 6.25e-5** (the AGC's real
    bandwidth inside the RateConverter cascade) and **26865 at 1e-5**. It is
    now `ema_alpha_decim(alpha, d)`, exact at `d == 1` by construction.
    `agc_step` was already the exact per-sample recursion, so that is a pure
    substitution which additionally makes `alpha == 1` exact.

    What this fixes, stated precisely: it makes `decim = 1` genuinely the
    undecimated recursion. That is a property, not a numeric shift — the
    decim table is unchanged to three decimals and full-precision readback
    moves only in the last digits (`gain_db` -123.40348827166616 →
    -123.40348827170807 at `alpha = 6.25e-5`).

    What it does **not** fix, measured rather than assumed: `agc_steps(decim=1)`
    and `agc_step` still disagree, and the pole was never the cause. The two
    paths apply GAIN differently — `agc_steps` ramps it across the chunk with
    a first-order hold, `agc_step` refreshes it per period — so the powers
    reaching the detector differ. At `alpha = 1e-5` the `p_avg` gap moves
    only 4.16e-11 → 3.96e-11. Worth knowing before anything asserts
    bit-exactness between those two paths, because it would fail for that
    reason and not for a defect. The loop-filter gain `k_d` is deliberately
    untouched: a linear approximation of a different quantity, tracked as
    #699.

### Removed

- **Two installed public headers that declared an API the library does not
    define** (#801). Both were installed by the `native/inc/**` rule and
    published to `docs/c-api/`, so a downstream C user could read them,
    include them, and fail at link time.

    **`native/inc/telemetry/tlm_recorder.h`** declared seven
    `dp_tlm_recorder_*` functions with full Doxygen; all seven were absent
    from every first-party artifact (`libdoppler.a`, `libdoppler.so`,
    `libdoppler_stream.a`, `libdoppler_stream.so`) and no implementation
    existed anywhere.

    It was **superseded, not unbuilt** — the capability ships as
    `dp_tlm_capture_*` (`open`/`open_memory`, `close`, `count`, `dropped`,
    `records`, `destroy`, plus `read`/`read_max_out`) with a Python face
    (`Telemetry`, `MemoryCapture`, `Capture`) and a worked example in
    `src/doppler/examples/mpsk_telemetry_capture_demo.py`, which answers the
    header's own rationale directly: the capture sizes its own ring and drains
    at every block boundary, so a drop is impossible rather than unlikely.

    The shapes differ because **telemetry is attach-on-demand**. Probes attach
    at runtime and one attach forwards to an object's children — a single
    `MpskReceiver` attach registers 13 — so the ring can only be sized once
    the probe table exists. That is why the shipped API opens a capture
    *after* the attach; `dp_tlm_recorder_create(t, path, block)` sizes at
    construction and structurally cannot express it. Keeping it would have
    pointed readers away from a working API toward a shape the model rules
    out.

    **`native/inc/stream/stream_core.h`** was a 21-line jm scaffold whose only
    content was `/* Declare module-level functions here. */`. The `stream`
    module is real (`stream_core.c`, `stream_nats.c`, `tlm_sink.c`); this was
    the per-module public header nobody filled in, and `[module.stream]` is
    `no_generate`, so jm does not put it back.

    Their four generated `docs/c-api/` pages go with them, via
    `make gen-c-api`. Nothing in the tree included either header.

- **`nda_tap` is gone; the carrier discriminator reads the on-time strobe.**
    The knob offered three nodes and the other two are deleted with it, along
    with `MPSK_RX_NDA_TAP_*`, the `mf_out`/`mf_in` code paths, and the
    `rx_nda_tap.c` harness that ranked them
    ([#832](https://github.com/doppler-dsp/doppler/issues/832)).

    Measured on the receiver's own waveform — NRZ, modulation off then dense,
    under a coupled Doppler ramp — the strobe won on every axis: lock **+0.860**
    at the data onset against `mf_out`'s +0.478 and `mf_in`'s +0.417. Its
    timing dependency costs nothing exactly where timing is impossible, because
    an unmodulated carrier is sampling-phase invariant.

    The pull-in range the taps traded for is not lost, it is derived: an
    M-th-power detector updating at `F` sees `|Δf| < F/(2M)`, and the strobe
    fixes `F = Rs`. A caller needing more states the requirement and gets a
    loop that meets it or a refusal (`docs/design/mpsk.md` §8.2).

### Fixed

- **`ciccompmf`'s header described a contract it does not have.** It said
    "M outside the Bernoulli table range leaves out unmodified"; it writes
    M **zeros**. A caller who pre-filled a fallback design and trusted that
    sentence got it silently replaced with the all-zero filter — a muted
    signal path rather than a degraded one. The range is also per-parity —
    odd M up to 19, even M only up to 18, since the Bernoulli table is nine
    entries — which the flat `[1, 19]` did not say. The doc now states both,
    and `test_resample_core.c` pins them.

- **`kaiser_num_taps(0, …)` crashed the process, and a Python shadow hid
    it.** Two defects, both found on the first run of the C test jm's
    gh-1034 now generates for a function-only module.

    The function ends in `htaps / (size_t)num_phases`, an INTEGER divide, so
    `num_phases == 0` was not a wrong answer but a **SIGFPE** — and this is a
    public module function, so a Python caller passing 0 took the
    interpreter down rather than getting an exception. A value below 1 is
    not a bank; it now returns 0, and the header says so.

    Nobody had seen it because `src/doppler/resample/__init__.py` **imported
    `kaiser_beta` and `kaiser_num_taps` from the extension on line 20 and
    then redefined both in pure Python below it.** Every Python caller got
    the shadow; the C implementations were bound, exported, and unreachable.
    The two had already drifted in exactly the way duplicated logic does —
    the C one crashed where the Python one raised `ZeroDivisionError` —
    while agreeing on every value either was ever asked for, which was
    verified across a sweep of both before the copies were deleted. The
    imports now stand alone. A module `__init__.py` is a re-export and
    nothing else.

- **Four benchmarks wrote their results under a name nothing reads.**
    `jm_bench_write_json(&b, "X")` writes `bench_X_core.json`, and both
    collectors open `bench_<component>_core.json`. `bench_hbdecim_core.c`
    passed `"hbdecim_core"`, `bench_resamp_core.c` passed `"resamp_core"`,
    `bench_awgn_core.c` passed `"bench_awgn_core"` and
    `bench_wfm_synth_core.c` passed `"synth"` — so each ran, printed its
    table, and had its JSON silently not found. **`awgn` and `wfm_synth` are
    real, measuring benchmarks that jm runs on every `make bench`, and
    neither has ever reached a C snapshot** — verified against
    `benchmarks/history/20260724T231732Z-c.json` and
    `benchmarks/published/v0.37.3/*-c.json`. Both components do appear in the
    published *Python* snapshots, so what was lost is the C-level row, not
    the component. Rule 4 of the new gate is what found them.

- **`snr_data_aided_db_series` and `snr_m2m4_db_series` are O(n · window)**
    ([#890](https://github.com/doppler-dsp/doppler/issues/890), filed not
    fixed). Both re-scan the whole window at every output sample: measured at
    277x / 898x / 2760x the whole-block estimate for windows of 256 / 1024 /
    4096, a ratio linear in the window, as an O(n · window) loop must be. Both
    estimators are sums and slide in O(n). Found by the new `snr` benchmark on
    its first run, which is what the two rows were written to ask.

- **The assertion ratchet compares against the merge base, not `origin/main`'s
    tip.** It asks whether *this branch* removed assertions, and the only honest
    baseline for that is where the branch started. Against the tip, a branch that
    is merely **behind** fails for assertions someone else *added* — naming a file
    it never touched, under the message "a test file LOST assertions".

    Measured twice on one branch: `test_mpsk_receiver_core.c` at −13 while `main`
    was 19 commits ahead, then `test_mpsk_core.c` at −10 after a soft-demapping
    commit landed 30 ahead. Neither file had been edited on the branch, and both
    "fixes" were a rebase — which is the tell: a gate whose verdict changes when
    you rebase is measuring the gap to `main`, and `git` already reports that.

    The cost is not the false alarm, it is the habit: a gate that cries wolf for a
    reason unrelated to the diff trains the reader to rebase-and-ignore, which is
    exactly how a real lost assertion would slip past. Its own docstring says a
    suite can go green while covering less; a ratchet can go red while nothing was
    lost, and that is the same defect from the other side.

    Proven both ways in a worktree pinned behind `main`: 51 assertions in the tree
    against 61 at the tip (**false FAIL**) and 51 at the merge base (**correctly
    quiet**), and removing three assertions there still goes red. Falls back to
    the given ref when no merge base exists, so an uncomputable baseline is not
    silently skipped. The messages now name which baseline was used.

- **The `doppler_channel` C benchmark published a ratio its own prose
    contradicted, and now does not.** It reported `ramp/static = 0.70x` — "a
    drifting offset is 30% cheaper than a fixed one" — directly beside a
    paragraph asserting the two cost the same. The paragraph was right.

    The benchmark already warmed up per configuration, which warms the caches
    for the config it precedes. What it could not warm is the CPU's frequency
    ramp out of a cold process, and that is charged entirely to whichever
    configuration runs *first*. `MIN` over rounds cannot remove it either,
    because every round in a cold process is equally cold — the usual defence
    against a slow outlier is no defence against a slow *start*.

    A process-level warm-up before any configuration is timed takes the ratio
    to **1.00x**, reproducibly. An instance of the class filed as
    [#896](https://github.com/doppler-dsp/doppler/issues/896), fixed here for
    this benchmark; the general case is still open.

    Worth knowing when reading any multi-config benchmark in this tree: a
    ratio *below* 1.0 against the first row is the signature, and it is a
    measurement artifact rather than a finding.

- **`make test-examples-c` discovers the C examples instead of listing
    them, and runs each under a deadline.** It iterated a hand-written list
    of nine binary names, so the other four compiled, shipped, and were
    executed by nothing — with no reason recorded, nothing failing if a
    fifth joined them, and nothing noticing if one was deleted. A C example
    is documentation that claims to be executable, so one that runs nowhere
    is the shape this repo already calls indistinguishable from a gate
    passing.

    Discovery is over `examples/c/*.c`, so a new example is gated the moment
    it exists. Opting one out costs an entry in `examples/c/.examples-skip`
    with a **mandatory reason** — the same contract
    `src/doppler/examples/.examples-skip` already holds the Python side to,
    and the mechanism this mirrors.

    Writing the four exclusions down turned out to separate them.
    `pipeline_demo` is PUSH/PULL between two **in-process** threads: it needs
    a broker but no peer, and it exits on its own. So it takes the
    conditional `broker:` idiom — it now RUNS wherever `127.0.0.1:4222`
    answers, which CI arranges. Ten examples run where nine did. The other
    three print *"Press Ctrl+C to stop"* and have no exit condition, so no
    broker makes them terminate; their round-trips stay covered by the
    stream suite and `make docker-stream`.

    Four properties, each failing rather than warning: a stale waiver, a
    waiver with no reason, a source that produced no binary (the same
    fail-open bug one layer down, in `examples/c/CMakeLists.txt`'s own hand
    lists), and running nothing at all. Every run is under a deadline, which
    is load-bearing rather than defensive — the cheapest way to reintroduce
    "an example nothing runs" is an example that runs forever, and without a
    deadline the gate hangs instead of failing, reading as *still working*
    until CI's own ceiling kills the job and names the wrong thing.

    The deadline runs through `scripts/with-deadline.sh` rather than
    `timeout(1)` directly. `timeout` is coreutils and is absent from
    GitHub's macOS runner — `gtimeout` too — which that script already
    measured and already solves behind one contract, POSIX-watchdog fallback
    and the 124 exit code included. A bare `timeout` there does not time
    anything out; it fails with 127 and reports the example as broken. The
    discovery loop is likewise a `while read` rather than `mapfile`, which
    is bash 4 against the runner's bash 3.2.

- **The Rust lockfile went back to a format the distro cargo can read, and the
    CI image lost 860 MB.** One defect, two symptoms
    ([#887](https://github.com/doppler-dsp/doppler/issues/887)).

    `ffi/rust/Cargo.lock` had drifted to format **v4** — written by whichever
    modern cargo last resolved anything — and cargo refuses v4 below 1.78,
    while apt ships **1.75 on both Ubuntu LTSes**. So anyone who provisioned
    from `bootstrap.toml` and ran `make test-rust` got

    ```text
    error: failed to parse lock file at: ffi/rust/Cargo.lock
    Caused by:
      lock file version 4 requires `-Znext-lockfile-bump`
    ```

    CI never saw it: the hosted runner carries a rustup cargo that shadowed
    apt's. Moving CI into a container removed the shadow and the failure
    surfaced on both Ubuntu legs at once, while macOS stayed green.

    **Nothing needed v4.** The crate is edition 2021 with two dependencies,
    and cargo 1.75 compiles the whole tree in under four seconds — measured
    before choosing the fix, because the alternative (provision rustup
    everywhere) is a much larger change to justify on a guess. The lockfile is
    v3 again by a one-line change that touches no dependency version, and
    `Cargo.toml` now declares `rust-version = "1.75"` so the floor is stated
    rather than implied.

    `make cargo-floor-check` holds it. Cargo rewrites the lockfile to v4 the
    first time a modern one resolves anything, silently, and a lockfile is not
    a file anyone reads — so the bump is invisible and the failure lands far
    from its cause. Both halves are why it is a gate rather than a note.

    The image benefits twice over: the rustup toolchain it had grown to work
    around this — **613 MB, the largest single thing in it** — is gone, taking
    `deploy/docker/Dockerfile.ci` from **3.17 GB to 2.31 GB**. What remains is
    what CI genuinely uses: llvm/clang for coverage, the distro Rust for
    `make test-rust`. `deploy/docker/README.md` now carries `doppler-ci`
    beside `doppler-glibc228` — the two images that bake nothing in — with the
    size breakdown and why it is one image rather than one per job shape.

    **The bespoke pinned-doxygen image is retired with it.** That image
    (556 MB) existed to hand `make gen-c-api` and `doxygen-check` a doxygen
    matching CI's — a question the CI image answers *by construction*, since
    CI's doxygen job runs inside it. The `doxygen-check` fallback was worse
    than redundant: it ran `apt-get install doxygen` inside `ubuntu:24.04` on
    every invocation, the same provision-at-runtime pattern the image removed
    from CI. Both paths now shim `$(CI_IMAGE)`, the version assertion moved
    with them (an upstream bump fails loudly instead of quietly changing
    generated output), and the retired names are registered so they cannot
    creep back.

- **`carrier_acq`'s state blob carried seven undefined bytes.**
    `carrier_acq_extra_t` puts a `uint8_t ready` in front of a `double`, and
    `get_state` writes the struct whole — so the seven bytes of padding the
    compiler inserts went into the blob holding whatever the stack last left
    there. A designated initializer zeroes the members it does not mention;
    padding keeps *unspecified* values (C11 6.7.9p10), and compilers differ on
    whether they zero it anyway.

    Found by this branch's fidelity check the moment it existed, and found the
    way this class always is: **green on macOS and on gcc 15, red on both Linux
    runners**, because whether two blobs of the same state compare equal
    depended on the compiler rather than on the object. Both DSSS receivers
    already declare an explicit `_pad[7]` for this reason; `carrier_acq` did
    not.

    The member is now explicit, which is a **format-preserving** fix — those
    seven bytes are the same seven the compiler was already inserting, at the
    same offsets, so no blob changes size or layout. They are merely defined.

- **`carrier_nda_core.h` cited the wrong design section three times, and
    `make doc-sections-check` now catches that class.** All three pointed at
    `docs/design/mpsk.md` §2.3 — "The invariant", which is about rate-keyed
    constants — for the one-AGC-per-receiver argument, the squaring-loss
    measurement, and the lock statistic's H0 variance. All three arguments
    are in the document, in §3.2 and §4.2.

    That is not a cosmetic slip. #796's sibling issue was filed reporting
    that the `~6 dB Es/N0` floor "has no measurement behind it that I can
    find anywhere in the tree"; §3.2 carries a measured table of loop SNR
    against the un-normalised form, six Es/N0 rows by three constellations,
    4e5 samples per point. **A citation reads as authority, so one pointing
    at the wrong argument is worse than none — the reader concludes the
    claim is unsupported.**

    The gate checks that a `docs/x.md §N` citation names a section that
    exists — 81 of them across the tree, which nothing checked before — and,
    when the citation also names the section's title, that the title matches.
    The title half is what catches a wrong-but-existing number, which is
    every one of the three above; it is optional, so each citation that gains
    a title is coverage that cannot regress.

- **The amplitude note now separates scale from Es/N0.** It read as though
    section 9 of `test_carrier_nda_core.c` established that loop gain is
    independent of signal level. It does not, and cannot: it scales a clean
    phasor, holding signal and noise in one ratio. Per-sample division by the
    instantaneous `|s+n|` is a hard limiter, so the S-curve slope genuinely
    does depend on Es/N0 — which is measured, in the §3.2 table now cited.
    Section 9 keeps its place as a **float-range** gate: proven by sabotage,
    un-hoisting the divide makes it fail, because forming `|z|^M` at the end
    returns 0 below `|z| = 0.032` and NaN above 1e4 at M = 8.

- **The CCSDS Reed-Solomon tables were derived under a data race, and
    `make test-tsan` now exists to say so.** `native/src/ccsds_tm/rs.c` built
    its field behind a plain `ready` flag: two threads reaching any entry
    point first would both see `ready == 0`, both call `rs_init`, and — the
    part that makes it undefined behaviour rather than a wasted
    initialisation — one could read the tables while the other was still
    writing them. Now `pthread_once`.

    It survived because it was unreachable: nothing called the encoder. Two
    things already in the tree make the first call the racy one —
    `dp_parallel.h` fans per-source signal builds across cores, and every
    block method declares `nogil = true`, so a Python encoder driven from a
    thread pool is the same race with a different scheduler. A first call is
    exactly what a freshly imported module makes.

    Precomputing the tables as `static const` would also have been
    thread-safe, and was rejected: it moves `g(x)` from something *derived*
    to something transcribed, and the derivation is what `test_ccsds_tm_rs`
    holds to Annex G. Thread safety should not cost the evidence.

- **Four `ccsds_tm` header claims that nothing asserted.** The claim
    inventory `docs/dev/contributing/validation.md` step 1 asks for, run against
    `ccsds_tm`'s three headers, found four rows the C tests did not cover —
    two of them the shapes that page warns about by name.

    **The dual basis was a consistency test.** Requiring the two transforms
    to invert each other is satisfied by *any* invertible 8×8 GF(2) matrix
    and its inverse, so it could not see a defect the two halves share.
    Demonstrated rather than argued: reading 4.3.9.3's two equations the
    wrong way round — the likeliest transcription error — leaves an exact
    inverse pair and the old check stayed green.

    Replaced by a **derived** check. Every GF(2)-linear functional on GF(2⁸)
    is `u -> Tr(c·u)` for a unique `c`, so the transform's eight output bits
    are eight field elements; the test solves for them from the shipped
    matrix and the shipped field — using `rs_core`'s own tables, not a
    private multiply — and asserts the structure a dual basis has: `c_0 = 1`,
    `c_j = c_1^j`, and `Tr(c_i · β_j) = δ_ij` read through the *other* matrix
    so both transcriptions are covered. Measured, `c_1 = α^117`, which is
    **not** primitive (`gcd(117, 255) = 3`) and does not need to be.

    **And the published oracle, which the derived check could not supply.**
    4.3.9.3's two matrices are now transcribed into the test as printed bit
    rows — the way `asm_published` and the randomiser's `published40` prefix
    already are — and checked row by row and across all 256 values. Both
    match the shipped pair exactly, all sixteen rows.

    The two are kept side by side and are not redundant. The transcription
    says these are *CCSDS's* matrices; the derivation says they are a dual
    basis *at all*, and would still catch a pair mis-transcribed the same way
    in both the implementation and the test — which a second transcription
    cannot. Closes gh-861.

    **`asm_find` promises FIRST below threshold, not best**, and nothing
    tested it: every case put one marker in a zero background, where the two
    are the same offset. Now two markers with the *earlier* one damaged, plus
    the same stream at a tighter tolerance so a search hard-wired to the
    first offset fails too. It matters because a best-match search has to see
    the whole stream before it can answer, which a frame synchroniser on a
    live capture cannot do.

    **The interleaver's differential ran no library code.** The section named
    "what interleaving is FOR" computed `b % DEPTH` in a loop and asserted
    arithmetic about its own loop — it held for any interleaver, including
    one that did not interleave. Now measured through `encode_block` /
    `decode_block`: a burst of `depth × E` is repaired in full and one symbol
    more costs exactly one codeword, at **every** depth 4.3.5.1 allows —
    which also closes depths 2, 3, 4 and 8, exercised nowhere before — plus
    the differential itself, an 80-symbol burst that depth 5 carries and
    depth 1 refuses at identical rate.

    Every one proven by sabotage: a flipped matrix bit, a self-consistent
    wrong pair, a best-match search, an interleaver that does not interleave,
    and an accepted out-of-range depth.

- **The coverage job's exit code stopped being thrown away a second time**,
    and a gate now holds the rule for every workflow step. Removing the
    leading `-` from the recipe's pytest fixed one discard; the step around
    it was doing the same thing one layer out:

    ```yaml
    run: make coverage | tee coverage.txt
    ```

    Actions runs a `run:` block under `bash -e`, where a pipeline reports the
    **last** command's status — so this step was green whenever `tee` was,
    which is always. `make coverage` had in fact failed: the install of the
    instrumented `wfmgen` died on a directory that does not exist in a clean
    checkout, no report was written, and the only symptom was the patch gate
    one step later opening a `coverage.lcov` nothing had produced. A missing
    number is harder to notice than a wrong one.

    Both halves are fixed. The step declares `shell: bash` (GitHub's alias
    for `bash -eo pipefail`), and the recipe creates the destination
    directory instead of relying on one. That directory is the whole reason
    the failure was CI-only: the build bundles `wfmgen` into the copied
    package, the purge that keeps deleted tests from lingering removes it —
    it is not a `*.so` — and takes the emptied `wfm/_bin/` with it, after
    which the directory came back only from `src/doppler/wfm/_bin/`, which is
    `.gitignore`d and therefore present on a developer's tree and never on a
    runner.

    The gate is `make lint-ci-pipefail`
    (`scripts/check_workflow_pipelines.py`): every step whose script contains
    a shell pipeline must have pipefail in effect, via `shell: bash`, a shell
    string that names it, or `set -o pipefail`. Registration-free — it walks
    every workflow and composite action, so a new file is covered the moment
    it exists. It reads shell quoting rather than searching for `|`, because
    a `jq` filter carries one as data and a pipeline inside `$( )` discards
    its status just the same; that distinction is not academic, since it
    found two more real discards in `release.yml`, both now declared.
    Sabotage-proven: restoring the bare `make coverage | tee` line turns the
    gate red, and `src/doppler/tests/test_ci_pipefail_gate.py` drives it over
    seeded YAML so it is exercised against a step that must fail, not only
    against a tree that passes.

- **`conv`'s claim inventory, and the four things it found.** The first two
    steps of `docs/dev/contributing/validation.md` — enumerate the header's claims, map
    each onto `test_conv_core.c` as pinned / pinned-only-at-literals / absent,
    then write and **sabotage** a test for every uncovered row.

    **`conv_outputs` and `conv_next_state` had zero mentions.** The file
    docstring calls the first "the only place that says what this family of
    codes emits" and the register convention "load-bearing", and both were
    exercised only *through* `conv_encode` — the one caller that agrees with
    them by construction. A user building a trellis (which is what
    `viterbi_create` does) reads them directly. Now the trellis is run BY
    HAND from the two of them and required to reproduce `conv_encode` symbol
    for symbol, at three codes, plus that a state IS the `k-1` previous
    inputs with the newest in the high stage.

    **The LLR sign convention was pinned only against the test's own
    helper.** Every section fed LLRs through one `to_llr`, so a decoder and a
    helper that flipped TOGETHER passed all of them — measured, not
    theorised: flipping both leaves every pre-existing section green. The
    identity code (`k=2, n=1, poly={0b10}`) closes it without importing
    anything, because a maximum-likelihood decode of the identity code is
    exactly a hard slicer, so the decoded bits must equal `llr < 0` element
    for element.

    **`d_free` was an explicit unknown** (`docs/design/viterbi.md` §8) and is
    now measured against published values: **10** for CCSDS's (171,133) K=7,
    5 for the K=3 (7,5), 6 for the K=4 (15,17).

    **And one claim was simply wrong.** The header said "the first `depth`
    bits of a stream produce no output"; the traceback walks `depth - 1`, and
    `viterbi_decode_max_out` agreed with the code. The test pinned the two
    against each other, so nothing could see the prose was off by one — 493
    bits come out of 500 symbols at depth 8, not 492. The prose moved, and
    the count is now pinned against a literal as well as against the sizing
    function.

- `make coverage` no longer hangs on any machine where `DEBUGINFOD_URLS` is
    set — which on Ubuntu is every machine, because
    `/etc/profile.d/debuginfod.sh` exports it into every shell. `llvm-cov`
    consulted debuginfod for each of the 33 objects it opens, three times over,
    looking for debug info that cannot exist: the objects were built locally
    minutes earlier and carry their own coverage mapping. Every lookup was a
    network round trip ending in a timeout. Measured: one extension `.so` took
    **over 120 s and produced no output**, against **0.105 s** with the
    variable cleared; the full report/show/export trio over all 33 objects now
    runs in **0.275 s**. The coverage recipe clears the variable for the LLVM
    tools only, so a developer's own debuginfod setup is untouched everywhere
    else.

- **`make coverage` stopped ignoring pytest's exit code**, and the four causes
    behind the failures it was hiding are fixed rather than tolerated. The
    recipe carried a leading `-`, so make discarded the result in the one job
    that produces the coverage number — 89 results (27 failed, 62 errors)
    against 2631 passed, invisible.

    | cause                                                      | count | fix                                                         |
    | ---------------------------------------------------------- | ----- | ----------------------------------------------------------- |
    | repo root resolved by counting directories                 | 81    | `repo_root()` walks up (separate change)                    |
    | the **normal** `wfmgen` shadowing the instrumented library | 7     | install the instrumented binary into the copied tree        |
    | a threading speedup measured under profiling               | 1     | withhold that one assertion when `LLVM_PROFILE_FILE` is set |
    | files deleted from `src/` surviving in the copied tree     | —     | clear the python half before the extract                    |

    The binary one is invisible without a byte comparison: the tar copy
    excludes `*.so` but **not executables**, so a 655 KB gcc/optimised
    `wfmgen` sat beside a 1.49 MB clang/Debug library, and every test
    asserting byte parity between the CLI and the library compared two
    different builds of the same source.

    The scaling one is the only case where the TEST is wrong under coverage
    rather than the environment. `-fprofile-instr-generate` makes every
    counter update an atomic on a page shared between threads, so two threads
    serialise on the profiling runtime instead of the GIL: 0.98x against
    ~1.9x uninstrumented. Asserting anyway would turn a threading claim into
    a measurement of llvm's counters. The assertion stays live when not
    instrumented — verified, because a skip that quietly disarms an example
    is worse than the failure it silences.

    The fourth had no failure count because nothing measured it. The extract
    is additive, so anything deleted from `src/` kept running from the copy —
    found when a temporary sabotage test, removed from `src/`, failed the
    next run anyway. **A deleted test that keeps passing is worse than one
    that keeps failing**: it reports coverage for source that no longer
    exists.

    Sabotage-proven both directions: a deliberate failing test now gives
    `COVERAGE_RC=2`, where before the same failure exited 0.

- **The coverage run no longer borrows the developer's machine.** With its
    exit code finally being read, the job reported 44 failures and 6 errors —
    none about the code under test. Three gates asked for artifacts in
    `build/`, which this job never builds (it builds `build-cov`), and four
    asked for console scripts on `PATH`:

    | asked for                                            | who                               | why it passed locally                   |
    | ---------------------------------------------------- | --------------------------------- | --------------------------------------- |
    | `build/libdoppler.a`                                 | the C doc-snippet gate, 33 blocks | an ordinary build tree is sitting there |
    | `build/native/validation/validate_{conv,rs}_certify` | the conv/rs certify harnesses     | same                                    |
    | `wfmgen` on `PATH`                                   | 9 sh doc fences                   | an activated venv                       |
    | `doppler-source` / `doppler-fir` / `doppler-specan`  | the cli block tests               | same                                    |

    The build tree is now one derivation, `doppler.tests._repo.build_dir()`,
    reading `$DOPPLER_BUILD_DIR` and falling back to `<repo>/build`. That
    variable is not new — `ffi/rust/build.rs` has always read it and the
    coverage recipe already exported it for the cargo leg; the Python gates
    each spelled `build/` themselves instead. The recipe now exports it for
    the pytest leg too, and prepends the instrumented `wfmgen` and the venv's
    `bin` to `PATH`.

    The C snippets are **run instrumented** rather than excluded: a snippet
    cannot link against a clang source-based archive without
    `-fprofile-instr-generate -fcoverage-mapping`, so the gate adds them (and
    prefers clang) when `LLVM_PROFILE_FILE` says the run is instrumented.
    Each snippet then writes its own `.profraw` into the directory the recipe
    merges, so 33 documented C examples contribute to the number instead of
    being a hole in it.

    This is the same class as the two exit-code discards on either side of
    it: what a gate reads from its environment has to come from the run, not
    from whoever happens to be running it.

- **`DelayCf64.ptr()` accepts the keyword its own type stub publishes, and
    a bare `ptr()` no longer promises one sample.** The stub said
    `ptr(count=1)`; the binding accepted `n=` and returned the whole window.
    Both halves were wrong, and in opposite directions — a caller following
    the stub got `TypeError: 'count' is an invalid keyword argument`, while
    one who found `n=` was told the default was 1 when it was `num_taps`.

    The keyword is now `count`, which every published face — the stub, the
    runtime docstring — had said all along; nothing in the tree passed it by
    keyword, so no caller moves. The default is now **declared** rather than
    hand-restored after each regeneration: `count_default` in
    `objects/delay.toml` is a C expression (just-makeit gh-1051), which is
    what lets an instance-derived default live in the manifest at all. The
    hand-patch it replaces is gone, so it cannot drift again.

- **A gate for the whole class: `make kwarg-parity-check`.** It reads the
    `_kwlist` out of every `native/src/*/*_ext_*.c` and compares it to the
    `def` its `.pyi` publishes — 169 methods, discovered, so a new object is
    covered the moment its fragment exists.

    `jm status --check` structurally cannot see this: the kwlist sits in a
    wrapper body, which jm's own output calls out as *"yours … not counted
    as drift"*. Measured, not assumed — renaming the kwarg back to `n`
    leaves `make drift-check` reporting exactly what it reported before, and
    passing.

    It found two more on arrival, both the reverse pairing (the stub
    *under*-publishes a hand-written `out=`), and carries them as a
    shrink-only ratchet: an entry that stops mismatching also fails, so the
    list cannot rot in either direction. doppler#922 tracks emptying it;
    just-makeit#1074 is the upstream gap underneath both findings.

- **The CI dependency install retries three times, not twice**, inside the
    same step ceiling. `DEPS_DEADLINE x DEPS_TRIES` moves from 600x2 to
    420x3 (~21.5 min against the 25-minute ceiling `deps-budget-check`
    enforces).

    Measured, not guessed. With the runners' azure mirror answering `Ign` and
    the archive.ubuntu.com fallback trickling, the timings say the retry is
    the thing that works and the long deadline is not:

    | job                   | install-deps                                  | outcome |
    | --------------------- | --------------------------------------------- | ------- |
    | Build on ubuntu-24.04 | 831 s = 600 (attempt 1 killed) + 10 + **221** | pass    |
    | Python 3.13           | 119 s                                         | pass    |
    | coverage, Python 3.11 | 1210 s = 600 + 10 + 600, both exhausted       | fail    |

    A healthy attempt is 120-220 s, so the back half of a 600 s deadline is
    spent stalled: it buys nothing a retry would not buy sooner. Three 420 s
    attempts give ~2x headroom over a healthy install and three rolls of the
    mirror lottery instead of two.

    This is a mitigation and is labelled as one — [#885](https://github.com/doppler-dsp/doppler/issues/885)
    carries the fix, which is to stop apt-installing the 112 MB of toolchain
    the runner image already ships.

- **A test that accumulates failures and never reports them is now a lint
    failure.** `DP_CHECK` counts into `dp_test_fails_` and carries on; only
    `DP_TEST_END` turns that counter into a non-zero exit. A file ending with
    its own `printf ("… OK …")` and `return 0` therefore runs every check,
    records every failure, and exits 0 — so each of its `DP_CHECK`s is
    decoration and `ctest` reports the test as passing while it asserts
    nothing that can fail.

    Two files ended that way, and they show the two different costs.
    `test_frame_meter_core.c` shipped **3 `DP_CHECK`s that could not fail**.
    `test_wfm_frame.c` asserted entirely through `DP_REQUIRE`, which *does*
    return 1, so its epilogue was latent rather than broken — and went off the
    moment 12 `DP_CHECK`s were added to it. That is the worse half: the file
    looked healthy, and writing an ordinary assertion into it produced an
    assertion that could not fail.

    Found by sabotage rather than by review. Making `wfm_frame_t`'s
    `preamble_reps = 0` emit one period instead of no preamble changed the
    layout, and every test still passed. It is precisely the shape
    `DP_TEST_END`'s own "ASSERTED NOTHING" guard exists to catch and cannot,
    because a file that never calls it never runs that guard either.

    Both epilogues now call `DP_TEST_END`, and `check_tests_ssot.py` refuses
    any test using the accumulating flavour without it. The rule is absolute
    rather than a ratchet — after the fix the count is zero — and
    registration-free, so a new test scaffolded from an old template is
    covered the moment it lands. Proven by putting the defect back and
    watching `make tests-ssot` name the file.

- **The FER anchor in the receiver battery can fail again.** It could not,
    at any operating point in the standard battery: the sabotage it exists to
    catch — corrupting every other frame's CRC — left `rx_battery --check`
    reporting **OK**. Now it fails at four points, and the reported slack at
    the anchor drops from **1.64x to 1.32x**.

    The cause was not the tolerance and not the interval width. Corrupting
    every other frame adds `0.5 x (1 - FER)`, so the *relative* change shrinks
    as the baseline rises — and `RX_FRAME_CONT` protected 1040 bits, at which
    the battery's SER=1e-3 anchor already fails two thirds of frames on noise
    alone. A gross fault therefore moved the measurement only 0.68 → 0.84, a
    factor of 1.23, under the anchor's floor of about 1.7. **A high baseline
    FER hides faults rather than exposing them.**

    The payload is now 304 bits (320 protected), where the baseline is 0.30
    and the same sabotage reads 0.65 — a factor of 2.18. `RX_FRAME_NONE` and
    `RX_FRAME_GOLD` move with it, keeping the pairings that make them
    comparable (same payload verbatim, same geometry); `test_dp_frame`
    asserts both and its bit-count table moves too. Shortening the frame
    rather than raising Es/N0 keeps **one** Es/N0 across all four battery
    metrics, which is what makes them comparable at all.

    A second point came back for free: `rate_odd` previously **REFUSED** its
    framed half — "no burst aligned, the marker never detected" — and now
    measures 111 frames. `qpsk` and `psk8` still refuse (their frame bits do
    not divide into whole symbols), unchanged.

- **`make gallery` reads the same skip list the examples gate does, so release
    step 2 can pass again.** The target ran every script in `GALLERY_SCRIPTS`
    and exited 1 on the first failure. One of them,
    `mpsk_receiver_performance_demo.py`, has been in
    `src/doppler/examples/.examples-skip` since its Monte Carlo was found to be
    draw-dependent — the smoke gate deliberately does not run it — while
    `gallery` ran it anyway and failed on it.

    So the two disagreed about the same judgement, and `make gallery` could not
    pass on any machine. That is step 2 of
    [the release checklist](../dev/release.md), which is where it was found:
    preparing for a release runs straight into it.

    The list is now read rather than restated. A skipped script prints `SKIP`
    with the file it came from — a quietly absent panel is how the stale assets
    in [#780](https://github.com/doppler-dsp/doppler/issues/780) accumulated —
    and the PNG move tolerates a file a skipped script never produced.

    Proven by sabotage: removing the entry from `.examples-skip` puts the
    target back to red, and restoring it goes green.

- **`make gallery` no longer leaves four untracked files in the repo root.**
    The demos that write a capture leave it where they ran. `burst.blue` was
    cleaned up; the SigMF and BLUE pairs from the `wfm_io` / `wfm_write` demos
    (`probe.ci16`, `probe.ci16.sigmf-meta`, `scene.cf32`,
    `scene.cf32.sigmf-meta`) were not — none of them gitignored, which is how a
    build artifact gets committed by accident.

- **35 committed gallery assets were stale and are regenerated.** #780 counted
    21; it is 35 now, and the figures are deterministic (verified by
    regenerating one twice and comparing hashes), so the diff is real content
    rather than PNG churn. #780's remaining halves — the gallery has no gate,
    and one script is in no target — are untouched.

- **A hung `install-deps` can no longer block a PR for hours.** CI's dependency
    provisioning reaches the network three times — the jbx bootstrap
    `curl … get-jb.sh | bash`, then jbx's own apt/brew — and none of those calls
    carried a timeout. `get-jb.sh` itself makes three curls with neither
    `--retry` nor `--max-time`. So a stalled mirror did not *fail* the step, it
    **hung** it, until GitHub's 360-minute job limit, reporting `pending` the
    whole way where a reader cannot tell it from slow CI. Measured 2026-08-19:
    one commit hung **3h45m** and then **2h42m** at `Install system   dependencies`, blocking an otherwise-green PR, while a sibling branch passed
    the identical step ninety seconds later.

    **A retry alone would have been decoration, and that is the finding.** This
    repo already runs a retry-on-failure pattern
    (`.github/actions/setup-uv/action.yml`), and it keys on a step *exiting*
    non-zero — correct for the 2026-08-07 incident it was built for, where a
    fetch timed out and returned an error. A hung step never exits, so the same
    pattern bolted onto provisioning would never once have fired. The deadline
    is what converts the hang into a failure; the retry is what then recovers
    instead of merely failing faster. They ship together, deadline first.

    New `scripts/with-deadline.sh` carries both, reached through
    `make install-deps-ci` / `make install-docs-deps-ci` so the Makefile stays
    the one place that says *how* a tool runs — a developer keeps the plain
    target, CI reaches for the bounded one, and neither is a second copy of the
    invocation. All seven provisioning call sites also gain a step-level
    `timeout-minutes: 25`, the backstop for a shell with no `timeout(1)` and
    the half that cannot be bypassed. `timeout-minutes` is **not supported on
    composite-action steps**, so unlike `setup-uv` this cannot be folded into a
    single action.

    Sized from measurement, not feel: healthy runs took **16–179s across 9
    samples** (median 18), so the 300s per-attempt deadline is ~1.7x the worst
    observed.

    **A deadline expiry is terminal and is not retried**, which the fix's own
    first live run is what established. Attempt 1 hit the 300s deadline inside
    `apt-get` and attempts 2 and 3 then failed in seconds with `Could not get   lock /var/lib/dpkg/lock-frontend. It is held by process 2429 (apt-get)`.
    The killed apt-get **survived**: jbx runs it under `sudo`, so it is
    root-owned and an unprivileged process-group kill gets `EPERM`. It then
    holds the lock forever, because the reason it was killed is that it hung.
    Retrying past that point is not merely wasteful — it is guaranteed to fail
    and it buries the real cause under two lock errors. Ordinary failures (a
    curl exiting non-zero, a transient resolver error) leave no lock and *are*
    still retried.

    **The stall is a mirror going quiet mid-download.** The log shows
    `azure.archive.ubuntu.com` producing nothing for 229 seconds partway
    through a 114 MB fetch, on the same package (`cmake-data`) across separate
    runs — a broken path, not random noise. `make apt-stall-config` sets
    `Acquire::http::Timeout "30"` / `Acquire::Retries "3"` before provisioning
    (Linux-only, best-effort, a no-op without apt or sudo). **Measured, that
    did not stop it**: apt carried the config and still sat nine minutes on the
    same package, so the deadline is what bounds this, not apt. The setting
    stays because it costs nothing and covers stall modes apt *can* see, but it
    is not the thing that works here.

    **What makes the retry viable is reclaiming the lock.** The holder is
    root-owned, and a CI runner gives us root — so on a deadline expiry the
    script kills it, removes the dpkg/apt lock files, runs `dpkg --configure -a`, and retries. That is gated on `CI` *and* sudo: clearing dpkg locks is
    reasonable for a disposable VM and unreasonable for a laptop, so off a
    runner a deadline expiry stays terminal.

    **What the retry buys is the partial-progress resume, not a fresh mirror.**
    This entry claimed the clean attempt would land "quite possibly against a
    different mirror node", the only thing that helps against a stall localised
    to one path. Measured 2026-08-19 (run 32250340944): Python 3.9 stalled on
    `Get:7 … cmake … [11.2 MB]`, the reclaim ran, and attempt 2 stalled on
    **the same cmake from the same host**. The retry does not move nodes. What
    it does keep is apt's partial downloads across the kill — attempt 2 opened
    with `Need to get 97.4 MB/114 MB` — which is real, and weaker than the
    claim it replaces. The same run also puts apt's own timeout beyond doubt:
    **591 seconds** of silence on a single `Get:` with
    `Acquire::http::Timeout "30"` demonstrably applied.

    **The stall is stochastic per runner, and that is the sizing fact.** In
    that run 16 of 19 jobs passed this step untouched, and re-running the two
    failures passed clean with no deadline message at all. So two tries is not
    obviously the wrong number; it simply cannot rescue a runner that drew a
    bad path, and the honest bound is that a persistent stall now costs ~20
    minutes and a clear message instead of 3h45m and a `pending` spinner.

    The deadline moves to **600s**: the 9 samples it was first sized from were
    all fast ones, so ~3.4x the worst observed is the honest margin, and a true
    hang is still bounded at ten minutes against the 360 it used to get.

    **The retry budget has to fit the step ceiling, and the first version did
    not.** 600s x 3 tries is 30 minutes against the `timeout-minutes: 15`
    those sites first carried, so the
    retry — which the reclaim had just made work, apt restarting cleanly with
    no lock error — was killed mid-download four minutes later. Now 2 tries
    against a 25-minute ceiling (~20.2 min of budget), and `make   deps-budget-check` derives both sides from the real numbers and fails if
    they ever stop fitting. Sabotage-proven: it reddens on the exact 600x3
    against-15 pairing that shipped, and refuses rather than passing silently
    if it can find no `timeout-minutes:` to check against.

    **The deadline bounds provisioning; a hang elsewhere was still
    unbounded.** Measured against a second incident while this branch was
    open: in PR #880 provisioning succeeded and then `make test-python` hung
    **70 minutes** inside `Test with coverage`, reporting `pending`
    throughout — the same symptom, at a step none of the fix's commits touch.
    It had to be cancelled and re-run by hand, which is the manual recovery
    the deadline exists to remove. So **all 25 jobs now carry a job-level
    ceiling**, a different mechanism that does not replace the deadline: a
    ceiling only kills, where the deadline kills *and retries*, so
    provisioning keeps the pairing that recovers. Sized from **180 successful
    jobs** — `coverage` to 90 minutes against a legitimate 54.7-minute run,
    every other job to 45 against a 30.0-minute worst case. `docs.yml` is 45
    rather than the 20 its 3-minute work suggests, and `deps-budget-check` is
    what caught that: at 20 the 1210s provisioning budget no longer fits,
    because a *job* ceiling bounds the provisioning step inside it just as a
    step ceiling does. That is the gate doing exactly what it was built for,
    one commit after it was written, on a value it was not written to police.

    Also measured on that run: **GitHub's macOS runner has neither
    `timeout(1)` nor `gtimeout`**, so the script carries a POSIX watchdog
    fallback with the same contract rather than leaving a whole platform on the
    workflow ceiling alone.

    Every path verified by sabotage: the deadline fires (`rc=124`, one attempt,
    no retry), a plain failure propagates its own code and still retries, a run
    that recovers on attempt 2 exits 0, and the missing-`timeout(1)` case
    announces itself rather than degrading silently.

- **An installed header can no longer declare a C API the library does not
    define.** `native/inc/` is installed wholesale, so a header there is a
    **published C API** whether or not anything implements it — a downstream
    can read it, include it, and fail at link time, which is the worst
    possible first experience of a library.

    Two headers had done exactly that (`telemetry/tlm_recorder.h`, seven
    functions superseded by `dp_tlm_capture` and never built; an empty
    `stream/stream_core.h` scaffold) and were deleted earlier.
    `make installed-headers-check` is the durable half: every non-`static`,
    non-`inline` function declared at file scope in an installed header must
    resolve in `libdoppler.a` or the optional `libdoppler_stream.a`.

    **It found a third on arrival.** `ber_meter/ber_meter_core.h` still
    declared `theory_ser`, `theory_ber`, `esn0_db_for_ser`,
    `evm_scatter_floor_db`, `settle_syms` and `lock_symbol` — the
    pre-consolidation names, left behind when #539 moved those kernels to
    `ber_core.h` under a `ber_` prefix. None of the six existed under those
    names anywhere in the tree, and nothing in-tree called them, so the only
    thing they could do was compile at a downstream and fail to link. They
    were also six unprefixed global names in a public header.

    Absolute rather than a ratchet, and with no allowlist: the right count is
    zero, and a list would only be somewhere for a fourth to hide. A
    declaration with no definition is either implemented or its header stops
    being installed.

- **`gen_jm_pin --check` now asks whether the pin move was announced by THIS
    change, so a rollback can no longer ship silently.** The check asserted
    that the pinned jm version appears somewhere in `CHANGELOG.md` on a line
    naming the pin. A **rollback** — "0.63.3 regressed us, go back to 0.55.3" —
    moves all three pin sites to a version the file already describes
    *arriving at*, so the scan found it and said nothing. Applied consistently
    is not the same as announced, which is the exact failure the assertion was
    written to close, one level in.

    It now compares the pin against the **merge base** (the same question and
    baseline the assertion ratchet uses: did *this branch* move it). When it
    moved, the new version must appear as a pin **destination** among the
    changelog lines the branch adds. History does not count, because history
    is what a rollback returns to.

    Two narrower scopes were tried first and both let the rollback through, for
    one reason: `[Unreleased]` is 198 KB here — doppler has not released since
    v0.42.0 — and holds every pin bump since 0.52.0, and `changelog.d/` is 109
    unassembled fragments. Each is this release *cycle's* history rather than
    this *branch's* statement.

    An untracked fragment is read as fully added, because writing a fresh
    `changelog.d/` file is the ordinary way to record a bump and `git diff`
    cannot see it — without that the gate would reject the workflow it asks
    for, which is how a gate gets switched off.

- **The same check no longer counts a version it is moving AWAY from.**
    Harvesting every semver on a pin line made 17 of 18 "recorded" versions
    left-hand sides of an `X → Y` entry.

    [#693](https://github.com/doppler-dsp/doppler/issues/693) proposed taking
    the last semver on the line; this file's own history refutes that twice.
    `pin 0.57.0 → 0.59.0, and the create-only headers 0.58.0 ships` ends on a
    version that is neither side of the move, and `pinned to 0.25.0 (from   0.24.0)` puts the destination **first**. The destination is now read from
    what the sentence does — an `X → Y` pair, else a bare `→ Y` / `to Y`, else
    the first semver with an explicit `(from X)` removed — which is right for
    all 22 pin lines in the tree.

    Proven by sabotage in both directions: an unannounced forward bump and an
    unannounced rollback each go red, and each passes once a fragment names
    the move. An unmoved pin stays green.

- **The lock detector's DETECTION side is measured, and its scope is written
    down.** `MPSK_RX_LOCK_THRESH_DEFAULT` is derived as
    `sigma_H0 * eta(Pfa)` — a false-alarm threshold sized against the
    no-signal distribution alone — and the header's claim that the statistic
    "reads ~1.0 at lock" carried no Es/N0 with it. It does at the design
    point: **100 % duty at every named battery point**, each at its own
    SER = 1e-3 anchor. It does not below: 69 % at +1 dB, **24 % at 0 dB**, 0.2
    % at −3 dB — while the statistic stays positive throughout and a
    concatenated link over that same record delivers error-free frames. So
    the default is an **uncoded-link indicator**, and a caller running where
    FEC exists to put you must gate on frame sync or on `node_sync_score`
    instead. [#835](https://github.com/doppler-dsp/doppler/issues/835).

    Reported and not gated, deliberately: a `lock_duty >= 0.9` gate was
    written and removed after sabotage, because `dp_ber_settle` already
    requires the flag to hold 90 % over 200 symbols and both sabotages
    reddened the tally gate first. A gate that cannot fail independently is
    one nobody can trust the day it goes green.

- **A claim-inventory row was read from a test's headline instead of its
    assertions, and understated the coverage.** The `MpskReceiver` report's §1.1
    recorded C6 — the two-way handover — as *"the flip is pinned; the DROP-BACK
    is absent"*. `test_mpsk_receiver_core.c` §4 in fact pins the flip, the
    drop-back **and** the re-declare; the row was written from the section's
    comment rather than from what it asserts.

    What §4 genuinely cannot test is the part the header argues — that the loop
    filter *carries the frequency estimate across* rather than re-acquiring —
    because §4 re-seeds the carrier by hand over the outage, so it would pass
    against a receiver that cleared it. That is what the new §12 covers, and the
    row now says so.

    Recorded because it is the campaign's own trap arriving from the other
    direction: **pinned-only-at-literals**, committed by the auditor rather than
    the author. An inventory is evidence about tests, so it has to be read the
    way the tests are — assertion by assertion.

- **Two claims the inventory listed as unmeasured are now measured — and did not
    hold**, which the report says instead of leaving them open-ended.

    **C21**, the `A^2` timing under-drive with `agc = 0`: §2.9 shows a level
    error reaching `timing_rate`, but the proxy is **not monotone in level**. At
    25 dB Es/N0 and amplitude 0.25 the un-levelled receiver reads *better* (4 ppm
    against 5), so an assertion on it would have been true at one operating point
    and false at another.

    **C16**, that `num_phases = 64` is the *measured saturation point*: swept 4
    to 1024 arms at an off-grid rate, EVM is flat to **0.08 dB**. So this
    geometry saturates below 4 arms and does not locate 64 as the knee at all —
    worse than unmeasured, because the obvious test would have asserted a
    difference that is not there.

    Both need a harsher stimulus than a per-push validator builds, so they belong
    in `make characterize` or `native/validation/`
    ([`docs/dev/contributing/adding-algorithms.md`](docs/dev/contributing/adding-algorithms.md) phase 7).
    Recorded because "measured and refuted" and "not yet measured" are different
    states, and a reader deciding what to do next needs to know which one applies.

- **Every `SER theory` figure in the `MpskReceiver` validation report was the
    bound at the wrong Es/N0.** `ber_theory_ser` takes **linear** Es/N0 — its
    header says so in capitals — and that validator passed dB, alone in the
    tree (`ber_esn0_db_for_ser.c`, `ber_awgn_demo.py` and
    `test_mpsk_receiver_performance.py` all convert).

    The consequence was not an offset but a **fold**: `db → 10·log10(db)` is
    compressive, so an 8/12/16 dB sweep was scored against the bound at
    9.0/10.8/12.0 dB and the sign of the error reversed across the sweep. It
    read as a receiver falling behind the bound at low Es/N0 and catching up
    at high — which is what an implementation loss is supposed to look like,
    and is why it survived review. The tell was 8PSK reading **20 dB better
    than the bound**; nothing beats a matched filter, so a negative
    implementation loss is always a defect in the measurement.

    Corrected, **BPSK's implementation loss is +0.47 to +0.67 dB and QPSK's
    +0.33 to +0.39 dB**, where the old table implied ~1.3 dB at BPSK and a
    10× rate deficit at QPSK. Every bound now goes through one
    `_bound(m, esn0_db)` helper, so there is a single conversion site rather
    than three that agreed by luck. Recorded as the report's F8.

- **`MpskReceiver`'s `resolves` verdict is derived from the experiment's design,
    not from its outcome.** It keyed on the *measured* error count clearing
    `MIN_ERRORS`, which was wrong twice over. Statistically: whether an
    experiment can resolve an effect is fixed by the bound and the record length
    before any data arrives, so reading it off the observed count judges a
    receiver measurable *because* it did worse. Practically: the measured count
    is machine-dependent, so 8PSK at 17 dB gave 20 errors on one toolchain and 6
    on another, straddling the floor — `resolves` read `yes` on one machine and
    `no` on the other, and **the set of asserted limits changed with it**. A cell
    dropped out of the certified envelope depending on which compiler built it.

    Now `theory * scored symbols >= MIN_ERRORS`, which is a closed form over
    `erfc` and bit-identical across those toolchains (measured). The measured
    `errors` column stays in §2.1 — it is what tells a reader whether the
    design's expectation was borne out. Found by the structural comparison in
    [#820](https://github.com/doppler-dsp/doppler/issues/820), which masked every
    number and left exactly this verdict flip visible.

- **The `MpskReceiver` report's "not covered" note no longer reads as if the tree
    lacks framing.** It said *"FER is absent because this object has no framing"*,
    which was true of the object and became misleading once `wfm.Frame`,
    `ccsds_tm_frame.h` and the CCSDS chain landed a layer up — and
    `native/validation/rx_frame_fer.c` already measures FER on a receiver through
    them. So `rx-test.md` goal 4's fourth metric is reachable; it is just not
    reachable from a report scoped to one object, which is a different statement.

    The note now also records a composition gap worth knowing about:
    `mpsk_soft_demap` produces per-bit LLRs and **`MpskReceiver` exposes none**,
    so a caller feeding a soft-decision decoder demaps from `steps()` output
    themselves rather than asking the receiver for it. The receiver's claim
    inventory is unaffected — the LLRs came out of the `mpsk` constellation
    module, not the receiver, so no header claim changed.

- **`MpskReceiver`'s tests seeded the carrier loop either ON the answer or
    past its pull-in cliff, so almost none of them measured acquisition**
    ([#843](https://github.com/doppler-dsp/doppler/issues/843)). Every seeding
    site is now stated in the loop's own units — cycles per **symbol**, the
    same normalization as `bn_carrier` and `bn_timing` — and held at or under
    the acquisition bound of `bn_carrier / m`.

    The `m` is the part that was missing everywhere.
    `_mpsk_rx_harness.freq_offset_inside_bw` returned `frac * bn / sps`,
    contradicting `docs/design/mpsk-refactor.md` §4.4 on two counts: the NDA
    discriminator is an M-th power, so the bound carries a divide by `m`, and
    the bound has no `sps` in it at all. Without the `m` one call read
    identically at every order while asking a 4× harder question at 8PSK than
    at BPSK, putting 8PSK exactly on the measured limit with no margin. The
    same expression was written out by hand in the C BER certification
    (`mpsk_receiver_ber.c`, `mpsk_receiver_real_ber.c`), whose sweep runs all
    three orders.

    Conversion to cycles per **sample** now happens once, at the constructor
    boundary that needs it, which is what `native/tests/dp_rx_mpsk.h` already
    warned about: *"Mixing them is an sps-sized error, and at sps=8 it asked
    the loop for 8x its design envelope."*

    The opposite defect sat in `test_mpsk_receiver.py` and
    `test_mpsk_receiver_core.c` §2, which passed `init_norm_freq` equal to the
    stimulus's own offset — so the loop started on the answer and never left
    its initial state. **A receiver whose carrier discriminator was wired to
    nothing passed all of those.** They now start one bound below truth, and
    the change is sabotage-proven: moving the seed to ten times the bound
    fails six of them, where seeded on truth they passed at any offset
    whatsoever.

    Bounds measured 2026-08-17 rather than asserted (6 seeds per point;
    BPSK/QPSK/8PSK at sps 8, bn 0.01, 20 dB, 4000 symbols): the carrier loop
    acquires reliably out to 4× its bound and collapses by 6×, so the rule
    keeps a 4× margin; the timing loop reaches 1.6× and collapses over
    1.8–2.0×, and `clock_offset_inside_bw` was confirmed correct as written —
    no `m` belongs in it, because the timing discriminator is not an M-th
    power.

    `scripts/check_stimulus_sources.py` gains a fourth signature, so a bare
    cycles-per-sample offset literal in the test, validation or example trees
    fails the gate the way a private pulse, level or EVM already does.

- **`make nats-down` removed the JetStream store while the broker was still
    writing to it**, and failed the cleanup step of jobs whose tests had
    passed:

    ```text
    rm: cannot remove '.../streams/DP_WORK_ep752264292/msgs': Directory not empty
    ```

    `kill` returns when the signal is *queued*, not when the process is gone.
    Measured against a real nats-server: it stayed alive for roughly nine
    hundred further `kill -0` polls after `kill` returned, and a JetStream
    shutdown spends that window flushing stream state to disk. The `rm -rf`
    ran straight into it — emptying a stream's `msgs/` directory and then
    failing its `rmdir` on a file the broker had re-created in between.

    The recipe now stops every writer, **waits for it to be gone** (SIGTERM,
    then SIGKILL after 10s), and only then removes the store; the container
    comes down before the store too, so a future bind mount cannot quietly
    reintroduce this. The removal itself can no longer fail the target — it
    runs under `if: always()`, so a surviving temp directory is worth a
    warning and is not worth turning a red test run into a red cleanup.

    `scripts/start-nats.sh` lost the same race in the other direction, where
    `set -e` made a "Directory not empty" abort the *start*. It now retries
    and then insists on an empty store, saying which command to run.

    Gated by `src/doppler/tests/test_nats_teardown.py`, which drives the real
    recipe against a stand-in that keeps writing through SIGTERM. Deleting
    the wait loop turns it red.

- **The "no private RNG" gate now scans `native/validation/`, where four
    harnesses had one.** `check_tests_ssot.py` enforces a rule its own
    docstring stated absolutely — an inline xorshift, a hand-written
    Box-Muller or either uniform mapping may exist only in `dp_rng_test.h` —
    and printed `no private RNG` in its summary. It scanned `native/tests/`
    only. `check_stimulus_sources.py` had scanned both directories all
    along, so the two SSOT gates disagreed about their own scope and the
    narrower one was the one whose rule was stated as absolute.

    `native/validation/` is where this costs the most, for the same reason
    it is exempt from the gate's *other* rules: a validation harness
    **reports** a number instead of asserting a bound, so there is no
    assertion to have margin and nothing to fail. A generator that is
    quietly wrong becomes a published figure that is quietly wrong — which
    is exactly what the consolidation found in `native/tests`, where a
    half-finished Box-Muller delivered mean +0.056 and variance 1.115 while
    claiming N(0, 1) and no test noticed.

    Scope is now **per rule**: the two randomness rules read both
    directories, while re-defining a `dp_*.h` macro, naming your own
    assertion, and the assertion ratchet stay `native/tests`-only. Those are
    about who owns the *assertions*, and a validation harness does not
    assert.

    `symsync_lock.c`, `ber_despreader.c`, `dll_jitter.c` and `rx_dynamics.c`
    were **migrated rather than excused**, so the rule stays absolute rather
    than becoming a ratchet. All four are bit-exact: three had `dp_xs32` /
    `dp_cgauss` / `dp_bit` spelled out by hand, and `ber_despreader.c`'s
    (13, 7, 17) triple on a `uint64_t` *is* `dp_xs64`. Proven by compiling
    each harness before and after with identical flags and diffing the full
    sweep, not just the `--check` subset.

    Widening the scan found the second half of the same defect.
    `rx_dynamics.c` drew both Gaussian components from **one state inside
    one expression** — indeterminately sequenced (C11 6.5.2.2p10), so it
    drew a different noise stream under `make test` (gcc) than under
    `make coverage` (clang), in a harness that publishes a lock/rate table.
    The rule against that could not see it either: its wrapper fold starts
    from the `dp_*` names, and this file's generator chain was private all
    the way down. **A private generator does not just risk being wrong; it
    hides the other rules from the code that uses it.** Rewritten into named
    locals, in the order this build's gcc actually chose, so the published
    numbers are unchanged — `docs/design/mpsk.md` quotes the strobe tap's
    `+0.860` at the data onset, and a re-ordering moves it to `+0.757`.

    Which order that is had to be MEASURED, not read off. `dp_rng_test.h`
    records gcc taking the imaginary operand first; here, under this
    harness's actual flags, gcc `-O3` and clang `-O0` both take the *real*
    one, so writing the documented order in produced a silently different
    table — one that still passed `--check`, because that subset prints a
    single line. Every one of the four migrations was confirmed by diffing
    the harness's FULL sweep, compiled before and after with its exact
    per-target flags.

    The point of the rewrite is not that the two compilers disagreed here;
    measured, they happened to agree. It is that nothing made them, and the
    published number had no defence if a flag, a version or a target ever
    changed the choice.

- **The gate now recognises a linear-congruential generator, which it had
    never looked for.** The scan knew xorshift and Box-Muller only, so it
    printed `no private RNG` over four live LCG streams — three of them in
    `native/tests`, the directory whose count the docstring called zero.
    Found on the new idiom's first run.

    It uses the same backreference discipline as the xorshift idioms, and it
    is load-bearing for the same reason: the *same* lvalue on both sides is
    what makes it a stream. `x = k * 1103515245u + 12345u`, the one-shot
    index hash in `dp_mf_test.h`, multiplies a different value and carries
    no state — a deterministic bit pattern, not a random source — and stays
    silent, as does `n = n * 2 + 1`. Both were checked as controls.

    Unlike the xorshift copies, migrating these **moves the stream** and
    therefore the numbers measured against it, so the four are held in
    `scripts/.private-rng-ratchet` with a reason each. The list may only
    shrink, and an entry matching nothing fails the gate rather than being
    quietly ignored.

    The summary line now names the directories it scanned and the number it
    is holding, so it cannot read as an unqualified zero again. Every rule
    above was proven by sabotage — putting each shape back and watching
    `make tests-ssot` name the file.

- **The release-notes renderer could exit 141 and publish nothing.** The
    `### Highlights` extraction exits at the next `### ` heading; fed from a
    pipe, the writer then takes SIGPIPE, and `set -o pipefail` turns that into
    a failed release — after emitting a **zero-byte** body, with every visible
    step looking fine. Found by running the renderer over a 132 KB section
    rather than by reading it. It reads a here-string now, and
    `test_extraction_does_not_die_of_sigpipe` asserts the exit code
    specifically, because the symptom was otherwise indistinguishable from an
    ordinary failure.

- **Tests found the repo root by counting directories, so 89 of them failed
    under the coverage harness.** `Path(__file__).parents[N]` is correct
    exactly once — from the source checkout. `make coverage` copies the
    package to `build-cov/pkg/doppler`, two levels deeper, so every
    fixed-depth root landed inside `build-cov/` and every test reading
    `docs/`, `scripts/` or `native/` failed there.

    It went unnoticed because `COVERAGE_CMD`'s pytest carries a leading `-`,
    so make ignores its exit code: **27 failed and 62 errors against 2631
    passed**, permanently tolerated in the one job that produces the coverage
    number. `wfm/tests/test_schema.py` alone contributed 47, because
    `parents[4]` missed `docs/schema/wfmgen.schema.json` and took out a
    module-scope fixture.

    `doppler.tests._repo.repo_root()` walks up for `pyproject.toml` **and**
    `native/` together — either alone could match an unrelated parent project
    — which is depth-independent and therefore right from both trees. 16
    sites converted.

    The walk is deliberately better than skipping those tests: `build-cov/`
    lives *inside* the repo, so it finds the real root and they now **run**
    under coverage and contribute to the number, rather than being excluded
    as "needs a checkout".

    `test_benchmark_fixtures.py` keeps its `parents[1]`, which means
    `src/doppler` rather than the repo root and was never wrong.

    Verified against both trees: the source-tree suite is unchanged at 2719
    passed / 7 skipped, and the fixed-depth idiom is gone from every site
    that meant "repo root".

- **A retired identifier stays retired, and there is now a gate for it.**
    The `fec` → `ccsds_tm` rename went green with **eleven** occurrences of
    the retired prefix still in the tree: two macros in a public header (the
    ASM pattern and the inner code's constraint length, now `CCSDS_TM_ASM` and
    `CCSDS_TM_CONV_K`), the NAME five C tests print at the end of a run, an
    encoder type in a design page that had not existed since the codec was
    generalised to `conv_enc_t`, and three header references in a changelog
    entry and a validation report. A compiler cannot notice any of them —
    every one still compiled, because every one still existed.

    `scripts/check_retired_names.py` (wired as `make lint-retired-names` and a
    pre-commit hook) scans the hand-written trees for the patterns in
    `scripts/.retired-names`, a table whose rows are added by the commit that
    retires a name and which only ever hold at zero occurrences. Proven three
    ways by sabotage: the macro back in the header, the old name back in a
    test's printed label, and a stale symbol back in a design page.

    The patterns match the IDENTIFIER forms only, so FEC keeps its meaning as
    the general subject — `docs/design/fec-receive.md` spans a general Viterbi
    and a general Reed-Solomon, and that page's name is deliberate. One
    consequence worth stating, because it is the rule working rather than a
    gap: an entry like this one cannot spell the names it retires, so it names
    what replaced them instead.

- **An incomplete receiver adapter now fails by name instead of segfaulting.**
    Every `dp_rx_iface_t` entry is mandatory — the instrument calls all twelve
    unconditionally — so a positional initializer that stops one short is a
    NULL call at the first operating point, before a single line is printed.
    `dp_rx_run()` now checks the adapter first and fails the record with
    `adapter entry '<name>' is NULL`, which `--check` turns red.

    It is a *failure*, not a refusal: a refusal is the instrument declining a
    number it cannot defend, and this is the instrument unable to run at all.

    Found the way the class is designed to be found — `ContinuousMpskReceiver`'s
    adapter was written against an eleven-entry interface and `zeta` was
    appended as the twelfth on another branch, so **both branches were green**
    and only their rebase was not. The guard is proven by sabotage: dropping
    `rx_mpsk_zeta` from `RX_CONT` reddens `validate_rx_battery --check` with the
    entry named, where before it exited 139 with no output.

- **The receiver instrument declared six frame-statistics fields and measured
    none of them.** `native/tests/dp_rx_test.h` carried `frames`,
    `sync_detected`, `crc_passed`, `fer`, `sync_miss` and `prot_bits`,
    documented FER in its own composition table, included
    `frame_meter_core.h` — and never called `frame_meter_create`. The
    instrument built to report goal 4's four metrics *together* reported
    three, and the missing one is the only truth-free metric that sees a
    false lock.

    The machinery already existed in `native/validation/rx_frame_fer.c`, so
    this **moves** it rather than writing a second one: `dp_rx_score_frames()`
    with the per-frame sync CONFIRMATION at ±`DP_RX_SYNC_SPAN` (a tracking
    window, not a re-acquisition), the truth-free CRC check, and the
    one-sided FER anchor asserted on the interval's **lower** limit.
    `rx_frame_fer.c` is now a caller and its copy is gone; it reproduces its
    committed table **bit for bit**, which is how a move is told apart from a
    rewrite. `framed == 0` prints **n/a**, never `0.0`.

    Measured consequence on the battery: the anchor's SER rises from
    1.087e-03 to 1.090e-03 once the periodic marker's sync words stop being
    scored as data (#793) — the only direction it could move, because symbols
    that could not be wrong stopped being counted as symbols that merely
    happened not to be.

- **`test_dp_test` did not link libm, and only the coverage build noticed.**
    The self-test calls `cabs` once, to show that `dp_cnear` accepts a diagonal
    error a magnitude bound would reject. gcc at `-O2` folds that call away and
    needs no libm; the coverage build is clang at `-O0` and emits it, so the
    missing `target_link_libraries(... m)` was invisible to `make test` and an
    `undefined reference` in `make coverage`. Its sibling `test_dp_rng` already
    linked `m`.

- **`validate.py --check` says what differs, not just that something does.**
    It printed `STALE — <file>` and stopped, which answers none of the first
    questions: which lines, and is this a real edit or a number's last digit? It
    now prints the unified diff (first 40 changed lines, no context, so every
    line shown is a changed one) and states explicitly that on a machine other
    than the one that generated the report, `make validate` will move the
    problem rather than fix it.

    Written because that distinction turned out to matter. Wiring
    `make validate-check` into CI for
    [#816](https://github.com/doppler-dsp/doppler/issues/816) — it sat in
    `GATES_DEPS` with no CI home, so report staleness was checked on developer
    machines and nowhere else — reported **four of eleven reports STALE on the
    runner** while all eleven were up to date locally on the same architecture,
    Python and numpy. Two of the four render their limits exactly as they always
    have, so their drift predates that change: **the committed reports have
    never been reproducible on a machine other than the one that generated
    them.**

    The CI step is therefore *not* wired, and `ci.yml` carries the evidence as a
    comment where the next reader will look. A permanently red job is worse than
    the hole it closes. Diagnosis and options are
    [#820](https://github.com/doppler-dsp/doppler/issues/820); #816 stays open
    with a named blocker instead of an open question.

- **`--check` gates a report's STRUCTURE, and the numbers are gated where they
    have units.** Byte-comparing `results.md` demanded reproducibility the
    numbers do not carry: measured across two toolchains (gcc 15.2/glibc 2.43
    against 13.3/2.39 on one CPU), a BPSK cell's error count moved 204 → 198 and
    SER, implementation loss and EVM moved with it — all inside the
    measurement's own ~7% standard error. Four of eleven reports were stale on
    the other machine, two of them for reasons predating any recent change
    ([#820](https://github.com/doppler-dsp/doppler/issues/820)).

    A tolerance was the obvious fix and does not work: absorbing the observed
    differences needs **>96%** relative, because relative deviation grows without
    bound as a quantity approaches zero and these reports deliberately measure
    quantities that converge to zero. At the point of comparison the artifact is
    markdown, so a `0.3` in a table has neither units nor provenance to key a
    per-quantity tolerance on.

    So `--check` masks numeric literals and byte-compares the rest — sections and
    their order, prose, each limit's claim wording, every verdict and finding
    tag, table shape. Section headings, `§N.M` references and `#N`/`gh-N`
    citations are **not** masked: they are structure written with digits, and an
    earlier version that masked them let a renumbered section and a re-pointed
    citation through. Numbers are gated by each object's
    `test_validation_limits.py`, which asserts them through the same `build()`
    with thresholds the author chose per quantity. Two gates, two questions.

    Verified against the toolchain that broke it: all eleven reports pass in a
    gcc 13.3 / glibc 2.39 container. **`make validate-check` is therefore wired
    into CI**, closing the half of #816 that was blocked on this.

- **`make validate-check` no longer discards the diagnosis it asks for.** It ran
    `validate.py --check > /dev/null`, so the one caller anyone actually uses
    threw away the output and printed a filename. `--check` now prints a unified
    diff when a report is stale, and the target captures and replays it (from
    `STALE:` onward, so the per-limit PASS lines stay out of the way).

    Measured consequence of not having it: wiring the target into CI for
    [#816](https://github.com/doppler-dsp/doppler/issues/816) reported four of
    eleven reports stale on the runner and the log said only *which files* — the
    half that cannot distinguish an edited validator, where `make validate` is
    the fix, from a machine difference, where re-running fixes nothing. Those
    have opposite responses, so a filename alone is the least useful thing to
    print.

    With the diff in hand the cause took one container run: the reports differ
    because the measured **error counts** differ (204 against 198 on one BPSK
    cell), which moves SER, implementation loss and EVM with them — all well
    inside the measurement's own ~7% standard error. So the reports print three
    and four significant figures where they carry about one, and byte-comparison
    is the wrong contract for the numeric half. Diagnosis, evidence and options:
    [#820](https://github.com/doppler-dsp/doppler/issues/820).

- **The validation reports' coherence checks ran in no CI workflow, on any
    report.** `Report._self_check` refuses a render that contradicts itself — a
    `§N.M` pointing at a section the report does not have, a gap in section 2's
    numbering, a table cell truncated mid-reference, and now limits counted but
    never rendered. It runs from `render()`, `render()` runs from `emit()`, and
    a `write=False` build skips it — so the only paths that reached it were
    `make validate` and `make validate-check`, **and neither is in any CI
    workflow** (`validate-check` sits in `GATES_DEPS`, and no job runs
    `make gates`; grep the workflows for "validate" and the one hit is a
    release-time wheel check).

    So the checks were tested in CI — `test_validation_report.py` drives them
    over seeded reports — and never *applied* in CI to the eleven reports they
    exist for. That is the campaign's founding bug one layer out: not a claim
    nobody executes, but a checker nobody points at the artifact.

    Each module's `test_validation_limits.py` now renders the report it already
    built (`assert_renders`), so all four coherence families are enforced on
    every real report inside `make test-python`. It is free — the report is
    already in memory — and registration-free per object, since a new object
    joins its module's existing `OBJECTS` map. Proven by sabotage: a dangling
    `§9.9` in `ema`'s validator takes exactly
    `test_the_report_renders_coherently[ema]` red and nothing else.

    Staleness is the half this cannot fix — whether the *committed* bytes match
    the generator is still `make validate-check`'s question, and still not in
    CI. Filed as [#816](https://github.com/doppler-dsp/doppler/issues/816),
    which also proposes making `gates-check` bidirectional so the next gate
    added to `GATES_DEPS` without a CI home fails loudly.

- **Section 4 of seven validation reports named none of the limits it
    counted.** `agc`, `ema`, `resamp`, `lockdet`, `mpsk`, `loop_filter` and
    `mpsk_receiver` each rendered the heading, the sentence "Claims a caller
    may rely on", and then nothing — while section 5 beside it closed with
    `N/N limits hold`. **262 certified claims that only four of eleven
    reports actually stated.**

    Neither gate could see it, for two different reasons.
    `test_validation_limits.py` asserts every limit and never reads the
    report. `make validate-check` re-renders and compares bytes, so a
    generator emitting an empty section agrees with itself perfectly — a
    staleness gate proves the artifact matches the generator, which says
    nothing about whether what is generated is complete. The passing tally
    sat beside the empty section, and the tally is what a reader trusts.

    The table is now emitted by `Report.summary()` — the one hook every
    validator calls, and the only one that runs after the last `limit()` —
    so it cannot be forgotten by a new object, and the four hand-rolled
    copies are gone rather than left to drift. `Report._self_check` refuses
    a render whose section 4 carries fewer rows than the run recorded,
    counted against the **rendered** text because that is the artifact a
    reader gets. Proven by sabotage, with three seeded cases in
    `test_validation_report.py` including the vacuity guard from the other
    side: a report asserting no limits needs no table.

- **A JSON scene with no `fs` rendered at 1 MHz, not at 1.0 — so a normalised
    frequency came out at DC.** `wfmgen --help` documents `--fs` as
    *"Sample rate (default 1.0; freq treated as normalised)"*, and the JSON
    reader defaulted the same field to **1e6**. A scene written to the
    documented contract —

    ```json
    {"version": 1, "segments": [{"type": "tone", "freq": 0.08}]}
    ```

    — was therefore read as 0.08 Hz against an unstated 1 MHz rate, which is
    a tone at DC. Nothing errored: the flag parser and the JSON reader are
    two faces of one generator, and they disagreed about a default.

    The reader now defaults `fs` to **1.0**, and the schema says so: `fs` is
    no longer in `required` for either segment form and carries
    `"default": 1.0` with the normalised-frequency contract spelled out. All
    three faces — flag, schema, reader — now agree.

    Found by `rate_converter_demo` failing its own frequency check with the
    tone **1245 bins** off, which is a self-validating example earning its
    keep; a silent DC tone has no other symptom. Gated by
    `test_json_fs_defaults_to_one_so_freq_is_normalised`, which asserts on
    the RENDERED waveform rather than the parsed struct and was proven by
    sabotage: restoring the 1e6 default gives `tone at 0.0, expected 0.08`.

    **Behaviour change for a scene that omitted `fs` and meant Hz.** Such a
    document was never schema-valid — `fs` was `required` — and every record
    `--record` writes states `fs` explicitly, so a round-tripped capture is
    unaffected. A hand-written scene relying on the old 1 MHz now needs
    `"fs": 1e6`, which it should have carried all along.

- **A periodic marker's sync words were scored as DATA.** `dp_ber_measure()`
    opened its scored window at the settled point and bounded it past the
    marker's end only for a *blind* marker. For a **periodic** one — a frame's
    sync word, recurring for the whole record — `ber_meter`'s exclusion reads
    an index before `t0` as "not a marker" (`ber_meter_core.c:122`), so every
    sync word *preceding* `t0` was scored as payload: known symbols that had
    no chance of being wrong, quietly flattering the denominator. The window
    now opens at the marker's **first occurrence** for the periodic shape and
    past its **end** for the blind one, which is what `rx_frame_fer.c` had
    already been doing by hand.

    The same argument holds at the **other** end and is now enforced there
    too: `in_marker` bounds exclusion at both ends
    (`off / period >= occurrences`), so scoring past the last excluded
    occurrence reintroduces the identical defect at the tail. Measured, it
    does not currently bite — exclusion already reaches 44 symbols past the
    scored top at period 285 and 824 at period 1679 — but that margin is a
    function of the detected **lag**, not a chosen constant: on the 285
    geometry a lag beyond ~245 turns it positive, and `DP_BER_LAG_SPAN` is
    200\. Bounded rather than left to a coincidence that holds at the only two
    geometries anyone has looked at. Today it changes no number.

- **`dp_ber_report_t` threw away the alignment it had just computed.** The
    `lag` and `phase` that fixed the record are now returned. They are part of
    *defending* the rate rather than a by-product of computing it: anything
    else scored against the same record — per-frame outcomes, a telemetry
    series — has to be placed in the same coordinates, and re-running the
    detection to find out where it sat is a second copy of the decision, free
    to disagree with the first.

- **`native/validation/` was running its own random number generators.**
    `native/tests/dp_rng_test.h` is the declared SSOT and
    `scripts/check_tests_ssot.py` enforces it *absolutely* — in
    `native/tests/` only. The other directory is the one whose numbers get
    **published**, and five harnesses there carried private xorshift32 and
    Box-Muller copies. They now use the SSOT.

    **Bit-exact, proven per file rather than argued**: every harness's full
    sweep was captured before and after, and all 19 outputs are byte-identical
    (the only line that moves anywhere in the tree is `ber_despreader`'s
    wall-clock `elapsed`, in a file this does not touch).

    Getting there required the draw **order** to be measured, and that is the
    part worth recording. Three of these files drew both Box-Muller components
    inside one expression, which is **indeterminately sequenced** (C11
    6.5.2.2p10) — so the stream each had was the compiler's choice, not the
    author's. `dp_rng_test.h` states that gcc takes the imaginary operand
    first; that is true of the bare `a + b*I` shape it was measured on and
    **not** of these. `carrier_nda_pullin`'s `cexp(...) + s*g + s*g*I` needed
    **real** first to reproduce its stream, and imaginary-first moved every
    number in the file. The order is a property of the **expression**, not a
    rule — which is an argument for named locals, not against them.
    `rx_nda_gauss`'s own two uniforms were in one expression too; sequencing
    them in `dp_gauss`'s order left the file byte-identical, so gcc had been
    drawing the logarithm's uniform first there.

    Migrated: `carrier_mpsk_jitter`, `carrier_nda_step_response` (xorshift
    only), `carrier_nda_pullin`, `rx_nda_tap`, and `mpsk_ber_common.h` with
    both BER twins. The gate is **not** widened to the directory yet: the
    sweep found **four more** files carrying private generators
    (`dll_jitter`, `symsync_lock`, `ber_despreader`, `ratesync_scurve`) that
    #802 did not list, so the ratchet would fail on unmigrated files. The
    directory having grown again is exactly what gh-687 predicted it would.

- **`mpsk_receiver_core.h` said the old defaults for the five parameters that
    now derive.** gh-644 made `0` request a derived value for `m_out`, `zeta`,
    `lock_thresh`, `num_phases` and `bn_agc_ratio`, and updated
    `objects/mpsk_receiver.toml` so the Python face said so — while the C
    header, which is the SSOT, went on documenting `(default 8)`,
    `(default 0.707)`, `(default 0.5)` and `(default 1024)`, and mentioned the
    derivation nowhere. The one face that is the source of truth was the one
    face that did not know about the feature.

    Each `@param` now states that `0` derives, what it derives to, and the
    getter that reads it back; a `@note` on `create()` carries the rule once.
    Documentation only — no behaviour change.

    Same class as the `burst_despreader_core.h` drift already recorded in
    `CLAUDE.md`, where hand-written `(default: X)` annotations went 5x out of
    step with the manifest, and which just-makeit#442 is the open ask to lint
    for. It recurred anyway, in the change that introduced the feature, which
    argues for that lint rather than against it.

- **One bits→symbol map, and it is the library's.** `wfm_synth`'s bit pattern
    had **four inlined copies** of the mapping — two in `wfm_synth_core.h`
    (`wfm_synth_next_symbol`, `wfm_synth_step`) and two in
    `wfm_synth_steps()`. The first of those carries a comment saying the
    kernel is shared "so the single-sample and block paths cannot diverge —
    they call the SAME function rather than each inlining the arithmetic", and
    the arithmetic was inlined four times anyway.

    All four now call `wfm_synth_bit_symbol()`, which hands a Gray label to
    `mpsk_constellation()` — the library's canonical map, and the one
    `dp_ber_score()` inverts to score bit errors.

    That mattered, latently: the QPSK copies put `b0` on the I sign and `b1`
    on the Q sign — the same *constellation*, but two of the four labels
    swapped against `mpsk_constellation()`. Nothing in the tree scored a QPSK
    bit pattern against truth, so it never produced a wrong number. A framed
    QPSK stream measured through the canonical scorer would have read about
    **half its symbols wrong on a working receiver**.

    `modulation` already meant BITS PER SYMBOL (1 = BPSK, 2 = QPSK), so 3 =
    8PSK extends the numbering rather than reinterpreting it.

    The gate is a round trip through the canonical demapper — a property no
    agreement between two copies can establish, since the old copies agreed
    with each other perfectly. The bit pattern counts up so every one of the M
    labels is exercised; an alternating pattern emits one or two labels for
    ever, and a label the test never produces is one the mapping can get wrong
    undetected. Proven by sabotage: LSB-first packing, the original I/Q-sign
    QPSK mapping restored, and Gray skipped each take it red.

    There was a **fifth** copy, in Python:
    `test_rrc_bits_matches_matched_filter` hand-built its QPSK reference from
    the same I/Q-sign formula, so the matched-filter check passed by comparing
    the generator against a restatement of the generator. Its reference now
    comes from `doppler.mpsk.mpsk_map` — the binding over the one map — which
    is why that test had to move with the fix rather than the fix move to it.

    That test is now parametrized over `sps`, because `sps` is what selects the
    shaping implementation: a power of two shapes with the polyphase `resamp`
    bank, anything else falls back to a dense FIR, and those are two separate
    block loops in `wfm_synth_steps()`. Every existing test used `sps` 4 or 8,
    so the **dense-FIR bits loop had no test on any path** — one of the four
    copies could have been fixed and the other not, with nothing to say so.
    `sps = 3` covers it; sabotaging that loop alone takes `[3]` red and leaves
    `[4]` green, which is how the split was checked rather than assumed.

- **A capped CIC silently cost the cascade its rate** — `RateConverter` at a
    power-of-two decimation past the CIC cap decimated by `R` and claimed `D`.
    Measured: `RateConverter_create(1/8192)` delivered `1/4096`, twice the rate
    asked for, and `1/16384` delivered four times it — with no error, no NaN,
    and an output that looks entirely plausible.

    The planner already hands whatever a capped CIC leaves to a `Resampler`
    stage. That residual was gated on the matched-terminal flag alone, so the
    plain constructor never got it. It is now emitted whenever the integer
    stages did not complete the decimation, which is the condition that
    actually decides whether it is needed.

- **The CIC decimation cap is `CIC_R_MAX` (2048), one halving below where the
    accumulator fills.** The 64-bit accumulator holds `65535 · R^4`; at
    `R = 4096` that is `2^64 - 2^48`, which fits and fills it to within one
    part in 65536. "Fits exactly" is not headroom — it is the value at which
    any further term overflows, and the CIC's exactness argument (every
    intermediate overflow cancels in the combs) holds only while the true
    result fits in 64 bits. 2048 leaves **16×**.

    The cap costs no rate, because of the fix above; past it the residual goes
    to the resampler, exactly as the non-power-of-two path already did. Both
    layers enforce it — `cic_create()` refuses beyond `CIC_R_MAX` and the
    planner never asks — because a cap the planner honours and the constructor
    does not is one bad call site away from the accumulator it protects.

    Lowering the cap is what exposed the rate defect: it moved the first
    affected geometry from `D = 8192` down to `D = 4096`, where a receiver can
    actually reach it. Both are pinned by
    `test_capped_cic_still_delivers_the_requested_rate`, proven by sabotage.

- **The clang-tidy pre-commit hook ran nowhere, and is removed** (gh-737).
    Fixing its missing pin left a second defect untouched: it sat at
    `stages: [pre-push]`, `make setup` runs a plain `pre-commit install` —
    which installs only the pre-commit hook type unless the config declares
    `default_install_hook_types`, and this one does not — and CI's `make lint`
    runs pre-commit at the default stage. So it executed on no machine and in
    no pipeline, reporting zero findings because it never looked: a dead gate
    that happened to be green, which nothing downstream could tell from
    success. Deleting it costs no coverage, because there was none; what it
    buys is that the tree stops advertising a gate it does not have.
    `make lint-clang-tidy` is unchanged and is still how to run it by hand.
    Restoring the hook needs an execution home — a CI job, or
    `default_install_hook_types` — **and** the gh-720 backlog cleared enough
    for it to pass; clearing the findings alone would not have revived it,
    since the backlog is why it could not be switched on and not why it did
    not run.

- **Every NDA tap now delivers the `bn_carrier` it was given.** `freq_scale`
    converted the carrier loop filter's output assuming it is radians per
    SYMBOL, which is true only for `strobe`. A tap updating `upd` times per
    symbol produces radians per UPDATE, so the LO was under-driven by `upd`
    and the loop ran narrower than the caller asked for.

    **A frequency STEP could never have shown this** — a type-2 loop nulls a
    step to zero steady-state error regardless of gain, which is why every
    acquisition test passed on both sides of the bug. A frequency RAMP holds a
    constant phase lag with a closed form, and that is where it was visible:

    ```
      theta_ss = 2*pi*r / wn^2,  wn = 8*zeta*bn/(4*zeta^2+1) = 1.8857*bn
    ```

    Measured at sps=200, bn=0.005/sym, the lag was that form times exactly
    `upd` — 1.00 for strobe, 2.00 for mf_out, 1.5625 for mf_in, at every ramp
    rate. With the fix all three match the form to under 1%, so the maximum
    trackable Doppler rate is now the same at every tap instead of `1/upd` of
    it. Closes gh-765.

- **The `mf_in` carrier loop was sized against a placeholder update rate.**
    `config_carrier()` runs inside `mpsk_rx_loops_init()`, which is before
    `mpsk_receiver_create()` can read the cascade's real `bank_sps`, so the tap
    got loop gains designed for `lo_sps` updates per symbol while actually
    updating `bank_sps` times — `ki` too small by `(lo_sps/bank_sps)^2`, which
    is 1.7e7 at Fs/Rs = 10000. The integrator never moved: the loop reported a
    flawless 0 Hz error at 0 Hz offset and acquired **nothing** at any other
    offset, at any rate ratio above sps=8. The filter is now re-sized once the
    real rate is known, and `rx_nda_tap.c` gates it at three rate ratios.

- **A `WFM_SEQ_PN` frame field with `poly = 0` was emitting a constant.**
    `wfm_frame.c` passed `poly` straight to `pn_create()`, which takes the tap
    mask verbatim — so 0, the natural "default" and the value `wfm_synth`'s
    `--pn-poly` already resolves, meant a register with NO FEEDBACK: it shifts
    the seed out and then emits zeros for ever. Measured, a 127-bit PN field at
    `reg_bits = 7` carried **2 ones**. Every generated PN field was a constant
    that still looked like a field, which is why nothing noticed.

    `test_wfm_frame.c` could not catch it because its check was a CONSISTENCY
    test: it compared `wfm_frame_bits()` against `pn_generate()` with
    `poly = 0` on both sides and they agreed perfectly — on two all-zero
    sequences. The new gate is a property no agreement between two halves can
    establish: one period of a length-n MLS carries exactly `2^(n-1)` ones, so
    a balance check over `2^n - 1` bits says the descriptor resolved a real
    polynomial. It reads 1 against 256 when it did not, and it fires even with
    the old mutually-consistent comparison restored.

    The fix applies the resolution the project already had
    (`poly ? poly : pn_mls_poly (reg_bits)`). The polynomial table moved from
    `wfm_synth_core.h` to **`pn_core.h`**, where the convention belongs — it is
    `pn_create()`'s tap mask, not the synth's — and `wfm_synth_mls_poly()`
    stays as a forwarder, so no call site changed and no second table exists.

- **`docs/c-api` was stale for `wfm_frame` and `frame_meter`.** Neither of the
    two commits that added them ran `gen-c-api-check`, so 26 mkdoxy files
    (including `wfm__frame_8h.md` and `frame__meter__core_8h.md`, which had
    never been generated at all) would have failed that CI gate. Regenerated.

- **A merged fix no longer leaves its issue open.** GitHub closes an issue only
    when a closing keyword reaches the default branch; doppler rebase-merges,
    so a commit message carries it — and nothing asked any branch to use one.

    The cost was three issues in one week, all fixed and all still open:
    `c0e0e615` gated the generated C API tree, which **is** #714, and left it
    open for a day; PR #717's F1/F2/F3 are #663, #664 and #665, found only
    because a triage pass read the PR body against the backlog. An open count
    that includes finished work is a backlog nobody can plan from, and the
    triage that produced the ranked board had to verify six issues against the
    tree one at a time to learn which were real.

    `make issue-link-check` asks a branch that changes code to declare either
    `Closes #N` or `No-issue:`. **The bar is a statement, not a link** — most
    branches close nothing, and a gate demanding an issue number from a
    re-vendor would argue with its author, which is the failure mode
    `changelog-check`'s own comment warns about. Silence is the one rejected
    answer, because it cannot be told apart from a closure nobody wrote down.

    A bare mention is not a link and is rejected with silence: `See #714 for   context` and `#714` alone both leave the issue open on merge. That
    discrimination is the gate, so it is what the mutation test targets —
    relaxing the pattern to accept any `#N` fails exactly those two cases and
    leaves the other eleven green.

    It shares `CHANGELOG_CODE_PATHS` with `changelog-check` rather than
    defining "code" a second time, takes the same base so both per-branch
    gates measure against the same commit, is inert on `main` by construction,
    and fails closed with no merge base. Logic lives in
    `scripts/issue-link-check.sh` so its 13 tests can drive it over seeded
    messages instead of fabricating a scratch repository.

    Closes #746.

- **A NaN lock metric held the lock forever; an unknown lock is not a lock.**
    `lockdet_step`'s drop test read `x < down_thresh`, and every comparison
    against NaN is false — so while locked a non-finite look counted as a
    *hit*, reset the drop run on every look, and left the flag lit
    indefinitely on a dead statistic. Measured through the binding before the
    fix: three NaN looks against `n_down = 2`, still reporting locked. Every
    receiver that publishes a lock lamp reads this decision.

    The library had already settled the question, in the component that
    exists to answer it. `util_core.h`'s `saturate()` documents *"a lock
    statistic wants NaN at the **floor** — an unknown lock is not a lock"* as
    the reason its `nan_to` is a parameter — and **no lock detector had ever
    called it**, so that paragraph described a caller who did not exist. AGC
    leverages the primitive in five places; `lockdet` now leverages the same
    one rather than encoding the policy a second way:

    ```c
    x = saturate (x, -INFINITY, INFINITY, -INFINITY);
    ```

    The bounds are infinite because the NaN substitution is the only job:
    every finite look, and both infinities, pass through untouched. `+inf`
    remains an ordinary hit and `-inf` an ordinary miss — only NaN is
    unordered — and the exclusive edge at `x == down_thresh` is unchanged.

    Doing the substitution once, up front, is also what keeps both
    comparisons plain. The first fix carried the policy in the *spelling* of
    a predicate (`!(x >= t)` rather than `x < t`, identical for every finite
    `x` and opposite for NaN), which is exactly the subtlety that let the
    drop side be written the wrong way to begin with. A rule that survives
    only in how an operator is spelled is a rule waiting to be re-broken.

    Three coverage gaps closed alongside it, none of them defects:
    `lockdet_steps` carrying the decision **and** the in-flight verify run
    *across calls* — the header's "frames of any size with no seam", where
    only a single block had been tested and one block cannot see a seam;
    `create()` clamping the verify counts (only `init()`'s clamp was pinned,
    and they are separate entry points); and `set_state` into a
    differently-tuned detector, which carries the source's configuration and
    not merely its decision.

- **`LoopFilter(t=0)` built a silently dead loop and `LoopFilter(t=inf)` one
    whose every output was NaN forever.**
    ([gh-740](https://github.com/doppler-dsp/doppler/issues/740)) The
    constructor accepted a caller's arbitrary doubles and validated none of
    them, so `t = 0` produced `kp = ki = 0` — a loop indistinguishable from
    the legitimate frozen `bn = 0` — and `t = inf`, or a NaN in any argument,
    produced NaN gains that poison every subsequent update permanently. Both
    were one line away in Python. `loop_filter_create()` now rejects
    `bn < 0`, `zeta <= 0`, `t <= 0` and any non-finite argument, and the
    binding raises **`ValueError`** with the component's own message instead
    of a blanket `MemoryError`.

    Enforcement is at `create()` and **deliberately nowhere else**.
    `loop_filter_init()` is the by-value path taken by the seven objects that
    embed a filter, all of which validate upstream — the only
    runtime-computed `t` in the tree is `mpsk_receiver`'s `1.0/upd`, safe
    because `m_out >= 2` is checked in its own constructor — so guarding it
    would be error handling for an impossible scenario. The asymmetry is
    pinned by `test_loop_filter_core.c` §1 and §10 rather than left to read
    as an oversight.

    Validating at the boundary also makes the arithmetic **total**: with
    `bn >= 0` and `zeta > 0` the gain denominator is at least 4, which closes
    the one genuinely pathological corner — for `zeta >= 1` a sufficiently
    negative `bn` drove it exactly through zero and both gains to infinity.

- **`check_tests_ssot.py` scanned the working tree rather than the
    repository, so an ignored file could fail `make lint`.** `jm apply`
    materialises its own create-only `jm_test.h`, which doppler does not use
    and which defines the retired `ALMOST_EQ` / `ALMOST_EQ_C` spellings this
    gate exists to forbid. It is gitignored and never lands, but a plain glob
    still found it and reported four violations in a file nobody had written
    and nobody could remove for good. The scan now derives from
    `git ls-files`, the same fix `validate-c` already uses after a glob there
    ran a stale binary whose source had been deleted. Verified to still catch
    a tracked violation, and to scan the same 90 tests and 8 harness headers
    as before.

- **The TED normaliser was never broken for DTTL; the measurement that said
    so differentiated the wrong equilibrium.** The RateSync validation
    report carried F15 — a normalised through-cascade S-curve slope of ~1.0
    for Gardner but 1.23 rising to 10.75 for DTTL across roll-off, tracked
    as [gh-669](https://github.com/doppler-dsp/doppler/issues/669) with the
    cause explicitly open. Both harnesses took that slope about a fixed
    offset of zero, which through the cascade is the **unstable T/2
    equilibrium** rather than the eye centre. Measured at the stable zero,
    DTTL reads 0.9998–1.0013 at every roll-off from beta 0.1 to 0.9, so `bn`
    names one loop bandwidth on either detector — exactly what
    `symsync_ted_slope` was always supposed to deliver.

    Two things hid it. Gardner's S-curve is near-sinusoidal, so its two
    zeros carry the same slope magnitude (1.0036 against 1.0044) and the
    shipped default detector read correct at either — only DTTL, whose curve
    is not sinusoidal, could expose the error, and it surfaced as a spurious
    *roll-off* dependence that sent the investigation after the pulse and
    the normalising formula instead of the offset. And the stable/unstable
    labelling came from a hard-coded `slope <= 0` test, which is meaningful
    only relative to a timing axis: the Python validator offsets the
    decimation phase and the C harness offsets the transmitter, so their
    axes run in opposite senses, every slope sign is negated between them,
    and the two agreed on every measured number while disagreeing about
    which zero to call stable.

    Both now locate the equilibrium by **eye opening**, which has no sign
    convention — mean `|symbol|` is 1.000 at the eye centre against
    0.53–0.79 at T/2. `validate_ratesync_scurve` reports both zeros, so the
    retired figures remain on the page as the unstable column; its DTTL
    ratchet is replaced by a real gate on both detectors, sabotage-verified
    by pointing the search back at the wrong equilibrium. The claim in
    `symsync_ted_slope`'s own doxygen that the shipped normalisation "varies
    10.6x between beta 0.1 and 0.9" came from the same measurement and is
    **withdrawn**.

- **A merge conflict could reach the docs site, three different ways.** The
    gate is ported from
    [just-makeit#977](https://github.com/just-buildit/just-makeit/pull/977),
    where one sat in `docs/configuration.md` for **ten days and a release** and
    rendered as page content. doppler's tree was clean, so this closes a hole
    rather than repairing damage — but doppler had three independent ways to
    miss it, and any one of them is sufficient on its own.

    **1. mdformat normalises the markers instead of refusing them**, so every
    pass through `make format` made the corruption *less* visible:

    | in the conflict | after mdformat                                                                                                                                       |
    | --------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
    | `<<<<<<< HEAD`  | `\<<\<<\<<< HEAD` — every `<` escaped, reads as prose                                                                                                |
    | `=======`       | **gone.** A line of `=` under text is a setext H1, so the sentence above is silently promoted to a heading and lands in the page's table of contents |
    | `>>>>>>> sha`   | `> > > > > > > sha` — seven nested blockquotes                                                                                                       |

    A check written against the literal three markers finds **one** of those
    three, and the `=======` case cannot be found after the fact at all.

    **2. mdformat ran first.** It was hook 59; `check-merge-conflict` was hook
    109 — so even a raw marker was rewritten before anything looked for it. The
    new hook is first in the file, and that ordering is load-bearing rather
    than tidy.

    **3. `check-merge-conflict` was inert anyway**, and this is the one worth
    knowing. Its first statement is
    `if not is_in_merge() and not args.assume_in_merge: return 0` — so outside
    an active merge it returned success without opening a file, on every
    ordinary `make lint` and every CI run, while printing `Passed`. A conflict
    that survived a rebase, or was resolved badly and committed, was invisible
    to it. It is **removed**: two checks for one property, where the weaker one
    is louder, is worse than one that always looks.

    `scripts/conflict-check.sh` runs over the tracked set on every lint,
    matching all five forms and anchored at column 1 so a marker quoted inside
    a fenced code block still passes — this repo's own prose quotes them, and
    git never writes one indented. The logic is a script rather than an inline
    recipe **so a test can drive it over seeded files**; a lint target whose
    only exercise is corrupting the repository is a target nobody proves.

    `src/doppler/tests/test_conflict_markers.py` covers all five forms, the
    code-block carve-out, the real tracked tree, and that `make lint` reaches
    it. Mutation-tested rather than assumed: dropping the two mdformat patterns
    fails exactly the two mdformat cases and leaves the raw three green, and
    dropping the `^` anchor fails the code-block case **and** the real-tree
    case — the anchor is load-bearing here, not theoretical. End to end, a
    seeded conflict in `docs/index.md` fails `make lint` with the marker still
    raw, which is the ordering fix demonstrated rather than argued.

- **The `docs-drift` hook ran in no context at all, and its file filter named
    a path that no longer exists.** Two independent defects, either of which
    alone made it inert.

    It sat at `stages: [pre-push]`, and **nothing installs a pre-push hook
    here**: `make setup` runs `pre-commit install`, which creates
    `.git/hooks/pre-commit` and nothing else, and no
    `default_install_hook_types` key asks for more. CI does not cover it
    either — `make lint` is `pre-commit run --all-files`, which runs the
    default stage. Measured, not inferred: pushing a branch that modified four
    `.h` files produced no hook output whatsoever.

    The staging had a stated reason — the entry needs `uv run`, and "the CI
    `pre-commit` job's bare setup-python environment does not have" it. That
    stopped being true when the job moved to `./.github/actions/setup-uv` with
    `sync: --group dev`, the same change that made every other hook dispatch
    into the Makefile. The rationale outlived its fact, and the hook kept the
    staging that cost it its execution.

    Its `files:` filter was `^(docs/.*\.md|docs/index\.md|jb\.toml)$` — and
    `jb.toml` became `bootstrap.toml` (jm 0.57.0, gh-936). So it named a path
    that no longer exists while missing the real input to
    `gen_install_scripts.py`, and never covered `gen_validation_log.py`'s
    inputs at all: editing the file a generator reads would not have run that
    generator's check. The filter is gone rather than corrected — keeping one
    in step with four generators' input sets is a second source of truth for
    what they read, and the whole target takes 0.67s.

    Now on the default stage with `always_run: true`, and proven the way this
    repo requires: it reports Passed on a clean tree, and drifting `README.md`'s
    synced body makes it fail naming `scripts/gen_readme.py --write`.

    **`clang-tidy` is left dead deliberately** — it is the other `pre-push`
    hook, and switching it on means 118 pre-existing findings (#723) blocking
    every push. Tracked in #737, which now records the part that is easy to
    miss: clearing those 118 does **not** by itself revive the gate, because
    the install mechanism is the actual defect. Whatever lands last has to give
    it an execution home too.

- **`changelog-check` asks what THIS BRANCH changed, and it now runs in CI at
    all.** Two independent failures in one gate, and the second was the worse
    one.

    It was **inert**: the question it asked was *"is `[Unreleased]` empty while
    code has shipped?"*, which stops having an opinion the moment a single
    entry exists — so every branch after the first passed for free. #700
    shipped a public C API, the whole EMA primitive, with no entry at all,
    straight through this target. That is #705, now closed.

    It also **ran nowhere**. It was listed in `GATES_DEPS` and nothing else,
    and no CI job runs `make gates` — CI's lint job runs `make lint` and
    nothing else, deliberately. So a gate this repo believed it had was
    executed by no PR, ever, for as long as it has existed. Being inert was
    only half the problem; `gates` is a local convenience, not CI. It is a
    prerequisite of `lint` now, beside `tests-ssot`.

    The new question is per BRANCH: a branch that changes
    `src`/`native`/`objects`/`ffi` must also touch `CHANGELOG.md`. An answer
    that cannot be satisfied by somebody else's earlier commit is precisely
    what the repo-state question lacked. Both questions live in the one target
    and share one definition of "code", because two targets would be two places
    to keep that list right.

    **Touching the file is the bar**, not growing a particular section — a
    change that genuinely warrants no user-facing note is one honest line from
    passing, and a gate that argues with its author about which changes
    *deserve* an entry is a worse gate. Inert on `main` by construction: HEAD
    is an ancestor of the base, so the range is empty and there is nothing to
    judge. It **fails closed** when it cannot find a merge base, so a CI wiring
    mistake is loud rather than a silent pass — which is why the lint job now
    checks out at `fetch-depth: 0` and passes the PR's base SHA as
    `CHANGELOG_BASE` (a `pull_request` checkout has no local `main`).

    Proven by sabotage in both directions and at the wiring: a committed code
    change with no `CHANGELOG.md` fails and names the file, adding one line
    clears it, a bogus base fails loudly instead of passing, and `make lint`
    itself goes red — the last being the half that was missing before.

    The design is lifted from
    [just-buildit/just-makeit#956](https://github.com/just-buildit/just-makeit/pull/956),
    which cites #705 by URL as its motivating case: jm reached the same
    conclusion from the other side, having assembled a seven-PR release in
    which not one PR wrote an entry.

- **The AGC's decimated loop filter integrated rectangularly, and `decim`
    now comes with a rule.** The detector's pole was compounded; the loop
    gain was not — it scaled linearly as `d*4*loop_bw`, the rectangular
    approximation to `1-(1-4*loop_bw)^d`. `(1 - d*k1)` is always the
    smaller, so **a larger `decim` always converged faster**, which is the
    2.53 dB spread `test_agc_core.c` §23 had recorded and declined to
    assert. Now `ema_alpha_decim(4*loop_bw, d)`, exact at `d == 1`.

    That cut it 3.3x (2.53 → 0.77 dB at the same settings) and did not
    remove it, because the rest is the **first-order hold**: the applied
    gain ramps across each chunk, so a longer chunk ramps over a longer
    span and the detector sees a different signal. Not a coefficient, so
    not compoundable — but boundable, by one number:

    **Keep `4*decim*loop_bw <= 0.05` and `decim` costs under 0.3 dB of
    transient.** Below that the worst case scales as roughly 6x the group,
    so halving it halves the error; §23's own settings sit at 0.32, six
    times the rule, which is why the anomaly showed up there.

    Both step directions are quoted in the header table because the loop is
    not symmetric — the detector is inside it and measures power, so a
    **rising** gain costs ~4x a falling one and sets the rule. That was
    nearly missed: the first sweep measured only the falling direction and
    put the promise at 0.1 dB. `agc_demo.py` cold-starts into a weak signal,
    so its new step-response family assert failed at 0.232 dB and forced the
    correction before any of it shipped — the example working as a gate, not
    an illustration. `docs/dev/contributing/validation.md`'s checklist now names that
    step explicitly.

    §23 asserts both directions, and they do different jobs: reverting the
    compounding moves the falling case 0.059 → 0.146 dB but the rising one
    only 0.197 → 0.232 dB, so the falling case is the **regression
    detector** (verified by reverting it) and the rising case pins the
    promise. Stated in the test, because a bound that cannot fail is
    decoration. Closes #699.

- **75 assertions across 20 C tests could not fail.** The hand-written
    epilogue every test carried — `if (_fails) { …; return 1; }` then
    `printf ("… PASSED"); return 0;` — had drifted in 20 files so the gate sat
    **above** later assertions. Anything checked after it printed `FAIL` to
    stderr and the test still exited **0**, which CTest reports as green.
    Demonstrated rather than argued: a deliberately broken check in
    `test_acc_cf64_core.c` produced

    ```
    FAIL …/test_acc_cf64_core.c:71  acc_cf64_get_acc (b) == 123456.0 + …
    test_acc_cf64_core PASSED
    EXIT=0
    ```

    **This is how the `specan_get_state` heap overflow below stayed hidden** —
    the state round-trip that would have caught it was one of the 75.

    `DP_TEST_END` replaces the whole shape and reports as the **last statement
    of `main`**, so nothing can be appended after it and the drift is
    unconstructible rather than merely discouraged. It also fails a test that
    asserted **nothing**: a body that is `#if 0`-ed out, or a loop that never
    ran, otherwise exits 0 and reads as passing forever.

- **The C-library tarball could be built empty, and would have shipped.**
    Found by running `package-c-tarball` for the first time — the point of
    making it a target. Two defects, both invisible while the logic lived in
    `release.yml`: the staging prefix was reached as `$(CURDIR)/$(C_INSTALL_DIR)`,
    which is nonsense once `BUILD_DIR` is absolute (`/home/…/doppler//tmp/…`),
    so cmake installed to one path while `tar` read another; and `tar` **wrote
    a 20-byte archive on its way out**. Had the prefix been merely empty rather
    than absent, tar would have exited 0 and a GitHub Release would have
    carried an empty tarball.

    `$(abspath …)` fixes the path for relative and absolute alike. The archive
    is then checked for `include/`, `lib/` and `lib/pkgconfig/` — the three
    documented consumer faces resolve through those — so a partial install
    fails the build instead of shipping. Verified: a tarball with `lib/` but no
    `include/` is refused; the real one carries 241 entries. The prefix is also
    staged fresh each run, since installing into a surviving one MERGES and a
    deleted header would ship forever.

- **`glibc-check` reported ALL GREEN when there was nothing to check.**
    Pointed at a directory with no `libdoppler.so`, `objdump` wrote its error
    to stderr, `$BAD` came back empty, and the target printed
    `all glibc symbols <= 2.28` and exited 0 — "no bad symbols found" and "no
    symbols found" were the same green line. Found while wiring `glibc-gate`,
    which is what raised the stakes: harmless when a human ran it right after
    a build, a false pass once it became the payload of a gate in
    `GATES_DEPS`.

    Both readings of nothing now fail closed — a missing `.so`, and a `.so`
    carrying no versioned glibc symbol at all — each naming which one it was.
    All four paths were exercised: the two vacuous ones now exit 1, the
    old-glibc build still passes, and the host build still fails on 2.29. The
    success line also reports the highest symbol actually seen, so a pass
    shows its evidence instead of asserting itself.

- **`max_out` is a pre-allocation hint, and four doc comments said it was a
    limit.** `nco_core.h` and `lo_core.h` both called `*_max_out()` the
    "maximum samples per call", and `nco_core.c`/`lo_core.c` both said a
    request past 65536 "overflows the buffer and is undefined behaviour". All
    four described the contract from before `pass_capacity` (jm gh-138) began
    telling each kernel the caller's capacity: every generator now clamps to
    its own `max_out` argument and returns what it wrote, and the Python
    binding grows its buffer on demand.

    Measured, not reasoned: a 70000-sample request returns 70000 correct
    samples on `lo.steps`, `lo.steps_ctrl`, `nco.steps_u32`,
    `nco.steps_u32_scaled` and `nco.steps_u32_ctrl` alike.

    The LO copies were found first and the NCO ones only because the sibling
    was then checked — one false sentence duplicated four times, which is the
    documentation form of the duplicate-that-drifts. `test_nco_core.c` §17
    pins the behaviour on each of the three NCO output mappings separately,
    since each has its own kernel and could regain a private ceiling
    independently; `test_lo_core.c` §19 does the same for the LO.

- **`LO` documents an SFDR *bound*, not just the typical figure.** The header
    said "~96 dBc" with no qualification, and a design sizing a spur budget
    would have taken that as the guarantee. It is not: the spur level is set
    by the LOW 16 bits of `phase_inc` — the remainder the LUT index discards —
    not by the frequency. Three regimes, measured: a remainder of zero
    truncates nothing and is spur-free to ~146 dBc; a generic remainder gives
    96.32–96.33 dBc across 400 random rates; and a HALF-bin remainder
    (`0x8000`) makes the error alternate with period 2, concentrating all of
    it into one spur at **92.40 dBc** — the classical `6.02·B − 3.92`
    phase-truncation bound, and 3.6 dB below what a caller would have
    budgeted. That set is not exotic: every increment congruent to `0x8000`
    mod 2^16 is in it.

    `lo_core.h` now states all three regimes and the real guarantee,
    **SFDR ≥ 90 dBc at any frequency** (2.4 dB of margin on the measured
    worst case, which held across 8 carrier positions × 2 capture lengths).
    The bound is gated rather than merely documented —
    `test_lo.py::test_sfdr_worst_case_meets_the_documented_bound` measures the
    worst remainder and pins it to the theory bound; dropping the phase index
    to 14 bits takes it to 84.06 dBc and turns the gate red.

    No behaviour changed: this is what the LO has always done, now stated
    correctly. Full characterisation, including the closed-loop cost of the
    LUT (half a phase bin, and no bandwidth):
    `src/doppler/source/tests/validation/lo/results.md`.

- **One `double` → phase-word conversion, with two named faces over it.**
    The conversion every rate-bearing object depends on existed in private
    copies at each call site, each computing its own cast and each undefined
    at its own boundary. `nco_phase_units()` is now the one place: total and
    saturating — below zero (and NaN) gives 0, at or above 2^32 saturates to
    2^32−1, in between it truncates toward zero. C99 gives the integer half
    of a phase accumulator away free (6.2.5p9, 7.20.1.1), so undefined
    behaviour can only enter at 6.3.1.4, which makes confining the cast
    structural rather than stylistic.

    Two faces share one body, so a call site declares its dimension instead
    of leaving it inferable from the assignment target:
    `nco_norm_freq_to_inc` (cycles per sample → a phase **increment**, 14
    sites) and `nco_norm_phase_to_word` (cycles, absolute → a phase **word**,
    3 sites). The old `nco_norm_to_inc` is gone.

    This is also the `rate == 1.0` fix: `(uint32_t)(2^32 / 1.0)` is the
    out-of-range conversion and yields 0 on x86 — an increment that never
    advances. **`Synth(sps=1, pulse="rrc")` emitted an all-zero waveform**
    (rms 0.0, one distinct sample) while sps 2/4/8 were correct.
    `nco_step_u32_ovf_ctrl`'s carry is still wrong for a negative control and
    carries an `@warning`; it is fixed separately.

    **Read this together with the interpolator's rule below, which lands the
    same boundary the other way.** Both are in this release and they are not
    in conflict: they are one value under two rules. Here, emission is gated
    on the phase advancing, so an increment of 0 emits nothing and saturating
    to 2^32−1 is what restores output. Under the interpolating rule outputs
    are emitted every tick and the phase word only decides when to *load*, so
    at rate 1 an increment of 0 is exactly right — one input per output, the
    phase pinned to one arm, and the signal filtered by that arm. `resamp`
    therefore lands the boundary modularly in its own `_step_inc` rather than
    through `nco_phase_units()`, which keeps saturating for phase
    accumulators, where a value beyond one cycle per sample is genuinely a
    limit.

- **The timing detector normalises by its own slope, computed at
    construction, instead of by a running power average.** A TED's raw output
    is the timing error times three things it did not choose — the signal
    amplitude, the transition density, and its own slope against the pulse —
    and only the last belongs to the detector. Dividing by an average of
    `|on|² + |mid|²` got all three wrong. It is an `A²` quantity, so it suited
    Gardner's amplitude law and left DTTL's loop gain proportional to `1/A`
    (a 4× swing over a 4× level change, in the detector BPSK and QPSK
    select). It normalises amplitude rather than slope, so the normalised
    slope varied **10.6×** between roll-off 0.1 and 0.9 — `bn = 0.01` meant
    something an order of magnitude different at the two ends of the
    supported range. And being an average it lagged: seeded on the first
    post-prime strobe, which lands in the cascade's amplitude ramp, it ran
    the loop at thousands of times its designed gain through exactly the
    interval that decides acquisition.

    The matched pair's composite is a raised cosine in closed form
    (`wfm_rc_h()`), so the slope is a construct-time number:
    `symsync_ted_slope()` evaluates `|dS/dτ|` at the lock point and the loop
    stores its **reciprocal**, so the hot path is one multiply — a divide and
    a running average per symbol both became construct-time work. Validated
    against the slope measured open-loop through a real cascade: Gardner
    within 1.3–8.6% across roll-off 0.1…0.9, DTTL within 0.2%.

    Removing the lag removes the acquisition defect it caused: on a fine
    sweep of initial timing offsets at `sps = 4`, a 0.3-symbol-wide band that
    took **7000–25403 symbols** to recover now acquires in **133–266** at
    every offset, and the peak normalised error falls from **38 to 0.13**.

    **Amplitude is now an upstream AGC's job, by contract** — a unity-gain
    matched cascade returns the symbol amplitude it was sent, so the detector
    needs no level estimate. Feed it something else and the loop is
    under-driven by `A²`. `RATESYNC_LOOP_STATE_VERSION` 1 → 2 (`pwr_avg` and
    `pwr_seeded` are gone from the blob). `SymbolSync` keeps its own
    normaliser: `symsync_create()` takes no pulse, so it cannot compute a
    slope yet.

- **`RateConverter` was inventing gain, and now states its own.** The matched
    terminal bank was scaled to unit **energy**, so a correlator returns
    `A·‖h‖` for a symbol of amplitude `A` — and `‖h‖` is an accident of `sps`,
    roll-off and pulse. Measured against a transmitted 0.2500, the recovered
    amplitude ran **0.2284 to 0.3537** (−9% to +41%). Scaling by the pulse's
    own energy `E = Σh(t)²` returns `A·E/E = A` exactly, for any
    configuration: now **0.2482–0.2501**, worst case 0.7%, across `sps` 4→64,
    roll-off 0.2→0.5, compensated or not. With droop compensation folded in
    the reference stays the *undistorted* pulse, because the compensator is
    built as the CIC droop's inverse.

    Nothing could have caught it: every EVM assertion fits the gain and
    divides it out, and the timing loop normalised by power, so a cascade
    with 3 dB of invented gain passed every existing test.

    So the object now **calculates** its gain rather than being measured for
    it — `RateConverter_gain()` multiplies what each stage reports from its
    own coefficients (`hbdecim_dc_gain`, `cic_dc_gain`, `fir_dc_gain`,
    `resamp_dc_gain`), tracking the measured value to 3.4e-4 at every rate in
    the plan space. Two gates replace a ±15% DC check that covered only
    `compensate = 0`. The probe is a tone at 1/512 of the output rate, not
    DC, because a CIC's DC output is insensitive to its own normalisation
    shift — the offset-binary conversion removes a bias derived from that
    same shift, so halving it doubles the filter's real gain while a DC probe
    still reads 1.000.

- **The matched bank's polyphase arm is a lag in input intervals.** It swept
    `+u/pulse_sps` symbols — a lead measured in *output* periods, which is
    what the superseded accumulator published. Derived from the
    matched-filter identity against the interpolating control port, which
    emits before it loads, the arm is `−u/sps + C`: a lag of one input
    interval per turn of the phase word. The output rate now appears nowhere
    in the bank, which is the check on the derivation — a polyphase arm is a
    fraction of an input interval and nothing else. Matched EVM on
    `CIC(8) + Resampler(0.923)` goes from −10.1 dB to better than −45 dB.

- **A single-phase polyphase bank selects arm 0 instead of shifting by 32.**
    `get_branch` computed `ph >> (32 - log2_phases)`; a one-arm bank makes
    that a 32-bit shift of a `uint32_t`, which is undefined (C99 6.5.7p3).
    x86 masks the count to 5 bits, so it evaluated to `ph` itself and indexed
    `bank[ph * num_taps]` — a read far outside a two-float bank. A one-phase
    bank has exactly one arm, so the phase selects nothing.

    Reachable but masked: the only single-phase user is `wfm_synth`'s
    polyphase RRC shaper at `sps == 1`, whose rate is then exactly 1.0 — and
    the `phase_inc` conversion at that rate was **itself** undefined, yielding
    0 on x86, which pinned the phase at 0 and made the bad shift harmless.
    **The two defects have to be fixed together**: repairing either alone
    turns a silently dead waveform into a segfault. This is the half that
    goes first.

- **`Capture` / `MemoryCapture` publish the constructor they actually have.**
    The stubs rendered `clock: Any = ...` for an argument the binding has
    always required, so a type checker blessed `MemoryCapture(tlm, block)` and
    the call raised `TypeError` at runtime. They now render
    `clock: object | None` — a required positional — and `tlm: object` in place
    of `tlm: Any`.

    Nothing about the behaviour changed; only its published description caught
    up, which is why the test asserting the argument is not omittable stayed
    green across the fix. The two axes were always separate: `clock=None` being
    *accepted* (0.53.0, above) and `clock` being *omittable* are different
    questions, and only the first was ever in play.

    Carried by the **just-makeit pin 0.53.0 → 0.53.1** (gh-845): jm's
    module-aggregated `.pyi` producer decided "required positional" from the
    manifest's `required` flag, which gh-805 §H had set `False` for every
    capsule — so the standalone producer was fixed and the aggregated one was
    not. It now tests capsule-ness directly. Unlike §H, this one arrives on a
    plain pin bump, because a stub is regenerated wholesale while a sacred
    fragment's existing member body is not.

    With it, `scripts/.init-param-optionality-ignore` is **empty** — the gate
    reported both entries as no longer diverging and refused to pass until they
    were deleted, which is the behaviour that list was built to have.

    The pin also carries **gh-844**: five hand-rolled TOML escapers at three
    levels of completeness, three of which emitted strings `tomllib` refuses
    (a lone `CR`, `U+007F`), now routed through one implementation — and
    `jm`'s `_dump` no longer returns a manifest whose self-check failed to
    parse, which is how three escaping bugs survived three releases.

- **`awgn_generate` shipped two implementations that produced different noise
    from the same seed, and every AWGN-derived number was therefore
    platform-dependent.** It dispatched at run time between a scalar path and an
    AVX-512 one. `awgn_create (42, 1.0f)`:

    |       | scalar                      | AVX-512                     |
    | ----- | --------------------------- | --------------------------- |
    | `[0]` | `-0.268593192  0.581977606` | `-0.268593192  0.581977606` |
    | `[1]` | `-0.054472364 -0.171774969` | `-0.345792860  0.103448674` |
    | `[2]` | `-0.578596532 -0.357516527` | `0.134781227 -0.333317131`  |

    BER curves, EVM, validation output and the published benchmarks all inherited
    that, silently, because the assertions on them are statistical and **both
    streams pass**. Closes #690.

    **Nothing caught it, and one test is why.** `test_reset_reproducible`
    compares a stream to *itself* after `awgn_reset` — one path, one machine — so
    it passes identically under either implementation. Self-consistency is not
    reproducibility; only an external reference separates them.

    **The fast path is removed rather than repaired, because of where it ran.**
    On Linux it was dead code: `_ZGVdN8v_logf` is a weak, *unresolved* symbol
    (no doppler artifact links libmvec — checked with `ldd` on `libdoppler.so`,
    `test_awgn_core` and `bench_awgn_core`), so the NULL check always sent
    execution to the scalar loop. On macOS/Windows x86-64 the non-Linux `#else`
    defines that symbol as a scalar-loop *shim*, so the check could not fail and
    the vector path was live. The divergence was paid exactly where the
    vectorised logarithm was absent, and the speed-up was unavailable where it
    existed.

    **The eight lanes were never independent**, which is the part worth keeping.
    The per-stream seed was `seed + j * 0x9e3779b97f4a7c15`, and that constant is
    what SplitMix64 adds to its own state on every draw — so advancing the
    *stream index* is the same operation as advancing the chain by one *draw*.
    Measured at seed 42: `vs[w][j] == vs[w-1][j+1]` for 21 of 21 pairs, **11
    distinct words across the 32 state slots**, and stream 0 identical to the
    scalar stream. Eight lanes overlapping in three of their four words are not
    eight streams, and it is why sample `[0]` agreed between the paths while
    everything after it diverged.

    **Compatibility.** No change on Linux or arm64 — both already ran the scalar
    path, confirmed by checking that the pre-change build already produces the
    values now pinned. macOS/Windows x86-64 with AVX-512 changes, and changes
    *onto* what every other platform has always produced. `vs[4][8]` stays
    seeded and serialized: dropping it would change `awgn_state_bytes`, which
    `wfm_synth_state_bytes` includes, churning the state protocol of two objects
    to save 256 bytes.

    **`test_stream_pinned` is the gate**, and it is an external reference rather
    than a mirror: four recorded samples plus a 262 144-sample accumulator, so
    the head being right does not imply the state update is. Sabotage-proven —
    xoshiro rotate `23`→`24`, `s[3]` rotate `45`→`44`, u1 extraction
    `>>40`→`>>41`, and uniform scale `2^-24`→`2^-25` each turn it red. Two
    earlier sabotages *passed* and proved nothing (one pointed the generator at
    `vs[0]`, which the overlap above makes identical to `s[0..3]`), and they are
    recorded in the commit because a no-op sabotage manufactures confidence.

    The 1.6x (446 against 280 Msamp/s with libmvec linked) is worth having back,
    and can return with a parity gate against these pinned values, an explicit
    `-lmvec`, genuinely separated per-stream seeds (SplitMix64 run forward per
    stream, or xoshiro's own `jump()`), and consuming this stream rather than a
    second one of its own.

## [0.42.0] — 2026-08-07

### Breaking

- **`acquire.CarrierAcquisition`'s constructor takes the two rates first, and
    requires them.** The published signature had not been callable as
    documented for months: the `.pyi` advertised
    `CarrierAcquisition(sample_rate_hz, symbol_rate_hz, ..., psd_template=...)`,
    while the compiled extension required `psd_template` **first** and let both
    rates default to `0.0` — which reaches `carrier_acq_create()` as an invalid
    rate and surfaces as a bare `MemoryError`. Following the documentation
    raised `TypeError: function missing required argument 'psd_template'`, and
    a type checker endorsed the failing call, because the stub is all it can
    see.

    The cause was a stale sacred fragment. The manifest moved `psd_template`
    and gave it a default; the `.pyi` regenerated from the manifest and the
    hand-owned binding could not, so the two faces drifted apart and stayed
    that way. jm reported it on every `jm apply`, by name — inside a block of a
    dozen warnings about fragments that were fine, which is why it survived.
    `scripts/check_init_param_optionality.py` now gates exactly this, and
    reported its own exemption as stale the moment the fix landed.

    The manifest's signature wins, because it is the one the docs, the stub and
    the examples all describe:

    ```python
    CarrierAcquisition(sample_rate_hz, symbol_rate_hz, ..., psd_template=...)
    ```

    Both rates are now `required` rather than defaulted, so omitting one raises
    a `TypeError` naming it instead of constructing an object that fails
    opaquely. **Callers passing the template positionally must move it to
    `psd_template=`;** every in-tree caller was updated. Keyword callers, which
    is what the docs have always shown, are unaffected.

### Added

- **Telemetry captures are lossless by construction** (C API;
    `dp_tlm_capture/dp_tlm_capture_core.h`). Dropping was the ring's fallback, and sizing it
    was a question nobody could answer — too small silently loses data, too big
    wastes memory, and neither shows up until after the run. It rests on one
    bound: **no probe emits more than once per input sample**, so a block of
    `N` inputs emits at most `probe_count * N` records
    (`dp_tlm_block_bound()`). Size the ring to that, drain it to empty at every
    block boundary, and the ring *cannot* overflow. That is a proof, not a
    heuristic — no polling interval, no scheduling assumption, no safety
    factor. `dp_tlm_capture_open()` does the sizing itself from the probes you
    attached, so the only number a caller supplies is their own block length.

- **`dp_tlm_set_now()` delegates to an open capture**, and callers already put
    it at the top of the block loop before stepping — so an existing
    `set_now / steps / read` loop becomes lossless by opening a capture and
    changing nothing else. With no capture open it is still a bare assignment.

- **Flat memory on a long capture**, kept deliberately separate from
    losslessness: the capture ping-pongs two staging buffers so a writer thread
    drains one while the producer fills the other. If the writer falls behind,
    the *boundary* blocks — backpressure, never loss. On disk the 16-byte
    `dp_tlm_rec_t` layout *is* the file (`np.fromfile` reads it directly), with
    a `<path>-meta` JSON sidecar carrying the probe table, the counters, the
    sample clock and the dtype, so a capture is self-describing without any
    doppler code. The time base is **borrowed** from the pipeline's
    `dp_sample_clock_t` rather than re-declared as a private `fs`/`t0` pair —
    two copies of a time base drift, and the one written into a file is the
    copy nobody can correct afterwards. An epoch is recorded only when the
    clock is genuinely anchored (`has_anchor`), never when it is
    `dp_sample_clock_init()`'s construction-time capture of "now": stamping a
    replayed 2019 capture as today is worse than no timestamp, because it
    looks authoritative.

- **`dp_tlm_capture_close()` fails loudly on a hole.** The invariant makes a
    drop impossible, so a non-zero count means the block contract was broken —
    a capture with a hole is not a smaller capture, it is a wrong one.

- New C surface alongside it: `dp_tlm_avail()`, `dp_tlm_resize()`,
    `dp_tlm_probe_id_at()`, and `dp_tlm_stats()` returning a by-value
    `dp_tlm_stats_t`. Plus `bench_dp_tlm_core` — the first telemetry
    benchmark in the tree, which is what makes "don't regress the emit path"
    falsifiable at all. It measures detached / decimated / emit / overrun, and
    records that the **overrun path is the slowest of the four** (an atomic
    read-modify-write on the drop counter, against a plain store for a
    successful write).

- **`raw` and `csv` captures keep their metadata in a sidecar.** Both file
    types take `fs`, `fc` and `t0` at construction and have nowhere in the
    container to put them, so they discarded them — the library asked for the
    caller's metadata and threw it away, leaving a file that not even its
    author could interpret later. A path-opened writer now emits
    `<path>.sigmf-meta` beside the capture, carrying exactly what it was
    told.

    The name is **appended, not swapped** (`cap.raw` → `cap.raw.sigmf-meta`):
    swapping would make a raw capture and a genuine `cap.sigmf-data` in one
    directory fight over `cap.sigmf-meta`, so writing one would silently
    retype the other. The file is SigMF-*shaped* rather than a SigMF capture
    — the spec pairs `.sigmf-data`, and for CSV `core:datatype` names the
    value domain the samples were quantised to rather than a byte layout.

    On by default because it is purely additive and the values are already in
    hand; `sidecar=False` opts out when an extra file would break a
    downstream glob. `blue` never participates (its header already carries all
    three, and a second copy is only somewhere for them to drift) and `sigmf`
    cannot opt out, because there the sidecar is half the capture. A sidecar
    that cannot be written fails `close()`, on the grounds that landing a
    capture whose `fs` and `fc` exist nowhere is the outcome the file is
    written to prevent.

- **`wfm.Writer(..., t0=…)` records the capture start.** UNIX seconds in,
    stored as a J1950 timecode in a BLUE header and as
    `captures[0]["core:datetime"]` in SigMF. This is the write half of
    `Reader.t0` / `Reader.t0_source`, which until now had nothing in doppler
    that could produce a reading other than `"none"`. Optional where `fs` is
    required — a capture with no wall-clock anchor is still readable, one
    with no rate is not — and `0.0` means unset all the way to disk, never
    1970\.

- **`dp_isotime.h` formats extended ISO 8601 as well as basic.** SigMF's
    `core:datetime` mandates the separators that make the basic form
    filename-safe; both spellings come out of one formatter, so the
    truncate-never-round rule cannot drift between them.

- **`make check-isotime-parity`** runs just-bashit's `iso-8601-basic` against
    `dp_isotime.h` over 20 stamps and diffs them. The golden vectors in
    `test_dp_isotime.c` are a snapshot of that helper's output, and a
    snapshot goes stale silently; this makes the agreement a fact that is
    checked rather than a comment. Wired into `gates` and into CI, which sets
    `ISOTIME_REQUIRE=1` so an absent reference is an error there rather than
    a skip.

- **The lossless capture has a Python face: `telemetry.Capture` and
    `telemetry.MemoryCapture`.** The C has existed since the capture landed;
    what did not exist was any way to reach it from Python, because its
    constructor takes a `dp_tlm_t *` from an OBJECT and a `dp_sample_clock_t *`
    from a `kind="handle"` module — and neither capsule direction was
    expressible until just-makeit gh-790 and gh-794. Both have now shipped, so
    the whole component is declarative: **no hand-written CPython, nothing in
    `status_allow`.** `wfm.SampleClock` publishes its pointer as a capsule for
    the first time (gh-794), which is what a capture borrows.

    ```python
    with MemoryCapture(tlm, BLOCK, SampleClock(1e6)) as cap:
        for i in range(0, len(x), BLOCK):
            tlm.set_now(i)          # drains the block just finished
            agc.steps(x[i : i + BLOCK])
        cap.close()                 # raises if anything was lost
        recs = cap.records()
    ```

    **Two flavours, because a capture that writes a file cannot hand its
    records back — the file IS the capture.** `MemoryCapture` accumulates and
    answers `records()`; `Capture(…, path, …)` writes raw 16-byte records that
    `np.fromfile` reads directly, plus the `<path>-meta` sidecar, and has no
    `records()` at all. `AttributeError` is the honest answer there; an empty
    array would read as "nothing was captured", which is a different and wrong
    statement. (It is also the only shape available: a jm `path` init-param is
    an `O&` whose call expression is `PyBytes_AS_STRING(...)`, so `path=None`
    cannot be spelled on one class.)

    **A hole raises on every exit path, including `with`.** That is the point
    of the component — the block bound makes a drop arithmetically impossible,
    so a non-zero count means the contract was broken and the capture is not a
    smaller capture but a wrong one. `dp_tlm_capture_destroy()` therefore
    widened from `void` to `int` and reports `close()`'s verdict (the
    `wfm_writer` gh-541 precedent), because `with`-exit and garbage collection
    both land there and would otherwise swallow it. Mutation-tested in both
    directions: making the destructor discard the verdict turns exactly one
    test red, and a clean capture stays silent.

    New C alongside it: `dp_tlm_capture_open_memory()` (the memory
    constructor, so the two flavours differ in constructor rather than in a
    sentinel), and `dp_tlm_capture_read()` / `_read_max_out()` — the copying
    twin of `dp_tlm_capture_records()`, since a borrowed pointer the capture
    can free is not something a binding may hand out. Shaped exactly like
    `dp_tlm_read()` so the two drains bind identically.

    **Known limitation:** the C takes a NULL clock to mean "no time base
    stated" and the sidecar then omits the keys rather than fabricating a
    rate, but a gh-790 capsule *init*-param rejects `None` unconditionally,
    while a gh-432 capsule *method* param maps it to NULL. So `clock` is
    mandatory on the Python face for now. The signature is already the one
    that works once jm allows it, so the fix turns a `TypeError` into a
    working call with nothing to migrate.

### Changed

- **`telemetry.Capture.close()` says what went wrong.** It reported
    `ValueError: close failed (rc=-4)` — the canned string jm's
    `status_return` could only ever raise — while the destructor, reaching the
    *same* verdict, explained itself in full. A caller learned more from
    letting the object fall out of scope than from asking it directly. Both
    now carry the same text, which is the point: they are one condition.

    Needs just-makeit 0.51.0 (gh-823 Ask D), where `status_return` gained
    `error` / `error_message`. Adopting it also required deleting the sacred
    fragment and re-applying — jm only ever *adds* missing members, so a
    changed one stays as written, which is exactly what its new gh-815
    result-shape warning exists to report.

- **BREAKING: `telemetry.Telemetry` renames four members**, as a consequence
    of the module becoming fully manifest-owned — its binding is now generated
    from `objects/dp_tlm.toml` with no hand-written CPython at all, and one
    manifest string names both the C parameter and the Python keyword.

    | was                     | now           | why                                                                                                        |
    | ----------------------- | ------------- | ---------------------------------------------------------------------------------------------------------- |
    | `read(max_records=-1)`  | `read(n=0)`   | one name for the C parameter and the keyword; `0`, not `-1`, means "everything available"                  |
    | `probe_names()`         | `probe_names` | a declarative `type = "dict"` **is** a property in just-makeit; a dict-returning method is not expressible |
    | `emit(probe_id, value)` | `emit(id, v)` | as above — the manifest names it once                                                                      |
    | `emitted(probe_id)`     | `emitted(id)` | as above                                                                                                   |

    Only `probe_names` is a forced change; the other three are the cost of
    having one name instead of two that can drift. Positional calls to `emit`
    and `emitted` are unaffected — only keyword callers need to change.

    `Telemetry(0)` and a non-power-of-two size now both raise `ValueError`
    rather than `ValueError` and `MemoryError` respectively. They were always
    the same `NULL` from `dp_tlm_create`, and a rejected size is what it
    almost always means.

- **BREAKING: `ber.BerMeter.set_truth()` returns `None` and raises.** It
    returned `0` and raised nothing, while its own documentation said
    *"Raises ValueError if any index is outside 0..m-1"* and its C returns a
    plain `DP_OK`/`DP_ERR_INVALID` status. The manifest had declared that
    intent twice, in two spellings that both silently did nothing on a method
    (`error` before just-makeit gh-805 §B, and `check_return`, which is a
    `jm function` key). The header's own doctest had recorded the wrong
    behaviour as expected output, so every face agreed with every other face
    and all of them disagreed with the manifest. It now raises `ValueError`
    on an out-of-range symbol index, as advertised.

- **BREAKING: `wfm.Writer` now requires `fs`.** It used to default to `1e6`,
    which made "nobody stated a rate" and "exactly 1 MHz" the same value — so
    a capture written without one recorded a confident 1 MHz in its header,
    in a file that outlives the process. Every container the writer emits
    needs the rate (BLUE stores `xdelta = 1/fs`, SigMF `core:sample_rate`),
    the caller knows it at construction, and that is now where it is asked
    for. `fs` moves to the second positional slot, so
    `Writer(path, fs, file_type=…)`; passing `fs=0.0` states "not known"
    explicitly, writing `xdelta = 0` and no `core:sample_rate` at all.

- **BREAKING: `write_blue_header()` now requires `fs` too**, in the second
    positional slot — `write_blue_header(path, fs, sample_type=…)`, mirroring
    `Writer`. `xdelta = 1/fs` is most of what a BLUE header *is*, so a
    detached pair written with a forgotten rate is a real capture on disk
    carrying none, with nobody ever asked for one. Omitting the key ended the
    *fabrication*; it did not make anyone answer. Callers already passing
    `fs=` as a keyword are unaffected. `fs=0.0` still states "not known" and
    writes `xdelta = 0`.

- **`Composer.to_sigmf()` derives an unstated `fs` from its segments.** It is
    the one caller that should *not* be asked: a `Composer` already holds a
    rate per segment, and the annotation edges in the very same document are
    `±fs/(2·sps)` computed from it — so omitting `core:sample_rate` withheld
    a rate the document demonstrably knew. It is derived when the segments
    agree, left unstated when they disagree (no single rate is true of that
    stream), and an explicit `fs=` always wins, for rendering a scene at a
    resampled rate. Not breaking: it makes the no-argument call correct where
    it used to be silent.

- **SigMF metadata omits what it was not told.** `core:sample_rate` is
    optional in SigMF 1.0.0 (only `core:datatype` and `core:version` are
    required in `global`), so an unstated rate is now absent from the
    document rather than emitted as a defaulted number. An absent key says
    "not stated"; a present one is a value a downstream tool will act on.
    `core:datetime` follows the same rule, and so now does
    **`core:frequency`**, which was written as a confident `0` — i.e. "this
    capture sits at baseband" — for every capture that simply never said. The
    BLUE writer had always omitted its `FREQ` keyword at `fc == 0.0` and the
    reader's `fc_source` reports `"none"` for an absent one, so the SigMF
    path was the odd one out rather than this being a new convention.

- **just-makeit pin 0.46.1 → 0.46.2.** Zero codegen drift: `jm apply`
    re-renders four binding fragments in jm's K&R and clang-format returns
    every one of them byte-identical, so only the three pins and `uv.lock`
    change here.

    The bump carries two fixes doppler filed. **gh-777** — a
    `PyMethodDef`/`PyGetSetDef` row that is simply *absent* from a sacred
    fragment is now `stale` rather than `unreconciled`, so `jm status   --check` fails on it and `jm apply` reconciles it. `unreconciled` was
    always meant for a wrapper *body* that differs (the author's, which no jm
    command clears); it was also swallowing a missing row, which let a member
    stay in the `.pyi` while the extension did not define it, indefinitely,
    with CI green. Measured on this tree, the clean result is unchanged —
    `2139 match, 72 unreconciled` — so the reclassification only bites where
    something was already wrong.

    **gh-779** — adding an `enum =` property to an *existing* fragment now
    brings the enum table's declaration with it. It previously emitted the
    table's use and not its declaration, so the module did not compile, and
    the workaround was to delete the fragment and let `jm apply` recreate it.
    That workaround is retired; it is how `wfm_reader`'s `fs_source` /
    `t0_source` had to be added.

### Removed

- `dp_tlm_recorder_*` (never released). A 200 µs polling drain thread traded a
    sizing guess for a scheduling guess and could not claim losslessness
    either way; `dp_tlm_capture_*` supersedes it.

### Fixed

- **The telemetry benchmark was being built from a stub that measured
    nothing.** Renaming the component to `dp_tlm` made `jm apply` scaffold a
    fresh `bench_dp_tlm_core.c` ("no step() to benchmark"), and the CMake
    target it generated alongside built *that* — so the real four-arm
    benchmark sat in the tree orphaned, compiled by no target and run by
    nothing, while `bench_dp_tlm_core` reported an empty `benchmarks[]` JSON.
    A benchmark that measures nothing is worse than an absent one: it is
    green, it is wired into the tooling, and "don't regress the emit path"
    quietly stops being falsifiable.

    The real benchmark now IS `bench_dp_tlm_core.c` — one file, named after
    the component like `test_dp_tlm_core.c`, so the file jm scaffolds and the
    file carrying the measurements cannot diverge again. Filed upstream as
    [just-makeit#806](https://github.com/just-buildit/just-makeit/issues/806),
    since a generated stub silently displacing a real benchmark is jm's to
    stop, not each project's to notice.

## [0.41.0] — 2026-08-03

### Added

- **Every public Python surface now carries a full docstring on both faces.**
    Class, method, property, module-function and result-record-field
    documentation is derived once from the C `_core.h` Doxygen and rendered to
    both the `.pyi` stubs and the runtime `help()` — Parameters, Returns, and a
    runnable example wherever the shape allows. A ratcheted coverage meter
    (`make check-docstring-coverage`, wired into the docs gate) prevents
    regressions; coverage is 91.2%, the remainder being jm-generated
    boilerplate glue that has no example to show.

- **A family of purpose-built container images** (see
    [`docs/install/docker.md`](docs/install/docker.md)):

    - `ghcr.io/doppler-dsp/doppler` — the runtime "try it" image: the CLIs plus
        the runnable example gallery, so `docker run` lands you among demos.
    - `ghcr.io/doppler-dsp/doppler-sdk` — doppler installed (headers, static +
        shared library, CMake config, `.pc`) with the toolchain and
        `just-makeit`, for building your own C / jm project on doppler.
    - `ghcr.io/doppler-dsp/doppler-downstream-jm` — the iqtools showcase, shipped
        already built with its whole suite green.

    All three are built from one shared stage, so they are version-locked to the
    same doppler source.

### Changed

- **Adopted just-makeit 0.35.0 → 0.43.0.** The both-faces docstring derivation
    above is the headline; the run also brought result records
    (`single = true` structseq), included-header field-doc derivation, block-tag
    and list/table rendering, and glue-method runtime docs.
- **The Makefile is the single driver for CI.** System-dependency install
    (`make install-deps` / `install-docs-deps`), the manifest version-consistency
    check (`make version-check`), and the three-consumer-faces smoke
    (`make consumer-faces-check`, building against cc / CMake / pkg-config) now
    run through make in CI exactly as a developer runs them; the container
    images build through `make docker-*` targets.
- The glibc-2.28 floor job now builds in the manylinux image doppler actually
    ships from, retiring the EOL-Debian archive-mirror workaround.

### Removed

- **The binary-dump root `Dockerfile`.** Its role (teaching by shipping stripped
    binaries) is replaced by the SDK image, which ships consuming-project
    *sources*; `docker-compose` now runs a lean streaming-services image.

## [0.40.0] — 2026-07-31

### Added

- **[`examples/downstream-jm/`](examples/downstream-jm/) — a worked example of
    building *on* doppler from another just-makeit project.** It answers two
    questions the docs did not: how to link `libdoppler.a` from a separate
    project, and how to put your own Python API over a doppler C core without
    hand-writing any CPython.

    It links doppler declaratively — `extra_link_libs = ["doppler::doppler-static"]`
    in the module manifest, resolved by `find_package(doppler)`, which works
    against an install *or* a build tree (`-Ddoppler_DIR=…`) with nothing else
    changing. And it declares a **jm view**: `Capture(path)` auto-detects a
    self-describing capture, while `RawCapture(path, sample_type=…, fs=…, fc=…)`
    is the honest constructor for a headerless one — two Python classes, one C
    core, no hand-written binding.

    The view earns its place rather than demonstrating syntax. On the same
    headerless ci16 file, `Capture` reports **half** the samples and raises
    nothing (it falls back to cf32 and reads at the wrong stride); `RawCapture`,
    told the sample type, recovers the true length. Both behaviours are pinned
    by tests, as is the `metadata_source` property that distinguishes a default
    from a reading. The C core is pure glue — every function forwards into
    `wfm_reader_*`, so no DSP is duplicated.

    **It is gated in CI, so it cannot rot.** Three gates, all driven from the
    Makefile and folded into jobs that already exist:
    `make test-example-downstream` (configure + build + CTest, deliberately
    `BUILD_PYTHON=OFF` so it also runs in the Python-free glibc 2.28
    container — answering "can a downstream link `libdoppler.a`?" on three
    platforms); `make test-example-downstream-python` (builds the extension,
    runs the example's own suite, which is what pins the view's behaviour); and
    `make drift-check`, which now also runs `jm status --check` against the
    example's *own* manifest using doppler's pinned jm. Each was verified to
    fail when its subject breaks, not merely to pass today.

- **`Reader.fc` now reads a BLUE capture's centre frequency**, and
    **`Reader.fc_source`** says where it came from. Type-1000 has no HCB
    field for centre frequency, so an RF capture carries it as a keyword —
    doppler ignored every one of them and reported `0.0`, indistinguishable
    from a genuine baseband capture, on files it did not write. Both
    encodings are accepted: ASCII in the HCB keyword area (BLUE 1.1
    §3.1.1.24.1, which is where captures in the wild put it) and a typed
    value in the extended header. The conventional tags are tried in order —
    `FREQ`, `RF_FREQ`, `CENTER_FREQ`, `F_C` — and `fc_source` reports which
    answered, or `"none"`. Check it before trusting `fc == 0.0`: `"none"` is
    the only thing that distinguishes "not found" from a capture that really
    does declare 0 Hz. A value that is not a bare number (`FREQ=2.4 GHz`) is
    left alone rather than guessed at, and stays visible in `.keywords`.

- **`Writer(..., fc=...)` now records it for BLUE**, in both places a reader
    looks: a typed `FREQ` in the extended header (§3.4-compliant, full
    double precision) and an ASCII `FREQ=<value>` mirror in the HCB keyword
    area (where X-Midas looks, and where §3.4 warns it may be deleted to make
    room for `IO`/`VER` — hence the mirror, not the original). Detached
    headers written by `write_blue_header` carry it too. `fc=0.0` writes
    nothing, since it is also the default for "not supplied".

- **`Reader.trailing_bytes`** — payload bytes past the last whole sample.
    Non-zero means the `sample_type`/`endian` hint is wrong for a headerless
    container, or the capture is truncated. A headerless container cannot
    check a hint against the file, so a wrong one does not fail; it returns
    plausible garbage at the wrong stride, and this is the only signal there
    was none of.

- **[Reading captures](docs/guide/wfmgen/reading.md)** — a new guide, with a
    table of what metadata each container actually preserves in each
    direction.

### Changed

- **Capture I/O is documented as its own topic, not as part of the waveform
    generator.** `Reader` and `Writer` rendered on a page titled *"Python:
    Waveform Generator (Synth / PN)"* and their guides sat under the
    *"Waveform Generator (wfmgen)"* nav section, so someone holding an IQ file
    had no path to the class that opens it — and most captures worth reading
    were not generated here in the first place. Reading a capture and
    generating a waveform are now independently findable:

    - new **[Python: Capture I/O](docs/api/python-wfm-io.md)** API page holds
        `Reader`, `Writer` and `write_blue_header`;
    - new top-level **Capture I/O** guide section holds
        [Writing captures](docs/guide/wfm-io/writing.md) (moved from
        `guide/wfmgen/output.md`) and
        [Reading captures](docs/guide/wfm-io/reading.md) (moved from
        `guide/wfmgen/reading.md`), with a landing page covering the round
        trip and the two ambiguities a capture reader has to know about.

    **Two guide URLs changed**, since there is no redirects plugin:
    `guide/wfmgen/output.md` → `guide/wfm-io/writing.md` and
    `guide/wfmgen/reading.md` → `guide/wfm-io/reading.md`. Every in-repo link
    was updated; the `wfmgen` guide and API pages now point into the new
    section. See [#545](https://github.com/doppler-dsp/doppler/issues/545).

- **The Makefile now includes a shared `standard.mk`, and three targets were
    renamed.** doppler adopted the cross-org Makefile standard
    ([#555](https://github.com/doppler-dsp/doppler/issues/555)): the shared
    targets live in a vendored `standard.mk` fetched from
    <https://just-buildit.github.io/standard.mk>, and doppler's own Makefile is
    configuration plus the 26 targets that are genuinely its own. What this
    changes for a contributor:

    - **`make python-test` → `make test-python`**, **`make rust-test` →
        `make test-rust`**, **`make check-version` → `make version-check`** —
        the noun leads, so `make test-<TAB>` completes the whole family.
    - **`make docs-build` is gone**; it never existed as a rule, though
        CONTRIBUTING named it in three places. Use `make docs`.
    - **`make wheel` now builds a wheel.** It was a `.PHONY` with no rule:
        advertised in `make help`, exiting 0, producing nothing.
    - **`make help` is generated** from each target's `##` comment and lists
        all 67 targets, where the hand-written list showed 30 of 50 and
        advertised two that did not work.
    - **Gates that were CI-only are runnable locally by name**: `abi-check`,
        `link-check`, `glibc-check`, `specan-check`, `test-stubs`,
        `test-api-docs`, `test-snippets`.
    - **`make lint` now also fails** on a target with no doc comment, a
        `.PHONY` with no recipe, or a vendored `standard.mk` that differs from
        canonical.
    - **`make test-python` selects exactly what CI selects.** It used to run
        everything under `src/` locally while CI excluded the `docs_snippets`
        and `examples` markers — the same target name meaning two different
        things depending on where you stood.

    The lint/format toolchain moved into `pyproject.toml`'s `dev` group and is
    pinned by `uv.lock`; the pre-commit hooks dispatch into the Makefile rather
    than resolving tools themselves, which is what makes local and CI resolve
    identically. Versions are unchanged, so nothing in the tree reformats.

### Removed

- **The per-PR performance regression gate (`perf-regression.yml`).** It
    reported regressions whose sign reversed when the same comparison was run
    locally, and being advisory (`continue-on-error`) it could not be trusted
    in either direction — an advisory gate nobody believes costs attention and
    buys nothing. It was never a required status check, so no PR behaviour
    changes. See [#543](https://github.com/doppler-dsp/doppler/issues/543).

    **Benchmarking is unaffected.** `make bench-interleaved` remains the sound
    comparison — it builds both sides in worktrees and runs them *alternately*,
    keeping the per-benchmark best, which is precisely what the CI gate lacked
    (it always ran base first and head second, charging any drift on the runner
    to the head). `make bench-baseline`, `make bench-check` and
    `[project.bench]` are all kept: the fault was in the harness, not in the
    comparison.

### Fixed

- **The mdformat plugins are pinned, so local and CI cannot disagree about a
    file.** `additional_dependencies` resolve when the hook environment is
    created, not when the config is written — so a warm local cache and a fresh
    CI runner could hold different versions and format the same markdown
    differently, while the config still read as fully pinned. Measured
    directly: the stale local environments held `mdformat-gfm-alerts` 2.0.0 and
    `mdformat-mkdocs` 5.1.4 where CI resolved 2.1.0 and 5.2.1. That is what put
    a `docs/api.md` through `make lint` cleanly and then failed the CI
    pre-commit job. All three are now pinned (to the versions CI was already
    resolving, so nothing in the tree reformats), and bumping them is a
    deliberate edit rather than a side effect of a cold cache.

- **`Reader` now detects the container from the file's CONTENT**, not its
    extension. A CSV called `capture.dat` reads as CSV instead of being
    decoded as binary IQ; a BLUE file called `capture.csv` reads as BLUE. The
    name is still consulted where content cannot decide — a `.det` payload,
    and a CSV whose first line is a column header.

- **`Reader.num_samples` is no longer 0 for a CSV**, which read as "empty
    capture". The count is exact (rows are parsed the way `read` parses them)
    and lazy: the scan happens on the first read of the property, so opening
    stays O(1) and a caller that never asks never pays.

- **`Writer(file_type="sigmf")` writes its `.sigmf-meta` sidecar.** It used
    to emit only the samples, and the datatype lives exclusively in the
    sidecar — so the result was not a lean capture but an undecodable one,
    and a working pair required going through `Composer`. Because both halves
    are found by name, the path must now end in `.sigmf-data`; anything else
    is refused with a message saying so, rather than silently producing a
    pair no SigMF reader will find.

- **`make changelog-check` no longer fails on every release PR.** Promotion
    moves each entry out of `[Unreleased]` and into `## [X.Y.Z]`, so the notes
    exist — they are simply no longer where the gate looked, and it failed the
    one PR that had done exactly the right thing. It now passes when
    pyproject's version has a changelog section **and** its tag does not exist
    yet. That second condition is what keeps the exemption from becoming
    permanent: the section stays in the file forever, so without it every
    later branch would inherit the pass and the gate would never fire again.

## [0.39.0] — 2026-07-29

### Breaking

- **Every block-output C kernel now takes a trailing `size_t max_out`.**
    38 more functions gained one this release (78 of them now have it) —
    `fft_execute_*`, `fft2d_execute_*`,
    `lo_steps`, `nco_steps_u32*`, `pn_generate`, `gold_generate`,
    `awgn_generate`, `cic_decimate`, `delay_ptr`, `delay_push_ptr`,
    `psd_*`, `corr_execute`, `corr2d_execute`, `interp_table_execute`,
    `Resampler_execute*`, `HalfbandDecimator_execute`, `acc_trace_value`,
    `wfm_reader_read` and the measure `*_spectrum_dbfs` — carry an
    explicit output capacity. Emission stops at
    that bound and the return value is the number actually written, so an
    under-reporting `*_max_out()` truncates instead of overrunning the
    caller's buffer. **C callers must add the argument**; it is normally
    the same length you already passed for the output buffer. The Python
    API is unchanged — the bindings pass the real buffer size.
- **A wrong-dtype `out=` buffer now raises `TypeError`** instead of
    silently marshalling through a temporary copy, so the caller's array
    was never written. `out=` must be a writable, C-contiguous ndarray of
    the output dtype.
- **`CorrDetector` / `CorrDetector2D` positional argument order** now
    matches the documented signature. 0.38.1 shipped a binding whose
    order disagreed with its own type stub, so the call a type checker
    blessed raised `TypeError` and the call that worked was undocumented.
    Code written against the stub now works; code written against the
    shipped binding must swap to the documented order.

### Changed

- **`execute()` / `steps()` results are NumPy-owned.** Each call returns
    an independent array instead of a view over a reused internal buffer,
    so holding a result no longer risks it being overwritten — or freed —
    by a later call. Holding 3000 x 64k results went from 1,548,388 KiB of
    RSS growth to 1,376 KiB, and the hold path got 82% faster.
- **`wfm.Writer` places non-standard keywords in the extended header.**
    BLUE 1.1 3.4 reserves the 92-byte HCB keyword area for six named
    keywords (`CREATOR`, `IO`, `PACKET`, `PKT_BYTE_COUNT`, `TC_PREC`,
    `VER`); X-Midas and NeXtMidas *delete* user keywords found there when
    the block fills. Anything else now goes where it survives.

### Added

- **`wfm.Reader.header`** — the 512-byte header control block as a
    `{field: value}` dict, under the names the format itself uses
    (`version`, `data_rep`, `data_start`, `type`, `timecode`, … plus the
    type-1000 adjunct `xstart` / `xdelta` / `xunits`). Nothing renamed,
    nothing omitted.
- **`wfm.Reader.keywords` now includes the HCB keyword area**, which was
    previously parsed only from the extended header and silently dropped
    otherwise.
- **`wfm_kw_check_standard()`** — an advisory conformance check for the
    BLUE 3.4.2 standard keywords. It reports and refuses nothing: the
    spec leaves their effect to individual processing systems, so a caller
    reproducing an odd Platinum-era value must be able to.
- **`docs/memory_management.md`** — who owns each buffer and for how
    long, across the C API, the bindings and `out=`. Its C signature
    listings are compile-checked against the real headers.

### Fixed

- **A doubled `@code` block in `awgn_core.h`** that broke the strict docs
    build, and **22 undocumented `max_out` parameters** across 12 headers
    that broke the zero-warning doxygen gate. The affected functions also
    still promised `@return n (always)`, which truncation had made untrue.
- **Eight rotted benchmark fixtures** that had drifted from the APIs they
    construct, plus a test that builds every fixture in every bench module
    so the blind spot cannot reopen.
- **A leak in the hand-owned `Resampler` and `HalfbandDecimator`
    bindings**, neither of which freed its output buffer on dealloc
    (~512 KB and ~256 KB per object).

### Internal

- just-makeit pin 0.33.13 → 0.33.15; `dsss.Acquisition` and
    `BurstAcquisition` are now declared rather than hand-bound.

## [0.38.1] — 2026-07-27

### Added

- **`doppler.ber` — error-rate measurement as a first-class module.** An error
    rate is a measurement, and this is its instrument: the settled window, a
    detected alignment, inverse binomial sampling and an exact confidence
    interval, in C once with a thin Python face.

    - `BerMeter` accumulates errors across bursts and stops on a fixed **error**
        count rather than a fixed symbol count. Under inverse binomial sampling
        the relative standard error is `1/sqrt(r)` — a function of the error
        count alone — so the error target *is* the precision. Stopping on
        symbols makes precision depend on the very rate being measured: 20 000
        symbols at SER 1e-3 yields ~20 errors and ~22% relative error.
    - `BerMeter.align()` **detects** where a recovered stream sits against
        truth, by correlating a known marker (sync word, PN period, or a
        stretch of the truth sequence) and gating the peak on a false-alarm
        probability. It does not search for the lag and rotation that minimise
        the error count — that is an optimisation over the answer, and it both
        false-passes on a lucky alignment and false-floors when the true lag
        falls outside the span. A marker too short to identify an alignment
        reports `ok=False` rather than a plausible wrong lag, and the marker's
        own symbols are excluded from scoring.
    - `ser()` / `ber()` / `interval()` return a `BerInterval` record whose
        limits are the **exact** Gamma/chi-square interval — no normal
        approximation, so it stays honest down to a single error. Its quantiles
        come from `detection.det_threshold_noncoherent`, doppler's own inverse
        regularized incomplete gamma, and are bit-identical to SciPy's
        `chi2.ppf` at r = 1, 2, 20, 200 and 1000. Assert on `lo`, never on
        `p_hat`.
    - Free functions: `ber_theory_ser` / `ber_theory_ber` (the coherent M-PSK
        bound), `ber_esn0_db_for_ser` (quote implementation loss in dB, which
        is comparable across M; a ratio of rates is not), `ber_evm_db`,
        `ber_evm_scatter_floor_db`, `ber_settle_syms`, `ber_settle_from` and
        `ber_lock_symbol`.

- **`ber_evm_scatter_floor_db(m)` — the EVM floor a scattered constellation
    actually reads.** `2 - 2 sin(pi/M)/(pi/M)`: **−1.4 dB at BPSK, −7.0 at
    QPSK, −12.9 at 8PSK**. The familiar "a scattered constellation reads ~0 dB"
    is the BPSK limit only. At 8PSK a stream with no carrier recovery at all
    reads the same −12.9 dB a perfectly healthy 13 dB link does, so any fixed
    EVM threshold must be stated against this floor and never against zero.

- **A real-IF BER validator** (`native/validation/mpsk_receiver_r_ber.c`), the
    twin of the complex-path one, sharing one stimulus and one measurement so
    the two paths stay comparable. Measured implementation loss at each M's
    SER=1e-3 anchor: 0.56 / 0.53 / 1.02 dB real against 0.61 / 0.60 / 0.68 dB
    complex, i.e. the R2C front end costs **~0.38 dB at 8PSK** and nothing
    measurable below it — corroborated to 0.03 dB by the EVM gap.

- **Gallery: "Measuring an Error Rate, Defensibly"** — `BerMeter` on AWGN with
    no receiver in the loop, so the instrument is what is on trial.

### Changed

- **`mpsk_receiver_create()`'s `m_out` documentation now gives the second, and
    at high M dominant, reason the default is 8.** Raising to the M-th power
    auto-convolves the spectrum M times, spreading energy over ~`M*Rs`, and
    whatever exceeds the update rate folds back onto itself; a clean strobe
    raises to a constant with nothing to fold, but every departure from clean
    is splattered M-fold and aliased. So the discriminator's tolerance for a
    coarse matched filter **collapses as M grows**. The matched-filter reason
    documented before is M-independent and predicts the same penalty at every
    order; measured, halving `m_out` from 8 to 4 costs BPSK 1.7 dB, QPSK 1.6 dB
    and **8PSK 3.0 dB**, the last also landing 0.87 dB from the fully-scattered
    EVM floor. `m_out = 8` is not optional at M = 8.

### Fixed

- **An 8PSK EVM assertion that could not fail.** `test_mpsk_receiver_r_core.c`
    asserted `evm < -12.0` at every M, but the 8PSK scatter floor is −12.9 dB —
    so a constellation with no carrier recovery whatsoever satisfied it. It is
    now gated on the floor as well as on absolute quality.

## [0.38.0] — 2026-07-27

### Added

- **Functional and performance tests for `MpskReceiverR`.** The real-IF
    receiver had none: both test files were untouched jm scaffolds — 95 Python
    lines and 58 C lines of construction checks, `test_getter_setter` and
    `test_reset` literally `pass` — so nothing had ever put a signal through
    it. Two things had been hiding behind that, and both are now pinned:

    - **`Ddcr`'s usable band.** The R2C halfband's image rejection is past
        −100 dB across roughly 0.06…0.44 of the input rate but only −7 dB at
        0.01 and −14 dB at 0.02, symmetric about fs/4. The constraint is on the
        signal's **occupied band**, not its centre: a rectangular pulse spans
        `fc ± 1/sps`, and when that reaches an edge the folded image lands on
        the wanted signal. This presents as a receiver bug at low oversampling
        — at `sps=10` an IF at 0.10 reaches DC and EVM collapses to −4 dB,
        while the *same geometry* at fs/4 measures −23 dB.
    - **The noiseless EVM floor, and where it lives.** ~−24 dB, and it is
        **not** the real path's fault: the complex twin measures the same
        −24.5 dB whenever its cascade also contains an integer decimation
        stage, which at any realistic oversampling it does. Both are limited by
        the shared CIC/halfband chain aliasing a rectangular pulse's sinc
        sidelobes. At `sps=2048` the two paths measure −24.5 and −24.6 dB.

    The new `test_mpsk_receiver_oversampling.py` measures **both** receivers for
    EVM and lock time at low/med/high/very-high oversampling (sps 20 → 4096),
    since "R is worse" and "both share a limit" look identical until measured
    together. Three measurement rules are encoded in the shared harness because
    each one, omitted, produces a confident wrong number:

    - the settling budget is `2 · (5/bn_timing + 5/bn_carrier)` — the two loops
        are **cascaded** (the carrier reads the strobe, so it cannot start
        converging until timing has), so the budgets **add**, and joint tracking
        **doubles** the sum. At the defaults that is 2000 symbols, not 500;
        using `max(5/bn)` reads −9.0 dB where the settled answer is −23.2 dB;
    - every case presents an offset **inside** the loop bandwidth (half of
        `Bn` on each loop). Seeded exactly on truth the carrier loop never
        leaves its initial state and any lock time measured is meaningless;
        asserted **outside** `Bn` the test measures luck. Characterised, not
        asserted: carrier lock takes 39/157 symbols at 0.25·Bn, 1376/1701 at
        1·Bn, and **never happens** at 2·Bn — identically on both paths, which
        makes pull-in a property of the shared symbol-rate NDA discriminator
        rather than of either front end;
    - lock time is read from the receivers' own verify-counted detectors, as a
        sustained run rather than a final contiguous one — under AWGN a detector
        legitimately dips, and dating the lock by its last dropout reported
        2286 instead of 415 and looked like a receiver that never locked.

- **`sync.mu` — the timing NCO's phase is now observable.** Every other timing
    probe is an *error* (`sync.e`) or a *correction* (`sync.ctrl`, `sync.rate`);
    `mu` is where the sampling instant actually ended up — the terminal
    resampler's control accumulator, in `[0, 1)` output periods, so the
    polyphase arm the last output read is `mu * num_phases`. A steady `mu` means
    the loop settled on a sampling phase, one that slews and wraps means a
    residual *rate* error still unabsorbed (one wrap is one output period of
    slip), and hash means the loop is being driven by something that is not a
    timing error. It lives on the shared `ratesync_loop_t`, so `RateSync` and
    both `MpskReceiver`s gain it at once — six timing probes now, eleven on a
    receiver — and `resamp_get_ctrl_acc()` exposes the same quantity in C.

    This is a diagnostic that pays for itself immediately: on the gh#536
    `sps=10` investigation it ruled the NCO out as the cause in one plot (rate
    parked 0.03% off nominal, `mu` steady, cumulative slip an eighth of one
    output period across 3000 symbols) after several sessions of hypotheses
    about the accumulator.

- **`nda_tap` — choose where the carrier discriminator reads.** An M-th-power
    discriminator updating at rate `F` can only observe `|Δf| < F/(2M)`, so its
    tap point *is* its pull-in range. Symbol-rate-only carrier tracking does not
    get far without acquisition aid, so both receivers now take a construction
    parameter selecting the trade:

    | `nda_tap`            | Reads                            | Update rate | Max acquired `Δf`     | Needs symbol timing? |
    | -------------------- | -------------------------------- | ----------- | --------------------- | -------------------- |
    | `"strobe"` (default) | the on-time strobe               | `Rs`        | `0.050·Rs` (`0.010`)  | **yes**              |
    | `"mf_out"`           | every terminal output            | `m_out·Rs`  | `0.033·Rs` (`0.015`)  | no                   |
    | `"lo_arm"`           | post-LO, free-running boxcar arm | LO rate     | **`0.090·Rs`** (same) | no                   |

    (Measured unaided, QPSK at `sps=8`, each at its own best `bn_carrier`, at the
    default `m_out=8` — `m_out=4` in parentheses, since `m_out` is on this axis
    too.) `lo_arm` is the one row `m_out` cannot move, which is the check on the
    mechanism rather than a curiosity: it taps *ahead* of the cascade, so the
    terminal rate is not in its path. At `m_out=4` it is **9× the strobe**, near
    the `sps` factor theory predicts; at `m_out=8` the strobe closes most of that
    gap *without its update rate changing at all*, because a sharper matched
    filter is a quieter discriminator and that is what raises the largest stable
    `bn_carrier` (0.01 → 0.05).

    The second axis matters as much as the range: `strobe` is the only tap that
    depends on symbol timing, so `mf_out` and `lo_arm` restore the property the
    NDA path exists for — acquiring with no data *and no symbol timing*.

    `bn_carrier` keeps its symbol-rate meaning at every tap; the tap widens what
    the discriminator can see and the stability margin, which is what then lets
    you raise `bn_carrier`. At a *fixed* `bn_carrier` all three taps measure the
    same `0.01·Rs`. Fixed at construction — nothing switches underneath you.

    **`lo_arm` does not work at 8PSK**: its arm is a short lowpass rather than
    the pulse matched filter, and the raw M-th-power gain over an arm goes as
    `Σ g_k^M`, which collapses at 8th power (measured SER 0.85, lock 0.081
    against the 0.41 ceiling). BPSK and QPSK decode cleanly on every tap.

    Also worth knowing at any tap: `Δf = k·F/M` is a **stable false lock** — the
    M-fold ambiguity is a frequency ambiguity as well as a phase one. The loop
    sits still reporting a healthy lock on a stationary constellation, so
    neither EVM nor blind M2M4 can flag it. Resolving it needs an external
    frequency reference or a sync word. Beyond any tap's range, pass a coarse
    estimate as `init_norm_freq`.

- **`track.MpskReceiverR` — the real-input M-PSK receiver.** `MpskReceiver` for a
    real IF: `steps()` and `bits()` take `float32` samples of a real bandpass
    signal instead of complex baseband, and a `MatchedDdcr` front end tunes and
    converts internally. Every loop, discriminator, handover rule and demapper is
    the *same implementation* shared with the complex type — only the front end
    and the two rate conversions its halfband forces differ, so a fix to receiver
    behaviour lands on both by construction.

    It is a separate class rather than a constructor flavor of `MpskReceiver` for
    the usual reason (a difference in constructor is a flavor, a difference in
    method signature is a separate type — and `steps()` takes a different dtype).
    Its one extra constraint is **`sps > 2 * m_out`**: the cascade behind the R2C
    halfband runs at twice the overall rate. `init_norm_freq` and `norm_freq` are
    both in cycles/sample at the real input rate; the halfband's baked-in `fs/4`
    shift and the intermediate-rate conversion are handled internally.

- **A matched *flavor* for both down-converters, and a second control port.**
    `MatchedDDC` and `MatchedDdcr` are the same objects as `DDC` and `Ddcr`,
    built by a different C constructor: the cascade's terminal stage carries a
    pulse-shaped matched-filter bank instead of the Kaiser anti-alias one, so
    the chain mixes, decimates *and* matched-filters in the dot products it
    was already doing.

    ```python
    rx = MatchedDDC(norm_freq=-0.09375, rate=2 / 16, pulse="rrc", span=8)
    symbols = rx.execute(x)          # 2 samples/symbol, matched-filtered
    ```

    `execute_ctrl(x, rate_ctrl, freq_ctrl)` and the per-input
    `execute_ctrl_push(x, rate_ctrl, freq_ctrl)` (both on all four classes)
    steer two accumulators that are duals of each other:

    - **`freq_ctrl` → the LO's phase accumulator** (cycles/sample at the input
        rate; the intermediate rate fs_in/2 for the real chain). The LO sits at
        the input rate, which is where predetection de-rotation belongs — the
        carrier is wiped off before any filter narrows the band around it.
    - **`rate_ctrl` → the terminal stage's accumulator** (the timing port
        `RateConverter` already exposed).

    Neither deviation is persisted — `norm_freq` and `rate` never move — so a
    tracking loop supplies its full filter output every call and the object
    holds no loop state. That is what makes carrier recovery *snap in*: it is
    the same `loop_filter` a timing loop uses, on the other port. Measured, a
    first-order loop on `freq_ctrl` parks on a 0.01 cycles/sample mistune to
    within 1e-9 and leaves the centre frequency untouched — with a small gain,
    because the loop closes *around* the matched filter and inherits its group
    delay as dead time.

    Also new on all four: a `clipped` flag forwarded from the cascade (a CIC
    bounds its input to ±1.0 and clips silently, costing ~25 dB of EVM that no
    downstream metric attributes to the front end), and a `narrow_pulse` flag
    for the one configuration that builds a degenerate matched filter —
    `pulse="iandd"` with fewer than four output samples per symbol, where the
    one-symbol-wide rectangle's matched filter is a 2–3 tap sum (measured on
    the timing loop this feeds: lock statistic −0.34 at two samples per symbol
    against +0.95 at four). Constructing one also raises a `UserWarning`, so
    the same diagnostic is available to push and to pull. CIC droop
    compensation is unconditional on the matched flavors: the fold is six taps
    per arm and worth 28 dB, so no caller can turn it off.

    End to end, RRC-BPSK at 16 samples per symbol on a carrier, decimated to
    two samples per symbol: **−45 dB EVM** on the complex chain (its `CIC(8)`
    alias floor) and **−60 dB** on the real one (whose cascade sees twice the
    rate and plans halfbands instead).

- **`track.RateSync` — matched filtering and symbol timing in one dot
    product.** Where `SymbolSync` runs a matched FIR and then a Farrow
    interpolator steered by a timing NCO, `RateSync` owns a `RateConverter`
    whose **terminal stage carries the pulse**: the cascade's last dot product
    *is* the matched filter, and the polyphase arm it selects *is* the
    fractional timing delay. It builds no filters of its own.

    Two consequences, and they are the point:

    - **`sps` is a `double`** — 4, 17.33389, an irrational ratio, or a slowly
        drifting clock all work by construction, because the terminal stage's
        accumulator is a double and the loop only steers the strobe. That is
        the real case whenever the ADC clock free-runs against the symbol
        clock.
    - **A high input rate is nearly free.** The cascade's HB/CIC stages do the
        bulk decimation at no multiplies, so the bank is sized by the
        post-decimation rate — the same size at 4 samples per symbol as at 256,
        where filtering at the input rate would need thousands of taps per arm.

    Measured on RRC-BPSK, noiseless, from every initial timing offset:
    **8/8 lock at all three planned cascade shapes**, EVM −40.1 dB (sps=4,
    halfband), −37.4 dB (sps=17.333, `CIC(8)` + fractional terminal) and
    −37.3 dB (sps=64, `CIC(32)`), tracking a ±1000 ppm clock offset to within
    0.01 samples/symbol. Gardner or DTTL detector, telemetry, a verify-counted
    lock detector and full state serialization, composing the cascade's and the
    loop filter's child blobs.

    Two design notes worth reading before use. Judge lock by `lock_stat` /
    `locked`, not by an error-vector magnitude — a single cycle slip during
    acquisition drags a windowed EVM by 20 dB while the eye stays wide open.
    And use `m >= 4` with `pulse="iandd"`: the rectangle is one symbol wide, so
    at `m = 2` its matched filter is a two-tap sum and the eye barely opens
    (measured `lock_stat` −0.34 at m=2 against +0.95 at m=4 on the same NRZ
    stream); the RRC spans many symbols and is unaffected.

    `SymbolSync` is unchanged and remains the answer when the matched filter is
    not one this family builds, or when the front end is already at a small
    integer `sps`. Worked example: the
    [Arbitrary-Rate Symbol Recovery](docs/gallery/ratesync.md) gallery page.

- **`CIC.clipped` / `RateConverter.clipped` — the input bound stops being a
    silent one.** `cic_core` quantizes at its boundary, so its input amplitude
    is bounded to `|Re|, |Im| <= 1.0` and anything past that is clipped before
    any filtering. Nothing in the sample stream shows it: no exception, no NaN,
    just a plausible-looking output that is quietly wrong. This cost real time
    during the matched-filter work — an overdriven waveform measured −25 dB EVM
    against −50 dB in range, and the investigation went through droop geometry,
    alias arithmetic and compensator length before reaching the amplitude.

    Both blocks now carry a sticky `clipped` flag, cleared by `reset()`, in the
    same convention the quantizing `cvt` converters (`adc`, `f32_to_uq15`, …)
    already used. **It is free**: the four boundary comparisons run on every
    sample regardless, so recording that one fired costs a register OR — the
    flag reports something the samples cannot. It is also serialized, so a
    resumed stream does not forget it clipped (`CIC_STATE_VERSION` → 2).

    `RateConverter` surfaces it because that is where the trap actually is: the
    planner selects a CIC for **any decimation by 8 or more**, so
    `RateConverter(rate=0.1)` inherits the bound without the word "CIC"
    appearing anywhere in the call. `stages` tells you whether the bound
    applies — a plan naming `CIC(...)` is not scale-free, every other plan is —
    and `clipped` tells you whether you hit it.

- **`RateConverter(pulse=…)` — the cascade IS the matched filter.** The
    terminal polyphase stage can now carry a pulse-shaped bank (`"rrc"` /
    `"iandd"`) instead of the default Kaiser anti-alias one, so a single dot
    product converts the rate *and* matched-filters, and that stage's arm is
    the fractional timing delay `execute_ctrl` steers. Selecting a pulse
    changes three things:

    - **The terminal fractional stage always exists.** The planner used to drop
        it whenever the integer stages already landed the rate, so `rate=2/64`
        planned a bare `CIC(32)` — correct arithmetic, nothing steerable at the
        end. It now plans `['CIC(32)', 'Resampler(1,rrc)']`, because that stage
        is simultaneously the matched filter and the timing element.
    - **The bank is sized by the post-decimation rate, not the input rate.**
        Matched-filtering at the input rate costs taps proportional to the input
        samples per symbol — 4225 taps/arm at 256 samples/symbol, ~35 MB of
        bank. After the integer stages have done the bulk decimation it is ~34
        taps/arm, **identical at 4 and at 256 samples/symbol** (~0.14 MB), with
        the CIC doing the decimation at no multiplies. Visible as the new
        `bank_shape` property.
    - **CIC droop folds into the bank** rather than appending a comp FIR.
        `ciccompmf`'s taps run at the decimated rate, which *is* the terminal
        bank's tap grid, so the fold is a per-arm convolution: exact, six extra
        taps per arm, no extra stage and no extra pass over the data. Measured
        against the reference architecture (plain cascade + separate comp FIR +
        dense matched filter) it agrees within 0.6 dB — and on a CIC cascade it
        is worth **28 dB of EVM** (−50 dB with `compensate=1`, −22 dB without),
        so on this path compensation is not a refinement.

    Measured on RRC-BPSK at a deliberately awkward 17.333 samples/symbol
    (`CIC(8)` + `Resampler(0.923,rrc)`), noiseless, best-case timing phase:
    **−50 dB EVM**; a halfband cascade reaches −60 dB. Keep the input inside
    `cic_core`'s Q15 full scale — a CIC quantizes at its boundary, so the same
    signal at peak 1.29 measures −25 dB for reasons unrelated to the filter.

- **`RateConverter.execute_ctrl_push(x, ctrl)`** — the per-input streaming
    form of `execute_ctrl`, and the only form a closed loop can use: a block
    call must know its whole `ctrl` history up front, while a timing loop
    computes each correction *from* the outputs already emitted. Bit-exact
    against the block form at constant `ctrl`, so open- and closed-loop paths
    are the same filter.

- **`RateConverter.bank_shape`** — `(num_phases, num_taps)` of the terminal
    polyphase stage, or `None` for an integer-only cascade. `stages` and
    `bank_shape` now both appear in the type stubs; `stages` previously did not.

### Changed

- **`MpskReceiver` is rebuilt on the matched-DDC cascade. Its outputs are no
    longer bit-identical, and `bn_carrier` changed units.** Read both of those
    before upgrading — the second one is silent.

    The receiver now owns no filter, no NCO and no interpolator of its own. It is
    a `MatchedDDC` with two loops closed around its two control ports: the
    terminal polyphase stage's **bank is the matched filter** and the **arm it
    selects is the fractional symbol-timing delay**, a carrier loop steers the LO
    (`freq_ctrl`), and `RateSync`'s own timing loop — reused, not copied — steers
    the terminal accumulator (`rate_ctrl`). Four pieces went away as a result: the
    per-sample integer-NCO wipe-off, the separate boxcar NDA arm and its AGC, the
    dense matched-filter FIR, and the `SymbolSync` Gardner+Farrow loop. All four
    remain first-class objects in their own right; this receiver just no longer
    needs them.

    - **Outputs move at the float level.** A polyphase bank is not a dense FIR and
        a bank arm is not a Farrow. Detection performance is unchanged — the fused
        matched filter measures on the Es/N0 bound — but exact-output pins are not.
    - **`bn_carrier` is now normalised to the symbol rate**, like `bn_timing`,
        rather than to the input sample rate. Old code keeps running and simply
        gets a much wider carrier loop than it asks for: at the old default
        `sps = 8`, the same number is an 8× wider loop. Values around
        `0.005` are where `0.02`–`0.03` used to be.
    - **`n` is now `m_out`, and means something different.** `n` sized the NDA arm
        (window = `sps/n`); there is no arm. `m_out` is the terminal stage's
        **outputs per symbol** (even, 2–8, default 8), setting the Gardner
        strobe/gate geometry. The default is 8 because that is where an I&D
        matched filter reaches the coherent bound: the rectangle is one symbol
        wide, so its matched filter is an `m_out`-tap sum spanning it, and a
        smaller `m_out` samples the same integral more coarsely. Measured on QPSK
        at `sps = 8` against `EVM_dB = -(Es/N0)_dB` — at 18 dB Es/N0, `m_out = 8`
        lands 0.41 dB off the bound where `m_out = 4` loses 3.11 dB; at 14 dB it
        is 0.25 dB against 1.71 dB, the gap widening as noise stops hiding it.
        Never pair 2 with `pulse="iandd"`: the matched filter degenerates to a
        two-tap sum (measured lock statistic −0.34 at 2 against +0.95 at 4) and
        acquisition itself fails about half the time.
    - **`MpskReceiverR`'s `sps` default is 32.0, not 16.0.** Forced by the above:
        that type requires `sps > 2 * m_out`, so an `m_out` of 8 cannot coexist
        with a 16.0 default — `MpskReceiverR()` would not construct at all. The
        complex twin's `sps` default is unchanged at 8.0 (`sps >= m_out` there,
        and a terminal ratio of 1.0 measures 0.42 dB off the bound).

    What the rebuild buys is that **`sps` is a `double` and the front end plans
    itself**. At `sps = 8` the plan is a halfband or two plus a terminal stage; at
    `sps = 256` it is a CIC in front of the *same* terminal stage, so the matched
    filter costs ~34 taps/arm at both ends of a 64× span of input rates, against
    the ~4225 taps/arm a single-stage design would need. An irrational `sps` — a
    free-running ADC clock against the symbol clock — is no harder than an integer
    one.

    Two acquisition faults surfaced while documenting this and are **fixed**
    (see the `nda_tap` entry below and gh#536). Briefly: restricting the
    discriminator to the on-time strobe had coupled carrier acquisition to
    symbol timing, so the loop integrated an invalid discriminator output
    through the ~130-symbol timing transient and failed to acquire for about a
    third of data seeds; and the first-strobe AGC seed could latch a
    pathological gain, reporting `lock` = 4.9e-19 on a receiver decoding every
    bit correctly. The steer, the AGC seed and the handover now wait on the
    timing loop's own lock detector.

- **The carrier lock statistic is normalised: it reads ~1.0 at lock for every
    M.** It used to carry a per-M `lock_scale` of 1 / 0.619 / 0.412, which made
    the statistic's ceiling M-dependent while `lock_thresh` stayed a single
    absolute number. Measured settled values were 1.00 / 0.63 / 0.43 for
    BPSK / QPSK / 8PSK against a default threshold of **0.5** — so:

    - **8PSK could never declare carrier lock.** Its ceiling was below the
        default threshold, so `car.locked` stayed 0 on a receiver decoding
        perfectly and `acq_to_track` could never hand over. Worse, the statistic
        overshoots its own ceiling during the acquisition transient, so at low
        Es/N0 8PSK *did* declare — the flag was anti-correlated with lock.
    - **QPSK had 0.13 of margin** and declared intermittently under noise.
    - Every call site that needed a meaningful threshold multiplied the scale
        back in by hand: `carrier_nda_pullin.c` computed
        `get_lock(c) / c->lock_scale /* normalize to ~1 */` and three C tests
        compared against `0.3 * c->lock_scale`. Those workarounds are gone.

    `carrier_nda_lock_scale()` is removed and `carrier_nda_disc()` loses its
    `scale` parameter — the lock signal is now `Re(z^M)` unscaled, which reads
    ~1.0 at lock at every order, so one threshold means one thing everywhere.
    The phase-error scaling (1, ½, ¼) is untouched: that one genuinely does
    normalise the discriminator gain so a single `bn` behaves identically across
    M, and it was never the problem.

- **The carrier lock statistic is now `Re((z/|z|)^M)` — the M-th power of a
    *limited* sample — so `lock_thresh` maps to a false-alarm probability, at
    every M.** Normalising the value at lock (above) was necessary but not
    sufficient: the *noise* distribution still depended on M, because `|z|^M` on
    Gaussian noise is unbounded and grows fast with M. Limiting fixes it, because
    under H0 the phase is uniform and `Var[Re(e^{jMθ})] = ½` for every M.

    The threshold chain is now derived rather than picked:
    `α = det_ema_alpha(0, 15.9) = 0.05` (`N_eff = 39` looks) →
    `σ_H0 = sqrt(½·α/(2−α)) = 0.1132` analytically, **0.1132 measured** → the
    unchanged default of `0.5` is 4.42 σ, a per-look Pfa of **5.0e-6**. Measured
    on noise only, 200 trials × 4000 symbols: σ 0.1133 / 0.1071 / 0.1138 for
    BPSK / QPSK / 8PSK, 0/200 over threshold at every order; and end to end with
    `acq_to_track=1`, 100 runs × 20 000 symbols, **0/100 false declares** at
    every order.

    This also fixed two real behaviours, not just the number. At `m_out = 4`,
    `mf_out`/8PSK decodes at chance (a `Σ g_k^M` gain collapse) and used to
    report lock **+0.94** while doing it — a false lock; it now reports
    **−0.069**, correctly not locked. And `mf_out` + `acq_to_track` at QPSK
    recovered from 2/5 decodes (SER 0.295) to **5/5** (SER 0.0000), because the
    handover is no longer fired by a meaningless statistic.

    Detectability `d' = (μ_H1 − μ_H0)/σ_H0` at Es/N0 = 10 / 20 dB, before →
    after: BPSK 5.70/6.21 → 7.95/8.75, QPSK 1.50/1.78 → 5.81/8.47, 8PSK
    0.02/0.04 → 1.76/7.52. Limiting *costs* H1 (it discards the `|z|^M` boost at
    low SNR) and wins at every M and Es/N0 anyway. Before it, only BPSK ever
    cleared a 1e-3 Pfa, so for M ≥ 4 there was no Pfa-derived threshold to be
    had.

    **What this costs you:** `rx.lock` is amplitude-blind and bounded in ±1, so
    a reading above 1 is no longer possible and the "lock statistic far above its
    ceiling means an AGC gain fault" diagnostic is gone. Only the lock path is
    limited — `phase_error` keeps its raw `|z|^M` weighting, which is the correct
    matched weighting on a pulse-shaped signal.

- **The `±1.0` input bound is now documented where callers meet it** rather
    than only as a cast-safety note in `QUANTIZATION.md` §2.4. `cic_core.h`,
    `RateConverter_core.h`, both classes' Python docstrings and the type stubs
    now state it plainly, and §8's headroom budget no longer reads as blanket
    reassurance: "no overflow occurs" describes the integer pipeline, not a
    licence to feed the block any amplitude. Pinned by tests in both harnesses
    so the docs and the code cannot drift apart.

- **`Ddcr`'s "roughly 2x cheaper than DDC" claim is corrected — it was never
    measured, and it is wrong.** Against `DDC` fed the same stream promoted to
    complex, the front end alone measures 1.04x-1.40x end to end at total rates
    0.25/0.125/0.0625, and 0.74x-1.13x once the real→complex promote is charged
    to `DDC`, with the ratio wandering by block size the way a memory-bound
    measurement does. The multiply-free ±1/0 coefficients are real, but they buy
    the *fs/4 shift*, not the halfband's own FIR — which does multiply
    (one output component is an FIR, the other a single scaled delay tap).

    Where the half rate does pay is a whole receiver, because it halves the
    sample rate ahead of the polyphase matched filter: `MpskReceiverR` against
    `MpskReceiver` on the same stream measures 1.13x at `sps=20`/`m_out=8`,
    1.50x at 32/8, 1.69x at 64/8, 1.50x at 20/4 and 1.74x at 40/4. It rises
    toward 2x as more of the total cost sits ahead of the terminal stage, and
    cannot reach it: both paths fire the same `m_out` terminal dot products per
    symbol, and those dominate at low `sps`. Choose `Ddcr` because the input IS
    real, not for a factor of two.

- `RateConverter.execute()` on a **matched** cascade routes through the unified
    accumulator (`execute_ctrl` at zero deviation) rather than `resamp`'s
    transposed-form decimating path. The two are different algorithms that index
    polyphase arms in opposite directions, and a pulse-shaped bank is laid out
    for the accumulator; mixing them yields a one-output-period sawtooth in the
    effective sampling instant. Plain (`pulse="none"`) cascades are unchanged.

### Fixed

- **The M = 8 lock signal was missing a factor of 4, so it was not `Re(z^8)`.**
    The recursion carries `qe = ½·Im(z^4)` (that half being the deliberate
    `{1, ½, ¼}` phase-error scaling), so `Re(z^8) = ql² − (2·qe)²` — but the lock
    signal read `ql² − qe²`, i.e. `Re(z^4)² − ¼·Im(z^4)²`. The two are exactly
    +1.0000 at `φ = 0` and differ everywhere else, so every *locked* measurement
    agreed and the error lived entirely in the noise-only tail — the one region
    that sets a detector's false-alarm rate. `Re(z^8)` is zero-mean on circular
    noise; the shortfall is not, leaving a positive bias of `¾·E[Im(z^4)²]`
    (measured mean **+8.94** where it should be **−0.11**, on unit-power complex
    Gaussian noise).

    The design note had recorded this as an acceptable trade — *"making it exact
    would require doubling the carried imaginary term, which would break the
    constant-gain property"* — which is false: the 4 belongs in the lock
    expression, where it cannot affect the phase-detector gain at all.
    `carrier_nda_scurve.c` had encoded the same conclusion as `if (m <= 4)`
    around its `|lk − Re(z^M)| < 1e-6` assertion, excusing the validator from
    the only order that was broken. That guard is gone, so the identity is now
    pinned at all three orders (residuals 1.16e-07 / 2.59e-07 / 4.89e-07).

    Found by randomising M in a Monte-Carlo characterisation. A fixed
    QPSK grid does not surface it, because QPSK mostly works.

    **Thresholds you have tuned by hand need rescaling**, since the same number
    is now a different fraction of the achievable ceiling: divide an existing
    QPSK threshold by 0.619, an 8PSK one by 0.412. The default 0.5 is unchanged
    and now means "half of achievable" at every M instead of 50% / 81% / 121%.

- **The carrier's strobe tap no longer waits for timing lock.** An earlier
    revision on this branch gated the carrier steer, the arm AGC seed and the
    two-way handover on the timing loop's own `lockdet`, because a pre-lock
    strobe is an arbitrary phase of the pulse. Measured, the gate does not buy
    what it appeared to: across a 24-cell sweep (sps × `m_out` × `bn_carrier`)
    removing it changes **exactly one cell** — `sps=8, m_out=4,   bn_carrier=0.04`, which goes to 5/24 — and `m_out` now defaults to 8, so
    that cell is off the default path. What it mainly bought was
    *measurability*: with the steer frozen until timing declares, the carrier
    transient starts at a known instant, which is convenient for instrumenting
    an acquisition and is not a property of a working receiver.

    The structural objection is the deciding one. A tap that needs timing it
    cannot wait for is a reason to choose a **different tap** — `nda_tap`
    exists precisely for that, and `mf_out`/`lo_arm` are timing-independent by
    construction. Resolving it inside the receiver hid a real trade behind a
    coupling the caller could neither see nor override, and made the default
    receiver's cold-start behaviour depend on a second loop's lock detector.
    `mpsk_rx_disc()`'s `may_act` parameter is gone with it (every call site
    passed the same value once the gate went). If cold acquisition fails at
    `m_out=4`, reach for `nda_tap="mf_out"` or `"lo_arm"`.

## [0.37.3] — 2026-07-24

### Added

- **`background=True` — fold a static source population into ONE `Plan` cache
    slot.** `Plan` caches every source separately so any one of them stays
    overridable, at the cost of one full-length buffer each. For a scene
    dominated by emitters that never move — a crowded uplink, a co-channel user
    population, an interference field — that buys an override nobody uses: 400
    users at 122.88 MHz over 10 ms is roughly **4 GB** of cache for a scene in
    which one interferer actually varies. Marking those sources
    `background=True` makes `prepare()` fold a contiguous **leading run** of
    them into a single pre-summed entry, each member pre-weighted by its own
    `10**(level/20)` so the composite carries `base_gain` 1.0.

    `render()`/`at()` need no special case — the composite is just a cache slot
    that happens to be pre-summed — so the whole field takes **one** entry in
    `gains`/`phases`/`enable` and counts as one in `n_sources()`. Scaling,
    rotating or dropping the entire background is now a single number, a control
    that did not exist before. Measured on the new gallery example: **67×
    smaller** cache at 200 users, **~5× faster** to re-render (3 buffers touched
    instead of 102), and a −10 dB trim scaling the field by 9.98 dB while the
    wanted signal moves 0.06 dB.

    The background sources must be a **prefix**, and that is load-bearing rather
    than stylistic: `compose()` sums into a running accumulator in spec order
    while the composite sums from zero, so the two agree to the last bit only
    when nothing precedes the folded block. A background source behind a
    foreground one raises `ValueError` instead of silently costing the
    bit-exactness contract. Because the sum must stay ordered, the parallelism
    comes from building a *group* of sources concurrently and then accumulating
    that group in spec order, with the ordered combine split by **sample** —
    bit-identical to a serial fold, and still using every core. The group is
    capped by a memory budget so a 400-source background never materialises 400
    live buffers. A *bundled* segment (a lone source carrying its own real SNR)
    folds nothing: its baked-in noise amplitude rides on the `base_gain` the
    fold would overwrite.

    One semantic worth knowing: for an ordinary slot `gains[k]` *replaces* the
    level (absolute dBFS), whereas for the composite it is a **trim** — members
    keep their relative levels and the whole mix scales.

### Fixed

- **A bundled `Plan` segment now renders bit-exactly at a non-zero level.** The
    composer scales a lone real-SNR source's signal *and* its baked-in noise
    with a single multiply, `g*(sig+noise)`, while `materialize()` scaled the
    cached signal and the reconstructed noise separately as `g*sig + g*noise` —
    identical in exact arithmetic, about an ULP apart in float. At `level=0`
    (gain exactly 1.0) both forms agree trivially, which is why every existing
    bundled test passed; at −3 dB it mismatched 696 of 1024 samples. The ON
    region is now summed at unit gain and scaled once, mirroring the composer.

- **`Plan` seeds its reconstructed noise from the anchor, not `sources[0]`.** A
    segment carries two different default seeds: the ranged off/delay *draws*
    key off `sources[0].seed`, while the auto-appended noise source is seeded
    from the **anchor** — whichever source carries the SNR. `materialize()`
    passed the former where it needed the latter. Those numbers are equal only
    when the anchor happens to be `sources[0]`, the idiomatic ordering and the
    one every test used, so this never surfaced; with the SNR source anywhere
    else the Plan reconstructed a completely different noise realization (not a
    rounding difference — full-scale, every sample). A `seed` override still
    moves both, so `at()`/`monte_carlo()` are unaffected.

### Changed

- **`CMAKE_BUILD_TYPE` now defaults to `Release`.** An unset build type is not
    "some sensible default" — CMake contributes no `-O` flag at all, so the bare
    `cmake -B build` documented in the README and the C-API docs built the whole
    DSP library at `-O0`: scalar, stack-spilled, unvectorised. `make` and the
    release workflow were always explicit, so nothing published was ever
    affected; the gap was the hand-typed configure, which is also what people
    benchmark against. Multi-config generators are untouched, and
    `-DCMAKE_BUILD_TYPE=Debug` is still honoured.

- **`Plan`'s `accumulate()` takes `restrict` pointers.** Without it the compiler
    cannot rule out overlap between the render output and a cache slot and
    settles for a half-width SLP vectorisation (one complex sample per
    iteration); with it, a full-width loop. No caller aliases them and the
    operation is element-wise, so results are bit-identical.

### Docs

- **New gallery page: "One Cache Slot for a Whole Background Field"**
    (`docs/gallery/plan-background.md`), built from the self-validating
    `plan_background_demo.py` — cache footprint against population size, and a
    PSD showing one gain moving the entire field while the wanted signal and the
    interferer stay put.

- **Corrected the `Plan` scope notes.** The gallery page still claimed a single
    non-ranged segment and that a lone bundled noisy source raises `ValueError`;
    both have been supported for some time. It now states what `prepare()`
    actually refuses — a ranged source field, a ranged ON-time, mixed sample
    rates, an unbounded timeline — and why.

- **`Plan` is serializable** — corrected the stale "not serializable" claim in
    the wfmgen guide: transport the recipe (the spec JSON), not the rendered
    cache; save/restore is a checkpoint mechanism.

______________________________________________________________________

## [0.37.2] — 2026-07-24

### Added

- **Streaming control port on `Resampler` and `RateConverter`.** A new
    `resamp_execute_ctrl_push` (C) drives the resampler one input at a time with
    an instantaneous rate deviation — the per-output-feedback form of
    `execute_ctrl` that a closed timing/rate-tracking loop needs (the block form
    takes a precomputed `ctrl[]` and cannot depend on outputs already emitted).
    Feeding a stream through it reproduces block `execute_ctrl` bit-for-bit.
    Built on it, **`RateConverter.execute_ctrl(x, ctrl)`** (C + Python) forwards
    a scalar rate deviation to the cascade's **terminal `Resampler` stage** while
    the fixed integer HB/CIC stages run unchanged — so a loop can decimate a high
    input rate cheaply and then arbitrary-rate + strobe-align in the last stage
    (the wideband/DSSS receiver front-end). No-op fall-through when the cascade
    has no fractional stage to steer. Foundation for the polyphase RX
    matched-filter + timing-recovery block.

### Changed

- **Polyphase RRC pulse shaping — ~`sps`× faster waveform synthesis.** The
    synth's RRC pulse shaper (`pn`/`bpsk`/`qpsk`/`bits`/`symbols`/`dsss` sources
    with `pulse="rrc"`) no longer builds a symbol-rate impulse train and runs a
    dense FIR over it — wasting `(sps-1)/sps` of every tap-multiply on structural
    zeros. For a power-of-two `sps` it now decomposes the RRC into a polyphase
    bank and shapes it as an interpolate-by-`sps` view over the existing `resamp`
    engine (`Resampler(rate=sps, bank=…)`), computing the identical convolution
    from only the nonzero contributions. Measured ~4× (`sps=4`), ~8× (`sps=8`),
    ~16× (`sps=16`) on the shaping stage — the win scales with `sps` and directly
    cuts `Composer`/`Plan.prepare()` build time. Output is aligned to the dense
    path to float precision (a one-time `sps`-sample latency prime), so shaped
    waveforms are unchanged at the sample level; `step()`==`steps()` stays
    bit-exact and mid-stream state serialization resumes bit-for-bit. Odd `sps`
    keeps the dense FIR (the polyphase branch select needs a power-of-two phase
    count). No API change.
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

- **`docs/dev/contributing/adding-a-module.md`** — rewritten to match the current
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
- **`docs/dev/contributing/error-convention.md`** — the entire error-code table was
    wrong (values didn't match `clib_common.h` at all, one code missing
    entirely) and described a two-header split that no longer exists.
- **`docs/dev/contributing/module-layout.md`** — added the `<module>_ext_<component>.c`
    hand-owned fragment pattern and the state-serialization requirement,
    both load-bearing per this repo's conventions but absent from the
    page entirely.
- **`docs/dev/contributing/benchmarking.md`**, **`docs/dev/release.md`**,
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
    `hbdecim_cf32_*` functions; `docs/dev/contributing/error-convention.md`'s `awgn()`
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
    and a new `docs/dev/contributing/docs-conventions.md` contributor guide explaining
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
    `dp_tlm_sink_*` helper (`stream/tlm_sink.h`, in the optional
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
- **`dp_tlm` telemetry taps** (`native/inc/dp_tlm/dp_tlm_core.h`) — a
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
    `docs/dev/contributing/doc-examples.md`. The gate resolves `--8<--` snippet includes, so a
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
[0.37.2]: https://github.com/doppler-dsp/doppler/compare/v0.37.1...v0.37.2
[0.37.3]: https://github.com/doppler-dsp/doppler/compare/v0.37.2...v0.37.3
[0.38.0]: https://github.com/doppler-dsp/doppler/compare/v0.37.3...v0.38.0
[0.38.1]: https://github.com/doppler-dsp/doppler/compare/v0.38.0...v0.38.1
[0.39.0]: https://github.com/doppler-dsp/doppler/compare/v0.38.1...v0.39.0
[0.4.0]: https://github.com/doppler-dsp/doppler/compare/v0.3.7...v0.4.0
[0.4.1]: https://github.com/doppler-dsp/doppler/compare/v0.4.0...v0.4.1
[0.40.0]: https://github.com/doppler-dsp/doppler/compare/v0.39.0...v0.40.0
[0.41.0]: https://github.com/doppler-dsp/doppler/compare/v0.40.0...v0.41.0
[0.42.0]: https://github.com/doppler-dsp/doppler/compare/v0.41.0...v0.42.0
[0.43.0]: https://github.com/doppler-dsp/doppler/compare/v0.42.0...v0.43.0
[0.43.1]: https://github.com/doppler-dsp/doppler/compare/v0.43.0...v0.43.1
[0.43.2]: https://github.com/doppler-dsp/doppler/compare/v0.43.1...v0.43.2
[0.44.0]: https://github.com/doppler-dsp/doppler/compare/v0.43.2...v0.44.0
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
[unreleased]: https://github.com/doppler-dsp/doppler/compare/v0.44.0...HEAD
