# A: notes for the Acquisition & DSSS rows in A.tsv

Read at `/home/matt/doppler/.claude/worktrees/finisher-acq` (decdcda31). All
paths below are relative to that checkout. Nothing was built or run; numbers
quoted from the snapshot come from `benchmarks/published/v0.72.0/`
(`portable-c.json` is what the page shows for C rows, `stats.mean / stats.iterations`; Python rows read `extra_info.MSa_s`, per
`scripts/bench_report.py:791-830`).

Outcome: **12 KEEP, 27 CUT** of 39 rows. Labels for the 12 kept rows are in
`A.toml` beside this file (one of them, `c:acq::native_nc256`, a REDRAFT of
the existing headline label).

______________________________________________________________________

## KEEP / CUT

The rules applied: KEEP only a row that answers a question a user or
maintainer asks (can it keep up at a realistic operating point; what does a
knob that changes cost per sample cost) and whose cost differs from every
kept sibling. A sweep over a knob that CAN change cost per sample (thread
count, zero-pad, block or grid size, Doppler window count) keeps its two
ends and cuts the points between, even where the ends read alike; a knob
that cannot (non-coherent epochs `nc`, the detection target behind it,
signal-vs-noise input at a fixed block) keeps one row. A hollow or
miscounted row whose honest twin exists is cut, except where it is its
bench's only row. Every CUT names the kept row that covers it.

### acq (C, `bench_acq_core.c`): 6 of 27 kept

The searcher's cost is per epoch and per frequency window, so:

- **the epoch count `ncN` does not change cost per sample.** Each epoch is
    one forward FFT plus one inverse per window (acq_core.c:2584-2613) and a
    magnitude-square accumulate (2662-2665); the decision runs once per dwell.
    The snapshot agrees: native 54.1/54.1/54.0 MSa/s at 90/176/256 epochs,
    op2M_U5k 16.1/16.2/16.2 at 51/96/137. One row per geometry is enough.
- **the window count is the knob.** Rate x windows is nearly constant across
    the tiled rows (35x3.5 = 122, 21x5.74 = 121, 53x2.33 = 123, 19x6.32 = 120,
    7x16.1 = 113, 3x33.3 = 100), and windows scale as uncertainty / chip rate
    while the sample rate scales with chip rate, so the real-time margin at a
    given uncertainty is the same at every chip rate (0.57x at +/-50 kHz:
    5.74/10, 2.33/4, 3.5/6.138). The design says so too
    (docs/design/async-dsss-receiver-measurements.md:39-43).

The geometry is a grid, chip rate x Doppler uncertainty, and both axes move
the window count, so each axis keeps its ends: chip rate 2 and 5 Mcps (the
design's range, docs/design/async-dsss-receiver.md:575; 3.069 Mcps is the
middle), uncertainty +/-5 and +/-50 kHz (the design's two values, :583-584;
+/-16.7 kHz is the middle), plus the one zero-uncertainty point, `native`
(the floor, at 3.069 Mcps, the only du = 0 row). The thread sweep exists only
at 5 Mcps, +/-50 kHz (1, 2, 4, 8): its ends are 1 and 8. One `nc` row per
point.

Kept, the questions they answer:

| row                  | question                                                                                                                                  | value (v0.72.0) |
| -------------------- | ----------------------------------------------------------------------------------------------------------------------------------------- | --------------- |
| `native_nc256`       | the floor: code search with no Doppler search (and the headline)                                                                          | 54.1 MSa/s      |
| `op5M_U50k_nc254`    | can one core keep up at the design operating point (5 Mcps, +/-50 kHz, 10 MSa/s in)? No: 0.57x; also the 1-thread end of the thread sweep | 5.74 MSa/s      |
| `op5M_U50k_t8_nc254` | the 8-thread end of the sweep: 1.64x real time (reads below 4 threads on the publishing box, see STILL CONFLICTING)                       | 16.4 MSa/s      |
| `op5M_U5k_nc225`     | what Doppler pre-compensation (+/-5 kHz) buys at 5 Mcps: 3.3x real time on one core                                                       | 33.3 MSa/s      |
| `op2M_U50k_nc57`     | the 2 Mcps end at +/-50 kHz: the most windows (53), the slowest per input sample, 0.58x real time                                         | 2.33 MSa/s      |
| `op2M_U5k_nc51`      | the 2 Mcps end at +/-5 kHz: 4.0x real time                                                                                                | 16.1 MSa/s      |

