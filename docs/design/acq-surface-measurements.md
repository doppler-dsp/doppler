<!-- domain: acquire -->

# The acquisition surface — the measurement record

*The dated record behind [the design page](acq-surface.md). The design page
states what is; this page is the runs behind it, under the same section
numbers. Nothing is rewritten after the fact: a later entry corrects an
earlier one in its own words. The data and the harnesses are committed
beside this page, in `acq-surface-measurements/`.*

## 18. One delay-Doppler map

*The runs behind §18 of the design page, 2026-11-04. §18.1 and §18.2 ran on
the bench box (`cachyos-x8664-ai465`: AMD Ryzen AI 9 465, 20 threads,
CachyOS, GCC 16.2.1), against main `df7ce744c`'s Release `libdoppler.a`,
after its 1-minute load had fallen under 1.0. §18.4 ran on the WSL2 dev box,
because a verdict, unlike a time, does not depend on the machine.*

### 18.1 Slow time first, against today's two paths

`proto-2732/shared_slow.c` builds the tiled surface both ways on the
library's own FFT, single-threaded and pinned to one of the fastest cores,
best of 7. `proto-2732/run.sh` runs every case below, and its output, the
source of every number in §18.1 and §18.2, is `proto-2732/proto-out.txt`.

- **Today's wideband path:** an inverse at $L$ for each tile and epoch, each
    product ramped exactly when a carrier is set, as `acq_ramp` does; then
    the slow-time transform per tile and lag column, chunked as
    `acq_tile_block` is.
- **Today's narrow path** (one tile): slow time first on the raw epochs,
    then a forward at $N$ and a padded inverse for each of the $DI$ rows, as
    `acq_make_surface` does through Corr2D's fast path.
- **Slow time first:** one slow-time transform per capture bin on $F \cdot DI$
    points, stored bin by bin and shared by every tile; then, per tile, the
    product gathered eight bins at a time at each bin's fine row, and an
    inverse per slow row. Against the narrow path it also pays the $D$
    forward transforms its input needs.

$N = 2046$ (a 1,023-chip code at two samples a chip), $L = 2048$, $I = 2$.
A carrier sets tile $r$'s dilation to $r$ times 0.004545 samples an epoch
(10 MSa/s at 2.2 GHz), or 0.001818 for the 4 MSa/s geometry.

| geometry                   | today, no carrier | ratio, no carrier | today, carrier | ratio, carrier ($F = 4$) |
| -------------------------- | ----------------- | ----------------- | -------------- | ------------------------ |
| 21 tiles, $D = 150$        | 79.21 ms          | 0.418             | 90.66 ms       | 0.478                    |
| 53 tiles, $D = 60$         | 71.41 ms          | 0.443             | 83.99 ms       | 0.391                    |
| 3 tiles, $D = 150$         | 11.31 ms          | 0.612             | 12.37 ms       | 0.869                    |
| 3 tiles, $D = 10$          | 0.43 ms           | 0.838             | 0.50 ms        | 0.908                    |
| 1 tile, $D = 150$ (narrow) | 7.26 ms           | 0.786             | —              | —                        |
| 1 tile, $D = 10$ (narrow)  | 0.43 ms           | 0.692             | —              | —                        |

The ratio is slow time first over today. One tile has no ramp (tile 0's
dilation is 0), so a carrier changes nothing there. Against the wideband
path's order at one tile, slow time first costs 6% more at $D = 150$
(3.74 ms against 3.52) and 23% more at $D = 10$ (0.17 ms against 0.14), both
without the forward transforms.

**Equivalence.** With no carrier, the two surfaces differ by float
rounding: the largest cell difference is $1.8$–$2.1 \times 10^{-7}$ of the
largest cell, and the RMS difference $1.7 \times 10^{-7}$ of the RMS. Solved
for the constant of the bound decided for #2752,
$|\Delta| \le c\,\varepsilon_{f32}\,(\log_2 N + \log_2 L + \log_2 DI)\,\sqrt{D}\,\lVert x\rVert_2 \lVert r\rVert_2$,
over 5 seeds at 21 tiles $D = 150$, 53 tiles $D = 60$ and 3 tiles $D = 10$,
$c$ is 0.73–0.84, 1.32–1.39 and 2.67–3.04: $c = 4$ holds every scene. With a
carrier the surfaces differ by the ramp's rounding (§18.2), measured as a
loss, not a bound; at a dilation of $10^{-12}$, where the rounding vanishes,
$F = 4$ matches today to $1.8 \times 10^{-7}$ (a check run on the dev box).

**What the prototype got wrong first, kept because it is the cost.** The
first carrier gather read each slow row's bins straight from a 20 MB
row-major grid with two integer divisions an element: on the bench box it
ran at 0.97, 0.92, 1.35 and 1.73 of today in the four carrier rows. Storing
the transform bin by bin, gathering eight bins at a time and wrapping the
row with a compare gave the table above. Sharing that one gather with the
no-carrier case took its ratios from 0.52, 0.56, 0.70 and 0.99 to the
values above.

