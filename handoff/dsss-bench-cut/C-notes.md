# Set C notes: Acquisition & DSSS (burst chain, despreaders, ppe)

Source read: worktree `finisher-acq` at decdcda31. Published values: v0.72.0
`portable*.json`, which is what the page shows. Nothing was built or run.

**Headline:** keep 12 of 31 rows and cut 19. None of the counts is wrong,
but one C row has an unchecked count. The two burst_demod labels already on
the page describe work the code does not do.

## KEEP / CUT

| row                                             | verdict | reason; for a CUT row, the kept row that covers it                                                                                                                                                                                                                                               |
| ----------------------------------------------- | ------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| c:burst_acq::push[sf=15,spc=2]                  | KEEP    | Low end of the code-period knob: a 30-sample period, 72.3 MSa/s.                                                                                                                                                                                                                                 |
| c:burst_acq::push[sf=15,spc=4]                  | CUT     | Sweep midpoint (45.8). Covered by the two endpoints, push[sf=15,spc=2] and push[sf=63,spc=4].                                                                                                                                                                                                    |
| c:burst_acq::push[sf=63,spc=2]                  | CUT     | Sweep midpoint (40.0). Covered by the same two endpoints.                                                                                                                                                                                                                                        |
| c:burst_acq::push[sf=63,spc=4]                  | KEEP    | High end of the knob: a 252-sample period, 36.8 MSa/s, about 2x the low end.                                                                                                                                                                                                                     |
| py:burst_acq::test_bench_push_idle              | CUT     | 56.9 falls inside the C range at the chain's 510-sample period, and the binding adds nothing a user needs. Covered by c:burst_acq::push[sf=15,spc=2] and [sf=63,spc=4].                                                                                                                          |
| py:burst_acq::test_bench_push_bursts            | CUT     | Same cost as push_idle (56.5 against 56.9), and a detection adds nothing. Covered by the same two C rows.                                                                                                                                                                                        |
| py:burst_acq::test_bench_construct              | KEEP    | 6.6 ms per object, about 5.7 times the 1.15 ms push of a whole 65,536-sample block. Anyone who builds per channel, per retune or a pool of receivers pays it. No C row times construction. This is where the cost comes from (acq sizing).                                                       |
| c:burst_capture::push[quiet]                    | CUT     | Same cost as the 4-burst row (28.2 against 27.7). Covered by c:burst_capture::push[4 bursts].                                                                                                                                                                                                    |
| c:burst_capture::push[4 bursts]                 | KEEP    | Capture cost per sample with refine and the window copy in use, at the C geometry (124-sample period, 2,448-sample bursts).                                                                                                                                                                      |
| c:burst_capture::push[4 bursts, file-backed]    | CUT     | Same cost (27.8 against 27.7). Covered by c:burst_capture::push[4 bursts]. The design claim it guards ("backing costs nothing") would be better as a ratio assert in a test than as a page row.                                                                                                  |
| py:burst_capture::test_bench_push_idle          | CUT     | It is the 4-burst Python row without refine. Covered by py:burst_capture::test_bench_push_bursts.                                                                                                                                                                                                |
| py:burst_capture::test_bench_push_bursts        | KEEP    | The only capture row at the chain's real operating point (255x5 preamble at 2 spc, 8,316-sample bursts, AWGN), with captures asserted. It reads 45.9 against C's 27.7, which is a different case, not a re-measure. If the C bench moves to this geometry, cut this row.                         |
| py:burst_capture::test_bench_push_bursts_backed | CUT     | Same cost as push_bursts (46.5 against 45.9). Covered by py:burst_capture::test_bench_push_bursts.                                                                                                                                                                                               |
| py:burst_capture::test_bench_construct          | CUT     | Same 6.6 ms as BurstAcquisition. That engine's sizing is the cost. Covered by py:burst_acq::test_bench_construct.                                                                                                                                                                                |
| c:burst_demod::demod[clean]                     | CUT     | Same code path as the offset row (rate search off, one FFT per pass). A zero-offset carrier is not a real case, and the 13% gap is not a different algorithm. Covered by c:burst_demod::demod[offset]. Its existing label must be deleted with it.                                               |
| c:burst_demod::demod[offset]                    | KEEP    | One real burst with a real carrier offset and the rate search off: the base demodulation cost (45.1 MSa/s).                                                                                                                                                                                      |
| c:burst_demod::demod[chirp]                     | KEEP    | The max_rate knob: the rate search turned on costs about 9x (5.01 MSa/s).                                                                                                                                                                                                                        |
| py:burst_demod::test_bench_demod_burst          | CUT     | The case is wrong. It hands demod() a whole 65,536-sample block as if it were one burst, so most of the time goes to despreading a 1,015-symbol run of later bursts and noise. Covered by c:burst_demod::demod[offset], which is the same estimator with the rate search off, on one real burst. |
| py:burst_demod::test_bench_demod_idle           | CUT     | Same miscast block, filled with noise. Covered by c:burst_demod::demod[offset].                                                                                                                                                                                                                  |
| c:burst_despreader::steps[sf=15]                | KEEP    | Low end of the spreading-factor sweep (152). Under the two-ends rule it stays even though it reads like sf=63 today: the once-per-symbol loop update is shared over sf·4 samples, so spreading factor can change cost per sample.                                                                |
| c:burst_despreader::steps[sf=31]                | CUT     | Sweep midpoint (154). Covered by c:burst_despreader::steps[sf=15] and [sf=63].                                                                                                                                                                                                                   |
| c:burst_despreader::steps[sf=63]                | KEEP    | High end of the spreading-factor sweep (151).                                                                                                                                                                                                                                                    |
| py:burst_despreader::test_bench_steps_idle      | CUT     | The same per-sample kernel plus the binding (127), at 31 chips and 2 spc, inside the C range. Covered by c:burst_despreader::steps[sf=15] and [sf=63].                                                                                                                                           |
| py:burst_despreader::test_bench_steps_bursts    | CUT     | Same cost as idle (133 against 127). Covered by the same two C rows.                                                                                                                                                                                                                             |
| py:burst_despreader::test_bench_construct       | CUT     | 0.39 µs per object (copying codes, setting loop gains): nobody budgets that. No covering row is needed, and the two C steps rows stay as the object's rows.                                                                                                                                      |
| c:despreader::steps                             | KEEP    | The continuous Costas+DLL despreader (42.5 MSa/s), its only C row.                                                                                                                                                                                                                               |
| py:despreader::test_bench_steps_64k             | CUT     | Same signal and parameters as the C row, and it reads the same (42.4 against 42.5). Covered by c:despreader::steps.                                                                                                                                                                              |
| c:ppe::doppler                                  | KEEP    | The estimator with the rate search off, the setting BurstDemod uses by default (18.4 MSa/s).                                                                                                                                                                                                     |
| c:ppe::chirp                                    | KEEP    | The max_rate knob: 53 chirp rates cost about 33x per sample (550 kSa/s).                                                                                                                                                                                                                         |
| py:ppe::test_bench_estimate_idle                | CUT     | Same rate-search-off estimator as c:ppe::doppler at a larger size (65,536 samples), plus the binding. Covered by c:ppe::doppler.                                                                                                                                                                 |
| py:ppe::test_bench_estimate_bursts              | CUT     | Same cost as idle (14.4 against 14.4), as its own docstring says it must be. Covered by c:ppe::doppler.                                                                                                                                                                                          |

