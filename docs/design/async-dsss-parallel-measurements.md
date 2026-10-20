# Parallel Asynchronous-Data DSSS Receivers — the measurement record

*The dated record behind [the design page](async-dsss-parallel.md): what each
number there was measured by, with the run. Section numbers are shared, so a
`§3.1` cited on the design page, in an issue or in a code comment is the same
entry here. The tables come from two runs of shipped tools, at one commit:
the lock validator, `native/validation/async_dsss_receiver_lock.c`, and the
C benches of the three objects. A third run, of the same validator, draws
§3.2's figures, and is held to the first instead: each of its stories must
match its sweep trial's carrier, arrival and symbol-lock time. Every table below, and every number the
design page quotes from them, is rendered from their data, committed beside
this page in `async-dsss-parallel-measurements/` by its `record.py`, whose
`--check` (in `make docs-invariants`) fails when either page differs from
the data.*

<!-- async-dsss-parallel:stamp:start -->

| run                        | when                       | machine                                                          | load before / after | commit             | toolchain                                                             | command                                                                                                                         |
| -------------------------- | -------------------------- | ---------------------------------------------------------------- | ------------------- | ------------------ | --------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------- |
| lock validator, full sweep | 2026-10-20 04:37 UTC, 42 s | `swiftgo-ai9465`: AMD Ryzen AI 9 465 w/ Radeon 880M (20 threads) | 1.9 / 1.5           | `e3dc58307`, clean | GNU 14.2.0; glibc 2.41; build Release, flags -O3 -DNDEBUG, native OFF | `make build`, then `record.py measure`                                                                                          |
| C benches                  | 2026-10-20 04:40 UTC       | `swiftgo-ai9465`: AMD Ryzen AI 9 465 w/ Radeon 880M (20 threads) | 2.0 / 1.8           | `e3dc58307`, clean | GNU 14.2.0; glibc 2.41; build Release, flags -O3 -DNDEBUG, native OFF | `make bench BENCH_ARGS="--c-only acq async_dsss_receiver async_dsss_pool"` once the load fell under 2, rows kept by `record.py` |
| lifecycle stories (§3.2)   | 2026-10-20 04:39 UTC, 1 s  | `swiftgo-ai9465`: AMD Ryzen AI 9 465 w/ Radeon 880M (20 threads) | 2.4 / 2.4           | `e3dc58307`, clean | GNU 14.2.0; glibc 2.41; build Release, flags -O3 -DNDEBUG, native OFF | `make build`, then `record.py stories`; each story is the sweep's arrival trial of its draws, checked                           |

<!-- async-dsss-parallel:stamp:end -->

