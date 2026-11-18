---
description: "A general OFDM modulator as a library object: a layout of symbol classes and runs in, samples out; the struct and what forced each field, its state, cost and proofs, and its standalone surface"
---

# OFDM: a general modulator

!!! note "Status"

    The modulator on this page is built (`ofdm_core.h`, with its layout, its
    state and its serialization); `wlan` is its first caller. Not built:
    wfmgen's `ofdm` waveform type, the complex-envelope PAPR and the EVM of
    an OFDM grid. "What exists, and what does not" says which is which. The
    decisions are at the end, the ruled ones and the open ones.

The library gains an OFDM modulator: an object that turns constellation
points into the baseband samples of an OFDM signal, for any OFDM signal
whose structure a **layout** can describe. It names no standard and has no
dependency on [wfmgen](wfmgen.md) or on any receiver: a caller with
constellation points and a layout has samples.

The IEEE 802.11 OFDM PHYs, wfmgen's `ofdm` waveform type, and every other
consumer are described on their own pages, as callers of this one. The first
is [wlan](wlan.md).

## Vocabulary

Each term is defined once here and used in this sense on this page and on
[wlan](wlan.md).

- **subcarrier**: one frequency bin of the transform, numbered `k` from
    `-n_fft/2` to `n_fft/2 - 1` with 0 at the carrier. Never "tone" or
    "bin" on these pages; a standard's "26-tone RU" is its name, not a term.
- **subcarrier spacing**: the sample rate divided by `n_fft`.
- **OFDM symbol**: one inverse transform's output with its cyclic prefix.
    The bare word "symbol" is not used.
- **constellation point**: one complex value carried by one data
    subcarrier of one OFDM symbol. Never "symbol".
- **grid**: the constellation points and fixed values of one OFDM symbol,
    one per occupied subcarrier, before the inverse transform.
- **cyclic prefix**: the last samples of an OFDM symbol's transform output,
    copied ahead of it, counted in samples. A standard may call it a guard
    interval and state it as a duration; they are the same samples.
- **window**: the amplitude taper over the transition between two
    consecutive OFDM symbols, `window` samples long.
- **subcarrier map**: which subcarriers may carry a constellation point
    (**data**) and which carry nothing the caller supplies (**null**: the
    DC subcarrier, the band edges, any gap). Pilots are not in the map.
    They are fixed entries (next item), because where a pilot sits and what
    it carries both change from one OFDM symbol to the next in some signals,
    so a pilot is a statement about a phase of the pattern and not about a
    subcarrier. A subcarrier that holds a fixed entry in some phase and is
    in the map as data displaces data in that phase.
- **fixed entry**: a subcarrier, a pattern phase and a complex value the
    modulator places itself: a pilot, a training value, a boosted pilot.
- **pattern phase**: the position of an OFDM symbol within the period of its
    class's fixed entries.
- **polarity sequence**: an optional per-class sequence of complex scalars;
    OFDM symbol `n` multiplies its fixed entries (never its data) by
    element `(n + polarity_offset) mod n_polarity`.
- **class**: one kind of OFDM symbol: a transform length, a subcarrier map,
    a fixed pattern, a scale, a cyclic prefix and a window.
- **run**: `count` consecutive OFDM symbols of one class.
- **layout**: a list of classes and a list of runs. It says what is
    transmitted in what order; the caller supplies the constellation points.
    It is the one description the modulator takes.
- **transform plan**: the FFT object's own precomputed tables for one
    length. It has nothing to do with a layout, and nothing here to do with
    wfmgen's `Plan`, the cached rendering of a scene (`doppler.wfm.Plan`).
    "Plan" on these pages always means the FFT's, and only where it must.

## 1. Who calls it