### Rows kept and cut, by object

Every object in the set keeps at least one row.

| object           | kept   | cut    | surviving rows                             |
| ---------------- | ------ | ------ | ------------------------------------------ |
| burst_acq        | 3      | 4      | c sf=15/spc=2, c sf=63/spc=4, py construct |
| burst_capture    | 2      | 5      | c 4 bursts, py push_bursts                 |
| burst_demod      | 2      | 3      | c offset, c chirp                          |
| burst_despreader | 2      | 4      | c sf=15, c sf=63                           |
| despreader       | 1      | 1      | c steps                                    |
| ppe              | 2      | 2      | c doppler, c chirp                         |
| **total**        | **12** | **19** |                                            |

**How the maintainer's sweep rule applies here.** A sweep over a knob that
can change cost per sample keeps its two ends. A knob that cannot keeps one
row.

| object           | knob                         | can it change cost per sample?                 | kept                                                       |
| ---------------- | ---------------------------- | ---------------------------------------------- | ---------------------------------------------------------- |
| burst_acq        | sf × spc                     | yes                                            | sf=15/spc=2 and sf=63/spc=4; the two middle points are cut |
| burst_despreader | sf                           | yes                                            | sf=15 and sf=63; sf=31 is cut                              |
| burst_demod      | max_rate (0 or 1e-6)         | yes, and only these two values exist           | offset and chirp                                           |
| burst_demod      | clean against offset         | no: carrier only, same code path               | one row (offset)                                           |
| burst_capture    | quiet / bursts / file-backed | no: input content and backing, not a cost knob | one C row                                                  |
| ppe              | doppler against chirp        | two configurations, not a one-knob sweep       | both                                                       |

