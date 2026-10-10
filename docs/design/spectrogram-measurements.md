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
