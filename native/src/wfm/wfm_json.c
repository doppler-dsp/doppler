/*
 * wfm_json.c — JSON spec (de)serialisation for the composer (Phase B).
 *
 * One canonical, sample-exact schema shared by `--record` (write) and
 * `--from-file` (read), so a recorded run reproduces byte-for-byte. Uses the
 * vendored cJSON.
 */
#include "doppler/wfm/wfm_compose.h"

#include "doppler/dp_complex.h"
#include <stddef.h> /* offsetof — the frame key tables name members once */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

/* Every enum name table this file spells comes from wfm_names.h, the one C
   home for them (doppler#760). Seven were declared here until then. One had
   already drifted: `DATA_NAMES` listed {"none","prbs"}, the reverse of
   wfmgen's, which was harmless only because this file compared its index to
   a literal instead of assigning it -- the shared table's order IS
   wfm_source_t.dsss_code_only, and read_dsss_source now assigns it. */
#include "doppler/wfm/wfm_defaults.h" /* DEF_SRC / DEF_SEG */
#include "doppler/wfm/wfm_names.h"
#include "doppler/wfm/wfm_surface.h" /* the field rows, generated */

/* A key a scene omits takes the MANIFEST's default -- the value the flags and
 * Python give for the same parameter -- read from the generated initialisers
 * rather than typed here. Typed here, they drifted: seed/sps/pn_length read
 * 1/8/7 against 0/1/15, and an omitted num_samples made an empty segment
 * (doppler#1596), the same way `fs` once rendered a tone at DC. */
static const wfm_source_t  DEF_SRC = WFM_SOURCE_DEFAULTS;
static const wfm_segment_t DEF_SEG = WFM_SEGMENT_DEFAULTS;

static int
name_index (const char *s, const char *const *names, int n)
{
  if (s)
    for (int i = 0; i < n; i++)
      if (strcmp (s, names[i]) == 0)
        return i;
  return -1;
}

/* Render a bits pattern as a malloc'd "0/1" string (caller frees), or NULL. */
static char *
bits_to_string (const uint8_t *bits, size_t n)
{
  char *s = malloc (n + 1);
  if (!s)
    return NULL;
  for (size_t i = 0; i < n; i++)
    s[i] = bits[i] ? '1' : '0';
  s[n] = '\0';
  return s;
}

/* Parse a "0/1" string into a malloc'd bit array; *n gets the length. Other
 * characters are skipped. Returns NULL on allocation failure. */
static uint8_t *
string_to_bits (const char *s, size_t *n)
{
  size_t   len = strlen (s);
  uint8_t *b   = malloc (len ? len : 1);
  if (!b)
    return NULL;
  size_t k = 0;
  for (size_t i = 0; i < len; i++)
    if (s[i] == '0' || s[i] == '1')
      b[k++] = (uint8_t)(s[i] - '0');
  *n = k;
  return b;
}

/* Free the per-source heap arrays (bits/symbols/dsss codes) of `ns` sources
 * (then the caller frees the array). */
static void
free_src_bits (wfm_source_t *srcs, size_t ns)
{
  if (srcs)
    for (size_t k = 0; k < ns; k++)
      {
        free ((void *)srcs[k].payload.bits);
        free (srcs[k].symbols);
        free ((void *)srcs[k].acq_code.bits);
        free ((void *)srcs[k].data_code.bits);
        free ((void *)srcs[k].sync.bits);
        /* A CARRIED description, when it came from JSON. `wfm_source_t`
           borrows a frame -- a caller's own outlives the source -- but one
           this file parsed has no other owner, the same asymmetry the const
           bit arrays above already carry. Fields first: their literal bits
           hang off the description being freed. */
        if (srcs[k].frame)
          {
            wfm_frame_desc_t *d = (wfm_frame_desc_t *)srcs[k].frame;
            for (unsigned f = 0; f < d->n_fields; f++)
              free ((void *)d->field[f].seq.bits);
            free (d);
            srcs[k].frame = NULL;
          }
      }
}

/* Emit the coding STAGES a source's frame carries, and only those that are
 * on: a record is what makes a capture reproducible, so a stage the record
 * does not carry is a capture nobody can rebuild -- and the omission would
 * look exactly like a plain uncoded waveform. Written conditionally so an
 * uncoded record is byte-identical to what it was before coding existed.
 *
 * NOT type-gated, for the reason add_frame_fields() gives about the frame
 * itself: these lived inside the bits path, so a coded DSSS burst recorded
 * its geometry and dropped its stages, and --from-file rebuilt an uncoded
 * waveform from a record that looked complete (doppler#1017). One writer,
 * every source that can carry a stage. */
static void
add_stage_fields (cJSON *o, const wfm_source_t *src)
{
  if (src->rs_depth)
    cJSON_AddNumberToObject (o, "rs_depth", (double)src->rs_depth);
  /* WHICH generator, not merely that one ran: 131.0-B-6 specifies two and
     only the matching receiver derandomises a given waveform, so a record
     carrying a bare `true` could not rebuild the capture it describes. */
  if (src->randomise)
    cJSON_AddStringToObject (o, "randomise",
                             RANDOMISE_NAMES[src->randomise == 2 ? 2 : 1]);
  if (src->attach_asm)
    cJSON_AddBoolToObject (o, "asm", 1);
  if (src->convolutional)
    cJSON_AddBoolToObject (o, "conv", 1);
  /* The UNIT travels with the depth, and is not defaulted away on the write
     side: 1 and 8 are different waveforms over the same data group, and a
     record that carried only the depth would replay an octet-interleaved
     capture as a bit-interleaved one -- the same length, different bytes,
     no error. Written only when the interleaver ran, so an uncoded record
     still carries no coding keys. */
  if (src->interleave_depth)
    {
      cJSON_AddNumberToObject (o, "interleave", (double)src->interleave_depth);
      cJSON_AddNumberToObject (o, "interleave_unit",
                               (double)(src->interleave_unit_bits
                                            ? src->interleave_unit_bits
                                            : 1u));
    }
}

/* Emit a bits source's modulation + pattern (no-op for other types). */
static void
add_bits_fields (cJSON *o, const wfm_source_t *src)
{
  if (src->type != WFM_SYNTH_BITS)
    return;
  if (src->payload.bits && src->payload.len)
    {
      char *bs = bits_to_string (src->payload.bits, src->payload.len);
      if (bs)
        {
          cJSON_AddStringToObject (o, "pattern", bs);
          free (bs);
        }
    }
}

/* Emit a "0/1" string field from a bit array (no-op when empty/OOM). */
static void
add_bit_string (cJSON *o, const char *key, const uint8_t *bits, size_t n)
{
  if (!bits || !n)
    return;
  char *s = bits_to_string (bits, n);
  if (s)
    {
      cJSON_AddStringToObject (o, key, s);
      free (s);
    }
}

/* A 64-bit mask as a hex STRING, not a JSON number.
 *
 * `poly`, `seed` and the Gold taps are `uint64_t`, and a JSON number is a
 * double: anything above 2^53 does not survive the round trip. A register
 * width of 64 is inside `wfm_seq_t`'s documented range, so that is not a
 * theoretical loss -- it is the top of the range this field is FOR. Hex also
 * reads as what it is, a tap mask, which decimal does not. */
