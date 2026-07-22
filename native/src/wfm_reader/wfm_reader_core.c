/*
 * wfm_reader.c — input containers for generated IQ (the dual of wfm_writer).
 *
 * Auto-detects raw / CSV / BLUE type-1000 (attached or detached) / SigMF and
 * yields unit-scale float complex samples. Container parsing and the wire→unit
 * conversion live here, in C; the Python `Reader` is a thin binding.
 */
#include "wfm_reader/wfm_reader_core.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "wfm/wfm_keywords.h"

/* per sample_type (0 cf32, 1 cf64, 2 ci32, 3 ci16, 4 ci8) — mirror wfm_writer
 */
static const size_t ELEM[5]  = { 4, 8, 4, 2, 1 }; /* bytes per component */
static const double SCALE[5] = { 0, 0, 2147483647.0, 32767.0, 127.0 };
static const char   FMTCH[5]
    = { 'F', 'D', 'L', 'I', 'B' }; /* BLUE format char */

struct wfm_reader_state
{
  FILE          *fp;
  int            file_type;   /* wfm_filetype_t */
  int            sample_type; /* 0..4 */
  int            mode;        /* wfm_mode_t: 0 complex, 1 scalar */
  int            endian;      /* 0 le, 1 be */
  double         fs, fc;      /* Hz; 0 if unknown */
  size_t         num_samples; /* total complex samples; 0 if unknown */
  uint8_t       *scratch;     /* read buffer for binary containers */
  size_t         scratch_cap;
  wfm_keyword_t *kw; /* decoded extended-header keywords (BLUE only) */
  size_t         nkw;
  /* BLUE declares its payload length, and anything after it (an extended
     header, X-Midas slack) is NOT samples. `bounded` says the limit is known;
     `remaining` counts down the samples still owed. Raw/CSV/SigMF run to EOF,
     which for them is the same thing. */
  int    bounded;
  size_t remaining;
  long   data_off; /* byte offset of the first sample, for reset() */
};

/* Copy sz bytes of *src into *dst, reversing on big-endian so the host (LE on
   both wheel targets) sees a native value. Inverse of wfm_writer's put(). */
static void
swab_copy (void *dst, const uint8_t *src, size_t sz, int be)
{
  uint8_t *d = dst;
  for (size_t k = 0; k < sz; k++)
    d[k] = be ? src[sz - 1 - k] : src[k];
}

static int
ends_with (const char *s, const char *suffix)
{
  size_t ls = strlen (s), lx = strlen (suffix);
  return ls >= lx && strcmp (s + ls - lx, suffix) == 0;
}

/* Swap @p path's final extension for @p ext (e.g. "foo/cap.prm" -> "foo/
   cap.det"). A path whose basename carries no dot just gains the extension.
   The dot must be in the basename, so a dotted DIRECTORY ("../cap") is not
   mistaken for an extension. */
static void
swap_ext (const char *path, const char *ext, char *out, size_t cap)
{
  const char *dot   = strrchr (path, '.');
  const char *slash = strrchr (path, '/');
  size_t      base  = (dot && (!slash || dot > slash)) ? (size_t)(dot - path)
                                                       : strlen (path);
  snprintf (out, cap, "%.*s%s", (int)base, path, ext);
}

/* Decode ONE wire component (an I, or a Q, or a scalar-mode real sample) into
   a unit-scale float. Per-component rather than per-pair because BLUE 'S'
   files carry one component per sample; complex is just two calls. */
static float
convert_elem (const uint8_t *p, int stype, int be)
{
  switch (stype)
    {
    case 0:
      {
        float a;
        swab_copy (&a, p, 4, be);
        return a;
      }
    case 1:
      {
        double a;
        swab_copy (&a, p, 8, be);
        return (float)a;
      }
    case 2:
      {
        int32_t a;
        swab_copy (&a, p, 4, be);
        return (float)(a / SCALE[2]);
      }
    case 3:
      {
        int16_t a;
        swab_copy (&a, p, 2, be);
        return (float)(a / SCALE[3]);
      }
    default:
      return (float)((int8_t)p[0] / SCALE[4]);
    }
}

/* Wire components per emitted sample: 2 for interleaved I/Q, 1 for scalar. */
static size_t
comps (int mode)
{
  return (mode == WFM_MODE_SCALAR) ? 1u : 2u;
}

/* Everything wfm_reader needs out of a 512-byte BLUE type-1000 HCB. */
typedef struct
{
  int    stype, mode, endian, detached;
  double fs;
  double data_start; /* bytes from the start of the DATA file */
  size_t nsamples;
  long   ext_off;  /* bytes from the start of the HEADER file; 0 = none */
  size_t ext_size; /* extended-header length in bytes; 0 = none */
} blue_hcb_t;

