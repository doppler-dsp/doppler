# The acquisition surface — the measurement record

*The dated record behind [the design page](acq-surface.md). The design page
states what is; this page is the runs behind it, under the same section
numbers. Nothing is rewritten after the fact: a later entry corrects an
earlier one in its own words. The data and the harnesses are committed
beside this page, in `acq-surface-measurements/`.*

## 18. One delay-Doppler map

*The runs behind §18 of the design page, 2026-11-04, on the bench box
(`cachyos-x8664-ai465`: AMD Ryzen AI 9 465, 20 threads, CachyOS, GCC 16.2.1),
at main `df7ce744c`, a Release build, 1-minute load under 1.0 before each
run.*

### 18.1 Slow time first, against today's two paths

`proto-2732/shared_slow.c` builds the tiled surface both ways on the
library's own FFT, linked against that commit's `libdoppler.a`, single
threaded and pinned to one of the fastest cores (`taskset -c 2`), best of 7:

- **today's wideband path:** an inverse at $L$ for each tile and epoch, then
    the slow-time transform per tile and lag column, chunked as
    `acq_tile_block` is;
- **today's narrow path** (one tile): slow time first on the raw epochs,
    then a forward at $N$ and a padded inverse for each of the $DI$ rows, as
    `acq_make_surface` does through Corr2D's fast path;
- **slow time first:** one slow-time transform per capture bin, shared by
    every tile, then an inverse per tile and slow row, plus the $D$ forward
    transforms its input needs.

$N = 2046$ (a 1,023-chip code at two samples a chip), $L = 2048$, $I = 2$.

| geometry                   | today    | slow time first | ratio | rows apart, relative |
| -------------------------- | -------- | --------------- | ----- | -------------------- |
| 21 tiles, $D = 150$        | 78.73 ms | 41.03 ms        | 0.521 | $1.8 \times 10^{-7}$ |
| 53 tiles, $D = 60$         | 71.50 ms | 39.82 ms        | 0.557 | $2.0 \times 10^{-7}$ |
| 3 tiles, $D = 150$         | 11.25 ms | 7.87 ms         | 0.700 | $1.8 \times 10^{-7}$ |
| 1 tile, $D = 150$ (narrow) | 7.29 ms  | 5.95 ms         | 0.816 | —                    |
| 1 tile, $D = 10$ (narrow)  | 0.43 ms  | 0.32 ms         | 0.750 | —                    |

The last column is the largest cell difference over the largest cell, for
the same random input: the reordering moves the surface by float rounding
($1.7 \times 10^{-7}$ of its RMS in every case). The one-tile rows compare
against the narrow path that runs today. Against the wideband path's order
at one tile (3.50 ms against 3.98 ms, $D = 150$, both without the forward
transforms), slow time first costs about 8% more, because it does $DI$
inverses against $D$.

<!-- docs-snippet: no-exec=needs a Release build/libdoppler.a and a pinned core on the bench box -->

```sh
cc -O3 -std=c99 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Inative/inc \
  docs/design/acq-surface-measurements/proto-2732/shared_slow.c \
  build/libdoppler.a -lm -lpthread -o shared_slow
taskset -c 2 ./shared_slow 2046 2048 150 2 21 7   # N L D I W reps
```

An earlier run on the WSL2 dev box (`zen-x2elite-x`) read 0.38, 0.44 and
0.57 for the first three geometries: the same direction, larger, because
the inverse at 2048 is relatively cheaper there.

### 18.2 The code-rate ramp on a shared transform

`proto-2732/ramp_grid.c` builds one tile's epochs as a code dilated by
$d$ samples an epoch, which today's per-epoch ramp cancels exactly, and
reads the shared transform on a grid $F$ times finer, rounding each bin's
shift. The loss is the shared map's peak against the exact one, over nine
slow-time tones from one row's center to the next. $N = 2046$, $D = 150$,
$I = 2$, $d = 0.0455$ (tile 10 at 10 MSa/s and a 2.2 GHz carrier, a shift of
up to 6.8 rows).

| $F$ | worst loss | mean loss | shared slow time, one tile's $N$ bins |
| --- | ---------- | --------- | ------------------------------------- |
| 1   | 0.433 dB   | 0.330 dB  | 2.11 ms                               |
| 2   | 0.402 dB   | 0.305 dB  | 5.02 ms                               |
| 4   | 0.048 dB   | 0.00 dB   | 5.85 ms                               |
| 8   | 0.040 dB   | 0.039 dB  | 12.30 ms                              |

The rounding error does not grow with the shift, only with $1/F$, so a
larger dilation needs no finer grid. With $d = 0$, $F = 1$ reads 0.000 dB.

### 18.3 The baseline: main against itself

`make bench-ab A=df7ce744c B=df7ce744c K=5 BENCH_ARGS="--c-only <component>"`
on the bench box, one run per component (acq, the pool, Corr2D), portable
build, pinned to the fastest cores. These are the numbers step 2.3 must
meet. The band is the larger side's second-best pass minus its best. The
JSON of every run is in `baseline-df7ce744c/`.

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
by 1.1–2.9%, and tied on a second run (`baseline-corr2d-r2.json`); that
second run started while the box's load was still settling from the first.
So a single flag on an all-core row is noise, and the speed gate counts a
row only when the same row is flagged twice (§18.4 of the design page).
`op5M_U5k_D150_t4_nc1` tied only because one pass made its band 4.4%.
