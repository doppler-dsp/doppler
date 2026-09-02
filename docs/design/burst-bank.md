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
    channels, at the same time. This is an *independence* requirement: a
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

|                                                    | mechanism                                                                                       | fits                                                                                | cost                                                                                                                                                                       |
| -------------------------------------------------- | ----------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **(a) serial `push`; parallelism is the caller's** | the Python pool over per-channel handles, as today; a C caller writes its own loop              | load balancing (channels are handles); span coverage only if the caller threads     | needs the channel to be addressable — shape B of `coarse-channel.md`; a C caller gets no scaling for free                                                                  |
| **(b) `dp_parallel_for` inside `push`, opt-in**    | `burst_bank_set_threads(n)`; `push` fans channels across up to `n` workers, serial when `n ≤ 1` | span coverage for every caller; bit-identical to serial by `dp_parallel`'s contract | `dp_parallel_for` creates its workers **per call** — there is no pool — so each push pays `n` thread creations; at a 4096-sample block that is a real fraction of the work |
| **(c) a separate `push_parallel`**                 | two entry points, one contract                                                                  | as (b)                                                                              | two faces of one function; a caller has to choose per call                                                                                                                 |

(b) over (c): a thread count is configuration, not a verb, and the serial
fallback is already inside `dp_parallel_for`. The open question is not the
API but whether per-call thread creation is affordable at the block sizes
the link uses — and if it is not, whether the bank grows a persistent pool
(the repository's first) or leaves scaling to the caller's pool, as (a).

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

## 11. See also

- [`coarse-channel.md`](coarse-channel.md) — question 1: is the channel
    an object or a slice of the bank.
- [`burst-capture.md`](burst-capture.md) — what each channel is, §11 for the
    sizing contract and hold/release.
- [`dsss-acquisition.md`](dsss-acquisition.md) — the engine, and the
    2026-09-01 note on the full-band search.
- [`state-serialization.md`](state-serialization.md) — the envelope the
    bank's blob adopts.