Which epoch row was kept per geometry: the 0.9-detection-probability row,
the API default (`objects/acq.toml:92-94`, `pd` default 0.9) and the only one
that is a single run under its name at 5 Mcps (see "Row-ID collisions").
Exception: `native_nc256`, kept because it is the existing headline with a
trend behind it; if the maintainer wants one convention, swap it for
`native_nc90` (same cost per sample).

| row                       | verdict | covered by / reason                                                                                                                                                                                                                                                                                                                                                                                                                         |
| ------------------------- | ------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| c:acq::native_nc90        | CUT     | `native_nc256`: same geometry, same cost per sample (54.0 vs 54.1); only the detection target differs                                                                                                                                                                                                                                                                                                                                       |
| c:acq::native_nc176       | CUT     | `native_nc256`: same geometry, same cost per sample (54.1)                                                                                                                                                                                                                                                                                                                                                                                  |
| c:acq::native_nc256       | KEEP    | the floor, one window, no Doppler search; existing headline                                                                                                                                                                                                                                                                                                                                                                                 |
| c:acq::wideband_nc112     | CUT     | `op5M_U50k_nc254` / `op2M_U50k_nc57`: the middle of the chip-rate axis at +/-50 kHz (35 windows, 3.5 MSa/s, on the per-window law, same 0.57x real-time margin). Note: 3.069 Mcps is the design's "exemplary use case" (docs/design/async-dsss-receiver.md:23), so the maintainer may want this one named anyway                                                                                                                            |
| c:acq::wideband_nc215     | CUT     | as wideband_nc112; epoch count does not change cost per sample (3.52)                                                                                                                                                                                                                                                                                                                                                                       |
| c:acq::wideband_nc256     | CUT     | as wideband_nc112 (3.5)                                                                                                                                                                                                                                                                                                                                                                                                                     |
| c:acq::op5M_U50k_nc254    | KEEP    | the design operating point, one thread                                                                                                                                                                                                                                                                                                                                                                                                      |
| c:acq::op5M_U50k_nc256    | CUT     | `op5M_U50k_nc254`: same cost per sample (5.76 vs 5.74); also two runs under one name                                                                                                                                                                                                                                                                                                                                                        |
| c:acq::op5M_U50k_t2_nc254 | CUT     | `op5M_U50k_nc254` (1 thread) + `op5M_U50k_t8_nc254` (8): an interior point of the thread sweep (10.5)                                                                                                                                                                                                                                                                                                                                       |
| c:acq::op5M_U50k_t2_nc256 | CUT     | as t2_nc254, and its epoch twin (10.5)                                                                                                                                                                                                                                                                                                                                                                                                      |
| c:acq::op5M_U50k_t4_nc254 | CUT     | `op5M_U50k_nc254` + `op5M_U50k_t8_nc254`: an interior point of the thread sweep, although it reads fastest today (17.5), see STILL CONFLICTING                                                                                                                                                                                                                                                                                              |
| c:acq::op5M_U50k_t4_nc256 | CUT     | as t4_nc254, and its epoch twin (17.5)                                                                                                                                                                                                                                                                                                                                                                                                      |
| c:acq::op5M_U50k_t8_nc254 | KEEP    | the 8-thread end of the thread sweep                                                                                                                                                                                                                                                                                                                                                                                                        |
| c:acq::op5M_U50k_t8_nc256 | CUT     | `op5M_U50k_t8_nc254`: same cost per sample (16.4); two runs under one name                                                                                                                                                                                                                                                                                                                                                                  |
| c:acq::op5M_U5k_nc225     | KEEP    | the pre-compensated uncertainty at the operating point                                                                                                                                                                                                                                                                                                                                                                                      |
| c:acq::op5M_U5k_nc256     | CUT     | `op5M_U5k_nc225`: same cost per sample (33.3); two runs under one name                                                                                                                                                                                                                                                                                                                                                                      |
| c:acq::op5M_U17k_nc238    | CUT     | `op5M_U5k_nc225` / `op5M_U50k_nc254`: the middle of the uncertainty axis at 5 Mcps, and it answered a SUPERSEDED design question, slicing +/-50 kHz across three engines instead of threading one; the measurements page records the answer (6% and 11% for the slice) and that it was superseded on 2026-09-03 by the roll per thread (async-dsss-receiver-measurements.md:42-57). Its 7-window cost also equals op2M_U5k's (16.1 vs 16.2) |
| c:acq::op5M_U17k_nc256    | CUT     | as op5M_U17k_nc238 (16.2)                                                                                                                                                                                                                                                                                                                                                                                                                   |
| c:acq::op2M_U50k_nc57     | KEEP    | the 2 Mcps end of the operating range, most windows, slowest per sample                                                                                                                                                                                                                                                                                                                                                                     |
| c:acq::op2M_U50k_nc107    | CUT     | `op2M_U50k_nc57`: same cost per sample (2.33)                                                                                                                                                                                                                                                                                                                                                                                               |
| c:acq::op2M_U50k_nc151    | CUT     | `op2M_U50k_nc57`: same cost per sample (2.33)                                                                                                                                                                                                                                                                                                                                                                                               |
| c:acq::op2M_U17k_nc54     | CUT     | `op2M_U5k_nc51` / `op2M_U50k_nc57`: the middle of the uncertainty axis at 2 Mcps, and the superseded slice question, as op5M_U17k (6.31)                                                                                                                                                                                                                                                                                                    |
| c:acq::op2M_U17k_nc101    | CUT     | as op2M_U17k_nc54 (6.32)                                                                                                                                                                                                                                                                                                                                                                                                                    |
| c:acq::op2M_U17k_nc144    | CUT     | as op2M_U17k_nc54 (6.33)                                                                                                                                                                                                                                                                                                                                                                                                                    |
| c:acq::op2M_U5k_nc51      | KEEP    | the 2 Mcps, +/-5 kHz corner of the operating range (7 windows, 4.0x real time)                                                                                                                                                                                                                                                                                                                                                              |
| c:acq::op2M_U5k_nc96      | CUT     | `op2M_U5k_nc51`: same cost per sample (16.2)                                                                                                                                                                                                                                                                                                                                                                                                |
| c:acq::op2M_U5k_nc137     | CUT     | `op2M_U5k_nc51`: same cost per sample (16.2)                                                                                                                                                                                                                                                                                                                                                                                                |