static void
add_u64_hex (cJSON *o, const char *key, uint64_t v)
{
  char buf[19];
  snprintf (buf, sizeof buf, "0x%llx", (unsigned long long)v);
  cJSON_AddStringToObject (o, key, buf);
}

/* Emit a GENERATED sequence's parameters under @p key.
 *
 * A generated sequence has no array to record -- carrying `(poly, seed,
 * reg_bits)` instead of a million-symbol run is its entire point, and what
 * makes a long capture reproducible from its metadata. `add_bit_string`
 * writes NOTHING for one, because there are no bits, so without this the
 * field would vanish from a `--record` and `--from-file` would rebuild an
 * unframed waveform: the same silent-unframed shape `add_frame_fields`
 * warns about below for the type gate.
 *
 * A LITERAL emits nothing here and is recorded by `add_bit_string` exactly
 * as it always was, so every record written before generated kinds existed
 * is byte-identical to the one written now. That matters: the 1-source
 * inline form's field order is frozen. */
static void
add_seq_gen (cJSON *o, const char *key, const wfm_seq_t *q)
{
  if (!q || q->kind == WFM_SEQ_LITERAL || q->len == 0)
    return;
  /* No OOM branch. Every cJSON_Add* below and the closing AddItemToObject
     are no-ops on a NULL object, so a failed allocation omits the key here
     exactly as an early return would -- and an unwind path no test can reach
     is a line the patch-coverage gate cannot accept. Same reasoning as the
     abort-on-OOM allocation helpers, and the same shape as the sibling
     builders in this file that already omit the check. */
  cJSON *g = cJSON_CreateObject ();
  cJSON_AddStringToObject (g, "kind", SEQ_KIND_NAMES[q->kind]);
  cJSON_AddNumberToObject (g, "len", (double)q->len);
  if (q->kind == WFM_SEQ_PN)
    {
      cJSON_AddNumberToObject (g, "reg_bits", (double)q->reg_bits);
      add_u64_hex (g, "poly", q->poly);
      add_u64_hex (g, "seed", q->seed);
      cJSON_AddNumberToObject (g, "lfsr", (double)q->lfsr);
    }
  else if (q->kind == WFM_SEQ_GOLD)
    {
      cJSON_AddNumberToObject (g, "reg_bits", (double)q->reg_bits);
      add_u64_hex (g, "taps_a", q->taps_a);
      add_u64_hex (g, "seed_a", q->seed_a);
      add_u64_hex (g, "taps_b", q->taps_b);
      add_u64_hex (g, "seed_b", q->seed_b);
    }
  /* DOTTED carries no parameters -- kind and len say all of it. */
  cJSON_AddItemToObject (o, key, g);
}

/* Emit the FRAME a source carries — the preamble, its repetitions, the sync
 * word and the CRC choice. Deliberately NOT type-gated: an unspread `bits`
 * source can be framed too, and gating this on dsss is how a framed bits
 * --record came to omit the frame entirely, so --from-file silently rebuilt an
 * unframed waveform. `dp_wfm_source_has_frame()` is the same predicate the
 * generator uses, so what is recorded is exactly what was applied.
 *
 * The payload is NOT here: a bits source already emits it as "pattern" via
 * add_bits_fields(), and dsss emits it as "payload" below. */
static void
add_frame_fields (cJSON *o, const wfm_source_t *src)
{
  if (!dp_wfm_source_has_frame (src))
    return;
  add_bit_string (o, "acq_code", src->acq_code.bits, src->acq_code.len);
  add_seq_gen (o, "acq_code_gen", &src->acq_code);
  cJSON_AddNumberToObject (o, "acq_reps", (double)src->acq_reps);
  add_bit_string (o, "sync", src->sync.bits, src->sync.len);
  add_seq_gen (o, "sync_gen", &src->sync);
  cJSON_AddStringToObject (o, "crc", CRC_NAMES[src->crc ? 1 : 0]);
}

/* Emit a dsss source's burst geometry: the two codes, preamble repetitions,
 * sync word, payload bits, and CRC choice (no-op for other types). */
static void
add_dsss_fields (cJSON *o, const wfm_source_t *src)
{
  /* The payload's GENERATOR, emitted here because this is the one helper
     BOTH emit paths reach for EVERY type -- the frozen 1-source inline form
     and the multi-source `sum` each build their own field list, so a key
     added to only one of them is a key half the records lose. It sits beside
     its two literal spellings rather than inside them ("pattern" on a bits
     source, "payload" on a dsss one): one field under two names, and a
     generated payload has bits for neither to write. */
  add_seq_gen (o, "payload_gen", &src->payload);
  if (src->type != WFM_SYNTH_DSSS)
    {
      /* Not spread, but possibly framed. */
      add_frame_fields (o, src);
      return;
    }
  /* CONTINUOUS (symbol_rate > 0): only the spreading code, the payload (when
     one drives the data), and symbol_rate — no preamble/sync/CRC frame. Emit
     just those so a continuous record round-trips clean, without the spurious
     "acq_reps"/"crc" the burst path always writes. */
  if (src->symbol_rate > 0.0)
    {
      add_bit_string (o, "data_code", src->data_code.bits, src->data_code.len);
      add_seq_gen (o, "data_code_gen", &src->data_code);
      add_bit_string (o, "payload", src->payload.bits, src->payload.len);
      if (src->dsss_code_only) /* omit for the data-modulated default */
        cJSON_AddStringToObject (o, "data", "none");
      return;
    }
  add_bit_string (o, "acq_code", src->acq_code.bits, src->acq_code.len);
  add_seq_gen (o, "acq_code_gen", &src->acq_code);
  cJSON_AddNumberToObject (o, "acq_reps", (double)src->acq_reps);
  add_bit_string (o, "data_code", src->data_code.bits, src->data_code.len);
  add_seq_gen (o, "data_code_gen", &src->data_code);
  add_bit_string (o, "sync", src->sync.bits, src->sync.len);
  add_seq_gen (o, "sync_gen", &src->sync);
  add_bit_string (o, "payload", src->payload.bits, src->payload.len);
  cJSON_AddStringToObject (o, "crc", CRC_NAMES[src->crc ? 1 : 0]);
}

/* Emit a symbols source's complex constellation as a flat interleaved
 * [re, im, re, im, …] JSON array (no-op for other types). Doubles represent
 * the float _Complex samples exactly, so the round-trip is lossless. */
static void
add_symbols_fields (cJSON *o, const wfm_source_t *src)
{
  if (src->type != WFM_SYNTH_SYMBOLS || !src->symbols || !src->n_symbols)
    return;
  cJSON *arr = cJSON_CreateArray ();
  if (!arr)
    return;
  for (size_t i = 0; i < src->n_symbols; i++)
    {
      cJSON_AddItemToArray (
          arr, cJSON_CreateNumber ((double)crealf (src->symbols[i])));
      cJSON_AddItemToArray (
          arr, cJSON_CreateNumber ((double)cimagf (src->symbols[i])));
    }
  cJSON_AddItemToObject (o, "symbols", arr);
}

/* cJSON number field with a fallback when absent/non-numeric. */
static double
num (const cJSON *obj, const char *key, double fallback)
{
  const cJSON *it = cJSON_GetObjectItemCaseSensitive (obj, key);
  return cJSON_IsNumber (it) ? it->valuedouble : fallback;
}

