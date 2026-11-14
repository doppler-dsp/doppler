---
description: "the dated runs behind the correlator design page under the same section numbers: §9's stage costs, bytes a sample against the triad ceiling, thread scaling, and the levers measured and not taken"
---

# The correlator — the measurement record

*The dated record behind [the design page](correlator.md). The design page
states what is; this page is the runs behind it, under the same section
numbers. Nothing is rewritten after the fact: a later entry corrects an
earlier one in its own words. The harnesses and their raw output are
committed beside this page, in `correlator-measurements/`.*

## 9. Cost and parallelism

*[#3047](https://github.com/doppler-dsp/doppler/issues/3047), 2026-11-14.
Unless a run names another box, it ran on the x86 dev box
(`omni-x7358h`: Intel Core Ultra X7 358H, 16 threads, WSL2, GCC), against
the Release `libdoppler.a` and the Python extension of the commit it names.
The design point is #2457's: 3.069 Mcps, 2 samples a chip, a 1023-chip
code, ±3 kHz, $D = 90$, noise input; "wide" is the same at ±15 kHz, 11
tiles.*

### 9.1 Where a sample's time goes (main `ae9493608`)

`probe.py` (the #2457 cost probe on `Correlator`) at one thread, profiled
with `py-spy --native` and aggregated by leaf frame. Shares of
`dp__search_push`:

| stage                                                             |   x86 | arm64 (`zen-x2elite-x`, Snapdragon X2) |
| ----------------------------------------------------------------- | ----: | -------------------------------------: |
| ns a sample                                                       |  38.1 |                                   24.6 |
| all FFT                                                           | 59.6% |                                  59.1% |
| the inverse of each correlation row (pffft)                       | 23.6% |                                  15.3% |
| the forward of each 2,046-sample epoch (pocketfft)                | 20.0% |                                  23.7% |
| ↳ the generic pass for the prime factor 31                        |  8.3% |                                  13.2% |
| slow time down each column, 180 points (#3047 said 90; see below) | 15.8% |                                  20.1% |
| everything else                                                   |  ~40% |                                   ~41% |

Thread scaling then, when the pool was capped at one worker a tile (3):

| threads                             |    1 |    2 |    3 |    4 |    8 |
| ----------------------------------- | ---: | ---: | ---: | ---: | ---: |
| x86                                 | 38.1 | 21.5 |    — | 15.8 | 14.0 |
| arm64, best of 3 interleaved rounds | 24.4 | 23.3 | 17.6 | 17.4 | 17.3 |

On arm64 two threads gained nothing; not explained.

The slow-time transform is $F\,D\,I$ points: $D = 90$, interpolation 2,
and $F = 1$ with no carrier set, so 180. #3047's Phase 0 table labelled it
90 (the depth), and a later comment 720 (with a carrier, $F = 4$).

**FFT sizes.** A C microbenchmark of the `fft` object (`dp_fft_create`) against
`build/libdoppler.a`: a 180-point transform (D = 90, interpolation 2) took
0.546 µs, 160 points 0.161 µs, 192 points 0.197 µs. Every length outside
pffft's set (a multiple of 16, factors 2, 3 and 5) is promoted to double.
A 2,048-point forward is 5.2× faster than the 2,046-point one.

That microbenchmark was not committed. `bench_blue.c` times the same `fft`
object, and its run on main `b45724d44` (two rounds, `run-b45724d44.txt`)
gives 180 points 0.574 and 0.578 µs, 160 points 0.171 and 0.172, 192 points
0.208 and 0.206, and 2,046 against 2,048 points 15.06 against 2.89 µs
(5.2×) and 15.64 against 2.87 (5.4×).

### 9.2 Bytes against bandwidth (main `b45724d44`)

`stage.c` times the map's two stages alone (the forward transforms, then
slow time and the rows) at three and eleven tiles, $D = 90$, $F = 4$, and
counts each stage's buffers read and written once. `triad.c` is a
STREAM-style triad over three 256 MB float arrays. Raw output:
`correlator-measurements/run-b45724d44.txt`.

| tiles | threads | forward, ns a sample | map, ns a sample | map, GB/s | logical B a sample (fwd + map) |
| ----: | ------: | -------------------: | ---------------: | --------: | -----------------------------: |
|     3 |       1 |                 7.16 |            35.39 |       3.4 |                       16 + 120 |
|     3 |      16 |                 1.55 |             4.77 |      25.2 |                                |
|    11 |       1 |                 7.62 |           135.01 |       3.3 |                       16 + 440 |
|    11 |      16 |                 1.43 |            15.84 |      27.8 |                                |

Triad, the same box and run: 31.6 GB/s at one thread, 52.9 at 2, 71.4 at 4,
74.3 at 8, 77.3 at 16.

The other boxes' triad, 2026-11-14 (main `ae9493608`'s harness, the same
source):

| box                                                |  1 thread | all threads                                       |
| -------------------------------------------------- | --------: | ------------------------------------------------- |
| `omni-x7358h` (Core Ultra X7 358H, 16 threads)     | 32.5 GB/s | 81.3 GB/s                                         |
| `zen-x2elite-x` (Snapdragon X2, 18 threads)        | 64.2 GB/s | 116.6 GB/s                                        |
| `hp-spark` (GB10, 20 threads)                      | 35.2 GB/s | 128.0 GB/s                                        |
| `cachyos-x8664-ai465` (Ryzen AI 9 465, 20 threads) | 41.1 GB/s | ~58 GB/s, peaking at 2–4 threads; possibly loaded |

### 9.3 Thread scaling after the split (main `b45724d44`)

`probe.py`, best of five pushes, one run each (`run-b45724d44.txt`):

| threads |     1 |    2 |    4 |    8 |   16 |
| ------- | ----: | ---: | ---: | ---: | ---: |
| design  |  38.7 | 21.2 | 13.2 |  8.5 |  8.4 |
| wide    | 100.2 | 54.7 | 33.7 | 22.0 | 16.0 |

Rerun of the design point, best of fifteen pushes, three rounds: 8 threads
8.9, 8.7, 8.6; 16 threads 6.7, 6.5, 7.4. The design page quotes the best
of the four 16-thread runs, 6.5.

The step from the capped pool, measured at the PR (#3048, #3050, landed as
train #3052) with `make bench-ab A=ae9493608 B=e7bd8e9ad`, K = 5, the
correlator rows: 11 faster, 45 tied, 0 slower over a run and its confirm;
`op5M_U5k_D150_t4` 0.853, `op5M_U50k_D150_t8` 0.966. Splitting the decide
pass at a fixed 8 chunks a tile cost 1.6% at one thread on 53 tiles (445.8
against 438.7 ns a sample), which the 24-chunk target removed.

### 9.4 Levers measured and not taken

- **Bluestein over pffft** (main `ae9493608`): 0.85× the current path at
    2,046 points, 0.39× at 90. The committed `bench_blue.c` then ran
    neither length; with them added, on main `b45724d44` (two rounds,
    `run-b45724d44.txt`): 0.84× and 0.87× at 2,046, 0.37× and 0.38× at 90.
- **Slow-time depth 80:** about 10% of the search for 0.5 dB of coherent
    gain, from §9.1's size table.
- **The unordered pffft inverse:** the row product gathered in pffft's
    internal order (a table built from `pffft_zreorder`, 4-bin re/im blocks),
    bit-identical; 29.98 against 29.60 ns a sample, best of 8 interleaved
    runs. Not committed.
- **`-march=native`:** 7% at one thread, 0 at 16 (pffft has no AVX path).

### 9.5 The open levers' evidence (main `ae9493608`, py-spy)

- **The decide pass fused into the rows:** a timing hack during #3048's work, 16 threads 6.0–6.5 against 6.8–7.3 ns a sample; no
    change at one thread.
- **Copies:** `memcpy` (input to the framer's ring, then to `blk_raw`) was
    13% of the 16-thread critical path.
- **Pool waits:** `pthread_cond_broadcast` and `wait` were about 18% at 16
    threads before each stage cut 4 chunks a worker.
