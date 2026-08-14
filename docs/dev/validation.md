# Object Validation

Every object doppler certifies carries its own **evidence**: measurements
taken from the shipped C through its shipped binding, a review that judges
them, and a set of limits a caller may rely on. This page is the process.
Follow it rather than reconstructing it from the last object that went
through.

## Why the evidence layer exists

The campaign started because a report's "Limits" section ended with the
sentence *"Claims a caller may rely on. A failure here is a regression, not
a new finding"* — and nothing executed it. The tree lived at
`src/doppler/tests/validation/`, no make target reached it, no CI job named
it, and `pytest --collect-only` found zero tests inside it. Forty-four
claims across the first two objects were asserted by nobody, and two
regressions went straight through them in one afternoon.

So the rule that shapes everything below: **a claim nothing runs is prose.**
When you add a phase, ask which gate executes it. If the answer is "none",
it is documentation, and it belongs in `docs/design/` where nobody will
mistake it for a guarantee.

## The order is not negotiable

1. **The C header is the SSOT.** Read `native/inc/<obj>/<obj>_core.h`
    first and enumerate every claim its prose makes.
1. **Map each claim onto `native/tests/test_<obj>_core.c`** — pinned,
    pinned only at literals, or absent. The uncovered rows dictate the C
    tests to write. Write them, and **prove each by sabotage** before
    trusting it.
1. **Only then** build the Python validation folder, which characterises,
    plots and asserts.

This was learned rather than designed. **NCO and LO were the first two
objects**, and NCO went Python-first: its header claims were discovered
empirically from outside, which produced evidence in the wrong language for
the gate and cost a rewrite. The question that corrected it — *"did we
start with `nco_core.h` as SSOT, then update/write C tests?"* — is the one
to ask yourself at the start of each object.

The reason is not tidiness. An object's most important surface is often the
one the binding does not expose: resamp's control accumulator,
`resamp_get_ctrl_acc`, is *the* diagnostic for a closed timing loop and has
no Python binding at all. A Python-first audit measures whatever the
binding happens to reach and reports a clean bill of health for the surface
that matters least.

## Step 1 — the claim inventory

Read the header and write down every prose claim, then grep the C test for
it. Three outcomes, and the middle one is the trap:

| verdict                     | meaning                                                                                   |
| --------------------------- | ----------------------------------------------------------------------------------------- |
| **pinned**                  | an assertion genuinely tests the claim                                                    |
| **pinned only at literals** | the test's *comment* claims more than its assertion does — prose wearing a test's clothes |
| **absent**                  | zero mentions                                                                             |

resamp's inventory is the worked example: 16 public entry points against a
593-line test, of which three had **zero** mentions
(`resamp_get_ctrl_acc`, `resamp_dc_gain`, `resamp_destroy(NULL)`) and two
more were literals-only — `set_rate`'s comment promised "preserves phase"
while asserting only that `get_rate()` read back, and `reset`'s promised
"zeroes phase/delay" while asserting only that the rate survived. A reset
that zeroed nothing at all passed that test.

## Step 2 — the C tests, and the sabotage

Write a test for every uncovered claim, then **break the code and watch it
go red**. A test you have not seen fail is not evidence.

Two failure modes to design against:

**Measure against an external truth, not against the other entry point.**
A consistency test (path A equals path B) is structurally blind to any
defect the two paths share. resamp's `eq_ctrl_push` compared the block and
push control paths, swept the deviation across unity on every run, and
passed for the entire period the control accumulator was running the
decimator's recurrence on the interpolator's structure. Prefer truths that
need no timing convention: a resampled pure tone is still a pure tone; the
output count is the integral of the rate regardless of the filter; a
polyphase arm can be *read off the output* by giving arm `p` the single tap
`p + 1`.

**A reject test can pass vacuously.** Assert the precondition too. resamp's
§12 asserts that `mu` stays bit-exactly `0.0` at an exact rate — which an
accessor hard-wired to `0.0` also satisfies. The sabotage that took §10,
§11 and §12's slewing case red left the steady case **green**, and it now
carries its own precondition: steer the same rate off-exact and require
`mu` to move. Same shape as `reset` — a reset test proves nothing if the
state was already zero when it ran.

## Step 3 — the validation folder

Each certified object owns a folder beside its **module's** tests — the
module, not the object, so `nco` and `lo` are siblings under `source`:

```text
src/doppler/<module>/tests/validation/<object>/
    validate.py     the runner: characterise -> review -> limits
    results.md      the authoritative report, GENERATED, never hand-edited
    *.png           plots it embeds
    data/*.csv      raw sweeps, so any number can be re-derived
```

Shared machinery lives in `src/doppler/tests/`:

- `_validation_common.py` — the `Report` class and the `--check` CLI, so
    the format cannot drift between objects
- `loop_reference.py` — the closed-loop reference every steered object is
    measured against, with the detector as a parameter

Anything a **second** object needs moves into the shared home when that
second caller appears, not in anticipation of one.

### The report's five sections, and the summary that opens it

