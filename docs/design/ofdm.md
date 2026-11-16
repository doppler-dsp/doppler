---
description: "OFDM for wfmgen, proposed: a general OFDM modulator as the primitive, the IEEE 802.11 OFDM PHYs as presets over it, which existing objects each 802.11 block composes, how it is proven against the standard's own example, and what is still unknown"
---

# OFDM, and the IEEE 802.11 OFDM PHYs

*Part of [wfmgen](wfmgen.md), which owns the tool's goals, what it
composes, and what is still unknown about it.*

!!! note "Proposed"

    No part of this page is built. It is the proposal that precedes the
    code: the layering, the scope, what is reused, how it is proven, and
    the decisions. Section 11 lists the ones the owner has ruled and the
    ones still open, each open one with a recommendation; sections 9 and 10
    list what is not known.

wfmgen gains one waveform type, `ofdm`. It is a general OFDM modulator, and
the IEEE 802.11 OFDM PHYs are named presets over it. The scope is the PHY
plus a minimal MAC wrap (section 3): the caller supplies a frame body, or
the whole PSDU, or a PN fill stands in for one, and the generator produces
the PPDU's baseband samples at the standard's sample rate. It builds no
management or control frame, aggregates nothing, and runs one spatial
stream (section 2).

Every clause, table and figure number below refers to IEEE Std 802.11-2020
unless it says otherwise. **None of them is checked against the text**:
every standard-fixed number here is from memory, and section 10 lists them
for checking.

## Vocabulary

Each term is defined once here and used in this sense on the rest of the
page. Field and parameter names are spelled as the standard spells them.

### The signal

- **subcarrier**: one frequency bin of the transform, numbered `k` from
    `-N_FFT/2` to `N_FFT/2 - 1` with 0 at the carrier. Never "tone" or
    "bin" on this page; the standard's "26-tone RU" is a name, not a term.
- **subcarrier spacing**, `Δf`: the channel bandwidth divided by `N_FFT`.
- **OFDM symbol**: one inverse transform's output with its cyclic prefix.
    The bare word "symbol" is not used.
- **constellation point**: one complex value carried by one data
    subcarrier of one OFDM symbol. Never "symbol".
- **grid**: the constellation points and pilot values of one OFDM symbol,
    one per occupied subcarrier, before the inverse transform.
- **cyclic prefix**: the last samples of an OFDM symbol's transform output,
    copied ahead of it. The primitive's word, counted in samples.
- **guard interval**, GI: the standard's name for the cyclic prefix,
    stated as a duration (`T_GI`). The 802.11 layer says GI; the modulator
    says cyclic prefix; they are the same samples.
- **window**: the amplitude taper over the transition between two
    consecutive OFDM symbols (17.3.2.5), `T_TR` long.
- **subcarrier map**: which subcarriers are **data**, which are **pilot**,
    and which are **null** (the DC subcarrier and the band edges).
- **pilot polarity sequence**: the 127-element sequence `p_n` that scales
    every pilot of OFDM symbol `n` (17.3.5.10).

### The 802.11 PPDU

- **PSDU**: the octets the PHY transmits as the Data field's payload. Here
    it is one MPDU, never an aggregate: the caller's octets as given, or the
    MPDU the minimal MAC builds from them.
- **MPDU**: one MAC frame: MAC header, frame body, FCS (Clause 9).
- **FCS**: the frame check sequence, the last four octets of an MPDU: a
    CRC-32 over the octets before it.
- **PPDU**: everything the PHY transmits for one PSDU: the preamble
    fields, the SIGNAL fields and the Data field.
- **field**: one named part of a PPDU, by the standard's name: L-STF,
    L-LTF, L-SIG, HT-SIG, HT-STF, HT-LTF, VHT-SIG-A, VHT-STF, VHT-LTF,
    VHT-SIG-B, RL-SIG, HE-SIG-A, HE-SIG-B, HE-STF, HE-LTF, Data, PE. Clause
    17 itself calls the first three the short training sequence, the long
    training sequence and SIGNAL; this page uses the `L-` names throughout.
- **PPDU format**: non-HT (Clause 17), HT-mixed and HT-greenfield
    (Clause 19), VHT (Clause 21), HE SU / HE ER SU / HE MU / HE TB
    (Clause 27).
- **MCS**: the standard's index into a modulation and coding-rate table,
    for HT and later formats. A non-HT PPDU has no MCS; it has a RATE.
- **spatial stream**: one of the independently coded data streams of an HT
    or later PPDU. Every PPDU here has one (goal 8).
- **clocking**: the same PHY run at a fraction of its sample rate; Clause
    17 defines 20 MHz, 10 MHz ("half-clocked") and 5 MHz
    ("quarter-clocked") channel spacings.

### Counts

`N_FFT` transform length; `N_SD` data subcarriers; `N_SP` pilot
subcarriers; `N_ST = N_SD + N_SP`; `N_BPSC` coded bits per constellation
point; `N_CBPS` coded bits per OFDM symbol; `N_DBPS` data bits per OFDM
symbol; `N_SYM` OFDM symbols in the Data field.

## 1. Who calls it

| who                           | with what                                    | what they do with the answer                                |
| ----------------------------- | -------------------------------------------- | ----------------------------------------------------------- |
| Someone who needs a PPDU      | `wfmgen --type ofdm --standard 802.11a ...`  | feeds a receiver, a signal generator or a sniffer           |
| A crowded-band scene          | an `ofdm` source summed with other sources   | measures a receiver against a wideband bursty interferer    |
| A C application               | the modulator and the PPDU renderer directly | renders PPDUs into its own buffer, no Python in the process |
| Someone building another OFDM | `--type ofdm` with their own subcarrier map  | a non-802.11 OFDM signal with the same engine               |
| A future OFDM receiver's test | a scene declared once                        | scores demodulation against truth the scene also carries    |

The last row is the reason the generator comes first: the library has no
OFDM receiver, and a receiver is measured against a generator that is
already proven.

## 2. Design goals

1. **The standard rules.** Every 802.11 number comes from the standard and
    is cited where it is written into code. A preset has no tunable that the
    standard fixes.
1. **The primitive is general.** The modulator knows nothing of 802.11. A
    preset is a configuration of it, the way `dp_CCSDS_TM_CONV` configures
    `conv_code_t`.
1. **Nothing is implemented twice.** Each PHY block calls the object that
    already owns it (section 4). What is absent is added as a general
    primitive where a second user is plausible, and only otherwise inside
    the 802.11 component.
1. **Bit-exact where the standard gives bits, to the printed precision
    where it gives samples.** Annex I is the anchor (section 8).
1. **0 dBFS is unit average power**, as for every wfmgen source. An
    802.11 PPDU's fields are normalized by the standard to equal power, so
    one scale factor serves the whole PPDU.
1. **Four faces, one engine**: C, the CLI, Python and the JSON scene render
    byte-identically, as [wfmgen](wfmgen.md) requires of every type.
1. **The PHY, and a minimal MAC wrap.** The PHY in full, and the least MAC
    that makes a PPDU a frame a real receiver accepts: an MPDU built from a
    caller's header and body with its FCS (section 3, "The minimal MAC").
    No aggregation, no management or control frames, no encryption, no
    channel access timing. Ruled 2026-11-16.
1. **One spatial stream. MIMO is out of scope, not deferred.** No more than
    one spatial stream, no space-time block coding, no cyclic shift
    diversity across transmit chains, no HT MCS 8 to 31. HT, VHT and HE
    are single-stream PHYs, and wfmgen keeps one output stream. Ruled
    2026-11-16; this is the one place the page says so.

## 3. The layers

```text
FACES      wfmgen --type ofdm · doppler.wfm · JSON scene
             └── fill in wfm_source_t; hold no logic

SOURCE     wfm_synth, type = ofdm
             ├── standard = none ── the caller's plan (the one shape)
             └── standard = 802.11x ── wlan PPDU renderer
                                         │
PRESETS    wlan ── PSDU bits -> one PPDU's samples (stateless render)
             ├── fields: L-STF, L-LTF, L-SIG, Data (then HT-, VHT-, HE-)
             ├── MAC wrap ── header octets · FCS ── CRC-32 (new, shared)
             ├── scrambler ── pn
             ├── FEC ── conv, with puncturing (new, in conv)
             ├── interleaver ── dp_interleave + the 802.11 second step
             ├── mapper ── qam (new) · mpsk
             └── ofdm ── the modulator
                           │
PRIMITIVE  ofdm ── grid in, samples out
             ├── fft (inverse, cf32)
             └── plan: classes · runs · pilot tables · cyclic prefix · window
```

These are new, each a C core first:

