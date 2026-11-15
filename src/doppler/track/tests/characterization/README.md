# Track characterizations

How the `track` loops behave across their operating envelope: phase 7,
"Explore", of
[adding-algorithms](../../../../../docs/dev/contributing/adding-algorithms.md),
run deliberately and never per push. What the category is and what a fast
twin does and does not cover is the package docstring's:
[`__init__.py`](__init__.py).

| subject                                                      | what it answers                                                                               | run                 | fast twin                  |
| ------------------------------------------------------------ | --------------------------------------------------------------------------------------------- | ------------------- | -------------------------- |
| [`pull_in/characterize.py`](pull_in/characterize.py)         | how far out `MpskReceiver`'s carrier and timing loops acquire                                 | `make characterize` | `test_pull_in_stress.py`   |
| [`ted_low_snr/characterize.py`](ted_low_snr/characterize.py) | Gardner or DTTL in `RateSync`, on the specification's gapped rectangular BPSK at Es/N0 = 3 dB | `make characterize` | `test_ted_low_snr_twin.py` |

`make characterize CHARACTERIZATIONS=<its script>` runs one subject alone.
The sweeps are seeded: a rerun at the same commit prints the same numbers.

## Results

### `ted_low_snr/characterize.py`

|         |                                                                                                                   |
| ------- | ----------------------------------------------------------------------------------------------------------------- |
| date    | 2026-11-15, 14:06 to 14:07 UTC                                                                                    |
| machine | `omni-x7358h`: Intel Core Ultra X7 358H, Linux 6.18 (WSL2), Python 3.13.18                                        |
| commit  | `0182a0f78` plus the counter's re-acquire, before it was committed                                                |
| command | `make characterize CHARACTERIZATIONS=src/doppler/track/tests/characterization/ted_low_snr/characterize.py` (60 s) |

Rectangular BPSK, 8 samples a symbol, Es/N0 3 dB, a 1 ppm data clock, into
`RateSync(pulse="iandd", m=4)` alone; 1000 frames a row, each one idle run,
128 dotted symbols, EB90, a 1 to 1017 octet payload and a CRC-16. Level 1x
is `RateSync`'s contract, unit symbol amplitude. Every row sees the same
symbols and the same noise.

| sym/s | G   | TED     | bn    | level | slips gap / data / other                                          | SER [99% CI]            | loss dB | lock in gap | lock_stat gap / data |
| ----- | --- | ------- | ----- | ----- | ----------------------------------------------------------------- | ----------------------- | ------- | ----------- | -------------------- |
| 3000  | 367 | gardner | 0.005 | 0.5x  | 0 / 0 / 0, 3 unaligned                                            | 0.0277 [0.0255, 0.0300] | +0.20   | 100%        | +0.15 / +0.41        |
| 3000  | 367 | gardner | 0.005 | 1x    | 47 / 8 / 3, 4 unaligned                                           | 0.0326 [0.0301, 0.0354] | +0.53   | 100%        | +0.14 / +0.39        |
| 3000  | 367 | gardner | 0.005 | 2x    | 622 / 216 / 8, 15 unaligned                                       | 0.0426 [0.0393, 0.0462] | +1.10   | 99%         | +0.11 / +0.32        |
| 3000  | 367 | gardner | 0.01  | 0.5x  | 68 / 91 / 3, 3 unaligned                                          | 0.0327 [0.0301, 0.0354] | +0.53   | 99%         | +0.14 / +0.37        |
| 3000  | 367 | gardner | 0.01  | 1x    | 507 / 51 / 1, 3 unaligned                                         | 0.0403 [0.0371, 0.0437] | +0.98   | 100%        | +0.12 / +0.35        |
| 3000  | 367 | gardner | 0.01  | 2x    | lost from frame 999: 1364 / 4850 / 5242 before it, 5476 unaligned | —                       | —       | 64%         | +0.07 / +0.20        |
| 3000  | 367 | dttl    | 0.005 | 0.5x  | 1 / 0 / 0, 2 unaligned                                            | 0.0289 [0.0266, 0.0313] | +0.28   | 100%        | +0.14 / +0.41        |
| 3000  | 367 | dttl    | 0.005 | 1x    | 81 / 4 / 0, 3 unaligned                                           | 0.0320 [0.0295, 0.0347] | +0.49   | 100%        | +0.14 / +0.39        |
| 3000  | 367 | dttl    | 0.005 | 2x    | 303 / 7 / 0, 2 unaligned                                          | 0.0350 [0.0323, 0.0380] | +0.67   | 100%        | +0.13 / +0.37        |
| 3000  | 367 | dttl    | 0.01  | 0.5x  | 276 / 21 / 3, 4 unaligned                                         | 0.0341 [0.0315, 0.0370] | +0.62   | 100%        | +0.13 / +0.37        |
| 3000  | 367 | dttl    | 0.01  | 1x    | 591 / 81 / 7, 4 unaligned                                         | 0.0374 [0.0344, 0.0405] | +0.81   | 100%        | +0.13 / +0.36        |
| 3000  | 367 | dttl    | 0.01  | 2x    | 1185 / 990 / 283, 606 unaligned                                   | 0.0425 [0.0392, 0.0461] | +1.10   | 99%         | +0.11 / +0.32        |
| 7000  | 357 | gardner | 0.005 | 0.5x  | 0 / 0 / 0, 3 unaligned                                            | 0.0270 [0.0248, 0.0292] | +0.15   | 100%        | +0.15 / +0.41        |
| 7000  | 357 | gardner | 0.005 | 1x    | 62 / 6 / 0, 1 unaligned                                           | 0.0294 [0.0271, 0.0319] | +0.32   | 100%        | +0.14 / +0.38        |
| 7000  | 357 | gardner | 0.005 | 2x    | 595 / 226 / 1, 1 unaligned                                        | 0.0399 [0.0368, 0.0433] | +0.96   | 99%         | +0.11 / +0.32        |
| 7000  | 357 | gardner | 0.01  | 0.5x  | 89 / 104 / 1, 1 unaligned                                         | 0.0313 [0.0288, 0.0340] | +0.44   | 99%         | +0.14 / +0.36        |
| 7000  | 357 | gardner | 0.01  | 1x    | 501 / 50 / 5, 2 unaligned                                         | 0.0352 [0.0324, 0.0382] | +0.68   | 100%        | +0.13 / +0.35        |
| 7000  | 357 | gardner | 0.01  | 2x    | 1432 / 4885 / 4505, 5275 unaligned                                | 0.0936 [0.0862, 0.1015] | +3.31   | 63%         | +0.07 / +0.20        |
| 7000  | 357 | dttl    | 0.005 | 0.5x  | 4 / 0 / 0, 2 unaligned                                            | 0.0278 [0.0256, 0.0301] | +0.21   | 100%        | +0.15 / +0.41        |
| 7000  | 357 | dttl    | 0.005 | 1x    | 65 / 5 / 0, 1 unaligned                                           | 0.0295 [0.0272, 0.0320] | +0.33   | 100%        | +0.14 / +0.39        |
| 7000  | 357 | dttl    | 0.005 | 2x    | 282 / 10 / 1, 2 unaligned                                         | 0.0307 [0.0283, 0.0333] | +0.40   | 100%        | +0.14 / +0.37        |
| 7000  | 357 | dttl    | 0.01  | 0.5x  | 256 / 29 / 3, 3 unaligned                                         | 0.0305 [0.0281, 0.0331] | +0.40   | 100%        | +0.14 / +0.37        |
| 7000  | 357 | dttl    | 0.01  | 1x    | 549 / 76 / 8, 28 unaligned                                        | 0.0336 [0.0310, 0.0365] | +0.59   | 100%        | +0.13 / +0.36        |
| 7000  | 357 | dttl    | 0.01  | 2x    | 1155 / 1035 / 237, 385 unaligned                                  | 0.0411 [0.0378, 0.0445] | +1.02   | 99%         | +0.11 / +0.32        |

