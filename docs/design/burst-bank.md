# BurstBank — the coarse-Doppler bank as one C object

*Phase 1 of [adding an algorithm](../dev/contributing/adding-algorithms.md).
Written 2026-09-01, after the Python bank (`doppler.dsss.orchestrator`)
learned to capture bursts (#1174, #1180) and after the engine it sits on was
found to leave 1/D of its Doppler prior unsearched (#1183). Reviewed, not
gated. Nothing below is implemented.*

______________________________________________________________________

## 1. Why — the bank exists, in the wrong language

`doppler.dsss.orchestrator.Acquirer` tiles `K` coarse-Doppler channels, each
`DDC → BurstCapture`, across `±doppler_uncertainty`, fans them over a thread
pool and collapses the same target seen in two adjacent channels to one
detection. It is the object a wide-Doppler burst link actually calls, it
ships to pods as `(descriptor, state, block)`, and since #1180 it returns
the burst windows as well as the detections.

It is Python. Three things follow, and each is a reason on its own:

- **Its claim rule is a second implementation of a claim rule.** `_dedup`
    decides *are these two detections one target* — `≤ res` in Doppler,
    `≤ 1` in code phase, keep the stronger — beside the capture's own
    temporal CLAIM in C. They answer different questions (cross-channel
    identity versus one-preamble identity), and that is exactly why they
    belong in one place where they can be read together: the repository's
    rule is that orchestration logic lives in C once, and a rule that lives
    in Python is a rule no C caller has.
- **The standard owes a C example and C pins, and a Python object cannot
    produce either.** `adding-algorithms.md` is explicit that both examples
    are owed and that they fail differently — the C one is where a caller
    sees the lifecycle it must manage. There is no lifecycle to show for an
    object that only exists inside the interpreter.
- **The pod hand-off is a blob of blobs framed in Python.** `_CC_MAGIC` /
    `_AQ_MAGIC` are envelopes `struct.pack`ed around the children's
    self-validating blobs. They work, and they are the only state envelope in
    the repository that `dp_state.h` does not own.

The Python bank is therefore this design's **prototype**, already de-risked
in the sense the standard means: the layout rule, the dedup, the per-channel
capture and the pod hand-off all have tests. What is not known is listed in
§6, and it is what the characterization measures.

## 2. Use cases

| caller                       | supplies                                  | wants back                                                            |
| ---------------------------- | ----------------------------------------- | --------------------------------------------------------------------- |
| a wide-Doppler burst link    | a stream, `±U` of Doppler uncertainty     | every burst, aligned, with its absolute Doppler — from one pass       |
| a pod in an autoscaled fleet | a descriptor, a blob, a block             | to resume the whole bank bit-for-bit, rings and all                   |
| a pod holding ONE channel    | the bank's descriptor and a channel index | that channel alone, its ring by centre, its blob restorable elsewhere |
| a recorder                   | a stream and a burst length               | windows per channel with the event naming where and at what Doppler   |
| a C consumer                 | the same                                  | to borrow a window without a second copy, as `DsssBurstReceiver` does |

The third row is the one the Python bank cannot serve today and the one the
HPA story is actually about: sharding channels across pods, not only
replicating whole banks. It is what fixes the ring naming (§4.3).

## 3. Design goals

1. **One object, one pass.** `push(x)` mixes, decimates, searches and
    captures on every channel and returns the bank-absolute detections; the
    windows and events are read per channel. Both faces come from one
    acquisition engine per channel — `BurstCapture.detections()` is what
    makes that possible, and it was built for this.
1. **The layout rule is derived, and it is now correct.** Channels sit
    `2·span` apart because the fast-time integrate-and-dump nulls a target a
    full `2·span` away, and a target midway is seen by both neighbours at
    `sinc(0.5)` = −3.9 dB. That claim was false on the shipped engine —
    the outermost native bin was never searched at even coherent depth, so a
    boundary target was seen by *neither* (#1179 / #1183) — and is true on
    the fixed one: a channel's statistic at exactly one span reads 13
    against 31 at DC. The bank does not re-derive the span; it reads the
    child's `doppler_span_hz` and `doppler_res_hz`.
1. **The cross-channel claim rule lives beside the temporal one.** Same
    target in two channels: Doppler within one `res`, code phase within one
    sample, keep the
    stronger `test_stat`. Stated in C once, read together with the
    capture's `refine_span` proximity rule, so the two cannot drift into
    contradiction.
1. **Positions are on one grid.** Every channel decimates by the same
    factor, so `preamble_start` means the same sample across the bank; a
    detection's channel index names which window holds its burst.
1. **The state envelope is `dp_state.h`'s.** `[hdr][K][per channel: len +  child blob]`, each child self-validating; a file-backed bank's rings are
    the history and the blob names positions into them. A wrong blob is
    refused, never reinterpreted.
1. **Threads are the caller's — with one bounded parallel-for on offer.**
    The C kernels already release the GIL, so the Python thread pool keeps
    working over the C object unchanged. In C, `dp_parallel_for` (the
    bounded pthread parallel-for `Plan.prepare()` uses) fans `push` across
    channels when asked. The object never creates a thread it was not asked
    for.
1. **Thin means thin.** `Acquirer` becomes glue: construct, forward `push`,
    reshape `bursts()`, expose the state triplet. Its docstrings keep their
    physics; its logic moves.

## 4. The object

### 4.1 Shape

```text
burst_bank_state_t
  K, centers_hz[K]           layout, derived at create()
  ddc[K]                     one DDC per channel: mix -f_k, decimate to acq rate
  cap[K]                     one BurstCapture (or backed) per channel
  span_hz, res_hz            read from cap[0] at create()
  det[], det_len             bank-absolute detections of the LAST push (scratch)
```

`burst_bank_create(code, code_len, reps, spc, chip_rate, source_rate, doppler_uncertainty_hz, burst_len, cn0_dbhz, pfa, pd, noise_mode)` and a
`_backed` flavour taking a ring DIRECTORY. `burst_bank_push(x, n, out, max_out)` returns detections; `burst_bank_windows(k, …)` / `events(k)` /
`detections()` read the channel faces; `release(k, i)` forwards a consumer's
verdict; `configure_search_raw` forwards to every channel; the state
triplet composes the children.

### 4.2 What it composes, and what it does not reimplement

- `DDC` — mixer + decimator, unchanged.
- `BurstCapture` / `PersistentBurstCapture` — search, refine, retain, emit,
    hold and release; unchanged.
- `bin_to_signed` / `dp_fftfreq` — the ONE fold, already public.
- The dedup is new code and the only algorithm here. It is ~20 lines.

### 4.3 Rings are named by centre, and a channel is addressable alone

A file-backed bank names each ring `ch{f:+.0f}Hz.cf32` in its directory, as
the Python bank does since #1180: widening the bank adds channels at the
edges and keeps every ring pointing at the sub-band it holds. The C object
adds what the Python one lacks — `burst_bank_channel_create(descriptor, k)` builds channel `k` alone, with its ring, so a pod holding one channel
needs no bank around it, and `burst_bank_channel_state` is exactly
`cap[k]`'s blob wrapped with the DDC's.

### 4.4 Diagnostics

`underpowered` mirrors `cap[0]`'s (the sizing is per channel and
identical); `n_bursts` sums; `dropped` sums — a lost burst is a lost burst
whichever channel dropped it.

## 5. What changes for a caller

- `Acquirer(...)` keeps its signature. `process()`, `acquire()`,
    `bursts()`, `get_state()`, `set_state()` keep their meaning. The blob
    format changes once (`dp_state` envelope, `STATE_VERSION 1`) — the format
    is unreleased.
- A C caller gets the bank for the first time.
- `Detection.code_phase` on the capture path is already `epoch mod   code_bins`; nothing moves.

## 6. The unknowns — what characterization has to settle

| unknown                                                      | measured how                                                                                                                               |
| ------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------ |
| coverage across a channel boundary, as a curve, at reps 3–10 | Pd vs absolute Doppler through a boundary, 60 trials/point, the sweep #1179 asked for                                                      |
| the sign fold at the exact boundary                          | how often the kept detection carries the wrong sign; whether spacing at `(D−1)/D·2·span` or a dedup on the magnitude of Doppler removes it |
| the dedup window                                             | `res` in Doppler and 1 sample in phase against the two-channel straddle at −3.9 dB                                                         |
| throughput: serial, `dp_parallel_for`, Python pool           | `bench_burst_bank_core` and `bench_burst_bank.py`, `make bench-interleaved` across the three                                               |
| the blob: RAM vs file-backed, per channel and whole          | `state_bytes()` at the test geometry and at an 8029-symbol frame                                                                           |

The first row is the one that decides whether goal 2's layout rule survives
contact with reps = 10, where the intra-segment sinc at the boundary is the
same −3.9 dB but the bin is a tenth of the span.

## 7. Plan

Phases 2–10 of the standard, in order: declare (a `burst_bank` manifest
fragment and a `[module.dsss]` entry, `--preset blockwise` with
`pass_capacity`), implement (a `burst_bank` core under `native/`), pin (a
C test, each pin sabotaged), bind, instrument (the state triplet;
`check_serializable.py`), explore (a characterization subject), certify (a
validation report), document (header `@code` on every public function, a C
example and a Python example — the Python one with a coverage figure for a
gallery page — and both benchmarks), land.

## 9. Question 2 — the sign fold at the exact boundary

### 9.1 Context: what the fold is, and how wide

At an even coherent depth `D` the slow-time DFT has a Nyquist bin, and
`dp_fftfreq` reports it as `−span` — the one fold this repository keeps in
one place. A target whose residual Doppler in channel `k` lies in the upper
half of that bin, `[span − res/2, span]`, is therefore reported at `−span`
and lands at an absolute Doppler of `f_k − span`: **wrong by `2·span`, one
channel spacing.** Channel `k+1` sees the same target at a residual of
`−span − ε`, which the periodic slow-time transform aliases into its own
Nyquist bin and reports as `−span`, so `f_{k+1} − span = f_k + span`:
**right, to within half a bin.** Both neighbours see it at `sinc(0.5)`,
−3.9 dB, so the dedup's *keep the stronger* is a coin toss between a right
answer and one that is `D` bins off.

The strip is half a bin wide per boundary and every boundary has one, so a
uniform Doppler prior puts `1/(2D)` of all targets in a strip and about
half of those come out wrong: **~`1/(4D)` of acquisitions** — 6% at
`D = 4`, 3% at `D = 8`. The demodulator pulls in a seed error of 4 bins and
fails at 8 ([§3.3](dsss-burst-receiver.md)), so at `D = 8` every one of
those is a lost burst, and at `D = 4` it is a coin toss on top of a coin
toss. Odd `D` has no Nyquist bin and no fold; the outermost bin is reported
at `±(span − res/2)`, sign right.

None of this existed before #1183 — the strip was simply not searched, and
the target was lost outright instead of mis-reported. Fixing the hole
exposed the fold.

### 9.2 The options

|                                              | mechanism                                                                                                                                                                             | cost                                                                                                                             | what it does not fix                                                      |
| -------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------- |
| **(i) resolve in the dedup**                 | two detections with the same code phase whose Dopplers differ by exactly one spacing are one target *on the boundary*; report the boundary                                            | none in channels or gain; needs BOTH neighbours to have detected (each at −3.9 dB)                                               | a boundary target seen by one neighbour only is still a coin toss         |
| **(ii) overlap the channels**                | space at `(D−1)/D · 2·span`: the strip of channel `k` sits a full bin inside channel `k+1`, which sees it at full amplitude while `k` sees it at −3.9 dB, so *stronger wins* is right | `D/(D−1)` more channels: +33% at `D = 4`, +14% at `D = 8`                                                                        | nothing, if the dedup window is one `res`                                 |
| **(iii) prefer an odd depth**                | with no design point size `D = reps` if odd, `reps − 1` if even; with one, let the sizer skip even depths                                                                             | up to `10·log10(D/(D−1))` of coherent gain: 1.2 dB at 4, 0.6 dB at 8; and a link with `reps = 4` loses a quarter of its preamble | nothing — there is no fold to fix                                         |
| **(iv) report the Nyquist bin as ambiguous** | the engine flags a hit in the Nyquist bin; the channel reports a Doppler magnitude of `span` with a sign the bank resolves from the neighbour                                         | an engine API change and a flag through the capture's event                                                                      | the single-channel pod (`coarse-channel.md` §2.2) has no neighbour to ask |

(i) and (ii) compose: overlap makes the strip's owner unambiguous, and the
dedup rule then only has to keep the stronger. (iii) is the only one that
helps the single-channel pod, because it is the only one that removes the
ambiguity *inside* a channel.

### 9.3 The work that answers it

1. **Confirm the strip and its width on the engine**, single channel,
    `D = 4` and `8`: sweep residual Doppler across `[span − res, span]` in
    `res/8` steps, 40 trials each, and tabulate the reported sign and the
    absolute error. Expected: a clean step at `span − res/2` from right to
    `−2·span`. This is the sweep `acq`'s report should carry as a
    characterization row, since the fold is the engine's.
1. **Measure the rate at the bank**, `K = 3`, targets uniform over the bank's
    width, `reps ∈ {4, 8}`, 600 trials: the fraction of dedup'd detections
    whose Doppler is off by more than 4 bins. Expected ~`1/(4D)`. This is
    the number the options are judged against.
1. **Implement (i) as a pure function** over `(doppler_hz, code_phase,  test_stat)` tuples — the fleet aggregator needs it in that form anyway
    ([`coarse-channel.md`](coarse-channel.md) §2.2) — and re-measure step 2.
1. **Re-measure step 2 with (ii)**, spacing at `(D−1)/D · 2·span`, and
    record the channel count.
1. **Re-measure step 2 with (iii)** at `reps = 4 → D = 3` and `reps = 8 → D  = 7`, and record the Pd at the design point beside it — the gain given
    up is only a cost if it moves Pd at the C/N0 the link runs at.
1. **Decide by the demodulator's number**: the option, or pair, that gets
    the >4-bin error rate below the design `pfa` (a mis-reported Doppler is
    a false burst as far as the demodulator is concerned) at the smallest
    channel count wins; (iii) is adopted for the single-channel pod
    regardless if its cost in Pd is below the report's resolution.

Steps 1–2 are Python over the shipped engine (the sweep scripts from #1183
do most of it). Steps 3–5 are the bank's characterization subject, written
once and run with each option switched in.

### 9.4 What was measured (2026-09-01)

Steps 1–5 of §9.3, on the engine with #1183 applied, 55 dB-Hz, 400 trials
per row with the target uniform across the centre channel and both of its
boundaries:

|                                        | D=4: off by ≥ 4 bins   | D=8: off by ≥ 4 bins | note                                                                                                              |
| -------------------------------------- | ---------------------- | -------------------- | ----------------------------------------------------------------------------------------------------------------- |
| baseline (`2·span`, keep the stronger) | **8.5%**               | **5.2%**             | every error is exactly one spacing (4 bins at D=4, 8 at D=8)                                                      |
| (i) resolve the pair to the boundary   | 6.8%                   | 7.0%                 | helps only when BOTH neighbours detected; alone they are a coin toss                                              |
| (ii) overlap at `(D−1)/D·2·span`       | 2.8%                   | 2.2%                 | the neighbour sees the strip at full amplitude; residual is the noise on "stronger"                               |
| **(i) + (ii)**                         | 3.7%                   | **0.5%**             | the pair is always there to resolve, and the boundary is the higher report                                        |
| (iii) odd depth                        | D=3: 0% but 23% missed | D=7: 4.8%            | **does not remove the fold**: at ×2 interpolation an odd D has an interpolated row at exactly ±span, and it folds |

Two facts the sweep exposed that §9.1 did not know:

- The strip on a single channel starts at `span − res/2` at D=8 exactly as
    predicted — but the reason it is a whole half-bin wide is that
    `acq_core.c` divides the peak row by `interp` before reporting, so the
    **Doppler estimate is quantized to native bins** even though the search
    runs on the interpolated surface. Reporting the interpolated row's
    frequency instead would halve the strip to `res/4`, halve every Doppler
    error the demodulator is seeded with, and cost nothing — option **(vi)**,
    the one to try first, because it helps the single-channel pod too.
- The fold always reports **low** (`dp_fftfreq` gives the Nyquist bin as
    `−span`), which is what makes (i) implementable as "the boundary is the
    higher of the two reports"; the first prototype took the midpoint and
    made things worse.

The recommendation is therefore (vi) in the engine, then (i)+(ii) in the
bank, measured again; (iii) is withdrawn.

## 10. Question 3 — parallelism, and what it is for

### 10.1 Three motivations, three different requirements

The maintainer named four reasons the bank runs in parallel, and they do
not ask for the same thing:

- **Frequency-span coverage.** `K = ceil(U / 2·span)`, and `span =   chip_rate / (2·sf)` runs from tens of Hz at 50 sym/s to tens of kHz at
    1 Msym/s. At the low-rate end a modest uncertainty is *hundreds* of
    channels, each a DDC and a capture over the same stream. This is a
    throughput requirement: the bank must keep up with the source at `K`
    times the per-channel cost, and the only way it does is by putting
    channels on cores.

- **Resource and load balancing.** Channels are independent, equal-cost
    units of work with no shared state, which makes them the natural unit
    to place — across the cores of one node, or across the pods of a fleet
    (`coarse-channel.md` §2.2). This is a *shape* requirement: the unit of
    work must be addressable alone and its state must travel.

- **Multiple simultaneous signals at different Dopplers.** Two bursts, two
    channels, at the same time — in a C++ application that brings its own
    threads or worker processes, not in a fleet of pods
    ([`coarse-channel.md`](coarse-channel.md) §2.1). This is an
    *independence* requirement: a
    capture in channel `k` must not suppress, delay or share a queue with a
    capture in channel `j`, and the dedup must collapse only detections that
    are the same target — same code phase, Doppler within one `res` — and
    never two signals that happen to overlap in time. Each capture already
    owns its own ring, queue and suppression span; the bank must not
    introduce anything shared between them on the push path.

- **Speed.** Distinct from coverage: coverage sets `K`, speed is whether
    one block's work across all `K` finishes inside the block's own duration
    — real-time keep-up at the high-rate end, where the source is tens of
    MSa/s and even `K = 3` is three DDCs and three searches per block — and
    the latency from a burst's last sample to its window, which a serial
    bank stretches by `K` times the per-channel cost. This is the
    requirement the benchmark measures directly, and the one where per-call
    thread creation (option (b) below) can cost more than it buys at small
    blocks.

The third is a correctness property the design already has and the
characterization must pin (two bursts, overlapping in time, three channels
apart: both captured, neither delayed). The other three are what the
mechanism question is about — coverage and speed decide whether the bank
must thread at all, load balancing decides what the unit of work is.

### 10.2 The options

|                                                    | mechanism                                                                                                      | fits                                                                                                                                  | cost                                                                                                                                                                       |
| -------------------------------------------------- | -------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **(a) serial `push`; parallelism is the caller's** | the Python pool over per-channel handles, as today; the C++ application pushes channel `k` from its thread `k` | the primary consumer exactly (§10.1's third motivation is an application with its own threads); load balancing (channels are handles) | needs the channel to be addressable — shape B of `coarse-channel.md`; a caller that does not want to schedule gets no scaling from the bank alone                          |
| **(b) `dp_parallel_for` inside `push`, opt-in**    | `burst_bank_set_threads(n)`; `push` fans channels across up to `n` workers, serial when `n ≤ 1`                | span coverage for every caller; bit-identical to serial by `dp_parallel`'s contract                                                   | `dp_parallel_for` creates its workers **per call** — there is no pool — so each push pays `n` thread creations; at a 4096-sample block that is a real fraction of the work |
| **(c) a separate `push_parallel`**                 | two entry points, one contract                                                                                 | as (b)                                                                                                                                | two faces of one function; a caller has to choose per call                                                                                                                 |

(a) is not optional: the primary consumer brings its own threads, so the
channel must be pushable alone regardless of what the bank's `push` does.
The question is only whether the bank's own `push` ALSO fans out — (b) over
(c) if it does: a thread count is configuration, not a verb, and the serial
fallback is already inside `dp_parallel_for`. That turns on whether
per-call thread creation is affordable at the block sizes the link uses —
and if it is not, whether the bank grows a persistent pool (the
repository's first) or stays serial and leaves scaling to the caller, as
(a) already provides.

### 10.3 The work that answers it

1. **Measure the per-push cost floor.** Time `dp_parallel_for` with an
    empty body for `n ∈ {2, 4, 8, 16}` workers: that is the thread-creation
    tax per push, in microseconds, on the build box and on a CI runner.
1. **Bench the bank three ways** at `K ∈ {3, 9, 27}` and block sizes
    `{4096, 65536, 1M}`: serial; `dp_parallel_for` with `n = K` and `n =  cores`; the Python pool over channel handles. `make bench-interleaved`
    across the three, minimum-of-runs, as `benchmarking.md` requires.
    Report speedup against serial and the block size at which (b) breaks
    even with (a).
1. **Compute the keep-up requirement** for the two ends of the envelope:
    `K` and the per-channel cost per source sample at 50 sym/s and 1 Msym/s
    (the per-channel cost is the DDC plus the capture's push, both already
    benchmarked), against the source rate, and the window latency
    `K · t_channel(block)` beside it. That says whether a single node keeps
    up serially, with (b), or not at all — the last is the case where the
    fleet (load balancing) is not optional — and whether the latency at the
    link's block size is inside what a downstream demodulator tolerates.
1. **Pin independence.** Two bursts overlapping in time at Dopplers three
    channels apart: both captured, each at its exact sample, neither
    channel's `pending` moved by the other. A C test, sabotage-proven by
    sharing one queue.
1. **TSan.** `make test-tsan` over the bank's C test with (b) enabled at `n  = K`; a race is a defect, not a flake.

Steps 1–2 are the bank's benchmarks, which the standard owes anyway. Step 3
is arithmetic on published numbers. Steps 4–5 are C pins.

### 10.4 What was measured (2026-09-01)

Steps 1–3 of §10.3, on an 8-core build box, minimum of runs:

- **The per-call tax of `dp_parallel_for`**: ~15 µs per worker created —
    28 µs at 2 workers, 62 at 4, 123 at 8, 247 at 16. A push that fans 8
    channels pays 123 µs before any work.
- **The per-channel cost** (DDC at rate 1 plus the capture's push) is flat
    at **47–51 ns per source sample** across block sizes and `K`. At a
    4096-sample block that is ~200 µs per channel, so an 8-worker tax is
    60% of the work; at 65,536 samples it is 4%. **Break-even for (b) is a
    block of roughly 25,000 samples** (tax at 10%); below it, the caller's
    persistent pool wins.
- **The Python pool** (persistent threads, kernels releasing the GIL):
    speedup over serial 2.2–2.9× at `K = 3`, 2.8–5.0× at `K = 9`, 2.9–5.8×
    at `K = 27` from 4096 to 262,144-sample blocks — 73% of 8 cores at the
    large block, and still 2–3× at the small one where per-call threads
    would have paid 60%.
- **Keep-up, at the envelope's two ends**, from 48 ns/sample: at 4 MSa/s
    one channel is 19% of real time, so a serial bank keeps up to `K = 5`
    and the pool to `K ≈ 30`. At the high-rate end — 1 Msym/s × 50
    chips/symbol × 4 samples/chip = 200 MSa/s — one channel is **9.6×
    real time**: no single node keeps up with even one channel at the
    per-channel cost measured, and the fleet (or a 10× cheaper channel) is
    not optional. At the low-rate end — 50 sym/s, 10 kSa/s — one channel is
    0.05% of real time and hundreds of channels run serially inside 10%.

So: (a) is the primary path and it already scales; (b) is worth having for
a C caller with no pool of its own, gated on block size; the high-rate end
is a per-channel-cost problem before it is a threading one, and that number
belongs in the bank's C benchmark from the first commit.

## 11. The continuous case — the C++ application's waveform

*Added 2026-09-02 from the maintainer's description; the numbers are
derived from `async-dsss-receiver.md`'s waveform and the measurements in §10.4,
and the questions at the end are open.*

The C++ application does not receive bursts. It receives **continuous**
DSSS with asynchronous data — the CCSDS command-link shape
[`async-dsss-receiver.md`](async-dsss-receiver.md) already specifies (a 1023-chip
Gold code, 3.069 Mcps, ±50 kHz) — and the stream carries a **data-free
period of one code period just before each frame sequence**. Several
emitters are in the air at once on the **same** Gold code, and what tells
them apart is Doppler: each emitter's frequency difference *is* its
Doppler. There is **one frequency channel**: every emitter is in the same
band on the same code, and what distinguishes them is **code phase, power
and Doppler**. *(Maintainer, 2026-09-02: "one Gold code period; each
emitter frequency difference is Doppler"; "ONE frequency channel, emitters
are distinguished by code phase, power, and Doppler.")*

### 11.1 What the data-free window changes

Everything the burst family assumes about a preamble holds for that window
and for nothing else in the stream:

- **There is no coherent gain to buy.** The data-free window is one code
    period, so `reps = 1` and the coherent depth is one epoch — exactly the
    continuous `Acquisition` engine's search (`D = 1`, sensitivity from
    non-coherent looks, `dsss-acquisition.md`'s warning). The window buys
    one clean epoch without a data transition inside it, which the
    continuous engine already prices as a straddle loss and survives. The
    bank's reason to exist in this use case is therefore **not** gain —
    §11.3 says what it is.
- **The hand-off is to a tracking receiver, not to a frame demodulator.**
    A burst ends; a continuous signal is tracked from the seed onward
    (`carrier_acq → Dll + Costas`, the monolithic C receiver). So the
    channel's product is the `DetectionEvent` the async spec defines —
    Doppler, code epoch, C/N0 — and the window copy `BurstCapture` makes is
    not needed for the signal's sake. What may still be needed is the
    capture's **refine**: the frame begins where the data-free window ends,
    so *which* code period the window ended on is the frame epoch, and
    acquisition alone cannot say (§3.1 of the receiver design). Whether
    the tracking receiver's own frame sync makes that redundant is
    question 3 below.
- **The channel repeats.** A burst is acquired once; a continuous signal
    is re-acquired at every data-free window, and between windows it drifts
    (< 500 Hz/s in the spec). The claim rule across windows is then
    "same signal, next frame", not "same preamble".
- **Emitters come and go, at their own frequencies, and the bank is
    always on the air.** An emitter rises into the band at some Doppler,
    is acquired at its next data-free window, is handed to a tracker, keeps
    transmitting while others rise and set around it, and eventually
    leaves. The bank never stops searching: a channel that has handed one
    emitter off must go on watching its band for the next, and an emitter
    that drops out must be noticed and re-acquired when it returns. That is
    a **lifecycle** — searching → acquired → tracked → lost → searching —
    the burst family has no state for; a `BurstCapture` is done when the
    window is out. It is also a **duration** requirement: the process runs
    for hours or days, so nothing in the bank may grow with time
    (`samples_fed` is 64-bit; the per-push scratch reaches its high-water
    mark and stays; the rings are fixed) and a checkpoint is for a restart
    mid-pass, taken while everything is live.

### 11.2 The numbers, from the spec and §10.4

- Native span `3.069e6 / (2·1023)` = **1.5 kHz**; channel spacing 3.0 kHz;
    covering ±50 kHz takes `2·ceil(50/3)+1` = **35 channels** — one bank,
    since there is one code.
- At `spc = 2` the source is 6.14 MSa/s; at §10.4's 48 ns/sample a channel
    is **0.29× real time**, so the bank is **~10× real time** — eight cores
    at the measured 5.8× pool speedup do not keep up. Two things follow:
    the C++ application's own threads (§10.1, the primary path) are not
    optional, and the per-channel cost is the number to attack first — 48
    ns/sample was measured for `DDC → BurstCapture`, and a channel that
    hands off a `DetectionEvent` rather than a window needs neither the
    capture's ring nor its refine.
- The continuous engine's own `window_bins` tiling covers ±50 kHz in
    **one** engine at the same `D = 1` — the same tiling this bank does
    with DDCs, at the same sensitivity. What the single engine cannot do is
    §11.3's first item, and that, not gain, is what the `K`-fold cost buys.

### 11.3 The async tools, and what the bank adds to them

The continuous chain exists and is the thing to compose, not to rebuild.
`AsyncDsssReceiver` is one object with a three-state machine — **searching**
(the continuous `Acquisition`, window-tiled over the uncertainty),
**refining** (`acq_build_handoff` → a frozen-carrier `Dll` →
`CarrierAcquisition`), **tracking** (Costas → `Dll` → `RateConverter` →
`MpskReceiver`) — and it is the validated C port of the search → refine →
track prototypes. `DsssReceiver` is the same without the refining stage.
Both cover the whole ±50 kHz in one engine at `D = 1`.

So the C++ application's channel is not `DDC → BurstCapture`. Against what
already exists, the bank adds exactly three things, and each is a design
decision rather than a given:

- **Resolution on the (Doppler × code phase) surface.** Every emitter
    is a peak on the same 2-D surface a channel already computes, at its
    own Doppler bin and code phase, with its own power. A Doppler bank
    partitions one axis of that surface: emitters more than a span apart
    land in different channels and are found independently, with
    independent CFAR references. But emitters *within* a span — the normal
    case, since there is one band and only Doppler separates them — share a
    surface, and a detector that takes the **maximum** of it reports one
    of them per dwell, the strongest, and masks the rest. So the channel's
    detector must report **every** peak above threshold in a dwell, each
    with an exclusion zone around it (a bin in Doppler, a chip in code
    phase) so one emitter is not reported as several — a multi-peak report
    the engine does not make today. Then **power**: a 1023-chip Gold code's
    cross-correlation floor is about −24 dB, so an emitter that much weaker
    than the strongest in the same surface sits under the strongest one's
    sidelobes and is found only by cancelling the strong one first
    (successive interference cancellation) — and two emitters at the same
    Doppler *and* code phase within a chip are one peak, distinguishable by
    nothing. Question 7 is therefore answered: emitters do share a span,
    and the bank's channel count buys parallel surfaces and independent
    references but not resolution; the resolution is the detector's, per
    surface, and it is the piece to design.
- **Many emitters, one band.** One `AsyncDsssReceiver` tracks one signal;
    its state machine has no "lost" state and no second emitter. The bank
    is what holds the pool: which emitters are up, which channel each is
    in, which tracker it went to, and when it stopped being heard. That is
    §11.3's question 5, and the tools do not answer it today.
- **The frame epoch.** The refining stage recovers carrier, not which code
    period the frame started on; if the application needs that from the
    bank, it is the capture's refine, transplanted.

Everything else — the DDC, the tiling rule, the tracker, the hand-off
record — is already there.

**Two rules from the maintainer (2026-09-02) fix the channel's shape:**

- **It always has to be searching.** A channel never stops acquiring: the
    emitter it just handed off keeps transmitting in its band while a
    second one rises beside it, and the first one's loss has to be noticed
    by something that is still looking. That rules out
    `AsyncDsssReceiver` as the channel — its state machine *replaces* the
    search with refining and then tracking, feeding every sample to the
    tracker. In the bank, search and track are **concurrent** per channel:
    the search engine runs on every block, and each hand-off spawns a
    consumer that is fed the same samples beside it. Two things follow. A
    channel that keeps searching re-detects the emitter it handed off at
    every data-free window, so something must recognise "that one is
    already handed off" — a suppression keyed by emitter (its Doppler and
    code phase), the analogue of the capture's `suppress_until` keyed by
    time — and that is the bank's, which settles the *minimum* of question
    5\. And the per-channel cost in §11.2 is the search alone; each tracked
    emitter adds a tracker's cost on top, on the application's threads.
- **The hand-off logic is selectable.** What a detection becomes is a
    policy, not a property of the channel: hand a `DetectionEvent` to a
    tracker (this use case), capture a window for a frame demodulator (the
    burst use case), or report and do nothing (surveillance). The channel
    owns the search and the event; the policy owns what happens next and
    is chosen per bank, possibly per channel. This answers question 1 —
    the channel is `DDC → search`, and `BurstCapture`'s ring and refine are
    one *policy's* apparatus, attached only when that policy is selected.

### 11.4 Questions this raises (open)

1. ~~**Hand-off target.**~~ **Answered:** selectable — a policy on the
    detection (track / capture a window / report), not a property of the
    channel. The channel is `DDC → search`, always searching.
1. ~~**One Gold code per signal.**~~ **Answered:** one Gold code, shared;
    emitters differ by Doppler. One bank; the multi-signal case is *within*
    it, across channels.
1. **The frame epoch.** Does the tracking chain's own frame sync recover
    where the frame starts, or does the channel owe the refined code epoch
    (the capture's refine, at `1023·spc`-sample periods)?
1. ~~**The data-free window's length.**~~ **Answered:** one code period,
    so `reps = 1` and no coherent gain. Still open: the frame cadence — how
    often an emitter can be (re)acquired and how far it drifts (< 500 Hz/s)
    in between.
1. **Who owns the lifecycle.** *Partly answered:* because the channel
    always searches, the bank must at least remember what it handed off
    (an emitter keyed by Doppler and code phase) or it re-hands-off the
    same emitter every window. The receiver's half — how "gone" is
    decided and what it releases — is designed in
    [`acq-multi-peak.md`](acq-multi-peak.md) §5. Still open: whether the
    bank also owns the tracker pool and the assigned table, or reports
    "still there / gone" to an application that owns them.
1. ~~**How many emitters at once**, and how long an emitter is typically
    in view.~~ **Answered** (maintainer, 2026-09-02): at least one always
    on, up to 10 at once, each on for 5 to 15 minutes on average. The pool
    and the soak follow in [`acq-multi-peak.md`](acq-multi-peak.md) §1.1,
    §5 and §6.
1. ~~**Can two emitters sit within one span of each other?**~~
    **Answered: yes** — one frequency channel, one code; emitters are
    separated by code phase, power and Doppler on one surface. A channel
    therefore needs a multi-peak report per dwell with exclusion zones, and
    the engine has none. Open in its place: the **power spread** between
    emitters that are up at once — inside the Gold code's ~−24 dB
    cross-correlation floor a multi-peak report suffices; beyond it the
    weak ones need the strong ones cancelled first, which is a different
    object.

## 12. See also

- [`coarse-channel.md`](coarse-channel.md) — question 1: is the channel
    an object or a slice of the bank.
- [`burst-capture.md`](burst-capture.md) — what each channel is, §11 for the
    sizing contract and hold/release.
- [`dsss-acquisition.md`](dsss-acquisition.md) — the engine, and the
    2026-09-01 note on the full-band search.
- [`state-serialization.md`](state-serialization.md) — the envelope the
    bank's blob adopts.