- **`ofdm`**, the modulator, driven by a plan (the one shape, below). Per
    OFDM symbol it places constellation points and the plan's fixed entries
    on a grid, inverse transforms, reads the class's `useful` samples,
    prefixes the symbol's cyclic prefix, windows, and overlap-adds onto the
    previous symbol's tail. It names no standard.

- **`qam`**, the square Gray-coded QAM map for 16, 64, 256 and 1024
    points, unit average power. A sibling of `mpsk`.

- **`wlan`**, the 802.11 PHY: a table of PPDU formats, and a renderer from
    PSDU bits to one PPDU's samples, including the minimal MAC wrap below.

- **The CRC-32 primitive**, a header beside `dp_crc16.h`. It is
    a shared primitive and `wlan` calls it: no CRC-32 exists in the tree
    today, and a copy inside `wlan` would be the second implementation the
    next CRC-32 user finds and diverges from.

And one extension: **puncturing in `conv`**, as a pattern beside
`conv_code_t`, in both directions: delete coded bits on encode, and on
decode insert an erasure at each deleted position ahead of `Viterbi`.
`Viterbi` takes soft channel symbols, one log-likelihood ratio each
(`viterbi_core.h`: "soft in, hard out"), so an erasure is an LLR of 0 and
the trellis is unchanged. It is general, not an 802.11 feature: the CCSDS
punctured rates are the same mechanism.

### The minimal MAC

Ruled 2026-11-16: the generator wraps the caller's octets as one MPDU, and
nothing more. A source's `mac` field selects one of three modes:

| `mac`  | the caller supplies          | the PSDU is                       |
| ------ | ---------------------------- | --------------------------------- |
| `none` | the whole PSDU               | the caller's octets, as given     |
| `fcs`  | an MPDU without its FCS      | the caller's octets, then the FCS |
| `data` | the header, and a frame body | header, body, FCS                 |

- **The header** in `data` mode is given as raw octets (`mac_header`), or,
    as a convenience over the same octets, as the fields of a data frame's
    header: Frame Control, Duration/ID, three addresses and Sequence
    Control, 24 octets. A header with more (a fourth address, QoS Control,
    HT Control) is given as raw octets. Which form the first slice declares
    is section 11's.
- **The FCS** is the CRC-32 primitive over every octet before it, appended in the byte
    order the standard defines (Clause 9; section 10).
- **`data_len`** counts the octets the caller supplies, which is the frame
    body in `data` mode and the whole PSDU or MPDU in the other two. The
    PPDU's LENGTH is derived from the PSDU, never from a field.

What a real receiver needs to accept the frame, and what this provides: a
valid FCS over the MPDU, or the receiver discards it; a header whose Frame
Control names a protocol version and a type and subtype the receiver knows,
which the caller's header carries; an Address 1 the receiver is listening
for, or any address to a receiver in monitor mode; and a L-SIG RATE and
LENGTH that agree with the PSDU, which `wlan` derives.

What is left out, deliberately: management and control frame builders
(beacon, ACK, RTS and the rest are the caller's raw octets, in `none` or
`fcs` mode); A-MPDU aggregation; retries and sequence-number management (the
caller sets the Retry bit and the Sequence Control value, and the generator
does not change either between PPDUs); encryption (a Protected Frame bit set
by the caller marks a frame whose body the caller encrypted); fragmentation;
and any timing between PPDUs beyond what the scene's frames and gaps already
give.

### The one shape

**One shape, the public one, from the first slice.** The `ofdm` modulator is
driven by a **plan**, and every 802.11 preset is a filled-in plan. The
modulator names no standard and branches on none; `wlan` builds plans. The
shape was checked against the PHYs below, not promised for all of them: the
presets promised are rows 1 to 3 of section 6, and the rest are
shown to fit so the API need not change when they arrive.

The plan is a list of **symbol classes** and a list of **runs**:

```c
typedef struct {            /* one entry of a class's fixed pattern      */
  uint32_t phase;           /* pattern phase, or OFDM_ALL_PHASES         */
  int32_t  k;               /* subcarrier number, -n_fft/2 .. n_fft/2-1  */
  float _Complex value;     /* pilot, training tone, boosted pilot       */
} ofdm_fixed_t;

typedef struct {            /* one kind of OFDM symbol                   */
  uint32_t n_fft;           /* transform length OF THIS CLASS            */
  uint32_t useful;          /* body samples read cyclically from the     */
                            /* inverse transform; 0 means n_fft          */
  uint32_t cp;              /* default cyclic prefix, samples            */
  uint32_t win;             /* window transition, samples                */
  double   scale;           /* multiplies the transform: 1/sqrt(N_Tone)  */
  const uint8_t        *role;  /* n_fft: DATA or NULL, index k + n_fft/2 */
  const float _Complex *tone;  /* n_fft: per-subcarrier multiplier, or   */
                               /* NULL (subband phase, amplitude)        */
  uint32_t n_phase;         /* period of the fixed pattern, in symbols   */
  const ofdm_fixed_t   *fixed; /* sorted by (phase, k)                   */
  uint32_t n_fixed;
  const float _Complex *pol;   /* per-symbol scalar on fixed values,     */
  uint32_t n_pol;              /* p[(n + z) mod n_pol]; NULL for none    */
} ofdm_class_t;

typedef struct {            /* a run of OFDM symbols of one class        */
  uint32_t cls;             /* index into the classes                    */
  uint32_t count;
  uint32_t cp;              /* per-symbol override, or OFDM_CP_DEFAULT   */
  uint32_t z;               /* offset into `pol`                         */
  uint32_t n0;              /* pattern phase of the run's first symbol   */
} ofdm_run_t;
```

Per OFDM symbol the modulator takes `n_data(phase)` constellation points,
places them on the DATA subcarriers the pattern leaves free, sets the fixed
entries of that phase (times `pol`), multiplies by `tone`, transforms,
reads `useful` samples cyclically from the result, prepends the last `cp` of
them, and overlap-adds the window. That is all of it; nothing else in the
modulator knows what a PPDU, a slot or a frame is. The constellation points
come from a mapper (`qam`, `mpsk`) or from the `symbols` source.

Every row below is checked against its own flavor. A cell reads "fits" with
what fills it; "needs X" names the generalization the cell forced into the
struct above. All 802.11 numbers are recalled, not read from the text; the
HE, LTE and DVB-T columns are stand-ins with the right occupancy.

| shape parameter                             | non-HT 20/10/5                                                                     | HT 20, HT 40                              | VHT 20/40/80/160                        | HE SU (not promised)                                          | LTE/NR (outsider)                                              | DVB-T (outsider)                               |
| ------------------------------------------- | ---------------------------------------------------------------------------------- | ----------------------------------------- | --------------------------------------- | ------------------------------------------------------------- | -------------------------------------------------------------- | ---------------------------------------------- |
| `n_fft`, per class                          | 64                                                                                 | 64, 128                                   | 64, 128, 256, 512                       | legacy 64 and HE 256 in one PPDU: **per class**, not per PPDU | 128 to 2048 (NR to 4096 and up)                                | 2048, 8192                                     |
| spacing                                     | derived, `fs/n_fft`: 312.5 kHz at 20 MHz                                           | 312.5 kHz                                 | 312.5 kHz                               | 312.5 kHz legacy, 78.125 kHz HE                               | 15 kHz (NR `15 * 2^mu`)                                        | derived, about 4.46 kHz (2k)                   |
| `role`: data, pilot, null, DC               | 48 + 4 of 52 used, DC null                                                         | 52 + 4 of 56; 108 + 6 of 114, -1..1 null  | 52 + 4; 108 + 6; 234 + 8; 468 + 16      | 234 + 8 at 20 MHz (RU242)                                     | 1200 used per 20 MHz, DC null                                  | all 1705 used, DC is a carrier                 |
| pilot positions                             | +-7, +-21                                                                          | +-7, +-21; +-11, +-25, +-53               | per width; 160 is two 80 MHz copies     | stand-in positions                                            | `6m + (v + shift) mod 6`, moves with symbol and cell           | continual list + `3 (l mod 4) + 12 p`          |
| `fixed` as a table over (phase, subcarrier) | one phase                                                                          | period 4 and 6                            | period 4, 6, 8                          | period 8                                                      | period 14, 200 per CRS symbol                                  | period 4 plus all-phase continual              |
| pilot VALUE vs symbol                       | base times `p[n + 1]`, no rotation                                                 | base **rotated** by `n`, times `p[n + 3]` | rotated, period 8 at 80 MHz, `p[n + 4]` | rotated, offset by field                                      | Gold-sequence QPSK per (slot, symbol, `m`): **not a rotation** | PRBS by carrier, 4/3 boost, **not a rotation** |
| cyclic prefix, per symbol                   | 16 samples at 20 MHz; none on L-STF; 32 on L-LTF with a 128-sample body (`useful`) | 16, or 8 (0.4 us) **per symbol**          | same, scaled                            | 16, 32, 64 (0.8, 1.6, 3.2 us)                                 | 160 on the first symbol of a slot, 144 after; extended 512     | 1/4, 1/8, 1/16, 1/32 of `n_fft`                |
| per-subband phase rotation                  | none                                                                               | upper subband times j at 40 MHz           | `[1,-1,-1,-1]` at 80; 8 entries at 160  | none                                                          | none                                                           | none                                           |
| duplicated legacy fields                    | none                                                                               | two subbands                              | four, eight                             | legacy at N=64, none                                          | none                                                           | none                                           |
| window                                      | `win`                                                                              | `win`                                     | `win`                                   | `win`                                                         | `win`                                                          | `win`                                          |
| per-field scaling                           | `1/sqrt(N_Tone)`, the STF's `sqrt(13/6)` in its table                              | per class                                 | per class                               | per class                                                     | per class                                                      | per class; boost is in the values              |
| truncated or repeated body                  | L-STF 160 of 64; L-LTF 128 of 64                                                   | same                                      | same                                    | HE-STF 80 of 256; HE-LTF 1x and 2x 64 and 128 of 256          | none                                                           | none                                           |
| data points per symbol                      | fixed                                                                              | fixed                                     | fixed                                   | fixed                                                         | **varies by phase** (pilots displace data)                     | fixed per phase                                |