Reading the columns: slips are whole symbols, counted by the shared
counter `src/doppler/track/tests/_frame_slips.py` from 256-symbol
markers that tile every frame from its sync word to its end, each one's
lag detected against truth. *gap* is a change across the idle run and the
dotted preamble, which a frame sync recovers; *data* is a change inside a
frame, which costs it; *other* spans a marker that was not detected. The
count is net per bracket, so a slip that returns inside one 256-symbol
block or one idle run is not seen. *unaligned* counts markers that
missed even a wide search; the counter re-acquires at the next frame's
head, so a change across them is still counted, as *other*. A row whose
stream is never re-acquired reads "lost from frame N", with no SER or
loss. *SER* is scored only on blocks whose alignment agrees with the
block before, so it is decision quality between slips; read it with the
slip columns, never alone.

**Nearly every slip is in the idle run, and costs no frame.** At the
receiver's bandwidth (0.005) and the contracted level, Gardner slipped
47 / 8 (gap / data) symbols at 3000 sym/s and 62 / 6 at 7000; the DTTL
81 / 4 and 65 / 5.

**The level matters more than the detector.** Gardner's loop gain goes as
`A^2`: at half the contracted level it slipped nothing in 1000 frames at
either rate (loss 0.15 to 0.20 dB); at twice it slipped 846 and 822
symbols, 216 and 226 of them in data. The DTTL's goes as `A^1`: 1 and 4 slips at
half, about 300 at twice, but only 7 and 10 in data. Over-driven, Gardner
loses frames and the DTTL does not.

**The detectors tie at the contract.** Loss 0.53 against 0.49 dB at 3000
sym/s and 0.32 against 0.33 at 7000, Gardner against DTTL, with
overlapping intervals; data slips 8 against 4 and 6 against 5. The DTTL
slips more in the gap at 3000 (81 against 47) but not at 7000 (65 against
62), so "the DTTL slips more in the gap" is not established.

**`locked` does not see the gap.** It held through 100% of the idle-run
symbols at the receiver's bandwidth, while `lock_stat` fell from 0.38-0.39
to 0.14 inside it.

**`RateSync`'s default bandwidth, 0.01, is too wide for this floor** with
either detector: 556 to 679 slips at the contracted level, and at 0.5x
Gardner already slips in data (91 and 104). At 2x it slips about 4,900 symbols in data at either rate, with over 5,000 markers missing, and the counter loses the 3000 sym/s stream in its last frame; the DTTL slips about 1,000.

**What this does not say.** One Es/N0, so no crossover: the header's
"degrades faster at low SNR" is a trend, and this run shows only that at
the floor the DTTL's SER and data slips are no worse (gh-751). No timing
drift is reported: the `mu` probe aliases on the hot rows, so nothing here
is derived from it. It is `RateSync` alone and does not reproduce gh-3079,
where the whole async-DSSS receiver's Gardner loop fails at 5 dB and its
DTTL holds. An over-driven Gardner loop is what slips in data here, but
the receiver does not over-drive its own: measured at the specification
point it hands the loop about 0.5 to 0.6 of the contracted level, because
its AGC holds signal plus noise at the reference (gh-3079's record). So
gh-3079's cause lies elsewhere.
