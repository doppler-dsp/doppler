# A Frame as a Description — the measurement record

*The dated record behind [the design page](frame-description.md): what was
measured, in the order it was measured, with the numbers, the wrong guesses
and what corrected them. The design page states what is; this page is why.
Section letters are shared — an `§F.2` cited in an issue, a test or a code
comment is the same entry here and there — and nothing is rewritten after the
fact: a later entry corrects an earlier one in its own words.*

______________________________________________________________________

## F. The Field

### F.1 How many ways a field was spelled (2026-09-27)

A read-only survey of `main` at `7b51ac4b`, taken to answer one question before
designing anything: is the shape wrong, or is only the Field missing? The
primitives were right — `wfm_seq_t`, `wfm_field_t` and `wfm_frame_desc_t` in
`native/inc/doppler/wfm/wfm_frame.h`, with one transmit compiler,
`dp_wfm_source_describe_frame()` in `wfm_synth_bridge.c` — and around them
the same two concepts had been restated many times.

**Seven representations of a sequence of bits:**

| #   | representation                                                                    | where                                                                                                           |
| --- | --------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------- |
| 1   | `wfm_seq_t` — the canonical one                                                   | `wfm_frame.h`                                                                                                   |
| 2   | a `(uint8_t *, len)` pair                                                         | `ccsds_tm_frame_spec_t`, `dp_wfm_synth_set_dsss`, the four-field DSSS helpers, every receiver's init parameters |
| 3   | twelve flattened arguments per field                                              | `dp_frame_create`, `dp_frame_create_desc`, `add_field`                                                          |
| 4   | the loose PN parameters of a source, borrowed by `--payload-len`                  | `wfm_compose.h`, `wfmgen.c`                                                                                     |
| 5   | the CLI triple `--X` / `--X-hex` / `--X-gen`                                      | `wfmgen.c` option table                                                                                         |
| 6   | a JSON literal string plus a `*_gen` object; `lit` / `gen` inside a carried frame | `wfm_json.c`                                                                                                    |
| 7   | the kind strings in the generated `Frame` bindings                                | `wfm_ext_frame*.c`                                                                                              |

**Six representations of a frame:** `wfm_frame_desc_t`; the fixed-slot
`wfm_frame_t` with its own `wfm_frame_layout_t`; the flat framing fields of a
source; `ccsds_tm_frame_spec_t`; `ccsds_tm_frame_cfg_t`; and the receivers'
own parameters (`acq_code`, `data_code`, `sync`, `reps`, a scalar
`frame_syms`).

**The same work, done more than once:**

- **Two sequence grammars that disagree.** The CLI's `pn:LEN:REG…` could not
    set `lfsr`; the JSON `*_gen` object could.
- **Four bit-string parsers.** `wfmgen.c` refuses a stray character; the JSON
    reader skips it; jm's `bit_pattern` coercion is a third; `Frame`'s a fourth.
    A typo reads as a shorter sync word on one face and a refusal on another.
- **The `reg_bits` range checked at five sites** across `wfmgen.c` and
    `wfm_json.c`.