| who                           | with what                                     | what they do with the answer                               |
| ----------------------------- | --------------------------------------------- | ---------------------------------------------------------- |
| A C application               | a layout and constellation points             | renders OFDM samples into its own buffer, no Python        |
| A Python script               | `doppler.ofdm`: an `Ofdm` object and an array | renders a signal of any OFDM structure, no wfmgen running  |
| A standard's preset           | a layout it built (802.11: [wlan](wlan.md))   | renders that standard's PPDUs through the same modulator   |
| wfmgen                        | a layout, as an `ofdm` source's description   | a waveform type, summed with other sources in a scene      |
| A future OFDM receiver's test | a layout declared once                        | scores demodulation against truth the generator also holds |

The last row is the reason the generator comes first: the library has no OFDM
receiver, and a receiver is measured against a generator that is already
proven.

## 2. Design goals

1. **The primitive is general.** The modulator knows nothing of any
    standard. A preset is a configuration of it, the way `dp_CCSDS_TM_CONV`
    configures `conv_code_t`; and no field of the layout exists for one
    standard alone.
1. **One shape, from the first slice.** The public API is the layout; no
    narrower API is shipped first and widened later. The shape was checked
    against the signals in section 4's table so it need not change when
    they arrive.
1. **Standalone.** The C object and the Python module work with no wfmgen in
    the process: nothing in `ofdm` includes a wfmgen header or names a
    waveform type.
1. **Nothing is implemented twice.** The inverse transform is `fft`, the
    constellation points come from `mpsk` and `qam`, and a mapper is not
    part of the modulator.
1. **0 dBFS is unit average power**, as for every wfmgen source, where the
    layout's `scale` is set to give it; the modulator itself applies exactly
    the `scale` the class states and no other normalization.
1. **Four faces, one engine**: C, the CLI, Python and the JSON scene render
    byte-identically, wherever a face exists. The modulator's own faces are
    C and Python (section 5); the CLI and JSON faces belong to wfmgen's
    type.
1. **MIMO is out of scope, not deferred.** One output stream. A spatial
    stream is another layout rendered separately, not a field here. Ruled
    2026-11-16.

## 3. What exists, and what does not

