# Adding an algorithm — the lifecycle

Fifteen pages under `docs/dev/` describe how to do each part of this well.
None of them says what order the parts go in, or which one owns a thing you
are looking for — so this page is the spine, and it owns exactly two things:

- **the order** the phases happen in, and
- **who owns the how** for each of them.

It deliberately contains **no mechanics**. Every step below links to the one
page that owns it, and where a step's home is a make target or a script, it
names that instead. If you find yourself adding a command here, that command
belongs on the page this one links to — a second copy of a procedure is the
thing that goes quietly out of date, and this page exists to make the map
readable, not to become a sixteenth copy of the territory.

______________________________________________________________________

## Start with why

Before any code, write `docs/design/<algo>.md`:

- **the use cases** — who calls this, with what, and what they do with the
    answer;
- **the design goals, and the unknowns** — say plainly which numbers you do
    not know yet. The unknowns are what the characterization later measures,
    and writing them down first is what stops a sweep being designed to
    confirm a decision already made;
- **a Python prototype to de-risk** — throwaway, in a scratch directory, to
    find out whether the idea works at all. It is not the implementation and
    it is not committed; see [Repository Map](repository-map.md) for why the
    algorithm lands in C exactly once;
- **a sketch of the implementation plan** — the object's shape, its state,
    and which existing primitive it composes rather than re-implements.

**This phase has no gate**, and a design doc is enforced by review alone —
which is precisely why it goes first: it is the cheapest place to be wrong.
The design page is also what section 1 of the eventual validation report links
to, rather than restating.

It is not, however, the *only* ungated phase — phase 4's sabotage is the
other, and that one is a gap rather than a design.

______________________________________________________________________

## The lifecycle at a glance

| #   | phase          | produces                                                 | the how lives in                                                                 | proven by                                          |
| --- | -------------- | -------------------------------------------------------- | -------------------------------------------------------------------------------- | -------------------------------------------------- |
| 1   | **Why**        | `docs/design/<algo>.md`                                  | this page                                                                        | review only                                        |
| 2   | **Declare**    | `objects/<obj>.toml`, a `just-makeit.toml` entry         | [Adding a Module](adding-a-module.md)                                            | `make drift-check`                                 |
| 3   | **Implement**  | `native/inc/<obj>/<obj>_core.h` + `_core.c`              | [Adding a Module](adding-a-module.md), [Error Convention](error-convention.md)   | `ctest`                                            |
| 4   | **Pin**        | `native/tests/test_<obj>_core.c`                         | [Object Validation](validation.md) step 2                                        | `ctest`, `make tests-ssot`                         |
| 5   | **Bind**       | `.pyi`, `__init__.py`, the `_ext` fragment               | [Module Layout](module-layout.md), [Docstring Authoring](docstring-authoring.md) | `make drift-check`, `make test-stubs`              |
| 6   | **Instrument** | the state triplet, telemetry probes                      | [state serialization](../design/state-serialization.md)                          | `check_serializable.py`, the state matrix          |
| 7   | **Explore**    | `native/validation/<obj>_*.c`, `tests/characterization/` | [Object Validation](validation.md)                                               | `make validate-c`, `make characterize`             |
| 8   | **Certify**    | `tests/validation/<obj>/results.md`                      | [Object Validation](validation.md)                                               | `make validate-check`, `test_validation_limits.py` |
| 9   | **Carry back** | a C section, an example, a gallery page                  | [Doc Examples](doc-examples.md)                                                  | `ctest`, `test_examples.py`                        |
| 10  | **Land**       | CHANGELOG entry, issues for what is left                 | [Release](release.md)                                                            | `make changelog-check`                             |

`make gates` runs the merge-guarding set; the per-phase targets above are how
you find out sooner.

______________________________________________________________________

## The phases, and what is decided in each

### 2 — Declare, and why the header is still the spec

The interface is declared in a manifest and `jm apply` scaffolds from it —
the C stub, the binding, the CMake, the stubs, the test and benchmark
skeletons. That makes the manifest the thing you edit and the generated
files the things you do not.

It does **not** make the manifest the specification. **The C header is the
SSOT**: it is where the contract is written in prose a caller reads, and it
is what the certification later audits claim by claim. The manifest declares
a shape; the header states what the object promises.

### 3 — Implement, C first

One algorithm, one implementation, in C. Wrappers are glue and never
re-implement logic — and that rule holds *inside* C too: two modules must not
grow private copies of the same primitive.

The header you write here is a deliverable, not a comment. Its Doxygen
becomes the Python docstring on both faces, so writing it well is cheaper
than fixing it twice — [Docstring Authoring](docstring-authoring.md) is the
home for that.

### 4 — Pin, and prove the pin

Write the C tests, then **break the implementation and watch each one go
red**. A test you have not seen fail is not evidence. [Object
Validation](validation.md) owns this rule and its two failure modes
(consistency tests that are blind to a shared defect; reject tests that pass
vacuously).

