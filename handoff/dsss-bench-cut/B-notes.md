# Set B notes: Acquisition & DSSS (async pool, async receiver, DSSS receiver, burst receiver, wide-Doppler)

Read at worktree `finisher-acq`, HEAD `decdcda31`. Read-only: nothing under
the repo was edited, built or run. Paths below are relative to the repo root.

Short names for the bench files:

- POOL-C `native/benchmarks/bench_async_dsss_pool_core.c`
- POOL-PY `src/doppler/dsss/benchmarks/bench_async_dsss_pool.py`
- ARX-C `native/benchmarks/bench_async_dsss_receiver_core.c`
- ARX-PY `src/doppler/dsss/benchmarks/bench_async_dsss_receiver.py`
- ASTIM `src/doppler/dsss/benchmarks/_async_stimulus.py`
- DRX-C `native/benchmarks/bench_dsss_receiver_core.c`
- DRX-PY `src/doppler/dsss/benchmarks/bench_dsss_receiver.py`
- BRX-C `native/benchmarks/bench_dsss_burst_receiver_core.c`
- BRX-PY `src/doppler/dsss/benchmarks/bench_dsss_burst_receiver.py`
- BSTIM `src/doppler/dsss/benchmarks/_burst_stimulus.py`
- WD-PY `src/doppler/dsss/benchmarks/bench_widedoppler.py`

______________________________________________________________________

## KEEP / CUT

The rules I applied, in this order:

1. A sweep over a knob that can change the cost per sample (thread count,
    operating point or waveform, grid, block size) keeps its two ends and cuts
    the points in between. A knob that cannot change it keeps one row.
1. A sibling that shows the same cost per sample as a kept row is cut.
1. When the C and Python faces time *different* waveforms (no bench here
    pairs them on one stimulus), the Python row is kept only if it is the
    honest version of a hollow C row, or if it measures something no C row
    measures. Otherwise it is cut, because at 2,046 to 1,048,576 samples per
    call the binding's overhead is not what the row shows.
1. A row whose figure does not measure what its name says, with no honest
    twin, is cut. The note says what would bring it back.

