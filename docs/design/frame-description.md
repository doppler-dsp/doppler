# A Frame as a Description

A frame is a list of **fields** that appear on the wire and a list of
**stages** that transform them. This page is the design for holding that once,
generally, so a standard's framing is a configuration a caller writes rather
than a framer somebody adds.

It is written before the code, which is where a design page belongs: this is
the phase with no gate, and therefore the cheapest place to be wrong
([Adding an Algorithm](../dev/contributing/adding-algorithms.md), phase 1).

______________________________________________________________________

## Why — the tree already has three framers and a 38-argument constructor

Every other standard-specific thing in doppler decomposed into *a general
primitive* plus *a configuration*. `conv_core.h` owns convolutional codes as a
description and `CCSDS_TM_CONV` is four numbers that configure it.
`rs_core.h` owns any Reed-Solomon code over any field and `CCSDS_TM_RS` is
five. `rs_core.h` states the principle outright — *"a standard picking a code
is not the same fact as the code existing"* — and `ccsds_tm_rs.h` repeats it
about the interleaver and the dual basis.

Framing is the one layer where that did not happen. What exists instead:

| what                        | shape                                    | owns                                            |
| --------------------------- | ---------------------------------------- | ----------------------------------------------- |
| `wfm/wfm_frame.h`           | a **closed struct** of four named fields | `[preamble × reps \| sync \| payload \| crc16]` |
| `ccsds_tm/ccsds_tm_frame.h` | a **second, disjoint** assembler         | `[ASM \| RS codeblock]`, four coded stages      |
| `frame/frame_core.h`        | a Python object over the first           | no layout at all — pure delegation              |

The two framers share nothing. Neither can express the other. And the closed
struct has already reached its limit in a way that is visible from outside:
`frame_create()` takes **38 positional arguments** — three fields at twelve or
thirteen numbers each, plus a CRC flag — because a fixed field list forces
every field's every parameter into the signature. Adding a fifth field is a
signature change in the Python face.

That is the cost being paid today. The cost being paid tomorrow is worse: a
user who wants a frame doppler did not anticipate has no move except to add a
fourth framer.

______________________________________________________________________

## Use cases — who calls this, and what they do with the answer

- **`wfmgen`, generating a test waveform.** Framing is *already* an axis there
    and not a waveform type: the usage text reads *"`--acq-code`/`--sync`
    describe a FRAME"*, layered on top of `--type bits` and the user's own
    `--bits`. Coding is the next stage on that same axis. A user should be
    able to say "randomise this, Reed-Solomon it at depth 5, prepend this
    marker, convolutionally code the lot" over bits they supplied, and get
    channel symbols.
- **The scoring path, measuring a capture.** `wfm_frame.h`'s premise is that
    the description is *"described once and read from both ends"* —
    `wfm_frame_crc_ok()` is what makes a **truth-free** frame error rate
    possible. A frame with an outer code has a strictly better error detector
    than CRC-16: `ccsds_tm_frame_decode()` already reports `rs_ok`,
    `rs_corrected` and `rs_symbols`, which is a refusal *and* the margin being
    spent. A generalized description is what lets one scorer read both.
- **A Python caller analysing a capture.** `frame/frame_core.h` exists because
    *"only C could hold one"* and `ber`'s frame meter had no way to be fed from
    the language captures are analysed in. A user-defined field list is what
    lets that object describe a frame doppler has never seen — including a
    CCSDS CADU, without `ccsds_tm` growing a binding it should not have.
- **A mission that is not CCSDS.** The point of the generalization. CCSDS is
    the first configuration because it is the one whose facts are published,
    not because it is the target.

______________________________________________________________________

## The model

Two lists. The second is where the value is.

```text
fields   ordered by POSITION — what appears on the wire, in order
         { name, kind, len, derived_by }
         kind: literal | pn | gold | dotted | marker | payload

stages   ordered by APPLICATION — what transforms which fields
         { kind, cover, config, emits_unit }
         kind: crc | rs | interleave | randomise | conv
```

A **layout** function derives every span and every length from the two, which
is exactly what `ccsds_tm_frame_layout()` already does for its fixed four and
`wfm_frame_layout()` for its fixed three-plus-CRC. Generalizing the descriptor
does not invent a new operation; it widens the one both already have.