<!-- docs-snippet: no-exec=needs a Release build/libdoppler.a and a pinned core on the bench box -->

```sh
# on the bench box, from a checkout with a Release build/libdoppler.a
bash docs/design/acq-surface-measurements/proto-2732/run.sh
```

### 18.2 The code-rate ramp on a shared transform

`proto-2732/ramp_grid.c` builds one tile's epochs as a code dilated by $d$
samples an epoch, which today's per-epoch ramp cancels exactly, and reads
the shared transform on a grid $F$ times finer, rounding each bin's shift.
The loss is the shared map's peak against the exact one, in dB, the worst
and the mean over 33 slow-time tones from one row's center to the next.
$N = 2046$, $D = 150$, $I = 2$. The transform's origin is the block's
middle; the last block is the origin at epoch 0, for comparison.

| $d$ (samples an epoch) | largest shift | $F$ | worst loss | mean loss | slow time, $N$ bins |
| ---------------------- | ------------- | --- | ---------- | --------- | ------------------- |
| 0.0045                 | 0.68 rows     | 1   | 0.582 dB   | 0.341 dB  | 2.13 ms             |
|                        |               | 2   | 0.198 dB   | 0.024 dB  | 4.97 ms             |
|                        |               | 3   | 0.076 dB   | 0.025 dB  | 8.08 ms             |
|                        |               | 4   | 0.076 dB   | 0.011 dB  | 5.77 ms             |
|                        |               | 8   | 0.026 dB   | 0.003 dB  | 12.12 ms            |
| 0.0455                 | 6.83 rows     | 1   | 0.400 dB   | 0.287 dB  | 2.10 ms             |
|                        |               | 2   | 0.110 dB   | 0.072 dB  | 4.99 ms             |
|                        |               | 3   | 0.064 dB   | 0.021 dB  | 8.06 ms             |
|                        |               | 4   | 0.080 dB   | 0.015 dB  | 5.77 ms             |
|                        |               | 8   | 0.006 dB   | 0.005 dB  | 12.15 ms            |
| 0.2                    | 30.0 rows     | 1   | 0.398 dB   | 0.311 dB  | 2.13 ms             |
|                        |               | 2   | 0.071 dB   | 0.053 dB  | 4.97 ms             |
|                        |               | 3   | 0.088 dB   | 0.021 dB  | 8.00 ms             |
|                        |               | 4   | 0.086 dB   | 0.023 dB  | 5.80 ms             |
|                        |               | 8   | 0.014 dB   | 0.004 dB  | 12.13 ms            |
| 0.0455, origin epoch 0 | 6.83 rows     | 1   | 0.445 dB   | 0.339 dB  | 2.12 ms             |
|                        |               | 2   | 0.422 dB   | 0.315 dB  | 4.96 ms             |
|                        |               | 4   | 0.060 dB   | 0.001 dB  | 5.82 ms             |
|                        |               | 8   | 0.040 dB   | 0.039 dB  | 12.14 ms            |

A negative per-tone loss, where the rounded read lands nearer a tone's peak
than the exact row does, is counted as it is, so a mean can sit under the
worst by more than the spread suggests. The origin at epoch 0 leaves a phase
error of $2\pi c$ times each bin's rounding, which is what held $F = 8$ at
0.04 dB there. The earlier 9-tone table this replaces averaged ratios, not
dB, which is why its mean was not monotone in $F$.

### 18.3 The baseline: main against itself

`make bench-ab A=df7ce744c B=df7ce744c K=5 BENCH_ARGS="--c-only <component>"`
on the bench box, one run per component (acq, the pool, Corr2D), portable
build, pinned to the fastest cores, each started once the box's 1-minute
load was under 1.0. These are the numbers step 2.3 must meet. The band is
the larger of the two sides' gaps between their second-best and best pass;
the verdict's tolerance is the band or 1% of A's best, whichever is larger.
The JSON of every run is in `baseline-df7ce744c/`.