/* Read `key` as either a scalar (→ returns it, *ranged = 0) or a two-element
 * [lo, hi] array (→ returns lo, sets *hi and *ranged = 1). A ranged field is
 * redrawn uniformly each repeat by the composer; see wfm_compose.h. Falls back
 * to `fallback` when the key is absent or malformed. */
static double
num_or_range (const cJSON *obj, const char *key, double fallback, double *hi,
              int *ranged)
{
  const cJSON *it = cJSON_GetObjectItemCaseSensitive (obj, key);
  if (cJSON_IsArray (it) && cJSON_GetArraySize (it) == 2)
    {
      const cJSON *a = cJSON_GetArrayItem (it, 0);
      const cJSON *b = cJSON_GetArrayItem (it, 1);
      if (cJSON_IsNumber (a) && cJSON_IsNumber (b))
        {
          *hi     = b->valuedouble;
          *ranged = 1;
          return a->valuedouble;
        }
    }
  *ranged = 0;
  return cJSON_IsNumber (it) ? it->valuedouble : fallback;
}

/* Emit `key` as a scalar, or as a [lo, hi] array when `ranged`. */
static void
add_num_or_range (cJSON *o, const char *key, double lo, double hi, int ranged)
{
  if (ranged)
    {
      cJSON *arr = cJSON_CreateArray ();
      cJSON_AddItemToArray (arr, cJSON_CreateNumber (lo));
      cJSON_AddItemToArray (arr, cJSON_CreateNumber (hi));
      cJSON_AddItemToObject (o, key, arr);
    }
  else
    cJSON_AddNumberToObject (o, key, lo);
}

/* wfm_names.h's STAGE_KIND_NAMES deliberately carries no `cenum=`: the enum
   ends in WFM_STAGE_USER, a BOUNDARY a caller allocates above rather than a
   sixth name, and the gate's cenum check (enumerator count, and each explicit
   value equal to its index) cannot express that. The ordering that table
   relies on is pinned HERE instead, where wfm_frame.h is in scope -- so a
   reordered enum is a compile error rather than a scene file that silently
   names the wrong stage. */
_Static_assert (WFM_STAGE_CRC16 == 0 && WFM_STAGE_RS == 1
                    && WFM_STAGE_RANDOMISE == 2 && WFM_STAGE_CONV == 3
                    && WFM_STAGE_INTERLEAVE == 4 && N_STAGE_KINDS == 5,
                "STAGE_KIND_NAMES is indexed by wfm_stage_kind_t");
_Static_assert (WFM_STAGE_USER >= N_STAGE_KINDS,
                "a named stage kind must not collide with a caller's own");

/* One numeric member of a frame's field or stage, named ONCE for both
 * directions.
 *
 * A stage carries six of these and a field three. That is the size at which
 * a hand-written emitter list and a hand-written parser list start to
 * disagree, and the disagreement is silent: every key here has a meaningful
 * zero default, so one added to the writer and forgotten in the reader
 * produces a record that reads back as a DIFFERENT frame rather than as an
 * error. `read_frame_fields` already drives its bit arrays from a table for
 * this reason; this is the same move applied to both directions at once.
 *
 * @p wide says which member type the offset points at, because a field mixes
 * `size_t` (reps, bits) with `unsigned` (derived_by), and reading one as the
 * other is precisely the corruption a shared list exists to prevent. */
typedef struct
{
  const char *key;
  size_t      off;  /**< offsetof into the owning struct   */
  int         wide; /**< 1 = size_t member, 0 = unsigned   */
} num_key_t;

#define STAGE_KEY(m) { #m, offsetof (wfm_stage_t, m), 0 }
static const num_key_t STAGE_KEYS[] = {
  STAGE_KEY (first_field), STAGE_KEY (n_fields), STAGE_KEY (depth),
  STAGE_KEY (unit_bits),   STAGE_KEY (emit_num), STAGE_KEY (emit_den),
};
#define N_STAGE_KEYS (sizeof STAGE_KEYS / sizeof *STAGE_KEYS)

static const num_key_t FIELD_KEYS[] = {
  { "reps", offsetof (wfm_field_t, reps), 1 },
  { "bits", offsetof (wfm_field_t, bits), 1 },
  { "derived_by", offsetof (wfm_field_t, derived_by), 0 },
};
#define N_FIELD_KEYS (sizeof FIELD_KEYS / sizeof *FIELD_KEYS)

/* Write every non-zero member named by @p keys. A zero is OMITTED rather
   than written, which is the rule the rest of this file keeps and is what
   makes read_num_keys()'s 0 default the exact inverse of this. */
static void
add_num_keys (cJSON *o, const void *base, const num_key_t *keys, size_t n)
{
  for (size_t i = 0; i < n; i++)
    {
      const void  *p = (const char *)base + keys[i].off;
      const double v = keys[i].wide ? (double)*(const size_t *)p
                                    : (double)*(const unsigned *)p;
      if (v != 0.0)
        cJSON_AddNumberToObject (o, keys[i].key, v);
    }
}

/* Read every member named by @p keys; an absent key is 0. The inverse of
   add_num_keys, from the same list -- which is the whole point of the list. */
static void
read_num_keys (const cJSON *o, void *base, const num_key_t *keys, size_t n)
{
  for (size_t i = 0; i < n; i++)
    {
      void        *p = (char *)base + keys[i].off;
      const double v = num (o, keys[i].key, 0);
      if (keys[i].wide)
        *(size_t *)p = (size_t)v;
      else
        *(unsigned *)p = (unsigned)v;
    }
}

/* A stage's kind: the NAME when this build has one, the raw integer when it
 * does not.
 *
 * The kind is an open `uint32_t`. doppler names 0..4 and promises never to
 * allocate at or above WFM_STAGE_USER, which is where a caller's own kinds
 * live. A name-only encoding could not carry those at all -- it would turn
 * "a mission that is not CCSDS" back into a pull request against a header --
 * and a number-only one would spell doppler's own stages as magic constants
 * in a file people are expected to read and edit. So both, and the reader
 * takes either. */
static void
add_stage_kind (cJSON *o, uint32_t kind)
{
  if (kind < N_STAGE_KINDS)
    cJSON_AddStringToObject (o, "kind", STAGE_KIND_NAMES[kind]);
  else
    cJSON_AddNumberToObject (o, "kind", (double)kind);
}

/* Read a kind written either way. Returns 0, or -1 when the key is absent,
   is neither string nor number, or names a stage this build does not know.
   REFUSING matters more here than anywhere else in this file: kind 0 is
   crc16, so a defaulting reader would turn every typo into a CRC stage. */
static int
read_stage_kind (const cJSON *o, uint32_t *out)
{
  const cJSON *k = cJSON_GetObjectItemCaseSensitive (o, "kind");
  if (cJSON_IsString (k))
    {
      const int i
          = name_index (k->valuestring, STAGE_KIND_NAMES, N_STAGE_KINDS);
      if (i < 0)
        return -1;
      *out = (uint32_t)i;
      return 0;
    }
  if (cJSON_IsNumber (k) && k->valuedouble >= 0.0)
    {
      *out = (uint32_t)k->valuedouble;
      return 0;
    }
  return -1;
}