| row                                                              | verdict  | reason                                                                                                                                                                                                                                                                                                                                   | covered by (for CUT)                                                                          |
| ---------------------------------------------------------------- | -------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------- |
| `c:async_dsss_pool::push[12 slots, serial]`                      | **KEEP** | Low end of the thread sweep. The population's cost per sample on one core, the unit the design budgets in (async-dsss-receiver.md §6.1, "budget, one core").                                                                                                                                                                             |                                                                                               |
| `c:async_dsss_pool::push[12 slots, all cores]`                   | **KEEP** | High end of the thread sweep (threads ≤ 0 = every online core, pool header :276-278). The thread count changes the wall-clock cost per sample: 6.18 → 8.97 MSa/s.                                                                                                                                                                        |                                                                                               |
| `py:async_dsss_pool::test_push_epoch`                            | CUT      | A different and unrepresentative case, not the C row plus overhead: noise with no emitter, the default depth of 407 periods (not the operating point's 154), false-alarm target 1e-3, a 16-peak list. Binding overhead on a ~1.3 ms, 2,046-sample call is not what it shows. Its existing label (labels.toml:335-339) goes with the row. | `c:async_dsss_pool::push[12 slots, serial]`                                                   |
| `c:async_dsss_receiver::steps[cell]`                             | **KEEP** | Small end of the waveform sweep for the pool's receiver: a 7-chip code with a 28-sample period and a 112-sample symbol, where the per-period and per-symbol overhead per sample is largest.                                                                                                                                              |                                                                                               |
| `c:async_dsss_receiver::steps[cell,op5M]`                        | **KEEP** | Large end of the same sweep, at the operating point (1,023 chips, 5 Mcps, the receivers' worst case per sample by design §6.4). This is what each extra tracking receiver adds to a pool push. **Must fix:** see COUNT PROBLEMS / stimulus (code restarts every symbol, replayed block, lock never checked).                             |                                                                                               |
| `c:async_dsss_receiver::steps[warm]`                             | CUT      | Same cost per sample as the cell row (30.8 vs 30.4). The flavor knob does not change the steady-state cost: past the Dll it is the same chain (ARX-C :300-304).                                                                                                                                                                          | `c:async_dsss_receiver::steps[cell]`                                                          |
| `c:async_dsss_receiver::steps[warm,op5M]`                        | CUT      | Same cost per sample as cell op5M (34.7 vs 34.8), for the same reason.                                                                                                                                                                                                                                                                   | `c:async_dsss_receiver::steps[cell,op5M]`                                                     |
| `py:async_dsss_receiver::test_bench_steps_warm`                  | CUT      | Python twin at a third waveform (3 Mcps Gold) whose cost equals the op5M end (35 vs 34.8). 64k calls, so overhead is not the question. It is the only steady-state row that checks it decodes (zero bit errors): move that check into ARX-C rather than keep the row.                                                                    | `c:async_dsss_receiver::steps[cell,op5M]`                                                     |
| `c:async_dsss_receiver::steps[cold]`                             | CUT      | Miscounted for its name (COUNT PROBLEMS 1). Also a one-time transient averaged over a bench-chosen 400 symbols, so it is not a rate.                                                                                                                                                                                                     | the steady state: `steps[cell]`; the search: `py:dsss_receiver::test_bench_steps_search`      |
| `c:async_dsss_receiver::steps[cold,op5M]`                        | CUT      | Same two reasons.                                                                                                                                                                                                                                                                                                                        | the steady state: `steps[cell,op5M]`; the search: `py:dsss_receiver::test_bench_steps_search` |
| `py:async_dsss_receiver::test_bench_steps_cold`                  | CUT      | Same defect: the whole 1,048,576-sample capture goes in one `steps()` call.                                                                                                                                                                                                                                                              | as above                                                                                      |
| `c:dsss_receiver::search`                                        | CUT      | Hollow input: 65,536 exact zeros (DRX-C :56), so the CFAR's noise reference is 0, the case BRX-C :74-77 and BSTIM :15-17 call "not the idle case a caller has". It is also a 7-chip code (a 28-point transform). Its honest twin exists.                                                                                                 | `py:dsss_receiver::test_bench_steps_search`                                                   |
| `py:dsss_receiver::test_bench_steps_search`                      | **KEEP** | The honest listening row: a realistic Gold-1023 code with real noise, reset every round, asserted never to lock (DRX-PY :113-124). It reads half the C toy's figure (45.1 vs 97 MSa/s), so the toy overstates a real search about 2x. Port this case to DRX-C, then cut this row.                                                        |                                                                                               |
| `c:dsss_receiver::track`                                         | **KEEP** | The C component's one row: the steady-state tracking cost per sample on a continuous, clean code.                                                                                                                                                                                                                                        |                                                                                               |
| `py:dsss_receiver::test_bench_steps_track`                       | CUT      | Same cost per sample (35.3 vs 34.7) on a different waveform. 64k calls.                                                                                                                                                                                                                                                                  | `c:dsss_receiver::track`                                                                      |
| `c:dsss_burst_receiver::push_idle`                               | **KEEP** | Low end of the bursts-per-block knob (0): the cost of listening.                                                                                                                                                                                                                                                                         |                                                                                               |
| `c:dsss_burst_receiver::push_burst`                              | **KEEP** | High end (1 burst per 8,192 samples): the search plus one decode. Asserted to decode and pass its CRC (BRX-C :289-304).                                                                                                                                                                                                                  |                                                                                               |
| `py:dsss_burst_receiver::test_bench_push_idle`                   | CUT      | Python twin on a different geometry (255×5 at spc 2 vs 31×4 at spc 4) and block size. It reads faster than C (53.8 vs 27.7), so binding overhead is not what it shows. See STILL CONFLICTING 6.                                                                                                                                          | `c:dsss_burst_receiver::push_idle`                                                            |
| `py:dsss_burst_receiver::test_bench_push_bursts`                 | CUT      | Same reasons. Its density (1 per 16,384) is an interior point of the bursts-per-block knob.                                                                                                                                                                                                                                              | `c:dsss_burst_receiver::push_burst`                                                           |
| `py:dsss_burst_receiver::test_bench_construct`                   | **KEEP** | No sibling: the one measurement of construction (6.62 ms), which sizes the search on the burst (BSTIM :200-203, about 4x a 64k push). It is what a caller pays to rebuild instead of `reset()`.                                                                                                                                          |                                                                                               |
| `py:widedoppler::test_bench_2d_roll[gps_16x2046-16-2046-79]`     | **KEEP** | Roll method, one end of the grid sweep. dsss-use-cases.md cites this grid (:57-58, :162-163).                                                                                                                                                                                                                                            |                                                                                               |
| `py:widedoppler::test_bench_2d_roll[burst_5x4094-5-4094-49]`     | **KEEP** | Roll method, the other end. It reads like the GPS grid today (1.69 vs 1.74), but rule 1 keeps both ends, and the doc's ≈10× burst-grid claim rests on it (:57-58, :136-139, :162-163).                                                                                                                                                   |                                                                                               |
| `py:widedoppler::test_bench_column_tile[gps_16x2046-16-2046-79]` | **KEEP** | Bank method, one end of the grid sweep.                                                                                                                                                                                                                                                                                                  |                                                                                               |
| `py:widedoppler::test_bench_column_tile[burst_5x4094-5-4094-49]` | **KEEP** | Bank method, the other end (35.3 vs 60.5: ny = 5, and 4,094 = 2·23·89 is a rough transform length).                                                                                                                                                                                                                                      |                                                                                               |

**Totals: 13 KEEP, 11 CUT.**

### Survival per object (every benchmarked component keeps at least one row)

| object                                              | kept / cut            | survives?                                  |
| --------------------------------------------------- | --------------------- | ------------------------------------------ |
| async_dsss_pool (C)                                 | 2 / 0 C, 0 / 1 Python | yes: both thread ends                      |
| async_dsss_receiver (C)                             | 2 / 4 C, 0 / 2 Python | yes: the cell rows at both waveform ends   |
| dsss_receiver (C)                                   | 1 / 1 C, 1 / 1 Python | yes: C track, plus the Python search       |
| dsss_burst_receiver (C)                             | 2 / 0 C, 1 / 2 Python | yes: idle, burst, plus Python construction |
| widedoppler (Python bench of FFT/Corr2D, no C twin) | 4 / 0                 | yes: two methods at two grids              |

Every Python row in this set is in the `dsss` module (`bench_report._component`
reads it from the path), and that module keeps 7 rows.

Judgment calls the coordinator may want to overrule:

- **Cold.** Cutting all three cold rows leaves no row for "what does a cold
    start cost". I cut them because none of them measures that (COUNT
    PROBLEMS 1). To bring one back honestly: feed the capture in blocks (for
    example 2,046 or 65,536 samples), and report it per acquisition (domain
    `call`) or as a time to lock. A per-sample figure over an arbitrary
    capture length is not a rate.
- **Searching flavor vs cell.** I treated it as a knob that cannot change
    cost per sample, from the measured equality and ARX-C :300-304. If the
    maintainer wants the standalone `AsyncDsssReceiver` named on the page, swap
    `warm`/`warm,op5M` in for `cell`/`cell,op5M`. Keep one pair, not both.
- **Python burst rows.** BRX-PY :20-22 keeps them on the same stimulus as
    the burst chain's parts (burst_acq, burst_despreader, burst_demod, ppe). If
    those parts keep Python rows in sets A or C, the composed receiver's
    Python pair is the comparable one. In that case, keep the Python
    idle/bursts pair and fix the C bench's geometry rather than the reverse.

______________________________________________________________________

## Per-row evidence

### AsyncDsssPool (C, kept)

- **Block and count.** 2,046 = TE = SF·SPC = 1,023·2 (POOL-C :15-17). Each
    round is one `dp_async_dsss_pool_push(p, x+pos, TE)` (:119-121). It is
    recorded with `jm_bench_add_unit(..., (int)TE, JM_BENCH_SAMPLE)` (:128-129),
    so `samples = 1`. The push consumes the whole block: the searcher loop
    resumes at `dp_acq_consumed()` until `off == x_len`
    (async_dsss_pool_core.c:282-285), and every receiver is fed the whole
    block (:347-350).
- **Chip and symbol rate.** CHIP_RATE 5e6, SYM_RATE 2700 (POOL-C :18, :20).
    The code is a hashed 1,023-chip sequence, not Gold (:80-89).
- **Constructor** (POOL-C :102-105, against pool header :314):
    - cn0 47, pfa 1e-2, pd 0.9, doppler_uncertainty 6000 Hz
    - code_only_epochs 31 (:29), doppler_rate 0, max_peaks 4, n_slots 12
    - threads 1 or 0, carrier 0, lost_confirm 2.0, max_on 0.0 (off)
    - segments 4, sps 8, cell gain and pull-in at their defaults
- **Depth D = 16.** acq_block_depth = acq_drift_depth(f_epoch, 0,
    (31+1)/2) = 16 (acq_core.c:915-919, :876-887). The searcher is built with
    that depth (async_dsss_pool_core.c:125-127). Row width f_epoch/D =
    4,887.6/16 = 305 Hz. The header's own arithmetic: D 154 → 31.7 Hz (pool
    header :250-251, :307-310).
