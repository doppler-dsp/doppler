# Parallel Asynchronous-Data DSSS Receivers — the measurement record

*The dated record behind [the design page](async-dsss-parallel.md): what each
number there was measured by, with the run. Section numbers are shared, so a
`§3.1` cited on the design page, in an issue or in a code comment is the same
entry here. Both runs are of shipped tools, at one commit: the lock
validator, `native/validation/async_dsss_receiver_lock.c`, and the C benches
of the three objects. Every table below, and every number the design page
quotes from them, is rendered from their data, committed beside this page in
`async-dsss-parallel-measurements/` by its `record.py`, whose `--check` (in
`make docs-invariants`) fails when either page differs from the data.*

<!-- async-dsss-parallel:stamp:start -->

| run                        | when                       | machine                                                          | load before / after | commit             | command                                                                                                                         |
| -------------------------- | -------------------------- | ---------------------------------------------------------------- | ------------------- | ------------------ | ------------------------------------------------------------------------------------------------------------------------------- |
| lock validator, full sweep | 2026-10-17 18:37 UTC, 35 s | `swiftgo-ai9465`: AMD Ryzen AI 9 465 w/ Radeon 880M (20 threads) | 2.0 / 2.1           | `bc874bf75`, clean | `make build`, then `record.py measure`                                                                                          |
| C benches                  | 2026-10-17 18:36 UTC       | `swiftgo-ai9465`: AMD Ryzen AI 9 465 w/ Radeon 880M (20 threads) | 1.9 / 1.8           | `bc874bf75`, clean | `make bench BENCH_ARGS="--c-only acq async_dsss_receiver async_dsss_pool"` once the load fell under 2, rows kept by `record.py` |

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

The validator's `dropout` trials. Each draws its carrier over ±1/T_E (the
first run fixed it at 1,500 Hz, the edge between two of the search's tiles,
and lost more draws to it), and is fed until the receiver tracks with symbol
lock up for 200 epochs in a row, then up to 10 ms more, then cut at a random
sample of the next epoch. A draw that has not held lock that long within
4 s, or whose lock is down at the cut, is excluded and counted (§5.1 says
why there are some); trials are drawn until 20 settle, so the intervals'
fixed-n assumption is approximate. The receiver's state at the cut is saved
and every case resumes from it on the same noise, so they differ only in
the dropout: the validator checks that the restore is exact, every case's
lock reading the for-good case's until its signal returns. Symbol lock is
read after every epoch, so every time is to 0.33 ms. The receiver stays
tracking throughout (`lost_confirm_s` = 0): a lock that falls comes back by
itself.

<!-- async-dsss-parallel:dropout:start -->

| Es/N0 | carriers drawn | draws settled: lock held 200 epochs within 4 s, up at the cut | lock falls after a loss, min / median / max | a 50 ms dropout: fell; back within 300 ms; min / median / max                | a 200 ms dropout: the same                                                   | data flips at the cut |
| ----- | -------------- | ------------------------------------------------------------- | ------------------------------------------- | ---------------------------------------------------------------------------- | ---------------------------------------------------------------------------- | --------------------- |
| 10 dB | -2892–+2821 Hz | 20 of 21 (0.77–0.99)                                          | 13.4 / 22.8 / 33.9 ms                       | fell 20 of 20 (0.84–1.00); back 20 of 20 (0.84–1.00); 19.4 / 26.2 / 48.2 ms  | fell 20 of 20 (0.84–1.00); back 20 of 20 (0.84–1.00); 19.6 / 38.7 / 53.2 ms  | 0.46–0.53             |
| 5 dB  | -2892–+2821 Hz | 20 of 28 (0.53–0.85)                                          | 14.6 / 18.0 / 30.8 ms                       | fell 20 of 20 (0.84–1.00); back 20 of 20 (0.84–1.00); 21.3 / 65.3 / 108.1 ms | fell 20 of 20 (0.84–1.00); back 18 of 20 (0.70–0.97); 29.4 / 68.1 / 141.3 ms | 0.45–0.53             |

<!-- async-dsss-parallel:dropout:end -->

**The hold, by dropout length.** Each trial is run through a dropout of every
length from one epoch to two past its own drop, each from the saved state:
held means symbol lock stayed up through the dropout and 60 ms after it.

<!-- async-dsss-parallel:hold:start -->

| Es/N0 | draws excluded: not settled | held 2 ms            | held 5 ms            | held 10 ms           | held 15 ms           | held 20 ms          | held 25 ms          | held 30 ms          | trials where a longer one held after a shorter fell |
| ----- | --------------------------- | -------------------- | -------------------- | -------------------- | -------------------- | ------------------- | ------------------- | ------------------- | --------------------------------------------------- |
| 10 dB | 1 of 21 (0.01–0.23)         | 20 of 20 (0.84–1.00) | 20 of 20 (0.84–1.00) | 17 of 20 (0.64–0.95) | 13 of 20 (0.43–0.82) | 7 of 20 (0.18–0.57) | 2 of 20 (0.03–0.30) | 0 of 20 (0.00–0.16) | 7                                                   |
| 5 dB  | 8 of 28 (0.15–0.47)         | 20 of 20 (0.84–1.00) | 20 of 20 (0.84–1.00) | 12 of 20 (0.39–0.78) | 3 of 20 (0.05–0.36)  | 1 of 20 (0.01–0.24) | 0 of 20 (0.00–0.16) | 0 of 20 (0.00–0.16) | 5                                                   |

<!-- async-dsss-parallel:hold:end -->