The pending depth rows already in `benchmarks/labels.toml:378-424` (not in
this set) compare D = 154/61 against D = 1 at the same geometry and thread
count. Their D = 1 partners that survive: `op5M_U50k_nc254` (t1),
`op5M_U50k_t8_nc254` (t8), `op5M_U5k_nc225` (t1), `op2M_U50k_nc57` (t1).
`op5M_U50k_D154_t2`/`_t4` lose their D = 1 partners with t2/t4 cut, and the
same two-ends rule would cut them (keep D154_t1 and D154_t8). The 2 Mcps and
+/-5 kHz depth rows sweep only 1 and 4 threads, so both are their ends.

### acq from Python (`bench_acq.py`, BurstAcquisition): 2 of 2 kept

The two rows are a two-point sweep over the dwell's grid size (124 x 16 =
1,984 samples against 2,046 x 10 = 20,460), a knob that can change cost per
sample, so both are its ends and both stay, though they read within 1.2x.

| row                               | verdict | covered by / reason                                                                                                                      |
| --------------------------------- | ------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| py:acq::test_bench_tile_test_grid | KEEP    | the small end of the grid-size sweep (31-chip code, 1,984-sample dwell), the test suite's grid                                           |
| py:acq::test_bench_tile_worked    | KEEP    | the large end: the burst engine on a realistic 1,023-chip code (the C `burst_acq::push` rows, in another set, use 15- and 63-chip codes) |

### carrier_acq (C, `bench_carrier_acq_core.c`): 1 of 4 kept

All four are hollow (COUNT PROBLEMS, item 1); their honest twins are the
Python search rows, which keep both ends of the zero-pad sweep (4 and 16).
`sequential` does not change cost per block (3.92 us for one block against
33.27 / 8 = 4.16 us), so it keeps one value, the default. One row is kept so
the C bench keeps a row (the coverage gate fails a C bench that records
nothing, scripts/check_bench_coverage.py:30-40), relabelled per call. The
two-ends rule would also keep `steps[pad=16,seq]`; it is cut here because
it is hollow and `search[16]` is its honest twin. Once #1967 makes the C
bench honest, keep pad 4 and pad 16 there, sequential.