The coding chain is a separate record, one per PPDU format and rate, and
`wlan` is where it lives:

| parameter                       | a to ac, one spatial stream                                                                                                                | fits?                                                                   |
| ------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------- |
| scrambler                       | `x^7 + x^4 + 1`, initial state per PPDU, the same in every flavor                                                                          | fits; one polynomial                                                    |
| BCC encoders                    | one, or more at high rates (HT above 300 Mb/s, VHT 160 MHz); the bits are dealt to the encoders in turn                                    | fits: `n_es`, one parser loop                                           |
| puncture pattern                | 1/2, 2/3, 3/4, 5/6, the same in every flavor                                                                                               | fits: a pattern table                                                   |
| interleaver                     | columns 16, 13, 18, 26 for 20 MHz non-HT, HT/VHT 20, 40, 80; rows `N_SD * N_BPSC / columns`                                                | fits: one first permutation, `rows = N_CBPS / columns`                  |
| interleaver, second permutation | `s = max(N_BPSC / 2, 1)`, the same everywhere                                                                                              | fits                                                                    |
| interleaver, third permutation  | per-stream rotation, only for more than one stream                                                                                         | out of scope (goal 8)                                                   |
| segment parser                  | none to 80 MHz; two 80 MHz segments at 160                                                                                                 | fits: `n_seg`, each segment interleaved on its own                      |
| constellation                   | BPSK to 256-QAM (1024 for HE); Q-BPSK is BPSK rotated 90 degrees                                                                           | fits: a per-symbol rotation on the mapper                               |
| SIG fields                      | L-SIG 24 bits with parity; HT-SIG and VHT-SIG-A 48 bits, CRC-8, two symbols; VHT-SIG-B 26 to 29 bits, CRC-8, tiled to fill the subcarriers | the coding fits; the bit layouts are `FrameDesc` fields and a CRC stage |

**What needed a generalization, and what did not.** The checks rendered
non-HT 20, HT 20, HT 40, VHT 80, an HE 20 PPDU, an LTE-like slot and a
DVB-T-like frame through one modulator, with no format name inside it. Five
things in the struct exist because a flavor forced them, and each is a
mechanism rather than an 802.11 special case:

1. **`useful`, a body read cyclically from the transform.** The L-STF is ten
    16-sample periods of a 64-point transform (160 samples, no prefix); the
    L-LTF is 128 samples with a 32-sample prefix; the HE-STF is 80 samples of
    a 256-point transform; the 1x and 2x HE-LTF are 64 and 128. A
    `reps` count would cover the first two and none of the others.
1. **A pilot pattern as a table over (phase, subcarrier)**, not
    "base pattern, rotate by `n`, polarity, offset". The rotate rule covers
    every 802.11 flavor from a to ac (the checks confirm periods 4, 6 and 8)
    and neither LTE (positions move, values are a sequence) nor DVB-T
    (values by carrier, boosted). The rotation is one constructor that
    writes the table; the polarity sequence stays a separate per-symbol scalar
    because it multiplies the pilots and not the data.
1. **Per-class `n_fft` and per-run `cp`.** HE needs 64 and 256 in one PPDU at
    one sample rate, and every flavor but non-HT 20 needs a prefix that
    changes from symbol to symbol (short GI, the first symbol of an LTE slot).
1. **A per-subcarrier multiplier (`tone`)**, which carries the per-subband
    phase rotation (`j` at 40 MHz, `[1,-1,-1,-1]` at 80) and would carry any
    per-subcarrier amplitude.
1. **A data count that varies with the phase.** It is derived (the DATA
    subcarriers less those a fixed entry displaces), never declared, and it is
    what an LTE symbol with reference signals needs.

Duplicating the legacy fields across subbands needed none of them: it is a
constructor that replicates the 64-point class's role, pattern and sequence
into each 20 MHz subband and writes the subband phases into `tone`.

**What does not fit the modulator, by design.** None of it is in the
modulator, and each is a finding:

- **Arithmetic on field contents** in `wlan`: the L-SIG LENGTH of a mixed-format
    PPDU (computed from the PPDU's duration so legacy stations defer), the VHT
    short-GI symbol-count disambiguation bit, and the symbol, tail and pad count
    per `n_es`. These are per-flavor rules, written once each in `wlan`, from
    the text.
- **Training sequence tables** for each flavor and width. They are data. The
    prototype used the recalled L-STF and L-LTF and stand-ins for the others, so
    it shows the shape holds them, not what they are.
- **A parametric CRC.** The SIG fields use CRC-8 and the FCS CRC-32: one CRC
    primitive with width, polynomial, initial value, final complement and bit
    order as parameters serves both, and `dp_crc16.h`'s fixed CCITT would be
    one instance of it. A CRC-32-only primitive would be the second
    implementation the next CRC finds. This changes the minimal MAC's primitive
    from section 3's "CRC-32 primitive" to a parametric CRC: the owner's call.
- **DFT-precoded OFDM** (LTE uplink, NR DFT-s-OFDM) transforms the data
    points before they are placed. It is an optional stage ahead of the
    modulator, not a field of the shape, and adding it later changes nothing
    here.
- **A second data stream on fixed carriers**, which DVB-T's TPS carriers
    need (their values follow per-frame content). It is an additive `role`
    value naming a second point source; nothing existing moves.
- **MIMO** is out by ruling, and nothing above would have to change for a
    later owner to revisit it: a spatial stream is another plan, not another
    field.

**Why 802.11ax is outside the promise.** It is not outside the shape: the
HE column fits. It is outside the promise because its preset needs the
resource-unit layouts and the DCM and dual-carrier mappings, which are
tables the first slice does not build, and the claim is checked for a to ac
only.

**What the shape costs.** The blob is unchanged in kind: the run index, the
symbol count within the run, the polarity offset and the window tail, which
is at most `win` samples. The plan is configuration, so it enters as a reject
key (a `dp_hash64` of its canonical serialization), not as state. Per OFDM
symbol the modulator does one pass over the DATA subcarriers and the fixed
entries of the phase, against an `O(n_fft log n_fft)` transform; that is
bounded by the transform, and its actual fraction of the render is unmeasured
(section 9). The memory is the tables: `n_fft` bytes of role per class, 8
`n_fft` bytes of `tone` where a class has one, and 16 bytes per fixed entry.
A DVB-T 8k class is about 2400 entries, about 38 kB; an LTE slot of 10 MHz,
under a thousand; an 802.11 class a few dozen. One transform plan is held per
distinct `n_fft`.

**The faces.** A plan is a structure of arrays, which the composer's source
fields cannot hold as flat rows, so it follows the one precedent for a
structured input: `frame` is `object = "frame.FrameDesc"` with
`surface = "bespoke"` and `copy_fn`, `free_fn` and `parse_fn`
(`modules/wfm_compose.toml:477-492`), a `--frame FILE` option, a JSON object,
and an owned object on the Python face. `plan` is the same: an
`ofdm.PlanDesc` object, `--ofdm-plan FILE`, a scene's `"plan"` object. A
`standard` preset expands to the same object, so `standard = none` with a
plan and `standard = 802.11a` are two ways to fill one field.

`PlanDesc` is built by calls, as `FrameDesc` is (`add_field`, `add_stage` in
`objects/frame.toml`), because a struct-pointer constructor is not
jm-expressible (the note in `objects/conv_enc.toml`). A throwaway project
scaffolded through the pinned jm (0.126.1) with a method of parameters
`uint32_t`, `int32_t[]`, `float _Complex[]` and `uint8_t[]` generated the
C declaration and a `.pyi` signature of `int`, `NDArray[np.int32]`,
`NDArray[np.complex64]` and `NDArray[np.uint8] | bytes`. So the builder needs **no new jm type**,
and no upstream ask follows from the plan itself. What was not scaffolded: a
second `object`-typed composer field beside `frame` (the bespoke row's
hand-written CLI and JSON readers are doppler's own, as `frame`'s are), and
a Python constructor that takes a dict; neither is needed for the first slice
and neither is asked of jm.

## 4. What each 802.11 PHY block composes

"Exists" means a caller reaches it today through a public C symbol.
Section 7 says which of these rows a prototype ran against the library.

| PHY block (clause)                     | state               | where                                                        | what is missing                                                    |
| -------------------------------------- | ------------------- | ------------------------------------------------------------ | ------------------------------------------------------------------ |
| Inverse transform (17.3.2.6)           | exists              | `fft`: `dp_fft_create`, `dp_fft_execute_cf32`                | nothing; any length, unnormalized, plan reused across calls        |
| Scrambler, `x^7 + x^4 + 1` (17.3.5.5)  | exists, lacks a map | `pn` (`objects/pn.toml`): `dp_pn_create`, poly `0x48`, len 7 | the mapping from the standard's 7-bit initial state to a `pn` seed |
| Pilot polarity `p_n` (17.3.5.10)       | exists              | the same `pn` sequence, all-ones start                       | nothing                                                            |
| BCC, K = 7, 133/171 octal (17.3.5.6)   | exists              | `conv_code_t` `{0133, 0171}`; `ConvEncoder` (`conv_enc`)     | nothing for rate 1/2                                               |
| Puncturing to 2/3, 3/4, 5/6            | absent              | `conv` (`conv_core.h`), `conv_enc`, `viterbi`                | a puncture pattern; on decode an LLR of 0 at each deleted position |
| Interleaver, first permutation         | exists              | `dp_interleave_index (k, N_CBPS/16, 16)`                     | nothing                                                            |
| Interleaver, second permutation        | absent              |                                                              | the within-group bit rotation; 802.11-specific, lives in `wlan`    |
| BPSK, QPSK map (Tables 17-8, 17-9)     | exists, to pin      | `mpsk`: `mpsk_constellation`                                 | the labels, from the text; see the note below                      |
| 16/64/256/1024-QAM map                 | absent              |                                                              | the `qam` component                                                |
| Subcarrier map, pilots, cyclic prefix  | absent              |                                                              | the `ofdm` component                                               |
| Window (17.3.2.5)                      | absent              |                                                              | in `ofdm`                                                          |
| L-STF, L-LTF sequences (17.3.3)        | absent              |                                                              | two constant tables in `wlan`                                      |
| L-SIG: RATE, LENGTH, parity (17.3.4)   | absent              |                                                              | field assembly in `wlan`                                           |
| SERVICE, tail, pad (17.3.5.2-4)        | absent              |                                                              | field assembly in `wlan`                                           |
| HT-SIG CRC-8, VHT-SIG-B, HE-SIG CRC    | absent              | `dp_crc16_ccitt` is the only CRC                             | a CRC of another width and polynomial                              |
| MAC header, frame body, FCS (Clause 9) | absent              |                                                              | field assembly in `wlan`; the minimal MAC, section 3               |
| FCS, CRC-32                            | absent              | `dp_crc16.h` is the only CRC                                 | a new shared CRC-32 primitive; not a copy in `wlan`                |
| LDPC (19.3.11.7)                       | absent              |                                                              | the codec; a design page of its own                                |
| Oversampling by an integer             | exists              | `fft` at `M * N_FFT` with a zero-padded grid                 | nothing                                                            |
| Rate change by a non-integer           | exists              | `RateConverter`, `Resampler`                                 | nothing; applied by the caller, not inside the source              |
| Spectral shaping filter                | exists              | `fir`                                                        | whether it is needed at all is unknown 9.2                         |
| Carrier offset, AWGN, level, Doppler   | exists              | `wfm_synth` (`lo`, `awgn`), `wfm_compose`, `doppler_channel` | nothing; the common tail every source already has                  |
| PSDU from a data source or a PN fill   | exists              | the payload data source (`data`, `data_len`, `fill`)         | nothing                                                            |
| Power spectrum, band power             | exists              | `psd`: `dp_psd_band_power`, `dp_psd_occupied_bw`             | a transmit-mask comparison over those bands                        |
| PAPR                                   | exists, real only   | `dp_tonemeas_time_stats`                                     | a complex-envelope PAPR                                            |
| EVM of an OFDM grid                    | absent              |                                                              | relative constellation error per 17.3.9.7.4                        |

Two rows carry a decision rather than a gap:

- **BPSK and QPSK.** `mpsk` sends BPSK bit `b` as `1 - 2b` (bit 0 is +1,
    `mpsk_core.h:139`), and its QPSK is Gray-labeled with the first point at
    pi/4. Whether Tables 17-8 and 17-9 are those maps on complemented bits,
    or some other relabeling, **is to be pinned from the text**: the
    prototype's agreement is with the tables as recalled, which section 7
    says is not evidence. `qam` covers 16 points and up; whether `wlan`
    reaches BPSK and QPSK through `mpsk` or through `qam` waits on that pin.
- **The scrambler seed.** `pn` reproduces the sequence, but at a phase
    offset from the standard's (four chips for the all-ones state), so the
    standard's initial state is not the `pn` seed and neither is its bit
    reverse. All 127 states are reachable by some seed.
    The conversion is a derivation, written once in `wlan` and pinned
    against Annex I's scrambled bits.

## 5. Where a waveform type plugs in

A waveform type is one entry in the `wfm_type` enum and whatever its
neighbors needed. `dsss`, the most recent, provides:

| layer            | what the type provides                                                                                                          |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------------- |
| enum             | a name in `[[enum]] wfm_type` (`just-makeit.toml`), the same name in `TYPE_NAMES` and `N_TYPES` (`wfm_names.h`), a C enumerator |
| C core           | a `wtype` branch in `wfm_synth_core.c`, a setter that installs its configuration, `reset`                                       |
| state            | its running fields in the `wfm_synth` blob, a version bump, its configuration as reject keys                                    |
| source struct    | fields on `wfm_source_t`, each declared once in `modules/wfm_compose.toml` with its CLI and JSON spelling                       |
| bridge           | refusals in `dp_wfm_source_error` and `dp_wfm_scene_error`, attachment in `wfm_synth_bridge.c`, SNR in `wfm_resolve.c`          |
| faces            | generated: CLI option table, JSON reader and writer, schema, Python keyword, options reference                                  |
| capture metadata | the type's name and parameters in the SigMF and BLUE writers                                                                    |
| C tests          | `native/tests/` pins, the state round trip, the mid-stream split                                                                |
| Python tests     | the state matrix, `test_dsp_correctness.py`, the surface round trip, the flag matrix                                            |
| certification    | the `wfm_synth` report's claims for the type                                                                                    |
| user story       | a section in `docs/guide/wfmgen/waveforms.md`, a C and a Python example, a gallery decision, a bench row                        |

### The enum

`ofdm` is the tenth name: index 9 in `[[enum]] wfm_type`
(`just-makeit.toml:128`), in `TYPE_NAMES` with `N_TYPES` raised to 10
(`wfm_names.h:24-29`), and `WFM_SYNTH_OFDM = 9` beside `WFM_SYNTH_DSSS = 8`
(`wfm_synth_core.h:45`). List order is the C value, so the name is appended,
never inserted. `scripts/check_wfm_enum_tables.py`, run as
`make lint-wfm-enum-tables`, holds the three to one another: each name
table has one C home and equals its manifest `values` in order, every table
carries an `SSOT:` annotation, the `count=` macro equals the table length,
and the Nth enumerator of a named C enum has the explicit value N. A name
added in one place and not the others turns it red.

### What `dsss` does, which `ofdm` cannot copy

The synth does not render a DSSS burst. The bridge assembles it:
`dp_wfm_source_attach_dsss` (`wfm_synth_bridge.c:901`) builds the chips from
the frame description and hands them to `dp_wfm_synth_set_dsss_chips`
(`wfm_synth_core.c:367`), which stores them as the synth's `bits` pattern.
From there a burst is played by the bit cursor and shaped by the same
`sps` and `pulse` machinery as any bit pattern. With a data source, one
burst is one frame pulled on demand:

- `dp_wfm_synth_set_refill` (`wfm_synth_core.c:272`) installs a callback of
    type `wfm_synth_refill_fn` (`wfm_synth_core.h:164`), `(user, bits, n)`.
    At the frame boundary the synth calls it to write the next frame's `n`
    bits into `bits` in place. **The frame length never changes**, and a
    refill is accepted only by a `bits` or `dsss` synth (`:279-283`).
- The bridge's callback is `data_pull_refill` (`wfm_synth_bridge.c:771`);
    `pull_park` (`:875`) parks the cursor at the end of a placeholder frame,
    so nothing is drawn until the first bit is due. A composer can build a
    source only to validate it without reading a byte of the data source.
- `dp_wfm_synth_set_symbols` (`wfm_synth_core.c:455`) is the only call that
    hands the synth a buffer of complex values. It copies them once and the
    synth cycles through them by `sym_read_idx`. It has no refill.

So neither call carries what `ofdm` needs: a refill delivers **bits** that
are then mapped and shaped, and `set_symbols` delivers one fixed buffer. An
OFDM source has bits in (the PSDU, per frame) and finished samples out, with
no `sps` and no pulse shaper between them.

### What `ofdm` provides, from that

- A `WFM_SYNTH_OFDM` branch whose input is the existing refill: one PSDU
    frame of `data_len` bits per pull, by the same `pull_new` and `pull_park`
    as a `bits` frame, so the data source (`data`, `data_from_file`, `fill`),
    its pacing and its state blob are reused unchanged.
- At each frame boundary the synth renders the PPDU's samples from the
    frame bits into a `cf32` buffer it owns, through the `wlan` renderer,
    and reads the buffer out through a sample cursor until it is spent.
    The PPDU length is a function of the configuration and `data_len`, so
    the buffer is sized once, at attach.
- **No shaper.** The rendered samples are the signal. The branch writes
    them where the shaper path writes its output and then takes the same
    tail: `wfm_mix_carrier` for a carrier, `wfm_synth_add_noise` for noise
    (`wfm_synth_core.c:846-851`). The common tail therefore applies
    unchanged, and `sps`, `pulse` and every `rrc_*` field are refused for
    `ofdm`.
- **A data source is a frame to the bridge, and `ofdm` is not a framable
    type.** `dp_wfm_source_has_frame` (`wfm_synth_bridge.c:99`) is true for a
    source with `data`, and `dp_wfm_framing_flags_error` (`:87`) then refuses
    any type outside `type_can_frame` (`:667`: `bits`, `bpsk`, `qpsk`, `pn`)
    with `WHY_CANNOT_FRAME`. `dsss` has an explicit carve-out at `:89`. An
    `ofdm` source with a `data` PSDU would be refused as it stands, so
    `ofdm` needs the same carve-out, in `dp_wfm_framing_flags_error` and in
    `dp_wfm_source_frame_error`, and an attach path. The reuse to try first
    is a description of one `data:LEN` field, which `pull_new` (`:827`)
    already accepts, so the PSDU rides the existing pull; PR 7 checks that
    before writing a second one. A frame description, `--sync`,
    `--crc` and `--acq-code` stay refused for `ofdm`: the PSDU is the
    payload and the PHY adds its own fields.

### State

What the blob carries today, from `dp_wfm_synth_get_state`
(`wfm_synth_core.c:602`): `wtype`, the symbol and bit cursors, the
continuous-DSSS chip and symbol counters, `data_ended`, a presence byte for
each child, the children's own blobs, and, when a refill is attached, the
frame in play (`n_bits` bytes, `:636-647`) followed by the data source's own
blob. `dp_wfm_synth_set_state` (`:647`) decodes every field into a local and
checks it before the first write, and a child or refill that refuses leaves
the synth as it was. The layout is version 6 (`wfm_synth_core.h:1468`); the
frame-in-play and the source position have travelled since version 3.

For `ofdm` that is almost all of it already:

- **The frame in play and the source position** travel as they do for a
    `bits` frame. The rendered samples do not travel. `set_state` re-renders
    the PPDU from the restored frame bits, which is sound only because `wlan`
    is a pure function of its configuration, the frame bits and the
    scrambler's initial state.
- **The scrambler's initial state is a render input.** A per-PPDU draw from
    `seed` (section 11) is reproducible on resume only if the draw is a
    function of the seed and a PPDU counter, and the counter travels. A draw
    from a running generator the blob does not hold would re-render a
    different PPDU for the same frame bits.
- **The sample cursor** is new, and is checked at `set_state` against the
    rendered length, as `sym_pos` is checked by `dp_wfm_synth_sym_pos_ok`
    (`:316`) before the kernel indexes a buffer with it.
- **Derived values are recomputed and checked, never trusted.** The
    continuous-DSSS symbol edge `next_edge` is not in the blob: `set_state`
    recomputes it from the chip and symbol counters and refuses a blob whose
    counters disagree (`dp_wfm_synth_clock_ok`, `:331`, installed at
    `:696-728`). The rendered buffer and its length are `ofdm`'s derived
    values and follow the same rule.
- **Reject keys**: the preset, the bandwidth, the constellation and code
    rate, and the window length are compared at `set_state`, because each
    sizes the PPDU or changes what the cursor means. The type itself
    already is one: `wtype` is in the blob and `wfm_synth_runnable` checks it.
- **The version rises to 7.** `make state-versions-check` fails a layout
    that reuses a version the project has shipped, so the bump is a gate and
    not a convention.

`ofdm` (the modulator) carries the run index, the symbol count within the
run, the polarity offset and the previous OFDM symbol's window tail (at most
`win` samples). Its blob holds those. The plan is configuration: a
`dp_hash64` of its canonical serialization is the reject key, so a blob never
restores into a different plan. Cost is in "The one shape".

