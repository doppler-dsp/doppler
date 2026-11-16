---
description: "OFDM for wfmgen, proposed: a general OFDM modulator as the primitive, the IEEE 802.11 OFDM PHYs as presets over it, which existing objects each 802.11 block composes, how it is proven against the standard's own example, and what is still unknown"
---

# OFDM, and the IEEE 802.11 OFDM PHYs

*Part of [wfmgen](wfmgen.md), which owns the tool's goals, what it
composes, and what is still unknown about it.*

!!! note "Proposed"

    No part of this page is built. It is the proposal that precedes the
    code: the layering, the scope, what is reused, how it is proven, and
    the decisions that are the owner's. Section 11 lists those decisions
    with a recommendation each; sections 9 and 10 list what is not known.

wfmgen gains one waveform type, `ofdm`. It is a general OFDM modulator, and
the IEEE 802.11 OFDM PHYs are named presets over it. The scope is the PHY
only: the caller supplies the PSDU (or a PN fill stands in for one) and the
generator produces the PPDU's baseband samples at the standard's sample
rate. There is no MAC.

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

- **PSDU**: the octets the MAC hands the PHY. The caller's data.
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
- **spatial stream**: one of `N_SS` independently coded data streams.
    Distinct from an **output stream**, which is one sequence of `cf32`
    samples leaving wfmgen.
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
1. **PHY only.** No MAC header, no FCS, no aggregation, no channel access
    timing. Section 11 asks whether an FCS helper is an exception.

## 3. The layers

```text
FACES      wfmgen --type ofdm · doppler.wfm · JSON scene
             └── fill in wfm_source_t; hold no logic

SOURCE     wfm_synth, type = ofdm
             ├── standard = none ── general OFDM: the caller's map,
             │                      cyclic prefix and constellation
             └── standard = 802.11x ── wlan PPDU renderer
                                         │
PRESETS    wlan ── PSDU bits -> one PPDU's samples (stateless render)
             ├── fields: L-STF, L-LTF, L-SIG, Data (then HT-, VHT-, HE-)
             ├── scrambler ── pn
             ├── FEC ── conv, with puncturing (new, in conv)
             ├── interleaver ── dp_interleave + the 802.11 second step
             ├── mapper ── qam (new) · mpsk
             └── ofdm ── the modulator
                           │
PRIMITIVE  ofdm ── grid in, samples out
             ├── fft (inverse, cf32)
             └── subcarrier map · pilots · cyclic prefix · window
```

Three new components, each a C core first:

- **`ofdm`**, the modulator. Configured with `N_FFT`, a subcarrier map, a
    pilot description, a default cyclic prefix and a window length. One
    call takes `N_SD` constellation points per OFDM symbol and writes
    samples; a second takes a whole grid with its own cyclic-prefix length
    and repetition count, which is what a training field needs (the L-LTF
    is one double-length GI followed by two periods). Both go through one
    kernel: place the grid, inverse transform, prefix, window, overlap-add
    onto the previous OFDM symbol's tail.
- **`qam`**, the square Gray-coded QAM map for 16, 64, 256 and 1024
    points, unit average power. A sibling of `mpsk`.
- **`wlan`**, the 802.11 PHY: a table of PPDU formats, and a renderer from
    PSDU bits to one PPDU's samples. Its name is section 11's first
    question.

And one extension: **puncturing in `conv`**, as a pattern beside
`conv_code_t`, in both directions (delete on encode; insert erasures ahead
of `Viterbi`). It is general, not an 802.11 feature: the CCSDS punctured
rates are the same mechanism.

## 4. What each 802.11 PHY block composes

"Exists" means a caller reaches it today through a public C symbol.
Section 7 says which of these rows a prototype ran against the library.