| row                              | verdict | covered by / reason                                                                                                                                                                                                       |
| -------------------------------- | ------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| c:carrier_acq::steps[pad=4]      | CUT     | `c:carrier_acq::steps[pad=4,seq]` (per call) and `py:carrier_acq::test_bench_steps_search[4]` (per sample): hollow, reads 40 of 65,536 samples; non-sequential is not the default mode (objects/carrier_acq.toml:112-114) |
| c:carrier_acq::steps[pad=4,seq]  | KEEP    | the C bench's one row, per call, at the defaults (sequential, zero-pad 4); replace when #1967/#2388 fix the bench                                                                                                         |
| c:carrier_acq::steps[pad=16]     | CUT     | `py:carrier_acq::test_bench_steps_search[16]`: hollow, reads 40 of 65,536                                                                                                                                                 |
| c:carrier_acq::steps[pad=16,seq] | CUT     | `py:carrier_acq::test_bench_steps_search[16]`: hollow, reads 320 of 65,536 (the zero-pad sweep's large end, kept on the honest face)                                                                                      |

### carrier_acq from Python (`bench_carrier_acq.py`): 2 of 4 kept

| row                                         | verdict | covered by / reason                                                                                                                                                                                                                                                                                               |
| ------------------------------------------- | ------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| py:carrier_acq::test_bench_steps_search[4]  | KEEP    | the small end of the zero-pad sweep, the default (11.6 MSa/s)                                                                                                                                                                                                                                                     |
| py:carrier_acq::test_bench_steps_search[16] | KEEP    | the large end: zero-pad changes cost per sample, 4.7x (2.49 MSa/s)                                                                                                                                                                                                                                                |
| py:carrier_acq::test_bench_steps_detect[4]  | CUT     | `search[4]`: signal against noise at a fixed block does not change cost per block, so one of the pair stays; a decision at 20 dB is one block folded and tested, which costs what a search block costs (334.6 us vs 5.642 ms / 16 = 352.6 us, min-based); time to decide = 4,096 samples at the search row's rate |
| py:carrier_acq::test_bench_steps_detect[16] | CUT     | `search[16]`: as above (1,458 us vs 26.27 ms / 16 = 1,642 us)                                                                                                                                                                                                                                                     |

### acquire (C, `bench_acquire_core.c`): 1 of 2 kept

| row                                               | verdict | covered by / reason                                                                                                                                                                                                                                                                                                                                        |
| ------------------------------------------------- | ------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| c:acquire::dp_fftfreq_index (inlined)             | CUT     | `c:acquire::bin_to_signed (...)`: the question it answered (does the wrapper cost a C hot path anything) is settled and written down (bench_acquire_core.c:4-15, bin_to_signed.c:1-15), and its number is not a per-call cost a caller sees: the grid size is a compile-time constant, so the loop is free to vectorize (OTHER RECORDING PROBLEMS, item 3) |
| c:acquire::bin_to_signed (wrapper, not inlinable) | KEEP    | the acquire module's own function and the one the Python face calls; the C bench's one row                                                                                                                                                                                                                                                                 |

### Does every object keep a row?

| object / component                             | rows | kept | survives                                            |
| ---------------------------------------------- | ---- | ---- | --------------------------------------------------- |
| acq, C (`bench_acq_core`)                      | 27   | 6    | yes                                                 |
| acq, Python (`bench_acq.py`, BurstAcquisition) | 2    | 2    | yes                                                 |
| carrier_acq, C (`bench_carrier_acq_core`)      | 4    | 1    | yes, but only as a per-call give-up row until #1967 |
| carrier_acq, Python (`bench_carrier_acq.py`)   | 4    | 2    | yes                                                 |
| acquire, C (`bench_acquire_core`)              | 2    | 1    | yes                                                 |

Cutting a row means changing its bench (it is emitted by a loop over
`CFGS`/`PD_POINTS`/`pads`/`seqs`, or a pytest parametrize); a label cannot
drop it. `bench_acq_core.c:54` (`PD_POINTS`) and `:82-113` (`CFGS`) are where
the acq cuts land; `bench_carrier_acq_core.c:70-71`; the `detect` test in
`bench_carrier_acq.py:121-142`; `bench_acquire_core.c:40-50`. The 5 Mcps
`nc256` duplicates disappear with `PD_POINTS` cut to one target, and that
forces one convention: at 0.9 the headline becomes `native_nc90` (the same
cost per sample as `native_nc256`, so only its ID and trend move); at 0.999
the other kept rows become their 0.999 twins (also the same cost).

______________________________________________________________________

## Evidence per kept row

### acq, C (all six)

- Code: 1,023 pseudo-random chips from xorshift, standing in for a Gold code
    ("a real Gold code's exact chips don't matter"), bench_acq_core.c:149-156,
    `SF 1023` :42. 2 samples per chip, `SPC 2` :43, so an epoch is
    `nx = SF*SPC = 2046` samples :142.
- Chip rates: `CHIP_RATE 3.069e6` :44 (native); 5e6 / 2e6 in `CFGS`
    :85-88, :103. Sample rate `fs = chip_rate * SPC` :163, so 6.138, 10, 4 MSa/s.
- Uncertainty and windows: native `du = 0.0`, 1 window :83; op5M_U50k
    50 kHz, 21 :85; op2M_U50k 50 kHz, 53 :86; op5M_U5k 5 kHz, 3 :87. The
    bench refuses a row whose window count differs (:194-203, `continue`), so
    every published row has exactly that count. Window sizing:
    acq_core.c:1239-1258. Meanings of `op`/`U`: bench :74-81 and
    docs/design/async-dsss-receiver.md:571-590 (§6.1: 2-5 Mcps, +/-50 kHz to
    start, +/-5 kHz after pre-compensation, 21/53 and 3/7 tiles).
- C/N0 37.31 dB-Hz :45, Pfa 1e-3 :48, passed to
    `dp_acq_create_continuous` :177-180; `code_only_epochs = 1` (D = 1) for all
    these rows, checked at :194.
- Detection target and epoch count: `PD_POINTS = {0.9, 0.99, 0.999}` :54; the
    epoch count is the sizer's (`acq_ascend_n_noncoh`, acq_core.c:1191-1212,
    capped at `ACQ_N_NONCOH_SAFETY_CEILING 256u`, acq_core.h:690), and the
    name is `"%s_nc%zu"` :267. The sizer is monotonic in the target, so with
    three distinct names the counts map 0.9 / 0.99 / 0.999 in ascending order.
    native's 256 is the ceiling value; whether 0.999 met its target there or was
    clamped is not visible without running (`pd_predicted` is printed, :230,
    not recorded).
