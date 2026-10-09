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
