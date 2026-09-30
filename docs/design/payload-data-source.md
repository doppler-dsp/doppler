# The Payload as a Data Source

A frame's payload stops being one block of bits and becomes **a place bits
are drawn from**. The frame declares how many bits it carries, `data:LEN`,
and `--data` names where they come from: a file, a literal, stdin, or a
seeded generator. The shape was settled with the Field, in
[frame description §F.5](frame-description.md#f5-the-payload-is-a-data-source);
this page is the pass that builds it
([#1619](https://github.com/doppler-dsp/doppler/issues/1619), part of
[#853](https://github.com/doppler-dsp/doppler/issues/853)). It takes the
shape as given and adds what building it needs decided: the goals, the rules
the shape leaves open, what is deleted and where it lives today, the numbers
not yet known, and the order of work.

What the prototype measured is on the
[measurement record](payload-data-source-measurements.md), under the same
section numbers.

______________________________________________________________________

## 1. Use cases — who calls this, and what they do with the answer

1. **A finite burst, all data known a priori.** A test engineer has a file,
    an array or a message, and wants it sent once as a sequence of frames,
    so a receiver's output can be compared bit for bit against the input.
    Today the only way is to make the payload the whole file, which makes
    one frame of arbitrary length.
1. **An infinite stream.** A pipe or a live source (`sdr | wfmgen --data -`),
    chunked into frames for as long as it runs. Paced with `--realtime`, the
    transmitter must not stop when the input pauses: a receiver that loses
    carrier has to reacquire.
1. **A seeded stream for scoring.** Long-run bit and frame error rates need
    data that differs frame to frame (a repeated payload hides pattern
    dependent errors, and a receiver can lock to a period) but that a scorer
    can regenerate without a copy: `pn:0:23`.
1. **Code only.** Continuous DSSS with no data, `--data none`, unchanged.

**What callers do with the answer:** compare the received bits with the
source (use cases 1 and 3), or pass them on (2). Either way they need to
know which frames carried fill, how many fill bits the last frame carried,
and how many idle frames were sent. That is the truth the record carries
(§4.8).

## 2. Goals

- **One payload model.** Every payload is `data:LEN` in the frame plus one
    `--data` source. The six payload spellings and the cycle behind them go.
- **The frame stays a description.** Its layout, length and every stage's
    cover are known before a single data bit is read, so a receiver built
    in the know (§F.5) needs nothing from the stream.
- **Refuse, never pad silently.** A pad nobody declared is data nobody sent.
    A refusal happens **before the first sample** wherever the answer is
    knowable then (§4.4).
- **Deterministic under test.** Whether a frame is idle is decided by wall
    clock timing in production and by the test in a test. No test sleeps.
- **C first, once.** The data source is one C object. The CLI, the JSON
    scene and Python bind it; none of them chunks, pads or counts.
- **No regression for the cases that do not use a data source.** An output
    that today uses no payload is byte-identical afterwards.

## 3. The shape

Taken as given from
[§F.5](frame-description.md#f5-the-payload-is-a-data-source), and not
restated: `data:LEN`; the `--data` table (file, literal Field, `-`,
`pn:0:REG[:SEED]`, `none`); packed octets MSB first; the last partial chunk
padded from `--fill` or refused; idle frames on a paced underrun; cycling
deleted; `--repeat` repeats a finite burst and is refused over a stream; a
finite source replays byte for byte, a stream replays its description.

The frame's fourth common field flag is `--data-len`
([§R](frame-description.md#r-one-frame-representation)): the common layout
is `[preamble × reps | sync | data:LEN | crc]`.

## 4. The rules the shape leaves open

Each of these came up while prototyping the chunker (record §4) or reading
the code that is replaced. Each is a recommendation for review, not yet a
decision.

### 4.1 An idle frame is all fill

When a paced stream underruns while some bits are already buffered (a
partial chunk), the idle frame carries **only fill**, and the buffered bits
open the next data frame. A frame is therefore exactly one of *data*,
*data with a padded tail* (the last one only), or *idle*. A receiver
designed in the know can recognise an idle frame by comparing its payload
with the fill, and the number of fill bits in a data frame is never
anything but zero, except in the last.

### 4.2 A chunk need not be a whole number of octets

`LEN` is in bits and need not be a multiple of 8. Octets straddle frames,
MSB first, and the source keeps the leftover bits for the next chunk. The
prototype pins this with 24 bits in 10-bit frames (record §4).

### 4.3 An empty source is refused

A file or literal with zero bits would be a burst of no frames. It is
refused, naming the source, rather than producing an empty output that
reads as success.

### 4.4 When the refusal happens

| source                  | needs `--fill` when                           | checked                 |
| ----------------------- | --------------------------------------------- | ----------------------- |
| a file, a literal Field | its length is not a multiple of `LEN`         | before the first sample |
| `-` (stdin)             | **always**: its length is unknowable up front | before the first sample |
| `pn:0:REG[:SEED]`       | never: it neither ends nor pauses             | —                       |
| `none`                  | never: there is no payload                    | —                       |

The row for stdin is the one that differs from §F.5's wording. A stream's
remainder is known only when it ends, so a stream without a fill could only
be refused after it had already emitted frames, which is a run that fails
halfway. Requiring the fill up front turns that into a refusal before
anything is written. For a file, the length comes from `fstat`, so its
refusal also comes first and names the remainder in bits.

### 4.5 Only a paced stream has idle frames

Unpaced, a stream never underruns: the writer waits for input, as `cat`
does. Idle frames exist only under `--realtime`, where a frame is due at a
time and the source has nothing to give. The data source therefore reports
three outcomes per chunk, **data**, **nothing yet**, or **ended**, and the
paced loop alone turns *nothing yet* into an idle frame. A C test drives
*nothing yet* through a test source that reports it on chosen frames, so
idle behaviour is pinned without a clock.

### 4.6 How long a run is

| source            | the run is                | `--count`                                           |
| ----------------- | ------------------------- | --------------------------------------------------- |
| finite            | `ceil(bits / LEN)` frames | **refused** (today, DSSS burst silently ignores it) |
| `-`               | until the input ends      | optional upper bound                                |
| `pn:0:REG[:SEED]` | until `--count`           | **required**                                        |

A run with a data source ends **on a frame boundary**. Where `--count`
bounds it, the run is the smallest whole number of frames covering
`--count` samples, and the record states the frame count, so the length a
user sees in the record is never a frame cut short.

### 4.7 Repeats and coding state

- **`--repeats N`** (a segment played N times) over a finite source plays
    the burst from its first bit each time, which is today's rule for a
    segment instance: the signal is fixed and only noise and ranged draws
    change. Over a stream it is refused, like `--repeat`.
- **Coding state is per frame.** Each frame is assembled on its own, as the
    bridge does today (`dp_ccsds_tm_frame_ops(&ops, NULL)`, no encoder state
    carried). A stream of frames is a sequence of self-contained codewords.
    A convolutional register that runs *across* frames is a different
    waveform and stays out of scope (§8).

### 4.8 What the record and SigMF carry

- **The record** (`--record`, `dp_wfm_spec_to_json`) stores the data source
    as written. A literal is stored inline, as the payload is today. A file
    is stored as its path, its length in bits and a content hash, and a
    replay refuses a file whose hash differs. A stream stores its
    description (`-`, `pn:0:23:0x5`).
- **The truth for scoring**: frames sent, fill bits in the last frame, idle
    frames sent. These go in the record, and as `wfmgen:` annotations in
    the SigMF metadata beside today's `wfmgen:seed` and `wfmgen:data`.

### 4.9 Each face, one source

| face       | spelling                                                                                                           |
| ---------- | ------------------------------------------------------------------------------------------------------------------ |
| CLI        | `--data SRC --data-len LEN [--fill FIELD]`                                                                         |
| JSON scene | `"data"`, `"data_len"`, `"fill"`; `"data"` takes the same text as the CLI                                          |
| Python     | `Source(data=, data_len=, fill=)`: a bit array (a finite literal) or Field text; a file through `cvt.bytes_to_bin` |
| C          | the data source object (§7), built from the same text                                                              |

A `--data` value that parses as a Field (`0x…`, `[01]+`, `pn:…`) is a
Field; anything else is a path. A file whose name parses as a Field is
written with a directory (`./0101`). A stream from Python (a generator of
bytes) is not in this pass (§8).

## 5. What is deleted

Every site below was read at `adf362b9`.

| deleted                                                                         | where it lives today                                                                                                                                                              | replaced by                                           |
| ------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------- |
| payload **cycling**                                                             | `wfm_synth_core.h:222,227,275` (`% n_bits`); the latch at `wfm_synth_core.c:622-628`; the bridge's single assemble, `wfm_synth_bridge.c:316-330`; the help text at `wfmgen.c:354` | a frame pulled from the source at each frame boundary |
| `--bits FIELD`, JSON `"payload"`                                                | the surface row at `wfm_surface.h:273-280`                                                                                                                                        | `--data FIELD --data-len LEN`                         |
| `--bits-file PATH`                                                              | `wfmgen.c:641`, `bits_from_file` at `wfmgen.c:184` (reads the whole file)                                                                                                         | `--data PATH`                                         |
| `--data prbs\|none` (a two-way choice)                                          | `wfmgen.c:645-649`, `DATA_SRC_NAMES` at `wfm_names.h:91-97`                                                                                                                       | `--data pn:0:REG[:SEED]` or `none`                    |
| `wfm_source_t.dsss_code_only`                                                   | `wfm_compose.h:377-381`                                                                                                                                                           | the `none` source                                     |
| `wfm_source_t.payload` (a `wfm_seq_t`)                                          | `wfm_compose.h:217-228`; moving it off is [#1617](https://github.com/doppler-dsp/doppler/issues/1617)                                                                             | the source's data source                              |
| Python `Source(bits=)`, and `pattern=`/`payload=` **silently remapped** onto it | `wfm_compose.pyi:113,198`; `wfm_compose_ext.c:390-430`                                                                                                                            | `Source(data=)`; the old names refused                |
| `_SynthEngine.set_dsss_cont(data="prbs", payload=)`                             | `wfm_ext_wfm_synth.c:272-297`                                                                                                                                                     | a data source argument                                |
| the refusal of `data:LEN`                                                       | `wfm_frame.c:285-287`, pinned by `test_wfm_frame.c:591-594` and `test_field_bits.py:62,77`                                                                                        | the parser accepts it                                 |

`--bits-hex`, `--payload-gen` and `--payload-len` are already refused
(`wfmgen.c:694-699`), but their messages name `--bits` as the replacement.
They are repointed at `--data`, and `--bits`, `--bits-file`, `"payload"`,
`"pattern"` and `"payload_gen"` join them as refusals naming `--data`.
`--payload` never existed on the CLI; §F.3's list named it, and this
pass corrects it.

**Kept:** `--seed`, `--pn-length`, `--pn-poly` and `--lfsr` stay, because
`--type pn` uses them. Continuous DSSS stops reading them for its data and
reads `--data` instead. Its default becomes `pn:0:<pn-length>:<seed>`
spelled out, so a continuous DSSS output that names no `--data` is
byte-identical. Combining an explicit `--data` with those rows on a DSSS
source is refused as two spellings of one thing.

**What depends on the cycle, and moves with it:**

- **C tests:** the cycle assertion in `test_wfm_compose.c:2749-2757`, and
    `test_wfm_synth_core.c:162-177`.
- **Python tests:** `test_the_frame_cycles_to_fill_the_segment`,
    `test_bits_cycles_to_fill` and `test_pattern_cycles`. Each is rewritten
    to pin the new rule, not deleted.
- **Validation:** `native/validation/rx_frame_fer.c`, whose frame error
    rate runs a cycled frame and moves to `pn:0`.
- **The flag-matrix golden:** 40 of its 102 cases name a payload flag.
- **Docs and examples:** 38 lines use `--bits`, 10 use `--bits-file`, and
    six example scripts use `"payload"` or `Source(bits=)`.

## 6. Unknowns — stated before they are measured

- **The shortest frame period a paced stdin keeps up with.** A pipe read
    has latency, and at a high symbol rate a frame lasts microseconds. Below
    some frame period, input that is actually flowing would read as an
    underrun and produce spurious idle frames. That period is unmeasured.
    The prototype worked at 50 ms per frame, which says nothing about 50 µs.
- **The cost of assembling every frame.** Today one frame is assembled per
    segment instance. Now every frame is, including Reed-Solomon and the
    convolutional code for a CADU. Frames per second against the rate a
    real-time run needs is unmeasured, and it decides whether assembly has
    to run ahead of the pacer.
- **When SigMF metadata is written.** The idle count is known only at the
    end of a run. Whether `.sigmf-meta` is written at close or has to be
    rewritten there is unread.
- **The content hash.** Nothing in `native/` hashes content (the plan
    hash is a build-time hash of source code). Recording a file by its hash
    needs a new primitive (only CRC-16-CCITT exists, `dp_crc16.h`, too weak
    to identify a file), and which one is undecided.

## 7. The plan, in phases

**The object.** One C data source, in the `wfm` module:

- **built from:** the `--data` text (a Field, a path, `-` or `none`), plus
    `LEN` and an optional fill Field;
- **its state:** the bit residue (§4.2), the fill, the fill phase, and the
    running counts (frames, fill bits, idle frames);
- **its operation:** "give me the next chunk", answering data, nothing yet,
    or ended (§4.5), with the fill applied.

It composes, rather than re-implements: `dp_bytes_to_bin` for the unpack,
`dp_pn_generate` for `pn:0` (stateful, and already serializable), and the
Field parser for literal and fill text. Its state triplet makes a stream
resumable like every other stateful object. The frame is still assembled
by `dp_wfm_frame_assemble`, once per chunk.

**The order of work**, following
[the lifecycle](../dev/contributing/adding-algorithms.md):

1. **This page**, reviewed. The gate for this phase is the owner's review.
1. **The parser and the frame accept `data:LEN`**, with exactly one data
    field per frame and `LEN > 0` there. Red first: the tests that pin
    today's refusal flip.
1. **The data source object**, with a C test for each row of §4.4 and
    §4.6, and for §4.1 and §4.2, each proven red by sabotage.
1. **The pull replaces the cycle**: the synth takes the next frame at each
    frame boundary, and idle frames happen in the paced loop.
1. **The faces**: the surface rows for `--data`, `--data-len` and `--fill`
    through `gen_wfm_defaults.py`, the JSON keys, and `Source(data=)`, with
    the deletions and refusals of §5. This lands with or after #1617, which
    moves the payload off `wfm_source_t`.
1. **The record and SigMF** (§4.8), and replay (use case 1 byte for byte,
    use case 3 by its seed).
1. **Explore, once:** measure the two throughput unknowns of §6, and write
    fast tests at the points they find.
1. **Document:** the guide's payload section becomes the §F.5 table, and
    the flag-matrix golden and examples move to `--data`.

Steps 2 to 4 need nothing from #1617 and can land first. Step 5 cannot.

## 8. Deliberately not in scope

- **A convolutional register that runs across frames** (§4.7). It is a
    different waveform. If a link needs one, it is a coding decision on the
    description, not a length one.
- **A stream from Python.** Python passes a finite array or Field text. A
    Python generator of bytes as a live source is its own question, about
    the GIL and back-pressure, and is left until a caller needs it.
- **In-band signalling.** A length, a frame counter or an idle flag is a
    field at a known position, declared in the description (§F.5). The data
    source does not write one.
- **The receive side.** Receivers that consume these frames are
    [#1620](https://github.com/doppler-dsp/doppler/issues/1620).
