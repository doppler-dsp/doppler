/*
 * wfm_json.c — JSON spec (de)serialisation for the composer (Phase B).
 *
 * One canonical, sample-exact schema shared by `--record` (write) and
 * `--from-file` (read), so a recorded run reproduces byte-for-byte. Uses the
 * vendored cJSON.
 */
#include "doppler/wfm/wfm_compose.h"

#include "doppler/dp_complex.h"
#include <stdarg.h>
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

/* Write @p f as its canonical Field text under @p key (wfm_frame.h). The ONE
   printer of the grammar is dp_wfm_field_format; this only sizes the buffer
   for it. A field with no bits is omitted. */
static void
add_field_text (cJSON *o, const char *key, const wfm_field_t *f)
{
  if (f->seq.len == 0)
    return;
  const size_t n = dp_wfm_field_format (f, NULL, 0);
  char        *t = dp_xmalloc (n + 1u);
  dp_wfm_field_format (f, t, n + 1u);
  cJSON_AddStringToObject (o, key, t);
  free (t);
}

/* Read a Field's text through the ONE reader of the grammar. Returns 0, or
   -1 with the parser's reason in @p why -- a non-string value included. */
static int
read_field_text (const cJSON *it, wfm_field_t *f, const char **why)
{
  const char *text  = cJSON_GetStringValue (it);
  uint8_t    *owned = NULL;
  if (!text)
    {
      *why = "a Field is written as text, e.g. \"0x1ACF\" or \"pn:63:6\"";
      return -1;
    }
  return dp_wfm_field_parse (text, f, &owned, why) == DP_OK ? 0 : -1;
}

static int
name_index (const char *s, const char *const *names, int n)
{
  if (s)
    for (int i = 0; i < n; i++)
      if (strcmp (s, names[i]) == 0)
        return i;
  return -1;
}

/* Free the per-source heap arrays (bits/symbols/dsss codes) of `ns` sources
 * (then the caller frees the array). */
static void
free_src_bits (wfm_source_t *srcs, size_t ns)
{
  if (srcs)
    for (size_t k = 0; k < ns; k++)
      {
        free (srcs[k].symbols);
        free ((void *)srcs[k].acq_code.bits);
        free ((void *)srcs[k].data_code.bits);
        free ((void *)srcs[k].sync.bits);
        free ((void *)srcs[k].data.bits);
        free ((void *)srcs[k].fill.bits);
        free ((void *)srcs[k].data_from_file);
        srcs[k].data_from_file = NULL;
        /* A CARRIED description, when it came from JSON. `wfm_source_t`
           borrows a frame -- a caller's own outlives the source -- but one
           this file parsed has no other owner, the same asymmetry the const
           bit arrays above already carry. Fields first: their literal bits
           hang off the description being freed. */
        dp_wfm_frame_free ((wfm_frame_desc_t *)srcs[k].frame);
        srcs[k].frame = NULL;
      }
}

/* The frame keys the surface table does not own: the CRC choice, whenever
 * the source is framed. Deliberately NOT type-gated: an unspread `bits`
 * source can be framed too, and gating it on dsss is how a framed bits
 * --record once came to omit the frame entirely. `dp_wfm_source_has_frame()`
 * is the predicate the generator uses, so what is recorded is what was
 * applied. The preamble, the sync word and the payload are table rows. */
static void
add_frame_fields (cJSON *o, const wfm_source_t *src)
{
  /* A carried description IS the frame, CRC stage and all, so the common
     frame's crc key would be a second, false statement beside it. */
  if (!dp_wfm_source_has_frame (src) || src->frame)
    return;
  cJSON_AddStringToObject (o, "crc", CRC_NAMES[src->crc ? 1 : 0]);
}

/* A dsss source's keys the table does not own: its CRC choice as a burst,
 * or -- continuous (symbol_rate > 0), which has no frame -- the data source
 * when it is the code alone. The codes and the payload are table rows. */