`wlan` (the renderer) is stateless: a pure function of its configuration
and the PSDU. `scripts/check_serializable.py` requires every object in
`objects/*.toml` to either declare `serializable = "true"` or be listed in
`scripts/.serializable-stateless`, a text file of one object name per line
with `#` comments. `wlan` takes one line there, with the comment saying why.

### On the faces

Fields proposed on a source, all refused for any other type, in the one
validator every face asks (`dp_wfm_source_to_synth_error`,
`wfm_synth_bridge.c:339`, which calls `dp_wfm_scene_error`):

| field            | meaning                                                                                            |
| ---------------- | -------------------------------------------------------------------------------------------------- |
| `standard`       | `none` or a preset name; a preset fixes every field below that the standard fixes                  |
| `bandwidth`      | channel bandwidth in Hz; the segment's `fs` is an integer multiple of it, or the source is refused |
| `constellation`  | `bpsk`, `qpsk`, `qam16`, `qam64`, `qam256`, `qam1024`                                              |
| `code_rate`      | `1/2`, `2/3`, `3/4`, `5/6`; a pair Table 17-4 does not list is refused                             |
| `scrambler_init` | the 7-bit initial state; section 11                                                                |
| `plan`           | `standard = none` only: the plan of section 3, "The one shape"                                     |
| `mac`            | `none`, `fcs` or `data`: how the caller's octets become the PSDU (section 3)                       |
| `mac_header`     | `data` mode: the MAC header octets; a field form is section 11's                                   |
| `mcs`, `gi`      | HT and later only; they arrive with section 6's rows 4 and 5                                       |

**`constellation` is a second field, not an extension of `modulation`.**
`modulation` is `enum = "bitmod"` (`modules/wfm_compose.toml:312`), whose
values `none`, `bpsk`, `qpsk` are the C enum `wfm_bitmod_t`, the
`BITMOD_NAMES` table and `[[enum]] bitmod`, bound together by the gate
above, and it describes the mapping of a `bits` pattern (`json_when = { type = "bits" }`, help section BITS). Adding `qam16` to it would change a
`bits` source's surface and its C enum for the benefit of another type, and
`none` has no OFDM meaning. A new `[[enum]] constellation` is appended and
nothing existing moves.

**`json_when` does not need to name a set of types.** It names one field and
one value (`scripts/gen_wfm_defaults.py:579`, "json_when names ONE field"),
which is what the new field wants: `json_when = { type = "ofdm" }`. That
generator is this project's own, so extending `json_when` to a set would be a
change here, not a request to jm, and nothing proposed needs it. No upstream
jm feature is required by anything on this page.