The shape above is **the prototype's**, not the first sketch's — see
[what the prototype settled](#what-the-prototype-settled) for the two things
that changed and why.

### `cover` is the load-bearing field, and the header says why

The single most important constraint on this design is already written down,
in `ccsds_tm_frame.h`, as a prediction of how it fails:

> An order is the representation that cannot express this: any chain of
> optional transforms applied to "the frame" is right at three stage
> boundaries and wrong at the fourth, and wrong in the direction that still
> encodes, still decodes against itself, and syncs to nothing.

A generic frame with user-defined fields and a list of optional transforms is
**precisely that chain** — unless every stage carries an explicit `cover` and
never an implicit "everything so far". Get this wrong and the general version
reintroduces, for every user, the exact defect `ccsds_tm_frame_layout_t` was
shaped to make impossible. `cover` is not a refinement to add once the basic
case works; without it the basic case is the bug.

Two consequences worth stating separately, because both are easy to lose:

- **Order and coverage are independent axes.** In CCSDS the ASM is *inserted
    third* and is covered by the stage applied *fourth*. A single ordered list
    cannot say that. The field list gives position; each stage's `cover` gives
    reach; neither is derivable from the other.
- **A stage may change length, and there are exactly two ways it can.**
    Reed-Solomon adds 32 check symbols per codeword and the CRC adds 16 bits —
    both of which *appear on the wire inside the same unit*. The convolutional
    code is different in kind: it consumes the assembled CADU and emits a
    **different stream**, channel symbols, which is why
    `ccsds_tm_frame_layout_t` carries `cadu_bits` and `out_bits` as two
    numbers rather than one. The first kind is a `derived_by` field; the second
    is a stage's `emits_unit`. Collapsing them into one "expand" rule is what
    the prototype started with and what it argued out of.

### The packed/unpacked boundary belongs to the assembler

`ccsds_tm_frame.h` says this explicitly and it does not become less true when
generalized: Reed-Solomon wants **packed octets** because an R-S symbol *is* a
byte, while the randomiser and the convolutional coder want **unpacked bits**
because they are bit machines. Both are right. The conversion belongs in
exactly one place rather than hidden inside a kernel that then works for one
caller — so the general assembler owns it, per stage, and a stage declares
which representation it consumes.

______________________________________________________________________

## Two configurations, and they are the design's falsification targets

A generalization that cannot express what already ships is not a
generalization. Both of these must fall out as **data**, with no special case
in the assembler:

### 1. doppler's existing frame

```text
fields:  [ preamble × reps | sync | payload | crc derived_by=crc16 ]
stages:  [ crc16  cover={payload} ]
```

If this needs a special case, the model is wrong. Note that it also fixes a
thing the closed struct states in prose and cannot express — the CRC covers
*the payload alone*, and the preamble sits outside the group — because
`cover` says so instead of a comment saying so.

### 2. one CCSDS CADU

```text
fields:  [ ASM | payload | rs_parity derived_by=outer ]
stages:  [ rs(255,223,E=16) depth=I  cover={payload, rs_parity}
           randomise                 cover={payload, rs_parity}
           conv(K=7, r=1/2)          cover={ASM, payload, rs_parity}
                                     emits_unit: b -> 2b ]
```

A stage's free parameter carries its configuration: `rs`'s `depth` is the
interleaving depth, and `randomise`'s selects **which** generator, since
131.0-B-6 specifies two and only the matching receiver derandomises a given
waveform. That is deliberately not something a kernel picks for itself.

The coverage asymmetry that the whole `ccsds_tm` slice exists to get right —
outer code no, randomiser no, inner code **yes** — stops being three
hand-written struct members and becomes a field range a user can write. And
the falsification is the strongest one available: the general assembler's
output must equal `ccsds_tm_frame_encode()`'s **byte for byte**, which is the
published-vector discipline this slice was built on, turned on the
generalization itself.

______________________________________________________________________

## Unknowns — the numbers and shapes this design does not yet know

Written down first, deliberately, so the work that follows measures them
rather than confirming a decision already made.

- **How many fields a real frame needs.** Five covers both configurations
    above. Whether the list should be bounded (a fixed maximum, stack-friendly,
    no allocation) or unbounded is unresolved, and it decides whether the
    descriptor stays a POD.
- **Whether a stage's `cover` must be contiguous.** Both configurations above
    use a contiguous range. A non-contiguous cover is expressible as a bitmask
    over fields at no real cost, but nothing yet needs one, and *"do not design
    for hypothetical future requirements"* applies.
- **Whether `interleave` is its own stage or part of `rs`.** CCSDS 4.4.1
    describes S1/S2 as part of the coding, and `ccsds_tm_rs_encode_block()`
    implements them together. Splitting them is more general and may be
    generality nobody asked for. *The prototype folded them, on the second
    ground: nothing in either configuration distinguishes them, and depth is
    already a parameter of the one stage.*
- **Whether `jm` can express the Python face.** A user-defined field list is a
    variable-length structured argument, and the current binding is 38 scalars.
    Whether that is a declarable shape in the manifest — or needs a jm feature
    filed upstream — is unknown and is on the critical path for
    `frame/frame_core.h`.
- **The migration cost in the DSSS path.** `wfm_frame_dsss_chips()` builds
    these bits and spreads them, reading the named offsets. It is the consumer
    most likely to resist an indexed field list.
- **What the CLI syntax should be.** Not a detail: the flag surface is pinned
    case-by-case by `native/tests/wfmgen_flag_matrix.json`, carried in
    `--record`, and published in `site/schema/wfmgen.schema.json`, so it is
    expensive to change after it ships.

______________________________________________________________________

## What the prototype settled

Phase 1 calls for *"a Python prototype to de-risk — throwaway, in a scratch
directory, to find out whether the idea works at all"*. One was written and is
not committed. It computes **no bits**: it holds fields and stages, derives a
layout, and is checked against what the two shipped framers actually report,
extracted from C (`ccsds_tm_frame_layout()` and `wfm_frame_layout()`). Lengths
and spans are the whole claim under test, so lengths and spans are all it
computes.

It reproduces **all three ground-truth layouts exactly**, from one `layout()`
function with no special case:

| configuration                   | result                                                                                             |
| ------------------------------- | -------------------------------------------------------------------------------------------------- |
| CCSDS concatenated, depth 5     | `out=20464 block=10200 cadu=10232`, spans `marker=0,32 outer=32,10200 rand=32,10200 inner=0,10232` |
| CCSDS, marker + randomiser only | `out=544 block=512 cadu=544`, spans `outer=0,0 rand=32,512 inner=0,0`                              |
| doppler's own frame             | `pre=0,24 sync=24,13 pay=37,16 crc=53,16 total=69`                                                 |

So the claim holds: the two framers this page opens by counting are one
descriptor with two configurations. Two things changed on the way, and both
are folded into the model above:

- **A derived field IS a field**, and making it one deletes machinery rather
    than adding it. The first version expanded the field a stage covered
    (payload 8920 → 10200 bits under R-S); the second gave the check symbols
    their own `derived_by` field and let the layout simply sum. Both reproduce
    ground truth, and the second needs **no in-unit length rule at all** —
    which also makes R-S parity and a CRC trailer the same concept instead of
    two.
- **"Emits a new unit" is a distinct property, and the model needs it.** This
    was missing from the first sketch and the prototype would not reproduce
    `out_bits` without it. A stage either transforms the unit in place or
    consumes it and emits a different stream; the convolutional code is the
    only one of the five that does the latter, and it is exactly why
    `ccsds_tm_frame_layout_t` reports `cadu_bits` and `out_bits` separately.

One caution the prototype cannot speak to, and it is the important one: it
tests **lengths and spans, not bits.** That a descriptor produces the right
coverage table says nothing about whether an assembler reading it applies the
stages to the right bits. That is what falsification target 2 is for, and it
stays a target — byte-for-byte against `ccsds_tm_frame_encode()`, not against
a round trip.

______________________________________________________________________

## Implementation sketch

**Home: `wfm/wfm_frame.h`, widened in place.** It already holds the
described-once-read-from-both-ends contract and is already on wfmgen's path;
widening it keeps one home and one set of callers to migrate rather than
adding a fourth framer to a page that opens by counting three.

**Composes, never reimplements.** Every stage already exists as a general
primitive with a CCSDS configuration beside it: `conv_core.h` + `CCSDS_TM_CONV`,
`rs_core.h` + `CCSDS_TM_RS`, `ccsds_tm_randomise`, `ccsds_tm_asm_bits`, and
`dp_crc16_ccitt`. The generalization adds a **descriptor and a layout**, and
no arithmetic whatsoever. If a stage's implementation appears in this work,
something has gone wrong.

Three consumers migrate, and the third is the one to schedule first because it
is the one with an external interface:

1. `wfm_frame.c` — the layout owner; named offsets become an indexed list
1. `frame/frame_core.h` — the Python object, today 38 positional arguments
1. `wfmgen` — the parse table, `--record`, the flag matrix, the schema

**A Python prototype de-risks the model before any C moves** — throwaway, in a
scratch directory, not committed, per phase 1. What it has to answer is
whether the field/stage/cover triple can express both configurations above
without a special case, because that is the whole claim.

______________________________________________________________________

## Deliberately not in scope

- **Virtual fill.** A frame off the `223 × I` octet grid still has no path
    through the outer code
    ([gh-813](https://github.com/doppler-dsp/doppler/issues/813)). The
    generalization makes that refusal *user-visible* for the first time, which
    is an argument for fixing it, not for hiding it behind padding — silently
    padding produces a codeblock a receiver configured for the full length
    cannot parse.
- **A streaming receiver object.** `ccsds_tm_frame_decode()` begins after the
    inner decode and after frame synchronisation, for the reasons its header
    gives: a Viterbi is streaming and emits decisions `depth` bits late, and
    the marker is only readable once the inner code is undone. That boundary
    does not move here.
- **New coding stages.** Turbo, LDPC and the B-5 channel interleaver are
    configurations this model should be able to grow into. None is being
    added.