- **Three Doppler tiles.** span = chip_rate/(2·sf) = 2,443.8 Hz
    (acq_core.c:1970). acq_cover_window_bins(6000, span) gives
    ns = ceil((6000 − 2,443.8)/4,887.6) = 1, so 2·1+1 = 3 (acq_core.c:1239-1258).
- **Threads.** Serial vs all cores: `threads[2] = {1, 0}` (POOL-C :99). 0
    means every online core (pool header :276-278). The searcher's tiles fan
    across them (dp_acq_set_threads, async_dsss_pool_core.c:129), and so do
    the receivers (pool created at :166, used at :350).
- **The capture.** Clean, +1,500 Hz (POOL-C :91, `build_capture(code, 2700, 1500.0, ...)`; "the cost of a push does not depend on the noise", :44-45).
    The code runs continuously on the global sample index (:70). The
    code-only window is the first 20 of every 270 symbols (:27-28, :64-66).
- **Warm-up and timed rounds.** The untimed warm-up pushes the first half
    of the capture (:108-111). The capture is 2,700 symbols at 2,700/s, so
    that is 0.5 s. The 200 timed pushes start at sample 5,002,470, the start
    of window symbol 1,350 (= 5·270), and never wrap (409,200 < n/2).
- **"One assigned and tracking"** comes from the bench's comments (:2-4,
    :108). It is **not checked**: `sink/ITERATIONS` is only printed
    (:131-132). See COUNT PROBLEMS 4.