**Spellings.** A throwaway project scaffolded through the pinned jm
(0.126.1) with `[[enum]] code_rate` and values `"1/2"`, `"2/3"`, `"3/4"`,
`"5/6"` on a writable property generated the lookup table of those strings,
the `.pyi` `Literal["1/2", "2/3", "3/4", "5/6"]`, and the refusal text
`invalid rate '%s' (choices: 1/2, 2/3, 3/4, 5/6)`: a "/" in an enum value
reaches the Python face unchanged. The CLI and JSON faces are generated
here, and already carry a non-identifier value (`blackman-harris`, in the
`psd_window` enum), which the schema lists verbatim and the help text prints
as "One of: ...". The recommended spellings are therefore the standard's own:
`code_rate` `1/2`, `2/3`, `3/4`, `5/6`; `standard` `802.11a`, `802.11g`,
`802.11p`, `802.11j`; `constellation` as above. What was not scaffolded is a
composer-kind module with these rows: that is the first thing PR 7 checks, by
running `scripts/gen_wfm_defaults.py --check` on the manifest before any C.

**What the bridge refuses**, each with the sentence every face gives:

- `standard`, `bandwidth`, `constellation`, `code_rate`, `scrambler_init`,
    `mac` and `mac_header` on any type but `ofdm`;
- an `ofdm` source whose `bandwidth` is not one the preset's clocking allows,
    whose `fs` is not an integer multiple of `bandwidth`, or whose
    `constellation` and `code_rate` are not a pair of Table 17-4;
- `sps`, `pulse`, `rrc_beta`, `rrc_span`, `symbols`, `symbol_rate`, the DSSS
    codes, a frame description, `--sync`, `--crc` and `--acq-code`, which
    describe a symbol-rate stream or a frame the PHY builds itself;
- a `data_len` that is not a whole number of octets, or outside the PSDU
    length the preset allows;
- a scrambler initial state of zero;
- `mac = data` without a `mac_header`, a `mac_header` in any other mode, a
    `mac = fcs` or `data` source whose octets are too few to hold a header, and
    a PSDU longer than the PHY's LENGTH field can describe.

```text
{
  "version": 1,
  "segments": [
    {
      "type": "ofdm", "fs": 20e6,
      "standard": "802.11a", "bandwidth": 20e6,
      "constellation": "qam16", "code_rate": "3/4",
      "data": "pn:800:15", "data_len": 800
    }
  ]
}
```

`standard`, `bandwidth`, `constellation` and `code_rate` are proposals;
every other key is today's. The caller's octets are the existing payload
data source, `data_len` bits per PPDU and one PPDU per frame, as a DSSS burst
is one burst per frame; `mac` says what they are (section 3).

### General OFDM, `standard = none`

Ruled 2026-11-16: the general shape is the API from the first slice, and the
802.11 presets are filled instances of it. `standard = none` takes a `plan`
(section 3, "The one shape", "The faces"); `standard = 802.11a` and the
other presets expand to the same `plan`. The bridge refuses:

- `plan` with a `standard` other than `none`, so a preset cannot be
    overridden by a field it fixes, and `plan` on any type but `ofdm`;
- a plan whose run names a class that does not exist, whose `role` or `tone`
    is not `n_fft` long, whose `useful` is zero past the default, whose fixed
    entry names a subcarrier outside `-n_fft/2 .. n_fft/2 - 1`, whose
    `n_phase` is zero, or whose `n_pol` is zero while `pol` is set;
- a plan with no symbol that takes data when the source supplies data, and a
    data source that supplies fewer points than the plan's data count.

The constellation points come from the source's own data path: the mapper
named by `constellation` over the payload bits, or, for `standard = none`,
the `symbols` type's stream.

## 6. The 802.11 flavors, in order

Each row says what it adds over the row above. Every row is one spatial
stream on one transmit chain (goal 8).

| #   | flavor (clause)                                  | adds                                                                                                                                                                                                   |
| --- | ------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 1   | non-HT OFDM, 20 MHz (17): 802.11a                | `N_FFT` 64, `Δf` 312.5 kHz, 48 data + 4 pilot subcarriers, GI 0.8 µs, L-STF, L-LTF, L-SIG, scrambler, BCC at 1/2, 2/3, 3/4, interleaver, BPSK to 64-QAM, eight rates                                   |
| 2   | ERP-OFDM (18): 802.11g                           | nothing at baseband; the same PPDU in the 2.4 GHz band, followed by a 6 µs signal extension of no transmission                                                                                         |
| 3   | 10 MHz and 5 MHz clocking (17): 802.11p, 802.11j | nothing but the sample rate; every duration doubles or quadruples                                                                                                                                      |
| 4   | HT, 20 MHz, MCS 0-7, 1 SS (19): 802.11n          | HT-mixed format: HT-SIG (two OFDM symbols, rotated BPSK, CRC-8), HT-STF, HT-LTF; 52 data subcarriers; rate 5/6; the 0.4 µs short GI; a 13-column interleaver; pilot values that rotate per OFDM symbol |
| 5   | HT, 40 MHz                                       | `N_FFT` 128, 108 data + 6 pilot subcarriers, the upper-half rotation by `j`, an 18-column interleaver, the legacy fields duplicated across both halves                                                 |
| 6   | HT-greenfield                                    | a second preamble: HT-GF-STF, HT-LTF1, HT-SIG, no legacy fields                                                                                                                                        |
| 7   | VHT, 20 to 160 MHz, 1 SS (21): 802.11ac          | VHT-SIG-A, VHT-STF, VHT-LTF, VHT-SIG-B; `N_FFT` 256 and 512; 256-QAM (MCS 8, 9); more than one BCC encoder at high rates; the segment parser at 160 MHz; a per-20 MHz phase rotation                   |
| 8   | LDPC (19.3.11.7)                                 | the second FEC; optional in HT and VHT, required by parts of HE                                                                                                                                        |
| 9   | HE SU, 1 SS (27): 802.11ax                       | `Δf` 78.125 kHz (four times the transform length for the same bandwidth), GI 0.8, 1.6 or 3.2 µs, RL-SIG, HE-SIG-A, HE-STF, HE-LTF in three sizes, packet extension, 1024-QAM (MCS 10, 11), DCM         |
| 10  | HE MU, HE TB, HE ER SU                           | OFDMA resource units, HE-SIG-B, the trigger-based preamble                                                                                                                                             |

Not proposed, named so they are not silently dropped: 80+80 MHz (two
non-contiguous segments), EHT (802.11be: 320 MHz, 4096-QAM), S1G
(802.11ah), TVHT (802.11af), the DMG and later millimeter-wave PHYs, and
the DSSS and CCK PHYs of 802.11b, which are not OFDM.

**The first slice is rows 1 to 3.** They are one PHY: the 10 MHz and 5 MHz
clockings and ERP-OFDM cost a sample rate and nothing else. Three reasons
it goes first:

1. Annex I is a complete worked example of exactly this PHY, so it is the
    one flavor that can be proven bit for bit.
1. Every later PPDU begins with these same L-STF, L-LTF and L-SIG fields,
    so nothing built here is set aside.
1. It needs no decision on LDPC or on a CRC other than the FCS's.

Rows 4 and 5 are the second slice; each later row is its own.

## 7. What the prototype shows

A throwaway numpy script, outside the tree, run against the installed
library's `PN`, `ConvEncoder`, `Interleaver` and `FFT`:

| check                                                                                | result                                                        |
| ------------------------------------------------------------------------------------ | ------------------------------------------------------------- |
| `pn` generates the 127-bit scrambler sequence of 17.3.5.5                            | yes: Galois, poly `0x48`, length 7, offset 4 chips            |
| every 7-bit initial state is some `pn` seed                                          | 127 of 127                                                    |
| `conv` with `{0133, 0171}` encodes the Annex I SIGNAL field (36 Mb/s, LENGTH 100)    | the 48 coded bits match                                       |
| `dp_interleave_index` with 3 rows, 16 columns is the first permutation               | the interleaved SIGNAL bits match                             |
| `fft`, inverse, 64 points, scaled by 1/64, gives the Annex I L-STF and L-LTF samples | within 0.0007 and 0.0003 of the printed three decimals        |
| L-STF and L-LTF average power                                                        | equal (0.0127 at the Annex's scale), so one PPDU scale factor |
| the Annex I PSDU, 96 octets as recalled, has the FCS the Annex prints                | CRC-32 gives `67 33 21 B6`                                    |
| `mpsk` on complemented bits gives the 802.11 BPSK and QPSK maps                      | all 2 and all 4 points match                                  |
| PAPR of one 64-QAM OFDM symbol, 4000 random draws, no window                         | median 6.6 dB, 99.9th percentile 10.1 dB                      |

A second prototype, also throwaway and outside the tree, drives one modulator
function with plans only (section 3, "The one shape"). It renders non-HT 20,
HT 20, HT 40, VHT 80, an HE 20 PPDU, an LTE-like slot and a DVB-T-like frame
and passes 34 structural checks: the forward transform returns each grid;
pilot positions and values follow their tables; the 40 and 80 MHz legacy
training fields are the rotated 20 MHz duplicates; symbol lengths and counts
come out (HE 272, 288 and 320 samples, an LTE slot of 15360, a DVB-T frame of
68 symbols of 2560). It verifies that the shape suffices, not that any
standard's number is right: the HT, VHT, HE, LTE and DVB-T tables in it are
stand-ins.

The reference values in these checks are recalled, not read from the text.
That they agree with an independent computation, and that the recalled
PSDU and FCS agree with each other through a CRC-32, is evidence the
recollection is right; it is not a substitute for the text.

## 8. How it is proven

The rule is that the anchor is the standard, never the implementation
under test and never a second implementation in the tree.

1. **Annex I, stage by stage.** Annex I encodes one 100-octet PSDU at
    36 Mb/s and prints every intermediate: the PSDU, the SIGNAL bits, their
    coded and interleaved forms, the scrambled, coded, punctured and
    interleaved Data bits, the subcarrier values and the time samples of
    each field. A C test holds those tables as literals **transcribed from
    the text** and compares each stage: bits exactly, samples to the
    printed precision. A table produced by running the implementation
    would encode the builder, so the transcription needs the text
    (section 11).
1. **The tables Annex I does not exercise, literal by literal.** Annex I
    covers one rate: 16-QAM at 3/4. The BPSK, QPSK and 64-QAM maps
    (Tables 17-8 to 17-11), the 2/3 puncture pattern (Figure 17-9), the
    RATE encodings (Table 17-6) and the timing constants (Table 17-5) are
    each small enough to pin entry by entry against the standard.
1. **The FCS.** The CRC-32 primitive is pinned by its published check value
    (the nine ASCII digits `123456789` give `CBF43926`) and by the Annex I
    PSDU's last four octets, which fix the byte order on the wire: `fcs`
    mode over the Annex I frame without its FCS must reproduce the printed
    PSDU exactly. Both values are from memory until read from the text.
1. **Structure, by inverting the primitive.** A forward `fft` of each
    OFDM symbol, cyclic prefix removed, returns the grid that went in:
    occupied subcarriers hold their constellation points and pilots, null
    subcarriers hold nothing. This inverts the modulator with an existing
    object; it is not a receiver.
1. **Transmit-quality measurements with the library's tools**: the power
    spectrum against the transmit mask (17.3.9.3), spectral flatness
    (17.3.9.7.3), relative constellation error (17.3.9.7.4) and PAPR. A
    clean generator passes these trivially at one sample per `1/bandwidth`
    with no window; they earn their place once the window and oversampling
    are in play, and they are the characterization's subject (section 9).
1. **State**: the shared round trip and the mid-stream split, split inside
    a window transition and inside a cyclic prefix.

**A decode loopback is not proof.** Demodulating with the library's own
`viterbi`, `dp_interleave` and `pn` passes when the generator and the
decoder share a defect: a swapped polynomial pair encodes and decodes
cleanly and matches no 802.11 receiver. It is a consistency check, useful
for lengths and puncture alignment at the rates Annex I does not print,
and it is not an anchor. The test harness has no rule against a
demodulator local to one test; the reason to keep it small is that an OFDM
demodulator is a library object with its own design page, not a helper.

**HT, VHT and HE have no printed waveform.** Annex I's examples are the
Clause 17 BCC PPDU and LDPC encodings. For the later formats the standard
gives structure to pin (field durations, subcarrier occupancy, SIG field
contents and CRCs) and no sample vector. What anchors them is unknown 9.6.

## 9. Unknowns

1. **The window's cost.** `T_TR` is informative in 17.3.2.5 (100 ns is its
    example). The EVM a given `T_TR` costs, and the mask margin it buys, are
    unmeasured.
1. **Whether a filter is needed.** Whether the window alone meets the
    transmit mask at each bandwidth and oversampling factor, or a `fir`
    stage is owed.
1. **PAPR per modulation, and the headroom a scene needs.** The prototype's
    one figure is section 7's; nothing is known for the preamble plus Data
    as a whole, per rate, or after the window.
1. **Speed.** No target is set. The cost is one `N_FFT`-point inverse
    transform per OFDM symbol plus the bit pipeline; whether the source
    renders faster than real time at 20 MHz, and at 160 MHz, on one core is
    unmeasured.
1. **What `snr_mode` means for `ofdm`.** `fs` is defined. `esno` needs a
    definition (per data subcarrier constellation point is the natural
    one, which differs from the in-band SNR by `N_ST / N_FFT` and the
    cyclic prefix), and `auto` needs a choice.
1. **The anchor for HT, VHT and HE.** A capture from a signal generator,
    another tool's output held as fixture data, or structure alone.
1. **The float32 error budget.** How far the rendered samples sit from
    the exact values, per `N_FFT`, and whether three decimals of Annex I
    is the tightest claim the test can make.
1. **The data source across PPDU lengths.** `data_len` fixes the PSDU
    length per frame today, and a refill never changes its frame length
    (`wfm_synth_core.h:164`); a scene with a different length per PPDU has no
    spelling.
1. **The gap between PPDUs.** A refilled `dsss` burst repeats back to back
    unless the scene gaps it. Whether an `ofdm` source plays PPDUs back to
    back, or idles between them for a time the standard defines, is
    unmeasured; the standard's interframe spaces are MAC timing, which
    section 2 puts out of scope.
1. **What the table-driven render costs.** The modulator's per-symbol pass
    over the DATA subcarriers and the phase's fixed entries is bounded by
    the transform; its fraction of a render, at 64 and at 4096 points, is
    unmeasured.
1. **Whether the one-field description reuses the pull.** Section 5 proposes
    a `data:LEN` description so the PSDU rides `pull_new`; it is read from the
    code and not yet tried.

## 10. Standard-fixed numbers, none verified against the text

Every number below is from memory. Those marked *(cross-checked)* agree
with section 7's independent computation.

| number                                                                                                                                                                                        | cited as                         |
| --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------- |
| Clause numbers: OFDM 17, ERP 18, HT 19, VHT 21, HE 27                                                                                                                                         | 802.11-2020, 802.11ax-2021       |
| The worked example is Annex I                                                                                                                                                                 | 802.11-2020                      |
| `N_FFT` 64, `Δf` 312.5 kHz, `N_SD` 48, `N_SP` 4; GI 0.8 µs, OFDM symbol 4 µs; L-STF 8 µs, L-LTF 8 µs                                                                                          | Table 17-5                       |
| pilots on subcarriers -21, -7, 7, 21 with values 1, 1, 1, -1                                                                                                                                  | 17.3.5.9, 17.3.5.10              |
| eight rates and their modulation and coding pairs; RATE bits                                                                                                                                  | Tables 17-4, 17-6                |
| scrambler `x^7 + x^4 + 1` and its 127-bit sequence *(cross-checked)*                                                                                                                          | 17.3.5.5                         |
| BCC generators 133 and 171 octal, output A first *(cross-checked at 1/2)*                                                                                                                     | 17.3.5.6                         |
| puncture patterns for 2/3 and 3/4                                                                                                                                                             | Figure 17-9                      |
| interleaver permutations, 16 columns *(first cross-checked)*                                                                                                                                  | 17.3.5.7                         |
| constellation maps and `K_MOD` (1, 1/√2, 1/√10, 1/√42)                                                                                                                                        | Tables 17-7 to 17-11             |
| L-STF and L-LTF sequences, the √(13/6) factor *(cross-checked)*                                                                                                                               | 17.3.3                           |
| window, `T_TR` 100 ns as an example                                                                                                                                                           | 17.3.2.5                         |
| transmit mask corner points; flatness limits; relative constellation error limits                                                                                                             | 17.3.9.3, 17.3.9.7               |
| Annex I example: 100 octets, 36 Mb/s, scrambler initial state 1011101 *(PSDU cross-checked)*                                                                                                  | Annex I                          |
| ERP-OFDM signal extension, 6 µs                                                                                                                                                               | Clause 18                        |
| MAC frame format: header, body, FCS; the 24-octet three-address data header (Frame Control, Duration/ID, three addresses, Sequence Control); QoS Control and HT Control as optional additions | Clause 9                         |
| FCS: CRC-32, generator polynomial, initial value, final complement and byte order on the wire; the check value `CBF43926` for `123456789`                                                     | Clause 9 (FCS field); IEEE 802.3 |
| L-SIG LENGTH field width and the PSDU length it limits                                                                                                                                        | 17.3.4                           |
| HT: 52 + 4 subcarriers at 20 MHz, 108 + 6 at 40 MHz, 13 and 18 interleaver columns, short GI 0.4 µs, rate 5/6, HT-SIG CRC-8                                                                   | Clause 19                        |
| VHT: `N_FFT` 256 and 512, 234 + 8 subcarriers at 80 MHz, 256-QAM at MCS 8 and 9                                                                                                               | Clause 21                        |
| HE: `Δf` 78.125 kHz, GI 0.8 / 1.6 / 3.2 µs, 1024-QAM at MCS 10 and 11                                                                                                                         | Clause 27                        |
| `K_MOD` for 256-QAM and 1024-QAM (1/√170, 1/√682)                                                                                                                                             | Clauses 21, 27                   |

