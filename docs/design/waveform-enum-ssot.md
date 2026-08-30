# One home for the waveform enum tables

Every waveform choice a user spells as a word — `--type qpsk`, `--file-type sigmf`, `"data": "prbs"` — is an **index** by the time it reaches C. The word
is looked up in a table of strings and the *position* it was found at is the
value that gets stored. Nothing else records that mapping: there is no
per-value tag, no lookup by name at the point of use, no assertion. The table
**is** the enum.

That property is what makes a duplicated table dangerous in a way ordinary
duplication is not. Two copies of a function drift into two behaviours and
somebody notices. Two copies of an ordered name list drift into *the same
program mapping one word to two different values*, with no compile error, no
exception, and no wrong-looking output — just a different waveform than the
one that was asked for.

This page records what owns those tables, why the ownership is arranged the
way it is, and what checks it — independently of the comment at the top of
`native/inc/wfm/wfm_names.h`, which states the rule but cannot be the reason
for it.

## What was measured

`just-makeit.toml` has said since gh-285 that its `[[enum]]` blocks are the
single source of truth, "collapsing the string tables historically duplicated
across wfmgen.c / wfm_json.c / wfmcompose_py.c / compose.py to this one
source". Measured in 2026-08 (doppler#760), that was true of the **Python
binding only**. jm renders `[[enum]]` into `_enum_*` tables inside
`wfm_compose_ext.c`, which is its job. The C side had three homes and no gate:

| where                         | how many  | how                               |
| ----------------------------- | --------- | --------------------------------- |
| `just-makeit.toml` `[[enum]]` | 19 blocks | drives the generated binding      |
| `native/inc/wfm/wfm_names.h`  | 3 tables  | hand-written, shared by two files |
| `native/src/app/wfmgen.c`     | 12 tables | hand-written, file-local          |
| `native/src/wfm/wfm_json.c`   | 7 tables  | hand-written, file-local          |

Adding or reordering a waveform type meant editing three places, and getting
one wrong was silent. Two instances had already landed before the consolidation
went looking for them:

- **`wfm_writer`'s copy of `TYPE_NAMES` fell to 8 entries** with no `"dsss"`,
    behind a `< 7` label guard that also dropped `"symbols"`. That is what
    caused `wfm_names.h` to be created in the first place — for three tables,
    leaving nineteen elsewhere.
- **`wfm_json.c`'s `DATA_NAMES` was the reverse of `wfmgen.c`'s `DATA_SRC`.**
    One listed `{"none", "prbs"}`, the other `{"prbs", "none"}`, for the same
    user-facing `--data` choice. This was harmless *only* by accident: the JSON
    reader compared its lookup result to a literal (`== 0`) while wfmgen
    assigned it straight into the field. Either file gaining the other's idiom
    — the obvious, correct-looking refactor — would have inverted the meaning
    of `--data` in recorded scenes with nothing to catch it.

The second one is the argument for this page. It is not a bug that was found by
a test; it was found by a compiler complaining about a *redefinition* while the
tables were being merged. Nothing in the repository would have reported it, and
the reading that makes it look safe ("they are only used locally") is exactly
the reading that stops being true the next time somebody touches either file.

## The decision

Three roles, kept apart on purpose:

**The manifest declares the values.** A `[[enum]]` block in `just-makeit.toml`
is the single declaration of *which words exist and in what order*. It is
already the input jm renders the Python binding from, so making it also the
authority for C means the two faces cannot disagree — rather than adding a
fourth party to a three-way disagreement.

**The header is the one C home.** `native/inc/wfm/wfm_names.h` holds exactly
one `static const char *const` table per enum. Every C file that needs one
includes the header. A table declared anywhere else in hand-written C is an
error, not a style preference.

**A gate binds them.** `scripts/check_wfm_enum_tables.py`, wired as
`make lint-wfm-enum-tables`, holds the header to the manifest, to the C enums
that pin the indices, and to the "no second home" rule. Without it the previous
two paragraphs are a wish: they describe the state the tree happened to be in
on the day they were written, which is precisely what the gh-285 comment did.

### Why the header is not generated from the manifest

The obvious next step — have jm emit `wfm_names.h` from `[[enum]]`, so the
manifest is not merely the authority but the *only* copy — is deliberately
**not** taken yet, and this is the reason rather than an oversight.

Generating it is a jm feature that does not exist: jm renders `[[enum]]` into
bindings, and a C header for hand-written C to include is a different output
with different ownership questions (where it lands, whether it is committed,
what `jm status --check` says about it). Asking for that feature is only worth
doing from a position where doppler has **one** C home to point at, because the
request is then "generate this file" rather than "generate something to replace
these three files, whose contents disagree". Consolidating first also proves
the contents *can* agree — which, per `DATA_NAMES` above, was not a given.

So the sequence is: one home, gated (this page); then the upstream ask. The
gate is not wasted work if the header is later generated — a generated file
still needs its `[[enum]]` correspondence checked, and the "no second home"
rule is about hand-written C either way.