### AsyncDsssPool (Python, CUT): what it times

- **Constructor** (POOL-PY :22-30): Gold-1023 code (:21), 5e6 chips,
    2,700 symbols, spc 2, cn0 47, doppler_uncertainty 6000, n_slots 12.
    Everything else is the manifest default (objects/async_dsss_pool.toml):
    - pfa 1e-3 (:86-88)
    - code_only_epochs **813** (:103-105)
    - doppler_rate 0 (:108-110)
    - max_peaks **16** (:113-115)
    - threads 1 (:126-128)
    - max_emitter_on_time 900 s (:141-143)
- **Depth D = (813+1)/2 = 407** periods (no rate bound), rows 12 Hz wide.
    Same three tiles as the C row.
- **Input.** Complex Gaussian noise, the same 2,046-sample array on every
    call (:33-42). `benchmark(obj.push, epoch)` (:41-42) gives one push per
    call, unit `call`, 2,046 samples.

### AsyncDsssReceiver (C, cell rows kept)

- **Two waveforms** (ARX-C :332-335):
    - `""` (toy): CODE7 (:58), 7 chips, spc 4, 1 Mcps, 35,714.29 symbols/s
    - `",op5M"`: a synthetic xorshift 1,023-chip code, not Gold (:323-330),
        spc 2, 5 Mcps, 2,700 symbols/s
    - The comments call op5M "the continuous async-DSSS operating point"
        (:27-33).
- **Symbol length.** tsym = round(chip_rate·spc/sym_rate) (:85-86): toy
    112 samples (= 28 chips = 4 code periods), op5M 3,704.
- **Capture.** pre = sf·spc·5+3 zeros (:87, :93-94): 143 for the toy,
    10,233 for op5M. Then N_SYM = 400 symbols (:44), so n = 44,943 and
    1,491,833. The TSV's iters_per_round matches both.
- **Per-symbol code restart.** The data sign is per symbol, and the chip
    index is `(i / spc) % sf` with `i` counting *within the symbol* (:98-107).
    The code restarts at every symbol. For the toy (4 whole periods per
    symbol) that is seamless. For op5M it is not: each 3,704-sample symbol
    holds one full period plus 829 chips, then restarts at chip 0. Hence
    "the code restarting at every symbol" in the cell,op5M definition.
- **Cell rows** (:242-299):
    - `dp_async_dsss_receiver_create_cell(..., m 2, cn0 70, pfa 1e-2, pd 0.9, segments 4, sps 8, diff 0, carrier 0, lost 0, periods, ASYNC_DSSS_RX_CELL_GAIN, ASYNC_DSSS_RX_CELL_PULLIN)` (:250-253).
    - periods = 100 for the 7-chip code, 154 otherwise (:249).
    - `seed(rx, 0.0, 0.0, 70.0)`: true phase 0, zero Doppler (:254).
    - Run once over x+pre, n−pre samples (:262).
    - Then the same second half, x+half, n−half samples, is replayed: a 0.25 s
        warm-up (:46, :274-281), then 20 timed rounds (:45, :282-289).
    - Count n−half: 22,472 and 745,917 (:291-292).
- **Cell mode semantics.** No acquisition, no refine chain; the Dll is held
    and corrected once every `correct_periods` (header :617-640). The
    carrier loop runs (:633-636). It is the pool's receiver (pool header
    :12-18, :233-243). "One held cell" (ARX-C :242-246).
- **154 = the operating point's D** at 5 Mcps (design §6.1 table, "coherent
    depth 154"; pool header :250-251).