/* Parse a 512-byte BLUE type-1000 HCB. Returns 0 on success, -1 if this is not
   a BLUE header — the "BLUE" magic at byte 0 is the gate, so a file that is
   not BLUE (a stray .hdr, a raw file that happens to be .det) is rejected,
   never mis-read. Every BLUE path (attached + detached) goes through here.

   The two-character `format` field (bytes 52..53) is [mode][type]: the mode
   says how many components make a sample, the type says how each component is
   stored. Both halves are validated — an unsupported mode (V/Q/M/T/…, three or
   more components per sample) is REJECTED rather than assumed to be
   interleaved I/Q, which would silently return garbage at the wrong stride. */
static int
parse_blue_hcb (const uint8_t h[512], blue_hcb_t *o)
{
  if (memcmp (h, "BLUE", 4) != 0) /* validate the magic before trusting it */
    return -1;
  int be = (memcmp (h + 4, "IEEE", 4) == 0); /* EEEI = le, IEEE = be */

  int md; /* format mode (byte 52): components per sample */
  if (h[52] == 'C')
    md = WFM_MODE_COMPLEX;
  else if (h[52] == 'S')
    md = WFM_MODE_SCALAR;
  else
    return -1;

  char fmt = (char)h[53]; /* format type (byte 53): per-component storage */
  int  st  = -1;
  for (int i = 0; i < 5; i++)
    if (FMTCH[i] == fmt)
      st = i;
  if (st < 0)
    return -1;
  double  ds, dsz, xdelta;
  int32_t det, xstart, xsize;
  swab_copy (&det, h + 12, 4, be);
  swab_copy (&xstart, h + 24, 4, be);
  swab_copy (&xsize, h + 28, 4, be);
  swab_copy (&ds, h + 32, 8, be);
  swab_copy (&dsz, h + 40, 8, be);
  swab_copy (&xdelta, h + 264, 8, be);
  o->stype      = st;
  o->mode       = md;
  o->endian     = be;
  o->fs         = (xdelta != 0.0) ? 1.0 / xdelta : 0.0;
  o->data_start = ds;
  /* data_size is BYTES; a scalar file packs one component per sample, so
     dividing by the complex size would under-count by 2x. */
  o->nsamples = (size_t)(dsz / (double)(comps (md) * ELEM[st]));
  o->detached = (int)det;
  /* ext_start counts 512-byte BLOCKS from the start of the file; ext_size is
     in BYTES (§3.1.1.7/.8). Both live in the HEADER file, which for a detached
     capture is not the file the samples come from. */
  o->ext_off  = (xstart > 0) ? (long)xstart * 512L : 0L;
  o->ext_size = (xsize > 0) ? (size_t)xsize : 0u;
  return 0;
}

/* Decode the extended header at [ext_off, ext_off + ext_size) of @p hf into
   r->kw. Best-effort by design: a file whose keyword region is truncated or
   malformed still yields its samples (and any keywords decoded before the bad
   entry), because metadata must never cost you the capture. Unrecognised
   keyword types are stepped over, per §3.3.1. */
static void
load_keywords (wfm_reader_state_t *r, FILE *hf, long ext_off, size_t ext_size)
{
  if (ext_off <= 0 || ext_size < 8)
    return;
  long save = ftell (hf);
  if (fseek (hf, ext_off, SEEK_SET) != 0)
    return;
  uint8_t *blob = (uint8_t *)malloc (ext_size);
  if (!blob)
    return;
  size_t got = fread (blob, 1, ext_size, hf);
  if (save >= 0)
    fseek (hf, save, SEEK_SET);

  size_t off = 0, cap = 0;
  while (off + 8 <= got)
    {
      wfm_keyword_t kw;
      size_t        used = 0;
      int rc = wfm_kw_decode (blob + off, got - off, r->endian, &kw, &used);
      if (rc < 0 || used == 0)
        break; /* malformed: keep what we have, stop walking */
      off += used;
      if (rc > 0)
        continue; /* unsupported type: skipped, not fatal */
      if (r->nkw == cap)
        {
          size_t         ncap = cap ? cap * 2 : 8;
          wfm_keyword_t *p
              = (wfm_keyword_t *)realloc (r->kw, ncap * sizeof *p);
          if (!p)
            {
              free (kw.value);
              break;
            }
          r->kw = p;
          cap   = ncap;
        }
      r->kw[r->nkw++] = kw;
    }
  free (blob);
}