Outside my set, to coordinate: `py:dsss_burst_receiver::test_bench_construct`
reads the same 6.6 ms as both construct rows here, for the same reason: it
builds the same acquisition engine. One construct row is enough for the
whole family. I kept the BurstAcquisition one because that is where the
cost lives. If the coordinator would rather keep the composed object's row
(the one users actually build), swap the two and cut mine.

## Evidence for the kept rows

### c:burst_acq::push[sf=15,spc=2] and [sf=63,spc=4]

The C source is `native/benchmarks/bench_burst_acq_core.c`.

- 32,768 samples per round: `BENCH_N`, line 27. Credited per sample at
    :114-115 (`jm_bench_add_unit(..., BENCH_N, JM_BENCH_SAMPLE)`).
- **Input** (:54-60): a 16-bit Galois LFSR (taps 0xB400) gives one ±1
    value per sample, with `0.02*s` in quadrature. That is a real-valued
    pseudo-random ±1 stream at a constant 1.15° phase, with no preamble in
    it, which matches the header's "a preamble that may never arrive" at
    :5-9.
- **Code** (:74-75): `((i * 2246822519u) >> 31) & 1` is a hashed
    pseudo-random code, not an m-sequence or a Gold code. It is turned into
    ±1 by `dp_bin_to_nrz` and held for spc samples (:80-82).
- **Create** (:83-85): reps 8, fs = 1e6\*spc, cn0 65 dB-Hz, Doppler
    uncertainty 0 (the native span only), pfa 1e-2, pd 0.9, noise mode 0
    (mean), doppler_rate 0 and rate_hypotheses 1. The parameters are named
    in `burst_acq_core.h:70-93`.
- **Each round** (:106-108): `dp_burst_acq_reset` runs outside the timer,
    then one timed `dp_burst_acq_push` with a 16-slot result array
    (`MAX_RESULTS`, :30).
- **Period:** 15 × 2 = 30 samples and 63 × 4 = 252 samples; 252/30 = 8.4.
    The bench header (:11-14) says the correlator is SF long, the grid is spc
    wide, and the cost is set by their product. The engine correlates by
    transform (`acq_core.h:20-24`).
- **Published (portable):** 0.453 ms and 0.890 ms per round, which gives
    72.3 and 36.8 MSa/s.

### py:burst_acq::test_bench_construct

- **Object** (`bench_burst_acq.py:57-67`):
    `BurstAcquisition(code_preamble(acq_code, 2), 5, 2e6, 60.0, 0.0, 1e-3, 0.9, "mean")`. The timed lambda is at :70-73.
- **Code:** a 255-chip m-sequence (`_burst_stimulus.py:46`, :53, :71-83,
    :133).
- **Preamble mapping:** `code_preamble` is `bin_to_nrz`, then each chip
    held spc samples (`acquire/tests/_preamble.py:32-50`). That makes a
    510-sample period.
- **Where 6.6 ms goes:** `dp_acq_create_burst` (`acq_core.c:2094-2125`)
    first normalizes the template and runs `acq_shape_of_template`, which is
    two 510-point cf64 FFTs (:1811-1860). It then sizes the search with
    `acq_auto_config_burst` (:1297-1388) and commits thresholds with
    `acq_commit_thresholds` (:1114-1188).