/* Emit the frame description a source CARRIES, when it carries one.
 *
 * `wfm_source_t.frame` is a frame the caller BUILT rather than one derived
 * from the flat framing fields, and it says things they cannot: a field of
 * the caller's own bits at a position of their choosing, a stage covering a
 * span they name. Without this key a `--record` of such a source would write
 * the flat fields alone and `--from-file` would rebuild the DERIVED frame --
 * a different waveform, silently. That is the same failure add_frame_fields()
 * describes for a framed source recorded unframed, one level up.
 *
 * Written only when a description is carried, so every record from a source
 * without one stays byte-identical to what it was before this existed. */
static void
add_frame_desc (cJSON *o, const wfm_source_t *src)
{
  const wfm_frame_desc_t *d = src->frame;
  if (!d)
    return;

  cJSON *fr     = cJSON_CreateObject ();
  cJSON *fields = cJSON_AddArrayToObject (fr, "fields");
  for (unsigned i = 0; i < d->n_fields; i++)
    {
      const wfm_field_t *f  = &d->field[i];
      cJSON             *fo = cJSON_CreateObject ();
      if (f->name[0])
        cJSON_AddStringToObject (fo, "name", f->name);
      /* Literal bits as the same "0/1" string every other array in this file
         uses; a generated field carries its parameters instead, through the
         shared add_seq_gen. The two are mutually exclusive, and read_seq_gen
         refuses a record that carries both. */
      if (f->seq.kind == WFM_SEQ_LITERAL && f->seq.bits && f->seq.len)
        {
          char *s = bits_to_string (f->seq.bits, f->seq.len);
          if (s)
            {
              cJSON_AddStringToObject (fo, "lit", s);
              free (s);
            }
        }
      add_seq_gen (fo, "gen", &f->seq);
      add_num_keys (fo, f, FIELD_KEYS, N_FIELD_KEYS);
      cJSON_AddItemToArray (fields, fo);
    }

  cJSON *stages = cJSON_AddArrayToObject (fr, "stages");
  for (unsigned i = 0; i < d->n_stages; i++)
    {
      const wfm_stage_t *s  = &d->stage[i];
      cJSON             *so = cJSON_CreateObject ();
      add_stage_kind (so, s->kind);
      add_num_keys (so, s, STAGE_KEYS, N_STAGE_KEYS);
      cJSON_AddItemToArray (stages, so);
    }
  cJSON_AddItemToObject (o, "frame", fr);
}

/* ── the surface rows: every table field, both directions ──────────────
 *
 * A source's and a segment's plain fields are rows of the generated surface
 * table (wfm/wfm_surface.h), and these two functions are the whole of their
 * JSON face: one writes every row, the other reads every row, from the same
 * table. A field cannot be spelled one way here and another on the command
 * line, or written by one direction and forgotten by the other -- the
 * failure the frame key tables below were already built to prevent.
 *
 * Each row carries its JSON policy from the manifest: omitted at its
 * default (so a record does not churn for a field it does not use), written
 * only while another row's choice has a given value (`f_end` for a chirp),
 * written as a bool, or required. A default is never restated: it is read
 * from WFM_SOURCE/SEGMENT_DEFAULTS at the row's own offset. */

static const void *
row_defaults (wfm_surf_owner_t owner)
{
  return owner == WFM_SURF_SOURCE ? (const void *)&DEF_SRC
                                  : (const void *)&DEF_SEG;
}

static size_t
row_ranged_off (wfm_surf_owner_t owner)
{
  return owner == WFM_SURF_SOURCE ? offsetof (wfm_source_t, ranged)
                                  : offsetof (wfm_segment_t, ranged);
}

/* The value at `off` in `base`, as a double -- the one type cJSON speaks. */
static double
row_get (const wfm_surface_row_t *r, const void *base, size_t off)
{
  const char *p = (const char *)base + off;
  switch (r->kind)
    {
    case WFM_SV_DOUBLE:
      return *(const double *)p;
    case WFM_SV_INT:
    case WFM_SV_CHOICE:
      return (double)*(const int *)p;
    case WFM_SV_SIZE:
      return (double)*(const size_t *)p;
    case WFM_SV_U32:
      return (double)*(const uint32_t *)p;
    case WFM_SV_U64:
      return (double)*(const uint64_t *)p;
    case WFM_SV_SYMBOLS:
      break; /* not a JSON row: a symbols stream has its own encoding */
    }
  return 0.0;
}

static void
row_set (const wfm_surface_row_t *r, void *base, size_t off, double v)
{
  char *p = (char *)base + off;
  switch (r->kind)
    {
    case WFM_SV_DOUBLE:
      *(double *)p = v;
      break;
    case WFM_SV_INT:
    case WFM_SV_CHOICE:
      *(int *)p = (int)v;
      break;
    case WFM_SV_SIZE:
      *(size_t *)p = (size_t)v;
      break;
    case WFM_SV_U32:
      *(uint32_t *)p = (uint32_t)v;
      break;
    case WFM_SV_U64:
      *(uint64_t *)p = (uint64_t)v;
      break;
    case WFM_SV_SYMBOLS:
      break;
    }
}

/* A choice row's index, with an out-of-range value read as the default. */
static int
row_choice (const wfm_surface_row_t *r, const void *base)
{
  const int i = (int)row_get (r, base, r->off);
  return (i >= 0 && i < r->n_choices)
             ? i
             : (int)row_get (r, row_defaults (r->owner), r->off);
}

/* Write every JSON row of `owner` held in `base`, in table order. */
static void
add_rows (cJSON *o, wfm_surf_owner_t owner, const void *base)
{
  const void    *def = row_defaults (owner);
  const unsigned ranged
      = *(const unsigned *)((const char *)base + row_ranged_off (owner));
  for (size_t k = 0; k < WFM_SURFACE_N; k++)
    {
      const wfm_surface_row_t *r = &WFM_SURFACE[k];
      if (r->owner != owner || !r->json)
        continue;
      if (r->has_when
          && row_choice (&WFM_SURFACE[r->when_row], base) != r->when_value)
        continue;
      const int    is_ranged = r->range_bit && (ranged & r->range_bit);
      const double v = r->kind == WFM_SV_CHOICE ? row_choice (r, base)
                                                : row_get (r, base, r->off);
      /* Omitted at its default, AND at zero: a C caller that zero-fills a
         struct means "unset" by it, and for every omitted row but `repeats`
         the two are the same value -- for `repeats`, 0 and 1 both mean one
         instance (wfm_compose.h). A ranged field is never omitted: it
         records its SPAN. */
      if (r->json_omit && !is_ranged
          && (v == 0.0 || v == row_get (r, def, r->off)))
        continue;
      if (r->kind == WFM_SV_CHOICE)
        cJSON_AddStringToObject (o, r->json, r->choices[(int)v]);
      else if (r->json_bool)
        cJSON_AddBoolToObject (o, r->json, v != 0.0);
      else if (r->range_bit)
        add_num_or_range (o, r->json, v, row_get (r, base, r->hi_off),
                          is_ranged);
      else
        cJSON_AddNumberToObject (o, r->json, v);
    }
}

/* Read every JSON row of `owner` into `base`; an absent key, or an unknown
   choice name, is the row's default. Returns -1 only for a REQUIRED row that
   is absent or unrecognised (a source's `type`). */