- **The chain timed.** Carrier wipeoff, held Dll, RateConverter,
    MpskReceiver (header :39-48 for the track leg). Every sample is consumed:
    the cell's REFINING and TRACKING both run `adr_track_chain` over all of x
    (async_dsss_receiver_core.c:1240-1256 and :1364).

### AsyncDsssReceiver (C cold/warm, Python cold/warm; all CUT): what they time

- **Searching-flavor constructor** (ARX-C :113-119, against header :591-598):
    - cn0 70, pfa 1e-2, pd 0.9, doppler_uncertainty 500
    - segments 4, sps 8
    - refine_max_error_db **100** (one despread sample per code period; the
        manifest says a 100.0 default "forced" segments=1,
        objects/async_dsss_receiver.toml:150-156)
    - refine_sps 4, refine margin 14, refine_n_fft **32** (default 64), zero
        pad 8, not sequential, cap 100,000, carrier 0, lost 0
- **Cold** (:167-196): a fresh receiver each round, built untimed (:183),
    then one `steps()` over the whole capture (:185). Count n.
- **Warm** (:198-240): the receiver that ran the whole capture (:201-202),
    re-fed the same second half (:212-230). Count n−half.
- **Python receiver** (ARX-PY :63-66): Gold-1023 (ASTIM :38), 3 Mcps,
    2,100 symbols/s, spc 2 (ASTIM :27-36). Every other argument is a manifest
    default: cn0 55, pfa 1e-3, U 100, refine 0.5 dB, which gives 11 segments
    at a 2,046-sample period (toml :133-137, :158-160), and refine_n_fft 64
    (:179-181).
- **Python stimulus.** A continuous Synth DSSS stream (ASTIM :41-67): +50 Hz,
    Es/N0 40 dB, no lead-in.
- **Python cold**: 16 blocks of 65,536 = 1,048,576 samples in one call
    (ARX-PY :51, :88-95). Rate from the minimum (:69-73, :112).
- **Python warm**: 12 priming blocks (:56, :117-121), then the next
    contiguous 64k block per round (:126-131). Asserts tracking, carrier
    lock above 0.99, and zero bit errors (:133-142).

### DsssReceiver (C track and Python search kept; C search and Python track CUT)

- **C search** (DRX-C :52-88):
    - `create(CODE7, 7, 1e6, 35714.29, 4, 2, 45.0, 1e-2, 0.9, 500.0, 4, 8, 0)`
        (:54-55, against header :261-264)
    - input `calloc`, all zeros (:56); the same block in every round, no
        reset (:58-65)
    - refuses if tracking (:69-76)
    - count 65,536 (:77-78)
- **C track** (DRX-C :93-175):
    - signal: sf 7, spc 4, 1 Mcps, 35,714.29 symbols/s (:94-97)
    - symbols alternate ±1 (:115); continuous code on the global index (:116)
    - noise at C/N0 90 dB-Hz (:109-112: σ² = fs/10⁹)
    - same constructor (:120-121)
    - locks untimed (:124-129) and refuses if not locked (:142-153)
    - the next contiguous 65,536 samples per round (:155-162); count 65,536
        (:163-164)
- **Python receiver** (DRX-PY :63-76): Gold-1023, 3 Mcps, 2,100 symbols/s,
    spc 2, cn0 55, U 100, segments 4, sps 8. pfa is the default 1e-3
    (objects/dsss_receiver.toml:83-85).
- **Python search** (:88-125):
    - Synth noise (:101-105)
    - `rx.reset()` in pedantic's untimed setup (:109-118)
    - asserts tracking stays 0 and nothing is emitted (:123-124)
    - rate from the minimum over 65,536 (:79-85)
- **Python track** (:128-170): 10 priming blocks, then the next contiguous
    block; bit-exact check (:157-165).
- **Engine.** Both faces' search is `dp_acq_create_continuous(..., code_only_epochs 1, rate 0)` (dsss_receiver_core.c:359-361). That gives
    D = 1 and one tile: U < span in both cases (C span 71,428 Hz, Python
    1,466 Hz; acq_core.c:1239-1242), so it runs serial (acq_core.c:2040-2042).
- **Transform per period.** cb = sf·spc: 28 points (C) vs 2,046 (Python).
    The async receiver's searching state builds the same engine with the same
    arguments (async_dsss_receiver_core.c:897-899). That is why
    `py:dsss_receiver::test_bench_steps_search` also covers the async
    receiver's search.

### DsssBurstReceiver (C idle and burst, Python construct kept; Python idle and bursts CUT)

