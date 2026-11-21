---
description: "The IEEE 802.11 OFDM PHYs as presets over the general OFDM modulator, proposed: the minimal MAC, the coding chain, which existing objects each block composes, how a preset is proven once the standard's text is in hand, and wfmgen's ofdm waveform type"
---

# IEEE 802.11 OFDM PHYs, and wfmgen's `ofdm` type

*Part of [wfmgen](wfmgen.md), which owns the tool's goals, what it
composes, and what is still unknown about it. The modulator this page calls
is [OFDM: a general modulator](ofdm.md), which owns the layout, its struct,
its state and its proofs.*

!!! note "Status"

    The 802.11 non-HT builders are built: `wlan` (C and Python) on the
    general modulator `ofdm` gives the rate table, the minimal MAC, the
    coding chain, the layout and the PPDU. Not built: wfmgen's `ofdm`
    waveform type (section 5), the HT, VHT and HE flavors, and Annex I's
    stage-by-stage conformance test. "What exists, and what does not" in
    section 4 says which blocks have a home today. Section 10 lists the
    decisions, ruled and open; sections 8 and 9 list what is not known.

!!! warning "No preset claims conformance yet"

    The general shape is the deliverable. Every clause, table and figure
    number below refers to IEEE Std 802.11-2024 unless it says otherwise.
    Section 9 lists each standard-fixed number and says whether it is
    **checked against that text**; the rest are unchecked. No test, header,
    document or changelog line says a preset "conforms to" 802.11a, g, n or
    ac, or cites Annex I as passed, until section 7's proofs run against
    tables transcribed from the text. A builder labels as unverified any
    number section 9 does not mark checked.

wfmgen gains one waveform type, `ofdm`. It is a caller of the general OFDM
modulator; the IEEE 802.11 OFDM PHYs are layouts that `wlan` builds for it.
The scope is the PHY plus a minimal MAC wrap (section 3): the caller
supplies a frame body, or the whole PSDU, or a PN fill stands in for one, and
the generator produces the PPDU's baseband samples at the standard's sample
rate. It builds no management or control frame, aggregates nothing, and runs
one spatial stream (section 2).

## Vocabulary