static int
read_rows (const cJSON *o, wfm_surf_owner_t owner, void *base)
{
  const void *def    = row_defaults (owner);
  unsigned   *ranged = (unsigned *)((char *)base + row_ranged_off (owner));
  for (size_t k = 0; k < WFM_SURFACE_N; k++)
    {
      const wfm_surface_row_t *r = &WFM_SURFACE[k];
      if (r->owner != owner || !r->json)
        continue;
      const double dv = row_get (r, def, r->off);
      const cJSON *it = cJSON_GetObjectItemCaseSensitive (o, r->json);
      if (r->kind == WFM_SV_CHOICE)
        {
          int i = name_index (cJSON_GetStringValue (it), r->choices,
                              r->n_choices);
          if (i < 0)
            {
              if (r->json_required)
                return -1;
              i = (int)dv;
            }
          row_set (r, base, r->off, i);
        }
      else if (r->json_bool)
        row_set (r, base, r->off, cJSON_IsTrue (it) ? 1.0 : 0.0);
      else if (r->range_bit)
        {
          double       hi = 0.0;
          int          rg = 0;
          const double v  = num_or_range (o, r->json, dv, &hi, &rg);
          row_set (r, base, r->off, v);
          row_set (r, base, r->hi_off, hi);
          *ranged = rg ? (*ranged | r->range_bit) : (*ranged & ~r->range_bit);
        }
      else
        row_set (r, base, r->off, num (o, r->json, dv));
    }
  return 0;
}

/* Add a source's fields to object `so`: its surface rows, then the frame and
 * payload keys the table does not own. Both segment forms use it -- the
 * inline 1-source form after the segment's rows, and each "sum" entry. */
static void
add_source_obj (cJSON *so, const wfm_source_t *src)
{
  add_rows (so, WFM_SURF_SOURCE, src);
  add_bits_fields (so, src);
  add_stage_fields (so, src);
  add_symbols_fields (so, src);
  add_dsss_fields (so, src);
  /* Last, and only when one is carried: a source without a description
     writes exactly the bytes it wrote before this key existed. */
  add_frame_desc (so, src);
}

/* A 64-bit mask from its hex string; 0 when the key is absent, which is what
   `wfm_seq_t` reads as "derive it" for `poly` and "use 1" for a seed. */
static uint64_t
u64_hex (const cJSON *o, const char *key)
{
  const char *v
      = cJSON_GetStringValue (cJSON_GetObjectItemCaseSensitive (o, key));
  return v ? (uint64_t)strtoull (v, NULL, 0) : 0u;
}

/* Restore a GENERATED sequence from @p key. Returns 0 when the key is absent
 * or was read, -1 when it is present and malformed.
 *
 * REFUSING rather than ignoring is the point. Every other way of being wrong
 * here produces a waveform that looks fine and is not the recorded one: an
 * unknown `kind` from a newer writer, a zero length, or a record carrying
 * BOTH the literal string and the generator block. A `--from-file` that
 * quietly builds a different waveform than the one recorded defeats the
 * whole reason the record exists. */
static int
read_seq_gen (const cJSON *so, const char *lit_key, const char *gen_key,
              wfm_seq_t *q)
{
  const cJSON *g = cJSON_GetObjectItemCaseSensitive (so, gen_key);
  if (!g)
    return 0;
  if (!cJSON_IsObject (g))
    return -1;
  /* One field, one source of bits. The writer never emits both. */
  if (cJSON_GetObjectItemCaseSensitive (so, lit_key))
    return -1;

  const int k = name_index (
      cJSON_GetStringValue (cJSON_GetObjectItemCaseSensitive (g, "kind")),
      SEQ_KIND_NAMES, 4);
  /* 0 is "literal", which has no generator to restore -- as wrong here as an
     unknown name, and wrong in the same direction. */
  if (k <= 0)
    return -1;
  const double n = num (g, "len", 0);
  if (n <= 0.0)
    return -1;

  q->kind = (wfm_seq_kind_t)k;
  q->len  = (size_t)n;
  q->bits = NULL;
  if (q->kind == WFM_SEQ_PN)
    {
      q->reg_bits = (uint32_t)num (g, "reg_bits", 0);
      q->poly     = u64_hex (g, "poly");
      q->seed     = u64_hex (g, "seed");
      q->lfsr     = (int)num (g, "lfsr", 0);
      if (q->reg_bits == 0u || q->reg_bits > 64u)
        return -1;
    }
  else if (q->kind == WFM_SEQ_GOLD)
    {
      q->reg_bits = (uint32_t)num (g, "reg_bits", 0);
      q->taps_a   = u64_hex (g, "taps_a");
      q->seed_a   = u64_hex (g, "seed_a");
      q->taps_b   = u64_hex (g, "taps_b");
      q->seed_b   = u64_hex (g, "seed_b");
      if (q->reg_bits == 0u || q->reg_bits > 64u)
        return -1;
    }
  return 0;
}

/* Restore a CARRIED frame description — the inverse of add_frame_desc().
 *
 * Returns 0 when the key is absent or was read, -1 when it is present and
 * malformed. Refusing rather than salvaging is the same judgement
 * read_seq_gen() makes and for the same reason: a frame read wrong builds a
 * waveform that looks fine and is not the recorded one.
 *
 * The description, and any literal bit arrays hanging off its fields, are
 * OWNED by the source here. `wfm_source_t` borrows a frame — a caller's own
 * description outlives the source — but one parsed from JSON has no other
 * owner, exactly as the acq_code/data_code/sync arrays above have none.
 * free_src_bits() releases all of it, including on the partial-failure paths
 * below: every slot is counted into n_fields/n_stages as it is claimed, so a
 * description abandoned half-built still frees completely. */
static int
read_frame_desc (const cJSON *so, wfm_source_t *out)
{
  const cJSON *fr = cJSON_GetObjectItemCaseSensitive (so, "frame");
  if (!fr)
    return 0;
  if (!cJSON_IsObject (fr))
    return -1;

  /* dp_xcalloc, not calloc: a fixed-size internal struct is the trusted
     allocation the abort-on-OOM helper is for, and it retires an unwind
     branch no test can reach -- the same reasoning add_seq_gen() gives for
     having no OOM path of its own. */
  wfm_frame_desc_t *d = dp_xcalloc (1, sizeof *d);
  out->frame          = d; /* owned from here; free_src_bits releases it */

  const cJSON *it;
  const cJSON *fields = cJSON_GetObjectItemCaseSensitive (fr, "fields");
  if (fields)
    {
      if (!cJSON_IsArray (fields))
        return -1;
      cJSON_ArrayForEach (it, fields)
      {
        if (!cJSON_IsObject (it) || d->n_fields >= WFM_FRAME_MAX_FIELDS)
          return -1;
        wfm_field_t *f = &d->field[d->n_fields++];

        const char *nm = cJSON_GetStringValue (
            cJSON_GetObjectItemCaseSensitive (it, "name"));
        if (nm)
          {
            /* Truncating would RENAME the field, and a name is what a
               receiver slices a capture by, so a name this build cannot
               hold is a spec it cannot honour. Refuse it rather than store
               a prefix that resolves to something else. */
            if (strlen (nm) >= WFM_FRAME_NAME_MAX)
              return -1;
            snprintf (f->name, sizeof f->name, "%s", nm);
          }

        const char *lit = cJSON_GetStringValue (
            cJSON_GetObjectItemCaseSensitive (it, "lit"));
        if (lit)
          {
            size_t   n = 0;
            uint8_t *b = string_to_bits (lit, &n);
            if (!b)
              return -1;
            f->seq.kind = WFM_SEQ_LITERAL;
            f->seq.bits = b;
            f->seq.len  = n;
          }
        /* Also refuses a field carrying BOTH "lit" and "gen". */
        if (read_seq_gen (it, "lit", "gen", &f->seq) != 0)
          return -1;
        read_num_keys (it, f, FIELD_KEYS, N_FIELD_KEYS);
      }
    }

  const cJSON *stages = cJSON_GetObjectItemCaseSensitive (fr, "stages");
  if (stages)
    {
      if (!cJSON_IsArray (stages))
        return -1;
      cJSON_ArrayForEach (it, stages)
      {
        if (!cJSON_IsObject (it) || d->n_stages >= WFM_FRAME_MAX_STAGES)
          return -1;
        wfm_stage_t *s = &d->stage[d->n_stages++];
        if (read_stage_kind (it, &s->kind) != 0)
          return -1;
        read_num_keys (it, s, STAGE_KEYS, N_STAGE_KEYS);
      }
    }
  return 0;
}