- **C geometry** (BRX-C :51-66):
    - ACQ_SF 31, an m-sequence from `dp_pn_create(pn_mls_poly(5), ...)`
        (:88-102)
    - DATA_SF 8, an arithmetic pattern (:104-111)
    - REPS 4, SPC 4, PAYLOAD 32, SYNC 13, CHIP_RATE 1e6, CN0 55
    - BENCH_N 8,192, BURST_AT 2,048
- **C frame.** sync | 32 bits | CRC-16 = 61 bits (:246-257). Burst =
    (4·31 + 61·8)·4 = 2,448 samples (:62-63; arithmetic 496 + 1,952).
- **C constructor.** `create_desc(acq, 31, data, 8, &frame, 4, 4, 1e6, 55.0, 0.0, 1e-3, 0.9, 0.0, 0.0, 10, NULL)` (:174-176, against header :285-290):
    doppler_uncertainty 0, pfa 1e-3, est_segments 10.
- **C noise.** σ 0.1 per rail (:244-245, :159-160) against ±1 chips
    (:147). Per-sample SNR = 1/(2·0.01) = 50, about 17 dB.
- **C rounds.** A fresh receiver per round, built untimed (:211-223). One
    push of 8,192 (:215). The header guarantees the push consumes every
    sample (dsss_burst_receiver_core.h:357, "EVERY SAMPLE OF @p x IS
    CONSUMED"). Count 8,192 (:225).
- **C checks.** Refuses unless the burst row decoded something that passed
    its CRC (:289-304).
- **C acquisition transform.** 31·4 = 124 points = 2²·31.
- **Python construct** (BRX-PY :82-85, receiver :64-79): acq 255-chip
    m-sequence (BSTIM :46, :53, :133), data 31-chip m-sequence (:54, :134),
    reps 5, spc 2 (:47), 1 Mcps (:48), cn0 60, U 0, pfa 1e-3, est_segments 10.
    `benchmark(lambda: _receiver(...))`, unit call. "Its search sized on the
    burst" (BRX-PY :83; BSTIM :200-203).
- **Python idle and bursts** (BRX-PY :88-128):
    - 65,536-sample blocks (BSTIM :50)
    - four bursts (BRX-PY :15-18, :104)
    - frame 13 + 64 + 16 = 93 bits (BSTIM :59-62)
    - BURST_LEN = (5·255 + 93·31)·2 = 8,316 (BSTIM :68)
    - Es/N0 12 dB (:49, :101-102)
    - checked bit-exact (BRX-PY :118-127)
    - fresh receiver per round (BSTIM :194-226); rate from the minimum
        (:229-239)
    - acquisition transform 510 points (BSTIM :32-40)

### Wide-Doppler (all four kept)

- **Grids.** gps: ny 16, nx 2,046, d 79, "±39 kHz / 1 kHz window"; burst:
    ny 5, nx 4,094, d 49, "±60 kHz / 2.44 kHz" (WD-PY :31-34).
- **Roll** (:37-56, :59-70): `FFT(nx, -1, 1)` forward and `FFT(nx, +1, 1)`
    inverse, both one thread (objects/fft.toml:9-22). Pre-rolled conjugate
    code spectra, d of them (:49-50). Per call: one forward, then d NumPy
    products and d inverses (:52-54). MSa_s = nx/mean (:70).
- **Tile** (:73-94):
    - `Corr2D(ref, 1)`: dwell 1, nthreads 1 (objects/corr2d.toml:26-33)
    - a random full ny×nx reference (:81-83), so not the single-row fast
        path (corr2d_core.h:15-27)
    - execute takes exactly one ny·nx frame (corr2d.toml:61-74)
    - MSa_s = ny·nx/mean (:93)
    - bank cost for the span = d·mean, extra_info only (:94)
- **Doc.** dsss-use-cases.md:54-59, :136-139, :158-167 cites both grids.

______________________________________________________________________

## COUNT PROBLEMS

Every row's recorded count equals the samples its timed call is handed.
Every timed call consumes its whole input:

- the pool loops on dp_acq_consumed
- the receivers' search at room 1 with no hit takes everything
    (acq_core.h:1607-1622)
- the burst push consumes everything by contract
- Corr2D refuses any other length

So no row credits samples it never read. Two rows' groups do more work per
counted sample than their names say, and four rows' counts are right but
their premise is unchecked.

1. **The cold rows run most of their capture twice: CUT rows
    `c:async_dsss_receiver::steps[cold]`, `[cold,op5M]` and
    `py:async_dsss_receiver::test_bench_steps_cold`.** Each round hands the
    whole capture to ONE `steps()` call (ARX-C :185, ARX-PY :93-95). After
    the hit, the tail of that call is handed to the refining state
    (async_dsss_receiver_core.c:1231-1235). There `adr_process_refine(state, x, x_len)` runs every whole code period of the call through the refine
    chain (:1258-1262, loop at :513-541):

    - frozen-carrier wipeoff, refine Dll, RateConverter, CarrierAcquisition
    - no stop once the estimate is ready; `dp_carrier_acq_steps` becomes a
        no-op, but the wipeoff, Dll and resampler upstream of it keep running
        (:471-473, :488-506; carrier_acq_core.c:310-311)

    Then the samples past the refine's end are handed to the TRACKING chain
    again as the "unused tail" (:1318-1323, :1354-1358). So nearly every
    sample after the detection is processed by both chains, and counted once.

    A caller feeding blocks pays the double cost only inside the one block
    where refinement ends. The cold figures therefore measure the one-call
    feeding pattern, not "what a burst-mode receiver pays" (ARX-C :20-21,
    ARX-PY :15-17). The bench's printed "cold/warm ... is acquisition and
    refinement, paid once per burst" (ARX-C :305-308) misreads it.

    This is consistent with the numbers, as an inference I could not run:
    cold − warm per sample is about 24 ns at op5M (54.3 vs 28.8 ns) and
    about 23 ns in Python (52.4 vs 28.6), roughly a second, refine-chain
    pass over the capture, not a one-off.

1. **`c:async_dsss_receiver::steps[cell,op5M]` (KEPT) and `[warm,op5M]`
    time a stimulus that is not the waveform they claim.**

    - The code restarts at every symbol (ARX-C :98-107; see the evidence
        above), so at op5M the code phase jumps 829 chips every 3,704 samples.
    - Every timed round also replays the same block (:217-228, :282-289), a
        code- and data-phase jump at each round start. The Python twin names
        this hazard: "a replayed block would jump the code phase and time a
        re-pull-in" (ARX-PY :123-125).
    - The only precondition is `get_tracking() == 1` (:150-151, :203, :263).
        That state cannot be left at `lost_confirm_s = 0` ("never enters it",
        header :65-66), and a receiver off the code "still emits symbols at
        full rate" (ARX-PY :27-30).

    Nothing in the bench shows these rows time a locked receiver. Whether
    the cost per sample depends on lock is not settled by source: most of
    the chain runs either way, but coasting skips the carrier update
    (async_dsss_receiver_core.c:670-688). The toy rows are not affected by
    the restart (4 whole periods per symbol), but they are by the replay.
    **Fix before trusting the kept op5M row:** index the code on the global
    sample (as POOL-C :70 does), feed the next contiguous block, and assert
    decoding, as ARX-PY's `_decodes` does.

1. **`c:dsss_receiver::search` (CUT)** times exact zeros (DRX-C :56). The
    count is right, but the CFAR reference is zero.

1. **`c:async_dsss_pool::push[...]` (KEPT, both)**: "one emitter assigned
    and tracking" is a comment (POOL-C :2-4, :108). The assigned count is
    printed and never checked (:131-132). If the warm-up ever failed to
    assign, the rows would silently time twelve idle receivers. Add a
    refusal (assigned == 1 and slot state tracking) like ARX-C :150-159.

1. **`py:async_dsss_pool::test_push_epoch` (CUT)** never asserts that its
    receivers stayed idle; a false alarm on the noise would seed one. The
    pfa is per dwell, split across cells (acq_core.h:526), so this is
    unlikely but unchecked.

______________________________________________________________________

## STILL CONFLICTING (with correct labels)

1. **Pool, C one-thread 6.18 vs Python 1.59 MSa/s (Python CUT).** Not
    binding overhead: ~0.95 ms of a 1.29 ms call. The cases differ in:

    - depth D: 16 vs 407
    - pfa: 1e-2 vs 1e-3
    - peak list: 4 vs 16
    - input: clean signal with a window vs noise
    - one tracking receiver vs none
    - max on-air time: off vs 900 s

    The source does not settle which dominates. One candidate, not
    established: the slow-time transform per tile is D·2 points
    (acq_core.c:182, :861-868), so 32 vs 814 = 2·11·37, a rough length. The
    caller-supplied profile (searcher ≈ 88% of a C push) is consistent with
    a searcher-dominated gap, but it was measured on the C case, not the
    Python one. Neither face runs the operating point's D = 154. If the
    depth knob's cost matters, it belongs in POOL-C as two ends (e.g. D 16
    and D 154), not in a Python row.