| piece                               | state                                          | where                                          |
| ----------------------------------- | ---------------------------------------------- | ---------------------------------------------- |
| inverse transform, any length       | exists                                         | `fft`: `dp_fft_create`, `dp_fft_execute_cf32`  |
| scrambler sequence                  | exists                                         | `pn`                                           |
| convolutional code, with puncturing | built (#3113); the pattern is in `conv_code_t` | `conv`, `conv_enc`, `viterbi`                  |
| interleaver, first permutation      | exists                                         | `dp_interleave_index`                          |
| BPSK and QPSK map                   | exists                                         | `mpsk`: `dp_mpsk_map`                          |
| 16- to 1024-point QAM map           | built (#3115)                                  | `mpsk` module: `qam_map`, `qam_demap`          |
| parametric CRC                      | built (#3116)                                  | the parametric CRC header, beside `dp_crc16.h` |
| the modulator and its layout        | exists                                         | `ofdm_core.h`: this page                       |
| a complex-envelope PAPR             | absent                                         | `dp_tonemeas_time_stats` is real-valued only   |
| EVM of an OFDM grid                 | absent                                         | a measurement, with the modulator's report     |

What the built pieces are, as this page uses them:

- **Puncturing** is a pair of fields of `conv_code_t`, `punct_period` and
    `punct_mask`, so the encoder and the decoder read one description. It is
    a constructor argument of `ConvEncoder` and `Viterbi` and a reject key of
    their state blobs, not a setter. The decoder inserts a 0.0 log-likelihood
    ratio at each deleted position.
- **QAM** is the square, Gray-coded map for M = 4 to 1024 points, unit
    average energy, labels packed least significant bit first in
    transmission order. It is the library's own mapping, consistent with
    `mpsk` (M = 4 equals its QPSK; the sign bit has `mpsk`'s polarity). **No
    standard's table is claimed.** Whether a standard's table is this map,
    or needs a relabeling, is what that standard's page pins from its text.
- **The CRC** is the Rocksoft parametric model: width, polynomial, initial
    value, reflected output and final XOR as parameters, with named
    catalogue instances (CRC-32/ISO-HDLC, CRC-8/SMBUS and CRC-16/IBM-3740
    (which `dp_crc16_ccitt` now is).

## 4. The layout

A layout is a list of **classes** and a list of **runs**. The structs below
are the proposed C declaration.

<!-- docs-snippet: skip=proposed struct layout for review, not a compilable program -->

```c
typedef struct {            /* one entry of a class's fixed pattern       */
  uint32_t phase;           /* pattern phase, or OFDM_ALL_PHASES          */
  int32_t  k;               /* subcarrier, -n_fft/2 .. n_fft/2 - 1        */
  float _Complex value;     /* a pilot, a training value, a boosted pilot */
} ofdm_fixed_t;

typedef struct {            /* one kind of OFDM symbol                    */
  uint32_t n_fft;           /* transform length OF THIS CLASS             */
  uint32_t useful;          /* body samples read cyclically from the      */
                            /* inverse transform; 0 means n_fft           */
  uint32_t cyclic_prefix;   /* default cyclic prefix, samples             */
  uint32_t window;          /* window transition, samples                 */
  double   scale;           /* multiplies the transform output            */
  const uint8_t *subcarrier_map;       /* n_fft entries, OFDM_DATA or     */
                                       /* OFDM_NULL, index k + n_fft/2    */
  const float _Complex *subcarrier_gain; /* n_fft complex multipliers on  */
                                       /* the grid, or NULL for all ones  */
  uint32_t n_phase;         /* period of the fixed pattern, in OFDM symbols */
  const ofdm_fixed_t *fixed;           /* sorted by (phase, k)            */
  uint32_t n_fixed;
  const float _Complex *polarity;      /* per-OFDM-symbol scalar on the   */
  uint32_t n_polarity;                 /* fixed entries; NULL for none    */
} ofdm_class_t;

typedef struct {            /* a run of OFDM symbols of one class         */
  uint32_t class_index;     /* index into the layout's classes            */
  uint32_t count;
  uint32_t cyclic_prefix;   /* per-run override, or OFDM_CP_DEFAULT       */
  uint32_t polarity_offset; /* offset into `polarity` for the run's first */
  uint32_t first_phase;     /* pattern phase of the run's first OFDM sym  */
} ofdm_run_t;
```

`scale` multiplies the transform output. A signal whose standard normalizes
a field by the square root of its occupied subcarriers sets it to that; the
standard's own symbol for the factor is read from its text, on that
standard's page. `subcarrier_gain` multiplies the grid per subcarrier and is
how a per-subband phase rotation or amplitude is expressed.

Per OFDM symbol the modulator takes `n_data(phase)` constellation points,
places them on the data subcarriers the pattern leaves free, sets the fixed
entries of that phase (times `polarity`), multiplies by `subcarrier_gain`,
inverse transforms, reads `useful` samples cyclically from the result,
prepends the last `cyclic_prefix` of them, and overlap-adds the window onto
the previous OFDM symbol's tail. Nothing else in the modulator knows what a
frame, a slot or a PPDU is.

**The subcarrier map, and what the vocabulary says of it.** The vocabulary
defines the map as data and null, and the struct agrees: pilots are fixed
entries, not map values, so a pilot has one home and not two. A pilot is not a
property of a subcarrier (its position moves with the OFDM symbol in LTE,
and its value with the symbol in every standard here), so it lives in the
only structure indexed by phase.

**Every field is checked against its flavor.** A cell reads "fits" with what
fills it, or names the generalization the cell forced into the struct. The
standards' numbers are recalled, not read from any text; the HE, LTE and
DVB-T columns are stand-ins with the right occupancy.

| shape parameter                    | non-HT 20/10/5                                                                     | HT 20, HT 40                                     | VHT 20/40/80/160                       | HE SU                                                         | LTE/NR (outsider)                                              | DVB-T (outsider)                               |
| ---------------------------------- | ---------------------------------------------------------------------------------- | ------------------------------------------------ | -------------------------------------- | ------------------------------------------------------------- | -------------------------------------------------------------- | ---------------------------------------------- |
| `n_fft`, per class                 | 64                                                                                 | 64, 128                                          | 64, 128, 256, 512                      | legacy 64 and HE 256 in one PPDU: **per class**, not per PPDU | 128 to 2048 (NR to 4096 and up)                                | 2048, 8192                                     |
| spacing                            | derived, `fs/n_fft`: 312.5 kHz at 20 MHz                                           | 312.5 kHz                                        | 312.5 kHz                              | 312.5 kHz legacy, 78.125 kHz HE                               | 15 kHz (NR `15 * 2^mu`)                                        | derived, about 4.46 kHz (2k)                   |
| subcarrier map: data, null         | 48 of 52 used, DC null                                                             | 52 of 56; 108 of 114, -1..1 null                 | 52; 108; 234; 468 data                 | 234 data at 20 MHz (RU242)                                    | 1200 used per 20 MHz, DC null                                  | all 1705 used, DC is a carrier                 |
| fixed entries: positions           | +-7, +-21                                                                          | +-7, +-21; +-11, +-25, +-53                      | per width; 160 is two 80 MHz copies    | stand-in positions                                            | `6m + (v + shift) mod 6`, moves with symbol and cell           | continual list + `3 (l mod 4) + 12 p`          |
| `fixed` as a table over (phase, k) | one phase                                                                          | period 4 and 6                                   | period 4, 6, 8                         | period 8                                                      | period 14, 200 per CRS symbol                                  | period 4 plus all-phase continual              |
| fixed value vs OFDM symbol         | base times `polarity[n + 1]`, no rotation                                          | base **rotated** by `n`, times `polarity[n + 3]` | rotated, period 8 at 80 MHz            | rotated, offset by field                                      | Gold-sequence QPSK per (slot, symbol, `m`): **not a rotation** | PRBS by carrier, 4/3 boost, **not a rotation** |
| cyclic prefix, per OFDM symbol     | 16 samples at 20 MHz; none on L-STF; 32 on L-LTF with a 128-sample body (`useful`) | 16, or 8 **per OFDM symbol**                     | same, scaled                           | 16, 32, 64                                                    | 160 on the first symbol of a slot, 144 after; extended 512     | 1/4, 1/8, 1/16, 1/32 of `n_fft`                |
| per-subband phase rotation         | none                                                                               | upper subband times j at 40 MHz                  | `[1,-1,-1,-1]` at 80; 8 entries at 160 | none                                                          | none                                                           | none                                           |
| duplicated legacy fields           | none                                                                               | two subbands                                     | four, eight                            | legacy at 64, none                                            | none                                                           | none                                           |
| window                             | `window`                                                                           | `window`                                         | `window`                               | `window`                                                      | `window`                                                       | `window`                                       |
| scaling                            | per class                                                                          | per class                                        | per class                              | per class                                                     | per class                                                      | per class; boost is in the values              |
| truncated or repeated body         | L-STF 160 of 64; L-LTF 128 of 64                                                   | same                                             | same                                   | HE-STF 80 of 256; HE-LTF 64 and 128 of 256                    | none                                                           | none                                           |
| data points per OFDM symbol        | fixed                                                                              | fixed                                            | fixed                                  | fixed                                                         | **varies by phase** (pilots displace data)                     | fixed per phase                                |

The 802.11 columns name fields of that standard's PPDU; [wlan](wlan.md)
defines them.

**What needed a generalization.** A throwaway prototype outside the tree
rendered the 802.11 columns, an LTE-like slot and a DVB-T-like frame through
one function with no format name inside it. It was for deciding the shape,
and none of its numbers is evidence (section 6). Five things in the struct
exist because a flavor forced them, and each is a mechanism rather than a
special case:

1. **`useful`, a body read cyclically from the transform.** One 802.11
    training field is ten 16-sample periods of a 64-point transform (160
    samples, no prefix); another is 128 samples with a 32-sample prefix; an
    HE training field is 80 samples of a 256-point transform. A `reps` count
    would cover the first two and none of the others.
1. **A fixed pattern as a table over (phase, subcarrier)**, not "base
    pattern, rotate by `n`, polarity, offset". The rotate rule covers the
    802.11 flavors and neither LTE (positions move, values are a sequence)
    nor DVB-T (values by carrier, boosted). The rotation is a constructor
    that writes the table; the polarity sequence stays a separate
    per-OFDM-symbol scalar because it multiplies the pilots and not the data.
1. **Per-class `n_fft` and per-run `cyclic_prefix`.** HE needs 64 and 256 in
    one PPDU at one sample rate, and nearly every flavor needs a prefix that
    changes from OFDM symbol to OFDM symbol (a short guard interval, the
    first symbol of an LTE slot).
1. **`subcarrier_gain`**, which carries a per-subband phase rotation (`j` at
    40 MHz, `[1,-1,-1,-1]` at 80) and would carry any per-subcarrier
    amplitude.
1. **A data count that varies with the phase.** It is derived (the data
    subcarriers less those a fixed entry displaces), never declared, and it
    is what an LTE symbol with reference signals needs.

Duplicating a legacy field across subbands needed none of them: it is a
constructor that replicates the 64-point class's map, pattern and sequence
into each 20 MHz subband and writes the subband phases into
`subcarrier_gain`.

**What does not fit the modulator, by design.** None of it is in the
modulator, and each is a finding:

- **Arithmetic on field contents**, which belongs to the layout's builder:
    a length field computed from a duration, a bit that disambiguates a
    symbol count, the symbol, tail and pad counts. Per-standard rules,
    written once each, on the standard's page.
- **Training sequence tables.** They are data a builder holds.
- **DFT-precoded OFDM** (LTE uplink, NR DFT-s-OFDM) transforms the data
    points before they are placed. It is an optional stage ahead of the
    modulator, not a field of the layout, and adding it later changes
    nothing here.
- **A second data stream on fixed carriers**, which DVB-T's TPS carriers
    need (their values follow per-frame content). It is an additive map
    value naming a second point source; nothing existing moves.
- **MIMO**, out by ruling: a spatial stream is another layout.

## 5. The object, and where it lives

**C.** `native/inc/doppler/ofdm/ofdm_core.h` and
`native/src/ofdm/ofdm_core.c`: the layout structs above and a modulator state
that holds one layout and the position in it. The header
includes `fft`, `dp_state.h` and nothing else of the library. A C
application calls it with no Python in the process.

**The calls.** The layout is built by calls, as `Frame` is built by
`add_field` and `add_stage`, because a struct-pointer constructor is not
jm-expressible (the note in `objects/conv_enc.toml`): `add_class`,
`add_subcarrier_gain` and `add_polarity` (a class's optional arrays),
`add_fixed` and `add_run`. `check` checks the whole layout; `modulate` checks on first use, and adding to
a checked layout re-opens it, keeping the position and the held window tail,
so a signal can be extended while it is rendered. `modulate` renders whole OFDM symbols from the
points it is given and `finish` returns the window tail still held.
`points_remaining` says how many points the rest of the layout consumes, and
the position is readable (`run_index`, `sym_in_run`, `tail_len`).

**Python.** A module of its own, `doppler.ofdm`, holding the modulator object
`Ofdm`, which holds its layout. It is not a part of `wfm` and does not import
it, so `import doppler.ofdm` works in a process that has never loaded wfmgen.
wfmgen's `ofdm` type imports it, never the reverse. A separate value object
for a layout that wfmgen's faces carry (a JSON scene, a CLI file) is wfmgen's
concern ([wlan](wlan.md), "The faces"), as `frame` is for `FrameDesc`.
Standard builders (a layout from a preset) are free functions that fill an
`Ofdm`; they live with the standard.

A refused build call raises `ValueError`, and so does a `modulate` whose point
count ends inside an OFDM symbol or runs past the layout, or whose layout is
unusable, with a sentence saying which; the modulator is left as it was.
Rendering nothing, with no points where none are needed, is an empty array
and not a refusal.

### The window

The boundary between two OFDM symbols is a raised-cosine cross-fade of
`window` samples: `rise[j] = sin^2 (pi (j + 1/2) / (2 window))` and
`fall[j] = 1 - rise[j]`. The later symbol's first `window` samples are
weighted by `rise`. The earlier symbol is continued past its end by `window`
samples of the transform output, `x[(useful + j) mod n_fft]`, weighted by
`fall`, and overlap-added. The length used at a boundary is the later symbol's
class `window`; the first symbol rises from silence over its own, and the
last continues over its own. A window of 0 is no window: the output is the
symbols back to back. A layout whose window is longer than an OFDM symbol of
its class does not pass `check`.

### State

The modulator carries the run index, the OFDM-symbol count within the run,
the length of the previous OFDM symbol's window tail, and that tail's samples
(the blob always holds the largest window's worth of slots, so its size is a
function of the layout and the slots past the tail are zero). A run's polarity
offset is part of the layout, not of the state. It implements the state triplet
(`ofdm_state_bytes`, `ofdm_get_state`, `ofdm_set_state`) and is declared
`serializable = "true"`.

- **The layout is configuration**, so it enters the blob as a reject key,
    not as state: a `dp_hash64` of its canonical serialization (the fixed
    entries held sorted by phase and subcarrier, so two orders of entry are
    one layout), and a blob never restores into a different one.
- **`set_state` decodes into a temporary, checks it whole, then commits**
    (the standing rule): the run index must name a run (or the end), the
    count must be within the run, and the tail length must be the one the
    position implies, which is the window of the preceding symbol's class, or
    nothing before the first symbol and after `finish()`.
- **A setter's value travels.** The modulator has no setter that changes
    what a blob means; a layout is replaced by constructing another.
- **The version is 1** and `make state-versions-check` holds it.

### Cost

Per OFDM symbol the modulator does one pass over the data subcarriers and
the fixed entries of the phase, against an `O(n_fft log n_fft)` transform:
bounded by the transform, with its fraction of a render unmeasured (section
7). The memory is the tables: `n_fft` bytes of map per class, 8 `n_fft`
bytes of `subcarrier_gain` where a class has one, and 16 bytes per fixed
entry. A DVB-T 8k class is about 2400 entries, about 38 kB; an LTE slot of
10 MHz, under a thousand; a Wi-Fi class a few dozen. One transform plan is
held per distinct `n_fft`.

## 6. How it is proven

The rule is that the anchor is never the implementation under test and
never a second implementation in the tree.

1. **An independent numpy reference, written separately from the C.** A
    direct per-OFDM-symbol computation (place, scale, transform, read,
    prefix, window, overlap-add) in numpy, sharing no code with the C, is
    the oracle for the whole render across layouts that exercise each field:
    a truncated body, a prefix per run, a pattern with period more than
    one, a polarity sequence with an offset, a subcarrier gain, a class
    change inside a layout. The oracle is held to float32 rounding.
1. **Structure, by inverting the primitive.** A forward `fft` of each OFDM
    symbol, cyclic prefix removed, returns the grid that went in: data
    subcarriers hold their points, fixed entries their values, null
    subcarriers nothing. This inverts the modulator with an existing
    object; it is not a receiver.
1. **Derivable properties**: the data count per phase equals the map's data
    subcarriers less the displaced ones; the length of a render equals the
    sum over runs of the OFDM symbol length; scaling the
    layout's `scale` scales the output linearly.
1. **State**: the shared round trip, and a mid-stream split, taken inside a
    window transition, that resumes bit for bit. A call renders whole OFDM
    symbols, the unit that consumes points, so a split falls on an OFDM-symbol
    boundary and the window tail is what crosses it.
1. **Sabotage.** Each pin is proven by breaking the code it pins and
    requiring the test to go red, before the Python face exists.
1. **The position is observable.** `run_index`, `sym_in_run` and `tail_len`
    are read-only properties, and a reset object's state is byte for byte a
    fresh one's. Telemetry probes (the lifecycle's other instrument) are not
    in the first PR; an issue carries them.

What a standard's preset is proven against is that standard's page. The
modulator's pins make no claim about any standard.

### What the prototype was for

A throwaway numpy prototype outside the tree drove one modulator function
with layouts only, before this page's shape was written down. Phase 1 of the
lifecycle allows a prototype to decide whether to build, and this one did:
it decided that one function with no format name inside it can render the
non-HT, HT, VHT and HE PPDU structures, an LTE-like slot and a DVB-T-like
frame, and it found the five generalizations of section 4. It could not
decide, and does not, whether any standard's number is right: its HT, VHT,
HE, LTE and DVB-T tables were stand-ins, and its other values were recalled.
**Its output is not evidence and this page quotes none of it.** Whatever is
worth claiming is pinned in PR 5's tests, against the numpy reference of
section 6, and nothing else is claimed.

## 7. Unknowns

1. **The window's cost.** The EVM a given window length costs, and the mask
    margin it buys, are unmeasured.
1. **Whether a filter is needed** to meet a standard's transmit mask at a
    given bandwidth and oversampling factor, or a `fir` stage is owed. That
    is each standard's question; the measuring tools are the library's.
1. **Speed.** No target is set. Whether the modulator renders faster than
    real time at 20 MHz, and at 160 MHz, on one core is unmeasured, as is
    the fraction of a render the per-OFDM-symbol table pass takes at 64 and
    at 4096 points.
1. **The float32 error budget.** How far the rendered samples sit from the
    exact values, per `n_fft`.
1. **PAPR per modulation after the window**, and the headroom a scene needs.
    Nothing is quoted.

## 8. Decisions

### Ruled

1. **The general shape is the deliverable.** The owner's ruling: the modulator takes a layout, the
    presets are filled instances of it, and **no preset claims conformance
    to any standard (802.11a, g, n or ac) until its numbers are checked
    against the standard's text.** The letters are not certified until
    validated against the specifications.
1. **The word is layout.** The modulator's description is a layout, so that
    it is not mistaken for wfmgen's `Plan`: `--ofdm-layout`, a source field
    `layout`, a scene key `"layout"`, `--ofdm-layout FILE`. "Transform plan"
    means the FFT's and nothing else.
1. **The modulator is a general library object** in its own module,
    `doppler.ofdm`, usable with no wfmgen in the process; wfmgen's `ofdm`
    type is one of its callers.
1. **`qam` lives beside `mpsk`**, in the same module on the Python face.
1. **A parametric CRC**, built, with `dp_crc16_ccitt` as one catalogue
    instance of it.
1. **No MIMO.** Out of scope, not deferred.

### Open

1. **Each standard's text.** The 802.11 text is not in `~/refs`; what it
    gates is on [wlan](wlan.md).
1. **The name of the constellation field** on wfmgen's type, and the
    spellings, on [wlan](wlan.md).

## 9. The work, in order

Sizes are rough and count hand-written C and tests, not generated files.
The order for the consumers is on [wlan](wlan.md).

| PR  | what                                                                                  | phases                                    | size        |
| --- | ------------------------------------------------------------------------------------- | ----------------------------------------- | ----------- |
| 5   | `ofdm`: the layout-driven modulator, its state triplet, `doppler.ofdm`, bench, report | declare, implement, pin, bind, instrument | ~1100 lines |

The primitives it builds on (puncturing in `conv`, the parametric CRC, `qam`)
are in section 3. Each pin is proven by sabotage before the Python face
exists. A modulator PR touches the gates a new object touches: `drift-check`
after the manifest, `state-versions-check` and `check_serializable.py` for the
blob, `doxygen-check` and `doc-symbols-check` for the header, and the
validation report's two gates.

## Related

- [wlan](wlan.md): the IEEE 802.11 OFDM PHYs, and wfmgen's `ofdm` type, as
    callers of this modulator
- [wfmgen](wfmgen.md): the tool whose waveform type this is one call of
- [State Serialization](state-serialization.md): the blob rules section 5
    applies
- [A Frame as a Description](frame-description.md): the precedent for a
    structure built by calls
- [Adding an algorithm](../dev/contributing/adding-algorithms.md): the
    lifecycle this page opens