The hold is not one number per draw. In many trials a longer dropout held
after a shorter one fell (the last column): the lock average, an exponential
average over 30 symbols, sinks under the detector's drop level in the
dropout and climbs back too late after it, and where it lands depends on the
noise of those few milliseconds. So the hold is reported as a fraction per
length, never as an edge. The first sweep also asserted that a dropout of
`LOCK_N_DOWN` symbols cannot drop the lock, which reads like a property of
the detector; some 5 dB trials held less, the low run supplied by the
recovery after the dropout, and the assertion went.

### 5.1 Arrival to tracking, and to symbol lock

The validator's `arrival` trials: noise alone for a uniform 0 to 20 ms, so
the signal starts at any sample of an epoch and any code phase against the
feed, then the signal at a carrier uniform over ±1/T_E, for 2 s after it
arrives. Times are from the arrival sample, to the first epoch the receiver
reads tracking and to the first after which symbol lock is up.

<!-- async-dsss-parallel:arrival:start -->

| Es/N0 | tracking: draws; min / median / max after arrival | symbol lock: draws; min / median / max          | locked 2 s after arrival | data flips |
| ----- | ------------------------------------------------- | ----------------------------------------------- | ------------------------ | ---------- |
| 10 dB | 20 of 20 (0.84–1.00); 43.5 / 44.6 / 47.3 ms       | 20 of 20 (0.84–1.00); 57.9 / 86.2 / 698.0 ms    | 20 of 20 (0.84–1.00)     | 0.47–0.56  |
| 5 dB  | 20 of 20 (0.84–1.00); 79.4 / 86.3 / 100.8 ms      | 15 of 20 (0.53–0.89); 124.9 / 379.8 / 1768.0 ms | 15 of 20 (0.53–0.89)     | 0.47–0.56  |

<!-- async-dsss-parallel:arrival:end -->

**What decides the lock: the refine stage's carrier.** The same trials, by
how far the refine stage's carrier estimate, read when the receiver first
tracks (`get_doppler_hz()`, frozen from then), lands from the truth: the
residual the carrier loop must pull in. That loop's acquisition bound is
`bn / m`, 60 Hz at this front end, and `costas_pullin.c` measures it acquiring
every draw inside the bound, still reliably to twice it, and dead by four
times (`async_dsss_receiver_core.h`); doppler#982 found the refine handing
over outside it. The table shows the same soft edge on the whole receiver:
lock time grows with the seed error, and the large errors do not lock.

<!-- async-dsss-parallel:seed:start -->

| refine seed error | locked, 10 dB                      | locked, 5 dB                       |
| ----------------- | ---------------------------------- | ---------------------------------- |
| 0–100 Hz          | 15 of 15 (0.80–1.00), median 67 ms | 6 of 6 (0.61–1.00), median 151 ms  |
| 100–200 Hz        | 3 of 3 (0.44–1.00), median 143 ms  | 5 of 5 (0.57–1.00), median 419 ms  |
| 200–300 Hz        | 2 of 2 (0.34–1.00), median 599 ms  | 4 of 7 (0.25–0.84), median 1444 ms |
| ≥ 300 Hz          | —                                  | 0 of 2 (0.00–0.66)                 |

<!-- async-dsss-parallel:seed:end -->

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
| tracking                                    | `async_dsss_receiver::steps[cell,sf=1023,5Mcps]` | 11.3 (11.1–11.6)            | 0.07                       |
| searching one tile, one-epoch looks         | `acq::native_nc90`                               | 22.4 (22.3–22.7)            | 0.14                       |
| searching ±1/T_E (3 tiles), one-epoch looks | `acq::op5M_U5k_nc225`                            | 36.5 (36.4–36.6)            | 0.22                       |
| searching ±1/T_E, slow time over D = 154    | `acq::op5M_U5k_D154_t1_nc1`                      | 88.1 (87.8–88.5)            | 0.54                       |
| the same, four threads                      | `acq::op5M_U5k_D154_t4_nc1`                      | 49.0 (44.1–57.3)            | 0.30 on four threads       |

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
| one receiver, tracking                     | `async_dsss_receiver::steps[cell,sf=1023,5Mcps]` | 10 MSa/s | 11.3 (11.1–11.6)            | 0.11                     |
| the pool, 12 slots, one emitter tracked    | `async_dsss_pool::push[12 slots, serial]`        | 10 MSa/s | 108.0 (44.9–1022.8)         | 1.08                     |
| the same, across the machine's cores       | `async_dsss_pool::push[12 slots, all cores]`     | 10 MSa/s | 97.9 (45.6–850.9)           | 0.98                     |
| its searcher at ±50 kHz, 21 tiles, D = 154 | `acq::op5M_U50k_D154_t1_nc1`                     | 10 MSa/s | 526.9 (526.0–527.9)         | 5.27                     |
| the same, eight threads                    | `acq::op5M_U50k_D154_t8_nc1`                     | 10 MSa/s | 127.2 (124.0–132.1)         | 1.27                     |
| its searcher at ±50 kHz, 53 tiles, D = 61  | `acq::op2M_U50k_D61_t1_nc1`                      | 4 MSa/s  | 2154.0 (2147.6–2162.3)      | 8.62                     |
| the same, four threads                     | `acq::op2M_U50k_D61_t4_nc1`                      | 4 MSa/s  | 602.9 (592.5–662.9)         | 2.41                     |

<!-- async-dsss-parallel:pool:end -->

The pool's mean push carries its searcher's dwells: its fastest push is the
receivers' work alone, and its slowest is a dwell (the min and max in the
row). The bench's rounds are not a whole number of dwells, so the mean is
approximate to within one dwell's share.
