# The Spectrogram — the measurement record

*The dated record behind [the design page](spectrogram.md): what each step of
its §5 measured, in the order it was measured, with the method, the numbers,
the caveats and the wrong guesses. The design page states what is; this page
is why. Section numbers are shared — a `§5.1` cited in an issue, a harness or
a code comment is the same entry here and there — and nothing is rewritten
after the fact: a later entry corrects an earlier one in its own words.*

*Every number comes from something re-runnable: a program, a harness under
`native/`, or a script the entry names. Where a number was taken on a machine
that was not quiet or not pinned, the entry says so beside the number.*

All timings: AMD Ryzen AI 9 465, GCC 16.2.1, a Release build with
`DOPPLER_NATIVE=OFF` (no `-march=native`), **not pinned to a core**, load
average 1.8–2.8 while they ran. This part's two core classes differ, so an
unpinned number is a statement about this run, not a ceiling — the
comparisons below are therefore between builds measured **interleaved in the
same minutes**, and read as ratios and medians, never as absolutes.

______________________________________________________________________

### 5.1 What was measured (2026-10-08) — promoting PSD's kernel changed nothing

**Question.** Splitting `psd_fold_frame` into a shared transform and a shared
dB conversion, so that `dp_psd_frame_power` / `dp_psd_frame_db` run the code
`accumulate` runs, must not change one bit of what `PSD` returns. The
existing PSD tests are not enough evidence for that: they check tolerances and
shapes, not bytes.

**Method.** A small C program linked against **two** builds of the library —
the one before the change and the one after — that dumps every readout to
stdout. For three windows (Hann, Kaiser β=7.5, Blackman-Harris), three lengths
(64, 100, 257 — the last two not powers of two), two zero-pad factors, and all
four averaging modes, it feeds a seeded stream of complex frames and then the
same length of real frames, and writes `psd_db`, `psd_dbhz`, `power_twosided`,
`power_onesided` and `noise_floor`. 72 configurations, 357,984 bytes. Run
against each library; compared with `cmp`.

**Result.** Byte-identical.

**The check on the check.** A comparison between two builds proves nothing if
they are the same build. `nm` finds `dp_psd_frame_db` in the second library
and not in the first, so they differ in exactly the symbols the change added.

**What this does not cover.** The new functions have no "before" to compare
with; they are pinned instead by C tests that require
`dp_psd_frame_db` to equal accumulate-then-`psd_db` bit for bit for every
window, with and without zero-padding, and each of those tests was shown to go
red when the code under it was broken (a frame that skips the zero-pad, a dB
conversion that divides by the gain instead of its square, a rectangular window
that is really Hann, a kernel that touches the average).

### 5.2 What was measured (2026-10-08) — `accumulate` is bimodal, before and after

**Question.** Did the refactor slow the PSD hot path?

**Method.** `bench_psd_core`, row `accumulate[nfft=1024]`, 12 runs of each
library, alternating, minimum over the run's own rounds as the bench reports.

**Result.**

| build             | min            | median | max  |
| ----------------- | -------------- | ------ | ---- |
| before the change | 1.78 ns/sample | 1.79   | 2.70 |
| after the change  | 1.77 ns/sample | 1.79   | 2.72 |

Both builds fall into one of **two modes**, about 1.79 and about 2.71
ns/sample, from run to run, in the same proportions. That is a memory-layout
effect (which of the working buffers share cache sets depends on where the
allocator put them in that process), and the unchanged library shows it, so it
is not the change. The first three comparisons of this entry happened to land
the new build in the slow mode twice and the old in the fast mode every time,
which read as a 50% regression until twelve interleaved pairs were taken.

**Lesson kept.** Three runs of a bimodal benchmark is a coin toss. A claim
about a hot path needs enough interleaved pairs to see both modes in both
builds, and is stated as medians.

### 5.3 The prototype (2026-10-08)

**What it was.** A numpy implementation in a scratch directory, not committed:
a one-shot reference (`rows_oneshot`), an incremental object with a carry of
fewer than `nfft` samples (`Inc`), and its `flush`. It ran before any C.

**Questions and answers.**

1. *Does the carry reproduce the one-shot rows for every split?* Seven shapes
    (`nfft`/`hop`: 8/8, 8/3, 8/1, 16/5, 64/16, 7/7, 1/1) × seven input lengths
    (0, 1, `nfft`−1, `nfft`, `nfft`+1, 3`nfft`+5, 1000) × seven chunkings
    (1, 7, `nfft`−1, `nfft`, `nfft`+1, 3`nfft`+5, and random), compared for
    row count and bit-exact values. **Zero disagreements.**
1. *Is the flushed row on the hop grid?* The row it emits equals row *k* of a
    one-shot run over the input zero-padded to that row's end, and exists
    exactly when the input holds a sample the earlier rows did not cover. 49
    shape × length cases. **Zero disagreements.** (392 cases in all, with the
    chunking half.)
