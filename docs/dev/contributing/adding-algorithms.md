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

!!! warning "Use the existing, proven tools"

    Every phase below has shipped helpers for the things a harness is
    tempted to build: assertions and the epilogue (`dp_test.h`), noise
    and random bits (`dp_rng_test.h`, `awgn`), stimulus (`dp_tx_test.h`,
    `dp_dsss_test.h`, and the waveform generator itself, `wfm_synth` /
    `wfm_compose`), verdicts (`dp_sym_test.h`, `dp_ber_test.h`,
    `BerMeter`), the receiver instrument (`dp_rx_test.h`), the state round
    trip (`dp_state_test.h`), and the bench scaffold (`dp_bench.h`). **A
    harness renders nothing and asserts through `dp_test.h`.** The full
    table, by phase, is
    [What each place already gives you](#what-each-place-already-gives-you);
    read it before the first line of any test, validator or bench.

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

| #   | phase          | produces                                                 | the how lives in                                                                 | proven by                                               |
| --- | -------------- | -------------------------------------------------------- | -------------------------------------------------------------------------------- | ------------------------------------------------------- |
| 1   | **Why**        | `docs/design/<algo>.md`                                  | this page                                                                        | review only                                             |
| 2   | **Declare**    | `objects/<obj>.toml`, a `just-makeit.toml` entry         | [Adding a Module](adding-a-module.md)                                            | `make drift-check`                                      |
| 3   | **Implement**  | `native/inc/<obj>/<obj>_core.h` + `_core.c`              | [Adding a Module](adding-a-module.md), [Error Convention](error-convention.md)   | `ctest`                                                 |
| 4   | **Pin**        | `native/tests/test_<obj>_core.c`                         | [Object Validation](validation.md) step 2                                        | `ctest`, `make tests-ssot`                              |
| 5   | **Bind**       | `.pyi`, `__init__.py`, the `_ext` fragment               | [Module Layout](module-layout.md), [Docstring Authoring](docstring-authoring.md) | `make drift-check`, `make test-stubs`                   |
| 6   | **Instrument** | the state triplet, telemetry probes                      | [state serialization](../../design/state-serialization.md)                       | `check_serializable.py`, the state matrix               |
| 7   | **Explore**    | `native/validation/<obj>_*.c`, `tests/characterization/` | [Object Validation](validation.md)                                               | `make validate-c`, `make characterize`                  |
| 8   | **Certify**    | `tests/validation/<obj>/results.md`                      | [Object Validation](validation.md)                                               | `make validate-check`, `test_validation_limits.py`      |
| 9   | **Document**   | header `@code`, a guide if needed, benchmarks, examples  | [Docstring Authoring](docstring-authoring.md), [Doc Examples](doc-examples.md)   | `make test-stubs`, `make test-examples-c`, `make bench` |
| 10  | **Land**       | CHANGELOG entry, issues for what is left                 | [Release](../release.md)                                                         | `make changelog-check`                                  |

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

**Its `@code` blocks are tests.** `jm` flows each one into the `.pyi` as an
`Examples` section and `make test-stubs` executes it, so an example that
drifts fails a gate rather than misleading a reader quietly. Write them
against a real run — pin the expected output by running the thing, never by
reasoning about what it should print. That is phase 9's first deliverable and
it is cheapest to write here, while the behaviour is in front of you.

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

### 9 — Document it, and carry it back

A finding that reaches only the report reaches nobody, and an object nobody
can find is an object nobody uses. Five deliverables, each with the gate that
keeps it honest — none of them optional except where the table says so:

| you owe                                               | it lives in                                           | kept true by                                        |
| ----------------------------------------------------- | ----------------------------------------------------- | --------------------------------------------------- |
| a `@code` example on each public function             | `native/inc/<obj>/<obj>_core.h`                       | `make test-stubs`                                   |
| a markdown guide, **when** prose outgrows a docstring | `docs/design/<algo>.md`, or `docs/guide/`             | `make docs-check`, `check_nav_index.py`             |
| a benchmark, C **and** Python                         | `native/benchmarks/`, `src/doppler/<mod>/benchmarks/` | `make bench`, `make bench-compare`                  |
| a runnable example, C **and** Python                  | `native/examples/`, `src/doppler/examples/`           | `make test-examples-c`, `make test-examples-python` |
| a gallery page, **when** there is a figure worth it   | `docs/gallery/`                                       | `make test-snippets`, `check_nav_index.py`          |

Three things about that table are worth knowing before you trip over them.

**The two `when`s are the only judgement calls.** A guide earns its place when
the object needs prose a docstring cannot hold — a derivation, a comparison,
a decision a caller has to make. A gallery page earns its place when there is
a figure that shows something a number cannot. Everything else on the list is
owed unconditionally, and "it is obvious from the code" has never once been
true for the person reading it six months later.

**A benchmark is a claim, and an unmeasured claim is a guess.** `jm apply`
scaffolds both files for you, so the deliverable is filling them in, not
creating them. Doing it now is also the only cheap moment: a number nobody
took before the algorithm was tuned cannot be recovered afterwards.

**Both examples are DISCOVERED, and both are owed.** `src/doppler/examples/*.py`
is globbed and `native/examples/*.c` is too, so a new example is gated the
moment it exists; opting one out costs an entry in `.examples-skip` or
`C_EXAMPLE_SKIPS` with a **mandatory reason**, and an entry naming a deleted
script fails a meta-test. The C side used to iterate a hand-written list
instead, which left four of thirteen programs compiling, shipping and
executed by nothing ([gh-863](https://github.com/doppler-dsp/doppler/issues/863),
since fixed) — so discovery is not a convenience here, it is what makes
forgetting survivable.

**C *and* Python, not either.** This asked for one of the two until 2026-08-26,
and one is not enough: the two faces fail differently. A C example is where a
caller sees the lifecycle it must actually manage — create, feed, drain,
destroy, and who owns the buffer — none of which the Python binding exposes
because it does that work for you. A Python example is where the *result* is
legible: a plot, an assertion against a reference, a number a reader can
check. An object with only the C one ships an algorithm nobody can see the
output of; with only the Python one it ships an API nobody can call from C
without reading the header and guessing.

Then carry the findings back. Whatever the certification established — a new
limit, a corrected rule, a number a caller has to choose by — goes into the C
test and into one of the artifacts above, because those are the two things
that keep it true and put it in front of someone.

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

## What each place already gives you

The family of shared helpers exists so that no harness renders its own
waveform, rolls its own noise, or invents its own assertion — and it is
easy to miss, because each member lives beside the tests that use it
rather than on a page. A measurement built on a private copy of any of
these is not wrong at first; it is wrong the day the shipped one is fixed
and the copy is not
([`native/tests/README.md`](https://github.com/doppler-dsp/doppler/blob/main/native/tests/README.md)
is the family's own page and the authority on the C side). The table is
by phase: reach for the row before writing a line of the thing it names.

| phase                 | you need                                                                                                                   | it already exists as                                                                                                                                                                        |
| --------------------- | -------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 3–4, C tests          | assertions, counters, the pass/fail epilogue                                                                               | `dp_test.h` — `DP_CHECK`, `DP_CHECK_NEAR`, `DP_REQUIRE`, `DP_TEST_END` (fails a test that asserted nothing)                                                                                 |
|                       | random bits and Gaussian noise                                                                                             | `dp_rng_test.h` — the one generator, the one Box-Muller; never a private `cgauss`                                                                                                           |
|                       | a shaped symbol stream (RRC/RC/NRZ, real-valued sps, carrier, timing offset)                                               | `dp_tx_test.h` — `dp_tx_cfg_t`, `dp_tx_make`                                                                                                                                                |
|                       | a code-spread BPSK capture, fixed Doppler or a ramp                                                                        | `dp_dsss_test.h` — `dp_dsss_capture`, `dp_dsss_ramp_capture`                                                                                                                                |
|                       | a framed stimulus                                                                                                          | `dp_frame_test.h` — the named frame set, one `wfm_frame_t`                                                                                                                                  |
|                       | an RRC-BPSK-on-carrier fixture and its EVM                                                                                 | `dp_mf_test.h`                                                                                                                                                                              |
|                       | "is this a real lock" without truth data                                                                                   | `dp_sym_test.h` — `dp_test_evm_db_hard`, `dp_test_m2m4_snr_db`                                                                                                                              |
|                       | an error rate with alignment, settling and a confidence interval                                                           | `dp_ber_test.h` — the instrument; a hand `min` over lags is a genie                                                                                                                         |
|                       | a whole receiver measured at an operating point                                                                            | `dp_rx_test.h` + an adapter — [Measuring a Receiver](measuring-a-receiver.md), not a new harness                                                                                            |
|                       | the state round trip and the clobbered-blob reject                                                                         | `dp_state_test.h` — `DP_STATE_ROUNDTRIP_TEST`                                                                                                                                               |
| 3–4, 7, any C         | **a waveform**: tone, PN, BPSK/QPSK, chirp, a bit pattern, a DSSS burst or a continuous asynchronous DSSS stream with data | `wfm_synth` (`wfm_synth_create` + `set_dsss_cont` / `set_dsss` / `set_bits` / `set_symbols`), the generator wfmgen itself renders with; `wfm_synth_noise_steps` for the noise a gap carries |
|                       | a scene: sources summed, segments in time, ranged fields, repeats, Doppler per source                                      | `wfm_compose` (`wfm_compose_create` / `_from_json`) — but it rebuilds a source at a segment boundary, so a fade of a *continuing* emitter is a scalar on one synth's stream, not a segment  |
|                       | noise at a stated SNR or C/N0                                                                                              | `awgn` with `awgn_amplitude_for_snr` (the one answer to "per rail or total"), `wfm_snr_over_fs` for the mode conversion                                                                     |
|                       | Doppler as physics: time-base dilation plus carrier, one parameter                                                         | `doppler_channel` — not a frequency offset                                                                                                                                                  |
|                       | a spreading code                                                                                                           | `gold` (CCSDS #365 is the header's example), `pn`                                                                                                                                           |
| 5, benches            | timing, min over rounds, the settle once per process, the JSON                                                             | `jm_bench.h` + `dp_bench.h`; interleave configurations — [Benchmarking](benchmarking.md)                                                                                                    |
| 7, validation harness | everything in the C-test rows, plus a `--check` spot check registered in CTest                                             | the exemplar is `native/validation/lockdet_verify.c`: a probability measured with the shipped `awgn`, because a private sigma moves a rate without failing anything                         |
| 8, Python report      | the report, its five sections, the limits and the staleness gate                                                           | `src/doppler/tests/_validation_common.py` — `Report`; [Object Validation](validation.md)                                                                                                    |
|                       | an error rate through the binding                                                                                          | `doppler.ber.BerMeter` / `FrameMeter` — they can refuse (`align_ok`, slips); a hand SER cannot                                                                                              |
|                       | a waveform through the binding                                                                                             | `doppler.wfm` — `Synth`, `compose`, `Gold`, `PN`, `SampleClock`, `wfm_awgn_amplitude`                                                                                                       |
|                       | detection sizing: thresholds, dwell, Pd, verify counts                                                                     | `doppler.detection` — `det_threshold*`, `det_pd*`, `det_n_noncoh`, `det_verify_count`                                                                                                       |
| 7, characterization   | a long Monte-Carlo with a fast twin                                                                                        | `src/doppler/dsss/tests/characterization/` — copy a subject's shape; `make characterization-check` fails one without a `__main__` or a twin                                                 |

Two rules the table implies, stated so they are not inferred:

- **A harness renders nothing.** If the signal under test is a waveform
    this library can generate, the harness calls the generator. The first
    pass of the continuous async-DSSS validators
    ([`async-dsss-receiver.md`](../../design/async-dsss-receiver.md) §12)
    built its emitter from chips by hand and derived its own noise sigma;
    the shipped path gave the same numbers to within trial spread, and the
    hand-rolled one was deleted — not because it was wrong that day, but
    because nothing would have told anyone when it became wrong.
- **A harness asserts through `dp_test.h`.** `DP_CHECK` counts, reports
    with file and line, and `DP_TEST_END` refuses a run that checked
    nothing. A `printf("FAIL ...")` and a return code do neither.

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