- **How many model evaluations:** `acq_mean_pd` averages over
    8 × 8 × `ACQ_DELAY_LOSS_NODES`(4) = 256 nodes. The 4 is at
    `acq_core.h:314` and the 256 is stated at `acq_core.c:1042`. In burst
    mode each node calls `acq_burst_pd_at` (:1009-1052), which costs one CFAR
    Pd evaluation plus two per alignment node, over `ACQ_ALIGN_NODES` = 8
    (:988). That is 17 per node and 4,352 per burst-Pd call.
    - At 60 dB-Hz and a 510-sample period, a² = 2·510·0.5 = 510, so depth 1
        meets pd 0.9 on the first try (the break at :1351-1356).
    - Construction therefore makes one burst-Pd call while sizing, then one
        more (`pd_burst`) and one plain 256-node call (`pd_predicted`) in
        commit: 4,352 + 4,352 + 256 ≈ 9,000 `dp_det_pd_cfar` evaluations.
    - The source itself says this quadrature is what create costs: "32 [nodes]
        ... costs 4x at every create" (:987).
    - I did not time the parts separately. "About 9,000" is a count of calls
        on the code path, not a measured share of the time.
- **Same cost elsewhere:** BurstCapture builds this engine through
    `dp_burst_acq_create` (`burst_capture_core.c:173`), with the same
    preamble, reps, fs and C/N0 (`bench_burst_capture.py:50-59`). That is
    why its construct row also reads 6.6 ms. DsssBurstReceiver composes a
    BurstCapture (`dsss_burst_receiver_core.c:529`, `state->cap`).
- **Why construction is worth a row:** the stimulus docstring
    (`_burst_stimulus.py:200-204`) records that construction "about 9 ms ...
    swamped the 1.6 ms push", which is why construction got a row of its
    own.

### c:burst_capture::push[4 bursts]

The C source is `native/benchmarks/bench_burst_capture_core.c`.

- **Geometry:** ACQ_SF 31, DATA_SF 8, REPS 4, SPC 4, PAYLOAD_SYMS 61 (:32-36).
    BURST_LEN is (4·31 + 61·8)·4 = 2,448 (:37). BENCH_N is 65,536 (:38).
- **Preamble code:** `dp_pn_create(pn_mls_poly(5), ...)`, a 31-chip
    m-sequence (:53-55).
- **Noise** (:70-80): an LCG drives each axis to uniform \[0, 0.02), so the
    noise is weak and has a nonzero mean.
- **Bursts** (:83-98): one every n/4 = 16,384 samples from sample 0, each a
    unit-amplitude preamble plus payload.
- **Create** (:117-124): fs 4e6, cn0 55, Doppler uncertainty 0, pfa 1e-3,
    pd 0.9, mean, no rate. A new capture is built every round, outside the
    timer (:118-132).
- **Count:** credited at :134. The header says "Every sample of @p x is
    consumed" (`burst_capture_core.h:698`), and `BURST_CAPTURE_HITS` is
    "never a correctness one: push() keeps looping" (:65-69).

### py:burst_capture::test_bench_push_bursts

- **Object:** `BurstCapture(code_preamble(255-chip, 2), burst_len=8,316, reps=5, fs=2e6, cn0_dbhz=60)` (`bench_burst_capture.py:46-59`). pfa 1e-3
    and pd 0.9 are defaults (`objects/burst_capture.toml:93-100`).
- **BURST_LEN** = (5·255 + 93·31)·2 = 8,316 (`_burst_stimulus.py:68`).
    The frame is 93 bits: 13 sync + 64 payload + 16 CRC (:59-62).
- **Burst spacing:** `refine_span + refine_span//10`
    (`_burst_stimulus.py:187`). Here refine_span = (k_lo + k_hi + reps)·P =
    (17 + 5 + 5)·510 = 13,770 (`burst_capture_core.c:210-212`), so the
    spacing is 15,147 and 65,536 // 15,147 = 4 bursts.