### Why the annotation lives beside the table

Each table in the header carries a machine-read annotation naming what owns it:

```text
/* SSOT: enum=wfm_type, count=N_TYPES */
static const char *const TYPE_NAMES[]
    = { "tone",  "noise", "pn",      "bpsk", "qpsk",
        "chirp", "bits",  "symbols", "dsss" };
#define N_TYPES 9

/* SSOT: enum=ftype, cenum=wfm_writer/wfm_writer_core.h:wfm_filetype_t */
static const char *const FTYPE_NAMES[] = { "raw", "csv", "blue", "sigmf" };
```

`enum=` names the `[[enum]]` block, `count=` an `N_*` macro that must equal the
table's length, `cenum=` a C enum whose Nth enumerator must have the value N.

The first draft of the gate kept this mapping as a dictionary inside the script.
That is a second list to maintain, keyed by table name, in a different file from
the thing it describes — the shape of the problem being solved, reintroduced one
level up. Putting it in the header means a table and its ownership are edited
together, and the gate derives its work list from the header rather than from a
list that can quietly stop matching it.

It is a plain `/* */` comment: doxygen reads only `/**` and `/*!`, so the
annotation never reaches the published C API docs. The gate requires the marker
to *open* its line, so prose that merely mentions `SSOT:` mid-sentence — as the
header's own banner does when it explains the convention — is not mistaken for
one. (That distinction is not hypothetical: the first run of the gate failed on
its own file banner.)

## What the gate checks

1. **One C home** — no hand-written file under `native/src` or `native/inc`
    may declare a string table whose contents equal one in `wfm_names.h`.
1. **The header matches the manifest** — element for element, in order.
1. **Fail-closed on a new table** — a table with no `SSOT:` annotation is an
    error. Adding one is exactly the moment to decide which `[[enum]]` owns it.
1. **The C enums agree** — where a table names a `cenum=`, that enum's Nth
    enumerator must be explicitly `= N`.
1. **The count macros match** the tables they size.

Plus one that is about the gate rather than the tree: a header in which the
gate finds **no** tables is a failure, not a pass. A matcher that stops
recognising the shape it scans for reports a clean tree, and this repository
has been bitten by that before — it is why `dispatcher_flags` in
`scripts/gen_wfmgen_flag_matrix.py` hard-fails on an empty discovery too.

Each rule was proven by sabotaging a **copy** of the tree and requiring the
gate to go red: a reordered table, a dropped entry (the `wfm_writer` rot,
replayed), an unannotated table, an annotation naming a nonexistent `[[enum]]`,
a drifted count macro, an enumerator whose value stops matching its index, a
re-declared table in `wfmgen.c`, a reordered `[[enum]]` on the manifest side,
and an emptied header. A gate that has only ever passed has not been tested.

## Scope, and what is deliberately outside it

**Three enums are declared in the manifest that no binding renders**
(`seed_advance`, `data_src`, `randomise`). They are C-only: their values reach
Python through other paths, so jm emits nothing for them. They are declared
anyway, because the alternative is a table whose ordering is authoritative and
written down nowhere the gate can reach — an enum with no declaration at all.
Ten of the nineteen pre-existing `[[enum]]` blocks are likewise unreferenced by
any binding, so this is the file's existing convention rather than a new one.

**`ELEMNAME[]` in `wfm_writer_core.c` is left alone.** Its five strings equal
the scalar half of `STYPE_NAMES`, but it is indexed by the *element-kind* enum
(`KIND[stype]`), not by `stype`, and it spells SigMF's vocabulary rather than
doppler's `--sample-type` names — the comment above it says so, and warns that
writing doppler's names into a SigMF sidecar would be a sidecar that lies about
its own data. Two orderings that presently coincide are not one table, and
coupling them would make a future divergence in SigMF's naming a change to
doppler's CLI. The gate's equality test does not fire on it because it is a
five-element table, not a copy of the ten-element one.

**jm's generated `_ext*.c` bindings are not scanned.** They contain `_enum_*`
tables rendered from the same `[[enum]]` blocks; gating them would report the
manifest's own output as a duplicate of the manifest.

**Non-waveform enums are out of scope.** `fc_source`, `fs_source`, `t0_source`,
`follow_end`, `interp_method`, `rc_pulse`, `sample_mode` and `wfm_stage_kind`
have `[[enum]]` blocks but no C name table; the header is the authority for
tables that exist, not a mandate to create one per enum.

## See also

- [Waveform Amplitude & Composition](wfmgen-composition.md) — the level and
    power conventions the same composer applies, and the `mix`/`concatenate`
    axes the scene schema exposes.
- [A Frame as a Description](frame-description.md) — `wfm_seq_kind` and
    `wfm_stage_kind`, two of the enums above, are the field and stage
    vocabularies that design introduces.
- [Capture Files](capture-files.md) — where `ftype`, `stype` and `endian` end
    up on disk.
