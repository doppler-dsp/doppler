/*
 * wfm_json.c — JSON spec (de)serialisation for the composer (Phase B).
 *
 * One canonical, sample-exact schema shared by `--record` (write) and
 * `--from-file` (read), so a recorded run reproduces byte-for-byte. Uses the
 * vendored cJSON.
 */
#include "wfm/wfm_compose.h"

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

#include "wfm_names.h" /* TYPE_NAMES / N_TYPES / MODE_NAMES (SSOT) */

static const char *const LFSR_NAMES[]   = { "galois", "fibonacci" };
static const char *const BITMOD_NAMES[] = { "none", "bpsk", "qpsk" };
static const char *const PULSE_NAMES[]  = { "rect", "rrc" };
static const char *const CRC_NAMES[]    = { "none", "crc16" };
/* Ordered to match wfm_seed_advance_t (NONE=0, NOISE=1, ALL=2). */
static const char *const SEED_ADVANCE_NAMES[] = { "none", "noise", "all" };

/* Emit a source's RRC pulse-shaping fields when shaping is on (so a default
 * rect spec stays byte-identical). */
static void
add_pulse_fields (cJSON *o, const wfm_source_t *src)
{
  if (!src->pulse)
    return;
  cJSON_AddStringToObject (o, "pulse", "rrc");
  cJSON_AddNumberToObject (o, "rrc_beta", src->rrc_beta);
  cJSON_AddNumberToObject (o, "rrc_span", src->rrc_span);
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
        free (srcs[k].bits);
        free (srcs[k].symbols);
        free (srcs[k].acq_code);
        free (srcs[k].data_code);
        free (srcs[k].sync);
      }
}