- **Placement:** the first burst starts at sample 0, because `off_samples`
    is the trailing gap (`wfm_surface.h:537`, "Trailing gap after the
    on-time").
- **Noise:** the AWGN floor is set by Es/N0 12 dB on the 31-chip data
    symbol (`_burst_stimulus.py:49`, :100-111).
- **Timing:** a new object every round (`on_fresh`, :194-226).
- **Recorded rate:** MSa_s = 65,536 / min (:229-239). The page reads Python
    rows from that rate (`scripts/bench_report.py:819-830`).
- **Captures asserted:** `win.size >= n_bursts * BURST_LEN` (:81-84).

### c:burst_demod::demod[offset] and [chirp] (both redrafted)

The C source is `native/benchmarks/bench_burst_demod_core.c`.

- **Burst** (`build_burst`, :86-104, and `dp_wfm_dsss_desc_chips`): 2,500
    preamble chips (500 × 5, :40-41), then 93 frame bits × 50 chips = 4,650,
    for 7,150 chips × 4 spc = 28,600 samples. The existing label's 28,600 is
    right. The frame is sync 13 + payload 64 + CRC-16 (:44-45, :69-79, and
    `wfm_frame.h:53`).
- **Cases** (:146-151):
    - offset: f0 = 0.012, prior 0.012, mu 0, max_rate 0.
    - chirp: f0 = 0.012, prior 0.0115, mu 6e-7, max_rate 1e-6.
    - Create is at :161-162 (est_segments 10). The preamble and prior are set
        at :169-170.
- **The old labels' errors:**
    - The header says the chain is estimate, dechirp, despread, sync, then
        "slice ... and STOP" (`burst_demod_core.h:5-13`). It "does not know
        what any of [the bits] mean ... which are a check" (:17-23), so there
        is no CRC check.
    - There is no acquisition either. The start and coarse Doppler come in
        through `set_prior` (`burst_demod_core.h:380-399`).
    - The bench's own header comment (:5-7, "acquires a preamble ... checks a
        CRC") is stale against the object header, and the old labels copied it.
- **Code path** (`burst_demod_core.c:297-632`):
    - Four preamble passes, `BURST_DEMOD_EST_ITERS` 4 (:11, :319-325). Each
        runs `form_partials` over 10,000 samples plus `dp_ppe_estimate`.
    - Timing search (:354-436). At spc 4 the span is 2. With x_len - npre -
        need = 28,600 - 10,000 - 18,600 = 0, the forward reach is 0, and with
        start 0 only one candidate exists. No re-estimate follows (:429).
    - `despread_data` runs over all 18,600 data samples (:276), then the NDA
        refinement `dp_ppe_create(nsym=93, rm)` (:486-489), then a second
        despread.
    - Sync search (:520), then slicing.
    - Every one of the 28,600 samples is read.
- **ppe sizing** (`ppe_core.c:23-46`): PPE_OVERSAMPLE 2 and
    PPE_MAX_RATE_BINS 8192 (:9-10).
    - **Offset row** (max_rate 0): n_rate 1 in both ppe instances, so one
        FFT per pass.
    - **Chirp, preamble ppe:** n_part = 5·10 = 50 (`burst_demod_core.c:127`).
        The rate maximum is 1e-6·(50·4)² = 0.04 (:138). dr = 1/(2·50²) = 2e-4,
        so nr = 401, and nfft = 64·4 = 256.
    - **Chirp, payload ppe:** rm = min(1e-6·200²·2, 0.02) = 0.02 (:486-487).
        dr = 1/(2·93²), so nr = 692, rounded up to the odd 693. nfft = 128·4 =
        512\.
    - In total, chirp runs about 4·401 + 693 ≈ 2,300 FFTs where offset runs
        5\. That is the 9-10x.
- **Published (portable):** offset 0.634 ms and chirp 5.70 ms, which give
    45.1 and 5.01 MSa/s.
- **Units:** I gave the frequency in cycles per sample, which is what the
    bench uses. At fs = 4 MHz, 0.012 cycles per sample is 48 kHz and 6e-7
    cycles/sample² is 9.6 MHz/s. I left those conversions out of the labels
    to keep them short.

### c:burst_despreader::steps[sf=15] and [sf=63]

The C source is `native/benchmarks/bench_burst_despreader_core.c`. The two
rows differ only in sf (:63, :69-74).

- **Block and spc:** BENCH_N 65,536 (:25); SPS 4 (:28).
- **Input** (:51-57): an LFSR gives ±1 per sample with 0.05 in quadrature.
    It is not spread by the code.
- **Code** (:70-71): hashed, sf chips.
- **Create** (:73-74): code_len = sf (15 or 63), sps 4, seed freq 0, seed phase 0,
    bn_carrier 0.05, bn_code 0.01. No `set_acq` call, so there is no
    preamble.
- **Rounds:** the instance is never reset (:104-110).
- **Count:** credited per sample at :113-114. All samples are consumed,
    because the loop guard `n_out < max_out`
    (`burst_despreader_core.c:282`) never binds: cap is 65,536 (:81-83).
- **Why SF can still move cost:** each sample costs one cexpf, three
    correlator taps and an NCO step (`burst_despreader_core.c:285-302`). The
    symbol-boundary block (two loop-filter steps, three cabsf, an atan2f
    during the preamble, and the statistics) runs once per sf·4 samples
    (:304-372). Its share per sample shrinks as sf grows, so sf is a knob
    that can change cost per sample, even though published sf=15/31/63 read
    0.431/0.426/0.435 ms mean today.

### c:despreader::steps

The C source is `native/benchmarks/bench_despreader_core.c`.

- **Setup:** SF 127, SPS 8 (:16-17). The code comes from xorshift
    (:22-30).
- **Signal** (:35-43): the code at 8 samples per chip times
    exp(j·2π·5e-5·n), with no noise.
- **Create** (:51-52): bn_carrier 0.05, bn_code 0.005, no FLL, zeta 0.707,
    spacing 0.5, one period per bit.
- **Rounds:** `dp_despreader_reset` runs before each timed round (:56-63).
    The count is 65,536 per sample (:64-65).
- **Per-sample path** (`despreader_core.c:184-206`): `costas_wipeoff`,
    `dll_lock_accumulate` (the "off-peak noise tap (lock det)", :189), then
    `dll_accumulate` (early/prompt/late). Each period then runs the loop
    updates plus `dp_dll_lock_look`/`lock_epoch`.

### c:ppe::doppler and c:ppe::chirp

The C source is `native/benchmarks/bench_ppe_core.c`.

- **doppler:** L 4,096, max_rate 0 (:57), and a tone at 0.05 cycles per
    sample (:33).
- **chirp:** L 512, max_rate 5e-5 (:58), and the tone sweeps at 1e-5
    cycles per sample squared (:33).
- **Count:** L per estimate, credited per sample (:47).
- **Sizing** (`ppe_core.c:23`, :36-45):
    - doppler: nfft = 4096·4 = 16,384, with one row.
    - chirp: nfft = 512·4 = 2,048. dr = 1/(2·512²) = 1.907e-6, so nr =
        (size_t)52.43 + 1 = 53, which is already odd. The bench prints n_rate
        at :45.
- **Per estimate** (`ppe_core.c:141-170`): `dp_kaiser_window` on every
    call (:148). Then, for each rate, `row_peak` (:89-139): a dechirp with
    cexpf per sample, the FFT, `dp_magnitude_db_cf32`, `dp_find_peaks_f32`
    and a mean.
- **"The setting BurstDemod uses unless a rate is declared":** BurstDemod's
    max_rate defaults to 0 (`objects/burst_demod.toml:72-74`), and
    `dp_ppe_create(..., 0)` collapses to a single FFT (`ppe_core.h:11-12`).

## COUNT PROBLEMS

No row's recorded count is clearly wrong. Every kept row reads all the
input it is credited with. What I found:

1. **c:burst_acq::push\[\*\]: the count is not checked, and the setup could
    break it. Probably not happening today.**
    - The bench gives the push only 16 result slots
        (`bench_burst_acq_core.c:30`, :108) and never reads
        `dp_burst_acq_consumed()`.
    - The engine **stops taking input** once the result array is full: "A
        FULL @p result takes nothing more ... and otherwise stops on that
        frame's boundary" (`acq_core.h:1609-1632`; code at
        `acq_core.c:2547-2556`). The bench would still credit all 32,768
        samples.
    - The input makes that more likely than the design pfa suggests. It is
        real-valued (±1 at a fixed 1.15° phase, :54-60), so correlation cells
        have half-normal magnitudes. The mean-CFAR gate assumes complex noise
        (Rayleigh): the gate is `threshold * noise_est` with threshold =
        eta·√(2/π) (`acq_core.c:456-463`, :1153-1158).
    - My estimate for the sf=15/spc=2 row (30 cells, 1,092 dwells per call)
        is about 1% per cell against the design 3e-4. That is enough to fill
        16 slots early.
    - **Against that:** the published times scale like full consumption of
        an FFT correlation. Per dwell they are 0.41 / 1.17 / 2.65 / 5.9 µs for
        30 / 60 / 126 / 252-sample periods, close to n log n. An early stop
        would make the per-sample cost fall as the period grows, and it
        rises.
    - **Not determined.** It needs a run: check `sink` (16 every round means
        full) or `dp_burst_acq_consumed(a) == BENCH_N` after the timed push.
    - **Suggested fix in the bench:** a fail-fast check, as the burst_demod
        and burst_despreader benches have (`bench_burst_demod_core.c:175-185`,
        `bench_burst_despreader_core.c:85-93`). Complex noise as input would
        also help.