1. *How wrong is a spectrogram built from `fft` plus `10·log10|X|²`?* A unit
    tone on a bin, 1024 points: it reads **54.18 dB** with a Hann window and
    **60.21 dB** with a rectangular one, against 0.00 dB under the PSD
    convention. The gap is exactly `20·log10(Σw)` in both. A second kernel
    would not be a rounding disagreement with `PSD`; it would be the wrong
    level by the window's coherent gain, and different for every window.

**The wrong guess.** The first run of the prototype printed **140
disagreements** out of 392 and the first reading was that the contract had a
subtlety — a carry that was off for some split. Classifying them showed the
carry was right in every case: the comparison treated an empty result of
shape `(0,)` and one of shape `(0, nfft)` as different, and the flush check
indexed a one-shot run that had no row *k* when the input held none. Both were
faults in the prototype's comparison, not in the idea. They were fixed in the
prototype, not argued away, and the corrected run is the 392-and-zero above.
That is also what a throwaway prototype is for: it found a bug in its own
test before a C test could inherit it.

**What it settled.** The contract in the design page's G1 and G2 is
satisfiable by a carry of fewer than `nfft` samples; and G3 is a requirement,
not a nicety, so PSD's kernel is promoted rather than copied.

**What it did not touch.** Speed, memory behaviour, threading and the ring: it
was numpy. The unknowns U1–U6 on the design page are all about those, and none
of them has a number yet.

### 5.4 The dB floor, measured (2026-10-10) — U5

**Question.** The design's U5. PSD's dB conversion clamps power at 1e-20
before the logarithm (`PSD_FLOOR`, `psd_core.c`), so no row reads below
−200 dB. How do an all-zero row and a near-zero one read, and does a caller
need to tell them apart?

