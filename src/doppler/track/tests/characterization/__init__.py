"""Characterization runs for `track` — the long ones, deliberately per-push.

A **characterization** answers "how does this object behave across its whole
operating envelope", by sweeping until the answer is statistically meaningful.
That costs minutes, which is why `make characterize` runs it deliberately and
nothing runs it on a push. The category and its trade are documented once, in
`doppler.dsss.tests.characterization` and `docs/dev/contributing/validation.md`; this module
exists because `track` had nowhere to put an envelope at all.

## Why this module was missing, and what it cost

`track` was the only receiver module with no characterization tree — `dsss` had
the only one. The consequence was not that a sweep went unwritten; it was that
a measured envelope had **nowhere to be an answer**, so it became a constant.

The M-PSK carrier loop's acquisition bound was measured in 2026-08, written
into three docstrings as dated prose, and re-derived by nothing. Two findings
in `MpskReceiver`'s validation report (F4 and F5) were then filed against the
receiver for behaviour that was really the test seeding past that bound —
because the bound existed as a sentence rather than as a curve anybody could
re-run. Both were retracted in doppler#843.

So the rule this module exists to serve is the repository's own: **a claim
nothing runs is prose.** A number a validator relies on has to be re-derivable,
and phase 7 (`docs/dev/contributing/adding-algorithms.md`) is where a number that takes
minutes to establish belongs.

## What runs what

| what | runs |
| --- | --- |
| `make characterize` | the full sweep, deliberately |
| `make test` / CI, per push | the **fast twin**, not this |
| `make characterization-check` (in `lint`) | that a subject has an entry point and a twin |

**The twin proves the helpers still run; it does not re-derive the envelope.**
A regression that moves a pull-in boundary without breaking an import waits for
the next `make characterize`. That is the trade the category makes, stated
rather than implied — see doppler#692.

Layout mirrors `validation/` on purpose: one directory per subject, one fixed
filename, so discovery is by glob and a new subject is covered the moment its
folder exists.
"""