1. **py:burst_demod::test_bench_demod\_\* (CUT): the count matches the
    samples read, but the case is wrong.**
    - `d.demod(bursts)` hands a whole 65,536-sample block in one call
        (`bench_burst_demod.py:76`). `demod()` treats everything after the
        2,550-sample preamble as payload.
    - `despread_data` runs over all 62,986 remaining samples, twice
        (`burst_demod_core.c:276`, :470, :479, :491). That is about 1,015
        symbols, against the frame's 93.
    - It also builds a 1,015-length ppe, with a 4,096-point FFT plan, on
        every call (:488), and runs the sync search over about 923 offsets
        (:520).
    - So 65,536 per call is honest about samples touched. But the number
        prices despreading a 1,015-symbol "payload", not demodulating bursts.
        A real caller (DsssBurstReceiver) hands one burst window per call.
    - It is cut, and c:burst_demod::demod[offset] is the honest twin.
1. **Small overcount from the carry (negligible, no action).** A push
    buffers, without searching, the remainder that does not complete a
    frame. With depth 1 (see the construct evidence; the C rows' 65 dB-Hz is
    stronger still):
    - the C acq rows carry 8 samples of 32,768;
    - py:burst_acq carries 65,536 - 128·510 = 256 (0.4%).
    - Inferred from the frame arithmetic, not observed.