**Nothing executes this, and you have to supply the discipline.** `ctest`
proves a test passes; no gate proves it can fail. The rule is stated on this
page, in [Object Validation](validation.md) step 2, and again in
[Measuring a Receiver](measuring-a-receiver.md) — and enforced on none of
them, which is the shape `validation.md` opens by warning about. The cost is
measurable: six tests in an already-certified object were later found to pass
against a receiver whose carrier discriminator did nothing at all
([gh-843](https://github.com/doppler-dsp/doppler/issues/843)). Until a gate
exists, treat the sabotage as part of writing the test, not as a review step
somebody else performs.

Do this *before* the Python face exists. A test written through the binding
measures whatever the binding happens to expose, and the surfaces that matter
most are routinely the ones with no binding at all.

### 6 — Instrument

Two things every stateful object owes its callers, and both are easy to
retrofit badly and hard to retrofit late:

- **the state triplet** — `state_bytes` / `get_state` / `set_state`, so the
    object can be checkpointed and resumed. Required for anything carrying
    running state;
- **telemetry probes** — the internals a caller cannot otherwise see.

The rule worth knowing here is that a *published* diagnostic and a *useful*
one are not the same thing: an object can publish two views of its own state
that are secretly the same view. Check what a composing receiver would
actually need to debug you.

### 7 — Explore, before you certify

This is the phase the word "characterization" names, and it is where a
one-time thorough sweep belongs: the whole envelope, enough trials to be
statistically meaningful, minutes of runtime. It answers *how does this
behave*, which is a different question from *what may a caller rely on*.

Two homes, and which one depends on the language of the answer:

- `native/validation/<obj>_*.c` when the surface is C-only — an inlined
    discriminator, a static helper, anything with no binding. Run in full by
    `make validate-c`, and by `ctest` as a fast regression subset.
- `src/doppler/<module>/tests/characterization/<subject>/` when it is a long
    Monte-Carlo sweep over an object that *does* have a binding. Run by
    `make characterize` and by nothing else, deliberately.

### 8 — Certify

Now, and only now, the evidence a caller is handed: header claims enumerated,
mapped onto the C tests, then measured, reviewed and asserted.
[Object Validation](validation.md) owns the whole of it, including the
report's five sections, the executive summary that opens it, and the two
gates.

The order matters and is not negotiable — header, then C tests, then the
Python report. Going the other way produces evidence in the wrong language
for the gate, and has cost a rewrite before.

**An algorithm with no Python face is still certified, and does not grow a
binding to become so.** `conv` was the first: a binding built only to be
measured is one nobody calls, and the campaign would then be certifying an
artifact of its own process. The substitution is one line long — a C harness
under `native/validation/` measures and emits, and a validator under
`src/doppler/tests/validation/<obj>/` renders and asserts through the same
`Report` — so the report format, both gates and the five sections are
unchanged. [Object Validation](validation.md#certifying-a-component-with-no-binding)
has the table.

### 9 — Carry it back

A finding that reaches only the report reaches nobody. Whatever the
certification established — a new limit, a corrected rule, a number a caller
has to choose by — goes back into the C test and into an example, because
those are the two things that keep it true and put it in front of someone.

### 10 — Land it

The CHANGELOG entry lands with the change, not at release time. Anything
found and deliberately not fixed gets **filed as an issue before the PR
merges**, never explained in a docstring — a carve-out written into a comment
is invisible to everyone who did not already know to look for it.

______________________________________________________________________

## Four places evidence lives

The single most confusing thing about this repo for a newcomer, and it is
worth memorising:

| where                            | answers                             | scope                    | run by                                              |
| -------------------------------- | ----------------------------------- | ------------------------ | --------------------------------------------------- |
| `native/tests/test_<obj>_core.c` | does this claim still hold?         | one assertion per claim  | `ctest`, every push                                 |
| `native/validation/<obj>_*.c`    | how does the C-only surface behave? | full sweep, C-only reach | `make validate-c`; `ctest` runs a subset            |
| `tests/validation/<obj>/`        | what may a caller rely on?          | the certified envelope   | `make validate-check` + the limits test, every push |
| `tests/characterization/<subj>/` | how does it behave everywhere?      | minutes of Monte Carlo   | `make characterize`, deliberately                   |

The split between rows three and four is a cost decision: a validator runs
twice on every push, so a sweep that needs minutes to say anything taxes
every push for an answer nobody asked for on that push.

______________________________________________________________________

## One word, three meanings

**"Characterize" means three different things in this repository**, and
nothing until now said so:

1. *classify the algorithm's I/O shape* — can one sample be processed
    independently with small fixed state? — which is what picks the `jm`
    preset ([Adding a Module](adding-a-module.md), its step 0);
1. *phase 1 of a validation report* — measure behaviour, tables and plots,
    no verdicts ([Object Validation](validation.md));
1. *the long-sweep category* — `tests/characterization/`, `make  characterize`, phase 7 above.

Sense 1 is unrelated to the other two and merely shares a name. When writing,
prefer "classify the object's shape" for the first and reserve
"characterization" for senses 2 and 3.

______________________________________________________________________

## What this page does not own

Nothing on this list is described here, and adding a description of one to
this page is the failure mode to watch for:

- how to run `jm`, what the manifest keys mean, what `jm apply` regenerates —
    [Adding a Module](adding-a-module.md)
- where files go and what `.pyi` / `__init__.py` may contain —
    [Module Layout](module-layout.md)
- how to write the header Doxygen so the docstrings come out right —
    [Docstring Authoring](docstring-authoring.md)
- how errors cross the C ABI — [Error Convention](error-convention.md)
- what is generated under `docs/` and what is hand-owned —
    [Docs Conventions](docs-conventions.md)
- how snippets and examples are discovered and tested —
    [Doc Examples](doc-examples.md)
- how to benchmark, and how to compare two snapshots honestly —
    [Benchmarking](benchmarking.md)
- which objects are certified today — [Validation Log](validation-log.md)