**Written last, read first: the executive summary.** `results.md` opens
with an unnumbered `## Executive summary` carrying two things — a
**status** line and a short list of **key takeaways**. It is emitted by
`Report.executive(title, takeaways)`, called from `build()` *after*
`limits()`, and it renders into the report's `head` rather than appending
to the body, so its position is a property of `render()` and not of who
called what when.

The split inside it is deliberate:

- **Status is DERIVED** — CERTIFIED when every limit holds, REGRESSED when
    any does not, plus the finding tally and which are still open. A
    hand-written status is the first thing to go stale, and this one cannot:
    it is a function of the same `limits` and `findings` lists the body
    renders.
- **Takeaways are AUTHORED**, because "what matters here" is judgement and
    no counter produces it. Three to six, each something a caller would
    change a decision over: a number to design to, a failure mode to defend
    against, a limit of the evidence. Cite the section that measured it
    (`§2.5`) so a reader can chase any of them down. A list of twelve is the
    body of the report again.

**Nothing in it may be time-varying.** `make validate-check` re-renders and
compares bytes, so a generated date, a duration or a hostname makes every
report permanently stale and sends the reader to a command that changes
nothing.

It is unnumbered on purpose. Numbering it `1.` would renumber every
section below it and invalidate the `§2.x` cross-references in seven
reports, the C tests that cite them and the issues filed against them —
for no gain, since front matter is what an executive summary is.

`scripts/check_validation_reports.py` enforces presence, position and both
parts, against the rendered file.

Then the five numbered sections:

1. **The object** — links to `docs/design/<obj>.md` and the header; does
    **not** restate them. If this section starts writing design rationale,
    that content belongs in `docs/design/`.
1. **Characterisation** — measured behaviour, tables and plots, **no
    verdicts**. Name the C section each part tracks in its heading (resamp
    does: `### 2.4 Decimating: the stopband … (C §17)`) so the two read side
    by side. The report's own numbering stays sequential regardless — it is
    not a mirror of the C file's, because a report section routinely merges
    several C ones, and nco's claim that it *was* a mirror is what let a gap
    at §2.8 sit unnoticed. Claims unreachable from Python are reported as
    **C-ONLY** with the C section that covers them, never silently skipped.
1. **Review** — findings with verdicts: `BY DESIGN`, `GAP`, `CONFIRMED`,
    `FIXED`, `C-ONLY`. Defined below, because two of them are easy to read
    backwards.
1. **Limits** — the envelope a caller may rely on, asserted.
1. **Summary.**

### The five verdicts

**A verdict is a judgement about a PROBLEM.** There is deliberately no
verdict meaning "this works" — that is what a limit is for, and recording
a passing result as a finding inflates the count with something already
gated.

| verdict     | means                                                                            | open? |
| ----------- | -------------------------------------------------------------------------------- | ----- |
| `GAP`       | something is missing or unestablished — no evidence, no binding, no fix yet      | YES   |
| `CONFIRMED` | a real defect, reproduced and understood, deliberately not fixed here            | YES   |
| `FIXED`     | a defect this certification found **and corrected**                              | no    |
| `BY DESIGN` | behaviour that reads as a defect and is intended; the report says why            | no    |
| `C-ONLY`    | a claim the Python face cannot reach, certified in C instead — name that section | no    |

`CONFIRMED` is the one that gets misused: it means **a confirmed defect**,
not a confirmed claim. It counts as OPEN, and the count flows into the
executive summary's status line and
[the validation log](validation-log.md)'s `still open` column. Using it for
a positive result — "the header's figure is confirmed correct" — reports a
clean object as carrying open defects, which is the same
finished-work-in-the-backlog problem `issue-link-check` exists to stop. The
mpsk report shipped that way for one commit; RateSync's F7 (a lock
indicator that cannot distinguish an under-driven loop, left open as
gh-661) is the shape `CONFIRMED` is actually for.

**Phases in order, and no fixes during characterisation** — findings only.
A fix made while measuring contaminates the measurement it came from.

## The two gates

They answer different questions, and the split is the whole point.

| gate                        | question                             | what runs it                   |
| --------------------------- | ------------------------------------ | ------------------------------ |
| `test_validation_limits.py` | do the limits still hold?            | `make test-python`, per module |
| `make validate-check`       | is the committed `results.md` stale? | `make gates`                   |

**The limits gate** lives at
`src/doppler/<module>/tests/test_validation_limits.py`. It runs each
object's own `build(write=False)` and fails on any limit that does not
hold, so the evidence and the gate are the *same code* and cannot disagree
the way a hand-copied assertion would. `write=False` suppresses
`results.md`, the plots and the CSVs — a test must never write into the
repo — while every measurement still executes.

It is a per-module file rather than one tree-wide collector because each
validator imports its own module's objects; a single collector would import
every extension in the tree to run any object's limits.

**The staleness gate** is `make validate-check`, which re-renders each
report in memory and fails if the committed bytes differ. `make validate`
regenerates. Both discover validators by glob, so **a new object is gated
the moment its folder exists** — there is no registration step to forget.