/* Map a SigMF datatype string ("cf32_le", "ci16_be", "ci8", …) to type/endian.
 */
static int
sigmf_datatype (const char *dt, int *stype, int *endian)
{
  static const struct
  {
    const char *prefix;
    int         s;
  } map[] = {
    { "cf32", 0 }, { "cf64", 1 }, { "ci32", 2 }, { "ci16", 3 }, { "ci8", 4 }
  };
  for (int i = 0; i < 5; i++)
    {
      size_t L = strlen (map[i].prefix);
      if (strncmp (dt, map[i].prefix, L) == 0)
        {
          *stype  = map[i].s;
          *endian = (strstr (dt + L, "be") != NULL);
          return 0;
        }
    }
  return -1;
}

/* Parse a SigMF .sigmf-meta sidecar for type/endian/fs/fc. Returns 0 on ok. */
static int
parse_sigmf_meta (const char *meta_path, int *stype, int *endian, double *fs,
                  double *fc)
{
  FILE *mf = fopen (meta_path, "rb");
  if (!mf)
    return -1;
  fseek (mf, 0, SEEK_END);
  long sz = ftell (mf);
  fseek (mf, 0, SEEK_SET);
  if (sz <= 0)
    {
      fclose (mf);
      return -1;
    }
  char *buf = (char *)malloc ((size_t)sz + 1);
  if (!buf)
    {
      fclose (mf);
      return -1;
    }
  size_t got = fread (buf, 1, (size_t)sz, mf);
  buf[got]   = '\0';
  fclose (mf);
  cJSON *root = cJSON_Parse (buf);
  free (buf);
  if (!root)
    return -1;

  int    rc     = -1;
  cJSON *global = cJSON_GetObjectItem (root, "global");
  cJSON *dt = global ? cJSON_GetObjectItem (global, "core:datatype") : NULL;
  if (dt && cJSON_IsString (dt)
      && sigmf_datatype (dt->valuestring, stype, endian) == 0)
    {
      cJSON *sr   = cJSON_GetObjectItem (global, "core:sample_rate");
      *fs         = (sr && cJSON_IsNumber (sr)) ? sr->valuedouble : 0.0;
      *fc         = 0.0;
      cJSON *caps = cJSON_GetObjectItem (root, "captures");
      if (caps && cJSON_GetArraySize (caps) > 0)
        {
          cJSON *c0 = cJSON_GetArrayItem (caps, 0);
          cJSON *fr = cJSON_GetObjectItem (c0, "core:frequency");
          if (fr && cJSON_IsNumber (fr))
            *fc = fr->valuedouble;
        }
      rc = 0;
    }
  cJSON_Delete (root);
  return rc;
}

/* Fill nsamples from the bytes remaining between the current offset and EOF.
 */
static void
fill_nsamples (wfm_reader_state_t *r)
{
  long cur = ftell (r->fp);
  if (cur >= 0 && fseek (r->fp, 0, SEEK_END) == 0)
    {
      long end = ftell (r->fp);
      fseek (r->fp, cur, SEEK_SET);
      if (end >= cur)
        r->num_samples
            = (size_t)(end - cur) / (comps (r->mode) * ELEM[r->sample_type]);
    }
}

/* Copy the parsed HCB fields the reader keeps. Split out so both BLUE entry
   points (header-first and .det-first) agree on what the header decides. */
static void
apply_hcb (wfm_reader_state_t *r, const blue_hcb_t *h)
{
  r->sample_type = h->stype;
  r->mode        = h->mode;
  r->endian      = h->endian;
  r->fs          = h->fs;
  r->num_samples = h->nsamples;
  r->bounded     = 1;
  r->remaining   = h->nsamples;
}

/* Record where the samples begin, so reset() can rewind to exactly here. The
   offset differs per container (512 into an attached BLUE, 0 for a .det, raw
   or SigMF payload), so it is captured once each path is positioned. */
static wfm_reader_state_t *
ready (wfm_reader_state_t *r)
{
  long p      = ftell (r->fp);
  r->data_off = (p > 0) ? p : 0;
  return r;
}