/* Read the frame back: preamble, repetitions, sync word, CRC choice. The
 * inverse of add_frame_fields(), and called for every waveform type for the
 * same reason it is written for every waveform type. `crc` defaults to crc16
 * (the burst_demod frame contract carries a trailer) and is inert unless a
 * preamble or a sync word is present. Returns 0, or -1 on OOM (partials
 * released). */
static int
read_frame_fields (const cJSON *so, wfm_source_t *out)
{
  const struct
  {
    const char *key;
    /* `const uint8_t **`, matching the member: a source OWNS these bits, but
       `wfm_seq_t` declares them const for the borrowing consumer. Storing a
       freshly-allocated non-const buffer into a const slot is exactly the
       direction C allows, so the read side needs no cast -- only the frees
       do. */
    const uint8_t **arr;
    size_t         *len;
  } bitkeys[] = {
    { "acq_code", &out->acq_code.bits, &out->acq_code.len },
    { "sync", &out->sync.bits, &out->sync.len },
  };
  for (size_t i = 0; i < sizeof bitkeys / sizeof bitkeys[0]; i++)
    {
      const char *v = cJSON_GetStringValue (
          cJSON_GetObjectItemCaseSensitive (so, bitkeys[i].key));
      if (v)
        {
          *bitkeys[i].arr = string_to_bits (v, bitkeys[i].len);
          if (!*bitkeys[i].arr)
            {
              free_src_bits (out, 1); /* drop this source's partials */
              return -1;
            }
        }
    }
  if (read_seq_gen (so, "acq_code", "acq_code_gen", &out->acq_code) != 0
      || read_seq_gen (so, "sync", "sync_gen", &out->sync) != 0)
    {
      free_src_bits (out, 1);
      return -1;
    }
  out->acq_reps = (size_t)num (so, "acq_reps", (double)DEF_SRC.acq_reps);
  int c         = name_index (
      cJSON_GetStringValue (cJSON_GetObjectItemCaseSensitive (so, "crc")),
      CRC_NAMES, 2);
  out->crc = (c < 0) ? 1 : c;

  /* The coding stages. Absent means off, which is what a record written
     before these existed says -- and what an uncoded one still says. */
  out->rs_depth = (unsigned)num (so, "rs_depth", 0);
  {
    /* A string names the generator; a bare `true` is read as the default,
       so a record written before the choice existed still loads. */
    const cJSON *rnd = cJSON_GetObjectItemCaseSensitive (so, "randomise");
    const char  *rn  = cJSON_GetStringValue (rnd);
    if (rn != NULL)
      {
        const int idx  = name_index (rn, RANDOMISE_NAMES, 3);
        out->randomise = (idx < 0) ? 0 : idx;
      }
    else
      out->randomise = cJSON_IsTrue (rnd) ? 1 : 0;
  }
  out->attach_asm
      = cJSON_IsTrue (cJSON_GetObjectItemCaseSensitive (so, "asm"));
  out->convolutional
      = cJSON_IsTrue (cJSON_GetObjectItemCaseSensitive (so, "conv"));
  out->interleave_depth = (unsigned)num (so, "interleave", 0);
  /* 0 reads as 1 in the kernel, so a record from before the unit was
     written -- or one a human edited down to the depth alone -- replays as
     bit interleaving, which is what a bare `--interleave R` means. */
  out->interleave_unit_bits = (unsigned)num (so, "interleave_unit", 0);
  return 0;
}

/* Parse a source object (the inline segment, or a "sum" entry) into *out.
 * Returns 0, or -1 on a missing/unknown waveform type. */
static int
parse_source_obj (const cJSON *so, wfm_source_t *out)
{
  /* Every field outside the table starts at zero, except acq_reps, whose
     default is not "absent"; every table row is read -- or defaulted -- by
     read_rows, which refuses a missing or unknown `type`. */
  *out = (wfm_source_t){ .acq_reps = DEF_SRC.acq_reps };
  if (read_rows (so, WFM_SURF_SOURCE, out) != 0)
    return -1;
  const int t = out->type;
  if (t == WFM_SYNTH_BITS)
    {
      const cJSON *pat      = cJSON_GetObjectItemCaseSensitive (so, "pattern");
      const char  *patt_str = cJSON_GetStringValue (pat);
      if (patt_str)
        {
          out->payload.bits = string_to_bits (patt_str, &out->payload.len);
          if (!out->payload.bits)
            return -1;
        }
    }
  /* The FRAME, whatever the waveform carrying it. Read for every type, the
   * mirror of add_frame_fields() on the way out — a framed `bits` source that
   * wrote its preamble and sync must get them back, or --record → --from-file
   * quietly rebuilds a different waveform. */
  if (read_frame_fields (so, out) != 0)
    return -1;
  /* A CARRIED description, if the record has one. Read after the flat fields
   * rather than before, so it is the LAST word on what frame this is —
   * matching dp_wfm_source_describe_frame(), where a carried description beats
   * the flat fields it sits beside instead of being merged with them. */
  if (read_frame_desc (so, out) != 0)
    {
      /* A refused description is still an ALLOCATED one -- it is reachable
         from `out->frame` the moment it exists, so that the partial-failure
         paths inside read_frame_desc have an owner. Releasing it is this
         function's job on every other reader too; skipping it here leaked
         the description and its fields' bits on exactly the paths the reject
         tests exercise, which is how ASan found it. */
      free_src_bits (out, 1); /* drop this source's partials */
      return -1;
    }
  if (t == WFM_SYNTH_DSSS)
    {
      /* The spread half: the payload's own code, the payload under "payload"
       * (or the bits type's "pattern" — same field), and the continuous-mode
       * discriminator. The preamble/sync/crc came from read_frame_fields. */
      const char *dc = cJSON_GetStringValue (
          cJSON_GetObjectItemCaseSensitive (so, "data_code"));
      if (dc)
        {
          out->data_code.bits = string_to_bits (dc, &out->data_code.len);
          if (!out->data_code.bits)
            {
              free_src_bits (out, 1); /* drop this source's partials */
              return -1;
            }
        }
      if (read_seq_gen (so, "data_code", "data_code_gen", &out->data_code)
          != 0)
        {
          free_src_bits (out, 1);
          return -1;
        }
      const char *pay = cJSON_GetStringValue (
          cJSON_GetObjectItemCaseSensitive (so, "payload"));
      if (!pay)
        pay = cJSON_GetStringValue (
            cJSON_GetObjectItemCaseSensitive (so, "pattern"));
      if (pay)
        {
          out->payload.bits = string_to_bits (pay, &out->payload.len);
          if (!out->payload.bits)
            {
              free_src_bits (out, 1); /* drop this source's partials */
              return -1;
            }
        }
      /* "data": "prbs" (default) / absent = the seeded PN; "none" = code-only
         (pure code, no modulation); a payload overrides to itself. The
         table's index IS dsss_code_only, so the lookup assigns rather than
         compares -- absent or unrecognised gives -1, which falls to the
         "prbs" default the same way the old `== 0` form did. */
      const int data_src = name_index (
          cJSON_GetStringValue (cJSON_GetObjectItemCaseSensitive (so, "data")),
          DATA_SRC_NAMES, 2);
      out->dsss_code_only = (data_src > 0) ? data_src : 0;
    }
  /* The payload's generator, read once for every type. Its literal spelling
     is "pattern" on a bits source and "payload" on a dsss one -- the same
     field under two names -- so whichever is PRESENT is the one refused
     alongside the generator. A record carrying both is refused rather than
     resolved, for the reason read_seq_gen gives. */
  if (read_seq_gen (so,
                    cJSON_GetObjectItemCaseSensitive (so, "payload")
                        ? "payload"
                        : "pattern",
                    "payload_gen", &out->payload)
      != 0)
    {
      free_src_bits (out, 1);
      return -1;
    }

  if (t == WFM_SYNTH_SYMBOLS)
    {
      const cJSON *sy = cJSON_GetObjectItemCaseSensitive (so, "symbols");
      int          n2 = cJSON_IsArray (sy) ? cJSON_GetArraySize (sy) : 0;
      size_t       ns = (size_t)(n2 / 2); /* flat interleaved [re, im] pairs */
      if (ns)
        {
          float _Complex *buf = malloc (ns * sizeof *buf);
          if (!buf)
            return -1;
          for (size_t i = 0; i < ns; i++)
            {
              const cJSON *r = cJSON_GetArrayItem (sy, (int)(2 * i));
              const cJSON *m = cJSON_GetArrayItem (sy, (int)(2 * i + 1));
              buf[i] = (float)(cJSON_IsNumber (r) ? r->valuedouble : 0.0)
                       + (float)(cJSON_IsNumber (m) ? m->valuedouble : 0.0)
                             * (float _Complex)I;
            }
          out->symbols   = buf;
          out->n_symbols = ns;
        }
    }
  return 0;
}