- `op2M_U5k`: 2 Mcps, 5 kHz, 7 windows, CFGS :88.
- Threads: `cfg->threads` :71, 1 for every row but t2/t4/t8 :102-104 (t8 :104), applied
    by `dp_acq_set_threads` :192 (acq_core.c:2229-2239: serial when n == 1 or
    one window). The fan is the per-epoch tile loop, `dp_pool_run(..., window_bins, acq_tile_epoch, ...)` acq_core.c:2613; acq_core.h:122-128.
- Count: `jm_bench_add_unit(..., (int)n_in, JM_BENCH_SAMPLE)` :268-269 with
    `n_in = n_noncoh * D * nx` :205, one timed `dp_acq_push(a, buf, n_in, ...)`
    per round :242. The push consumes every sample: at D = 1 a frame is one
    epoch (acq_core.c:1509, `frame_n = cb` tiled, `new_n = cb` at one window),
    the room check (:2566-2570) only stops it at 4 results or a decision with
    no room, and the decision falls on the last epoch of the push
    (:2665-2675). snapshot `iterations` equal `nc * 2046` for every row
    (A.tsv). Per-sample count is honest.

### acq, Python: test_bench_tile_test_grid and test_bench_tile_worked

- test_grid: `_acq(31, 5, reps=16, spc=4)` bench_acq.py:58-61; 31-chip
    m-sequence (`mls_poly(5)`), 4 samples per chip, 1 Mcps, 4 MSa/s; snapshot
    `extra_info.N` = 1,984 = 16 x 124.

- `_acq(1023, 10, reps=10, spc=2)` bench_acq.py:71; `PN(poly=mls_poly(10), seed=1, length=10).generate(1023)` :27-28 is the 1,023-chip m-sequence
    (src/doppler/wfm/wfm.pyi:68-91: period 2^length - 1); held 2 samples per
    chip by `code_preamble` (acquire/tests/\_preamble.py:31-48);
    `fs = 1.0e6 * spc` :38, so 1 Mcps, 2 MSa/s.

- Noise input: `rng.standard_normal` complex, bench_acq.py:48-51.