1. **Pool, all cores only 1.45× one thread.** The searcher fans its three
    tiles. The per-tile results merge serially, and the forward transform is
    shared (acq_core.h:118-129). Only one of the twelve receivers does real
    work: idle ones return at once (async_dsss_receiver_core.c:1189-1190).
    The core count of the measuring machine is not in the row ID, so the
    all-cores figure is machine-dependent.

1. **Cell toy 30.4 vs cell op5M 34.8.** The toy pays more per sample:

    - a 28-sample code period vs 2,046
    - a 112-sample symbol vs 3,704
    - 8 resampler outputs per 112 inputs vs per 3,704

    That is the expected direction. The op5M figure carries COUNT PROBLEMS 2.

1. **DsssReceiver search, C 97 vs Python 45.1 (C CUT): not the same case.**

    - a 28-point vs a 2,046-point transform per period
    - design C/N0 45 vs 55; pfa 1e-2 vs 1e-3; U ±500 vs ±100 Hz
    - zeros vs noise; no reset vs reset each round
    - C reports the mean, Python the minimum (item 8)

    The transform length alone predicts roughly the 2× seen.

1. **DsssReceiver track, C 34.7 vs Python 35.3 (Python CUT).** Equal despite
    the toy's ~25× higher symbol and period rate per sample. The track
    chain's per-sample work (wipeoff, Dll) evidently dominates its per-period
    and per-symbol work at both points. That is consistent, not
    contradictory.