*The operating point throughout: Gold-1023 (CCSDS #365) at 3.069 Mcps, two
samples a chip, so one code epoch is 2,046 samples, 333.3 µs, and 1/T_E is
3 kHz; asynchronous BPSK at 2,700 sym/s. The receiver is the searching flavor
at the binding's defaults, its own search over ±1/T_E, its lock detector the
shipped `lockdet` at the header's settings, read as it reports: locked or
not. The emitter is the shipped continuous-DSSS synth carrying the
validator's own data bits, with no data-free window, and the validator
checks the synth is sending them: every lock measurement is taken where the
symbols transition, and each row's "data flips" is the fraction of symbol
boundaries that flip around its event, counted from the bits. The noise is
the shipped awgn, and an arrival or a dropout cuts the signal only. A
fraction carries its 95% Wilson interval.*

______________________________________________________________________

### 3.1 Symbol lock through a dropout

The validator's `dropout` trials. Each draws its carrier over ±1/T_E, and is fed until the receiver tracks with symbol
lock up for 200 epochs in a row, then up to 10 ms more, then cut at a random
sample of the next epoch. A draw that has not held lock that long within
4 s, or whose lock is down at the cut, is excluded and counted (§5.1 says
why a draw can miss); trials are drawn until 20 settle, so the intervals'
fixed-n assumption is approximate. The receiver's state at the cut is saved
and every case resumes from it on the same noise, so they differ only in
the dropout: the validator checks that the restore is exact, every case's
lock reading the for-good case's until its signal returns. Symbol lock is
read after every epoch, so every time is to 0.33 ms. The receiver stays
tracking throughout (`lost_confirm_s` = 0): a lock that falls comes back by
itself.

<!-- async-dsss-parallel:dropout:start -->

| Es/N0 | carriers drawn | draws settled: lock held 200 epochs within 4 s, up at the cut | lock falls after a loss, min / median / max | a 50 ms dropout: fell; back within 300 ms; min / median / max               | a 200 ms dropout: the same                                                   | data flips at the cut |
| ----- | -------------- | ------------------------------------------------------------- | ------------------------------------------- | --------------------------------------------------------------------------- | ---------------------------------------------------------------------------- | --------------------- |
| 10 dB | -2892–+2821 Hz | 20 of 20 (0.84–1.00)                                          | 26.1 / 29.9 / 34.6 ms                       | fell 20 of 20 (0.84–1.00); back 20 of 20 (0.84–1.00); 13.7 / 26.8 / 44.9 ms | fell 20 of 20 (0.84–1.00); back 20 of 20 (0.84–1.00); 14.9 / 36.5 / 62.7 ms  | 0.45–0.54             |
| 5 dB  | -2892–+2821 Hz | 20 of 20 (0.84–1.00)                                          | 14.8 / 21.2 / 42.4 ms                       | fell 20 of 20 (0.84–1.00); back 20 of 20 (0.84–1.00); 26.6 / 47.2 / 99.7 ms | fell 20 of 20 (0.84–1.00); back 20 of 20 (0.84–1.00); 40.9 / 67.2 / 140.0 ms | 0.46–0.53             |

<!-- async-dsss-parallel:dropout:end -->

**The hold, by dropout length.** Each trial is run through a dropout of every
length from one epoch to two past its own drop, each from the saved state:
held means symbol lock stayed up through the dropout and 60 ms after it.

<!-- async-dsss-parallel:hold:start -->

| Es/N0 | draws excluded: not settled | held 2 ms            | held 5 ms            | held 10 ms           | held 15 ms           | held 20 ms           | held 25 ms          | held 30 ms          | trials where a longer one held after a shorter fell |
| ----- | --------------------------- | -------------------- | -------------------- | -------------------- | -------------------- | -------------------- | ------------------- | ------------------- | --------------------------------------------------- |
| 10 dB | 0 of 20 (0.00–0.16)         | 20 of 20 (0.84–1.00) | 19 of 20 (0.76–0.99) | 18 of 20 (0.70–0.97) | 19 of 20 (0.76–0.99) | 15 of 20 (0.53–0.89) | 9 of 20 (0.26–0.66) | 0 of 20 (0.00–0.16) | 8                                                   |
| 5 dB  | 0 of 20 (0.00–0.16)         | 20 of 20 (0.84–1.00) | 17 of 20 (0.64–0.95) | 16 of 20 (0.58–0.92) | 13 of 20 (0.43–0.82) | 2 of 20 (0.03–0.30)  | 2 of 20 (0.03–0.30) | 0 of 20 (0.00–0.16) | 11                                                  |

<!-- async-dsss-parallel:hold:end -->

The hold is not one number per draw. In many trials a longer dropout held
after a shorter one fell (the last column): the lock average, an exponential
average over 30 symbols, sinks under the detector's drop level in the
dropout and climbs back too late after it, and where it lands depends on the
noise of those few milliseconds. So the hold is reported as a fraction per
length, never as an edge.

### 3.2 A receiver through a signal's life

The validator's lifecycle stories (`--telemetry DIR`): two of §5.1's
arrival trials, each run on past its own end as one receiver's whole life.
Its first second is the trial sample for sample, and the table holds each
story to its trial. Then the signal is cut for 10, 20 and 50 ms at 1.0, 1.3
and 1.6 s, every cut mid-data, and leaves for good at 2.0 s; the run ends
at 2.4 s. Each figure is the receiver's own telemetry from that run, drawn
by `scripts/plot_capture.py`. **Grey is where the signal was off**, read
from the stimulus the validator records beside the receiver's probes
(`stim.on`), not drawn from the schedule above. `stim.on` is emitted every
block from the first, so the shared time axis starts at the first block; the
receiver's own traces start where its live chain starts, not at the arrival
(none of them reports before then; the search's `rx.acq.*` are captured, not
drawn).

<!-- async-dsss-parallel:stories:start -->

| story            | Es/N0 | seed | carrier    | arrival | symbol lock after arrival |
| ---------------- | ----- | ---- | ---------- | ------- | ------------------------- |
| `lifecycle-10db` | 10 dB | 1    | +2395.3 Hz | 19.5 ms | 110.5 ms                  |
| `lifecycle-5db`  | 5 dB  | 13   | +2587.8 Hz | 19.1 ms | 153.2 ms                  |

![lifecycle-10db](async-dsss-parallel-measurements/lifecycle-10db.png)

![lifecycle-5db](async-dsss-parallel-measurements/lifecycle-5db.png)

<!-- async-dsss-parallel:stories:end -->

At 10 dB symbol lock rides out the 10 and 20 ms fades, falls in the 50 ms
one and comes back after it, and falls for good at the departure, as do the
carrier and code flags; the code flag falls in every fade. At 5 dB symbol
lock rides out only the 10 ms fade. The demodulator's timing flag
(`demod.sync.locked`) falls in none of them: it stays up through every fade
and long after the signal has gone (#2409).

The record's run at `9c386aad5` had a third story,
`lifecycle-5db-no-lock`: seed 5 at 5 dB, whose refine seed was 444 Hz off,
further than the carrier loop pulls in, so the carrier loop never declared
and symbol lock never came, while the code loop tracked and the timing flag
came up and stayed up. Its figure is in that commit. Since the `Dll`'s
output is divided by a slowly tracked level (#2495), that draw's seed lands
106 Hz off and it locks in 296 ms, and no 5 dB arrival trial of this run
misses, so this run has no story that never locks.

### 5.1 Arrival to tracking, and to symbol lock

The validator's `arrival` trials: noise alone for a uniform 0 to 20 ms, so
the signal starts at any sample of an epoch and any code phase against the
feed, then the signal at a carrier uniform over ±1/T_E, for 2 s after it
arrives. Times are from the arrival sample, to the first epoch the receiver
reads tracking and to the first after which symbol lock is up.

<!-- async-dsss-parallel:arrival:start -->

| Es/N0 | tracking: draws; min / median / max after arrival | symbol lock: draws; min / median / max          | locked 2 s after arrival | data flips |
| ----- | ------------------------------------------------- | ----------------------------------------------- | ------------------------ | ---------- |
| 10 dB | 20 of 20 (0.84–1.00); 43.5 / 44.6 / 47.3 ms       | 20 of 20 (0.84–1.00); 55.6 / 77.2 / 219.5 ms    | 20 of 20 (0.84–1.00)     | 0.47–0.56  |
| 5 dB  | 20 of 20 (0.84–1.00); 79.4 / 86.3 / 100.8 ms      | 20 of 20 (0.84–1.00); 119.1 / 288.6 / 1640.5 ms | 20 of 20 (0.84–1.00)     | 0.47–0.56  |

<!-- async-dsss-parallel:arrival:end -->

**What decides the lock: the refine stage's carrier.** The same trials, by
how far the refine stage's carrier estimate, read when the receiver first
tracks (`get_doppler_hz()`, frozen from then), lands from the truth: the
residual the carrier loop must pull in. That loop's acquisition bound is
`bn / m`, 60 Hz at this front end, and `costas_pullin.c` measures it acquiring
every draw inside the bound, still reliably to twice it, and dead by four
times (`async_dsss_receiver_core.h`); doppler#982 found the refine handing
over outside it. The whole receiver is more forgiving than the loop alone:
lock time grows with the seed error, and the 200–300 Hz row still locks, past
four times the bound and slowly. The seed-edge block below gives the largest
locked error and counts the draws at 250 Hz or more, of which this run has
none. The error is read when
tracking starts, which need not be the residual the loop finally pulls in;
`rx.car.nco` (#2412) now shows what the loop pulls in, and read as a residual
it is not yet measured (doppler#2409).

<!-- async-dsss-parallel:seed:start -->

| refine seed error | locked, 10 dB                      | locked, 5 dB                        |
| ----------------- | ---------------------------------- | ----------------------------------- |
| 0–100 Hz          | 16 of 16 (0.81–1.00), median 74 ms | 10 of 10 (0.72–1.00), median 157 ms |
| 100–200 Hz        | 4 of 4 (0.51–1.00), median 165 ms  | 8 of 8 (0.68–1.00), median 482 ms   |
| 200–300 Hz        | —                                  | 2 of 2 (0.34–1.00), median 1424 ms  |
| ≥ 300 Hz          | —                                  | —                                   |

<!-- async-dsss-parallel:seed:end -->

<!-- async-dsss-parallel:seed-edge:start -->

| refine seed error, the sweep's arrival trials | 10 dB              | 5 dB               |
| --------------------------------------------- | ------------------ | ------------------ |
| largest seed error that locked                | 146.9 Hz (seed 10) | 239.1 Hz (seed 17) |
| draws at ≥ 250 Hz that locked                 | 0 of 0             | 0 of 0             |
| largest seed error, locked or not             | 146.9 Hz (seed 10) | 239.1 Hz (seed 17) |

<!-- async-dsss-parallel:seed-edge:end -->

### 6. What a receiver costs

The C benches, `jm_bench.h`'s wall clock over its rounds (min, mean and max
per row), each round a whole dwell (the engine) or a block of samples (the
receiver). The engine's work per 2,046-sample epoch depends on its tile count
and its coherent depth, not on the chip rate, so a row timed at another chip
rate is this design's state by its tiles: one tile is `acq::native` (3.069
Mcps, no Doppler search), ±1/T_E is the 3-tile `op5M_U5k`, and its slow-time
search is that row at D = 154, deeper than the 94 this design's 500 Hz/s
rate allows, so it bounds it. The engine rows are sized at the bench's own
C/N0, 37.31 dB-Hz (`bench_acq_core.c`), which sets their dwell, not their
cost per sample. The tracking row is the receiver bench's held cell receiver
at 1,023 chips, which that bench reads as the searching flavor's steady
state. The real-time factor is the mean per sample times 6.138 MSa/s; the
four-thread row is wall time, the CPUs it spends are more. The machine's
load either side of the bench is in the stamp.

<!-- async-dsss-parallel:cost:start -->

| receiver state                              | bench row                                        | ns a sample, mean (min–max) | × real time at 6.138 MSa/s |
| ------------------------------------------- | ------------------------------------------------ | --------------------------- | -------------------------- |
| tracking                                    | `async_dsss_receiver::steps[cell,sf=1023,5Mcps]` | 11.1 (11.0–11.6)            | 0.07                       |
| searching one tile, one-epoch looks         | `acq::native_nc90`                               | 22.4 (22.3–22.8)            | 0.14                       |
| searching ±1/T_E (3 tiles), one-epoch looks | `acq::op5M_U5k_nc225`                            | 36.6 (36.5–37.1)            | 0.22                       |
| searching ±1/T_E, slow time over D = 154    | `acq::op5M_U5k_D154_t1_nc1`                      | 88.0 (87.6–88.6)            | 0.54                       |
| the same, four threads                      | `acq::op5M_U5k_D154_t4_nc1`                      | 50.8 (43.6–56.8)            | 0.31 on four threads       |

<!-- async-dsss-parallel:cost:end -->

### 7. The pool as built

The same run's pool bench (twelve slots, one emitter tracked, the searcher's
31-epoch window) and the engine at the pool's design point of
async-dsss-receiver.md §6.1, ±50 kHz, at the ends of its chip-rate range: 21
tiles at 5 Mcps with D = 154, 53 at 2 Mcps with D = 61. Each is at the rate it
was timed at; a threaded row is wall time.

<!-- async-dsss-parallel:pool:start -->

| per push                                   | bench row                                        | timed at | ns a sample, mean (min–max) | × real time at that rate |
| ------------------------------------------ | ------------------------------------------------ | -------- | --------------------------- | ------------------------ |
| one receiver, tracking                     | `async_dsss_receiver::steps[cell,sf=1023,5Mcps]` | 10 MSa/s | 11.1 (11.0–11.6)            | 0.11                     |
| the pool, 12 slots, one emitter tracked    | `async_dsss_pool::push[12 slots, serial]`        | 10 MSa/s | 104.0 (44.8–959.9)          | 1.04                     |
| the same, across the machine's cores       | `async_dsss_pool::push[12 slots, all cores]`     | 10 MSa/s | 107.4 (44.1–1342.3)         | 1.07                     |
| its searcher at ±50 kHz, 21 tiles, D = 154 | `acq::op5M_U50k_D154_t1_nc1`                     | 10 MSa/s | 530.3 (526.8–537.5)         | 5.30                     |
| the same, eight threads                    | `acq::op5M_U50k_D154_t8_nc1`                     | 10 MSa/s | 141.2 (127.8–160.1)         | 1.41                     |
| its searcher at ±50 kHz, 53 tiles, D = 61  | `acq::op2M_U50k_D61_t1_nc1`                      | 4 MSa/s  | 2150.2 (2140.3–2180.4)      | 8.60                     |
| the same, four threads                     | `acq::op2M_U50k_D61_t4_nc1`                      | 4 MSa/s  | 605.8 (592.6–644.4)         | 2.42                     |

<!-- async-dsss-parallel:pool:end -->

The pool's mean push carries its searcher's dwells: its fastest push is the
receivers' work alone, and its slowest is a dwell (the min and max in the
row). The bench's rounds are not a whole number of dwells, so the mean is
approximate to within one dwell's share. So a mean near 1x real time does not
say the pool keeps up: between runs the mean moves by that share while
the fastest push, the receivers' own work, holds still (a same-host,
interleaved comparison across two earlier runs of this record moved it under
1%, #2484). Read the fastest push for the receivers' cost and the slowest for
a dwell's.