- One push of `N = doppler_bins * code_bins` :47; snapshot `extra_info.N` =
    20,460 = 10 x 2,046. A burst engine with one window has `frame_n = new_n = D * cb` (acq_core.c:1506-1509), framed at hop = frame (:2270), so a push
    of N is exactly one frame, one dwell. `doppler_uncertainty` defaults to 0
    (objects/burst_acq.toml), so no window tiling. Pipeline per the bench
    docstring :3-4.

- `MSa_s = n / mean` :55. Unit "sample", samples 1.

### carrier_acq, C: steps[pad=4,seq]

- `dp_carrier_acq_create(SAMPLE_RATE_HZ=4e6, SYMBOL_RATE_HZ=1e6, resolution 0.0, pads[p], HANN, 0.0f, NULL, 0, 1e-3, 0.9, 2.0, seqs[q], MAX_N_BLOCKS=8)` bench_carrier_acq_core.c:31-33,77-79.
- Block length: resolution 0 -> `symbol_rate/10` = 100 kHz
    (carrier_acq_core.c:67-68), `n_fft = llround(4e6 / 1e5) = 40` (:74).
    Transform `nfft = next_pow_two(n * pad)` (psd_core.c:69; util_core.h:92-102):
    160 -> 256 at pad 4, 640 -> 1,024 at pad 16. Hop = n (psd_core.c:33-38).
- Give-up cap: `carrier_acq_giveup_cap` = `max_n_blocks` (8) in sequential
    mode, `dwell_target` otherwise (carrier_acq_core.c:224-226).
    `dwell_target = dp_det_n_noncoh(2.0, 40, 0.9, 1e-3, 100000)` (:161-162);
    the header's doctest gives 1 at n_coh = 16 (detection_core.h:510-516) and
    a longer coherent length only raises Pd, so it is 1 here. `steps` returns
    at the cap (:309-310) and its block loop stops there (:331).
- Input: `x[i] = s_i * exp(j 2 pi 0.011 i)` with `s_i` from a 16-bit Galois
    LFSR stepped once PER SAMPLE (bench :57-64). So the carrier sits at
    0.011 x 4 MHz = +44 kHz, but its BPSK runs at one symbol per sample
    (4 Msym/s): a +/-1 m-sequence has a flat spectrum, and rotating a flat
    spectrum leaves it flat. The detector correlates the averaged PSD against a
    sinc^2(f / 1 MHz) template (carrier_acq_core.c:18-28, 133-134); a flat
    spectrum gives no peak against it, so the test behaves as under noise
    (Pfa 1e-3 per test). **x contains a modulated carrier, but nothing the
    detector is built to find; no detection fires.** This is inferred from
    source; the snapshot corroborates it: sequential / non-sequential per call
    is 33.27 / 3.92 us = 8.5x at pad 4 and 150.6 / 18.41 us = 8.2x at pad 16,
    i.e. 8 blocks against 1 (a detection inside the sequential run would cut
    that ratio).
- Count for the label: the round records 65,536 sample units (:110-111), so
    `domain = "call"`, `count = 65536` reads as the call time (mean 33.27 us).

### carrier_acq, Python: search[4], search[16]

- `CarrierAcquisition(sample_rate_hz=4e6, symbol_rate_hz=1e6, resolution_hz=4e6/4096, zero_pad=pad, psd_template=[], sequential=True)`
    bench_carrier_acq.py:41-49,76-84; other defaults (Hann, pfa 1e-3, pd 0.9,
    design_snr 2.0, max_n_blocks 100,000) objects/carrier_acq.toml:65-127.
    Block 4,096 (asserted `ca.nfft // pad == N_FFT`, :108), transform 16,384
    at pad 4 and 65,536 at pad 16.
- Noise: `Synth(type="noise", fs=FS, seed=3)` 64k, "unit-power AWGN" :52-56.
- Asserted: no detection, `n_blocks == 16` (:111-115), so all 65,536 are
    folded and tested (sequential tests every block, carrier_acq_core.c:286).
    `MSa_s = 65536 / min` :116-118. Unit "sample", samples 1.
- "a carrier found on the first block costs one of those blocks": the detect
    rows (cut) fold one 4,096 block (snapshot `n_blocks` = 1) in 334.6 us at
    pad 4 against 352.6 us per search block.

### acquire, C: bin_to_signed