1. **Burst receiver, C slower than Python (idle 27.7 vs 53.8, burst 22.7 vs
    31.1).** BRX-PY :22-25 says the C bench "measures the same split; the gap
    between the two faces is the binding's per-call overhead". That is
    false. The stimuli differ in:

    - transform length: 124 = 4·31 vs 510 = 2·3·5·17 (BSTIM :32-45 explains
        why rough lengths cost)
    - spc 4 vs 2, reps 4 vs 5, design C/N0 55 vs 60
    - data code: 8-chip pattern vs 31-chip m-sequence
    - block: 8,192 vs 65,536, with a fresh receiver per round amortized over
        each
    - burst density: 1 per 8,192 vs 1 per 16,384
    - statistic: mean vs minimum

    Fix the docstring, or move BRX-C onto BSTIM's geometry.

1. **Wide-Doppler, tile 60.5 vs roll 1.74 MSa/s.** Read per input sample,
    the tile looks 35× faster, yet the doc says the bank is ≈40× the roll.
    Both are true:

    - A roll call covers all d windows from one epoch.
    - A tile covers one window from ny epochs.
    - Over the span, the bank's per-sample rate is tile/d: 60.5/79 = 0.77
        and 35.3/49 = 0.72 MSa/s, against the roll's 1.74 and 1.69. Per input
        sample the bank is only ~2.3× the roll.
    - The doc's 40×/10× is per call: d tile calls over ny epochs against one
        roll call over one epoch.

    The definitions now say "covers all N bins" and "1 of N windows". The
    page's per-sample reading still cannot show the per-span ratio the doc
    quotes. Also, the roll row includes 80 binding calls and 79 NumPy
    products per epoch. The shipped C engine does the roll internally (the
    acq window-tiled engine), so these rows are a Python study of a choice
    design §6.4 already settled, not a library throughput. Consider whether
    they belong on the page at all, or only behind the doc.

1. **Statistic mismatch across faces (page-wide, not per row).**

    - C rows record units/**mean** (jm_bench.h:16-18, :419, :446).
    - ARX-PY, DRX-PY and BSTIM `rate()` record from the **minimum** (ARX-PY
        :69-73, DRX-PY :79-85, BSTIM :229-239).
    - WD-PY uses the mean (:65, :90).
    - A Python `call` row (the pool) is pytest-benchmark's mean
        (scripts/bench_report.py `seconds_per_unit`).

    Every C-vs-Python comparison on the page mixes statistics. It is not
    fixable in a label; the page or the benches should pick one.

______________________________________________________________________

## Could not determine from source

- Whether the op5M receiver rows time a locked receiver (needs a run). Same
    for whether lock changes their cost per sample.
- Which difference drives the pool's C/Python 3.9× gap. The 814-point
    slow-time transform is a hypothesis.
- The fraction of each cold capture spent searching and refining. The bench
    does not report it. This is moot once the cold rows are cut.
- The core count behind the v0.72.0 all-cores figure.
- Non-coherent look counts (n_noncoh) for each searcher configuration. They
    are auto-sized at create from C/N0, pfa, pd and D
    (acq_core.c:1392-1405), and the source gives no fixed value.
- **Blocker for the coordinator.** dsss-use-cases.md quotes both widedoppler
    grids (≈40×, ≈10×). I kept all four rows, partly for that reason. Cutting
    either grid needs the doc's figure re-homed first.