| row                                  | best of 5 (µs) | band  | A against A      |
| ------------------------------------ | -------------- | ----- | ---------------- |
| `native_nc90`                        | 2508.6         | 0.64% | TIE              |
| `op2M_U50k_D60_t1_nc1`               | 82348.9        | 0.05% | TIE              |
| `op2M_U50k_D60_t4_nc1`               | 25009.4        | 0.31% | TIE              |
| `op2M_U50k_nc57`                     | 19437.4        | 0.11% | TIE              |
| `op2M_U5k_nc51`                      | 2878.0         | 0.20% | TIE              |
| `op5M_U50k_D150_t1_nc1`              | 88158.1        | 0.15% | TIE              |
| `op5M_U50k_D150_t8_nc1`              | 28952.4        | 0.74% | TIE              |
| `op5M_U50k_nc254`                    | 36360.6        | 0.05% | TIE              |
| `op5M_U50k_t8_nc254`                 | 14962.2        | 0.29% | TIE              |
| `op5M_U5k_D150_t1_nc1`               | 15804.9        | 0.59% | TIE              |
| `op5M_U5k_D150_t4_nc1`               | 9100.3         | 4.39% | TIE              |
| `op5M_U5k_nc225`                     | 7060.4         | 0.07% | TIE              |
| `push[full,n=1,threads=1]`           | 89910.4        | 0.46% | TIE              |
| `push[full,n=4,threads=1]`           | 89886.9        | 0.66% | TIE              |
| `push[full,n=4,threads=4]`           | 50009.6        | 0.53% | TIE              |
| `push[native,n=1,threads=1]`         | 12878.8        | 0.17% | TIE              |
| `push[native,n=4,threads=1]`         | 12904.1        | 0.10% | TIE              |
| `push[native,n=4,threads=4]`         | 12886.8        | 0.08% | TIE              |
| `bank_64x512_c12_corrected_batch_1t` | 1685.9         | 0.68% | TIE, then TIE    |
| `bank_64x512_c12_l4_1t`              | 1365.8         | 0.17% | TIE, then TIE    |
| `bank_64x512_c6_l2_1t`               | 676.2          | 0.14% | TIE, then TIE    |
| `bank_64x512_detect_l2_l4_1t`        | 835.5          | 0.60% | TIE, then TIE    |
| `batch_64x512_h48_all_1t`            | 7014.1         | 0.37% | TIE, then TIE    |
| `batch_64x512_h48_all_all`           | 2134.3         | 0.12% | SLOWER, then TIE |
| `batch_64x512_h48_execute_loop_1t`   | 7160.1         | 0.39% | TIE, then TIE    |
| `batch_64x512_h48_execute_loop_all`  | 2179.7         | 0.77% | SLOWER, then TIE |
| `batch_64x512_h48_power_max_1t`      | 6747.7         | 0.11% | TIE, then TIE    |
| `batch_64x512_h48_power_max_all`     | 2234.0         | 0.41% | TIE, then TIE    |
| `delay_doppler_256x4096_1t`          | 6420.5         | 0.41% | TIE, then TIE    |
| `delay_doppler_256x4096_all`         | 1617.4         | 1.61% | SLOWER, then TIE |
| `delay_doppler_32x1024_1t`           | 136.6          | 0.51% | TIE, then TIE    |
| `multi_row_general_path`             | 538.1          | 0.22% | TIE, then FASTER |
| `single_row_fast_path`               | 429.6          | 0.06% | TIE, then TIE    |

Three Corr2D rows that use every core read SLOWER against the same commit,
by 1.1–2.9%, and tied on a second run (`baseline-corr2d-r2.json`). That
second run, unlike the first three, was started without the load wait, at
a 1-minute load of 4.5 from the runs before it.
So a single flag on an all-core row is noise, and the speed gate counts a
row only when the same row is flagged twice (§18.4 of the design page).
`op5M_U5k_D150_t4_nc1` tied only because one pass made its band 4.4%.

### 18.4 The verdicts with the rounded ramp

`emul-2752/rounded-ramp.patch` (a scratch build on #2815's branch,
`bd58be73c`, not merged) rounds `acq_tile_epoch`'s ramp to the grid the
shared map reads, $F = $ `ACQ_EMUL_F`, with the slow-time shift of each bin
$d_q j'/N$ replaced by `round(F DI d_q j'/N) / (F DI)` about the block's
middle, so the engine computes what the map will. The five validators that
set a carrier, each run unchanged (`ACQ_EMUL_F` unset) and at $F = 4$ through
`make test-sweep CTEST="ctest -V -R ^<name>$"`; their output, less the
harness lines, is in `emul-2752/<name>-exact.log` and `-rounded.log`.

| validator                     | unchanged | $F = 4$ | output                                                                                                                                                           |
| ----------------------------- | --------- | ------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `acq_block_coherent`          | pass      | pass    | 24 lines differ: the compensated rows' statistic 1457 → 1451 at $D = 150$ and 237 → 236 at $D = 16$; the row's worst −3.61 → −3.65 dB, its mean −2.45 → −2.48 dB |
| `refine_bias`                 | pass      | pass    | only the timing line differs: it never ramps                                                                                                                     |
| `async_dsss_receiver_release` | pass      | pass    | identical                                                                                                                                                        |
| `tracker_through_window`      | pass      | pass    | identical                                                                                                                                                        |
| `rx_dynamics`                 | pass      | pass    | the MPSK receiver: it never calls acq. Labelled `threaded`, not `sweep`, so it runs in `make test`, not `make test-sweep`                                        |

The identical ones never ramp: their search has one tile or $D = 1$. So
among the validators the carrier verdict rests on `acq_block_coherent`.

The C suites, on the same build at $F = 4$ through
`make test CTEST_SELECT="-R ^<name>$"`, widen it:

| suite                           | $F = 4$                                                                                                                                                 |
| ------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `test_acq_core`                 | every check passes, the tiled carrier cases included, but one: the #2815 pin of `block_prompt`'s rows against the exact ramp, which the map keeps exact |
| `test_async_dsss_receiver_core` | pass                                                                                                                                                    |
| `test_async_dsss_pool_core`     | pass                                                                                                                                                    |
| `test_burst_capture_core`       | pass                                                                                                                                                    |