**Method.** `native/validation/spectrogram_certify.c`:
`make build BUILD_TARGET=validate_spectrogram_certify`, then
`./build/native/validation/validate_spectrogram_certify` (`--emit` writes the
same numbers as CSV). Rows at `nfft` 1024, under each of the four windows
(Kaiser at β 8), of three inputs. The first is an on-bin tone (bin 37) of
power L dBFS, one row per level. The second is an all-zero frame. The third
is seeded complex Gaussian noise of total power L dBFS, **256 frames** per
window and level, because one frame's median scatters by about 0.2 dB. The
run is seeded and untimed, so a re-run of the same build reads the same
values. Intel Core Ultra 7 355 under WSL2, GCC 15.2.0, on main 1ea1cca0b plus
this entry's commits. The harness measures and decides nothing; until the
certification (#1941's A4, step 5) turns these numbers into limits, nothing
asserts them.

**A tone reads its level down to the floor, then the floor.** The tone's bin
reads L to four decimals under every window, from −120 to −200. At −201,
−205, −210 and −250 it reads **−200.0000**, and so does every bin of the
**all-zero frame** (1024 of 1024, under every window). A tone below
−200 dBFS and digital silence make the same row, bit for bit.

**What else a tone row shows is the window's leakage.** PSD's windows are
symmetric (length N − 1 in the cosine), so they are not orthogonal on the
N-point grid ([#2053](https://github.com/doppler-dsp/doppler/issues/2053)): an
on-bin tone under Hann leaks −6.0 dBc into bins ±1, then −69.7, −78.2 and
−83.7 dBc into ±2, ±3 and ±4. At −120 dBFS the first three pairs are above
the clamp and ±4 is under it, so exactly 7 bins of 1024 rise off the floor
(1017 at −200). Blackman-Harris's main lobe spans ±3 bins, so it raises the
same 7, and Kaiser β 8's sidelobes on the grid raise 25: both counts are
what a symmetric window's DFT predicts above −80 dBc. Only the rectangular
window is orthogonal on the grid, and only its tone bin rises (1023 at
−200).

**Wideband noise reaches the floor sooner, by about `10·log10(nfft)`.** A
frame of complex noise spreads its power over the bins. Against PSD's tone
reference each bin's power is exponential with mean `μ = P·ENBW/n`, so its
median is `L + 10·log10(ENBW/n) + 10·log10(ln 2)` dB. From each window's own
ENBW at `nfft` 1024, that is L − 31.7 for rect, L − 29.9 for Hann, L − 29.5 for
Kaiser β 8 and L − 28.7 for Blackman-Harris. Where the median sits clear of
the clamp (expected above −199.5 dB), the mean of 256 frames' medians is
within **0.02 dB** of the formula for every window. That is well inside the
0.46 dB that separates Hann from Kaiser, so the windows' ENBW are separately
confirmed. The same model gives the share of bins at the clamp,
`n·(1 − exp(−10⁻²⁰/μ))`, and the measured mean is within 2.3 bins of it
everywhere:

| L, dBFS | Hann            | Kaiser β 8      | Blackman-Harris | rect            |
| ------- | --------------- | --------------- | --------------- | --------------- |
| −150    | 7.2 (7.0)       | 6.4 (6.3)       | 5.0 (5.2)       | 10.8 (10.4)     |
| −160    | 67.1 (67.5)     | 60.7 (61.0)     | 51.0 (51.0)     | 99.8 (99.7)     |
| −170    | 505.9 (506.3)   | 470.9 (469.9)   | 411.1 (409.3)   | 658.5 (656.2)   |
| −175    | 905.3 (905.5)   | 876.1 (877.2)   | 821.1 (820.1)   | 983.4 (983.8)   |
| −180    | 1022.8 (1022.9) | 1021.9 (1021.8) | 1017.8 (1017.8) | 1024.0 (1024.0) |

*Bins of 1024 reading exactly −200 dB: the mean of 256 frames, and the
model's expectation in brackets.*

So noise of total power −180 dBFS leaves, on average, about 1 bin of 1024
above the floor under Hann, 2 under Kaiser, 6 under Blackman-Harris and none
under rect, and at −190 none under any window. These are averages, not
bounds: one frame scatters around them (up to 15 bins at the clamp at
−150, where the mean is 7). The offset is `10·log10(ENBW/n)`, so the level at
which noise vanishes rises 3 dB with each doubling of `nfft`. That follows
from the formula; this entry does not measure other sizes.

**The falsifier fired.** U5 rested on the guess that nothing a caller feeds
a spectrogram is that small. Float sources in the tree make such samples. A
wfm source's `level` is in dBFS with only an upper bound (`wfm_compose.h`,
the source's `level`, `<= 0`), and the composer applies it as a gain of
`10^(level/20)` (`wfm_compose.c`, `level → gain`). So a wfm noise source at
`level` −190 is a valid input, and its rows at `nfft` 1024 can't be told from
silence. float32 holds such samples as ordinary normal numbers (its smallest
normal is an amplitude of about −759 dBFS), so nothing upstream rounds them
to zero first.

**The answer to U5.** By design, for the dB face. A logarithm has to stop
somewhere, and the clamp is where it stops; what was missing was a statement
of it. A caller that has to tell digital zero from a signal under the floor
reads the samples, or the linear row once `mode = power` is enabled (#1968).
The clamp lives in the dB conversion alone: `dp_psd_frame_linear`, power
mode's path, applies none. The linear row has float32's own floor instead: a
bin's power goes subnormal below about −379 dB and rounds to zero below about
−449 dB, so it keeps zero and tiny apart down to there.

### 5.5 What callers want at the end of a stream (2026-10-10) — U6

**Question.** The design's U6. `flush` is explicit; does any caller want it
implied by closing the source?

**Method.** A survey of main at 1ea1cca0b, with nothing timed. It covers
every call of `dp_spectrogram_push` or `dp_spectrogram_flush` outside the
tests and the benchmarks, every loop #1894 plans to port onto the object,
the stream readers in the tree, and what each transport promises about its
end-of-stream marker.

**Callers.** One: `native/examples/spectrogram_demo.c`. Its stream is its
own buffer, so it knows where the stream ends. It flushes explicitly, then
checks that a second flush owes nothing. The loops #1894 lists for porting
don't flush at all. `wfm_composition_demo.py` and `dsss_burst_demo.py` each
build their rows over a whole capture with `range(0, len(x) - nfft, hop)`,
which stops before the last frame. `specan`'s two re-blockers feed a live
display.

**What the transports promise.** End of stream is a marker, and how
reliable it is depends on the transport (`stream.h`, `dp_stream_send_eos`,
"What it does NOT promise"; `io-termination.md`, on PUSH/PULL):

| transport               | the end-of-stream marker                                                    |
| ----------------------- | --------------------------------------------------------------------------- |
| PUB/SUB (`dp_sub_recv`) | at-most-once: it can be dropped like any frame, so the reader never sees it |
| PUSH/PULL (`Pull.recv`) | at-least-once: it can arrive twice, and in a pool it reaches ONE consumer   |

**What the readers do with it.**

| reader                                            | on the marker                                                     |
| ------------------------------------------------- | ----------------------------------------------------------------- |
| C `dp_sub_recv` (`stream.h`)                      | returns `DP_ERR_EOF`                                              |
| Python `Subscriber.recv`, `Pull.recv`             | raise `EOFError`                                                  |
| `native/examples/spectrum_analyzer.c`             | `if (rc != DP_OK) continue;`: carries on as after a timeout       |
| `specan`'s `SocketSource.read`, `PullSource.read` | catch the `EOFError` and return what is buffered, as on a timeout |
| `specan`'s `FileSource.read`                      | none: at end of file it seeks to the start and loops              |

**The answer to U6.** `flush` stays explicit, because only the caller knows
where its own stream ends, and the transports' marker is not reliable enough
to imply it. An implied flush behind a dropped PUB/SUB marker would never
come, losing the last row; behind a duplicated PUSH/PULL marker it would
come twice, the second time a zero-padded row in the middle of the next
producer's stream. The two live readers above already treat the marker as
a pause, which suits a display that has no use for a last row. "Implied by
closing the source" also has no home in the object: the Spectrogram never
learns where its samples came from (§7 of the design), so only the code that
composes a source with it could join the two, and that code is the caller.

### 5.6 What the carry's copy costs (2026-10-10) — U1

**Method, shared by 5.6 to 5.9.** One run of
`make bench-interleaved VERSION=0.66.0-a4 K=5` at **c1ae84190**, a commit
on main, on **cachyos-x8664-ai465**:

- **The machine:** AMD Ryzen AI 9 465, kernel 7.2.9-2-cachyos, governor
    `performance`, boost on, platform profile `performance`, GCC 16.2.1.
- **Pinning:** every measurement ran on **cpus 0–3 and 10–13**, the fastest
    core class (`bench_report.fastest_cpus`). The builds ran unpinned.
- **Two Release builds,** run alternately, five passes each: *portable*
    (`-O3 -march=x86-64-v2 -ffast-math`) and *native*
    (`-O3 -march=native -mprefer-vector-width=256 -ffast-math`). Every pair
    of cells below is portable / native.
- **When:** 2026-10-10, 09:15 to 10:24 UTC. No other job was started on the
    machine. One `ps` sample during the first pass showed the bench as the
    only process using a measurable share of a core. Load was not logged
    through the run.
- **The rows** are `bench_spectrogram_core`'s `push`, `direct` and `chunk` rows
    and `bench_psd_core`'s `fft`, `frame_power` and `frame_db` rows (#2061).

**The data and the arithmetic are committed.** `bench_interleaved.py` keeps
only each row's lowest-mean pass and deletes the per-pass snapshots with its
worktrees, so the ten were saved as they were written. They are in
`docs/design/spectrogram-measurements/u1u4/`, filtered to the spectrogram
and PSD rows. Every table in 5.6 to 5.9 is the output of
`docs/design/spectrogram-measurements/u1u4.py`, run on them. Its `--check`
is in `make docs-invariants`, so a cell cannot be edited by hand.

- A row's cost in one pass is the bench's per-pass minimum per unit (sample
    or frame).
- A cell is the median of that over the five passes.
- A *spread* is (max − min) / median over the same passes.

The decision rules were set before the run, in A4's plan as agreed with the
coordinator, but were not committed to the tree until now:

- **U1** closes if push/direct is at most 1.05 at every shape, and is
    falsified if the ratio scatters more than 5% across passes.
- **U2's** model gets no refit and must hold to 10%.
- **U3's** dB conversion "dominates" above 50% of a row.
- **U4** must agree with U3 times U1 to within 10%.

**What was measured.** `push` is the object, fed the whole block. `direct`
is a hand-written loop that calls `dp_psd_frame_db` on `x + k·hop`, with no
ring, carry or copy, and it refuses to run unless its rows equal `push`'s bit
for bit. Each pair runs adjacently, in an order swapped every round.

<!-- spectrogram-u1u4:u1:start -->

| nfft   | hop    | push, ns/sample | direct, ns/sample | push − direct | push/direct   | worst pass    | ratio spread |
| ------ | ------ | --------------- | ----------------- | ------------- | ------------- | ------------- | ------------ |
| 256    | 64     | 27.319 / 26.025 | 27.222 / 25.967   | 0.078 / 0.117 | 1.003 / 1.005 | 1.004 / 1.007 | 0.3% / 0.5%  |
| 256    | 256    | 6.904 / 6.617   | 6.856 / 6.529     | 0.087 / 0.087 | 1.013 / 1.013 | 1.015 / 1.016 | 0.8% / 1.2%  |
| 1,024  | 256    | 27.787 / 26.391 | 27.646 / 26.282   | 0.141 / 0.109 | 1.005 / 1.004 | 1.010 / 1.006 | 0.6% / 0.3%  |
| 1,024  | 1,024  | 7.111 / 6.786   | 7.019 / 6.718     | 0.106 / 0.086 | 1.015 / 1.013 | 1.017 / 1.015 | 1.3% / 0.7%  |
| 4,096  | 1,024  | 27.540 / 26.002 | 27.437 / 25.919   | 0.103 / 0.058 | 1.004 / 1.002 | 1.005 / 1.003 | 0.7% / 0.3%  |
| 4,096  | 4,096  | 7.406 / 7.038   | 7.276 / 6.887     | 0.138 / 0.123 | 1.019 / 1.017 | 1.023 / 1.023 | 0.8% / 0.6%  |
| 16,384 | 4,096  | 27.003 / 25.455 | 26.837 / 25.314   | 0.166 / 0.144 | 1.006 / 1.006 | 1.008 / 1.008 | 0.5% / 0.4%  |
| 16,384 | 16,384 | 7.756 / 7.333   | 7.595 / 7.249     | 0.178 / 0.119 | 1.024 / 1.017 | 1.025 / 1.027 | 1.0% / 2.4%  |
| 65,536 | 16,384 | 28.268 / 26.465 | 28.141 / 26.307   | 0.108 / 0.158 | 1.004 / 1.006 | 1.005 / 1.010 | 0.3% / 0.6%  |
| 65,536 | 65,536 | 8.106 / 7.711   | 8.005 / 7.433     | 0.101 / 0.243 | 1.013 / 1.033 | 1.018 / 1.037 | 0.6% / 2.2%  |

<!-- spectrogram-u1u4:u1:end -->

*push − direct is the median of each pass's difference, not the difference of
the two medians beside it. Worst pass is the largest single pass's ratio.*

**The answer to U1: closed, no bypass.** The carry's copy costs 0.06 to
0.24 ns per sample. The median ratio is at most 1.024 in the portable build
and 1.033 in the native one. The worst single pass anywhere is 1.037 (native,
65,536/65,536), so the 1.05 rule holds at every shape in every pass. The
ratio scatters at most 2.4% across passes, so the falsifier did not fire.

The copy weighs more at `hop = nfft` (1.3–3.3%) than at `nfft/4`
(0.2–0.6%). A quarter hop computes four rows per `nfft` samples, while each
sample is copied once either way.

The absolute rows move more than their ratio: up to 4.2% and 5.6% across
passes. That is the machine drifting under both rows of a pair at once, and
it is why the pairs are adjacent and the ratio is the quantity decided on.

### 5.7 Row latency, against the chunk size (2026-10-10) — U2

**In samples, there is none.** A row comes back with the push that delivers
its last sample. The C test pins that (`test_spectrogram_core.c` §15), and
the certification asserts it over every one-sample partition.

**In time, it is the cost of that push.** The model, fixed before the run,
has two parameters:

- *c_s*, the per-sample cost of the one-block push;
- *c_p*, the per-push overhead, read from the chunk-1 row as its per-sample
    cost minus *c_s*.

A chunk of *C* samples should then cost `c_s + c_p / C` per sample.

<!-- spectrogram-u1u4:u2:start -->

| *c_s*, ns/sample | spread      | *c_p*, ns per push |
| ---------------- | ----------- | ------------------ |
| 27.787 / 26.391  | 3.7% / 3.1% | 4.640 / 4.607      |

| chunk  | measured, ns/sample | predicted `c_s + c_p/C` | error         |
| ------ | ------------------- | ----------------------- | ------------- |
| 1      | 32.427 / 30.998     | (defines *c_p*)         | —             |
| 256    | 27.812 / 26.443     | 27.805 / 26.409         | +0.0% / +0.1% |
| 1,024  | 27.875 / 26.492     | 27.792 / 26.396         | +0.3% / +0.4% |
| 16,384 | 27.828 / 26.374     | 27.788 / 26.391         | +0.1% / -0.1% |

<!-- spectrogram-u1u4:u2:end -->

**The answer to U2, at `hop` 256.** The model holds to 0.4% with no refit,
inside its 10%. Read that for what it tests:

- **The 0.4% tests only *c_s*.** From 256 samples on, `c_p / C` is at most
    0.018 ns, under 0.07% of a sample's cost.
- **The chunk-1 row alone** carries *c_p*, about 4.6 ns per push, and that
    row also defines it. So one sample per push costs 17% more per sample
    than one block, and from 256 samples on the chunk size is invisible.

A row is ready when the push carrying its last sample returns. For a chunk
that completes one row, that push is mostly the row itself: about 7 µs at
`nfft` 1024 (entry 5.8's `frame_db`).

**What this does not cover.** As built (#2061), the chunk rows run at
`nfft` 1024 and `hop` 256 only, so U2 *against `hop`* is unmeasured. The
rows for the other hops wait on the bench's 32-row cap
(just-buildit/just-makeit#2188), and they are added to #2062's work beside
U1's `nfft/2` rows.

### 5.8 Where a row's time goes (2026-10-10) — U3

**What was measured.** `bench_psd_core` times PSD's per-frame kernel three
ways per `nfft`, in one interleaved loop:

- `fft`: PSD's own `dp_fft_create (nfft, -1, 1)` and `dp_fft_execute_cf32`.
- `frame_power`: the window, the FFT and the power fold.
- `frame_db`: all of that, then the dB conversion, which is the Spectrogram's
    row.

The shares below are per-pass differences, as fractions of `frame_db`.

<!-- spectrogram-u1u4:u3:start -->

| nfft   | fft, µs          | frame_power, µs   | frame_db, µs      | the FFT       | window + power | dB conversion | dB, ns per bin | frame_db spread |
| ------ | ---------------- | ----------------- | ----------------- | ------------- | -------------- | ------------- | -------------- | --------------- |
| 256    | 0.244 / 0.201    | 0.355 / 0.294     | 1.707 / 1.696     | 14.5% / 11.8% | 6.6% / 5.5%    | 78.9% / 82.7% | 5.27 / 5.47    | 4.0% / 1.7%     |
| 1,024  | 1.182 / 0.922    | 1.614 / 1.301     | 6.990 / 6.911     | 16.9% / 13.4% | 6.1% / 5.5%    | 76.9% / 81.2% | 5.25 / 5.48    | 3.4% / 1.4%     |
| 4,096  | 5.572 / 4.343    | 7.232 / 5.776     | 28.630 / 27.885   | 19.5% / 15.6% | 5.8% / 5.2%    | 74.7% / 79.3% | 5.22 / 5.40    | 3.2% / 1.0%     |
| 16,384 | 25.841 / 20.195  | 32.639 / 26.384   | 117.832 / 114.521 | 22.1% / 17.6% | 5.7% / 5.4%    | 72.2% / 77.0% | 5.20 / 5.38    | 3.2% / 1.0%     |
| 65,536 | 119.132 / 95.749 | 149.840 / 124.383 | 492.000 / 478.422 | 24.2% / 20.0% | 6.1% / 6.0%    | 69.6% / 74.0% | 5.21 / 5.41    | 2.9% / 1.6%     |

<!-- spectrogram-u1u4:u3:end -->

**The answer to U3: the dB conversion dominates.** It is **69.6–82.7%** of a
row at every size, at a nearly constant 5.2–5.5 ns per bin. Per bin, the
conversion (`psd_read_power`, `psd_core.c`) is a divide by the reference, a
clamp at the floor, a double-precision `log10` and a cast to float. The
`log10` is the likely bulk, but this bench does not separate it from the
other three.

The FFT is 12–24% of a row. The native build makes the FFT faster but not
the conversion, so the conversion's share grows there.

By the design's own rule, a cheaper conversion would change what the default
output mode should be. That is a decision about PSD's dB face, which every
PSD reading and both certifications pin byte for byte, so it is filed rather
than made here: #2074. Power mode (#1968) skips the conversion altogether.

**What this cannot separate.** `frame_power − fft` is the window and the
power fold together. As built, the bench cannot split them, and the
difference **understates** them. `frame_power`'s FFT runs on PSD's scratch,
which its own window loop has just written, while the `fft` row reads the
caller's frame cold. That matters most at 65,536, where the frame is 512 KB.

### 5.9 What one core sustains (2026-10-10) — U4

At `nfft` 1024 and `hop` 256 (75% overlap), one core of the fastest class
sustains this. Rates are rounded to the push row's spread.

<!-- spectrogram-u1u4:u4:start -->

| build    | push, ns/sample | samples per second | rows per second | `frame_db`/hop × push/direct | push against it |
| -------- | --------------- | ------------------ | --------------- | ---------------------------- | --------------- |
| portable | 27.79 (±3.7%)   | 36 M               | 141,000         | 27.44                        | +1.3%           |
| native   | 26.39 (±3.1%)   | 38 M               | 148,000         | 27.11                        | -2.6%           |

<!-- spectrogram-u1u4:u4:end -->

**About the cross-check.** It is U3's `frame_db` per row, divided by the hop
and multiplied by U1's push/direct. The measured push is within +1.3% /
−2.6% of it, inside the 10% the plan set. But it is not independent:
push / (`frame_db`/hop × push/direct) reduces to `direct`·hop / `frame_db`.
That is two benches timing one kernel, `bench_spectrogram_core`'s hand loop
and `bench_psd_core`'s row. What it shows is that the two benches agree, not
that the push is right.

**What that leaves a display.** At a stream of *f* samples per second, the
Spectrogram takes about *f* × 28 ns of one core: 28% at 10 MSa/s, and all of
it near 36 MSa/s. Three quarters of that is the dB conversion (U3), so a
display that reads power rows, or converts only the bins it draws, keeps
most of the core.

**Kept in mind when these rows are compared next.** The existing spectrogram
rows (`push[nfft=…,hop=…]`) are measured here in a new setting: 29
configurations interleaved, `push_max_out` outside the timer, and each chunk
given exact room. So a step on unchanged row names at the next release can
be the method, not the code. This run's merged set
(`benchmarks/published/v0.66.0-a4/`) stays on the measuring machine. It is a
characterization, not a release's published numbers.

### 5.12 Where a PSD frame's time goes (2026-10-10) — the #2094 baseline

**Method, shared by 5.12 to 5.14.** One run of
`make bench-interleaved VERSION=0.66.0-b2094 K=5` at **c36a042e0**, a commit
on main carrying #2113's rows and none of #2094's changes: no SIMD fold
(#2105), no fused passes, and #2117's dB conversion not yet in. It is the
BEFORE of all three, and runs once.

- **The machine:** cachyos-x8664-ai465, AMD Ryzen AI 9 465, kernel
    7.2.9-2-cachyos, governor `performance`, boost on, platform profile
    `performance`, GCC 16.2.1. Each was read from sysfs before the launch
    and is in the run's `doppler_meta`.
- **Pinning:** every measurement on **cpus 0–3 and 10–13**, as in 5.6. The
    builds ran unpinned.
- **Two Release builds,** portable and native, five alternating passes each,
    with the same flags as 5.6. Every pair of cells is portable / native.
- **When:** 2026-10-10, 16:46 to 17:56 UTC, under `systemd-inhibit   --what=idle`, in a detached worktree at exactly that commit. No other
    job ran on the machine. Each pass's snapshot was copied out as it was
    written, by a 1 s loop pinned to cpu 19, outside the measured class, and
    niced.
- **The data and the arithmetic are committed,** as for 5.6:
    `docs/design/spectrogram-measurements/b2094/`, filtered to the `psd::`,
    `acc_trace::` and `spectrogram::` rows. Every table here is the output
    of `b2094.py`, whose `--check` is in `make docs-invariants`. A cell is
    the median over the five passes of each pass's minimum per unit, and a
    spread is (max − min) / median. Both are `_record.py`'s, shared with
    `u1u4.py`.

**Not like-for-like with 5.8.** `accumulate_frame` and `frame_linear` joined
`bench_psd_core`'s rotation (#2113), which changes what runs before `fft`,
`frame_power` and `frame_db` in each round. #2117's before is this run's
rows, not 5.8's. The one unchanged row the two runs share agrees: the dB push
at 1024/256 reads 27.770 / 26.510 ns per sample here, against 27.787 / 26.391
in 5.6.

`accumulate_frame` is one frame of `dp_psd_accumulate`: window, FFT, power,
and the fold into the running mean. `frame_power` is the same without the
fold. So the window and power pass is `frame_power − fft`, and the fold
inside the frame is `accumulate_frame − frame_power`, each per bin.

<!-- spectrogram-b2094:frame:start -->

| nfft   | fft, µs          | frame_power, µs   | accumulate_frame, µs | accumulate / fft | window + power, ns/bin | fold, ns/bin | accumulate spread |
| ------ | ---------------- | ----------------- | -------------------- | ---------------- | ---------------------- | ------------ | ----------------- |
| 256    | 0.245 / 0.202    | 0.354 / 0.294     | 0.408 / 0.322        | 1.66 / 1.60      | 0.43 / 0.36            | 0.21 / 0.11  | 1.4% / 3.0%       |
| 1,024  | 1.180 / 0.920    | 1.616 / 1.300     | 1.828 / 1.395        | 1.55 / 1.51      | 0.42 / 0.37            | 0.21 / 0.09  | 0.9% / 0.9%       |
| 4,096  | 5.585 / 4.352    | 7.233 / 5.782     | 8.072 / 6.123        | 1.45 / 1.40      | 0.40 / 0.35            | 0.21 / 0.08  | 0.6% / 0.7%       |
| 16,384 | 25.896 / 20.238  | 32.719 / 26.435   | 35.414 / 27.276      | 1.36 / 1.35      | 0.41 / 0.38            | 0.16 / 0.05  | 0.7% / 0.5%       |
| 65,536 | 119.147 / 95.756 | 149.955 / 122.737 | 163.072 / 129.554    | 1.36 / 1.36      | 0.47 / 0.42            | 0.20 / 0.10  | 0.9% / 6.3%       |

<!-- spectrogram-b2094:frame:end -->

**#2094's measure is accumulate / fft: 1.36–1.66 portable, 1.35–1.60
native.** The FFT is 60–74% of an accumulated frame. The rest splits unevenly.
The window and power pass is 0.35–0.47 ns per bin in both builds. The fold
is 0.16–0.21 portable, but only 0.05–0.11 native. That is consistent with
the compiler vectorizing the scalar Welford loop under `-march=native`, which
was not checked in the binary. So the window and power pass is the
larger share of the overhead in both builds: about two thirds portable, and
about four fifths native.

That bounds what each of #2094's changes can buy. A fold that cost nothing
would leave `frame_power / fft`: at 1024, 1.37 portable and 1.41 native,
against today's 1.55 and 1.51. Anything below that has to come out of the
window and power pass, which is PR-c's (fusion). The target #2094 asks for
is set from these numbers in the PRs that change them, not here.

The native 65,536 row's 6.3% spread is not one outlier: its five passes run
from 124.8 to 133.0 µs, so that one cell carries about ±3%.

**A reading's two steps after the power,** at the three sizes #2113 gave a
`frame_linear` row. Normalisation is `frame_linear − frame_power`, and the
dB conversion and floor are `frame_db − frame_linear`, each per bin.

<!-- spectrogram-b2094:split:start -->

| nfft   | frame_linear, µs  | frame_db, µs      | normalisation, ns/bin | dB, ns/bin  | dB, share of frame_db | frame_db spread |
| ------ | ----------------- | ----------------- | --------------------- | ----------- | --------------------- | --------------- |
| 256    | 0.394 / 0.316     | 1.708 / 1.684     | 0.16 / 0.08           | 5.13 / 5.33 | 76.9% / 81.2%         | 3.4% / 3.3%     |
| 1,024  | 1.777 / 1.393     | 6.991 / 6.865     | 0.16 / 0.09           | 5.09 / 5.34 | 74.6% / 79.7%         | 2.8% / 2.8%     |
| 65,536 | 158.897 / 128.963 | 491.389 / 475.657 | 0.14 / 0.09           | 5.07 / 5.31 | 67.6% / 73.0%         | 3.1% / 4.1%     |

<!-- spectrogram-b2094:split:end -->

**The dB conversion is 5.1–5.3 ns per bin, two thirds to four fifths of a dB
frame,** and the native build does not help it: the scalar double `log10`
gains nothing from `-march=native`. Normalisation is under 0.2 ns per bin. This table is #2117's
before.

### 5.13 The fold on its own (2026-10-10) — the #2094 baseline

The method is 5.12's. `acc_trace::fold` is `dp_acc_trace_accumulate` alone,
over splitmix64 frames, in each of the four modes. The last row is the fold
inside PSD's frame from 5.12, for comparison.

<!-- spectrogram-b2094:fold:start -->

| mode, ns/bin            | 256         | 1,024       | 4,096       | 16,384      | 65,536      |
| ----------------------- | ----------- | ----------- | ----------- | ----------- | ----------- |
| `mean`                  | 0.21 / 0.08 | 0.21 / 0.08 | 0.20 / 0.08 | 0.20 / 0.08 | 0.20 / 0.09 |
| `exp`                   | 0.16 / 0.11 | 0.16 / 0.10 | 0.15 / 0.10 | 0.15 / 0.10 | 0.15 / 0.10 |
| `maxhold`               | 1.49 / 0.80 | 1.45 / 0.77 | 1.48 / 0.77 | 1.46 / 0.74 | 1.47 / 0.74 |
| `minhold`               | 1.47 / 0.77 | 1.44 / 0.76 | 1.46 / 0.76 | 1.46 / 0.76 | 1.45 / 0.74 |
| in PSD's frame (`mean`) | 0.21 / 0.11 | 0.21 / 0.09 | 0.21 / 0.08 | 0.16 / 0.05 | 0.20 / 0.10 |

<!-- spectrogram-b2094:fold:end -->

**`maxhold` and `minhold` are the slow folds: about 1.46 ns per bin portable
and 0.76 native, seven and nine times `mean`.** `mean` and `exp` are flat
across sizes. The fold inside PSD's frame matches the fold on its own within
0.03 ns per bin, except at 16,384, where it reads lower in both builds. That is
the two cache regimes #2113's bench header describes, not a defect. These are
#2105's before.

### 5.14 Power rows against dB rows (2026-10-10) — the #2094 baseline

The method is 5.12's. Each power row (`mode=power`, the default since #1968)
is timed in its dB row's group, in an order rotated every round. This is the
U4-for-power entry moved here from #1968.

<!-- spectrogram-b2094:rows:start -->

| nfft   | hop    | dB row, ns/sample | power row, ns/sample | dB / power  | power, MSa/s | power spread |
| ------ | ------ | ----------------- | -------------------- | ----------- | ------------ | ------------ |
| 256    | 64     | 27.204 / 26.233   | 6.584 / 5.491        | 4.13 / 4.83 | 152 / 182    | 2.4% / 7.3%  |
| 1,024  | 256    | 27.770 / 26.510   | 7.531 / 6.044        | 3.68 / 4.39 | 133 / 165    | 1.0% / 0.9%  |
| 65,536 | 16,384 | 28.198 / 26.605   | 9.357 / 7.696        | 3.01 / 3.48 | 107 / 130    | 5.3% / 1.0%  |

<!-- spectrogram-b2094:rows:end -->

**A power row is three to five times cheaper than a dB row: 107–152 MSa/s on
one core portable, 130–182 native, against 35–38 for dB.** The gap is
5.12's dB conversion, done once per bin per row at a quarter hop. A display
that reads power rows, or converts only the bins it draws, keeps that
difference. #2117 narrows it from the dB side.

This run's merged set (`benchmarks/published/v0.66.0-b2094/`) stays on the
measuring machine. It is a characterization, not a release's published
numbers.