static void
add_dsss_fields (cJSON *o, const wfm_source_t *src)
{
  if (src->type != WFM_SYNTH_DSSS)
    {
      add_frame_fields (o, src); /* not spread, but possibly framed */
      return;
    }
  if (src->symbol_rate > 0.0)
    return;        /* code-only is the table's "code_only" row */
  if (!src->frame) /* a carried description carries its own CRC stage */
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
static cJSON *frame_desc_obj (const wfm_frame_desc_t *d);

static void
add_frame_desc (cJSON *o, const wfm_source_t *src)
{
  if (src->frame)
    cJSON_AddItemToObject (o, "frame", frame_desc_obj (src->frame));
  /* The surface-only data_from_file row is bespoke: written as its path. */
  if (src->data_from_file)
    cJSON_AddStringToObject (o, "data_from_file", src->data_from_file);
}

/* A scene's "data_from_file": a path to a file of packed octets, owned by
 * the source from here (free_src_bits). stdin is refused BY NAME: a scene
 * is replayed from its record, and stdin's bytes are gone once read, so
 * `-` belongs to `wfmgen --data-from-file -` alone (payload-data-source.md
 * section 4.8). Returns 0, or -1 with the reason in @p why. */
static int
read_data_file (const cJSON *so, wfm_source_t *out, const char *base,
                const char **why)
{
  const cJSON *it = cJSON_GetObjectItemCaseSensitive (so, "data_from_file");
  if (!it)
    return 0;
  const char *path = cJSON_GetStringValue (it);
  if (!path || !*path)
    {
      *why = "\"data_from_file\" is the path of a file of packed octets";
      return -1;
    }
  if (strcmp (path, "-") == 0)
    {
      *why = "\"data_from_file\": \"-\" is stdin, a stream a scene cannot "
             "replay from its record; give a file, or use wfmgen "
             "--data-from-file - on the command line";
      return -1;
    }
  /* A relative path is the SCENE's, resolved against the directory the
     scene was read from (when the reader knows it): a scene is moved and
     replayed as a unit with its data, not from wherever it is run. */
  const int    rel = base && *base && path[0] != '/';
  const size_t nb  = rel ? strlen (base) + 1u : 0u;
  const size_t n   = nb + strlen (path) + 1u;
  char        *p   = dp_xmalloc (n);
  if (rel)
    (void)snprintf (p, n, "%s/%s", base, path);
  else
    memcpy (p, path, n);
  out->data_from_file = p;
  return 0;
}

/* One frame object -- {"fields": [...], "stages": [...]} -- the inverse of
 * read_frame_obj(). The one writer of the form: a scene's "frame" key and
 * dp_wfm_frame_to_json() both come through here. */
static cJSON *
frame_desc_obj (const wfm_frame_desc_t *d)
{
  cJSON *fr     = cJSON_CreateObject ();
  cJSON *fields = cJSON_AddArrayToObject (fr, "fields");
  for (unsigned i = 0; i < d->n_fields; i++)
    {
      const wfm_field_t *f  = &d->field[i];
      cJSON             *fo = cJSON_CreateObject ();
      if (f->name[0])
        cJSON_AddStringToObject (fo, "name", f->name);
      /* A field with bits is its Field text -- `*REPS` included -- through
         the one printer of the grammar; a DERIVED field has no text form
         and is its length and producing stage (frame-description.md F.3). */
      if (f->derived_by)
        add_num_keys (fo, f, FIELD_KEYS, N_FIELD_KEYS);
      else
        add_field_text (fo, "spec", f);
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
  return fr;
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
      /* A bespoke row's key is written by its own code (add_frame_desc). */
      if (r->owner != owner || !r->json || r->kind == WFM_SV_BESPOKE)
        continue;
      if (r->has_when
          && row_choice (&WFM_SURFACE[r->when_row], base) != r->when_value)
        continue;
      if (r->kind == WFM_SV_FIELD)
        {
          /* Its Field text, *REPS from the row's count when it has one. */
          wfm_field_t f = { 0 };
          f.seq         = *(const wfm_seq_t *)((const char *)base + r->off);
          f.reps = r->reps_off
                       ? *(const size_t *)((const char *)base + r->reps_off)
                       : 1u;
          add_field_text (o, r->json, &f);
          continue;
        }
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
read_rows (const cJSON *o, wfm_surf_owner_t owner, void *base,
           const char **why)
{
  const void *def    = row_defaults (owner);
  unsigned   *ranged = (unsigned *)((char *)base + row_ranged_off (owner));
  /* Two keys no face takes together (the manifest's `exclusive`): refused
     before either is read, so neither is silently dropped. */
  for (size_t k = 0; k < WFM_SURFACE_N_EXCLUSIVE; k++)
    {
      const wfm_surface_exclusive_t *e = &WFM_SURFACE_EXCLUSIVE[k];
      const wfm_surface_row_t *a = &WFM_SURFACE[e->a], *b = &WFM_SURFACE[e->b];
      if (a->owner == owner && e->json_why
          && cJSON_GetObjectItemCaseSensitive (o, a->json)
          && cJSON_GetObjectItemCaseSensitive (o, b->json))
        {
          *why = e->json_why;
          return -1;
        }
    }
  for (size_t k = 0; k < WFM_SURFACE_N; k++)
    {
      const wfm_surface_row_t *r = &WFM_SURFACE[k];
      /* A bespoke row's key is read by its own code (read_frame_desc). */
      if (r->owner != owner || !r->json || r->kind == WFM_SV_BESPOKE)
        continue;
      const cJSON *it = cJSON_GetObjectItemCaseSensitive (o, r->json);
      if (r->kind == WFM_SV_FIELD)
        {
          if (!it)
            continue; /* absent: no such field (len 0) */
          wfm_field_t f;
          if (read_field_text (it, &f, why) != 0)
            return -1;
          if (f.reps > 1 && !r->reps_off)
            {
              if (f.seq.kind == WFM_SEQ_LITERAL)
                free ((void *)f.seq.bits);
              *why = WFM_SURFACE_REPS_WHY_JSON;
              return -1;
            }
          *(wfm_seq_t *)((char *)base + r->off) = f.seq;
          if (r->reps_off)
            *(size_t *)((char *)base + r->reps_off) = f.reps;
          continue;
        }
      const double dv = row_get (r, def, r->off);
      if (r->kind == WFM_SV_CHOICE)
        {
          int i = name_index (cJSON_GetStringValue (it), r->choices,
                              r->n_choices);
          if (i < 0)
            {
              if (r->json_required)
                {
                  *why = "a source needs a known \"type\"";
                  return -1;
                }
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
  add_symbols_fields (so, src);
  add_dsss_fields (so, src);
  /* Last, and only when one is carried: a source without a description
     writes exactly the bytes it wrote before this key existed. */
  add_frame_desc (so, src);
}

/* Restore a CARRIED frame description — the inverse of add_frame_desc().
 *
 * Returns 0 when the key is absent or was read, -1 when it is present and
 * malformed. Refusing rather than salvaging is the same judgement the Field
 * reader makes and for the same reason: a frame read wrong builds a
 * waveform that looks fine and is not the recorded one.
 *
 * The description, and any literal bit arrays hanging off its fields, are
 * OWNED by the source here. `wfm_source_t` borrows a frame — a caller's own
 * description outlives the source — but one parsed from JSON has no other
 * owner, exactly as the acq_code/data_code/sync arrays above have none.
 * free_src_bits() releases all of it, including on the partial-failure paths
 * below: every slot is counted into n_fields/n_stages as it is claimed, so a
 * description abandoned half-built still frees completely. */
/* The longest path a refusal names: "segments[N].sum[N].frame.fields[N]".
   A segment or field index past this many digits is truncated in the
   message, never in the check. */
#define WFM_JSON_PATH_MAX 96

/* Write a refusal's path, `segments[1].sum[0].frame`, into @p out. One
   writer for every level: a nested path is its parent's plus a step, so it
   can outgrow the buffer, and that is fine -- the path only NAMES where a
   key sits, and a truncated one still does. Going through vsnprintf says
   so once, instead of each call site tripping -Wformat-truncation; the
   format attribute keeps every argument checked against its conversion
   (-Werror=format), which is the half that matters (doppler#1741). */
#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((format (printf, 3, 4)))
#endif
static void
json_path (char *out, size_t cap, const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  (void)vsnprintf (out, cap, fmt, ap);
  va_end (ap);
}

/* Refuse a key object @p o's level does not take, naming it and where it
 * sits -- `segments[1].sum[0]: unknown key "nope"`. The level's keys are
 * the schema's own properties (WFM_JSON_KEYS, generated from it), so the
 * reader and the schema cannot disagree about what a scene may say.
 *
 * Before this, every level read the keys it knew and dropped the rest in
 * silence: a top-level "fs" -- not a key; fs is per segment -- left every
 * segment at fs = 1, and `--realtime` then paced a 7 ms scene for two
 * hours (doppler#1153). A misspelled key is the same defect, quieter.
 *
 * The reason is formatted into a thread-local buffer, valid until this
 * thread parses again: the key is the caller's text, so no static string
 * can name it. Call it AFTER a level's retired-key check, so a retired key
 * keeps the reason that names its replacement. Returns 0, or -1. */
static int
refuse_unknown_keys (const cJSON *o, wfm_json_level_t lvl, const char *where,
                     const char **why)
{
  static _Thread_local char buf[WFM_JSON_PATH_MAX + 96];
  const cJSON              *it;
  cJSON_ArrayForEach (it, o)
  {
    int known = 0;
    for (const char *const *k = WFM_JSON_KEYS[lvl]; *k && !known; k++)
      known = strcmp (*k, it->string) == 0;
    if (known)
      continue;
    /* The one misplaced key worth its own sentence: the one #1153 was. */
    if (lvl == WFM_JSON_ROOT && strcmp (it->string, "fs") == 0)
      {
        *why = "\"fs\" is per segment: set segments[].fs";
        return -1;
      }
    snprintf (buf, sizeof buf, "%s: unknown key \"%.64s\"", where, it->string);
    *why = buf;
    return -1;
  }
  return 0;
}

static int read_frame_obj (const cJSON *fr, wfm_frame_desc_t *d,
                           const char *where, const char **why);

static int
read_frame_desc (const cJSON *so, wfm_source_t *out, const char *where,
                 const char **why)
{
  const cJSON *fr = cJSON_GetObjectItemCaseSensitive (so, "frame");
  if (!fr)
    return 0;
  if (!cJSON_IsObject (fr))
    return -1;

  /* dp_xcalloc, not calloc: a fixed-size internal struct is the trusted
     allocation the abort-on-OOM helper is for, and it retires an unwind
     branch no test can reach. */
  wfm_frame_desc_t *d = dp_xcalloc (1, sizeof *d);
  out->frame          = d; /* owned from here; free_src_bits releases it */
  char path[WFM_JSON_PATH_MAX];
  json_path (path, sizeof path, "%s.frame", where);
  return read_frame_obj (fr, d, path, why);
}

/* Read one frame object -- {"fields": [...], "stages": [...]} -- into @p d,
 * which the caller owns and frees with dp_wfm_frame_free() on either
 * outcome. The one reader of the form: a scene's "frame" key and
 * `wfmgen --frame FILE` both come through here. */
static int
read_frame_obj (const cJSON *fr, wfm_frame_desc_t *d, const char *where,
                const char **why)
{
  const cJSON *it;
  char         path[WFM_JSON_PATH_MAX];
  if (refuse_unknown_keys (fr, WFM_JSON_FRAME, where, why) != 0)
    return -1;
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

        /* A field's bits are its Field text, "spec" -- `*REPS` inside it.
           The keys it replaced are refused, never read beside it. */
        if (cJSON_GetObjectItemCaseSensitive (it, "lit")
            || cJSON_GetObjectItemCaseSensitive (it, "gen")
            || cJSON_GetObjectItemCaseSensitive (it, "reps"))
          {
            *why = "a frame field's \"lit\", \"gen\" and \"reps\" are "
                   "retired: write its Field as \"spec\", e.g. "
                   "\"spec\": \"pn:31:5*4\"";
            return -1;
          }
        json_path (path, sizeof path, "%s.fields[%u]", where,
                   d->n_fields - 1u);
        if (refuse_unknown_keys (it, WFM_JSON_FIELD, path, why) != 0)
          return -1;
        const cJSON *sp = cJSON_GetObjectItemCaseSensitive (it, "spec");
        if (sp)
          {
            char name[WFM_FRAME_NAME_MAX];
            memcpy (name, f->name, sizeof name);
            if (read_field_text (sp, f, why) != 0)
              return -1;
            memcpy (f->name, name, sizeof name); /* parse clears the name */
          }
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
        json_path (path, sizeof path, "%s.stages[%u]", where,
                   d->n_stages - 1u);
        if (refuse_unknown_keys (it, WFM_JSON_STAGE, path, why) != 0)
          return -1;
        if (read_stage_kind (it, &s->kind) != 0)
          return -1;
        read_num_keys (it, s, STAGE_KEYS, N_STAGE_KEYS);
      }
    }
  return 0;
}

/* Read the frame back: the CRC choice (the preamble and sync word are table
 * rows). The
 * inverse of add_frame_fields(), and called for every waveform type for the
 * same reason it is written for every waveform type. `crc` defaults to crc16
 * (the burst_demod frame contract carries a trailer) and is inert unless a
 * preamble or a sync word is present. Returns 0, or -1 on OOM (partials
 * released). */
static int
read_frame_fields (const cJSON *so, wfm_source_t *out)
{
  int c = name_index (
      cJSON_GetStringValue (cJSON_GetObjectItemCaseSensitive (so, "crc")),
      CRC_NAMES, 2);
  out->crc = (c < 0) ? 1 : c;

  return 0;
}

/* Parse a source object (the inline segment, or a "sum" entry) into *out.
 * Returns 0, or -1 on a missing/unknown waveform type. */
static int
parse_source_obj (const cJSON *so, wfm_source_t *out, const char *base,
                  wfm_json_level_t lvl, const char *where, const char **why)
{
  /* Keys a Field replaced, refused by name with what replaced them -- never
     read as aliases (docs/design/frame-description.md F.3). */
  static const struct
  {
    const char *key, *why;
  } RETIRED[] = {
    { "payload", "\"payload\" is retired: a payload is a data source, "
                 "\"data\" (a Field) with \"data_len\" bits per frame "
                 "(doppler#1718)" },
    { "pattern", "\"pattern\" is retired: a payload is a data source, "
                 "\"data\" (a Field)" },
    { "payload_gen", "\"payload_gen\" is retired: a payload is a data "
                     "source, \"data\", e.g. \"pn:1024:10\"" },
    { "acq_code_gen", "\"acq_code_gen\" is retired: write the generated "
                      "preamble as \"acq_code\", e.g. \"pn:1023:10\"" },
    { "acq_reps", "\"acq_reps\" is retired: repeat the preamble in its "
                  "Field, e.g. \"acq_code\": \"pn:31:5*4\"" },
    { "data_code_gen", "\"data_code_gen\" is retired: write the generated "
                       "code as \"data_code\"" },
    { "sync_gen", "\"sync_gen\" is retired: write the generated sync word "
                  "as \"sync\", e.g. \"pn:63:6\"" },
    /* The coding sugar: a coded frame is a description, "frame", whose
       stages name the spans they cover (frame-description.md R). */
    { "rs_depth", "\"rs_depth\" is retired: a coded frame is a \"frame\" "
                  "description -- add an RS stage over its data group" },
    { "randomise", "\"randomise\" is retired: a coded frame is a "
                   "\"frame\" description -- add a randomise stage over its "
                   "data group" },
    { "asm",
      "\"asm\" is retired: a coded frame is a \"frame\" description "
      "-- add the marker as its first field, \"spec\": \"0x1ACFFC1D\"" },
    { "conv", "\"conv\" is retired: a coded frame is a \"frame\" "
              "description -- add a conv stage over every field" },
    { "interleave", "\"interleave\" is retired: a coded frame is a "
                    "\"frame\" description -- add an interleave stage over "
                    "its data group" },
    { "interleave_unit", "\"interleave_unit\" is retired: it is the "
                         "interleave stage's \"unit_bits\" in a \"frame\" "
                         "description" },
  };
  for (size_t k = 0; k < sizeof RETIRED / sizeof *RETIRED; k++)
    if (cJSON_GetObjectItemCaseSensitive (so, RETIRED[k].key))
      {
        *why = RETIRED[k].why;
        return -1;
      }
  /* After the retired keys, so each keeps the reason naming its
     replacement. An inline segment is a source and a segment at once, so
     its level's keys are both. */
  if (refuse_unknown_keys (so, lvl, where, why) != 0)
    return -1;

  /* Every field outside the table starts at zero, except acq_reps, whose
     default is not "absent"; every table row is read -- or defaulted -- by
     read_rows, which refuses a missing or unknown `type`. */
  *out = (wfm_source_t){ .acq_reps = DEF_SRC.acq_reps };
  if (read_rows (so, WFM_SURF_SOURCE, out, why) != 0)
    {
      free_src_bits (out, 1); /* a Field parsed before the refused one */
      return -1;
    }
  const int t = out->type;
  /* The FRAME, whatever the waveform carrying it. Read for every type, the
   * mirror of add_frame_fields() on the way out — a framed `bits` source that
   * wrote its preamble and sync must get them back, or --record → --from-file
   * quietly rebuilds a different waveform. */
  if (read_frame_fields (so, out) != 0)
    return -1;
  /* A CARRIED description, if the record has one. It is the whole frame, so
   * a sync word or an unspread preamble read beside it above is refused by
   * dp_wfm_source_frame_error() rather than merged with it or dropped. */
  if (read_frame_desc (so, out, where, why) != 0
      || read_data_file (so, out, base, why))
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
      /* A finite source -- data, or a carried frame of fixed bits -- SETS
         the segment's length (its frames), so a record omits the derived
         num_samples exactly as a scene must: the reader refuses one given
         beside it, and a replay derives it again. */
      for (size_t k = 0; k < g->n_sources; k++)
        if (dp_wfm_source_data_frames (&g->sources[k]) > 0)
          {
            cJSON_DeleteItemFromObjectCaseSensitive (s, "num_samples");
            break;
          }
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
  wfm_source_t tone = {
    .type      = WFM_SYNTH_TONE,
    .freq      = 1e5,
    .snr       = 20.0,
    .sps       = 8,
    .pn_length = 7,
    .seed      = 1,
  };
  wfm_source_t bits = {
    .type       = WFM_SYNTH_BITS,
    .snr        = 30.0,
    .sps        = 8,
    .pn_length  = 7,
    .seed       = 1,
    .modulation = 2, /* qpsk */
    /* A data source as a generated Field, sent as one frame with no check:
       2000 PN bits are 1000 qpsk symbols, the segment's 8000 samples. */
    .crc      = 0,
    .data     = { .kind = WFM_SEQ_PN, .len = 2000, .reg_bits = 11 },
    .pulse    = 1, /* rrc */
    .rrc_beta = 0.35,
    .rrc_span = 8,
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
    /* No num_samples: a finite data source sets its own run (8000). */
    { .sources     = &bits,
      .n_sources   = 1,
      .fs          = 1e6,
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
  return dp_wfm_compose_from_json_at (json, NULL, why);
}

dp_wfm_compose_state_t *
dp_wfm_compose_from_json_at (const char *json, const char *base,
                             const char **why)
{
  /* Every reader below names its refusal through `why`, so it always has
     somewhere to write -- the caller's, or this one when they pass NULL. */
  const char *unasked = NULL;
  if (!why)
    why = &unasked;
  *why        = NULL;
  cJSON *root = cJSON_Parse (json);
  if (!root)
    return NULL;
  if (refuse_unknown_keys (root, WFM_JSON_ROOT, "the scene", why) != 0)
    {
      cJSON_Delete (root);
      return NULL;
    }
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
    char          where[WFM_JSON_PATH_MAX], at[WFM_JSON_PATH_MAX];
    json_path (where, sizeof where, "segments[%zu]", i);
    if (sum && ty)
      goto reject;
    if (cJSON_IsArray (sum))
      {
        if (refuse_unknown_keys (s, WFM_JSON_SUM_SEGMENT, where, why) != 0)
          goto reject;
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
          json_path (at, sizeof at, "%s.sum[%zu]", where, k);
          if (parse_source_obj (so, &srcs[k], base, WFM_JSON_SOURCE, at, why)
              != 0)
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
        if (parse_source_obj (s, &srcs[0], base, WFM_JSON_INLINE_SEGMENT,
                              where, why)
            != 0)
          {
            free (srcs);
            goto reject;
          }
      }
    segs[i] = (wfm_segment_t){ .sources = srcs, .n_sources = ns };
    /* A segment has no required row, so this cannot refuse. */
    (void)read_rows (s, WFM_SURF_SEGMENT, &segs[i], why);
    {
      /* A data source sets the segment's length (payload-data-source.md
         4.6). A finite one is its frames, so a "num_samples" beside it is
         refused by name; with any data source an absent one is 0 -- the
         composer derives it, or runs a stream until it ends -- rather
         than the 1024 default. */
      int has = 0, finite = 0;
      for (size_t k = 0; k < ns; k++)
        {
          has |= srcs[k].data.len || srcs[k].data_from_file;
          finite |= dp_wfm_source_data_frames (&srcs[k]) > 0;
        }
      const int given
          = cJSON_GetObjectItemCaseSensitive (s, "num_samples") != NULL;
      if (finite && given)
        {
          if (why)
            *why = "\"num_samples\": a finite source -- data, or a carried "
                   "frame of fixed bits, sent once -- sets the segment's "
                   "length (its frames); drop num_samples, and give "
                   "\"repeats\" for more";
          free_src_bits (srcs, ns);
          free (srcs);
          goto reject;
        }
      if ((has || finite) && !given)
        segs[i].num_samples = 0;
    }
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
  const char *bad = dp_wfm_scene_error (segs, n, repeat, cont);

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
  return dp_wfm_compose_from_file_why (path, NULL);
}

dp_wfm_compose_state_t *
dp_wfm_compose_from_file_why (const char *path, const char **why)
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
  buf[rd] = '\0';
  /* The scene's own directory, so a relative "data_from_file" is the
     scene's, not the caller's working directory's. A path with no slash is
     in the working directory, which a NULL base already means; one in the
     root keeps its leading "/" (n is at least 1). */
  const char *slash = strrchr (path, '/');
  char       *dir   = NULL;
  if (slash)
    {
      const size_t n = (size_t)(slash - path) + (slash == path);
      dir            = dp_xmalloc (n + 1u);
      memcpy (dir, path, n);
      dir[n] = '\0';
    }
  dp_wfm_compose_state_t *c = dp_wfm_compose_from_json_at (buf, dir, why);
  free (dir);
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

void
dp_wfm_frame_free (wfm_frame_desc_t *d)
{
  if (!d)
    return;
  /* Fields first: their literal bits hang off the description being
     freed. */
  for (unsigned f = 0; f < d->n_fields; f++)
    free ((void *)d->field[f].seq.bits);
  free (d);
}

wfm_frame_desc_t *
dp_wfm_frame_from_json (const char *json, const char **why)
{
  static const char *const not_obj
      = "a frame is a JSON object: {\"fields\": [...], \"stages\": [...]}";
  const char *dummy;
  if (!why)
    why = &dummy;
  *why = NULL;
  if (!json)
    {
      *why = "no frame text to read";
      return NULL;
    }
  cJSON *root = cJSON_Parse (json);
  if (!cJSON_IsObject (root))
    {
      cJSON_Delete (root);
      *why = not_obj;
      return NULL;
    }
  wfm_frame_desc_t *d = dp_xcalloc (1, sizeof *d);
  if (read_frame_obj (root, d, "the frame", why) != 0)
    {
      if (!*why)
        *why = not_obj;
      dp_wfm_frame_free (d);
      d = NULL;
    }
  cJSON_Delete (root);
  return d;
}

char *
dp_wfm_frame_to_json (const wfm_frame_desc_t *d)
{
  if (!d)
    return NULL;
  cJSON *fr  = frame_desc_obj (d);
  char  *out = dp_xnn (cJSON_PrintUnformatted (fr));
  cJSON_Delete (fr);
  return out;
}

wfm_frame_desc_t *
dp_wfm_frame_copy (const wfm_frame_desc_t *src)
{
  if (!src)
    return NULL;
  wfm_frame_desc_t *d = dp_xmalloc (sizeof *d);
  *d                  = *src;
  /* Each literal field's bits are the caller's; the copy owns its own, so
     dp_wfm_frame_free() on it never reaches a buffer it did not allocate. */
  for (unsigned f = 0; f < d->n_fields; f++)
    {
      const wfm_seq_t *q   = &src->field[f].seq;
      d->field[f].seq.bits = NULL;
      if (q->bits && q->len)
        {
          uint8_t *b = dp_xmalloc (q->len);
          memcpy (b, q->bits, q->len);
          d->field[f].seq.bits = b;
        }
    }
  return d;
}