## Adding an object

- [ ] Enumerate the header's claims; map each onto the C test

- [ ] Write C tests for the uncovered ones; sabotage each and watch it fail

- [ ] Create `src/doppler/<module>/tests/validation/<object>/` with
    `validate.py` and an `__init__.py`

- [ ] Add the module's `test_validation_limits.py` if it is the module's
    first object; otherwise add the object to its `OBJECTS` map

- [ ] `make validate` to generate `results.md`, plots and CSVs

- [ ] **Update the C test and the Python example to carry whatever the
    validation established** — a new limit, a corrected rule, a number a
    caller has to choose by. A finding that reaches only the report reaches
    nobody: the report is evidence, while `native/tests/test_<obj>_core.c`
    is what keeps the property true and `src/doppler/examples/<obj>_demo.py`
    is where a user meets it. Both are already gated (`ctest`, and
    `test_examples.py` runs every example and requires it to self-validate
    with physical asserts), so a rule written into them cannot quietly stop
    being true.

    This is not bookkeeping. The AGC's `4*decim*loop_bw <= 0.05` rule was
    calibrated on one step direction, and it was **the example** — which
    cold-starts into a weak signal, the other direction — that failed its
    own assert and forced the rule 4x tighter before it shipped.

- [ ] **Write the executive summary last** — `R.executive(...)` after
    `limits()`, with three to six takeaways aimed at a caller who will read
    nothing else. Do it at the END, when the object is understood: the
    takeaways are the one part of the report that says which of its
    findings a reader should act on, and they are unwritable before the
    measurements exist. Status comes for free.

- [ ] `make validate-check` and the module's pytest, both green

- [ ] Commit the generated report — it is the deliverable, not a build
    artifact

## Where a long sweep goes instead — characterization

A validator runs on every push, twice: `test_validation_limits.py` executes
its `build(write=False)`, and `make validate-check` re-renders its report.
Both run **every measurement**. So a validator is the wrong home for a
sweep that needs minutes to say anything — putting one there taxes every
push for an answer nobody asked for on that push.

Those live in **`src/doppler/<module>/tests/characterization/<subject>/`**
as `characterize.py`, run by `make characterize` and by nothing else. The
two categories answer different questions:

|             | **validation**                        | **characterization**                          |
| ----------- | ------------------------------------- | --------------------------------------------- |
| asks        | do the certified limits still hold?   | how does it behave across its whole envelope? |
| runs        | every push (limits + staleness)       | `make characterize`, deliberately             |
| deliverable | `results.md`, a caller may rely on it | the sweep's own findings                      |

The category exists because two DSSS Monte-Carlo sweeps were sitting in
`src/doppler/examples/`, where the smoke gate ran them on every push:
measured, **164.6 s + 117.7 s against ~58 s for the other 65 examples put
together** — 75% of that gate. Shortening them to fit would have spent the
statistical confidence that is their entire point, so they moved.

Each subject keeps a **fast twin** under `tests/` that imports its helpers
and runs a few trials, and that twin is the per-push cover. Be precise
about what it buys: the twin proves the helpers still run, **not** that the
envelope still holds. A regression that moves a pull-in boundary without
breaking an import waits for the next `make characterize` — a window tracked
in [gh-692](https://github.com/doppler-dsp/doppler/issues/692), not accepted
silently.
`make characterization-check` (in `lint`) is the floor — it fails a subject
with no `__main__` block or no twin at all, which are the two ways a sweep
becomes a silent no-op. Full rationale:
`src/doppler/dsss/tests/characterization/__init__.py`.

## Conventions worth knowing

- `results.md` is **generated**. It is excluded from mdformat, and
    `validate.py` rstrips to exactly one trailing newline so the
    end-of-file-fixer and the generator cannot fight each other.
- **The report self-checks as it renders.** `Report.render()` refuses to
    emit a document that contradicts itself: a `§N.M` reference with no such
    section, section-2 numbering with a gap, or a table cell truncated
    mid-reference. All three shipped before the check existed, and
    `make validate-check` stayed green through every one of them — it proves
    `results.md` matches what the generator renders, which says nothing
    about whether what is rendered is coherent. Registration-free: every
    object renders through the same method.
- **Render findings in full.** `R.find()`'s text is the argument for a
    verdict, and the report is where it belongs. Truncating to a first
    sentence (`txt.split(".")[0]`) both hides the reasoning and cuts any
    sentence containing a decimal mid-reference.
- **Emit `![...](plot.png)` from the section, never from `plots()`.**
    `plots()` runs only when `write=True`, so markdown emitted there is
    absent from the `--check` render — and `make validate-check` is then
    permanently stale with a diff that looks like drift and is really a
    missing section. `plots()` draws files; the section owns the reference.
- A failing **limit** is a regression. A new problem found while
    characterising is a **finding**, and goes in section 3 with a verdict.
- Findings that will not be fixed in the same pass get **filed as issues**
    before the PR merges, not explained in a docstring.