- `dp_bin_to_signed` = `(int)dp_fftfreq_index(bin, n_bins)` in its own file
    (native/src/acquire/bin_to_signed.c:17-22); declared
    acquire_core.h:16-38; `dp_fftfreq_index` is the static inline in
    clib_common.h:159-163. No LTO flag anywhere in the CMake tree (searched
    `INTERPROCEDURAL`, `flto`), so a C caller cannot inline it.
- Loop of `BENCH_N = 65536` calls per round, `nbins = 8`
    (bench_acquire_core.c:22, 38, 53-62), recorded `JM_BENCH_CALL` with
    iters 65,536 (:63-64): the unit is one function call. `domain = "call"`,
    `count = 1`. 796 ps per call (portable).

______________________________________________________________________

## COUNT PROBLEMS

1. **`c:carrier_acq::steps[pad=4|pad=16|pad=4,seq|pad=16,seq]` (all four):
    65,536 samples recorded, 40 or 320 read.** Recorded:
    `jm_bench_add_unit(..., BENCH_N, JM_BENCH_SAMPLE)` with `BENCH_N 65536`
    (bench_carrier_acq_core.c:28, 110-111). Read: non-sequential stops after
    `dwell_target` = 1 block of 40 samples; sequential after `max_n_blocks` =
    8 blocks, 320 samples (carrier_acq_core.c:74, 161-163, 224-226, 309-310,
    331). That is 0.061% and 0.49% of what is credited, so the page's
    16.7 / 3.56 GSa/s (non-sequential) and 1.97 / 0.435 GSa/s (sequential)
    are inflated 1,638x and 205x. Worse, no row decides anything: the input's
    bits change every sample (evidence above), so all four time a give-up,
    and the bench's own text is wrong twice: "A BPSK stream with a real
    offset to find" (:54-56) and "it consumes more blocks before deciding"
    (:118-125). Per folded sample they run at about 10.2 MSa/s (pad 4) and
    2.2 MSa/s (pad 16), in line with the Python search rows despite the
    100x smaller block. Fix in the bench (#1967, #2388): hold each bit for
    `SAMPLE_RATE_HZ / SYMBOL_RATE_HZ` = 4 samples so the template has
    something to find, assert `ready`, and record the folded samples
    (`n_blocks * n`) as the Python twin does (bench_carrier_acq.py:135-142);
    or record `JM_BENCH_CALL` with iters 1.

1. **`py:carrier_acq::test_bench_steps_detect[*]` (cut, but if kept): the
    page count rides on an unasserted `n_blocks == 1`.** MSa_s is credited
    with `ca.n_blocks * N_FFT` (bench_carrier_acq.py:135-142); the snapshot
    records `n_blocks` 1 for both pads, but the bench asserts only
    `folded < BLOCK_64K` (:136). A run that needed two blocks would credit
    8,192, and a `domain = "call", count = 4096` label would then show half
    the true time per call with no gate failing.

No other row in this set has a recorded count that misreports one round's
work: every acq row's iterations equal its push (`nc * 2046`, consumed
whole), the Python acq rows push exactly one frame, the Python search rows
assert all 16 blocks folded, and the acquire rows count one call per
iteration.

## OTHER RECORDING PROBLEMS

1. **Two configurations under one row ID at 5 Mcps.** The row name is the
    epoch count alone (`"%s_nc%zu"`, bench_acq_core.c:267), and at 5 Mcps the
    sizer returns the 256 ceiling for both the 0.99 and the 0.999 target: the
    0.9 counts are 254 / 238 / 225 and every 3-row geometry shows 0.99 needing
    about 1.9x the 0.9 count, past 256, and `dp_det_n_noncoh` returns -1 past
    its bound, which the sizer turns into the ceiling (acq_core.c:1201-1203).
    So `bench_acq_core` emits `op5M_U50k_nc256`, `op5M_U5k_nc256`,
    `op5M_U17k_nc256` and `op5M_U50k_t{2,4,8}_nc256` TWICE per run. `jm bench` keeps both (just_makeit/\_bench.py:367-369, a plain append);
    `scripts/bench_interleaved.py:375-389` (`_merge_best`, keyed by
    `fullname`) silently keeps the faster. The work is identical, so the
    count is right, but those rows are a min over 2K runs where every other
    row is a min over K. All of them are CUT above; the bench should name
    rows by the detection target, not by the sizer's output, which also
    keeps the ID stable when the sizer's model moves (today a sizer change
    renames rows and orphans their labels).