char *
dp_wfm_spec_to_json (const wfm_segment_t *segs, size_t n_segs, int repeat,
                     int continuous, int seed_advance, double headroom)
{
  cJSON *root = cJSON_CreateObject ();
  if (!root)
    return NULL;
  cJSON_AddNumberToObject (root, "version", 1);
  cJSON_AddBoolToObject (root, "repeat", repeat != 0);
  cJSON_AddBoolToObject (root, "continuous", continuous != 0);
  /* Omitted at NONE, exactly as headroom is omitted at 0 dB: the 1-source
   * inline form's field order is frozen for byte-identity, and a key that is
   * always present would churn every recorded spec for a default. */
  if (seed_advance > WFM_SEED_ADVANCE_NONE
      && seed_advance <= WFM_SEED_ADVANCE_ALL)
    cJSON_AddStringToObject (root, "seed_advance",
                             SEED_ADVANCE_NAMES[seed_advance]);
  if (headroom != 0.0) /* omit at 0 dB so pre-headroom specs are unchanged */
    cJSON_AddNumberToObject (root, "headroom", headroom);
  cJSON *arr = cJSON_AddArrayToObject (root, "segments");
  for (size_t i = 0; i < n_segs; i++)
    {
      const wfm_segment_t *g = &segs[i];
      cJSON               *s = cJSON_CreateObject ();
      /* The segment's own rows, then its source: inline for one (the
         compact form a hand-written scene uses), as a "sum" array for more.
         Both forms are the same rows in the same order, so a key cannot be
         written by one and missed by the other. */
      add_rows (s, WFM_SURF_SEGMENT, g);
      if (g->n_sources == 1)
        add_source_obj (s, &g->sources[0]);
      else
        {
          cJSON *sum = cJSON_AddArrayToObject (s, "sum");
          for (size_t k = 0; k < g->n_sources; k++)
            {
              cJSON *so = cJSON_CreateObject ();
              add_source_obj (so, &g->sources[k]);
              cJSON_AddItemToArray (sum, so);
            }
        }
      cJSON_AddItemToArray (arr, s);
    }
  char *out = cJSON_Print (root);
  cJSON_Delete (root);
  return out;
}

char *
dp_wfm_spec_template_json (void)
{
  /* A representative, ready-to-edit spec exercising the schema surface, built
   * from in-memory structs and run through the same serialiser as --record so
   * it is valid by construction.  It parses back through --from-file
   * unchanged: the two 1-source segments are no-ops for noise resolution, and
   * the `sum` segment's first snr-bearing source (bpsk) anchors the floor
   * while the tone is placed above it — neither over-specifies (no snr+level
   * on a non-anchor), so dp_wfm_resolve_noise() accepts it. */
  static const uint8_t pattern[] = { 1, 0, 1, 1, 0, 0, 0, 1, 1, 0 };
  wfm_source_t         tone      = {
    .type      = WFM_SYNTH_TONE,
    .freq      = 1e5,
    .snr       = 20.0,
    .sps       = 8,
    .pn_length = 7,
    .seed      = 1,
  };
  wfm_source_t bits = {
    .type         = WFM_SYNTH_BITS,
    .snr          = 30.0,
    .sps          = 8,
    .pn_length    = 7,
    .seed         = 1,
    .modulation   = 2, /* qpsk */
    .payload.bits = (uint8_t *)pattern,
    .payload.len  = sizeof (pattern),
    .pulse        = 1, /* rrc */
    .rrc_beta     = 0.35,
    .rrc_span     = 8,
  };
  wfm_source_t mix[2] = {
    { .type      = WFM_SYNTH_BPSK, /* anchor: sets the noise floor */
      .snr       = 20.0,
      .sps       = 8,
      .pn_length = 7,
      .seed      = 1,
      .pulse     = 1,
      .rrc_beta  = 0.35,
      .rrc_span  = 8 },
    { .type      = WFM_SYNTH_TONE, /* placed 10 dB above the floor */
      .freq      = 2e5,
      .snr       = 10.0,
      .sps       = 8,
      .pn_length = 7,
      .seed      = 2 },
  };
  wfm_segment_t segs[3] = {
    { .sources = &tone, .n_sources = 1, .fs = 1e6, .num_samples = 10000 },
    { .sources     = &bits,
      .n_sources   = 1,
      .fs          = 1e6,
      .num_samples = 8000,
      .off_samples = 2000 }, /* a trailing gap of zeros */
    { .sources = mix, .n_sources = 2, .fs = 1e6, .num_samples = 10000 },
  };
  return dp_wfm_spec_to_json (segs, 3, 0, 0, 0, 0.0);
}

double
dp_wfm_spec_headroom (const char *json)
{
  cJSON *root = cJSON_Parse (json);
  if (!root)
    return 0.0;
  double h = num (root, "headroom", 0.0);
  cJSON_Delete (root);
  return h;
}