1. **Different statistics (not a count, but it skews twins).**
    - C rows are read from mean/iterations (`bench_report.py:822-826`).
    - The burst-chain Python rows record MSa_s from the **min** round
        (`_burst_stimulus.py:229-239`).
    - py:despreader records it from the **mean** (`bench_despreader.py:44`).
    - The gap is about 1% on these rows (portable min against mean). It
        does not explain any of the conflicts below.

## STILL CONFLICTING

These pairs still look contradictory even with correct labels. The source
explains some of them only partly.

1. **c:burst_demod::demod[clean] 51.0 against [offset] 45.1 (clean is CUT).**

    - Both run the identical code path: max_rate 0, one FFT per pass, one
        timing candidate, no re-estimate.
    - The only difference is the carrier (0 against 0.012 cycles per sample)
        and therefore the phase fed to every `cexpf` (`burst_demod_core.c:226`,
        :282).
    - The gap depends on the machine: native 0.366 against 0.533 ms (46%),
        portable 0.558 against 0.634 ms (13%). That points at libm's sin/cos
        for near-zero against full-circle arguments, not at the algorithm.
    - This is a hypothesis; the source cannot settle it. It is one reason to
        keep only offset.

1. **c:burst_capture::push[4 bursts] 27.7 against
    py:burst_capture::test_bench_push_bursts 45.9 (both KEPT).** The C
    kernel is slower than its own Python wrapper.

    |                  | C row                  | Python row  |
    | ---------------- | ---------------------- | ----------- |
    | code             | 31-chip                | 255-chip    |
    | samples per chip | 4                      | 2           |
    | period           | 124 samples            | 510 samples |
    | repetitions      | 4                      | 5           |
    | design C/N0      | 55 dB-Hz               | 60 dB-Hz    |
    | noise            | near-noiseless uniform | AWGN        |
    | statistic        | mean                   | min         |

    - The source suggests two candidates. First, 4x as many dwells per
        sample in C, each with a fixed cost. Second, a 124 = 2²·31 FFT length:
        the stimulus module measured a prime-factor length (4·127) running 21
        against 52 MSa/s end to end for this reason (`_burst_stimulus.py:32-45`).
    - Neither is measured here.
    - Recommendation: move the C bench to the chain geometry, then cut the
        Python row.
    - The same tension shows inside the C rows: C burst_acq at a 252-sample
        period (36.8) is slower per sample than py:burst_acq at 510 (56.9).
        Those also differ in input (real-valued ±1 against complex AWGN), pfa
        (1e-2 against 1e-3), reps (8 against 5) and statistic. The source does
        not say which of these dominates.

