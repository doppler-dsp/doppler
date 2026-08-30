/*
 * wfm_names.h — the one C home for the waveform enum name tables.
 *
 * One definition per enum: the [[enum]] manifest order IS the C int,
 * append-only. Each table carries an `SSOT:` annotation naming the
 * `[[enum]]` in just-makeit.toml that owns its contents, the `N_*` macro
 * that sizes it, and the C enum that pins its indices where one exists.
 * `make lint-wfm-enum-tables` reads those annotations and holds all four to
 * each other, so the single-source-of-truth claim is a gate rather than a
 * comment. A table added here without an annotation FAILS that gate.
 *
 * These existed as per-file copies until the wfm_writer one silently fell
 * behind (8 entries, no "dsss"; a `< 7` label guard that also dropped
 * "symbols"), and wfmgen.c and wfm_json.c kept twelve and seven more
 * (doppler#760) — one of which, the `--data` source list, had already drifted
 * into the reverse order. That is the rot this header ends: list order IS the
 * enum value, so a copy that drifts maps a flag to the wrong waveform rather
 * than failing.
 */
#ifndef WFM_NAMES_H
#define WFM_NAMES_H

/* SSOT: enum=wfm_type, count=N_TYPES */
static const char *const TYPE_NAMES[]
    = { "tone",  "noise", "pn",      "bpsk", "qpsk",
        "chirp", "bits",  "symbols", "dsss" };
#define N_TYPES 9

/* SSOT: enum=snr_mode */
static const char *const MODE_NAMES[] = { "auto", "fs", "ebno", "esno" };

/* Wire sample types, wavegen order. The complex five first, then the same
   five element encodings in BLUE's scalar mode -- APPEND only, because the
   index IS the manifest [[enum]] value and STYPE_FMT[]/KIND[] are indexed by
   it. wfmgen kept its own copy of the first five until doppler#1032, which
   is the third instance of exactly the rot this header was written to end.
   SSOT: enum=stype, count=N_STYPES */
static const char *const STYPE_NAMES[]
    = { "cf32", "cf64", "ci32", "ci16", "ci8",
        "f32",  "f64",  "i32",  "i16",  "i8" };
#define N_STYPES 10

/* SSOT: enum=crc */
static const char *const CRC_NAMES[] = { "none", "crc16" };

/* SSOT: enum=bitmod */
static const char *const BITMOD_NAMES[] = { "none", "bpsk", "qpsk" };

/* SSOT: enum=ftype, cenum=wfm_writer/wfm_writer_core.h:wfm_filetype_t */
static const char *const FTYPE_NAMES[] = { "raw", "csv", "blue", "sigmf" };

/* SSOT: enum=endian */
static const char *const ENDIAN_NAMES[] = { "le", "be" };

/* SSOT: enum=wfm_lfsr */
static const char *const LFSR_NAMES[] = { "galois", "fibonacci" };

/* SSOT: enum=wfm_pulse */
static const char *const PULSE_NAMES[] = { "rect", "rrc" };

/* SSOT: enum=seed_advance, cenum=wfm/wfm_compose.h:wfm_seed_advance_t */
static const char *const SEED_ADVANCE_NAMES[] = { "none", "noise", "all" };

/* Ordered to match wfm_segment_t.gap_noise, which is a plain int whose value
   IS this index -- there is no enumerator for the gate to cross-check.
   SSOT: enum=gap_noise */
static const char *const GAP_NOISE_NAMES[] = { "auto", "off" };

/* --data's two sources, ordered to match wfm_source_t.dsss_code_only rather
   than to match the usage text: "prbs" is the seeded PN (code_only 0) and
   "none" is code-only (code_only 1), so the chosen index IS the field.
   wfm_json.c held the REVERSE of this order until doppler#760, harmless only
   because it compared the index instead of assigning it.
   SSOT: enum=data_src */
static const char *const DATA_SRC_NAMES[] = { "prbs", "none" };

/* Where a frame field's bits come from.
   SSOT: enum=wfm_seq_kind, cenum=wfm/wfm_frame.h:wfm_seq_kind_t */
static const char *const SEQ_KIND_NAMES[]
    = { "literal", "pn", "gold", "dotted" };

/* --randomise: WHICH section-10 generator, because 131.0-B-6 specifies two
   and they produce waveforms only the matching receiver derandomises.
   Index 0 is "off" so an absent flag and an explicit off are one value, and
   index 1 is B-6 10.4.1's -- the `shall` -- which is what OPT_CHOICE_OPT
   selects when the flag is given with no value. "legacy" is 10.4.2's 255-bit
   sequence, kept for backward compatibility and carrying spectral lines at
   1/255 of the symbol rate.
   SSOT: enum=randomise */
static const char *const RANDOMISE_NAMES[] = { "off", "ccsds", "legacy" };

#endif /* WFM_NAMES_H */
