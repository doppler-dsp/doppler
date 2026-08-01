# Authoring docstrings — write the C header, get the Python docs

doppler's Python documentation is **derived**, not written. You write Doxygen
comments in a `native/inc/<obj>/<obj>_core.h` header; `jm` turns them into the
numpy docstrings that land in the `.pyi` type stubs (what IDEs, type-checkers,
and the mkdocstrings site read) and, increasingly, into the runtime `__doc__`
that `help()` shows. Write the header once, well, and both faces improve
together.

This page is the house style for that header. Its goal is uniformity: every
header should derive the same shape of top-notch doc, so the API reads as one
system. The mechanism itself is documented on the `jm` side
(`just-makeit`'s `docs/developers/docstring-derivation.md`); this page is about
*what to write*. Coverage is measured by `make check-docstring-coverage` (see
[Docs Conventions](docs-conventions.md) for the gate landscape).

______________________________________________________________________

## The two faces

!!! warning "One source, two faces — never author them separately"

    Both the `.pyi` stub and the runtime `__doc__` are derived from the
    **same** header. You never write either by hand: each is regenerated from
    the header on the next `jm apply`, so a hand-edit is silently overwritten
    (and flagged by the manifest-drift gate). Author the header once — both
    faces improve together.

The two faces it derives:

| Face                    | What it is                                                    | Who reads it                                         |
| ----------------------- | ------------------------------------------------------------- | ---------------------------------------------------- |
| **stub** (`.pyi`)       | full numpy block — brief, body, Parameters, Returns, Examples | IDEs, `mypy`, the mkdocstrings API site (via griffe) |
| **runtime** (`__doc__`) | what `help(obj)` prints                                       | anyone at a REPL                                     |

The stub face is rich today. The runtime face is reaching parity through `jm`
(Parameters/Returns/Notes into the C literals). You do not author for one or the
other: you author the header, and each face renders as much of it as its
pipeline currently supports. **Header text is release-stable** — nothing you
write here needs revisiting when a `jm` capability lands.

______________________________________________________________________

## Anatomy of a documented declaration

The template every public declaration should match:

```text
/**
 * @brief One-sentence summary, as a real sentence.
 * Extended description: how it works, what the intent is, any non-obvious
 * mechanics. Continuation lines flow into one paragraph; a blank ` *` line
 * starts a new paragraph.
 *
 * @param foo  What foo means — units, valid range, constraints. Not its type.
 * @param bar  What bar means.
 * @return     What comes back, and its units/shape.
 * @code
 * >>> from doppler.mymod import MyObj
 * >>> obj = MyObj(foo=0.5, bar=4)
 * >>> round(obj.method(1.0), 3)
 * 0.5
 * @endcode
 */
```

Each tag maps to a numpy section: `@brief`+body → the summary and extended
description, `@param` → **Parameters**, `@return` → **Returns**, `@code` →
**Examples**.

______________________________________________________________________

## The rules, tag by tag

- **`@brief`** — a real one-sentence summary. jm suppresses **only** a brief
    that restates the function name (its scaffold form — `@brief fir_step.`,
    matched ignoring `_`, case, and spacing), falling back to a name stub.
    A vague-but-sentence-shaped brief like `Steps the object.` is treated as
    real documentation and rendered **as-is** — so it is the coverage meter,
    not jm, that catches a lazy brief. Write a sentence that says what the
    thing *does*.
- **Body** (untagged prose after `@brief`) — the extended description. Verbose
    is good: explain how it works and why, per the
    [code principles](repository-map.md). Continuation lines join into one
    paragraph; a blank ` *` line is a paragraph break. Do **not** put a blank
    line between every line — that double-spaces the render.
- **`@param <name>`** — one per parameter, and the name must match the
    (jm-injected) signature. Describe **meaning, units, range, constraints** —
    not the type (jm supplies the type). `@param loop_bw  Loop noise bandwidth in cycles/sample; keep below 1/(4*decim).`
    A mismatched name is **not** an error: jm falls back to a positional zip of
    the leftover descriptions, so a typo'd name silently renders attached to the
    *wrong* parameter. Get the names right (jm#667 will lint this).
- **`@return`** — whenever the function returns a value. (Exception: a
    constructor — `<obj>_create()` — does not get a Returns section; jm renders
    the class from it but omits Returns by design.)
- **`@code … @endcode`** — a **usage example** a reader learns from, which also
    runs in CI. See [Examples](#doctests-that-run) below. This is the single
    highest-value tag: it becomes the Examples section a user copies from *and*
    is executed against the real API, so it can never mislead.
- **`@note` / `@warning` / `@see` / `@retval` / `@pre`** — **write these now.**
    They are dropped from the rendered docstring today and will begin rendering
    into numpy **Notes** / **Warnings** / **See Also** / **Raises** when the
    section-mapping work lands upstream — with **no header rework** in between.
    Treat them exactly as you would for Doxygen.
- **Inline `@c` / `@p` / `@a` / `@ref`** — code/parameter cross-references, safe
    to use: jm strips the marker and keeps the word (`@c clip_db` → `clip_db`,
    `@p n` → `n`). Use `@c` for a literal or expression, `@p` for a parameter
    name. Non-word arguments (`@c -1`, `@c "A"`, `@c +/-10^(x)`) only strip
    cleanly from **jm v0.35.0** — the version doppler pins as of this pass.

______________________________________________________________________

## Examples — teach usage first { #doctests-that-run }

An `@code` block becomes the numpy **Examples** section a reader lands on to
learn how to *use* the thing. **That is its job.** The example exists to show a
user how to leverage the function — realistic arguments, a representative call,
and a result that reveals what the call actually does — so someone reading the
API page comes away knowing how to wire it into their own code.

That every block is *also* executed in CI by `make test-stubs`
(`pytest --doctest-glob='*.pyi'`, against the freshly built extension) is the
bonus that keeps the teaching honest: a printed value that no longer matches
the API **fails the build**, so the usage a reader copies can never have
silently rotted. Documentation first; the test is what stops it drifting.

- **Show real usage, not construction.** A bare `>>> obj = MyObj()` with no
    meaningful call teaches nothing — it proves the constructor exists and
    stops there. Write the smallest example that shows the object *doing its
    job*: feed it a representative input, make the call the method is *for*,
    and print a result that makes the behaviour legible. If a reader could not
    infer how to use the API from your example, it isn't done.
- **Score it, don't eyeball it.** The printed value must be the *physically
    correct* answer — compute it, don't write down what looks plausible. A
    magnitude that "looks about right" but is wrong sails through review and
    fails a user. (This is the same discipline the DSP work follows: lock the
    number against ground truth.)
- **Deterministic, printable output.** Round floats (`round(x, 3)`), seed any
    RNG, never print a wall-clock or an address — so the example a reader
    trusts is also reproducible.
- **Small, but never trivial.** One construct + one meaningful call + one
    checked result is ideal; a Monte-Carlo sweep is not. "Small" trims noise,
    it does not mean "omit the part that shows how to use it".

Good — constructs the AGC, drives it with a sample, and prints results that
show both the passthrough and the loop starting to act, so a reader sees the
call pattern *and* what to expect back:

```text
 * @code
 * >>> from doppler.agc import AGC
 * >>> agc = AGC(ref_db=0.0, loop_bw=0.0025, alpha=0.05)
 * >>> agc.step(1.0+0.0j)            # unity gain at start
 * (1+0j)
 * >>> round(agc.gain_db, 6)         # loop has begun to drive
 * 0.0
 * @endcode
```

______________________________________________________________________

## Per-surface guidance

| Python surface                              | Where the doc comes from                                                       | Notes                                                                                                                                                                                                               |
| ------------------------------------------- | ------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Class**                                   | `<obj>_create()` block                                                         | `@brief` + `@param` (ctor args) reach it today. Body and `@code` are **not yet** rendered on a class (jm#624) — a synthesised construction demo replaces `@code`; write both anyway (release-stable). No `@return`. |
| **Method**                                  | `<obj>_<method>()` block                                                       | The full template.                                                                                                                                                                                                  |
| **Built-ins** `reset`/`step`/`steps`        | `@brief` on that C declaration                                                 | A real sentence, not the scaffold.                                                                                                                                                                                  |
| **Property** (field)                        | manifest `doc=` today; the struct field's trailing `/**<` **not yet** (jm#671) | See [struct fields](#struct-fields) below.                                                                                                                                                                          |
| **Module free function**                    | the module-header function block                                               | Same as a method.                                                                                                                                                                                                   |
| **Module docstring**, **structseq records** | *no header source yet*                                                         | Need new upstream surfaces; not authorable in the header today.                                                                                                                                                     |

______________________________________________________________________

## Lists and tables in prose

When the prose enumerates **modes or flags** — most often for an enum-valued
parameter — reach for a markdown bullet list in the **body** (not inside a
`@param` description):

```text
 * @brief Interpolate a sample from the table.
 * The kernel selects how a fractional index is resolved:
 *   - floor:   nearest sample at or below the point
 *   - nearest: nearest sample either side
 *   - linear:  linear blend of the two bracketing samples
```

**This renders flattened onto one line today** — jm's paragraph grouper
space-joins adjacent lines, and list preservation is jm#653 (scheduled, not yet
shipped). Author it correctly now anyway: the header text needs no rework when
#653 lands. Keep lists in the **body**, because `@param` continuation lines are
joined even after #653 (that issue is about the body, not param descriptions).
A comparison across options is a markdown pipe table — same "correct now,
renders flat until #653" caveat.

______________________________________________________________________

## Struct fields → properties { #struct-fields }

A struct field that becomes a Python property is most naturally documented right
where it is declared, with a trailing member comment:

```text
typedef struct myobj_state
{
    double gain_db;   /**< Current loop gain, in dB (unity = 0). */
    size_t decim;     /**< Envelope decimation factor. */
} myobj_state_t;
```

That `/**<` text is the doc a property *should* carry. **jm cannot see it
yet** — extraction reads only `/** … */` blocks preceding a declaration, so a
trailing member comment derives nothing today (jm#671, filed off doppler's own
grep: ~518 struct-field comments; scheduled, not shipped). Until it lands, a
property's rendered doc comes from the manifest `doc=` (or a documented
getter). Author the `/**<` anyway — it is the right home, it is release-stable,
and it is where jm#671 will read from. Keep it accurate: a stale member comment
becomes a stale property doc the moment #671 ships.

______________________________________________________________________

## The workflow

1. Edit `native/inc/<obj>/<obj>_core.h`.
1. Run `jm apply` (scoped — `jm apply objects/<obj>.toml` — when a sacred
    `_ext_<obj>.c` fragment exists), which regenerates the `.pyi` from the
    header.
1. `make test-stubs` — runs every `@code` doctest against the build.
1. `make check-docstring-coverage` — confirm the module's incomplete count
    dropped and no tag leaked.
1. Check Doxygen at **CI's version** (older than a typical local one) — the
    zero-warnings gate is version-sensitive.
1. Commit the header **and** the regenerated `.pyi` together — the
    manifest-drift gate fails on a header edit without its stub.

**Never hand-edit a generated `.pyi` or `_ext.c`.** They are owned by `jm apply`
and guarded by the drift gate; a manual edit is reverted on the next apply and
flagged by CI. The header is the source of truth.

______________________________________________________________________

## What "documented" means (the meter)

`scripts/check_docstring_coverage.py` scores every public callable on **both
faces**:

- **FULL** — summary + a `Parameters` entry for every parameter + `Returns`
    (when it returns) + at least one `>>>` example. A property needs only a
    non-empty docstring.
- **PARTIAL** — a summary and some sections, but missing a required one.
- **STUB** — no docstring, or a bare summary.

The gate **ratchets**: a module's incomplete count may drop but never rise, and
a raw Doxygen tag surviving into rendered text is zero-tolerance. So each
authoring PR lowers a number, and nothing backslides.

______________________________________________________________________

## Don'ts

- Don't ship a vague `@brief` (`"Reset the state."`). jm will *not* flag it —
    it only suppresses a brief that restates the function name, so a vague
    sentence renders verbatim; the coverage meter is what catches it.
- Don't restate the type in `@param` — describe meaning, units, and range.
- Don't ship a construct-only example (`>>> obj = MyObj()` and nothing more).
    It passes the doctest and teaches a reader nothing; the example's job is to
    show the API *doing its job*. Make the call the method is for.
- Don't write an example you have not run and verified.
- Don't use `///`, `//!`, or `/*!` comment forms — jm sees only `/** … */`, so
    those derive **nothing**, silently. doppler is 100% `/** */`; keep it that
    way.
- Don't hand-edit the generated `.pyi` or `_ext.c`.

______________________________________________________________________

## See also

- [Docs Conventions](docs-conventions.md) — what's generated vs. hand-owned,
    and every docs gate.
- [Doc Examples](doc-examples.md) — how the fence and `.pyi` doctest gates work.
- [Adding a Module](adding-a-module.md) — the full `jm` workflow a new object
    goes through.
- `just-makeit`'s `docs/developers/docstring-derivation.md` — the derivation
    pipeline itself (the source of truth for the mechanism).
