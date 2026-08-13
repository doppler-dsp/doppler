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

Rows appear by arrival: the generator globs
`src/doppler/*/tests/validation/*/results.md`, the same discovery
`make validate` uses, so a newly certified object is listed the moment its
folder exists. There is no list here to update.

## Certified objects

<!-- validation-log:start -->

**6 of 71 objects certified.** The denominator is every `objects/*.toml` jm fragment, which is the whole object surface — not every one of them is a DSP object with an envelope worth certifying, so read it as a ceiling rather than a target.

| object                                                                                                              | module     | limits | findings | still open                          |
| ------------------------------------------------------------------------------------------------------------------- | ---------- | ------ | -------- | ----------------------------------- |
| [AGC](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/agc/tests/validation/agc/results.md)             | `agc`      | 18/18  | 6        | 2 — F4, F6                          |
| [resamp](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/resample/tests/validation/resamp/results.md)  | `resample` | 14/14  | 9        | none                                |
| [LO](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/source/tests/validation/lo/results.md)            | `source`   | 26/26  | 9        | none                                |
| [NCO](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/source/tests/validation/nco/results.md)          | `source`   | 18/18  | 9        | none                                |
| [RateSync](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/track/tests/validation/ratesync/results.md) | `track`    | 26/26  | 12       | 8 — F1, F2, F3, F4, F5, F6, F7, F11 |
| [EMA](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/util/tests/validation/ema/results.md)            | `util`     | 15/15  | 6        | none                                |

<!-- validation-log:end -->

## Not yet certified

Everything else. The campaign works through objects deliberately rather than
in bulk, because the expensive part is not writing the report — it is
enumerating a header's prose claims and proving each new C test by sabotage.
An object's absence from the table above means no certification has been
attempted, **not** that it is known to be sound.