| PHY block (clause)                    | state                | where                                                        | what is missing                                                            |
| ------------------------------------- | -------------------- | ------------------------------------------------------------ | -------------------------------------------------------------------------- |
| Inverse transform (17.3.2.6)          | exists               | `fft`: `dp_fft_create`, `dp_fft_execute_cf32`                | nothing; any length, unnormalized, plan reused across calls                |
| Scrambler, `x^7 + x^4 + 1` (17.3.5.5) | exists, lacks a map  | `pn`: `dp_pn_create` (Galois, poly `0x48`, length 7)         | the mapping from the standard's 7-bit initial state to a `pn` seed         |
| Pilot polarity `p_n` (17.3.5.10)      | exists               | the same `pn` sequence, all-ones start                       | nothing                                                                    |
| BCC, K = 7, 133/171 octal (17.3.5.6)  | exists               | `conv`: `conv_code_t` `{0133, 0171}`, `invert = 0`           | nothing for rate 1/2                                                       |
| Puncturing to 2/3, 3/4, 5/6           | absent               | `conv`, `viterbi`                                            | a puncture pattern, both directions                                        |
| Interleaver, first permutation        | exists               | `dp_interleave_index (k, N_CBPS/16, 16)`                     | nothing                                                                    |
| Interleaver, second permutation       | absent               |                                                              | the within-group bit rotation; 802.11-specific, lives in `wlan`            |
| Interleaver, third permutation (HT)   | absent               |                                                              | the per-stream frequency rotation; only for `N_SS > 1`                     |
| BPSK, QPSK map (Tables 17-8, 17-9)    | exists, other labels | `mpsk`: `mpsk_constellation`                                 | 802.11 maps bit 0 to the negative rail; `mpsk` maps it to the positive one |
| 16/64/256/1024-QAM map                | absent               |                                                              | the `qam` component                                                        |
| Subcarrier map, pilots, cyclic prefix | absent               |                                                              | the `ofdm` component                                                       |
| Window (17.3.2.5)                     | absent               |                                                              | in `ofdm`                                                                  |
| L-STF, L-LTF sequences (17.3.3)       | absent               |                                                              | two constant tables in `wlan`                                              |
| L-SIG: RATE, LENGTH, parity (17.3.4)  | absent               |                                                              | field assembly in `wlan`                                                   |
| SERVICE, tail, pad (17.3.5.2-4)       | absent               |                                                              | field assembly in `wlan`                                                   |
| HT-SIG CRC-8, VHT-SIG-B, HE-SIG CRC   | absent               | `dp_crc16_ccitt` is the only CRC                             | a CRC of another width and polynomial                                      |
| FCS, CRC-32                           | absent, out of scope |                                                              | MAC; see section 11                                                        |
| LDPC (19.3.11.7)                      | absent               |                                                              | the codec; a design page of its own                                        |
| Oversampling by an integer            | exists               | `fft` at `M * N_FFT` with a zero-padded grid                 | nothing                                                                    |
| Rate change by a non-integer          | exists               | `RateConverter`, `Resampler`                                 | nothing; applied by the caller, not inside the source                      |
| Spectral shaping filter               | exists               | `fir`                                                        | whether it is needed at all is unknown 9.2                                 |
| Carrier offset, AWGN, level, Doppler  | exists               | `wfm_synth` (`lo`, `awgn`), `wfm_compose`, `doppler_channel` | nothing; the common tail every source already has                          |
| PSDU from a data source or a PN fill  | exists               | the payload data source (`data`, `data_len`, `fill`)         | nothing                                                                    |
| Power spectrum, band power            | exists               | `psd`: `dp_psd_band_power`, `dp_psd_occupied_bw`             | a transmit-mask comparison over those bands                                |
| PAPR                                  | exists, real only    | `dp_tonemeas_time_stats`                                     | a complex-envelope PAPR                                                    |
| EVM of an OFDM grid                   | absent               |                                                              | relative constellation error per 17.3.9.7.4                                |

Two rows carry a decision rather than a gap:

- **BPSK and QPSK.** The 802.11 maps are the `mpsk` maps on complemented
    bits: the prototype maps all four QPSK pairs through label
    `b0 | b1 << 1` of the complemented pair, and both BPSK bits, onto
    Tables 17-8 and 17-9 as recalled. `wlan` calls `mpsk` through that
    complement, so the tree keeps one BPSK map and one QPSK map; `qam`
    covers 16 points and up.
