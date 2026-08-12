"""Characterization runs — the long ones, deliberately not per-push.

A **characterization** answers "how does this object behave across its
whole operating envelope", by sweeping C/N0, Doppler, sample rate and
seed until the answer is statistically meaningful. That costs minutes,
which is exactly why it does not belong beside a smoke test.

## Why this is a category and not an example

Both scripts here lived in `src/doppler/examples/` and were run on every
push by the example smoke gate (`src/doppler/tests/test_examples.py`).
Measured, they were **75% of that gate**: 164.6 s for
`dsss_receiver/characterize.py` and 117.7 s for
`acquisition/characterize.py`, against ~58 s for the other 65 examples
combined. An example earns its place by being short enough that nobody
thinks about it; a 300-trial Monte-Carlo sweep does not, and shortening
one to fit trades away the statistical confidence that is its entire
point.

## What runs them, and what does not

| what                              | runs                            |
| --------------------------------- | ------------------------------- |
| `make characterize`               | the full sweep, deliberately    |
| `make test` / CI, on every push   | the **fast twin**, not this     |
| `src/doppler/tests/test_examples.py` | nothing here — they left that glob |

Each subject keeps a **fast twin** in `src/doppler/dsss/tests/` —
`test_dsss_receiver_stress.py` and `test_dsss_acquisition_stress.py` —
which imports this module's helpers and runs a handful of trials. That
is what keeps the code honest per-push: the twin exercises the same
`run_trial`, the same geometry and the same scene builder, so a
characterization script cannot silently stop importing or stop working
while nobody is looking.

**Be clear about what that does and does not cover.** The twin proves
the helpers still run; it does not re-derive the envelope. A regression
that moves a pull-in boundary or a C/N0 threshold without breaking an
import will be caught by `make characterize`, which is deliberate rather
than automatic. That is the trade this category exists to make, and
`characterize-conformance` (in `src/doppler/tests/`) is the floor under
it: it fails if a subject here has no entry point or no twin at all.

Layout mirrors `validation/` on purpose — one directory per subject, one
fixed filename, artifacts written beside the script — so `make
characterize` discovers subjects by glob and **a new subject is covered
the moment its folder exists**, with no registration step to forget.
That was the lesson of the validation tree, which was executed by
nothing for two whole objects (`docs/dev/validation.md`).

## The committed plot is a snapshot, and nothing gates it

Each subject's `<subject>.png` is committed, because the sweep's result
is the deliverable and a result nobody can see is not one. But it is
**not** the same kind of artifact as validation's `results.md`: that has
`make validate-check` re-rendering it on every push, and this has
nothing. It cannot — a staleness gate here would re-run the sweep, which
is the cost the category exists to avoid.

So read the plot as "what the last deliberate run measured", and the
commit that carries it as the date. If you need to know whether it still
holds, the answer is to run `make characterize`, not to trust the file.
"""