/* Emit a bits source's modulation + pattern (no-op for other types). */
static void
add_bits_fields (cJSON *o, const wfm_source_t *src)
{
  if (src->type != WFM_SYNTH_BITS)
    return;
  int bm = (src->modulation >= 0 && src->modulation < 3) ? src->modulation : 1;
  cJSON_AddStringToObject (o, "modulation", BITMOD_NAMES[bm]);
  if (src->bits && src->n_bits)
    {
      char *bs = bits_to_string (src->bits, src->n_bits);
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

/* Emit a dsss source's burst geometry: the two codes, preamble repetitions,
 * sync word, payload bits, and CRC choice (no-op for other types). */
static void
add_dsss_fields (cJSON *o, const wfm_source_t *src)
{
  if (src->type != WFM_SYNTH_DSSS)
    return;
  add_bit_string (o, "acq_code", src->acq_code, src->n_acq_code);
  cJSON_AddNumberToObject (o, "acq_reps", (double)src->acq_reps);
  add_bit_string (o, "data_code", src->data_code, src->n_data_code);
  add_bit_string (o, "sync", src->sync, src->n_sync);
  add_bit_string (o, "payload", src->bits, src->n_bits);
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

/* Add a source's fields to object `so` (no fs/num/off — those are the
 * segment's; level omitted at 0). Used for the "sum" array entries; the inline
 * 1-source form keeps its own field order for byte-identity. */
static void
add_source_obj (cJSON *so, const wfm_source_t *src)
{
  int t = (src->type >= 0 && src->type < N_TYPES) ? src->type : 0;
  int m = (src->snr_mode >= 0 && src->snr_mode < 4) ? src->snr_mode : 0;
  cJSON_AddStringToObject (so, "type", TYPE_NAMES[t]);
  add_num_or_range (so, "freq", src->freq, src->freq_hi,
                    src->ranged & WFM_RANGE_FREQ);
  if (src->type == WFM_SYNTH_CHIRP) /* chirp end frequency */
    add_num_or_range (so, "f_end", src->f_end, src->f_end_hi,
                      src->ranged & WFM_RANGE_FEND);
  add_num_or_range (so, "snr", src->snr, src->snr_hi,
                    src->ranged & WFM_RANGE_SNR);
  cJSON_AddStringToObject (so, "snr_mode", MODE_NAMES[m]);
  cJSON_AddNumberToObject (so, "seed", (double)src->seed);
  cJSON_AddNumberToObject (so, "sps", src->sps);
  cJSON_AddNumberToObject (so, "pn_length", src->pn_length);
  cJSON_AddNumberToObject (so, "pn_poly", (double)src->pn_poly);
  cJSON_AddStringToObject (so, "lfsr", LFSR_NAMES[(src->lfsr == 1) ? 1 : 0]);
  if (src->level != 0.0 || (src->ranged & WFM_RANGE_LEVEL))
    add_num_or_range (so, "level", src->level, src->level_hi,
                      src->ranged & WFM_RANGE_LEVEL);
  add_bits_fields (so, src);
  add_symbols_fields (so, src);
  add_dsss_fields (so, src);
  add_pulse_fields (so, src);
}

/* Parse a source object (the inline segment, or a "sum" entry) into *out.
 * Returns 0, or -1 on a missing/unknown waveform type. */
static int
parse_source_obj (const cJSON *so, wfm_source_t *out)
{
  const cJSON *ty = cJSON_GetObjectItemCaseSensitive (so, "type");
  int          t = name_index (cJSON_GetStringValue (ty), TYPE_NAMES, N_TYPES);
  if (t < 0)
    return -1;
  const cJSON *md = cJSON_GetObjectItemCaseSensitive (so, "snr_mode");
  int          m  = name_index (cJSON_GetStringValue (md), MODE_NAMES, 4);
  /* freq/snr/level/f_end each accept a scalar or a [lo, hi] uniform range. */
  double freq_hi = 0, snr_hi = 0, level_hi = 0, f_end_hi = 0;
  int    rf = 0, rs = 0, rl = 0, re = 0;
  double freq  = num_or_range (so, "freq", 0.0, &freq_hi, &rf);
  double snr   = num_or_range (so, "snr", 100.0, &snr_hi, &rs);
  double level = num_or_range (so, "level", 0.0, &level_hi, &rl);
  double f_end = num_or_range (so, "f_end", 0.0, &f_end_hi, &re);
  *out         = (wfm_source_t){
    .type      = t,
    .freq      = freq,
    .snr       = snr,
    .snr_mode  = (m < 0) ? 0 : m,
    .seed      = (uint32_t)num (so, "seed", 1),
    .sps       = (int)num (so, "sps", 8),
    .pn_length = (int)num (so, "pn_length", 7),
    .pn_poly   = (uint64_t)num (so, "pn_poly", 0),
    .lfsr  = (name_index (cJSON_GetStringValue (
                              cJSON_GetObjectItemCaseSensitive (so, "lfsr")),
                          LFSR_NAMES, 2)
              == 1)
                 ? 1
                 : 0,
    .level = level,
    .f_end = f_end,
    .ranged
    = (unsigned)((rf ? WFM_RANGE_FREQ : 0) | (rs ? WFM_RANGE_SNR : 0)
                 | (rl ? WFM_RANGE_LEVEL : 0) | (re ? WFM_RANGE_FEND : 0)),
    .freq_hi  = freq_hi,
    .snr_hi   = snr_hi,
    .level_hi = level_hi,
    .f_end_hi = f_end_hi,
  };
  if (t == WFM_SYNTH_BITS)
    {
      int bm = name_index (
          cJSON_GetStringValue (
              cJSON_GetObjectItemCaseSensitive (so, "modulation")),
          BITMOD_NAMES, 3);
      out->modulation       = (bm < 0) ? 1 : bm;
      const cJSON *pat      = cJSON_GetObjectItemCaseSensitive (so, "pattern");
      const char  *patt_str = cJSON_GetStringValue (pat);
      if (patt_str)
        {
          out->bits = string_to_bits (patt_str, &out->n_bits);
          if (!out->bits)
            return -1;
        }
    }
  if (t == WFM_SYNTH_DSSS)
    {
      /* Burst geometry: codes/sync as "0/1" strings, payload under "payload"
       * (or the bits type's "pattern" — same field), crc none|crc16
       * (default crc16: the burst_demod frame contract carries a trailer). */
      const struct
      {
        const char *key;
        uint8_t   **arr;
        size_t     *len;
      } bitkeys[] = {
        { "acq_code", &out->acq_code, &out->n_acq_code },
        { "data_code", &out->data_code, &out->n_data_code },
        { "sync", &out->sync, &out->n_sync },
      };
      for (size_t i = 0; i < 3; i++)
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
      const char *pay = cJSON_GetStringValue (
          cJSON_GetObjectItemCaseSensitive (so, "payload"));
      if (!pay)
        pay = cJSON_GetStringValue (
            cJSON_GetObjectItemCaseSensitive (so, "pattern"));
      if (pay)
        {
          out->bits = string_to_bits (pay, &out->n_bits);
          if (!out->bits)
            {
              free_src_bits (out, 1); /* drop this source's partials */
              return -1;
            }
        }
      out->acq_reps = (size_t)num (so, "acq_reps", 1);
      int c         = name_index (
          cJSON_GetStringValue (cJSON_GetObjectItemCaseSensitive (so, "crc")),
          CRC_NAMES, 2);
      out->crc = (c < 0) ? 1 : c;
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
  if (name_index (cJSON_GetStringValue (
                      cJSON_GetObjectItemCaseSensitive (so, "pulse")),
                  PULSE_NAMES, 2)
      == 1)
    {
      out->pulse    = 1;
      out->rrc_beta = num (so, "rrc_beta", 0.35);
      out->rrc_span = (int)num (so, "rrc_span", 8);
    }
  return 0;
}

char *
wfm_spec_to_json (const wfm_segment_t *segs, size_t n_segs, int repeat,
                  int continuous, double headroom)
{
  cJSON *root = cJSON_CreateObject ();
  if (!root)
    return NULL;
  cJSON_AddNumberToObject (root, "version", 1);
  cJSON_AddBoolToObject (root, "repeat", repeat != 0);
  cJSON_AddBoolToObject (root, "continuous", continuous != 0);
  if (headroom != 0.0) /* omit at 0 dB so pre-headroom specs are unchanged */
    cJSON_AddNumberToObject (root, "headroom", headroom);
  cJSON *arr = cJSON_AddArrayToObject (root, "segments");
  for (size_t i = 0; i < n_segs; i++)
    {
      const wfm_segment_t *g = &segs[i];
      cJSON               *s = cJSON_CreateObject ();
      if (g->n_sources == 1)
        {
          /* 1-source inline form — field order frozen for byte-identity. */
          const wfm_source_t *src = &g->sources[0];
          int t = (src->type >= 0 && src->type < N_TYPES) ? src->type : 0;
          int m
              = (src->snr_mode >= 0 && src->snr_mode < 4) ? src->snr_mode : 0;
          cJSON_AddStringToObject (s, "type", TYPE_NAMES[t]);
          cJSON_AddNumberToObject (s, "fs", g->fs);
          add_num_or_range (s, "freq", src->freq, src->freq_hi,
                            src->ranged & WFM_RANGE_FREQ);
          if (src->type == WFM_SYNTH_CHIRP) /* chirp end frequency */
            add_num_or_range (s, "f_end", src->f_end, src->f_end_hi,
                              src->ranged & WFM_RANGE_FEND);
          add_num_or_range (s, "snr", src->snr, src->snr_hi,
                            src->ranged & WFM_RANGE_SNR);
          cJSON_AddStringToObject (s, "snr_mode", MODE_NAMES[m]);
          cJSON_AddNumberToObject (s, "seed", (double)src->seed);
          cJSON_AddNumberToObject (s, "sps", src->sps);
          cJSON_AddNumberToObject (s, "pn_length", src->pn_length);
          cJSON_AddNumberToObject (s, "pn_poly", (double)src->pn_poly);
          cJSON_AddStringToObject (s, "lfsr",
                                   LFSR_NAMES[(src->lfsr == 1) ? 1 : 0]);
          add_num_or_range (s, "num_samples", (double)g->num_samples,
                            (double)g->num_samples_hi,
                            g->ranged & WFM_RANGE_NUM_SAMPLES);
          add_num_or_range (s, "off_samples", (double)g->off_samples,
                            (double)g->off_samples_hi,
                            g->ranged & WFM_RANGE_OFF_SAMPLES);
          if (g->repeats > 1) /* omit at 1 so old specs are unchanged */
            cJSON_AddNumberToObject (s, "repeats", (double)g->repeats);
          if (g->delay_samples || (g->ranged & WFM_RANGE_DELAY_SAMPLES))
            add_num_or_range (s, "delay_samples", (double)g->delay_samples,
                              (double)g->delay_samples_hi,
                              g->ranged & WFM_RANGE_DELAY_SAMPLES);
          if (g->gap_noise) /* omit at auto so old specs are unchanged */
            cJSON_AddStringToObject (s, "gap_noise", "off");
          if (src->level != 0.0 /* omit at 0 dBFS so old specs are unchanged */
              || (src->ranged & WFM_RANGE_LEVEL))
            add_num_or_range (s, "level", src->level, src->level_hi,
                              src->ranged & WFM_RANGE_LEVEL);
          add_bits_fields (s, src);
          add_symbols_fields (s, src);
          add_dsss_fields (s, src);
          add_pulse_fields (s, src);
        }
      else
        {
          /* multi-source: segment-level fs/num/off + a "sum" of sources. */
          cJSON_AddNumberToObject (s, "fs", g->fs);
          add_num_or_range (s, "num_samples", (double)g->num_samples,
                            (double)g->num_samples_hi,
                            g->ranged & WFM_RANGE_NUM_SAMPLES);
          add_num_or_range (s, "off_samples", (double)g->off_samples,
                            (double)g->off_samples_hi,
                            g->ranged & WFM_RANGE_OFF_SAMPLES);
          if (g->repeats > 1) /* omit at 1 so old specs are unchanged */
            cJSON_AddNumberToObject (s, "repeats", (double)g->repeats);
          if (g->delay_samples || (g->ranged & WFM_RANGE_DELAY_SAMPLES))
            add_num_or_range (s, "delay_samples", (double)g->delay_samples,
                              (double)g->delay_samples_hi,
                              g->ranged & WFM_RANGE_DELAY_SAMPLES);
          if (g->gap_noise) /* omit at auto so old specs are unchanged */
            cJSON_AddStringToObject (s, "gap_noise", "off");
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
wfm_spec_template_json (void)
{
  /* A representative, ready-to-edit spec exercising the schema surface, built
   * from in-memory structs and run through the same serialiser as --record so
   * it is valid by construction.  It parses back through --from-file
   * unchanged: the two 1-source segments are no-ops for noise resolution, and
   * the `sum` segment's first snr-bearing source (bpsk) anchors the floor
   * while the tone is placed above it — neither over-specifies (no snr+level
   * on a non-anchor), so wfm_resolve_noise() accepts it. */
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
    .type       = WFM_SYNTH_BITS,
    .snr        = 30.0,
    .sps        = 8,
    .pn_length  = 7,
    .seed       = 1,
    .modulation = 2, /* qpsk */
    .bits       = (uint8_t *)pattern,
    .n_bits     = sizeof (pattern),
    .pulse      = 1, /* rrc */
    .rrc_beta   = 0.35,
    .rrc_span   = 8,
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
  return wfm_spec_to_json (segs, 3, 0, 0, 0.0);
}

double
wfm_spec_headroom (const char *json)
{
  cJSON *root = cJSON_Parse (json);
  if (!root)
    return 0.0;
  double h = num (root, "headroom", 0.0);
  cJSON_Delete (root);
  return h;
}

wfm_compose_state_t *
wfm_compose_from_json (const char *json)
{
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
    double      num_hi = 0, off_hi = 0, dly_hi = 0;
    int         rn = 0, ro = 0, rd = 0;
    double      n_samp = num_or_range (s, "num_samples", 0, &num_hi, &rn);
    double      o_samp = num_or_range (s, "off_samples", 0, &off_hi, &ro);
    double      d_samp = num_or_range (s, "delay_samples", 0, &dly_hi, &rd);
    const char *gn     = cJSON_GetStringValue (
        cJSON_GetObjectItemCaseSensitive (s, "gap_noise"));
    segs[i] = (wfm_segment_t){
      .sources          = srcs,
      .n_sources        = ns,
      .fs               = num (s, "fs", 1000000.0),
      .num_samples      = (size_t)n_samp,
      .off_samples      = (size_t)o_samp,
      .ranged           = (unsigned)((rn ? WFM_RANGE_NUM_SAMPLES : 0)
                                     | (ro ? WFM_RANGE_OFF_SAMPLES : 0)
                                     | (rd ? WFM_RANGE_DELAY_SAMPLES : 0)),
      .num_samples_hi   = (size_t)num_hi,
      .off_samples_hi   = (size_t)off_hi,
      .repeats          = (size_t)num (s, "repeats", 1),
      .delay_samples    = (size_t)d_samp,
      .delay_samples_hi = (size_t)dly_hi,
      .gap_noise        = (gn && strcmp (gn, "off") == 0) ? 1 : 0,
    };
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
  wfm_compose_state_t *c = wfm_compose_create (segs, n, repeat, cont);
  wfm_compose_set_seed_advance (c, seed_advance);
  for (size_t j = 0; j < n; j++)
    {
      free_src_bits (segs[j].sources, segs[j].n_sources);
      free (segs[j].sources);
    }
  free (segs);
  return c;
}

wfm_compose_state_t *
wfm_compose_from_file (const char *path)
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
  buf[rd]                = '\0';
  wfm_compose_state_t *c = wfm_compose_from_json (buf);
  free (buf);
  return c;
}