The signal terms (subcarrier, OFDM symbol, constellation point, grid, cyclic
prefix, window, subcarrier map, fixed entry, class, run, **layout**) are
defined once on [the modulator's page](ofdm.md) and used in that sense here.
Two terms are worth repeating, because both words name something else
nearby:

- **layout**: the modulator's description of what is transmitted. It is not
    wfmgen's `Plan` (`doppler.wfm.Plan`, the cached rendering of a scene).
- **transform plan**: the FFT object's tables for one length, and nothing
    else.

Field and parameter names are spelled as the standard spells them.

### The 802.11 PPDU

- **guard interval**, GI: the standard's name for the cyclic prefix, stated
    as a duration (`T_GI`). The 802.11 layer says GI; the modulator says
    cyclic prefix; they are the same samples.
- **subcarrier spacing**, `Δf`: the channel bandwidth divided by `N_FFT`.
- **pilot polarity sequence**: the 127-element sequence `p_n` that scales
    every pilot of OFDM symbol `n` (17.3.5.10). It is the modulator's
    polarity sequence.
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
    or later PPDU. Every PPDU here has one (goal 7).
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
| A C or Python application     | `wlan`'s builders and the modulator directly | renders PPDUs into its own buffer, no wfmgen in the process |
| Someone building another OFDM | `--type ofdm` with their own layout          | a non-802.11 OFDM signal with the same engine               |

## 2. Design goals

1. **The standard rules.** Every 802.11 number comes from the standard's
    text and is cited where it is written into code. A preset has no
    tunable that the standard fixes. A number section 9 does not mark
    checked is labeled unverified where it is written, and the preset
    claims no conformance (Ruled, section 10).
1. **A preset is a layout.** `wlan` builds layouts; the modulator is the
    general one of [ofdm](ofdm.md) and branches on no standard.
1. **Nothing is implemented twice.** Each PHY block calls the object that
    already owns it (section 4). What is absent is added as a general
    primitive where a second user is plausible, and only otherwise inside
    the 802.11 component.
1. **Bit-exact where the standard gives bits, to the printed precision
    where it gives samples**, against the standard's own example, when the
    text is in hand (section 7). Annex I is the anchor *when it is read*; it
    is not a claim of the first slice.
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
    2026-11-16.

## 3. The layers

```text
FACES      wfmgen --type ofdm · doppler.wfm · JSON scene
             └── fill in wfm_source_t; hold no logic

SOURCE     wfm_synth, type = ofdm
             ├── standard = none ── the caller's layout
             └── standard = 802.11x ── wlan PPDU renderer
                                         │
PRESETS    wlan ── PSDU bits -> a layout, and the PPDU's constellation points
             ├── fields: L-STF, L-LTF, L-SIG, Data (then HT-, VHT-, HE-)
             ├── MAC wrap ── header octets · FCS ── parametric CRC
             ├── scrambler ── pn
             ├── FEC ── conv, with puncturing
             ├── interleaver ── dp_interleave + the 802.11 second step
             ├── mapper ── qam · mpsk
             └── layout ── to the modulator
                           │
PRIMITIVE  ofdm ── constellation points and a layout in, samples out
                   (the modulator: its own page)
```

`wlan` is new. It is a table of PPDU formats, and a builder from a format
and a PSDU to a layout and the constellation points the layout takes,
including the minimal MAC wrap below. Every other block it calls exists or
is built (section 4).

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
    is section 10's.
- **The FCS** is the library's parametric CRC with the FCS's
    parameters (the catalogue's CRC-32/ISO-HDLC is the candidate; the
    standard's text decides), over every octet before it, appended in the
    byte order the standard defines (Clause 9; section 9).
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

### The coding chain

The coding chain is a record per PPDU format and rate, and `wlan` is where it
lives. The table is what it must hold for a to ac with one spatial stream;
"fits" means the shape of [ofdm](ofdm.md) and the library's objects carry it.

| parameter                       | a to ac, one spatial stream                                                                                                                     | fits?                                                                   |
| ------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------- |
| scrambler                       | `x^7 + x^4 + 1`, initial state per PPDU, the same in every flavor                                                                               | fits; one polynomial                                                    |
| BCC encoders                    | one, or more at high rates (HT above 300 Mb/s, VHT 160 MHz); the bits are dealt to the encoders in turn                                         | fits: `n_es`, one parser loop                                           |
| puncture pattern                | 1/2, 2/3, 3/4, 5/6, the same in every flavor                                                                                                    | fits: a pattern table of `conv_code_t` fields                           |
| interleaver                     | columns 16, 13, 18, 26 for 20 MHz non-HT, HT/VHT 20, 40, 80; rows `N_SD * N_BPSC / columns`                                                     | fits: one first permutation, `rows = N_CBPS / columns`                  |
| interleaver, second permutation | `s = max(N_BPSC / 2, 1)`, the same everywhere                                                                                                   | fits                                                                    |
| interleaver, third permutation  | per-stream rotation, only for more than one stream                                                                                              | out of scope (goal 7)                                                   |
| segment parser                  | none to 80 MHz; two 80 MHz segments at 160                                                                                                      | fits: `n_seg`, each segment interleaved on its own                      |
| constellation                   | BPSK to 256-QAM (1024 for HE); Q-BPSK is BPSK rotated 90 degrees                                                                                | fits: a per-OFDM-symbol rotation on the mapper                          |
| SIG fields                      | L-SIG 24 bits with parity; HT-SIG and VHT-SIG-A 48 bits, CRC-8, two OFDM symbols; VHT-SIG-B 26 to 29 bits, CRC-8, tiled to fill the subcarriers | the coding fits; the bit layouts are `FrameDesc` fields and a CRC stage |

**Why 802.11ax is outside the promise.** It is not outside the layout: the
HE column of [ofdm](ofdm.md)'s table fits. It is outside the promise because
its preset needs the resource-unit layouts and the DCM and dual-carrier
mappings, which are tables the first slice does not build.

**What does not fit the modulator, by design**, and is therefore `wlan`'s:

- **Arithmetic on field contents**: the L-SIG LENGTH of a mixed-format
    PPDU (computed from the PPDU's duration so legacy stations defer), the VHT
    short-GI symbol-count disambiguation bit, and the symbol, tail and pad count
    per `n_es`. These are per-flavor rules, written once each in `wlan`, from
    the text.
- **Training sequence tables** for each flavor and width. They are data.
- **The CRCs.** The SIG fields use CRC-8 and the FCS a CRC-32: both are
    instances of the one parametric CRC.

### The default window

A caller who gives no window gets the standard's 100 ns transition
(17.3.2.5), `2 * M` samples at the output rate; `window = 0` is the explicit
rectangular choice, and any other value is a count of samples at the output
rate, 0 to `80 * M`. The default must scale with `M` while `window` is a
count, so the constructor's argument has one reserved value,
`WLAN_WINDOW_STANDARD` (`UINT32_MAX`), and one C function,
`dp_wlan_window_resolve (window, M)`, turns it into `2 * M` (any other value
comes back as it was). `dp_wlan_create` and `dp_wlan_params_why` call it, so
the state holds the resolved count (`Wlan.window` reads `2 * M` for a
default caller) and the range check and its sentence (`window must be 0 to 80 * oversample`) run on the count, never the sentinel. The free functions
take counts: `dp_wlan_layout` and `dp_wlan_ppdu` refuse the sentinel as above
`80 * M`, and `dp_wlan_ppdu_len` is a pure sum whose caller resolves first;
the function is public for a caller who wants the default there.

Two other spellings were weighed and not built: a separate transition-time
argument (a fifth argument on every C caller, a rounding rule for times that
are not whole samples, a conflict rule when both are given) and a default
each face fills in itself (the same default in three places that can
drift).

**What the Python face shows.** The manifest declares the default as
`default_raw = "WLAN_WINDOW_STANDARD"`, so the stub reads
`window: int = ...` and the docstring `window : int, default WLAN_WINDOW_STANDARD`; the binding starts at the constant and passes it to
create. An omitted keyword is the standard's window and `window=0` is the
rectangular one, by asking.

**What a default caller sees.** The symbol boundaries are cross-faded over
`2 * M` samples and the PPDU is `2 * M` samples longer (`dp_wlan_ppdu_len`
adds the window); at `M = 1` that is 2 samples. The transmit mask passes
every point by at least 7.9 dB at `M = 4` (finding
`default_window_fails_mask` in
`src/doppler/wlan/tests/validation/wlan/results.md`), where the rectangular
window fails the 30 MHz point. The tests that pin `M = 1` bit-identity
against a committed PPDU state `window = 0`.

**The wfmgen face.** `wfm_source_t.window` is a `size_t` whose manifest
default is a named constant, `WFM_WINDOW_UNSET` (`SIZE_MAX`): a source a face
builds starts unset, and a C caller that zero-initializes a source gets `0`,
the rectangular window, by its own spelling. Unset is told from 0 by value
on every face:

| face                                  | unset                            | explicit rectangular |
| ------------------------------------- | -------------------------------- | -------------------- |
| struct                                | `WFM_WINDOW_UNSET` (the default) | `0`                  |
| CLI                                   | `--window` omitted               | `--window 0`         |
| scene JSON                            | no `window` key                  | `"window": 0`        |
| Python `Segment`, `Synth`, `Composer` | `window` omitted                 | `window=0`           |

`dp_wfm_source_window (src, fs)` is the one place the count is decided: the
held count, or for an unset window `dp_wlan_window_standard` (the rule
`dp_wlan_window_resolve` applies, an inline so wfmgen's draw library shares
it without linking the DSP) at the oversampling `dp_wfm_source_oversample` derives (`fs` over the bandwidth,
the one derivation the renderer's geometry shares). When `fs` has no
oversampling (no whole multiple of the bandwidth, or above 128) the unset
value comes back, which `dp_wfm_ofdm_fs_error` refuses by its own sentence;
a record of such a scene writes no `window` key, and the scene cannot
compose. The renderer, the scene writer and the SigMF annotation each call
it, so a scene written by `Composer` and `wfmgen:window` hold the resolved
count, never the absence: a record then renders the same samples under any
later default. A scene a
user wrote without `window` is unset and takes the default at load. The
range check and its sentence (`window is 0 to 80 times fs over bandwidth`)
run on the resolved count, never the sentinel. The scene writer's rule "not
written at its default or at zero" keeps the zero for this row
(`json_keep_zero`, set by the generator for a default that names a
constant): `"window": 0` is a value, and a record without it would read
back as the default. A window on another type is refused as before, and the
unset one is not a window. The Python face holds the field as it is: a
source's `window` attribute reads back `2**64 - 1` while unset, because a
getter has no `fs` to resolve against and jm renders none for a source
field; the resolved count is in `to_json()`, `to_sigmf()` and the samples.
The docstring names the constant, `WFM_WINDOW_UNSET`, which is the C
spelling of that number. The tests are
`test_the_default_window_is_the_standards_100_ns` (C and Python) and the two
record tests in `src/doppler/wfm/tests/test_ofdm_source.py`.

## 4. What each 802.11 PHY block composes

### What exists, and what does not

"Exists" means a caller reaches it today through a public C symbol.

| PHY block (clause)                     | state                    | where                                                                                   | what is missing                                                                                      |
| -------------------------------------- | ------------------------ | --------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------- |
| Inverse transform (17.3.2.6)           | exists                   | `fft`: `dp_fft_create`, `dp_fft_execute_cf32`                                           | nothing; any length, unnormalized, plan reused across calls                                          |
| Scrambler, `x^7 + x^4 + 1` (17.3.5.5)  | exists                   | `pn`; `dp_wlan_scrambler_state_from_sequence`, `dp_wlan_scrambler_sequence`             | nothing                                                                                              |
| Pilot polarity `p_n` (17.3.5.10)       | exists                   | the same `pn` sequence, all-ones start                                                  | nothing                                                                                              |
| BCC, K = 7, 133/171 octal (17.3.5.6)   | exists                   | `conv_code_t` `{0133, 0171}`; `ConvEncoder` (`conv_enc`)                                | nothing for rate 1/2                                                                                 |
| Puncturing to 2/3, 3/4, 5/6            | exists                   | `dp_wlan_bcc` sets `punct_period`, `punct_mask` in `conv_code_t`; `conv_enc`, `viterbi` | 5/6 is Clause 19's and is not checked against the text; a decoder reads 0.0 at each deleted position |
| Interleaver, first permutation         | exists                   | `dp_interleave_index (k, N_CBPS/16, 16)`                                                | nothing                                                                                              |
| Interleaver, second permutation        | exists                   | `dp_wlan_interleave`, with `perm2` in `dp_wlan_tables_standard()`                       | nothing                                                                                              |
| BPSK, QPSK map (Tables 17-15, 17-16)   | exists                   | `dp_wlan_map`, with the standard's labels in `dp_wlan_tables_standard()`                | nothing; see the note below                                                                          |
| 16/64/256/1024-QAM map                 | exists                   | `dp_wlan_map`; `qam_map` of the `mpsk` module (#3115) for the points                    | the 256 and 1024 labels are Clause 19's and are not checked                                          |
| Subcarrier map, pilots, cyclic prefix  | exists                   | `ofdm`, [the modulator](ofdm.md); `dp_wlan_layout`                                      | nothing                                                                                              |
| Window (17.3.2.5)                      | exists                   | `ofdm`                                                                                  | nothing                                                                                              |
| L-STF, L-LTF sequences (17.3.3)        | exists                   | `stf`, `ltf` and their scales in `dp_wlan_tables_standard()`                            | nothing                                                                                              |
| L-SIG: RATE, LENGTH, parity (17.3.4)   | exists                   | `dp_wlan_signal_bits`                                                                   | nothing                                                                                              |
| SERVICE, tail, pad (17.3.5.2-4)        | exists                   | `dp_wlan_data_bits`                                                                     | nothing                                                                                              |
| HT-SIG CRC-8, VHT-SIG-B, HE-SIG CRC    | built (#3116)            | the parametric CRC header, beside `dp_crc16.h`                                          | the parameters of each, from the text                                                                |
| MAC header, frame body, FCS (Clause 9) | built (CRC); rest absent | the parametric CRC for the FCS                                                          | field assembly in `wlan`; the minimal MAC, section 3                                                 |
| LDPC (19.3.11.7)                       | absent                   |                                                                                         | the codec; a design page of its own                                                                  |
| Oversampling by an integer             | exists                   | `fft` at `M * N_FFT` with a zero-padded grid                                            | nothing                                                                                              |
| Rate change by a non-integer           | exists                   | `RateConverter`, `Resampler`                                                            | nothing; applied by the caller, not inside the source                                                |
| Spectral shaping filter                | exists                   | `fir`                                                                                   | whether it is needed at all is unknown 8.2                                                           |
| Carrier offset, AWGN, level, Doppler   | exists                   | `wfm_synth` (`lo`, `awgn`), `wfm_compose`, `doppler_channel`                            | nothing; the common tail every source already has                                                    |
| PSDU from a data source or a PN fill   | exists                   | the payload data source (`data`, `data_len`, `fill`)                                    | nothing                                                                                              |
| Power spectrum, band power             | exists                   | `psd`: `dp_psd_band_power`, `dp_psd_occupied_bw`                                        | a transmit-mask comparison over those bands                                                          |
| PAPR                                   | exists, real only        | `dp_tonemeas_time_stats`                                                                | a complex-envelope PAPR                                                                              |
| EVM of an OFDM grid                    | absent                   |                                                                                         | relative constellation error per 17.3.9.7.4                                                          |

Two rows carry a decision rather than a gap:

- **BPSK and QPSK.** `mpsk` sends BPSK bit `b` as `1 - 2b` (bit 0 is +1,
    `mpsk_core.h`), and its QPSK is Gray-labeled with the first point at
    pi/4; the QAM map at M = 4 is that same QPSK. Tables 17-15 and 17-16
    are read from the text (Figure 17-10): BPSK's first bit 0 is -1, the
    complement of `mpsk`'s `1 - 2b`, and QPSK's I and Q each take their own
    bit, so `mpsk`'s QPSK is a relabel of them, `[3, 2, 1, 0]` by bit-group
    value. The same holds for 16-QAM and 64-QAM against
    `qam`'s derived map: `wlan` owns the relabeling, a table of 16 and 64
    entries, and `qam` is not changed to carry a standard.
- **The scrambler seed.** `pn` reproduces the sequence, but at a phase
    offset from the standard's (four chips for the all-ones state), so the
    standard's initial state is not the `pn` seed and neither is its bit
    reverse. All 127 states are reachable by some seed. The conversion is a
    derivation, written once in `wlan` and pinned against Annex I's
    scrambled bits when the text is in hand.

## 5. wfmgen's `ofdm` type

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
(`just-makeit.toml`), in `TYPE_NAMES` with `N_TYPES` raised to 10
(`wfm_names.h`), and the C constant `WFM_SYNTH_FRAMES = 9` beside
`WFM_SYNTH_DSSS = 8` (`wfm_synth_core.h`): the constant is named for what the
branch is, and the `ofdm` face type selects it. List order is the C value, so the name is appended,
never inserted. At the head that adds the synth, `N_TYPES` is 9 and only the C
constant exists: the name, the manifest entry and the faces arrive with the
bridge. `scripts/check_wfm_enum_tables.py`, run as
`make lint-wfm-enum-tables`, holds the three to one another: each name
table has one C home and equals its manifest `values` in order, every table
carries an `SSOT:` annotation, the `count=` macro equals the table length,
and the Nth enumerator of a named C enum has the explicit value N. A name
added in one place and not the others turns it red.

### What `dsss` does, which `ofdm` cannot copy

The synth does not render a DSSS burst. The bridge assembles it:
`dp_wfm_source_attach_dsss` builds the chips from
the frame description and hands them to `dp_wfm_synth_set_dsss_chips`
, which stores them as the synth's `bits` pattern.
From there a burst is played by the bit cursor and shaped by the same
`sps` and `pulse` machinery as any bit pattern. With a data source, one
burst is one frame pulled on demand:

- `dp_wfm_synth_set_refill` installs a callback of
    type `wfm_synth_refill_fn` (`wfm_synth_core.h:164`), `(user, bits, n)`.
    At the frame boundary the synth calls it to write the next frame's `n`
    bits into `bits` in place. **The frame length never changes**, and a
    refill is accepted only by a `bits` or `dsss` synth.
- The bridge's callback is `data_pull_refill`;
    `pull_park` parks the cursor at the end of a placeholder frame,
    so nothing is drawn until the first bit is due. A composer can build a
    source only to validate it without reading a byte of the data source.
- `dp_wfm_synth_set_symbols` is the only call that
    hands the synth a buffer of complex values. It copies them once and the
    synth cycles through them by `sym_read_idx`. It has no refill.

So neither call carries what `ofdm` needs: a refill delivers **bits** that
are then mapped and shaped, and `set_symbols` delivers one fixed buffer. An
OFDM source has bits in (the PSDU, per frame) and finished samples out, with
no `sps` and no pulse shaper between them.

### What `ofdm` provides, from that

- `WFM_SYNTH_FRAMES` (the `ofdm` type) is a synth type with a frame callback installed
    (section 5, "The frame callback"). Its input is the
    existing bit refill: one PSDU frame of `data_len` bits per pull, by the
    same `pull_new` and `pull_park` as a `bits` frame, so the data source
    (`data`, `data_from_file`, `fill`), its pacing and its state blob are
    reused unchanged. The synth renders nothing itself: at each wrap it hands
    the frame's bits to the callback, which the bridge installs and which
    builds the PPDU through the `wlan` builder and the modulator. The PPDU
    length is a function of the configuration and `data_len`, so the frame
    buffer is sized once, at attach.
- **No shaper.** The rendered samples are the signal. The type plays them as
    they are and then takes the same tail: `wfm_mix_carrier` for a carrier, `wfm_synth_add_noise` for
    noise. The common tail therefore applies unchanged, and `sps`, `pulse` and every `rrc_*` field are refused for
    `ofdm`.
- **A data source is a frame to the bridge, and `ofdm` is not a framable
    type.** `dp_wfm_source_has_frame` is true for a
    source with `data`, and `dp_wfm_framing_flags_error` then refuses
    any type outside `type_can_frame` (`bits`, `bpsk`, `qpsk`, `pn`)
    with `WHY_CANNOT_FRAME`. `dsss` has an explicit carve-out. An
    `ofdm` source with a `data` PSDU would be refused as it stands, so
    `ofdm` needs the same carve-out, in `dp_wfm_framing_flags_error` and in
    `dp_wfm_source_frame_error`, and an attach path. The reuse to try first
    is a description of one `data:LEN` field, which `pull_new`
    already accepts, so the PSDU rides the existing pull; the type's PR
    checks that before writing a second one. A frame description, `--sync`,
    `--crc` and `--acq-code` stay refused for `ofdm`: the PSDU is the
    payload and the PHY adds its own fields.

### State

What the blob carries, from `dp_wfm_synth_get_state`
:   `wtype`, the symbol and bit cursors, the
    continuous-DSSS chip and symbol counters, `data_ended`, for a frames synth
    the count of frames rendered, the `refused` flag and the frame's length and
    bit count, a presence byte for each child and for the renderer's triplet,
    the children's own blobs, and, when a refill is attached, the frame in play
    (`n_bits` bytes) followed by the data source's own blob and then the
    renderer's. `dp_wfm_synth_set_state` decodes every field into a local and
    checks it before the first write; a refusal before its commit leaves the
    synth as it was, and a child that refuses after the commit leaves the
    synth's own fields restored (#2104). The layout is
    `WFM_SYNTH_STATE_VERSION`, 7.

For the `ofdm` type:

- **The frame in play and the source position** travel as they do for a
    `bits` frame. The rendered samples do not travel. `set_state` re-renders
    the PPDU from the restored frame bits, which is sound only because the
    `wlan` builder is a pure function of its configuration, the frame bits
    and the scrambler's initial state.
- **The scrambler's initial state is a render input.** A per-PPDU draw from
    `seed` (section 10) is reproducible on resume only if the draw is a
    function of the seed and a PPDU counter, and the counter travels. A draw
    from a running generator the blob does not hold would re-render a
    different PPDU for the same frame bits.
- **The sample cursor** is the synth's `sym_read_idx`, and is checked at `set_state` against the
    rendered length, as `sym_pos` is checked by `dp_wfm_synth_sym_pos_ok`
    before the kernel indexes a buffer with it.
- **Derived values are recomputed and checked, never trusted.** The
    continuous-DSSS symbol edge `next_edge` is not in the blob: `set_state`
    recomputes it from the chip and symbol counters and refuses a blob whose
    counters disagree (`dp_wfm_synth_clock_ok`). The rendered buffer and its length are `ofdm`'s derived
    values and follow the same rule.
- **Reject keys**: the constellation, the code rate, the window, the
    oversampling `M`, the `mac` mode, `scrambler_init` and the seed are the
    renderer's, compared at `set_state` whole; the frame's bit count and
    length are the synth's own. The `gap` is not a key: it is the length less
    the PPDU's, which those fix. Each is proved alone, at an equal length, in
    `test_wfm_ofdm_bridge.c`. The type itself
    already is one: `wtype` is in the blob and `wfm_synth_runnable` checks it.
- **The version is 7.** `make state-versions-check` fails a layout that reuses
    a version the project has shipped, so the bump is a gate and not a
    convention.

The modulator's own state, and the layout as its reject key, are on
[its page](ofdm.md). `wlan` is stateless: a pure function of its
configuration and the PSDU. `scripts/check_serializable.py` requires every
object in `objects/*.toml` to either declare `serializable = "true"` or be
listed in `scripts/.serializable-stateless`, a text file of one object name
per line with `#` comments. `wlan` takes one line there, with the comment
saying why.

### The faces

Fields proposed on a source, all refused for any other type, in the one
validator every face asks (`dp_wfm_source_to_synth_error`,
`wfm_synth_bridge.c`, which calls `dp_wfm_scene_error`):

| field            | meaning                                                                                            |
| ---------------- | -------------------------------------------------------------------------------------------------- |
| `standard`       | `none` or a preset name; a preset fixes every field below that the standard fixes                  |
| `bandwidth`      | channel bandwidth in Hz; the segment's `fs` is an integer multiple of it, or the source is refused |
| `constellation`  | `bpsk`, `qpsk`, `qam16`, `qam64`, `qam256`, `qam1024`                                              |
| `code_rate`      | `1/2`, `2/3`, `3/4`, `5/6`; a pair Table 17-4 does not list is refused                             |
| `scrambler_init` | the 7-bit initial state; section 10                                                                |
| `layout`         | `standard = none` only: the modulator's layout ([ofdm](ofdm.md), section 4)                        |
| `mac`            | `none`, `fcs` or `data`: how the caller's octets become the PSDU (section 3)                       |
| `mac_header`     | `data` mode: the MAC header octets; a field form is section 10's                                   |
| `mcs`, `gi`      | HT and later only; they arrive with section 6's rows 4 and 5                                       |

**A layout is a structure of arrays**, which the composer's source fields
cannot hold as flat rows, so it follows the one precedent for a structured
input: `frame` is `object = "frame.FrameDesc"` with `surface = "bespoke"` and
`copy_fn`, `free_fn` and `parse_fn` (`modules/wfm_compose.toml:477-492`), a
`--frame FILE` option, a JSON object, and an owned object on the Python
face. `layout` is the same: an `ofdm.LayoutDesc` (the `doppler.ofdm` module, a
value object of this type's PR; the modulator `Ofdm` holds its own layout),
`--ofdm-layout FILE`, a scene's `"layout"` object. A `standard` preset
expands to the same object, so `standard = none` with a layout and
`standard = 802.11a` are two ways to fill one field. Whether a second
`object`-typed composer field beside `frame` generates, and a Python
constructor that takes a dict, are checked by the type's PR before any C;
neither is asked of jm.

**`constellation` is a second field, not an extension of `modulation`.**
`modulation` is `enum = "bitmod"` (`modules/wfm_compose.toml:312`), whose
values `none`, `bpsk`, `qpsk` are the C enum `wfm_bitmod_t`, the
`BITMOD_NAMES` table and `[[enum]] bitmod`, bound together by the gate
above, and it describes the mapping of a `bits` pattern (`json_when = { type = "bits" }`, help section BITS). Adding `qam16` to it would change a
`bits` source's surface and its C enum for the benefit of another type, and
`none` has no OFDM meaning. A new `[[enum]] constellation` is appended and
nothing existing moves. The name of the field is open (section 10).

**`json_when` does not need to name a set of types.** It names one field and
one value (`scripts/gen_wfm_defaults.py:579`, "json_when names ONE field"),
which is what the new field wants: `json_when = { type = "ofdm" }`. That
generator is this project's own, so extending `json_when` to a set would be a
change here, not a request to jm.

**Spellings.** The recommended spellings are the standard's own:
`code_rate` `1/2`, `2/3`, `3/4`, `5/6`; `standard` `802.11a`, `802.11g`,
`802.11p`, `802.11j`; `constellation` as above. A "/" in an enum value
reaches the CLI and JSON faces unchanged (`psd_window` already carries
`blackman-harris`, which the schema lists verbatim). What is not yet shown
is a composer-kind module with these rows: that is the first thing the type's
PR checks, by running `scripts/gen_wfm_defaults.py --check` on the manifest
before any C.

**What the bridge refuses**, each with the sentence every face gives:

- `standard`, `bandwidth`, `constellation`, `code_rate`, `scrambler_init`,
    `mac`, `mac_header` and `layout` on any type but `ofdm`;
- an `ofdm` source whose `bandwidth` is not one the preset's clocking allows,
    whose `fs` is not an integer multiple of `bandwidth`, or whose
    `constellation` and `code_rate` are not a pair of Table 17-4;
- `sps` above 1, `pulse`, `symbols`, `symbol_rate`, the DSSS codes, a frame
    description, `--sync`, `--crc` and `--acq-code`, which describe a
    symbol-rate stream or a frame the PHY builds itself. `rrc_beta` and
    `rrc_span` are read only with `pulse = rrc`, which is refused, so they
    need no refusal of their own. `modulation`, `pn_length`, `pn_poly`
    and `lfsr` are another type's parameters and are ignored, as every
    existing type ignores the parameters of the types it is not
    (`dp_wfm_source_error` asks `pn_poly` only of `pn`, `bpsk` and `qpsk`);
    the refusals above are the fields that would silently change what an
    OFDM waveform means;
- a `data_len` that is not a whole number of octets, or outside the PSDU
    length the preset allows;
- `snr_mode = ebno` and `esno` (owner ruling: `auto` and `fs` mean signal
    power over noise in the full band);
- a `scrambler_init` above 127. Zero is not refused: it draws the initial
    state per PPDU from `seed` and the frame index;
- a `mac` outside `none`, `fcs` and `data`; `mac = data` without a
    `mac_header`, a `mac_header` in any other mode, a `mac_header` that is
    not whole octets, not 24 or 30 octets, a `data:LEN` or a Field that does
    not build. `mac = fcs` takes one octet or more. The header is a Field
    (a `wfm_seq_t`, as `data_code` is: reuse of its copy, free and JSON
    handling), its octets unpacked most significant bit first like every
    byte-to-bit unpacking here;
- a `window` above 80 times `fs / bandwidth`, a negative `bandwidth`, a
    `constellation` or `code_rate` index no face offers (a C or Python
    source can give one), and a frame, its `gap` included, over 2^24 samples,
    each range-checked before it is narrowed or summed;
- a PSDU longer than the PHY's LENGTH field can describe (4095 octets, the
    FCS included).

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

### The preset, `--wlan N`

`ofdm` takes general fields: `constellation`, `code_rate`, `scrambler_init`,
`window`, `mac`, `mac_header`, `bandwidth` (section 5). `wlan` is a second
spelling of the first two. It is not a type and not a second code path:

```text
--wlan 36    is    --constellation qam16 --code_rate 3/4
```

The standard's numbers arrive as a table. `dp_wlan_tables_standard()` already
holds the eight (rate, RATE bits) rows of Table 17-6; `wlan` reads the
table's first column, the data rate in Mb/s, which Table 17-6 prints beside
the RATE bits. The synth does not know 802.11.

#### The field

`wlan` is an integer, `json_when = { type = "ofdm" }`, declared once in
`modules/wfm_compose.toml` like every other source field. wfmgen has no
preset mechanism; every option is a field, so this is one more. The bridge
expands it at attach through `dp_wlan_rate_from_mbps`. One spelling per
source: a source that gives `wlan` and also `constellation` or `code_rate` is
refused, so the JSON writer's output has one reading.

`wlan` is the 20 MHz data rate. A source with `wlan` and a `bandwidth` other
than 20 MHz is refused: at 10 and 5 MHz the same pair carries half and a
quarter of the rate, and a flag that says 36 must not produce an 18 Mb/s
signal. A 10 or 5 MHz source is written with `constellation`, `code_rate` and
`bandwidth`.

#### The per-rate numbers

`dp_wlan_signal_rate_t` gains a `mbps` member, the rate as Table 17-6 prints
it. Two lookups over the table, with one convention (1 on success, 0 on a
refusal, as `dp_wlan_bcc` and `dp_wlan_rate_ok` return):

| function                              | what                                                    |
| ------------------------------------- | ------------------------------------------------------- |
| `dp_wlan_mbps(t, r, *mbps)`           | the printed rate of a pair; 0 for a pair with no row    |
| `dp_wlan_rate_from_mbps(t, mbps, *r)` | the pair of a printed rate; 0, `*r` unchanged, for none |

Both refuse an empty table. The 4 in `n_dbps / 4` is not in any function
body: it is the 4 us symbol, and a test holds the transcribed column against
`n_dbps / 4` for all eight rows, so the table and the arithmetic are two
independent statements that must agree. The eight values are distinct (6, 9,
12, 18, 24, 36, 48, 54), checked in the same test.

On the faces:

| capability            | C                        | Python                 | CLI        |
| --------------------- | ------------------------ | ---------------------- | ---------- |
| a pair's rate in Mb/s | `dp_wlan_mbps`           | `Wlan.rate_mbps`       |            |
| a rate's pair         | `dp_wlan_rate_from_mbps` | none; just-makeit#2683 | `--wlan N` |

`Wlan.rate_mbps` is the 20 MHz rate and the docstring says so, as
`Wlan.airtime` already does; the object holds no clocking. It is 0 for a pair
Table 17-6 has no row for (BPSK 2/3, 64-QAM 1/2, any 5/6). Python has no
lookup from a rate to a pair: none; just-makeit#2683.

#### Refusals, and the test for each

Each refusal is tested on every face it can be reached from, and a test fails
if the refusal does not trip. The C validator is
`dp_wfm_source_to_synth_error`; the others are the JSON scene, the CLI and the
Python keyword.

| refusal                                       | C   | JSON | CLI | Python |
| --------------------------------------------- | --- | ---- | --- | ------ |
| `wlan` not one of the eight, naming the eight | x   | x    | x   | x      |
| `wlan` with `constellation` or `code_rate`    | x   | x    | x   | x      |
| `wlan` with a `bandwidth` other than 20 MHz   | x   | x    | x   | x      |
| `wlan` on a type other than `ofdm`            | x   | x    | x   | x      |

At the object, `dp_wlan_mbps` and `dp_wlan_rate_from_mbps` each have a test
for the empty table, an unknown pair, an unknown rate, and `*r` unchanged on
refusal, with the output poisoned first.

#### What is proved

For each of the eight N, a source with `wlan = N` renders output
byte-identical to the source spelled with its `constellation` and
`code_rate`, same seed. That holds the expansion, not the content.

The content of a PPDU is held in this tree by `test_wlan_core.c` section 10b,
which runs the 36 Mb/s PPDU (16-QAM, 3/4) only, against the Annex I scaling.
The other seven rates are held by the per-stage unit tests (the RATE bits, the
puncture masks, the permutation, the constellation encodings) and by no
whole-PPDU check; the downstream comparison recorded on #3148 is outside
this repository.

#### The 36 Mb/s PPDU

The header is 24 octets: Frame Control, Duration, three addresses, Sequence
Control. The line runs under `make test-snippets`.

```sh
wfmgen --type ofdm --fs 20e6 --wlan 36 --scrambler-init 93 --mac data \
    --mac-header 0x08020000ffffffffffff0200000000010200000000020000 \
    --data pn:800:15 --data-len 800 -o ppdu.cf32
```

### The frame callback

`WFM_SYNTH_FRAMES` is a synth type that plays one finished frame of samples at a
time, drawn lazily from a frame callback that turns a frame of bits into
samples. It is the sample-domain sibling of the bit refill and of the symbols
source, and the synth knows no standard: a frame is bits in, samples out. The
face type is named `ofdm` because that is what a user types and selects it;
the constant and the branch are generic, and the header says so.

**Why a type of its own, not the symbols source.** `WFM_SYNTH_SYMBOLS` cycles
its buffer: the cursor wraps modulo `n_symbols` in four places
(`wfm_synth_next_symbol`, `dp_wfm_synth_step`, and the two loops of
`dp_wfm_synth_steps`), so the cursor reaching the end of a frame is never
observable there. Making the cursor stop, drawing the next frame lazily and
calling the bit refill when its pacing says so (as `wfm_synth_bit_ahead` does
for bits since #1718) would change those four sites for every symbols synth,
and would then need the rules the shaper makes necessary: refuse `sps != 1`, an
attached shaper or FIR, `set_rrc` and `set_nsps` after the install, widen
`set_refill` (which refuses a symbols synth) and say what `set_symbols` does on
a synth with a callback. A type of its own needs none of those: its branch has
no shaper to refuse (`set_rrc` is already a no-op for a type that is not
pn, bpsk, qpsk, bits, symbols or dsss), `set_symbols` is already a no-op for
every type but symbols, and `SYMBOLS` keeps its wrap and its tests as they are.
It reuses the synth's own fields, `symbols` (the frame in play, `n_out`
samples), `sym_read_idx` (the cursor), `bits` (the frame's bits) and the bit
refill with its source triplet.

How the sources differ, in the header and here:

| source                        | the caller hands the synth                        | the synth then                             |
| ----------------------------- | ------------------------------------------------- | ------------------------------------------ |
| `set_refill` (bits)           | a callback that writes the next frame's bits      | maps them and shapes them                  |
| `set_symbols`                 | one fixed buffer of complex symbols               | plays it cycled, shaped or as it is        |
| `attach_frames` (type `ofdm`) | a callback that renders a frame's bits to samples | plays each frame once, then draws the next |

```text
typedef int (*wfm_synth_frame_fn) (void *user, uint64_t frame,
                                   const uint8_t *bits, size_t n_bits,
                                   float _Complex *out, size_t n_out);
int dp_wfm_synth_attach_frames (state, wfm_synth_frame_fn fn, void *user,
                             void (*free_user) (void *),
                             size_t n_bits, size_t n_out);
```

**The cursor and the helper.** In this type the cursor STOPS at `n_out`; it
never wraps. One helper, `wfm_synth_frame_ahead`, the sibling of
`wfm_synth_bit_ahead`, is the only place it moves, and both sites that read
samples (the block path and the single step) call it; `next_symbol` serves only
the shaped path, which this type has none of. When the cursor is at `n_out` the helper draws the next frame lazily, when
its first sample is due: it asks the bit refill for the frame's `n_bits` bits
(a refill that reports its end, or no refill or no renderer, latches
`data_ended`), normalises each byte to 0 or 1 (the renderer is promised bits,
and the bits path normalises on read), then calls
`fn (user, frame, bits, n_bits, out, n_out)`. A non-zero return is a refusal and
latches `data_ended` and `refused`, and the output is silence after, as it is
after a refill's end; `dp_wfm_synth_frames_refused` says the end was a
refusal. There is no second stopping behavior. The cursor's range is
`0 .. n_out`, and `set_state` checks `<= n_out` for this type and `< n_symbols`
for the others.

**`frame` is the index of the frame being rendered**, from 0: the callback
receives 0 for the first frame it renders. The blob holds the COUNT rendered:
0 means no frame is in play, and `set_state` does not re-render then; a count
`k > 0` means frame `k - 1` is in play, and `set_state` renders it with
`frame = k - 1`. The cursor starts at `n_out` (no frame drawn), and a freshly
created synth draws on its first sample.

**Install order.** The type is created, then `attach_frames` allocates `bits`
(`n_bits` bytes) and `symbols` (`n_out` samples), then `set_refill` installs
the bit source, then `set_refill_state` its triplet. `set_refill`'s predicate
widens by one clause, as it did for dsss: it accepts an `ofdm` synth that has
its frames installed, and its `@return` says so. `bit_idx` is not used by this
type: the frame's bits are drawn whole, `n_bits` at a time, and never played
bit by bit.

**Detaching.** `attach_frames (NULL, ...)` detaches the callback and keeps the
frame in play, as `set_refill (NULL, ...)` detaches a source and keeps the
pattern: with no callback the frame plays out and the data ends, the same end
as a pattern with no refill. `set_symbols` on this type returns 0 and does
nothing, as it does for every type but symbols.

**What the blob carries**, and the version. The layout is version 7
(`WFM_SYNTH_STATE_VERSION`). Written always: `frame` (`u64`, the count
rendered), `refused` (`u8`), the frame's bit count and length (`u64` each, 0
for a type that is not frames), and a presence byte for the renderer's triplet,
as `pres[5]` is for the bit refill, with the triplet's sub-blob if it has one.
The frame in play's bits and the bit source's own sub-blob travel as they do
for a bit refill. The samples do not. The Python state matrix and any recorded
blob size move with the version. The synth refuses to serialize (state
refusal) while a frame is in play and the renderer or the bit source is
detached, because a restore could not re-render it.

**Resume.** `set_state` re-renders the frame in play from the blob's bits and
`frame`, and the result is bit-exact for the same build on the same machine
and FFT dispatch, which is what the state tests assert. The order is:

1. decode every field into a local and check it, as today (#2142), including
    the cursor against the frame length, the frame length and bit count against
    this synth's, `refused` against `data_ended`, and that a frame is in play
    only when the data has not ended;
1. read ahead past the children's sub-blobs, checking each child's envelope,
    and the bit source's against the live objects' own, the renderer's
    sub-blob against the live renderer's `get_state` byte for byte, and the
    frame's bits (each byte 0 or 1), before any write; each child's size is this
    instance's `state_bytes`. The renderer's `set_state` is never called, so
    nothing of the renderer is written at any step;
1. render the frame in play into a scratch buffer, and refuse if the callback
    refuses or the allocation fails;
1. restore the children, the carrier and the noise. A child's `set_state`
    checks more than the envelope step 2 did (the AWGN refuses an all-zero
    generator or a bad amplitude), so one can still refuse here, and the
    children before it stay restored, which is #2104;
1. restore the bit source's triplet. Its position is outside the synth (a
    counter, a file, a caller's data) and no refusal can put it back, so it
    moves only after the last child that can refuse;
1. commit the fields, the bits and the scratch buffer swapped in, none of which
    can fail.

A refusal at any step therefore leaves the bit source, the frame and the
synth's own fields as they were; at step 4 or 5 the children before the one
that refused are restored (#2104). The other types restore their children
after the commit and their refill last, which is the same rule: nothing that
cannot be undone moves before the last thing that can refuse.

`set_state` holds two frames of memory transiently (the frame in play and the
scratch). There is no re-render before the first frame or after `data_ended`
has latched: with the data ended no frame is in play.

**Reject keys.** The callback is a pure function of its configuration, the
bits and `frame`; it carries no running state, and its triplet's bytes are
its configuration, compared whole with the live renderer's. A blob restored
into a source with a different seed, window, `M` or rate would re-render a
different PPDU with no refusal, so the bridge's 802.11 callback uses the existing optional
triplet (`wfm_synth_refill_state_t`, attached by
`dp_wfm_synth_attach_frames_state`) to carry its configuration and to refuse a
mismatch. The frame length and the bit count are keys the synth writes itself.
A test restores into a source differing in each key and checks each is
refused.

`attach_frames` and `attach_frames_state` are C-only installers that the
mutator-state gate does not read (their names are not `set_`). What pins them
instead is that their values travel or are keys: the frame length and bit count
are in the blob and checked, the renderer's configuration is its triplet, and
the function pointer is a handle that a restore leaves as it is.

**Contracts the header carries.**

- **Level.** The carrier (`LO`) and the noise (`AWGN`) are applied after the
    frame, so unit mean power over the PPDU's samples is the callback's contract,
    as the 802.11 convention makes it
    ([#3148](https://github.com/doppler-dsp/doppler/issues/3148)).
- **Frames play back to back**, with no overlap-add across frames. An idle gap
    between PPDUs is part of the frame's length: the bridge appends zero samples
    to the rendered PPDU, a field of the source (`gap`, in samples at `fs`,
    default 0). Unit mean power is over the PPDU's samples and excludes the gap's
    zeros, so the noise is set against the PPDU and the gap is noise only. A test
    renders the same source with `gap = 0` and `gap > 0` and checks the noise
    variance is the same.
- **`reset()`** rewinds the cursor to the start of the frame in play
    (`sym_read_idx = 0`), which replays the last frame rendered. A frame is in
    play when the cursor is below `n_out`, the definition `set_state` uses, so
    a reset with none in play (none drawn yet, the frame spent at `n_out`, a
    refusal, the data ended) leaves the cursor and replays nothing. It does
    not rewind `frame` or the data source, as a bit refill's reset does not
    rewind a source it has consumed, and it never clears `data_ended`: the
    bits types' rule that a pattern with no refill is the whole of the data
    un-ends on a reset does not apply, since a frames synth has no pattern.
    `refused` goes with `data_ended`: it survives every reset, and is cleared
    wherever `data_ended` is, by attaching or detaching a bit source. A reset synth and a fresh one therefore
    differ.
- **`user`** is owned by the synth when `free_user` is given, which it calls on
    detach or destroy, as `set_refill` does.
- **The frame's length** `n_out` is fixed at install. A PPDU's length is a
    function of the configuration and the PSDU's length, never of its bits. `n_out`
    includes `gap`, and a frame is at most `2^24` samples (128 MB of `cf32`): a
    larger `n_out` makes `attach_frames` return -1. The bridge, which knows the
    parts, refuses the source naming the limit, the PPDU and the gap.

`WFM_SYNTH_FRAMES` therefore has nothing in it that is OFDM or 802.11: the bridge
installs the 802.11 callback, and another standard's frame would use the same
type.

### Oversampling: `fs = M x bandwidth`

A PPDU is rendered at the bandwidth clock by default. A segment whose `fs` is
`M` times the bandwidth, for an integer `M >= 1`, is rendered by a larger
transform, which is zero-padding the grid before the inverse transform, not a
resampler. `Ofdm` already takes `n_fft`, `useful` and `cyclic_prefix` per
class, so the oversampled PPDU is another layout:

- `n_fft = 64 M`; every length is `M` times its 20 MHz value: the L-STF's
    `useful` (`160 M`), the L-LTF's body (`128 M`) and prefix (`32 M`), each
    symbol's body (`64 M`) and prefix (`16 M`), and so `dp_wlan_ppdu_len`;
- what does not change: the subcarrier indices (`k` from -26 to 26), the pilot
    subcarriers (`WLAN_PILOT_K`) and their values, the scale and the pilot
    polarity;
- the scale does not change. `Ofdm`'s transform is unnormalized,
    `x[m] = scale * sum_k grid[k] exp(+2j pi k m / n_fft)`, so at `m = M n` the
    exponent is `2 pi k n / 64`: `x_M[M n] = x_1[n]` for the same `scale`, and the
    unit-power convention holds at every `M` because the same waveform is
    sampled finer.

The C functions take `oversample` (`M`) beside the `window`: `dp_wlan_layout (o, t, n_sym, window, m)`, `dp_wlan_ppdu_len (n_sym, window, m)`, `dp_wlan_ppdu (..., window, m, out, cap)`. In wfmgen there is no flag: `M = fs / bandwidth`,
and a quotient that is not an integer is refused naming why. `M = 1` is
bit-identical to the output before the argument, pinned against two PPDUs
committed from the commit before it.

**Which `M`.** `M` is 1 to 128 (`WLAN_OVERSAMPLE_MAX`), so `n_fft` is at most
8192\. The limit is chosen from a use, not from `OFDM_N_FFT_MAX`: a 20 MHz PPDU
in a generator running at 2.56 GS/s is `M = 128`, and no 802.11 generator
needs a faster clock. At `M = 128` the longest PPDU (4095 octets at 6 Mb/s,
1366 symbols) is 14,039,040 samples, under the synth's `2^24` frame limit
(16,777,216), so the frame limit never refuses a PPDU in range. The transform
takes any length, so `M = 3, 5, 11` are accepted; the test builds the layout at
`M = 1, 2, 3, 5, 11, 16, 128` and refuses `0` and `129`.

**The decimation check, predicted before any run.** The oversampled PPDU with
`window = 0`, decimated by taking every `M`-th sample, equals the `M = 1` PPDU
with `window = 0`: one equation on a finer grid. The difference is float32
rounding in two transforms, and the statistic is the relative `l2` error over
the compared samples. The tolerance is set by the error the check must
catch: a layout error (a length, a prefix, a scale, a grid offset) moves
samples by `O(1)`, and the controls are at least `1e-2`. The tolerance only
has to sit above the rounding, and its source is the accuracy the FFT's own
test pins: `test_fft_core.c` accepts the library's float32 transform against a
double reference to `5e-4` absolute in each of the real and imaginary parts,
on inputs of unit scale. A complex sample therefore differs by at most
`sqrt 2 x 5e-4`; two transforms are compared, so `2 sqrt 2 x 5e-4 = 1.41e-3`
per sample at unit rms, which bounds the relative `l2` error; the tolerance is
`1.5e-3`. It is asserted at every `M` tested, `2, 3, 5, 11, 16` and `128`,
powers of two or not, and the committed-bytes cross-build check of `M = 1`
uses the same tolerance and source. The expected size of the rounding is far
smaller, a small multiple of `u log2 N` with `u = 2^-24` (Higham, *Accuracy and
Stability of Numerical Algorithms*, the FFT section), about `1e-6`; that is an
expectation and is not asserted.

**With a window.** The raised cosine is
`rise[j] = sin^2 (pi (j + 1/2) / (2 w))` for `w` samples. The two PPDUs compared
are `M`-times with `window = M w` and `M = 1` with `window = w`: the same
transition time. At a decimated sample `i` the fine index is `M i`, so the fine
weight is `sin^2 (pi (i + 1/(2M)) / (2 w))` against the coarse
`sin^2 (pi (i + 1/2) / (2 w))`: the same function, sampled `(M - 1) / (2 M)` of
a coarse sample apart, which is `(M - 1) / 2` fine samples. The two are not
equal inside a transition. The rise
and the earlier symbol's continuation both lie on the `w` samples after a
boundary (`ofdm_core.h`: the later symbol's first `w` samples are weighted by
`rise`, the earlier symbol is continued past its end by `w` samples weighted
by `fall`, and the two overlap-add): nothing before the boundary changes. The
equality is therefore asserted outside the `w` samples after each boundary,
counted on the `M = 1` grid: the start of the L-STF (the first symbol rises
from silence), of the L-LTF, of SIGNAL and of each Data symbol.

**The spectrum.** At `M = 1` the discontinuity between symbols falls inside
the band. At `M > 1` the 52 occupied subcarriers lie between `-26 / (64 M)` and
`+26 / (64 M)` of `fs`, and outside them is only the spectrum of the symbol
boundaries: each OFDM symbol is a burst of finite length, so its subcarriers
carry sin(x)/x sidelobes falling as 1/f, and a window replaces the burst's edge
with a raised-cosine one, whose sidelobes fall faster. The unit test shows the
power at four out-of-band frequencies is lower with a window than without. The
802.11 transmit spectral mask bounds the signal, and no preset claims the mask
is met.

**`window` at fs.** `window` is in samples at the output rate `fs`, not at the
bandwidth clock. Every C function here counts output samples
(`dp_wlan_ppdu_len` adds `window` samples, and `Ofdm`'s window is a number of
output samples per class), so there is one unit throughout and nothing is
converted. The transition time for a given `window` therefore shortens as `fs`
rises: `window = 2` is 100 ns at `fs = 20 MHz` and 12.5 ns at 160 MHz. The
standard's 100 ns transition (17.3.2.5) is `2 M` samples at `fs`. The help
text for `window` on an `ofdm` source says samples at `fs`.

**What the allocation is.** The layout's maps are heap arrays of `n_fft`
bytes, not stack arrays.

### Es/N0 for an `ofdm` source

**Proposed, for the owner's confirmation.** The owner's definition: Es/N0 is
the signal-to-noise ratio a receiver's constellation shows, so in AWGN alone
it equals the modulation error ratio (MER), "-MER or similar" in the
owner's words. The reference is ETSI TR 101 290 V1.4.1 (2020-06), read from
the text:

- **MER** (clauses 6.9.2, 9.18.2 for DVB-T, 11.3.19.2 for DVB-T2): the sum of
    the squared magnitudes of the ideal symbol vectors divided by the sum of
    the squared magnitudes of the error vectors, as a power ratio in dB. It
    is a positive number: a better signal reads a larger MER. The error vector
    is the distance from the ideal position of the decided symbol to the
    received one. The TR says the definition does not assume an equalizer and
    that a quoted figure should say whether one was used; the DVB-T2 clause
    measures after equalization (its interface C2).
- **Which carriers**: clause 9.18.1 says the IQ analysis applies to single
    carriers or groups, and, because scattered pilots, continual pilots and
    TPS carriers use a different modulation, recommends excluding them or
    applying a specific analysis. MER is therefore over the data carriers.
- **SNR and MER** (annex G, clause G.8): the ratio of I/Q signal power to I/Q
    noise power in dB is the definition of both; they differ in which
    perturbations are included, and with noise the only impairment they are
    equal. The TR says the relation to C/N depends on the noise bandwidth
    chosen (G.8; G.10 gives its Eb/N0 form).
- **MER and EVM** (annex C.3): as voltage ratios, MER is the reciprocal of
    the product of EVM and the constellation's peak-to-mean voltage ratio.

**The definition taken from it.** `esno` for an `ofdm` source is the MER of
the data subcarriers after equalization, with the ideal symbols as the
reference: `Es/N0 [dB] = MER [dB]`, where `Es` is the mean energy per data
constellation point and `N0` the noise energy per constellation point, both
as seen on the received grid after the forward transform. Pilots, training
values and null subcarriers are excluded. Where the TR's text says MER is
over all carriers or before equalization, this takes the TR's data-carrier
and after-equalization wording, stated here rather than assumed.

**Sign.** MER is positive, and the owner's "-MER" is the EVM in dB with the
reference chosen so that EVM [dB] = -MER [dB]. The library's own EVM meter
(`dp_ber_evm_db`, `rx-test.md`) is not that: it scales its window to unit
total power, signal plus noise, so it reads
`10 log10 (2 - 2 sqrt(r / (1 + r)))` at `r = Es/N0` (20.03 dB below at
20 dB, 5.91 below at 5 dB) and approaches `-Es/N0` only at high SNR.
`ofdm`'s definition is the TR's MER, not that meter's EVM; a test that
reads the library's meter subtracts its own noise floor, as the meter's
header says.

**Relation to the in-band SNR over `fs`.** Let the receiver's transform be
unitary, so white noise of power `s2` per complex sample at `fs` is `s2` per
subcarrier. With `N_D` data subcarriers at mean energy `Es` per point,
`E_fixed` the mean energy of the fixed entries (pilots) per OFDM symbol and
`rho = E_fixed / Es`, the body's mean sample power is
`P_data = (N_D Es + E_fixed) / N_FFT`. The cyclic prefix repeats samples of
the body, so it does not change the mean power. With `g = P_data / P_seg`
(1 where the fields are equal in power, as 802.11's are), and `SNR_fs` the
segment's `P_seg / s2`:

```text
Es/N0 = Es / s2
      = SNR_fs * g * N_FFT / (N_D + rho)

Es/N0 [dB] = SNR_fs [dB] + 10 log10 (N_FFT / (N_D + rho)) + 10 log10 g
```

The factor `N_FFT / (N_D + rho)` is the occupancy factor, at least 1 for
equal-energy pilots: the noise spreads over all `N_FFT` bins at `fs`, the
signal sits on the occupied ones. For 802.11a (`N_FFT` 64, `N_D` 48,
`N_SP` 4, unit pilots) it is 64/52. A pilot boost `b` is `rho = b^2 N_SP` and
costs data-point energy for the same `P_seg`, which is why `rho` is in the
denominator: boosting the pilots lowers `Es/N0` at a given `SNR_fs`.

**What the cyclic prefix does.** Nothing to either quantity. The receiver
discards the prefix and transforms `N_FFT` samples, so the noise per
subcarrier is `s2` with or without it, and the mean sample power is the
body's. The prefix costs transmit energy: the energy radiated per
constellation point, prefix included, is `Es (1 + N_cp / N_FFT)`, which is not
`Es` in this definition. A receiver's constellation does not show that cost,
so the definition, which is the constellation's, leaves it out.

The equation is the baseline. Characterization (PR 8) measures it: render at
a stated `SNR_fs`, forward transform each OFDM symbol, and read the data
subcarriers' MER against the equation.

**`auto` for `ofdm`: Es/N0.** Proposed. `auto` is Es/N0 for the types that
carry symbols (`bpsk`, `qpsk`, `dsss`) and relative to full scale for the rest;
`ofdm` carries constellation points, and Es/N0 is what its receiver displays,
so `auto` resolves to `esno`. `fs` stays available, and the equation above
converts. `ebno` is refused for `ofdm` in the first slice: it needs a choice
between per coded bit and per information bit that nothing here has decided.

### General OFDM, `standard = none`

The general modulator is reachable from wfmgen with no 802.11 in play.
`standard = none` takes a `layout`; `standard = 802.11a` and the other
presets expand to the same `layout`. The bridge refuses:

- `layout` with a `standard` other than `none`, so a preset cannot be
    overridden by a field it fixes, and `layout` on any type but `ofdm`;
- a layout whose run names a class that does not exist, whose
    `subcarrier_map` or `subcarrier_gain` is not `n_fft` long, whose `useful`
    is zero past the default, whose fixed entry names a subcarrier outside
    `-n_fft/2 .. n_fft/2 - 1`, whose `n_phase` is zero, or whose
    `n_polarity` is zero while `polarity` is set;
- a layout with no OFDM symbol that takes data when the source supplies
    data, and a data source that supplies fewer points than the layout's
    data count.

The constellation points come from the source's own data path: the mapper
named by `constellation` over the payload bits, or, for `standard = none`,
the `symbols` type's stream.

## 6. The 802.11 flavors, in order

Each row says what it adds over the row above. Every row is one spatial
stream on one transmit chain (goal 7). Each is a preset and claims no
conformance until its numbers are checked against the text.

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
    one flavor that can be proven bit for bit once the text is in hand.
1. Every later PPDU begins with these same L-STF, L-LTF and L-SIG fields,
    so nothing built here is set aside.
1. It needs no decision on LDPC or on a CRC other than the FCS's.

Rows 4 and 5 are the second slice; each later row is its own.

## 7. How a preset is proven

The rule is that the anchor is the standard, never the implementation
under test and never a second implementation in the tree. The modulator's
own proofs are on [its page](ofdm.md) and need no standard. What follows
holds a preset to a standard, and the tables are
transcribed from the text (section 9); the unit test holds the RATE bits,
both training sequences, the constellation encodings and the 2/3 and 3/4
puncture masks against their printed form, and the second permutation by its
worked 16-QAM example and by a re-derivation of the printed equation (for
every index of all four pairs). Not held to the text: Annex I's
stage-by-stage example, which waits for its fixture
([#3148](https://github.com/doppler-dsp/doppler/issues/3148)), and the pilot
polarity sequence. Where the tests pin only what is derivable (a layout's structure, the lengths, the
round trip of the coding chain's own inverses), claiming no conformance.

1. **Annex I, stage by stage, when the text is read.** Annex I encodes one
    100-octet PSDU at 36 Mb/s and prints every intermediate: the PSDU, the
    SIGNAL bits, their coded and interleaved forms, the scrambled, coded,
    punctured and interleaved Data bits, the subcarrier values and the time
    samples of each field. A C test holds those tables as literals
    **transcribed from the text** and compares each stage: bits exactly,
    samples to the printed precision. A table produced by running the
    implementation would encode the builder, so the transcription needs the
    text (section 10).
1. **The tables Annex I does not exercise, literal by literal.** Annex I
    covers one rate: 16-QAM at 3/4. The BPSK, QPSK and 64-QAM maps
    (Tables 17-15 to 17-18), the 2/3 puncture pattern (Figure 17-9), the
    RATE encodings (Table 17-6) and the timing constants (Table 17-5) are
    each small enough to pin entry by entry against the standard.
1. **The FCS.** The CRC is pinned by its published catalogue check value
    (the nine ASCII digits `123456789` give `CBF43926` for CRC-32/ISO-HDLC)
    independent of this standard, and the byte order on the wire by the Annex
    I PSDU's last four octets: `fcs` mode over the Annex I frame without its
    FCS must reproduce the printed PSDU exactly. That second test does not
    exist yet and the Annex I octets are not in the tree
    ([#3148](https://github.com/doppler-dsp/doppler/issues/3148)); until it
    does, the byte order rests on the catalogue value alone.
1. **Transmit-quality measurements with the library's tools**: the power
    spectrum against the transmit mask (17.3.9.3), spectral flatness
    (17.3.9.7.3), relative constellation error (17.3.9.7.4) and PAPR. A
    clean generator passes these trivially at one sample per `1/bandwidth`
    with no window; they earn their place once the window and oversampling
    are in play, and they are the characterization's subject (section 8).

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
contents and CRCs) and no sample vector. What anchors them is unknown 8.3.

## 8. Unknowns

1. **Whether a filter is needed.** Section 4's row: whether the window alone
    meets the transmit mask at each bandwidth and oversampling factor, or a
    `fir` stage is owed. The modulator's own unknowns (the window's cost,
    speed, the float32 error budget) are on [its page](ofdm.md).
1. **PAPR per modulation, and the headroom a scene needs**, for the preamble
    plus Data as a whole, per rate, after the window. Nothing is quoted.
1. **The anchor for HT, VHT and HE.** A capture from a signal generator,
    another tool's output held as fixture data, or structure alone.
1. **The data source across PPDU lengths.** `data_len` fixes the PSDU
    length per frame today, and a refill never changes its frame length
    (`wfm_synth_core.h:164`); a scene with a different length per PPDU has no
    spelling.
1. **The gap between PPDUs.** A refilled `dsss` burst repeats back to back
    unless the scene gaps it. Whether an `ofdm` source plays PPDUs back to
    back, or idles between them for a time the standard defines, is
    unmeasured; the standard's interframe spaces are MAC timing, which
    section 2 puts out of scope.
1. **Whether the one-field description reuses the pull.** Section 5 proposes
    a `data:LEN` description so the PSDU rides `pull_new`; it is read from the
    code and not yet tried.

## 9. Standard-fixed numbers, and which are checked against the text

Each row says whether the number is checked against the text of IEEE Std
802.11-2024, the edition every clause, table and figure number on this
page refers to. A row marked unchecked is from memory: a builder that
writes it into code labels it unverified, and a test that holds it claims
no conformance.

| number                                                                                                                                                                                        | cited as                                                                   | against the text                                                                                                                                                                                                                                                   |
| --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Clause numbers: OFDM 17, ERP 18, HT 19, VHT 21, HE 27                                                                                                                                         | table of contents                                                          | checked                                                                                                                                                                                                                                                            |
| The worked example is Annex I                                                                                                                                                                 | Annex I                                                                    | checked                                                                                                                                                                                                                                                            |
| `N_FFT` 64, `Δf` 312.5 kHz, `N_SD` 48, `N_SP` 4; GI 0.8 µs, OFDM symbol 4 µs; L-STF 8 µs, L-LTF 8 µs                                                                                          | Table 17-5                                                                 | checked                                                                                                                                                                                                                                                            |
| pilots on subcarriers -21, -7, 7, 21 with values 1, 1, 1, -1                                                                                                                                  | 17.3.5.9, 17.3.5.10, Eq. (17-24)                                           | checked                                                                                                                                                                                                                                                            |
| eight rates and their modulation and coding pairs; RATE bits                                                                                                                                  | Tables 17-4, 17-6                                                          | checked                                                                                                                                                                                                                                                            |
| scrambler `x^7 + x^4 + 1`                                                                                                                                                                     | 17.3.5.5, Eq. (17-14)                                                      | checked                                                                                                                                                                                                                                                            |
| BCC generators 133 and 171 octal, output A first                                                                                                                                              | 17.3.5.6                                                                   | checked                                                                                                                                                                                                                                                            |
| puncture patterns for 2/3 and 3/4                                                                                                                                                             | Figure 17-9                                                                | checked                                                                                                                                                                                                                                                            |
| puncture pattern for 5/6                                                                                                                                                                      | Clause 19                                                                  | unchecked                                                                                                                                                                                                                                                          |
| SIGNAL bit order: R1..R4, reserved, LENGTH LSB first, even parity, six tail bits                                                                                                              | Figure 17-5, 17.3.4.1                                                      | checked                                                                                                                                                                                                                                                            |
| second interleaver permutation, `s = max(N_BPSC/2, 1)`; the worked example `k = 17` to `i = 13`, `j = 12`                                                                                     | Eq. (17-16), (17-17); Annex I.1.6.2, Tables I-17, I-18                     | checked                                                                                                                                                                                                                                                            |
| interleaver, 16 columns (the first permutation uses `N_CBPS/16`)                                                                                                                              | 17.3.5.7, Eq. (17-15)                                                      | checked                                                                                                                                                                                                                                                            |
| `K_MOD` (1, 1/√2, 1/√10, 1/√42)                                                                                                                                                               | Table 17-14                                                                | checked                                                                                                                                                                                                                                                            |
| constellation bit encoding: BPSK, QPSK, 16-QAM, 64-QAM                                                                                                                                        | Figure 17-10, Tables 17-15 to 17-18                                        | checked; held as relabels of the library's maps (section 4)                                                                                                                                                                                                        |
| L-STF and L-LTF sequences and the √(13/6) factor                                                                                                                                              | 17.3.3, Eq. (17-6), (17-8)                                                 | checked; the per-field scale is `1/√52`, unit average power, derived (goal 5)                                                                                                                                                                                      |
| window, `T_TR` about 100 ns                                                                                                                                                                   | 17.3.2.5                                                                   | checked                                                                                                                                                                                                                                                            |
| transmit mask corner points; flatness limits; relative constellation error limits                                                                                                             | 17.3.9.3, 17.3.9.7                                                         | unchecked                                                                                                                                                                                                                                                          |
| Annex I example: 100 octets, 36 Mb/s                                                                                                                                                          | Annex I                                                                    | checked                                                                                                                                                                                                                                                            |
| Annex I example: `SCRAMBLER_INITIAL_VALUE` 54                                                                                                                                                 | I.1.5.2                                                                    | checked; see the note below                                                                                                                                                                                                                                        |
| ERP-OFDM signal extension, 6 µs (`aSignalExtension`)                                                                                                                                          | Clause 18                                                                  | checked                                                                                                                                                                                                                                                            |
| MAC frame format: header, body, FCS; the 24-octet three-address data header (Frame Control, Duration/ID, three addresses, Sequence Control); QoS Control and HT Control as optional additions | Clause 9                                                                   | unchecked                                                                                                                                                                                                                                                          |
| FCS: CRC-32, generator polynomial, ones complement and byte order on the wire                                                                                                                 | 9.2.4.9                                                                    | checked against the catalogue value only (`CBF43926` over `123456789`, appended least significant octet first, `0x26 0x39 0xF4 0xCB`, in the test); not reproduced over Annex I's PSDU: no test does ([#3148](https://github.com/doppler-dsp/doppler/issues/3148)) |
| HT-SIG, VHT-SIG-B CRC-8 parameters                                                                                                                                                            | Clauses 19, 21                                                             | unchecked                                                                                                                                                                                                                                                          |
| L-SIG LENGTH is 12 bits and limits the PSDU to 1 to 4,095 octets                                                                                                                              | 17.3.4.3, Table 17-1                                                       | checked                                                                                                                                                                                                                                                            |
| HT: 52 + 4 subcarriers at 20 MHz, 108 + 6 at 40 MHz; 13 and 18 interleaver columns; short GI 0.4 µs                                                                                           | Clause 19 (Table 19-17 for the columns)                                    | checked                                                                                                                                                                                                                                                            |
| VHT: 234 + 8 subcarriers at 80 MHz; 256-QAM at MCS 8 and 9                                                                                                                                    | Table 21-5, Clause 21 MCS tables                                           | checked                                                                                                                                                                                                                                                            |
| VHT: `N_FFT` 256 and 512                                                                                                                                                                      | derived, not stated: Table 21-5 gives `N_ST` 242 and 484 at `Δf` 312.5 kHz | consistent                                                                                                                                                                                                                                                         |
| HE: `Δf` 78.125 kHz, GI 0.8 / 1.6 / 3.2 µs                                                                                                                                                    | Table 27-13                                                                | checked                                                                                                                                                                                                                                                            |
| HE: 1024-QAM at MCS 10 and 11                                                                                                                                                                 | Clause 27                                                                  | unchecked                                                                                                                                                                                                                                                          |
| `K_MOD` for 256-QAM and 1024-QAM (1/√170, 1/√682)                                                                                                                                             | 21.3.10.9, 27.3.12                                                         | checked                                                                                                                                                                                                                                                            |

**The Annex I scrambler start, two ways.** The text gives the example's
`SCRAMBLER_INITIAL_VALUE` as 54. That value is the first seven bits of the
scrambling sequence, `0110110`, not the shift register's contents. The
register state that produces it is `1011101`: running `x^7 + x^4 + 1` from
that state gives the sequence `0110110 0000 1100 1...`. A builder is
seeded from either spelling and a test holds both, so the two cannot
drift.

## 10. Decisions

### Ruled

1. **The general shape is the deliverable, and the letters wait on the
    text.** No preset claims conformance to 802.11a, g, n or ac until its
    numbers are checked against the standard's text. The owner's ruling: the
    general shape shared across the flavors is what matters, and the
    letters are not certified until validated by the specifications.
1. **The name of the 802.11 component is `wlan`.** A C identifier cannot
    begin with a digit, "Wi-Fi" is a certification mark rather than the
    standard's word, and "wireless LAN" is the standard's own title. On the
    faces the preset is spelled as users say it: `802.11a`, `802.11g`,
    `802.11p`, `802.11j`, `802.11n`.
1. **The modulator's description is a layout**, not a plan, so it is not
    confused with wfmgen's `Plan`: `--ofdm-layout FILE`, a scene key
    `"layout"`, a source field `layout`. OFDM is generally available as a
    library object ([ofdm](ofdm.md)); this page's type is one caller of it.
1. **One type, `ofdm`, with a `standard` discriminator** naming a letter
    (`802.11a`), as `dsss` has `symbol_rate`, rather than a `wlan` type beside
    it. A PPDU's rate is `wlan N`, the 20 MHz data rate, or `constellation`
    and `code_rate` (section 5, "The preset").
1. **Both an object and a type.** The modulator, the QAM map, the CRC and
    the `wlan` builders are certified as objects first; the `ofdm` type is
    the composition.
1. **The first slice is Clause 17 non-HT** at 20, 10 and 5 MHz: section 6's
    rows 1 to 3.
1. **LDPC has its own design page**, ahead of HE and after VHT.
1. **A non-HT rate is spelled by the two columns of Table 17-4 that do not
    change with clocking**: a constellation and `code_rate`. Mb/s names a
    different pair at 10 MHz than at 20 MHz, so `wlan N`, which names it by
    Mb/s, is the 20 MHz rate and is refused at any other bandwidth.
1. **The scrambler's initial state is drawn per PPDU** from the source's
    `seed` through the composer's draw hash, so it is reproducible and read
    back, with `scrambler_init` to fix it (the Annex I test fixes it). The
    draw is a function of the seed and a PPDU counter that travels in the
    state blob, or a restored synth re-renders a different PPDU (section 5,
    State).
1. **A minimal MAC, including the FCS** (section 3): an MPDU from the
    caller's header and body with its FCS, or the caller's octets as the
    whole PSDU. Nothing beyond it.
1. **A parametric CRC** serves the FCS and the SIG fields, with
    `dp_crc16_ccitt` as one of its instances.
1. **The synth plays a frame callback and knows no standard.** One frame of bits in, one frame of symbols out, a pure function
    of the configuration, the bits and the frame index; the bridge installs the
    802.11 callback (section 5, "The frame callback").
1. **A segment's `fs` is `M` times the bandwidth, rendered by a larger
    transform**, not resampled: `n_fft = 64 M`, the same subcarriers, every
    length scaled by `M`, `M` from 1 to 128, `M = 1` bit-identical to the PPDU
    without it (section 5, "Oversampling").
1. **No MIMO.** Out of scope, not deferred (goal 7).

### Open

1. **Annex I, stage by stage.** The Clause 17 tables are transcribed
    (section 9); the unit test holds the RATE bits, the training sequences, the
    constellation encodings and the 2/3 and 3/4 puncture masks against their
    printed form and the second permutation by a worked point and a
    re-derivation of the equation. What is
    open is Annex I's own stage-by-stage example (section 7's first proof),
    which no test holds yet ([#3148](https://github.com/doppler-dsp/doppler/issues/3148)),
    and the rows section 9 marks unchecked (the MAC frame format, the mask
    and accuracy limits). Until the Annex I proof runs no preset is
    certified.

1. **What anchors HT, VHT and HE.** Unknown 3 in section 8: no printed
    waveform exists for them.

1. **Confirm the Es/N0 definition.** Section 5 proposes MER on the data
    subcarriers after equalization, from ETSI TR 101 290 V1.4.1, and `auto`
    as Es/N0. The owner confirms before the wfmgen type; the modulator does
    not need it.

1. **The name of the constellation field.** Recommended: a new
    `constellation` field and `[[enum]] constellation`, appended, not qam
    values added to `modulation`, whose enum `bitmod` is bound to a C enum, a
    name table and the `bits` type (section 5). The cost is two spellings
    for a mapping: `modulation` on `bits`, `constellation` on `ofdm`.

1. **The spellings.** Recommended: the standard's own, `1/2`, `2/3`, `3/4`,
    `5/6` and `802.11a`; section 5 says what is checked first.

1. **The form of the MAC header in the first slice.** Recommended: raw
    octets (`mac_header`) only. The field form (Frame Control, Duration/ID,
    three addresses, Sequence Control) needs a six-octet address spelling no
    source field has, and is sugar over the same 24 octets, so it can follow
    the first slice without moving anything. The alternative declares both
    at once.

## 11. The work, in order

Sizes are rough and count hand-written C and tests, not generated files. The
modulator (PR 5) and the primitives it uses (puncturing, the parametric
CRC, `qam`) are on [ofdm](ofdm.md); PR 6 onward consume them.

| PR  | what                                                                                            | phases                        | size        |
| --- | ----------------------------------------------------------------------------------------------- | ----------------------------- | ----------- |
| 6   | `wlan`: layout and chain builders for non-HT, the minimal MAC; Annex I test when transcribed    | declare, implement, pin, bind | ~1400 lines |
| 7   | `wfm_synth` type `ofdm`: enum, source fields, `LayoutDesc` on the faces, bridge, capture, state | all of section 5              | ~1300 lines |
| 8   | characterization: window, mask, EVM, PAPR, speed; the reports for `ofdm` and `wlan`             | explore, certify              | ~800 lines  |
| 9   | guide section, C and Python examples, gallery decision, benches                                 | user story, land              | ~500 lines  |
| 10+ | HT 20 MHz; HT 40 MHz; VHT; LDPC; HE SU, each its own slice                                      |                               |             |

PR 6 needs the modulator (PR 5). PR 6's builders label unverified what
section 9 does not mark checked. Each pin in PRs 6 and 7 is proven by sabotage before the
Python face exists.

**What PR 7 checks first.** Before any C, `scripts/gen_wfm_defaults.py --check` on the new manifest rows, which is the generator that has not yet
seen a `/` in a choice or a second type-conditional field; then the one-field
description of section 5.

## Related

- [OFDM: a general modulator](ofdm.md): the object this page calls
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