- **The scrambler seed.** `pn` reproduces the sequence, but at a phase
    offset from the standard's (four chips for the all-ones state), so the
    standard's initial state is not the `pn` seed and neither is its bit
    reverse. All 127 states are reachable by some seed.
    The conversion is a derivation, written once in `wlan` and pinned
    against Annex I's scrambled bits.

## 5. Where a waveform type plugs in

A waveform type is one entry in the `wfm_type` enum and whatever its
neighbors needed. For `dsss`, the most recent, that is:

| layer            | what the type provides                                                                                       |
| ---------------- | ------------------------------------------------------------------------------------------------------------ |
| enum             | a `[[enum]]` value in `just-makeit.toml`, the same entry in `native/inc/doppler/wfm/wfm_names.h`, the C enum |
| C core           | a branch in `wfm_synth`'s symbol source, a `set_*` call that installs its configuration, `reset`             |
| state            | its running fields in the `wfm_synth` blob, a version bump, its configuration as reject keys                 |
| source struct    | fields on `wfm_source_t`, each declared once in `modules/wfm_compose.toml` with its CLI and JSON spelling    |
| bridge           | validation and attachment in `wfm_synth_bridge.c`; SNR conversion in `wfm_resolve.c`                         |
| faces            | generated: CLI option table, JSON reader and writer, schema, Python keyword, options reference               |
| capture metadata | the type's name and parameters in the SigMF and BLUE writers                                                 |
| C tests          | `native/tests/` pins, the state round trip, the mid-stream split                                             |
| Python tests     | the state matrix, `test_dsp_correctness.py`, the surface round trip, the flag matrix                         |
| certification    | the `wfm_synth` report's claims for the type                                                                 |
| user story       | a section in `docs/guide/wfmgen/waveforms.md`, a C and a Python example, a gallery decision, a bench row     |

`ofdm` provides the same, with one structural difference: its samples are
not a held constellation point times a carrier. The source is a rendered
buffer of samples, one PPDU long, read out sample by sample and refilled
per frame, which is the shape `dp_wfm_synth_set_symbols` and
`dp_wfm_synth_set_refill` already have between them. The common tail
(carrier, AWGN) applies unchanged.

### On the faces

Proposed fields on a source, all refused for any other type:

| field                                         | meaning                                                                                            |
| --------------------------------------------- | -------------------------------------------------------------------------------------------------- |
| `standard`                                    | `none` or a preset name; a preset fixes every field below that the standard fixes                  |
| `bandwidth`                                   | channel bandwidth in Hz; the segment's `fs` is an integer multiple of it, or the source is refused |
| `modulation`                                  | the existing field, extended with `qam16`, `qam64`, `qam256`, `qam1024`                            |
| `code_rate`                                   | `1/2`, `2/3`, `3/4`, `5/6`; non-HT takes this pair, refusing a pair Table 17-4 does not list       |
| `mcs`                                         | HT and later: the standard's index                                                                 |
| `gi`                                          | guard interval where the format offers a choice                                                    |
| `scrambler_init`                              | the 7-bit initial state; section 11                                                                |
| `fft_size`, `cp_len`, `subcarriers`, `pilots` | `standard = none` only: the caller's own OFDM                                                      |

The PSDU is the existing payload data source: `data` / `data_from_file`,
`data_len` bits per PPDU, `fill`, one PPDU per frame, exactly as a DSSS
burst is one burst per frame. A scene:

```text
{
  "version": 1,
  "segments": [
    {
      "type": "ofdm", "fs": 20e6,
      "standard": "802.11a", "bandwidth": 20e6,
      "modulation": "qam16", "code_rate": "3/4",
      "data": "pn:800:15", "data_len": 800
    }
  ]
}
```

`standard`, `bandwidth`, `code_rate` and the `qam16` value are proposals;
every other key is today's.

