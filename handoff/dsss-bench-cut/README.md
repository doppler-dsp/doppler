# DSSS-family benchmark cut: handoff (delete this directory before the PR)

The owner's direction (2026-10-16), the pilot of the page-wide cut that
replaces #2386's "label all 795":

- **Over-benching.** A row exists only if it answers a question someone asks
    AND reads differently from every kept sibling.
- **A sweep over a knob that can change cost per sample keeps its two
    ends.** Owner's example: `HalfbandDecimator` blocks 1,024 and 65,536 of
    four. A knob that cannot (non-coherent epochs `nc`) keeps one row.
- **FFT is the exception.** "We will have more" lengths, not fewer.
- **No hand edits around jm.** No `status_allow` entries and no deleted jm
    scaffolds. The 7 Python bench files that would lose every row stay
    UNTOUCHED until jm ships a per-object opt-out (just-buildit/just-makeit#2316):
    `bench_async_dsss_pool.py`, `bench_async_dsss_receiver.py`,
    `bench_burst_demod.py`, `bench_burst_despreader.py`,
    `bench_despreader.py`, `bench_ppe.py`, `bench_dsss_burst_receiver.py`.
    Cutting rows inside the AUTHORED bench files is fine (owner's answer).
- **The list.** `keep-cut.md`: 94 rows, 37 KEEP, 57 CUT, with the kept row
    covering each cut. The owner approved it with these recommendations:
    1. Fix the carrier-acq C bench (#1967, #2388): a real BPSK, credit only
        the samples read, pad 4 and pad 16. No row that times a give-up.
    1. One construct row: keep `py:burst_acq::test_bench_construct`. The
        `dsss_burst_receiver` one waits with its file, per the jm rule above.
    1. Wideband (3.069 Mcps) is cut as a middle chip rate.
    1. The thread sweep keeps 1 and 8. The run is pinned to 4 of the box's 10
        cores plus their SMT siblings (logical CPUs 0-3 and 10-13), so 8
        threads run two to a core and read below 4. Say so in the label. #2394
        is the pinning question.
    1. Fix the async receiver stimulus.
    1. No cold-start row.

## Done in feeb7769a, each bench built and run through `make bench`

| bench                 | change                                                                                                                                                                                                                                                                                                                    | measured here (swiftgo, not the publish box)                                                                                                                                                                |
| --------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `acq`                 | one detection target, the API default 0.9 (`objects/acq.toml:91-94`); cut wideband, the U17k pair, `_t2`/`_t4`, and the unpublished `D154_t2`/`_t4` (two-ends rule)                                                                                                                                                       | 12 rows; the no-Doppler row is now **`native_nc90`**, so the HEADLINE label moves there from `native_nc256` (both are published, at the same 54 MSa/s)                                                      |
| `acquire`             | cut the inline `dp_fftfreq_index` row: a compile-time constant grid made it time a vectorized loop                                                                                                                                                                                                                        | `bin_to_signed` 1.2 ns/call                                                                                                                                                                                 |
| `async_dsss_receiver` | stimulus fixed: the code runs on the global sample (it restarted every symbol, a jump of 829 chips at 5 Mcps); timed rounds take the NEXT contiguous 65,536 samples, not a replay; the lead-in comes from `ASYNC_DSSS_RX_CELL_PULLIN`; tracking AND `code_locked` are required before and after timing; warm and cold cut | `steps[cell,sf=7]` 38.7 ns/sample, `steps[cell,sf=1023,5Mcps]` 33.2 ns/sample. RENAMED: the old stimulus never held code lock at 5 Mcps (probe: `code_locked=0` throughout), and replay broke it at 7 chips |
| `burst_acq`           | two period ends (sf·spc 30 and 252); input is complex Gaussian noise (it was a ±1 LFSR the CFAR read as detections); every push must consume its whole block (`dp_burst_acq_consumed`)                                                                                                                                    | the check caught it on the old input: 20,790 of 32,768 read. Now 44.1 / 35.3 MSa/s (published 72.3 / 36.8: the sf 15 row was inflated about 1.6×)                                                           |
| `burst_capture`       | keep `push[4 bursts]`; every push must hand back exactly 4 windows                                                                                                                                                                                                                                                        | 38.6 ns/sample                                                                                                                                                                                              |
| `burst_demod`         | cut `clean`; header corrected (demod checks no CRC, #1022)                                                                                                                                                                                                                                                                | offset 0.67 ms, chirp 7.2 ms per burst                                                                                                                                                                      |
| `burst_despreader`    | cut `sf=31`                                                                                                                                                                                                                                                                                                               | 7.5 ns/sample at both ends                                                                                                                                                                                  |
| `dsss_receiver`       | cut the C `search` row (it timed zeros on a 7-chip code)                                                                                                                                                                                                                                                                  | track 34.0 ns/sample                                                                                                                                                                                        |

## Left to do

1. **`bench_carrier_acq_core.c`: rewrite as `detect[pad=4]` and
    `detect[pad=16]`.** Mirror the Python file's `detect` case:
    - rect BPSK at 4 samples per symbol (1 Msym/s at 4 MSa/s), 44 kHz off
        center, 20 dB Es/N0;
    - `resolution_hz = FS / 4096` (the default, symbol rate ÷ 10, gives a
        40-sample block);
    - `sequential = true`, `reset` before every timed call;
    - require `ready`, `|residual_hz - 44 kHz| < 2 × resolution`, and folded
        `n_blocks × 4096 < BENCH_N`;
    - record `JM_BENCH_SAMPLE` with count = the samples folded;
    - delete the comments claiming a real BPSK and "more blocks before
        deciding".
1. **`bench_dsss_receiver_core.c`.** The track section's comments (about
    lines 85-115) still name "the search row above".
1. **Python cuts inside files that keep a row:**
    - `bench_carrier_acq.py`: cut the `detect` tests (`search` stays), and
        fix the docstring's false "same zero-pads as the C twin" claim;
    - `bench_burst_acq.py`: cut `push_idle` and `push_bursts`;
    - `bench_burst_capture.py`: cut `push_idle`, `push_bursts_backed` and
        `construct`;
    - `bench_dsss_receiver.py`: cut `track`.
1. **Labels** (`benchmarks/labels.toml`, then delete each labelled row's
    line from `benchmarks/.labels-exempt`). Drafts are in `A/B/C.toml`, with
    file:line evidence in `*-notes.md`. They are agent drafts: check each
    against the source before using it.
    - Label only kept rows whose measurement is UNCHANGED and which v0.72.0
        publishes.
    - A renamed row (`detect[...]`, `steps[cell,sf=...]`) is not published
        yet, so it gets its label at release, when the label gate asks.
    - A CUT row stays on the page until v0.73.0 measures without it, so its
        exempt line, or its existing label, STAYS. The exempt list may not
        grow.
    - Correct the three published `burst_demod` labels (they claim a CRC
        check).
    - Move `headline = true` to `c:acq::native_nc90`.
1. **Prove each new check by sabotage**, and record the outcome per
    sabotage:
    - **burst_acq:** the old ±1 input must fail the consumption check;
    - **burst_capture:** 0 bursts must fail its 4-window check;
    - **async_dsss_receiver:** a per-symbol code restart must fail the hold
        check.
1. **Run all of:**
    - `make bench BENCH_ARGS="--c-only acq acquire async_dsss_receiver burst_acq burst_capture burst_demod burst_despreader carrier_acq dsss_receiver"`;
    - `make bench-python` on the changed files;
    - `make bench-docs` (re-render), then `make lint`, `make test`,
        `make test-python` and `make docs-check`.
1. **Issues:**
    - The PR closes #1967 and #2388.
    - File one: "the cold path may process samples twice". B-notes, COUNT
        PROBLEMS 1, says that after detection the refine runs every remaining
        sample and then the tracking chain runs them again. It is UNVERIFIED;
        check it in `async_dsss_receiver_core.c` first.
    - File one: the pool bench never checks that an emitter is assigned
        (B-notes).

**Coming:** jm will update canonical (just-buildit.github.io) soon, and
`standard-check` then fails until doppler re-vendors.