## 11. Decisions

### Ruled

On 2026-11-16 the owner agreed to this page's recommendations, with two
exceptions (9 and 10), and made a further ruling (11). Each is stated design now, and the rest of the page is written
as if it were so.

1. **The name of the 802.11 component is `wlan`.** A C identifier cannot
    begin with a digit, "Wi-Fi" is a certification mark rather than the
    standard's word, and "wireless LAN" is the standard's own title. On the
    faces the preset is spelled as users say it: `802.11a`, `802.11g`,
    `802.11p`, `802.11j`, `802.11n`.
1. **One type, `ofdm`, with a `standard` discriminator**, as `dsss` has
    `symbol_rate`, rather than a `wlan` type beside it.
1. **Both an object and a type.** The modulator, the QAM map, the CRC-32
    primitive and the renderer are certified as objects first; the `ofdm`
    type is the composition.
1. **The first slice is Clause 17 non-HT** at 20, 10 and 5 MHz: section 6's
    rows 1 to 3.
1. **LDPC has its own design page**, ahead of HE and after VHT.
1. **A non-HT rate is spelled by the two columns of Table 17-4 that do not
    change with clocking**: a constellation and `code_rate`. Mb/s names a
    different pair at 10 MHz than at 20 MHz. What the constellation field is
    called is open (below).
1. **The scrambler's initial state is drawn per PPDU** from the source's
    `seed` through the composer's draw hash, so it is reproducible and read
    back, with `scrambler_init` to fix it (the Annex I test fixes it). The
    draw is a function of the seed and a PPDU counter that travels in the
    state blob, or a restored synth re-renders a different PPDU (section 5,
    State).
1. **`qam` lives beside `mpsk`**, in the same module on the Python face.
1. **A minimal MAC, including the FCS.** This replaces the recommendation of
    none. The scope is section 3's "The minimal MAC": an MPDU from the
    caller's header and body with its FCS, or the caller's octets as the
    whole PSDU. Nothing beyond it.
1. **No MIMO.** Out of scope, not deferred (goal 8): it is not in the
    roadmap, and the ordered rows end at single-stream HE.
1. **The general shape is the API from the first slice.** The owner's call
    on 2026-11-16, replacing the recommendation to ship `standard` only: the
    `ofdm` modulator takes a plan, and the 802.11 presets are filled
    instances of it (section 3, "The one shape").

### Open

1. **The standard's text.** There is none in `~/refs`. Section 8's first two
    proofs need Annex I and the Clause 17 tables transcribed from it, and
    every number in section 10, including the MAC frame format and the FCS,
    needs checking against it.
1. **What anchors HT, VHT and HE.** Unknown 6 in section 9: no printed
    waveform exists for them.
1. **A second field for the constellation.** Recommended: a new
    `constellation` field and `[[enum]] constellation`, appended, not qam
    values added to `modulation`, whose enum `bitmod` is bound to a C enum, a
    name table and the `bits` type (section 5, On the faces). The cost is
    two spellings for a mapping: `modulation` on `bits`, `constellation` on
    `ofdm`.
1. **The spellings.** Recommended: the standard's own, `1/2`, `2/3`, `3/4`,
    `5/6` and `802.11a`; section 5 shows what was scaffolded and what was
    not.
1. **A parametric CRC rather than a CRC-32 primitive.** Recommended: one
    CRC primitive with width, polynomial, initial value, final complement
    and bit order as parameters, serving the FCS (CRC-32) and the SIG
    fields (CRC-8), with `dp_crc16.h`'s CCITT as an instance. Section 3's
    "The one shape" gives the reason. The cost is a slightly larger first
    PR than a CRC-32 alone.
1. **The form of the MAC header in the first slice.** Recommended: raw
    octets (`mac_header`) only. The field form (Frame Control, Duration/ID,
    three addresses, Sequence Control) needs a six-octet address spelling no
    source field has, and is sugar over the same 24 octets, so it can follow
    the first slice without moving anything. The alternative declares both
    at once.

## 12. The work, in order

Sizes are rough and count hand-written C and tests, not generated files.

| PR  | what                                                                                       | phases                                    | size        |
| --- | ------------------------------------------------------------------------------------------ | ----------------------------------------- | ----------- |
| 1   | this page                                                                                  | why                                       | this page   |
| 2   | puncturing in `conv`, erasure insertion ahead of `Viterbi`; the `conv` report's new claims | declare to certify                        | ~400 lines  |
| 3   | the CRC primitive (parametric, or CRC-32 alone: section 11) and its pins                   | implement, pin                            | ~200 lines  |
| 4   | `qam`: map and hard demap, 16 to 1024 points                                               | declare to user story                     | ~400 lines  |
| 5   | `ofdm`: the plan-driven modulator, its state triplet, bench                                | declare, implement, pin, bind, instrument | ~1100 lines |
| 6   | `wlan`: plan and chain builders for non-HT, the minimal MAC, the Annex I test              | declare, implement, pin, bind             | ~1400 lines |
| 7   | `wfm_synth` type `ofdm`: enum, source fields, `PlanDesc`, bridge, faces, capture, state    | all of section 5                          | ~1300 lines |
| 8   | characterization: window, mask, EVM, PAPR, speed; the reports for `ofdm` and `wlan`        | explore, certify                          | ~800 lines  |
| 9   | guide section, C and Python examples, gallery decision, benches                            | user story, land                          | ~500 lines  |
| 10+ | HT 20 MHz; HT 40 MHz; VHT; LDPC; HE SU, each its own slice                                 |                                           |             |

PRs 2, 3 and 4 are independent of one another and of 5. PR 6 needs 2, 3, 4
and 5. Each pin in PRs 2 to 7 is proven by sabotage before the Python face
exists.

**What PR 2 touches.** The objects are `objects/conv_enc.toml`
(`ConvEncoder`, `serializable = "true"`, `depends_on` `conv` with
`link = true`, a generated fragment) and `objects/viterbi.toml`
(`serializable = "true"`); the code description is `conv_core.h`'s
`conv_code_t`, which has no `puncture` member. The gates a pattern reaches:

- the state rules: a pattern chosen at `create()` is a reject key; a pattern
    a setter can change travels in the blob, and
    `src/doppler/tests/test_mutator_state.py` fails on a mutator whose value
    does not, with the shrink-only list `scripts/.mutator-state-exempt`;
    `make state-versions-check` fails a blob layout that reuses a shipped
    version;
- the manifest: `make drift-check` after any edit to the two `objects/`
    files, since both fragments are jm-generated;
- the report: `src/doppler/tests/validation/conv/validate.py` and the
    `validation-report-check` and `characterization-check` gates in
    `make lint`, which hold a changed claim to a regenerated report;
- the headers: `doxygen-check` and `doc-symbols-check`, since a pattern adds
    public symbols the docs will name.

**What PR 7 checks first.** Before any C, `scripts/gen_wfm_defaults.py --check` on the new manifest rows, which is the generator that has not yet
seen a `/` in a choice or a second type-conditional field; then the one-field
description of section 5.

## Related

- [wfmgen](wfmgen.md): the tool this is a waveform type of
- [One Home for the Waveform Enum Tables](waveform-enum-ssot.md): what
    adding a tenth type touches
- [A Frame as a Description](frame-description.md) and
    [Payload Data Source](payload-data-source.md): where the PSDU comes
    from
- [Viterbi](viterbi.md) and [Interleaving](interleaving.md): the coding
    objects the BCC and the interleaver reuse
- [State Serialization](state-serialization.md): the blob rules section 5
    applies
- [Waveform Amplitude & Composition](wfmgen-composition.md): the level
    convention, and where PAPR meets headroom
- [Adding an algorithm](../dev/contributing/adding-algorithms.md): the
    lifecycle this page opens