### State, and reject keys

- **`ofdm` (the modulator)** carries the previous OFDM symbol's window
    tail and the pilot polarity index. Its blob holds both. `N_FFT`, the
    window length (it sizes the tail) and a `dp_hash64` of the subcarrier
    map and pilot description (they change what the blob means) are reject
    keys.
- **`wlan` (the renderer)** is a pure function of its configuration and
    the PSDU, so it is stateless and takes a line in
    `scripts/.serializable-stateless`.
- **`wfm_synth`, type `ofdm`** carries a cursor into the rendered PPDU and
    the frame in play (which it already carries for a refilled source). The
    rendered samples are not in the blob: `set_state` re-renders them from
    the frame, as `next_edge` is recomputed today. The preset, the
    bandwidth, the modulation and coding, the GI and the window length are
    reject keys; each sizes the PPDU or changes its meaning.

## 6. The 802.11 flavors, in order

Each row says what it adds over the row above. "1 SS" is one spatial
stream on one transmit chain.

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
| 11  | more than one spatial stream                     | HT MCS 8-31, VHT and HE `N_SS > 1`, STBC, cyclic shift diversity across transmit chains: more than one output stream                                                                                   |

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
1. It needs no decision on multiple output streams, LDPC or a second CRC.

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
    length per frame today; a scene with a different length per PPDU has no
    spelling.

## 10. Standard-fixed numbers, none verified against the text

Every number below is from memory. Those marked *(cross-checked)* agree
with section 7's independent computation.

| number                                                                                                                      | cited as                   |
| --------------------------------------------------------------------------------------------------------------------------- | -------------------------- |
| Clause numbers: OFDM 17, ERP 18, HT 19, VHT 21, HE 27                                                                       | 802.11-2020, 802.11ax-2021 |
| The worked example is Annex I                                                                                               | 802.11-2020                |
| `N_FFT` 64, `Δf` 312.5 kHz, `N_SD` 48, `N_SP` 4; GI 0.8 µs, OFDM symbol 4 µs; L-STF 8 µs, L-LTF 8 µs                        | Table 17-5                 |
| pilots on subcarriers -21, -7, 7, 21 with values 1, 1, 1, -1                                                                | 17.3.5.9, 17.3.5.10        |
| eight rates and their modulation and coding pairs; RATE bits                                                                | Tables 17-4, 17-6          |
| scrambler `x^7 + x^4 + 1` and its 127-bit sequence *(cross-checked)*                                                        | 17.3.5.5                   |
| BCC generators 133 and 171 octal, output A first *(cross-checked at 1/2)*                                                   | 17.3.5.6                   |
| puncture patterns for 2/3 and 3/4                                                                                           | Figure 17-9                |
| interleaver permutations, 16 columns *(first cross-checked)*                                                                | 17.3.5.7                   |
| constellation maps and `K_MOD` (1, 1/√2, 1/√10, 1/√42)                                                                      | Tables 17-7 to 17-11       |
| L-STF and L-LTF sequences, the √(13/6) factor *(cross-checked)*                                                             | 17.3.3                     |
| window, `T_TR` 100 ns as an example                                                                                         | 17.3.2.5                   |
| transmit mask corner points; flatness limits; relative constellation error limits                                           | 17.3.9.3, 17.3.9.7         |
| Annex I example: 100 octets, 36 Mb/s, scrambler initial state 1011101 *(PSDU cross-checked)*                                | Annex I                    |
| ERP-OFDM signal extension, 6 µs                                                                                             | Clause 18                  |
| HT: 52 + 4 subcarriers at 20 MHz, 108 + 6 at 40 MHz, 13 and 18 interleaver columns, short GI 0.4 µs, rate 5/6, HT-SIG CRC-8 | Clause 19                  |
| VHT: `N_FFT` 256 and 512, 234 + 8 subcarriers at 80 MHz, 256-QAM at MCS 8 and 9                                             | Clause 21                  |
| HE: `Δf` 78.125 kHz, GI 0.8 / 1.6 / 3.2 µs, 1024-QAM at MCS 10 and 11                                                       | Clause 27                  |
| `K_MOD` for 256-QAM and 1024-QAM (1/√170, 1/√682)                                                                           | Clauses 21, 27             |

