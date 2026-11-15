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
| date    | 2026-11-15, 13:10 to 13:11 UTC                                                                                    |
| machine | `omni-x7358h`: Intel Core Ultra X7 358H, Linux 6.18 (WSL2), Python 3.13.18                                        |
| commit  | `21874d1fb` plus this subject, before it was committed                                                            |
| command | `make characterize CHARACTERIZATIONS=src/doppler/track/tests/characterization/ted_low_snr/characterize.py` (22 s) |

Rectangular BPSK, 8 samples a symbol, Es/N0 3 dB, a 1 ppm data clock, into
`RateSync(pulse="iandd", m=4)` alone; 1000 frames a row, each one idle run,
128 dotted symbols, EB90, a 1 to 1017 octet payload and a CRC-16. Level 1x
is `RateSync`'s contract, unit symbol amplitude. Every row sees the same
symbols and the same noise.

| sym/s | G   | TED     | bn    | level | slips (gap)               | SER [99% CI]            | loss dB | walk gap / data | at sync | settle | lock in gap | lock_stat gap / data |
| ----- | --- | ------- | ----- | ----- | ------------------------- | ----------------------- | ------- | --------------- | ------- | ------ | ----------- | -------------------- |
| 3000  | 367 | gardner | 0.005 | 0.5x  | 0 (0), 4 unaligned        | 0.0280 [0.0258, 0.0304] | +0.23   | 0.070 / 0.053   | 45%     | 294    | 100%        | +0.15 / +0.40        |
| 3000  | 367 | gardner | 0.005 | 1x    | 58 (15), 4 unaligned      | 0.0342 [0.0315, 0.0370] | +0.62   | 0.202 / 0.087   | -2%     | 95     | 100%        | +0.14 / +0.38        |
| 3000  | 367 | gardner | 0.005 | 2x    | 746 (311), 4 unaligned    | 0.0553 [0.0509, 0.0599] | +1.74   | 0.286 / 0.286   | 109%    | never  | 99%         | +0.11 / +0.31        |
| 3000  | 367 | gardner | 0.01  | 0.5x  | 156 (27), 4 unaligned     | 0.0340 [0.0313, 0.0368] | +0.61   | 0.196 / 0.101   | 13%     | 135    | 99%         | +0.14 / +0.37        |
| 3000  | 367 | gardner | 0.01  | 1x    | 539 (390), 4 unaligned    | 0.0385 [0.0355, 0.0418] | +0.88   | 0.296 / 0.191   | 32%     | never  | 100%        | +0.12 / +0.34        |
| 3000  | 367 | gardner | 0.01  | 2x    | 84 (12), 985 unaligned    | 0.2877 [0.2650, 0.3119] | +10.05  | 0.237 / 0.331   | 93%     | never  | 64%         | +0.07 / +0.16        |
| 3000  | 367 | dttl    | 0.005 | 0.5x  | 1 (0), 3 unaligned        | 0.0301 [0.0277, 0.0326] | +0.36   | 0.130 / 0.073   | 24%     | 176    | 100%        | +0.14 / +0.40        |
| 3000  | 367 | dttl    | 0.005 | 1x    | 85 (34), 4 unaligned      | 0.0329 [0.0303, 0.0356] | +0.54   | 0.220 / 0.088   | -1%     | 98     | 100%        | +0.14 / +0.39        |
| 3000  | 367 | dttl    | 0.005 | 2x    | 304 (224), 3 unaligned    | 0.0352 [0.0324, 0.0382] | +0.68   | 0.287 / 0.113   | 4%      | 70     | 100%        | +0.13 / +0.36        |
| 3000  | 367 | dttl    | 0.01  | 0.5x  | 296 (187), 4 unaligned    | 0.0347 [0.0320, 0.0377] | +0.66   | 0.285 / 0.106   | -4%     | 92     | 100%        | +0.13 / +0.37        |
| 3000  | 367 | dttl    | 0.01  | 1x    | 643 (506), 4 unaligned    | 0.0385 [0.0355, 0.0418] | +0.88   | 0.294 / 0.125   | 18%     | never  | 100%        | +0.13 / +0.35        |
| 3000  | 367 | dttl    | 0.01  | 2x    | 1314 (647), 303 unaligned | 0.0955 [0.0880, 0.1035] | +3.38   | 0.305 / 0.286   | 104%    | never  | 98%         | +0.11 / +0.30        |
| 7000  | 357 | gardner | 0.005 | 0.5x  | 0 (0), 2 unaligned        | 0.0266 [0.0245, 0.0289] | +0.13   | 0.070 / 0.054   | 44%     | 284    | 100%        | +0.15 / +0.40        |
| 7000  | 357 | gardner | 0.005 | 1x    | 68 (10), 2 unaligned      | 0.0297 [0.0274, 0.0322] | +0.34   | 0.202 / 0.091   | -3%     | 93     | 100%        | +0.14 / +0.38        |
| 7000  | 357 | gardner | 0.005 | 2x    | 705 (292), 2 unaligned    | 0.0413 [0.0381, 0.0448] | +1.04   | 0.285 / 0.280   | 111%    | never  | 99%         | +0.11 / +0.31        |
| 7000  | 357 | gardner | 0.01  | 0.5x  | 190 (27), 3 unaligned     | 0.0310 [0.0285, 0.0336] | +0.42   | 0.205 / 0.111   | 12%     | 133    | 99%         | +0.14 / +0.37        |
| 7000  | 357 | gardner | 0.01  | 1x    | 534 (383), 3 unaligned    | 0.0334 [0.0308, 0.0362] | +0.57   | 0.292 / 0.186   | 21%     | never  | 100%        | +0.13 / +0.34        |
| 7000  | 357 | gardner | 0.01  | 2x    | 10 (2), 994 unaligned     | 0.1798 [0.1625, 0.1985] | +6.15   | 0.323 / 0.380   | 148%    | never  | 80%         | +0.09 / +0.23        |
| 7000  | 357 | dttl    | 0.005 | 0.5x  | 4 (0), 2 unaligned        | 0.0281 [0.0259, 0.0305] | +0.23   | 0.128 / 0.073   | 21%     | 162    | 100%        | +0.15 / +0.40        |
| 7000  | 357 | dttl    | 0.005 | 1x    | 70 (23), 2 unaligned      | 0.0289 [0.0266, 0.0314] | +0.29   | 0.219 / 0.089   | 0%      | 102    | 100%        | +0.14 / +0.39        |
| 7000  | 357 | dttl    | 0.005 | 2x    | 287 (212), 2 unaligned    | 0.0305 [0.0281, 0.0331] | +0.39   | 0.284 / 0.109   | 1%      | 66     | 100%        | +0.14 / +0.36        |
| 7000  | 357 | dttl    | 0.01  | 0.5x  | 288 (167), 3 unaligned    | 0.0298 [0.0275, 0.0324] | +0.35   | 0.282 / 0.105   | -7%     | 90     | 100%        | +0.14 / +0.37        |
| 7000  | 357 | dttl    | 0.01  | 1x    | 597 (457), 3 unaligned    | 0.0323 [0.0298, 0.0350] | +0.51   | 0.286 / 0.122   | 19%     | never  | 100%        | +0.13 / +0.35        |
| 7000  | 357 | dttl    | 0.01  | 2x    | 789 (381), 528 unaligned  | 0.2502 [0.2305, 0.2712] | +8.66   | 0.307 / 0.296   | 106%    | never  | 98%         | +0.11 / +0.30        |