1. **c:despreader::steps 42.5 against c:burst_despreader::steps[sf=15] 152
    and [sf=63] 151 (all KEPT).** Two despreaders with the same loop
    structure are 3.6x apart per sample.

    - The source shows despreader doing more per sample: a fourth (lock
        noise) correlator, `dll_lock_accumulate` at `despreader_core.c:189`.
        It also runs the composed `costas_wipeoff`/`dll_accumulate` kernels.
    - burst_despreader does one inline cexpf plus three taps
        (`burst_despreader_core.c:285-302`).
    - Samples per chip (8 against 4) should not matter per sample.
    - One extra tap does not obviously account for 3.6x, so this is
        unexplained in size. The two definitions now say what each runs.

1. **py:burst_demod idle 64.9 against burst 72.4 (both CUT).** The bench
    docstring says "Both rows do the same work" (`bench_burst_demod.py:15`).

    - The source has a data-dependent branch. At spc 2 the timing search
        has two candidates (start 0 and 1, `burst_demod_core.c:354-400`).
    - If noise picks the second, `best_d != span` re-runs the four preamble
        passes (:429-436). That is about 10,200 extra cexpf samples against
        about 126,000 for the two despreads, roughly the 11% seen.
    - Consistent with the source, not verified.

1. **py:burst_despreader idle 127 against bursts 133 (both CUT).**

    - The per-sample code is the same.
    - Hypothesis: `car_phase` is never wrapped (`burst_despreader_core.c:285`,
        :287, :342), so on noise, where the carrier loop wanders, cexpf gets
        ever larger arguments.
    - Side observation for the maintainer: casting an unbounded double
        phase to float loses phase resolution on long streams.

1. **py:ppe 14.4 (both rows) against c:ppe::doppler 18.4.** The Python rows
    use a 262,144-point FFT (65,536 × 4) against C's 16,384. Same estimator,
    larger size, plus the binding. The Python rows are cut.

## Could not determine (no run allowed)

- Whether c:burst_acq push fills its 16 result slots and stops early
    (COUNT PROBLEMS 1).
- What makes C burst_capture slower than its Python twin, and the size of
    the despreader against burst_despreader gap (STILL CONFLICTING 2 and 3).
- The coherent depth the C burst_acq and burst_capture engines choose.
    From their design points I argued it is 1 for C burst_acq and for the
    Python geometry. I did not compute it for the C capture (55 dB-Hz at
    4 MHz).
- Whether the C burst_capture "4 bursts" row actually captures four bursts.
    The bench asserts nothing about it; the Python twin asserts it.
- The split of the 6.6 ms construct cost between the Pd quadrature and the
    FFT plans. The "about 9,000 evaluations" is a call count, and the
    source's own note says the quadrature is what create pays for.
