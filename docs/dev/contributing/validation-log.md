# Validation log

Which objects are certified, and what their certification currently says.

[Object Validation](validation.md) is the process page — the order of work,
the sabotage rule, the report's five sections, the two gates. This page is
its running record: one row per object that has been through it, with a link
straight to that object's evidence.

## How to read a row

Each row is derived from the object's own `results.md`, which is generated
from the C implementation through its own binding — nothing here is
transcribed by hand.

| column       | means                                                                            |
| ------------ | -------------------------------------------------------------------------------- |
| `limits`     | how many of the certified envelope's claims hold, out of the total               |
| `findings`   | every judgement the review phase recorded, of any verdict                        |
| `still open` | of those, the ones with verdict `GAP` or `CONFIRMED`, named so you can find them |

**A `findings` count is not a defect count**, and a high one is not a bad
sign — `BY DESIGN` and `FIXED` are the common verdicts, so an object that was
looked at hard has more of them.

**Nor does an open finding mean the object failed.** Every object in the
table below holds all of its limits *while* carrying open findings, and the
two say different things: a limit is a claim a caller may rely on, and a
failing one is a regression; an open finding is a gap in what has been
established, or a defect that is understood and documented but not yet
fixed. Certification means the envelope is measured and holds — not that
nothing is left to do. Only `limits` is bolded when it is short of its
total, because that is the cell that would mean something is broken.

## What the numbers are worth

The chain from code to table is gated at every link:

1. `make validate` regenerates each `results.md` by measuring the C;
1. `make validate-check` fails if a committed report is stale — so a report
    cannot drift from the code it describes;
1. this page's table is generated from those reports by
    `scripts/gen_validation_log.py`, and `make docs-drift-check` fails if it
    is stale — so the table cannot drift from the reports.

What that chain does **not** prove is that a limit is the right limit, or
that it would fail if the behaviour regressed. Only the sabotage step in
[Object Validation](validation.md) establishes that, and no amount of
regeneration substitutes for it — a report that regenerates faithfully from
a vacuous assertion is faithfully vacuous.

Rows are sorted by module, then by object, so the table reads as a map of
the library rather than as a history — arrival order is what the git log
is for. Discovery is a glob of
`src/doppler/*/tests/validation/*/results.md`, the same one `make validate`
uses, so a newly certified object is listed the moment its folder exists.
There is no list here to update.

## Certified objects

<!-- validation-log:start -->

**30 objects certified** — 20 of the 79 `objects/*.toml` jm fragments, plus 10 with no object manifest at all (`ccsds_tm`, `conv`, `detection`, `ema`, `mpsk`, `resamp`, `rs`, `wfm_compose`, `wfm_frame`, `wfm_plan`): a function primitive, or a core declared another way. Not every fragment is a DSP object with an envelope worth certifying, so read the denominator as a ceiling rather than a target — and note the two counts are different populations, not a percentage.

| object                                                                                                                                 | module       | limits | findings | still open                 |
| -------------------------------------------------------------------------------------------------------------------------------------- | ------------ | ------ | -------- | -------------------------- |
| [AGC](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/agc/tests/validation/agc/results.md)                                | `agc`        | 18/18  | 6        | 2 — F4, F6                 |
| [Interleaver](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/coding/tests/validation/interleaver/results.md)             | `coding`     | 20/20  | 8        | 2 — F7, F8                 |
| [detection](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/detection/tests/validation/detection/results.md)              | `detection`  | 24/24  | 6        | 1 — F5                     |
| [LockDet](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/detection/tests/validation/lockdet/results.md)                  | `detection`  | 22/22  | 6        | none                       |
| [acq](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/dsss/tests/validation/acq/results.md)                               | `dsss`       | 16/16  | 7        | 3 — F3, F6, F7             |
| [BurstAcquisition](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/dsss/tests/validation/burst_acq/results.md)            | `dsss`       | 17/17  | 4        | none                       |
| [BurstDemod](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/dsss/tests/validation/burst_demod/results.md)                | `dsss`       | 15/15  | 5        | none                       |
| [BurstDespreader](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/dsss/tests/validation/burst_despreader/results.md)      | `dsss`       | 15/15  | 5        | none                       |
| [DsssBurstReceiver](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/dsss/tests/validation/dsss_burst_receiver/results.md) | `dsss`       | 32/32  | 11       | none                       |
| [PolynomialPhaseEstimator](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/dsss/tests/validation/ppe/results.md)          | `dsss`       | 15/15  | 5        | none                       |
| [M-PSK constellation](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/mpsk/tests/validation/mpsk/results.md)              | `mpsk`       | 24/24  | 5        | 1 — F3                     |
| [resamp](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/resample/tests/validation/resamp/results.md)                     | `resample`   | 14/14  | 9        | none                       |
| [LO](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/source/tests/validation/lo/results.md)                               | `source`     | 26/26  | 9        | none                       |
| [NCO](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/source/tests/validation/nco/results.md)                             | `source`     | 18/18  | 9        | none                       |
| [Corr2D](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/spectral/tests/validation/corr2d/results.md)                     | `spectral`   | 18/18  | 6        | none                       |
| [CorrDetector2D](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/spectral/tests/validation/detector2d/results.md)         | `spectral`   | 15/15  | 5        | none                       |
| [CarrierNda](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/track/tests/validation/carrier_nda/results.md)               | `track`      | 43/43  | 12       | 6 — F4, F5, F6, F7, F8, F9 |
| [LoopFilter](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/track/tests/validation/loop_filter/results.md)               | `track`      | 26/26  | 10       | none                       |
| [MpskReceiver](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/track/tests/validation/mpsk_receiver/results.md)           | `track`      | 63/63  | 8        | 2 — F6, F7                 |
| [RateSync](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/track/tests/validation/ratesync/results.md)                    | `track`      | 36/36  | 17       | 2 — F7, F17                |
| [EMA](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/util/tests/validation/ema/results.md)                               | `util`       | 15/15  | 6        | none                       |
| [Composer](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/wfm/tests/validation/wfm_compose/results.md)                   | `wfm`        | 15/15  | 4        | none                       |
| [Frame](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/wfm/tests/validation/wfm_frame/results.md)                        | `wfm`        | 16/16  | 4        | 1 — F2                     |
| [Plan](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/wfm/tests/validation/wfm_plan/results.md)                          | `wfm`        | 17/17  | 4        | 2 — F1, F2                 |
| [Reader](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/wfm/tests/validation/wfm_reader/results.md)                      | `wfm`        | 16/16  | 4        | 2 — F2, F3                 |
| [Synth](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/wfm/tests/validation/wfm_synth/results.md)                        | `wfm`        | 19/19  | 6        | 2 — F2, F5                 |
| [Writer](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/wfm/tests/validation/wfm_writer/results.md)                      | `wfm`        | 16/16  | 5        | 2 — F1, F5                 |
| [ccsds_tm](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/tests/validation/ccsds_tm/results.md)                          | `— (C only)` | 12/12  | 5        | 1 — F2                     |
| [conv](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/tests/validation/conv/results.md)                                  | `— (C only)` | 7/7    | 3        | none                       |
| [rs](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/tests/validation/rs/results.md)                                      | `— (C only)` | 11/11  | 4        | none                       |

<!-- validation-log:end -->

## Not yet certified

Everything else. The campaign works through objects deliberately rather than
in bulk, because the expensive part is not writing the report — it is
enumerating a header's prose claims and proving each new C test by sabotage.
An object's absence from the table above means no certification has been
attempted, **not** that it is known to be sound.