Reading the columns: *slips* are whole symbols, in brackets those slipped
inside an idle run; the rest slipped in data and cost a frame. *SER* is
scored between slips, so it and the slip column together are the link.
*walk* is the drift short of a slip, RMS in symbols; with the whole-symbol
part removed it cannot exceed 0.289 (uniform), so a row near that has lost
its timing phase altogether. *at sync* and *settle* are only meaningful
where slips are rare. *unaligned* frames are mostly the shortest payloads,
whose marker is too short to clear the false-alarm gate at 3 dB; a slip
across one still counts at the next aligned frame.

Every loss below is decision quality BETWEEN slips (*SER* is scored
only while the strobe holds the eye it held at the frame's marker), so it
is quoted with its slip count beside it and never alone.

**The level, through the loop gain, matters more than the detector.**
Gardner's gain goes as `A^2`: at half the contracted level it slipped
nothing in 1000 frames at either rate and lost 0.13 to 0.23 dB; at twice
it slipped 705 to 746 symbols and lost 1.04 to 1.74 dB between them. The
DTTL's goes as `A^1`: 1 to 4 slips at half, about 300 at twice, three
quarters of them in the idle run.
`RateSync`'s default bandwidth, 0.01, is too wide for this floor with
either detector: 534 to 643 slips in 1000 frames at the contracted level.

**At the receiver's bandwidth and the contracted level, the detectors tie on
SER and differ on where they slip.**

- **SER:** loss 0.62 dB with 58 slips against 0.54 dB with 85 at 3000
    sym/s, and 0.34 dB with 68 against 0.29 dB with 70 at 7000, Gardner
    against DTTL, with overlapping intervals. Between slips, at Es/N0 = 3
    dB, the DTTL's decisions are no worse than Gardner's.
- **Slips:** both slip, 58 and 68 symbols in 1000 frames for Gardner, 85 and
    70 for the DTTL. The DTTL's land in the idle run about twice as often, 34
    against 15 and 23 against 10, and across it its strobe drifts slightly
    further (0.220 against 0.202 symbols RMS). In data, where a slip costs
    the frame, they are even: 43 and 58 for Gardner, 51 and 47 for the DTTL.
- **Across the gap:** both loops drift about 2.2 to 2.5 times as far as over
    the same number of data symbols, and both are back before the sync word:
    the 128 dotted symbols take the walk to 10% in 93 to 102 symbols, with
    nothing measurable left at the sync word.
- **The lock flag does not see the gap.** `locked` held through 100% of the
    idle-run symbols at this point, while `lock_stat` fell from 0.38 to
    0.14 inside it.

**The two rates read alike.** 367 and 357 idle symbols differ by less than
the run resolves.

**What this does not say.** One Es/N0, so no crossover: the header's
"degrades faster at low SNR" is a trend, and this run shows only that at
the floor the DTTL's SER is not worse and its slips move into the idle run
(gh-751). It is `RateSync` alone: it does not reproduce gh-3079, where the
whole async-DSSS receiver's Gardner loop fails at 5 dB and its DTTL holds.
Since a hot Gardner loop is what slips here, the level the receiver hands
its timing loop is the first thing to measure there; that is a lead, not a
result of this run.