wfm_reader_state_t *
wfm_reader_create (const char *path, int hint_stype, int hint_endian)
{
  if (!path || hint_stype < 0 || hint_stype > 4)
    return NULL;
  wfm_reader_state_t *r = (wfm_reader_state_t *)calloc (1, sizeof *r);
  if (!r)
    return NULL;
  r->sample_type = hint_stype;
  r->endian      = hint_endian ? 1 : 0;
  size_t plen    = strlen (path);
  char   side[1024];

  /* SigMF: <base>.sigmf-data + <base>.sigmf-meta sidecar. */
  if (ends_with (path, ".sigmf-data"))
    {
      snprintf (side, sizeof side, "%.*s.sigmf-meta", (int)(plen - 11), path);
      if (parse_sigmf_meta (side, &r->sample_type, &r->endian, &r->fs, &r->fc)
          != 0)
        goto fail;
      r->file_type = WFM_FT_SIGMF;
      r->fp        = fopen (path, "rb");
      if (!r->fp)
        goto fail;
      fill_nsamples (r);
      return ready (r);
    }

  /* BLUE detached, entered from the DATA side: <base>.det + its header
     sibling. The header is conventionally <base>.tmp or <base>.prm (spec
     3.1.1.4); doppler's own writer emits <base>.hdr. Try each — the usual
     entry point is the header itself, handled by the magic-peek path below. */
  if (ends_with (path, ".det"))
    {
      static const char *const HDR_EXT[] = { ".hdr", ".prm", ".tmp" };
      FILE                    *hf        = NULL;
      for (size_t i = 0; i < sizeof HDR_EXT / sizeof *HDR_EXT && !hf; i++)
        {
          swap_ext (path, HDR_EXT[i], side, sizeof side);
          hf = fopen (side, "rb");
        }
      if (!hf)
        goto fail;
      uint8_t    h[512];
      int        ok = (fread (h, 1, 512, hf) == 512);
      blue_hcb_t hcb;
      if (!ok || parse_blue_hcb (h, &hcb) != 0)
        {
          fclose (hf);
          goto fail;
        }
      apply_hcb (r, &hcb);
      /* the extended header lives in the HEADER file, which is not the file
         the samples come from — decode it before letting go of hf. */
      load_keywords (r, hf, hcb.ext_off, hcb.ext_size);
      fclose (hf);
      r->file_type = WFM_FT_BLUE;
      r->fp        = fopen (path, "rb"); /* .det is raw from byte 0 */
      if (!r->fp)
        goto fail;
      return ready (r);
    }

  /* CSV by extension. */
  if (ends_with (path, ".csv"))
    {
      r->file_type = WFM_FT_CSV;
      r->fp        = fopen (path, "r");
      if (!r->fp)
        goto fail;
      return ready (r);
    }

  /* Otherwise peek for the BLUE magic; fall back to raw. */
  r->fp = fopen (path, "rb");
  if (!r->fp)
    goto fail;
  uint8_t h[512];
  size_t  got = fread (h, 1, 512, r->fp);
  if (got >= 4 && memcmp (h, "BLUE", 4) == 0)
    {
      blue_hcb_t hcb;
      if (got != 512 || parse_blue_hcb (h, &hcb) != 0)
        goto fail;
      apply_hcb (r, &hcb);
      r->file_type = WFM_FT_BLUE;
      /* this file IS the header (attached or detached), so its extended
         header is here regardless of where the samples end up. */
      load_keywords (r, r->fp, hcb.ext_off, hcb.ext_size);
      if (hcb.detached != 0)
        {
          /* Detached (spec 3.1.1.4): this file holds ONLY the header +
             extended keywords; the payload is a separate <base>.det. The
             header file is conventionally <base>.tmp or <base>.prm (doppler's
             own writer uses <base>.hdr) — the extension is irrelevant, the
             `detached` field is what decides. det == 1 means the collocated
             .det; 2..127 name an X-Midas auxiliary path (3.1.1.4.1) that
             cannot be resolved without an X-Midas environment, so we try the
             collocated file and fail loudly rather than misread. WITHOUT this
             branch data_start (0 for a detached capture) seeks back to byte 0
             of the HEADER file and the 512-byte HCB is returned as IQ. */
          swap_ext (path, ".det", side, sizeof side);
          fclose (r->fp);
          r->fp = fopen (side, "rb");
          if (!r->fp)
            goto fail;
          return ready (r); /* .det carries raw payload from byte 0 */
        }
      fseek (r->fp, (long)hcb.data_start, SEEK_SET);
      return ready (r);
    }
  rewind (r->fp);
  r->file_type = WFM_FT_RAW;
  fill_nsamples (r);
  return ready (r);

fail:
  if (r->fp)
    fclose (r->fp);
  free (r);
  return NULL;
}

void
wfm_reader_info (const wfm_reader_state_t *r, wfm_reader_info_t *info)
{
  info->file_type   = r->file_type;
  info->sample_type = r->sample_type;
  info->mode        = r->mode;
  info->endian      = r->endian;
  info->fs          = r->fs;
  info->fc          = r->fc;
  info->num_samples = r->num_samples;
}