1. **The headline row's label called the code "Gold-1023".** The bench uses
    1,023 xorshift chips (bench_acq_core.c:149-156); the redraft says
    "1,023-chip code". The file's header comment (:4) also says Gold-1023.

1. **`c:acquire::dp_fftfreq_index (inlined)` does not measure what it
    claims.** The comment says the grid size is "not a compile-time
    constant, so neither call can be folded away" (bench_acquire_core.c:35-36),
    but `nbins = (size_t)(8 + (BENCH_N & 0))` (:38) is an integer constant
    expression, 8, so `i % nbins` is `i & 7` and the inlined loop is free to be
    vectorized. 199 ps per call is about one cycle on the 5.09 GHz Zen 5 cores
    the run is pinned to (scripts/bench_report.py:330-332). The 4x gap to the
    wrapper is then the call PLUS lost vectorization, not the call alone.
    Not verified against the generated assembly (no build). Cut above.

1. **The Python carrier_acq docstring's claim about its C twin is false**
    (bench_carrier_acq.py:30-32: "sweeps the same two zero-pads; the gap
    between the faces is the binding's per-call overhead"). The C bench uses
    40-sample blocks and 256/1,024-point transforms, the Python one 4,096 and
    16,384/65,536; the C input never detects; the C bench also runs
    non-sequential. The faces cannot be subtracted.

## STILL CONFLICTING

Pairs whose numbers will still look contradictory with correct labels:

1. **`op5M_U50k_t8` (16.4 MSa/s, KEPT as the sweep's end) is slower than
    `t4` (17.5, cut).** The v0.72.0
    C snapshots record `doppler_meta.pinned_cpus = [0, 1, 2, 3, 10, 11, 12, 13]`: the fastest core class, which scripts/bench_report.py:326-353 picks
    and its docstring (:330-332) gives as 4 Zen 5 cores on this Ryzen AI 9 465.
    Eight logical CPUs on 4 physical cores means t8 puts two threads on each
    core. That 10-13 are the SMT siblings of 0-3 is inferred (8 logical
    CPUs, 4 cores), not read from sysfs. A label cannot fix it: on this box
    the kept 8-thread row reads as a ceiling reached at 4 cores, and the page
    should say the run is pinned to 4 physical cores (or the bench should cap
    threads at the physical count).
1. **`native` (one window) is only 1.6x faster than `op5M_U5k` (three).**
    Across the tiled rows rate x windows is about 100-123 MSa/s per window, so
    a one-window search "should" run near 100+. It does not because one window
    takes the other code path: `wideband = window_bins > 1` (acq_core.c:2537)
    sends a single window through `acq_make_surface` and the 2-D correlator
    (:2622-2639) instead of one shared forward FFT plus one inverse per window
    (:2584-2613). Explained by source; a reader will still find it odd. Both
    are kept.
1. **Carrier search, C against Python** (if any C row stays): the kept C row
    (33.3 us per call, 320 samples) and the Python search rows (5.6 / 26.3 ms
    per call, 65,536 samples) are different configurations (OTHER RECORDING
    PROBLEMS, item 4), and the C row times a give-up. Not comparable until
    #1967 makes the C bench use the Python configuration.

Checked and NOT conflicting (the labels now say why):

- Identical MSa/s across `ncN` siblings of one geometry: cost is per epoch.
- `op5M_U17k` (16.1) equal to `op2M_U5k` (16.2) at different chip rate and
    uncertainty: both are 7 windows, and an epoch is 2,046 samples at every
    chip rate.
- Python search vs detect: the same cost per block (above).

## Could not determine from source

- Whether native's 256 epochs met the 0.999 target or was clamped at the
    ceiling (the label says only "the engine's ceiling and what its sizer
    gives for 0.999", true either way).
- That the 5 Mcps 0.99 run also lands on 256 is inferred from the sizer's
    shape and the snapshot's missing row, not observed.
- `dwell_target = 1` for the C carrier bench is inferred from the
    dp_det_n_noncoh doctest and monotonicity, corroborated by the 8x timing
    ratio; it was not run.
- Whether the inlined `dp_fftfreq_index` loop is actually vectorized (needs
    the assembly).
- SMT sibling mapping of the pinned CPUs.
