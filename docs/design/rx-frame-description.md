# A Receiver Built from a Frame Description

`BurstDemod` and `DsssBurstReceiver` will take the frame's description, the
same `wfm_frame_desc_t` the transmitter spreads, and derive from it
everything they are told by hand today: the sync word, how many symbols a
frame occupies, how long a burst is, and whether a frame passed its check.
This page is that pass through the lifecycle ([#1620][i1620], part E of
[#853][i853]).

It changes what a receiver is **told**, not what it decides. The receiver
still stops at decisions ([DsssBurstReceiver §10][s10]): `push()` returns
one bit per frame symbol and `llrs()` the matching soft values, and undoing
the frame stays `Frame.deframe()`. The model the description comes from is
[A Frame as a Description](frame-description.md); this page adds only its
receive side.

**Why a page of its own.** `frame-description.md` states the model and is
already 722 lines. This is a separate pass with its own inventory, deletions
and plan, the same shape as the payload pass beside it ([#1650][p1650]). The
model page's "not in scope: the receive side" entry links here.

______________________________________________________________________

## 1. Why

A receiver is told `frame_syms`, a number every caller computes by adding up
field lengths that the description already holds. The inventory (§5) counts
**21** such sums in 17 files, and 16 more literals in header examples. They
agree with the transmitter only because each caller wrote the same
arithmetic, and one did not: `bench_dsss_burst_receiver_core.c` passes
`PAYLOAD` (32) for a 61-symbol frame, so its benchmark times half a decode
and no burst ever passes its CRC ([#1669][i1669]).

The description is read from both ends everywhere else. The transmitter
spreads it (`dp_wfm_dsss_desc_chips`), and the scorer checks against it
(`dp_wfm_frame_check`, `Frame.deframe()`). The receiver is the one reader
handed a copy of its numbers instead. `DsssBurstReceiver` also restates the
frame's shape in C: `dsss_br_frame_valid` assumes `sync | payload | CRC-16`
(`native/src/dsss_burst_receiver/dsss_burst_receiver_core.c:30,42`).

## 2. Use cases — who builds a receiver from a description

No CLI or scene face builds a receiver: `wfmgen` transmits only, and nothing
under `native/src/app` references either receiver. The faces are C and
Python.

1. **A test, validator or benchmark** (C and Python) builds its stimulus and
    its receiver from one description, and compares the frame bits with the
    transmitted frame bit for bit.
1. **An example or guide reader** (Python) decodes a `wfmgen` capture. It
    builds a `Frame` from the Field texts it passed to `wfmgen`
    (`Frame(sync=field_bits("1111100110101"), …)`, Barker-13), then
    `deframe()`s each row and slices the payload at `field_off`.
1. **A C integrator receiving a CADU** builds the description with
    `dp_ccsds_tm_frame_describe()`, with no inner code (§3.4), and checks
    each row with `dp_wfm_frame_check()` and `dp_ccsds_tm_frame_ops()`.
1. **A stream receiver** (C and Python) receives frames whose payload is
    `data:LEN`: a data field of `LEN` bits with no bits of its own (§8). It
    compares each payload with the regenerated or recorded source.

What each of them does with `push()`'s rows is unchanged:
`frame[field_off(i):][:field_bits(i)]` after `deframe()` ([§10.3][s103]).

______________________________________________________________________

## 3. The shape

### 3.1 The constructors

Both receivers take the description **in place of** the sync word and
`frame_syms`. The spreading stays: the acquisition code, its repetitions and
the data code are how bits become chips, not the frame ([§R][r]), and a DSSS
description does not carry the preamble
(`native/inc/doppler/wfm/wfm_frame.h:848`).

```text
today                                      after
dp_burst_demod_create (data_code, len,     dp_burst_demod_create (data_code,
    spc, …, frame_syms, est_segments)          len, frame, spc, …)
  + dp_burst_demod_set_sync (d, sync, n)   (the sync word is the frame's)
dp_dsss_burst_receiver_create (acq, …,     dp_dsss_burst_receiver_create (
    sync, sync_len, reps, …, frame_syms,       acq, …, data, …, frame,
    …)                                         reps, …)
BurstDemod(data_code, …, frame_syms=)      BurstDemod(data_code, frame, …)
  + set_sync(sync)
DsssBurstReceiver(acq_code, data_code,     DsssBurstReceiver(acq_code,
    sync, …, frame_syms=)                      data_code, frame, …)
```

In C, `frame` is `const wfm_frame_desc_t *`. In Python it is a `Frame` or a
`FrameDesc` (`objects/frame.toml`), whose state already holds that
description (`dp_frame_state_t.d`, `native/inc/doppler/frame/frame_core.h`).

### 3.2 What is derived, and from what

Each value comes from a function that already exists, and the transmitter
reads the same one.

- **The sync word** is field 0's bits (§3.3). Today it is the `sync`
    argument, or `set_sync()`.
- **`frame_syms`** is `out_bits` from `dp_wfm_frame_desc_layout()`, one
    symbol per bit. Today it is the caller's sum.
- **The burst length** is
    `dp_wfm_dsss_desc_nchips (d, acq_len, reps, data_len) * spc` samples.
    Today it is restated at `dsss_burst_receiver_core.c:125`.
- **`frame_valid`** is `dp_wfm_frame_desc_crc_ok (d, bits) == 1`. Today it
    is a hand CRC-16 over an assumed layout.

`frame_syms` counts **from the sync word on**, sync included. That is what
the demodulator slices (`burst_demod_core.c:498` onward), and the header
example asserts `bits == frame`, sync first. The struct's own comment says
"AFTER the sync word" (`burst_demod_core.h:81`); that is wrong, and it goes
with the parameter.

### 3.3 Field 0 is the sync word

The demodulator correlates the symbols against the sync word to find the
frame and resolve the BPSK sign, and slices from there. With a description,
the sync word is **field 0**. That holds for both configurations that exist:
`dp_wfm_frame_fixed()` without a preamble puts `sync` first, and
`dp_ccsds_tm_frame_describe()` puts the marker at field 0 (its fields have no
names). The owner chose this over the field *named* `sync` (D1, §10).

Field 0 is refused, and the constructor returns `NULL`, when it:

- is empty, derived, or a data field: the receiver needs bits it knows;
- is covered by any stage: a stage that rewrites the sync word on the wire
    leaves the receiver correlating against bits nobody sent;
- is **named `preamble`**. `dp_wfm_frame_fixed()` with a preamble puts it at
    field 0 under that name, and a DSSS preamble is not a field of the
    spread frame. Refused by name, so that mistake is a `NULL` rather than
    a receiver that correlates against its own preamble.

### 3.4 What is refused

- **A description with an emitting stage** (a convolutional code). The code
    covers the sync word, so frame synchronisation would have to run after
    the Viterbi ([§10.4][s104]). It is refused, rather than accepted and
    never synced. A CADU without the inner code is accepted: the outer code
    and the randomiser do not cover its marker.
- **A description `dp_wfm_frame_desc_layout()` refuses.** Its reason is the
    reason; this object does not write a second one.

### 3.5 The receiver copies what it keeps

The receiver copies the description, with field 0's bits, at create. It
never borrows: it is fed across many `push()` calls, which is why it already
copies the codes (`dsss_burst_receiver_core.c:92`). A Python `Frame` may
therefore be dropped after the receiver is built.

______________________________________________________________________

## 4. Goals, and when this is done

- **No caller computes `frame_syms` by hand.** Once the parameter is gone
    this holds by construction: there is nothing to pass, and the compiler
    and the binding refuse a caller that tries.
- **The receiver and the transmitter are built from one description, in
    every example and test.** In C that is `dp_wfm_dsss_desc_chips()` and the
    receiver over one `wfm_frame_desc_t`. In Python it is a `Source(frame=)`
    built from the receiver's `Frame` (§8.3).
- **Byte-identical against the current receivers on the existing burst
    suite.** Same stimulus, same parameters, the old constructor against the
    new: identical frame bits, LLRs, symbols, events, read-backs and state
    blob.
- **One statement of the sync word.** `set_sync()` and field 0 could
    disagree, so `set_sync()` goes.

### 4.1 How byte-identity is measured

1. **An A/B C test, while both constructors exist** (plan step 2). Every
    fixture in `test_burst_demod_core.c` and `test_dsss_burst_receiver_core.c`
    runs through both constructors, and the outputs are compared with
    `memcmp`: the `demod()`/`push()` bits, `llrs()`, `symbols()`, `events()`,
    the `est_*` read-backs and `get_state()`. `make test` runs it.
1. **The certified reports.** `make validate-check` reruns each validator
    and compares it with the committed `results.md`. The reports under
    `src/doppler/dsss/tests/validation/burst_demod/` and
    `…/dsss_burst_receiver/` must pass unedited.
1. **The Python suite and every documented example**, through the targets
    below.

```sh
make test
make validate-check
make test-python PYTEST_ARGS='-k "burst or dsss_source"'
make test-examples-python
make test-examples-c
make test-snippets
make test-stubs
```

The stimulus moves separately (plan step 4). A C test's hand-built burst
becomes `dp_wfm_dsss_desc_chips()` over the same description, and that PR
asserts once that the two produce the same chips before it deletes the hand
builder. The receiver's identity and the stimulus's identity are then two
checks, not one entangled one.

______________________________________________________________________

## 5. Inventory

Measured at `26b57b8c` (`origin/main`) with the commands shown.
`docs/c-api/**` is generated and excluded, as are `CHANGELOG.md` and
`changelog.d/`.

### 5.1 Hand-computed frame lengths — 21 lines in 17 files

```sh
git grep -nE '(FRAME_SYMS|\bFRAME\b|frame_syms)\s*(=|\().*\+' \
  -- ':!docs/c-api' ':!CHANGELOG.md' ':!changelog.d'
```

- **C tests, benches and the demo, 4:** `test_burst_demod_core.c:18`,
    `test_dsss_burst_receiver_core.c:30`, `bench_burst_demod_core.c:47`,
    `native/examples/dsss_burst_receiver_demo.c:74`.
- **Python tests, 5:** `test_burst_demod.py:25`,
    `test_dsss_burst_receiver.py:26,357`, `wfm/tests/test_dsss_source.py:38`,
    `characterization/dsss_burst_receiver/characterize.py:229`.
- **Python validators, 4:** `validation/burst_demod/validate.py:147,332,542`,
    `validation/dsss_burst_receiver/validate.py:114`.
- **Python benches, 1:** `dsss/benchmarks/_burst_stimulus.py:55`.
- **Python examples, 3:** `dsss_burst_pipeline_demo.py:535`,
    `dsss_burst_receiver_demo.py:110`, `dsss_realtime_file_demod.py:69`.
- **Docs, 4:** `docs/api/python-dsss.md:91`,
    `docs/guide/wfmgen/waveforms.md:637`,
    `docs/design/dsss-burst-receiver.md:815,1028`.

`test_dsss_burst_receiver.py:358` then adds the outer code's check symbols
by hand, `32 * 8 * rs_depth`: a second derivation of what
`dp_wfm_frame_desc_layout()` already sums.

The grep finds sums, not every hand-stated length. Passed positionally,
`bench_dsss_burst_receiver_core.c:179` states `PAYLOAD` where the frame is 61
symbols (#1669), and no grep for a sum sees it. Once the parameter is
deleted, neither form can exist.

### 5.2 Literal lengths in header examples — 16

```sh
git grep -nE 'frame_syms=([0-9]+|len\()' -- native/inc
```

There are 14 Python `@code` examples: 8 in `burst_demod_core.h`
(`frame_syms=93`, once `len(frame)`) and 6 in `dsss_burst_receiver_core.h`
(`frame_syms=32`). Two C `@code` blocks pass a positional literal:
`burst_demod_core.h:35` (`256`) and `dsss_burst_receiver_core.h:17` (`61`).
Each becomes a `Frame` in the rewritten example, and `make test-stubs` runs
the Python ones.

### 5.3 Constructor calls that pass raw codes and a length — 66

```sh
git grep -nE '\b(BurstDemod|DsssBurstReceiver)\(' \
  -- ':!docs/c-api' ':!native' ':!CHANGELOG.md' ':!changelog.d' ':!*.pyi'
git grep -nE '(BurstDemod|DsssBurstReceiver)\(' -- native/inc
git grep -nE 'dp_(burst_demod|dsss_burst_receiver)_create *\(' \
  -- native/tests native/benchmarks native/examples native/validation
```

- **Python outside the headers, 28 in 15 files:** 24 in tests, benches,
    validators and examples, and 4 in docs.
- **Python header `@code`, 14 in 2 files.**
- **C, 24 in 5 files:** `test_burst_demod_core.c` 16,
    `test_dsss_burst_receiver_core.c` 5, the two benches 1 each, the demo 1.

There is one more, internal call: `DsssBurstReceiver` composing `BurstDemod`
(`dsss_burst_receiver_core.c:111`). `native/validation/` has no caller.
`set_sync()` is called 23 times in 10 files outside the object itself.

### 5.4 Frames built by hand on the transmit or scoring side — 27

```sh
git grep -lE 'BurstDemod|DsssBurstReceiver' -- '*.py' \
  | xargs grep -cE 'crc16\('
git grep -cE 'dp_crc16_ccitt *\(' -- native/tests/test_burst_demod_core.c \
  native/tests/test_dsss_burst_receiver_core.c \
  native/benchmarks/bench_burst_demod_core.c \
  native/benchmarks/bench_dsss_burst_receiver_core.c
```

**16** `crc16(` calls in 8 Python files and **11** `dp_crc16_ccitt` calls in
4 C files build or check a `sync | payload | CRC-16` frame by hand. These
are the sites the "one description" goal moves onto `Frame.bits()`,
`crc_ok()` and `deframe()` in Python, and onto `dp_wfm_dsss_desc_chips()` in
C.

______________________________________________________________________

## 6. What is deleted

- **The `frame_syms` constructor parameter** on both receivers
    (`burst_demod_core.h:230`, `dsss_burst_receiver_core.h:221`, both
    `objects/*.toml`), replaced by the layout's `out_bits`.
- **`sync`/`sync_len` on `DsssBurstReceiver`, and `BurstDemod.set_sync()`**
    (`dsss_burst_receiver_core.h:221`, `objects/burst_demod.toml`), replaced
    by field 0.
- **`DSSS_BR_CRC_BITS` and the hand CRC in `dsss_br_frame_valid`**
    (`dsss_burst_receiver_core.c:30,42-56`), replaced by
    `dp_wfm_frame_desc_crc_ok()`.
- **The restated burst length** (`dsss_burst_receiver_core.c:125`),
    replaced by `dp_wfm_dsss_desc_nchips()`.
- **`frame_bits` beside `frame_syms`** on the receiver's state: two fields
    holding one value (set at `dsss_burst_receiver_core.c:87,123`). One
    field stays.
- **`burst_demod_core.c`'s unused `ccsds_tm_frame.h` include and
    `BURST_DEMOD_CRC_BITS`** (`burst_demod_core.c:3,12`).
- **`ccsds_tm`, `conv` and `rs` in both receivers' `depends_on`**, and the
    comment in `objects/burst_demod.toml:24-28` claiming the demodulator
    uses them (D4).
- **The 21 sums, 16 literals and 66 call sites of §5, and the 27
    hand-built frames of §5.4**, each replaced by one description per
    caller.

Neither core calls a coding, framing or sequence function today. This
returns nothing:

```sh
git grep -nE '\bdp_(rs|conv|ccsds_tm|wfm|pn|gold)_[a-z_]+ *\(' \
  -- native/src/burst_demod native/src/dsss_burst_receiver
```

After this pass they call `dp_wfm_` functions, so `wfm_frame` stays, with the
`pn`, `gold` and `cvt` it needs, and the three coding components go.

**Kept:** `frame_syms` as a **read-only** property on both. A caller still
sizes a buffer by it and strides `llrs()` by it. It is reported, no longer
told.

## 7. `ccsds_tm_frame_cfg_t` is kept

#1620 asks for it to be re-evaluated, and the receive side gives no reason
to delete it.

- **Neither receiver touches it.** `burst_demod_core.c` includes its header
    and uses nothing from it; the include goes (§6).
- **It is what a CADU's description is made from.**
    `dp_ccsds_tm_frame_describe (cfg, frame_len, bits, &d)` turns its four
    knobs into a `wfm_frame_desc_t`, and that is how a C caller builds a
    receiver for a CADU (§2). The receiver consumes the cfg's output, not
    the cfg.
- **It configures the codec's own kernels.** `dp_ccsds_tm_frame_layout`,
    `_encode` and `_decode` read it directly, and the general assembler's
    output is checked against `_encode` byte for byte (the model's
    falsification target 2). It has users in 6 files outside `ccsds_tm`,
    none of them a receiver:

```sh
git grep -l ccsds_tm_frame_cfg_t -- native \
  ':!native/inc/doppler/ccsds_tm' ':!native/src/ccsds_tm'
```

Whether `_encode`/`_decode` should stay as a second implementation of a
chain the general assembler also implements is a question about `ccsds_tm`,
not about receivers. This pass does not decide it.

______________________________________________________________________

## 8. Interaction with the payload pass (F, #1619)

F makes the payload `data:LEN`: a field with a length and no bits of its
own, filled per frame from a data source ([#1650][p1650], the design;
[#1660][p1660], step 2, the field kind).

### 8.1 The receiver needs a data field's length and nothing else

The receiver reads lengths from `dp_wfm_frame_desc_layout()`, and #1660
makes a data field lay out at its length, so a CRC covers it. `frame_syms`,
the burst length and the CRC's position therefore come from a `data:LEN`
description exactly as from a literal one, without a data bit. A generated
payload (`pn:LEN:REG`) carries its length in `wfm_seq_t.len` and lays out
the same way. The receiver never renders a payload, so it never needs the
source, the fill or the seed.

- **`data:LEN*REPS`** is one draw sent `REPS` times, and lays out as
    `LEN * REPS` bits, like any repeated field.
- **An idle or padded frame** has the same length, by F's rule that every
    frame keeps one length. The receiver returns it like any other frame;
    recognising fill is the caller's job, since the caller knows the fill.
- **A data field is never the sync word** (§3.3). #1660 already refuses
    `data:LEN` outside the payload slot on every transmit face.

### 8.2 The Python door for a data field

`FrameDesc.add_field(name, bits)` takes bits only ([§F.3][f3]), and
`field_bits("data:1024")` raises by design (#1660). The door for a
`data:LEN` payload is `FrameDesc.add_data(name, n)`, over the C
`dp_frame_add_data`, which appends the `WFM_SEQ_DATA` field the Field parser
builds from `data:n` (D5, [#1786][i1786]). A `Source(frame=)` holding it
renders the same samples as a scene's `"frame"` key or `wfmgen --frame` with
that text, byte for byte, over a multi-frame data source
(`test_frame_source.py`; `test_frame_core.c` holds the two fields equal).

The receive half is not built. `deframe()` and `check()` read the layout
that `build()` writes, and `build()` refuses a description with a data
field, because it materialises one frame and a data field has no bits of its
own. The existing receiver test still works around it with zeros,
"geometry, when the bits arrive later"
(`src/doppler/dsss/tests/test_dsss_burst_receiver.py:397`); plan step 6
replaces it. What `build()` and `bits()` mean for a data description is an
open question, [#1789][i1789].

### 8.3 The transmit side in Python takes the description

`Source` takes `frame=`, a `FrameDesc` or a `Frame`
([#1703](https://github.com/doppler-dsp/doppler/pull/1703), the first half
of [#1617][i1617]), and `data=` with `data_len=`, a payload drawn from a
data source (F step 6,
[#1721](https://github.com/doppler-dsp/doppler/pull/1721)). So a Python test
builds the transmitter from the receiver's `Frame` directly. A `data:LEN`
payload renders through `data=` on the common frame (`[preamble | data]`), or
through a `frame=` description carrying a `data:LEN` field from
`FrameDesc.add_data` (§8.2), which is also how a sync word and a CRC are
given: a source carries neither (#1617). The Python half of goal 2 waits on moving the
existing tests and examples (step 7), not on #1617, whose
remainder moves `sync` and `crc` off `wfm_source_t`.

### 8.4 Dependencies

- Plan steps 2 to 5 need nothing from F.
- Step 6 builds on #1660 (`WFM_SEQ_DATA`), which is merged.
- Step 7 builds on `Source(frame=)` and F step 6 (`Source(data=)`), both
    built, and on step 6 for a `data:LEN` description.

______________________________________________________________________

## 9. Unknowns, stated before they are measured

- **U1. Whether jm binds a cross-module `FrameDesc` argument to a
    `const wfm_frame_desc_t *`.** An init param `object = "<comp>"` resolves
    to a capsule the producing component publishes (`resolve_object_ref` in
    jm's `_config.py`, gh-1224; `capsule_type`, gh-1235). The frame is in the
    `wfm` module and the receivers in `dsss`. `objects/frame.toml` publishes
    no capsule today. Whether one can reach a member
    (`expr = "&self->handle->d"`) and be accepted from a `FrameDesc` view
    instance is not measured. It is measured on a fresh jm scaffold, not in
    this tree. **Fallback:** the capsule carries the default
    `dp_frame_state_t *`, and each receiver gains a one-line C create
    function that passes `&f->d` to the C constructor. The C face still takes
    the description, and no binding logic is written.
- **U2. Whether an unbuilt `FrameDesc` is safe to pass.** The receiver lays
    the description out itself, so `build()` should not matter.
    **Fallback:** refuse an unbuilt description, naming `build()`.
- **U3. Whether the state blob moves.** The blob packs running state; the
    sync word and the length are configuration that `create()` restores, so
    it should not change. The A/B test compares `get_state()` bytes.
    **Fallback:** if a field moves, bump
    `DSSS_BURST_RECEIVER_STATE_VERSION` (7 today), and claim byte-identity
    for the outputs, not the blob.
- **U4. Whether any caller relies on `frame_syms` differing from the
    frame.** The `DsssBurstReceiver` header examples pass `frame_syms=32`
    against a 13-bit sync with no frame behind it, and a refusal test passes
    `PAYLOAD` (`test_burst_demod_core.c:209`). Found during migration.
    **Fallback:** such a caller describes the frame it actually slices.

## 10. Decided

The owner decided all five in review of
[#1673](https://github.com/doppler-dsp/doppler/pull/1673) on 2026-09-30,
each as recommended, with one addition to D1. The trade each one accepts is
kept beside it.

- **D1. The sync word is field 0.** It holds for both configurations that
    exist, and CCSDS's fields have no names. **Addition:** a field 0 named
    `preamble` is refused by name (§3.3). *Trade:* a leading field of any
    other name is taken as the sync word, so a mislabelled description is
    a caller error. *Not chosen:* the field named `sync`, which would also
    refuse a CADU, whose field 0 has no name.
- **D2. One constructor, taking the description.**
    `Frame(sync=field_bits(…), payload=…, crc="crc16")` is the bit-array
    door #1620 names, so a second flavour taking `sync=` and `frame_syms=`
    would be two spellings of one receiver. *Trade:* every caller of §5.3
    changes, and it ships as a breaking change with a changelog entry.
- **D3. A description with no CRC is not valid**, and the capture releases
    the window. That is today's behaviour (`dsss_br_frame_valid` returns 0
    below `sync_len + 16` bits), so it is byte-identical. *Trade:* an
    unprotected link never claims a span, so a decoy after it is not
    suppressed either. *Not chosen:* valid on demodulation, the rule before
    #1181.
- **D4. The verdict reads the CRC only**, through
    `dp_wfm_frame_desc_crc_ok()` in `wfm_frame`, so `ccsds_tm`, `conv` and
    `rs` leave the link lines. *Trade:* a frame with an outer code and no
    CRC is judged not valid (D3), though R-S could say it decoded. *Not
    chosen:* `dp_wfm_frame_check()` with the CCSDS ops, which would put a
    code's verdict back in the receiver, where [§10][s10] took it out.
- **D5. The Python door for `data:LEN` is `FrameDesc.add_data(name, n)`.**
    *Trade:* one more method on the frame object. *Not chosen:* Field text
    in `add_field`, which reopens the "an object takes bits" rule of
    [§F.3][f3].

______________________________________________________________________

## 11. Plan — one PR per step

1. **This page.** Gate: the owner's review.
1. **C: the description constructor, beside the old one**, on both
    receivers, with the derivations of §3.2 and the refusals of §3.3 and
    §3.4. It carries the A/B test of §4.1 over every fixture, and a C test
    per refusal, the field 0 named `preamble` among them. Each is proven by
    sabotage: `out_bits` off by one, a burst length off by a chip, the CRC
    check skipped, the `preamble` refusal removed. Gate: `make test`.
    (#1669, the benchmark's wrong length, is fixed on its own first.)
1. **Python: the frame publishes a capsule, and the receivers take
    `object = "frame"`.** U1 is measured first, on a fresh scaffold. Gate:
    `make drift-check`, `make test-stubs`, and the Python A/B over the same
    fixtures.
1. **Callers move to one description.** C tests, benches and the demo build
    their bursts with `dp_wfm_dsss_desc_chips()`, asserting once that the
    chips match the hand builder, which is then deleted. Python tests,
    validators, benches, examples and doc fences build a `Frame`. Gate: the
    targets of §4.1.
1. **Delete the old constructors**, `set_sync()`, the hand CRC, the dead
    include, the three link-line entries and the duplicate field, and
    rewrite the header examples on a `Frame`. The A/B test goes with the old
    constructor; from here the validators' reports carry byte-identity.
    Gate: `make drift-check`, `make test-stubs`, `make validate-check`,
    `make lint`.
1. **`data:LEN` on the receive side**, after #1660: `dp_frame_add_data` and
    `FrameDesc.add_data` (D5), and a receiver test over a `data:LEN`
    description whose frames are compared with the source. Gate: `make test`,
    `make test-python`.
1. **The Python transmit side from the description**, over
    `Source(frame=)` and `Source(data=)`, which exist: the remaining Python
    tests and examples build `Source(frame=)`
    from the receiver's `Frame`. Gate: the targets of §4.1.

Each code PR carries a `changelog.d/` fragment and `Refs #1620`; step 7
closes it.

[f3]: frame-description.md#f3-every-face-takes-a-field-in-the-form-natural-to-it
[i1617]: https://github.com/doppler-dsp/doppler/issues/1617
[i1620]: https://github.com/doppler-dsp/doppler/issues/1620
[i1669]: https://github.com/doppler-dsp/doppler/issues/1669
[i1786]: https://github.com/doppler-dsp/doppler/issues/1786
[i1789]: https://github.com/doppler-dsp/doppler/issues/1789
[i853]: https://github.com/doppler-dsp/doppler/issues/853
[p1650]: https://github.com/doppler-dsp/doppler/pull/1650
[p1660]: https://github.com/doppler-dsp/doppler/pull/1660
[r]: frame-description.md#r-one-frame-representation
[s10]: dsss-burst-receiver.md#10-the-receiver-stops-at-decisions-2026-08-27
[s103]: dsss-burst-receiver.md#103-what-the-split-cost-and-what-it-bought
[s104]: dsss-burst-receiver.md#104-the-inner-code-still