/* CSV: one "I,Q" per line; integer wire types are divided back by full-scale.
 */
static size_t
read_csv (wfm_reader_state_t *r, float _Complex *out, size_t max)
{
  double scale = (r->sample_type >= 2) ? SCALE[r->sample_type] : 1.0;
  size_t i;
  for (i = 0; i < max; i++)
    {
      double a, b;
      if (fscanf (r->fp, " %lf , %lf", &a, &b) != 2)
        break;
      out[i] = (float)(a / scale) + (float)(b / scale) * (float _Complex)I;
    }
  return i;
}

size_t
wfm_reader_read (wfm_reader_state_t *r, size_t max, float _Complex *out)
{
  if (max == 0)
    return 0;
  if (r->file_type == WFM_FT_CSV)
    return read_csv (r, out, max);

  if (r->bounded)
    {
      if (r->remaining == 0)
        return 0; /* the payload is spent; whatever follows is not samples */
      if (max > r->remaining)
        max = r->remaining;
    }
  size_t elem = ELEM[r->sample_type], nc = comps (r->mode);
  size_t need = max * nc * elem;
  if (r->scratch_cap < need)
    {
      uint8_t *p = (uint8_t *)realloc (r->scratch, need);
      if (!p)
        return 0;
      r->scratch     = p;
      r->scratch_cap = need;
    }
  size_t         got   = fread (r->scratch, 1, need, r->fp);
  size_t         nsamp = got / (nc * elem);
  const uint8_t *p     = r->scratch;
  for (size_t i = 0; i < nsamp; i++)
    {
      /* scalar mode has no Q component on the wire -- it reads as exactly 0,
         so an 'S' capture surfaces as a real signal on the imaginary axis. */
      float re = convert_elem (p, r->sample_type, r->endian);
      float im = (nc == 2) ? convert_elem (p + elem, r->sample_type, r->endian)
                           : 0.0f;
      out[i]   = re + im * (float _Complex)I;
      p += nc * elem;
    }
  if (r->bounded)
    r->remaining -= nsamp;
  return nsamp;
}

size_t
wfm_reader_read_max_out (wfm_reader_state_t *r)
{
  /* 0 = "no fixed upper bound": jm's variable_output machinery then sizes its
     buffer from whatever the caller asks for and grows on demand. A reader
     streams, so declaring nsamples here would allocate the whole capture the
     moment the object is constructed. */
  (void)r;
  return 0;
}

void
wfm_reader_reset (wfm_reader_state_t *r)
{
  if (!r || !r->fp)
    return;
  fseek (r->fp, r->data_off, SEEK_SET);
  r->remaining = r->num_samples; /* only consulted when `bounded` */
}

/* Property accessors for the generated binding (the "computed" property kind).
   Keeping these instead of exposing the struct is what lets the layout above
   stay private -- jm only needs a pointer to an incomplete type. */
int
wfm_reader_get_file_type (const wfm_reader_state_t *r)
{
  return r->file_type;
}

int
wfm_reader_get_sample_type (const wfm_reader_state_t *r)
{
  return r->sample_type;
}

int
wfm_reader_get_mode (const wfm_reader_state_t *r)
{
  return r->mode;
}

int
wfm_reader_get_endian (const wfm_reader_state_t *r)
{
  return r->endian;
}

double
wfm_reader_get_fs (const wfm_reader_state_t *r)
{
  return r->fs;
}

double
wfm_reader_get_fc (const wfm_reader_state_t *r)
{
  return r->fc;
}

size_t
wfm_reader_get_num_samples (const wfm_reader_state_t *r)
{
  return r->num_samples;
}

size_t
wfm_reader_num_keywords (const wfm_reader_state_t *r)
{
  return r->nkw;
}

const wfm_keyword_t *
wfm_reader_keyword (const wfm_reader_state_t *r, size_t i)
{
  return (i < r->nkw) ? &r->kw[i] : NULL;
}

const wfm_keyword_t *
wfm_reader_find_keyword (const wfm_reader_state_t *r, const char *tag)
{
  for (size_t i = 0; i < r->nkw; i++)
    if (strcmp (r->kw[i].tag, tag) == 0)
      return &r->kw[i];
  return NULL;
}

void
wfm_reader_destroy (wfm_reader_state_t *r)
{
  if (!r)
    return;
  if (r->fp)
    fclose (r->fp);
  for (size_t i = 0; i < r->nkw; i++)
    free (r->kw[i].value);
  free (r->kw);
  free (r->scratch);
  free (r);
}