- **Checks that test the pointer, not the length** — which a generated
    sequence, having no array, fails
    ([#1592](https://github.com/doppler-dsp/doppler/issues/1592)); the same
    class `dp_wfm_source_has_frame()` had already been fixed for
    ([§C.1](#c1-the-sites-and-how-each-was-settled)).

The answer: the shape is right and the Field is missing. Every one of the
seven exists because a field had no value of its own to pass.

### F.2 The grammar, prototyped before any C (2026-09-27)

A throwaway Python parser and printer for the grammar in §F.1 of the design
page — never committed — was run against every sequence the flag-matrix
golden (`native/tests/wfmgen_flag_matrix.json`) records.

| measured                                                                                  | result                                                                                                                                                                           |
| ----------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| golden sequences converted old spelling → Field                                           | **40 / 40** lossless: same kind, length, parameters and bits                                                                                                                     |
| `parse(format(f)) == f` and `format` stable                                               | **40 / 40**                                                                                                                                                                      |
| by kind                                                                                   | 27 literal, 10 pn, 2 gold, 1 dotted                                                                                                                                              |
| malformed specs refused (empty, `0102`, `0x`, `pn:31:65`, `dotted`, `1101*0`, a space, …) | **16 / 16**                                                                                                                                                                      |
| a repetition count other than 1, in the golden                                            | 4, all `2`, all on the acquisition code                                                                                                                                          |
| old spellings in `docs/` and `src/doppler/examples/`                                      | **58** — `--bits-hex` 23, `--acq-reps` 8, `--acq-code-hex` 5, `--sync-gen` 4, `--payload-len` 4, `--payload-gen` 4, `--data-code-hex` 4, `--data-code-gen` 3, `--acq-code-gen` 3 |
| `--acq-reps` values in docs and examples                                                  | 4 of 4 occurrences with a value are `4`                                                                                                                                          |

What it settled: the grammar expresses everything every face currently
records, with nothing lost; the printed form round-trips; and `*N` is rare
enough that quoting it is cheap.

### F.3 How a refusal names its cause (2026-09-27)

The [error convention](../dev/contributing/error-convention.md) gives an
`int` function `DP_OK` or a negative code and no reason, and a CLI that says
only "invalid" about a malformed Field teaches nothing. The wfm module had
already met this: `dp_wfm_compose_from_json_why(json, const char **why)`
returns a **static** sentence through an optional out-parameter, because "a
NULL return is the one answer that cannot teach anything"
(`wfm_compose.h`). `dp_wfm_field_parse` takes the same shape rather than a
new convention.

### F.4 What review changed (2026-09-27)

The first draft said every face carries a Field **as one string**. Review
asked whether that meant payload bits had to be strings; it did, and it was
wrong. Three corrections followed, each from the owner, in this order:

1. **A literal field is data.** A payload is an array, a binary file or a
    byte stream; only a generated field is text by nature. The draft had
    generalised from sync words — short, written by hand — to payloads, which
    are neither.

1. **Two use cases decide the payload's shape:** a finite burst with all data
    known a priori, and an infinite stream that must be chunked and framed.
    Neither is served by one block cycled to fill the run, so the payload
    became a data source consumed a frame at a time (`data:LEN`, `--data`).
    The decisions that fell out: the last partial chunk is **padded with a
    declared fill** (refused without one), a paced stream that runs dry sends
    **idle frames**, and the feature is **designed now and built after** the
    Field.

1. **"Delete capability that isn't strictly necessary."** Applied, it removed
    more than it added:

    | deleted                                                                                                                  | because                                                                             |
    | ------------------------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------- |
    | payload cycling                                                                                                          | a burst runs once; more is `--repeat`, which exists                                 |
    | six payload flags (`--bits`, `--bits-hex`, `--bits-file`, `--payload`, `--payload-gen`, `--payload-len`)                 | one question — where the payload comes from — one flag, `--data`                    |
    | the Python `Field` object                                                                                                | a frame's layout already reports its fields                                         |
    | `_` separators in the grammar                                                                                            | convenience syntax, one fewer rule                                                  |
    | the frame sugar — six CLI coding flags, the flat source framing fields, the by-name compiler, the "both at once" refusal | a frame said two ways needs a compiler and a tie-break; said one way, needs neither |
    | `prbs` as a name                                                                                                         | it is `pn:0:REG[:SEED]` in the one grammar                                          |

    **Kept**, each because removing it would lose something a caller needs:
    both LFSR forms (a published sequence can require Fibonacci), `*REPS` (a
    DSSS preamble is a repeated code), `--data none` (code-only continuous
    DSSS), and the fixed-layout CLI field flags for the common frame — which
    build a description, not a second representation.

Two more followed later in the same review, and both **deleted** an unknown
rather than answering it:

- **A receiver is designed in the know.** The draft listed "how a receiver
    tells an idle frame from data, and where the data stopped" as needing a
    general in-band mechanism. It needs none: a receiver is built against the
    description it expects, so a length or idle marker is a field at a known
    position (`N:M`), and the generator does not interpret it.
- **A repeat is invariant.** The draft left "what `--repeat` means over a
    data source" open, reading a repeat as possibly drawing new data. It never
    does: a repeat sends the same bits again, whatever the field is —
    `data:LEN*N` is simple time diversity for resilience — and `--repeat`
    over an unending stream is refused.

### F.5 The three jm scaffold checks (2026-09-27)

The design names three things jm must do before they are relied on (§
[Unknowns](frame-description.md#unknowns)). Each was measured at the pinned
**jm 0.92.2**. Checks 1 and 2 used a **fresh** `jm new` project (one
no-state object with `bits: uint8_t[]` and `spec: const char *`, whose
`create()` prints what reached C), not doppler, so the result is jm's
behaviour and not doppler's history. Check 3 used doppler's own manifest in
a throwaway worktree.

| check                                              | result                                                                                                                                                                                                                                                                                         |
| -------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1. an object param taking an array **or** a string | **No.** There is no such param type, and `coerce = "bit_pattern"` on an init-param is warned as "a composer field key" and ignored                                                                                                                                                             |
| 2. an omittable param                              | **Yes, both kinds.** An array with `default = "[]"` reaches `create()` as `NULL`, length 0 (jm#1003). A `const char *` with `default = "NULL"` is `str \| None` and reaches C as `NULL`. (`optional = true` is something else: it dispatches to a second constructor and needs a `create_fn`.) |
| 3. extra keys survive a re-save                    | **Yes.** Seven keys (`cli`, `cli_aliases`, `json`, `help`, `metavar`, `faces`, `kind`) on a `source.fields` row, plus a sibling `[module.wfm_compose.surface]` table, round-trip through a mutating `load`/`save` and read back intact. `jm apply` and `jm status --check` exit 0              |

What the probes turned up besides the answers:

- **A string into a `uint8_t[]` param is silently a number.** `Fld("0101")`
    reaches C as one element, `101`. `Fld(b"\x01\x00")` is *refused*
    (`ValueError: invalid literal for int()`), so the one Python type that
    already is a byte buffer cannot be passed. Both come from one conversion
    through `int()`: [jm#1700](https://github.com/just-buildit/just-makeit/issues/1700).
- **Unknown keys are advisory, never gating.** `apply` prints one
    deduplicated `warning ~:` line per unknown key (8 for the probe), with
    `gates=False`. But `jm upgrade` calls the same manifest "Not up to date:
    apply will refuse". It does not refuse:
    [jm#1702](https://github.com/just-buildit/just-makeit/issues/1702).
- **A save that dirties a composer table deletes its comments, but no
    command does that.** Calling jm's own `load`/`save` with one field's
    `doc` changed removed **15 comment lines** from that table. That was a
    probe for "some command rewrites this table", and the premise was
    wrong: a real mutating command (`jm method fir …`) writes only
    `objects/fir.toml` and leaves the root manifest alone. The defect is
    reachable only through the API (or, unmeasured, a schema migration):
    [jm#1701](https://github.com/just-buildit/just-makeit/issues/1701).
- **A composer field is documented twice.** All 40 `wfm_compose` fields
    carry a manifest `doc` key, which jm reads (`_composer.py`), and
    `wfm_compose.h` comments the same struct members. Nothing ties the two
    together, and they already disagree (`freq`). The header is the
    primary SSOT, and the manifest `doc` is a fallback, not a peer. A surface-table `help` key would be a
    third copy:
    [jm#1703](https://github.com/just-buildit/just-makeit/issues/1703).
- **The design's stated fallback for check 1 no longer exists.** It was "an
    array goes through `Field.from_bits`", and §F.4 above deleted the
    `Field` object. So a failed check 1 needs a new decision, not a
    fallback.

**Decided** (owner, the same day). The first answer was two parameters
per field, `<field>=` for data and `<field>_spec=` for text. It was replaced
before landing by something simpler: **the C interface takes bits only**, one
empty-by-default `uint8_t[]` per field, and module helpers (`field_bits`,
`cvt.hex_to_bin`, `cvt.bytes_to_bin`) turn every other form into bits. That
needs no dispatch and no jm feature, and removes the "both set" refusal
(design §F.3). The surface keys stay **on the jm rows**, and the advisory
warnings are accepted.

______________________________________________________________________

### F.6 The parser, explored (2026-09-29)

Phase 7 of the lifecycle, run once on purpose: a parser has no statistical
envelope, so the exploration is a corpus, not a sweep. A scratch harness
compiled `wfm_frame.c` under ASan and UBSan and drove
`dp_wfm_field_parse`, `dp_wfm_field_format` and `dp_wfm_field_bits` with:

| corpus                                                                                              | size    | property                                                                                                                                     |
| --------------------------------------------------------------------------------------------------- | ------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| generated valid Fields (bin, hex, pn, gold, dotted, `*REPS`)                                        | 200,000 | parse → format → parse is the same field; the text is canonical (formatting twice agrees); both texts render the same bits, every one 0 or 1 |
| single-character mutations of those (delete, insert, replace, truncate)                             | 200,000 | accepted or refused, never a crash; if accepted, the same round trip                                                                         |
| hand edges (2^64-1 and 2^64 lengths, `*0`, `**2`, huge `*REPS`, `PN:`, signed numbers, a bare `0x`) | 26      | as above                                                                                                                                     |

The corpus is reproducible: an xorshift64 generator seeded with
`0x9E3779B97F4A7C15` picks one of five shapes — a 1–70-bit binary literal,
a `0x`/`0X` literal of 1–20 digits drawn from both cases, `pn` (LEN 1–3000,
REG 2–31, five argument shapes including an explicit seed, a hex seed and
zero poly, `fibonacci`, and one malformed empty-seed form), `gold` over the
CCSDS preferred pair with LEN 1–2000, and `dotted` with LEN 1–500 — and
appends `*1`–`*9` to a third of them. The mutations draw from the
alphabet `01:x*X_- +pngoldtd9aF\t`.

**One finding, in 422 cases, all one class.** `pn:LEN:1` with no POLY —
spelled `1`, `01` or `0x1` — was ACCEPTED by the parser and could never be
built: a 1-bit register has no maximal-length polynomial, so the render
refused it (#1602). The refusal therefore arrived later, on every face, and
on two of them without a reason (`wfmgen --bits pn:12:1` and
`--data-code pn:7:1` exited 1 with "could not build the waveform spec").
It is now refused where the text is read, naming the remedy — give POLY, or
a wider REG — and pinned in `test_wfm_frame.c`'s refusal table, red before
the fix. A 1-bit register WITH a POLY builds, and is pinned as accepted.

After the fix: 259,037 round trips, 200,000 mutations, **0 findings**, no
sanitizer report.

**A second finding, outside the parser, filed rather than fixed here.** A
huge LEN parses (`pn:4000000000:5`, up to `2^64 - 1`) and the face that
holds the bits then ABORTS: `wfmgen --bits pn:4000000000:5` exits 134, on
`SIGABRT`, because the source path allocates the caller's length with the
abort-on-OOM helper. Python's `field_bits` reports "negative dimensions" for
`2^64 - 1`. The design states no length limit, and choosing one is a design
decision, so it is [#1622](https://github.com/doppler-dsp/doppler/issues/1622).

## C. The CCSDS sites

### C.1 The sites, and how each was settled

Moved here from the design page on 2026-09-27, unchanged, when the page was
narrowed to state what is.

Of the five the earlier plan listed, **site 1 is done** —
`dp_wfm_source_describe_frame()` builds through the by-name builder rather than
`dp_ccsds_tm_frame_desc_of()`. Three others turned out not to be leaks at all:
`frame_core.c`, `wfm_synth_bridge.c` and `burst_demod_core.c` include
`ccsds_tm` to *compose* it, in the acyclic direction the design intends, and
each is the place a caller is meant to meet the standard's kernels.

The fifth was a different kind of thing, and it is **closed**
([#1220](https://github.com/doppler-dsp/doppler/issues/1220)):

| site                              | what it was                                                                                | how it was settled                                                                                                                                                             |
| --------------------------------- | ------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| the marker's own translation unit | a CCSDS translation unit compiled into the **general** `wfm_core`, under `native/src/wfm/` | deleted. The marker is `doppler.ccsds.asm_bits()` now, over a `ccsds` component that delegates to `dp_ccsds_tm_asm_bits` — the standard beside the general layer, not under it |

A marker one standard picked is a **literal field of a preset**, not a symbol
in the general namespace. The move needed somewhere to put it, and every
existing module says in its own docstring that it is general — `coding`'s
opening line is *"the general channel codes … rather than any standard's
picks"* — so `doppler.ccsds` was created to be the one place a published
literal is at home.

Two things changed with site 1 that the plan did not anticipate:

- **A source carries `wfm_seq_t`** for its preamble, spreading code and sync
    word, rather than three pointer/length pairs. Those pairs could only ever
    describe a LITERAL run, so the four field kinds were reachable from no
    face at all — the descriptor supported them and every route into it
    flattened them away
    ([#762](https://github.com/doppler-dsp/doppler/issues/762)).
- **`dp_wfm_source_has_frame()` tests LENGTH, not the pointer.** A generated
    sequence has no array, so a pointer test read a PN sync as *unframed* and
    emitted the payload bare.

### C.2 The isolation gate was once too broad

An earlier version of `make ccsds-isolation-check` scanned every component
and allowlisted the four that include a `ccsds_tm` header, as a ratchet meant
to fall to zero. That was wrong, and worth recording because the mistake is
easy to repeat: **a consumer composing the two is the design working.**
`frame -> ccsds_tm -> wfm_frame` is acyclic and deliberate — `ccsds_tm` has no
Python binding and is not getting one, so `frame` is where a caller meets the
outer code, the randomiser and the inner code. A ratchet over a rule that
should never reach zero is a slow push toward a refactor nobody wants.
