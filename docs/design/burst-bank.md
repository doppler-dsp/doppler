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

## 8. See also

- [`burst-capture.md`](burst-capture.md) — what each channel is, §11 for the
    sizing contract and hold/release.
- [`dsss-acquisition.md`](dsss-acquisition.md) — the engine, and the
    2026-09-01 note on the full-band search.
- [`state-serialization.md`](state-serialization.md) — the envelope the
    bank's blob adopts.