## 11. Decisions that are the owner's

Each has a recommendation first.

1. **The name of the 802.11 component.** Recommended: `wlan`. A C
    identifier cannot begin with a digit, "Wi-Fi" is a certification mark
    rather than the standard's word, and "wireless LAN" is the standard's
    own title. On the faces the preset is spelled as users say it:
    `802.11a`, `802.11g`, `802.11p`, `802.11j`, `802.11n`.
1. **One type, or two.** Recommended: one type `ofdm` with a `standard`
    discriminator, as `dsss` has `symbol_rate`, rather than a `wlan` type
    beside it.
1. **Both an object and a type.** Recommended: both. The modulator, the
    QAM map and the renderer are certified as objects first; the `ofdm`
    type is the composition.
1. **The first slice.** Recommended: section 6's rows 1 to 3.
1. **Multiple output streams.** Recommended: out of scope until wfmgen
    itself has more than one output stream, which is a design page of its
    own across `Synth`, the composer and the writers. Until then every
    flavor is 1 SS on one transmit chain.
1. **LDPC.** Recommended: its own design page, ahead of HE and after VHT.
1. **How a non-HT rate is spelled.** Recommended: `modulation` and
    `code_rate`, the two columns of Table 17-4 that do not change with
    clocking. Mb/s names a different pair at 10 MHz than at 20 MHz.
1. **The scrambler's initial state.** The standard asks for a
    pseudorandom nonzero state per PPDU. Recommended: drawn per PPDU from
    the source's `seed` through the composer's draw hash, so it is
    reproducible and readable back, with `scrambler_init` to fix it (the
    Annex I test fixes it).
1. **The FCS.** Recommended: none; the PSDU is the caller's octets,
    FCS included or not. The alternative is a helper that appends CRC-32,
    which is MAC, and which makes a generated PPDU acceptable to a real
    receiver without the caller computing anything.
1. **The standard's text.** Section 8's first two proofs need Annex I and
    the Clause 17 tables transcribed from the text, and section 10 needs
    checking against it.
1. **Where `qam` lives on the Python face.** Recommended: beside `mpsk`,
    in the same module.

## 12. The work, in order

Sizes are rough and count hand-written C and tests, not generated files.

| PR  | what                                                                                       | phases                                    | size        |
| --- | ------------------------------------------------------------------------------------------ | ----------------------------------------- | ----------- |
| 1   | this page                                                                                  | why                                       | this page   |
| 2   | puncturing in `conv`, erasure insertion ahead of `Viterbi`; the `conv` report's new claims | declare to certify                        | ~400 lines  |
| 3   | `qam`: map and hard demap, 16 to 1024 points                                               | declare to user story                     | ~400 lines  |
| 4   | `ofdm`: the modulator, its state triplet, bench                                            | declare, implement, pin, bind, instrument | ~900 lines  |
| 5   | `wlan`: the non-HT renderer and the Annex I test                                           | declare, implement, pin, bind             | ~1200 lines |
| 6   | `wfm_synth` type `ofdm`: enum, source fields, bridge, faces, capture metadata, state       | all of section 5                          | ~1000 lines |
| 7   | characterization: window, mask, EVM, PAPR, speed; the reports for `ofdm` and `wlan`        | explore, certify                          | ~800 lines  |
| 8   | guide section, C and Python examples, gallery decision, benches                            | user story, land                          | ~500 lines  |
| 9+  | HT 20 MHz; HT 40 MHz; VHT; LDPC; HE SU, each its own slice                                 |                                           |             |

PRs 2 and 3 are independent of each other and of 4. PR 5 needs 2, 3 and
4\. Each pin in PRs 2 to 6 is proven by sabotage before the Python face
exists.

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
