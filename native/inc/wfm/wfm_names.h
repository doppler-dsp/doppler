/*
 * wfm_names.h — SSOT name tables shared by the JSON scene serializer
 * (wfm_json.c) and the SigMF sidecar (wfm_writer.c).
 *
 * One definition: the [[enum]] manifest order IS the C int, append-only.
 * These existed as per-file copies until the wfm_writer one silently fell
 * behind (8 entries, no "dsss"; a `< 7` label guard that also dropped
 * "symbols") — the duplicated-table rot this header ends.
 */
#ifndef WFM_NAMES_H
#define WFM_NAMES_H

static const char *const TYPE_NAMES[]
    = { "tone",  "noise", "pn",      "bpsk", "qpsk",
        "chirp", "bits",  "symbols", "dsss" };
#define N_TYPES 9

static const char *const MODE_NAMES[] = { "auto", "fs", "ebno", "esno" };

/* Wire sample types, wavegen order. The complex five first, then the same
   five element encodings in BLUE's scalar mode -- APPEND only, because the
   index IS the manifest [[enum]] value and STYPE_FMT[]/KIND[] are indexed by
   it. wfmgen kept its own copy of the first five until doppler#1032, which
   is the third instance of exactly the rot this header was written to end. */
static const char *const STYPE_NAMES[]
    = { "cf32", "cf64", "ci32", "ci16", "ci8",
        "f32",  "f64",  "i32",  "i16",  "i8" };
#define N_STYPES 10

#endif /* WFM_NAMES_H */