dp_wfm_compose_state_t *
dp_wfm_compose_from_json_why (const char *json, const char **why)
{
  if (why)
    *why = NULL;
  cJSON *root = cJSON_Parse (json);
  if (!root)
    return NULL;
  const cJSON *arr = cJSON_GetObjectItemCaseSensitive (root, "segments");
  if (!cJSON_IsArray (arr) || cJSON_GetArraySize (arr) == 0)
    {
      cJSON_Delete (root);
      return NULL;
    }
  int repeat
      = cJSON_IsTrue (cJSON_GetObjectItemCaseSensitive (root, "repeat"));
  int cont
      = cJSON_IsTrue (cJSON_GetObjectItemCaseSensitive (root, "continuous"));
  /* "seed_advance": "none"|"noise"|"all" (default none; unknown → none). */
  int seed_advance
      = name_index (cJSON_GetStringValue (cJSON_GetObjectItemCaseSensitive (
                        root, "seed_advance")),
                    SEED_ADVANCE_NAMES, 3);
  if (seed_advance < 0)
    seed_advance = WFM_SEED_ADVANCE_NONE;
  size_t         n    = (size_t)cJSON_GetArraySize (arr);
  wfm_segment_t *segs = calloc (n, sizeof (*segs));
  if (!segs)
    {
      cJSON_Delete (root);
      return NULL;
    }
  size_t       i = 0;
  const cJSON *s = NULL;
  cJSON_ArrayForEach (s, arr)
  {
    /* a segment is either inline (1-source fields) or a "sum" array, never
     * both; a bad type / empty sum / OOM rejects the whole spec. */
    const cJSON  *sum  = cJSON_GetObjectItemCaseSensitive (s, "sum");
    const cJSON  *ty   = cJSON_GetObjectItemCaseSensitive (s, "type");
    wfm_source_t *srcs = NULL;
    size_t        ns   = 0;
    if (sum && ty)
      goto reject;
    if (cJSON_IsArray (sum))
      {
        ns = (size_t)cJSON_GetArraySize (sum);
        if (ns < 1)
          goto reject;
        srcs = malloc (ns * sizeof (wfm_source_t));
        if (!srcs)
          goto reject;
        size_t       k  = 0;
        const cJSON *so = NULL;
        cJSON_ArrayForEach (so, sum)
        {
          if (parse_source_obj (so, &srcs[k]) != 0)
            {
              free_src_bits (srcs,
                             k); /* k sources parsed OK before this one */
              free (srcs);
              goto reject;
            }
          k++;
        }
      }
    else
      {
        ns   = 1;
        srcs = malloc (sizeof (wfm_source_t));
        if (!srcs)
          goto reject;
        if (parse_source_obj (s, &srcs[0]) != 0)
          {
            free (srcs);
            goto reject;
          }
      }
    segs[i] = (wfm_segment_t){ .sources = srcs, .n_sources = ns };
    /* A segment has no required row, so this cannot refuse. */
    (void)read_rows (s, WFM_SURF_SEGMENT, &segs[i]);
    i++;
    continue;
  reject:
    for (size_t j = 0; j < i; j++)
      {
        free_src_bits (segs[j].sources, segs[j].n_sources);
        free (segs[j].sources);
      }
    free (segs);
    cJSON_Delete (root);
    return NULL;
  }
  cJSON_Delete (root);

  /* Ask the ONE frame rule before handing over, purely so the reason can be
     REPORTED. dp_wfm_compose_create() asks it too and would refuse either way;
     what it cannot do is say why, because it answers with a NULL pointer.
     A spec is the interface most likely to be hand-written, so it is the one
     that most needs the sentence -- doppler#1155, where a derived field
     naming no producing stage generated a wrong record in silence. */
  const char *bad = NULL;
  for (size_t j = 0; j < n && !bad; j++)
    for (size_t k = 0; k < segs[j].n_sources && !bad; k++)
      bad = dp_wfm_source_frame_error (&segs[j].sources[k]);

  dp_wfm_compose_state_t *c = NULL;
  if (bad)
    {
      if (why)
        *why = bad;
    }
  else
    {
      c = dp_wfm_compose_create (segs, n, repeat, cont);
      dp_wfm_compose_set_seed_advance (c, seed_advance);
    }
  for (size_t j = 0; j < n; j++)
    {
      free_src_bits (segs[j].sources, segs[j].n_sources);
      free (segs[j].sources);
    }
  free (segs);
  return c;
}

dp_wfm_compose_state_t *
dp_wfm_compose_from_json (const char *json)
{
  return dp_wfm_compose_from_json_why (json, NULL);
}

dp_wfm_compose_state_t *
dp_wfm_compose_from_file (const char *path)
{
  FILE *f = fopen (path, "rb");
  if (!f)
    return NULL;
  fseek (f, 0, SEEK_END);
  long len = ftell (f);
  if (len < 0)
    {
      fclose (f);
      return NULL;
    }
  rewind (f);
  char *buf = malloc ((size_t)len + 1);
  if (!buf)
    {
      fclose (f);
      return NULL;
    }
  size_t rd = fread (buf, 1, (size_t)len, f);
  fclose (f);
  buf[rd]                   = '\0';
  dp_wfm_compose_state_t *c = dp_wfm_compose_from_json (buf);
  free (buf);
  return c;
}

/* The drawn per-instance values as JSON — see wfm/wfm_compose.h.
 *
 * Every number here comes from dp_wfm_compose_draws(), never from the source
 * struct: for a ranged field the struct still holds `lo`, and reporting that
 * is exactly the defect doppler#1086 measured at 1224 Hz and 6.0 dB out. */
char *
dp_wfm_draws_json (const wfm_segment_t *segs, size_t n_segs)
{
  const size_t n_rows = dp_wfm_compose_draws (segs, n_segs, NULL, 0);
  wfm_draw_t  *rows   = n_rows ? dp_xmalloc (n_rows * sizeof *rows) : NULL;
  if (rows)
    (void)dp_wfm_compose_draws (segs, n_segs, rows, n_rows);

  cJSON *arr = dp_xnn (cJSON_CreateArray ());
  for (size_t i = 0; rows && i < n_rows; i++)
    {
      const wfm_draw_t *d = &rows[i];
      cJSON            *o = dp_xnn (cJSON_CreateObject ());
      /* Where the row sits in the spec and in the stream. */
      cJSON_AddNumberToObject (o, "seg", (double)d->seg);
      cJSON_AddNumberToObject (o, "instance", (double)d->instance);
      cJSON_AddNumberToObject (o, "src", (double)d->src);
      cJSON_AddNumberToObject (o, "start", (double)d->start);
      cJSON_AddNumberToObject (o, "delay", (double)d->delay);
      cJSON_AddNumberToObject (o, "on", (double)d->on);
      cJSON_AddNumberToObject (o, "off", (double)d->off);
      /* What this instance actually flew. */
      cJSON_AddNumberToObject (o, "freq", d->freq);
      cJSON_AddNumberToObject (o, "f_end", d->f_end);
      cJSON_AddNumberToObject (o, "snr", d->snr);
      cJSON_AddNumberToObject (o, "level", d->level);
      cJSON_AddNumberToObject (o, "doppler", d->doppler);
      cJSON_AddNumberToObject (o, "doppler_rate", d->doppler_rate);
      cJSON_AddItemToArray (arr, o);
    }
  free (rows);
  char *out = dp_xnn (cJSON_PrintUnformatted (arr));
  cJSON_Delete (arr);
  return out;
}
