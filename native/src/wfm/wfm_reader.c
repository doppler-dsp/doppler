/*
 * wfm_reader.c — input containers for generated IQ (the dual of wfm_writer).
 *
 * Auto-detects raw / CSV / BLUE type-1000 (attached or detached) / SigMF and
 * yields unit-scale float complex samples. Container parsing and the wire→unit
 * conversion live here, in C; the Python `Reader` is a thin binding.
 */
#include "wfm/wfm_reader.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

/* per sample_type (0 cf32, 1 cf64, 2 ci32, 3 ci16, 4 ci8) — mirror wfm_writer
 */
static const size_t ELEM[5]  = { 4, 8, 4, 2, 1 }; /* bytes per component */
static const double SCALE[5] = { 0, 0, 2147483647.0, 32767.0, 127.0 };
static const char   FMTCH[5]
    = { 'F', 'D', 'L', 'I', 'B' }; /* BLUE format char */

struct wfm_reader
{
  FILE    *fp;
  int      ft;       /* wfm_filetype_t */
  int      stype;    /* 0..4 */
  int      mode;     /* wfm_mode_t: 0 complex, 1 scalar */
  int      endian;   /* 0 le, 1 be */
  double   fs, fc;   /* Hz; 0 if unknown */
  size_t   nsamples; /* total complex samples; 0 if unknown */
  uint8_t *scratch;  /* read buffer for binary containers */
  size_t   scratch_cap;
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
parse_blue_hcb (const uint8_t h[512], int *stype, int *mode, int *endian,
                double *fs, double *data_start, size_t *nsamples,
                int *detached)
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
  int32_t det;
  swab_copy (&det, h + 12, 4, be);
  swab_copy (&ds, h + 32, 8, be);
  swab_copy (&dsz, h + 40, 8, be);
  swab_copy (&xdelta, h + 264, 8, be);
  *stype      = st;
  *mode       = md;
  *endian     = be;
  *fs         = (xdelta != 0.0) ? 1.0 / xdelta : 0.0;
  *data_start = ds;
  /* data_size is BYTES; a scalar file packs one component per sample, so
     dividing by the complex size would under-count by 2x. */
  *nsamples = (size_t)(dsz / (double)(comps (md) * ELEM[st]));
  *detached = (int)det;
  return 0;
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
fill_nsamples (wfm_reader_t *r)
{
  long cur = ftell (r->fp);
  if (cur >= 0 && fseek (r->fp, 0, SEEK_END) == 0)
    {
      long end = ftell (r->fp);
      fseek (r->fp, cur, SEEK_SET);
      if (end >= cur)
        r->nsamples = (size_t)(end - cur) / (comps (r->mode) * ELEM[r->stype]);
    }
}

wfm_reader_t *
wfm_reader_open (const char *path, int hint_stype, int hint_endian)
{
  if (!path || hint_stype < 0 || hint_stype > 4)
    return NULL;
  wfm_reader_t *r = (wfm_reader_t *)calloc (1, sizeof *r);
  if (!r)
    return NULL;
  r->stype    = hint_stype;
  r->endian   = hint_endian ? 1 : 0;
  size_t plen = strlen (path);
  char   side[1024];

  /* SigMF: <base>.sigmf-data + <base>.sigmf-meta sidecar. */
  if (ends_with (path, ".sigmf-data"))
    {
      snprintf (side, sizeof side, "%.*s.sigmf-meta", (int)(plen - 11), path);
      if (parse_sigmf_meta (side, &r->stype, &r->endian, &r->fs, &r->fc) != 0)
        goto fail;
      r->ft = WFM_FT_SIGMF;
      r->fp = fopen (path, "rb");
      if (!r->fp)
        goto fail;
      fill_nsamples (r);
      return r;
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
      uint8_t h[512];
      int     ok = (fread (h, 1, 512, hf) == 512);
      fclose (hf);
      double ds;
      int    det = 0;
      if (!ok
          || parse_blue_hcb (h, &r->stype, &r->mode, &r->endian, &r->fs, &ds,
                             &r->nsamples, &det)
                 != 0)
        goto fail;
      r->ft = WFM_FT_BLUE;
      r->fp = fopen (path, "rb"); /* .det is raw from byte 0 */
      if (!r->fp)
        goto fail;
      return r;
    }

  /* CSV by extension. */
  if (ends_with (path, ".csv"))
    {
      r->ft = WFM_FT_CSV;
      r->fp = fopen (path, "r");
      if (!r->fp)
        goto fail;
      return r;
    }

  /* Otherwise peek for the BLUE magic; fall back to raw. */
  r->fp = fopen (path, "rb");
  if (!r->fp)
    goto fail;
  uint8_t h[512];
  size_t  got = fread (h, 1, 512, r->fp);
  if (got >= 4 && memcmp (h, "BLUE", 4) == 0)
    {
      double ds;
      int    det = 0;
      if (got != 512
          || parse_blue_hcb (h, &r->stype, &r->mode, &r->endian, &r->fs, &ds,
                             &r->nsamples, &det)
                 != 0)
        goto fail;
      r->ft = WFM_FT_BLUE;
      if (det != 0)
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
          return r; /* .det carries raw payload from byte 0 */
        }
      fseek (r->fp, (long)ds, SEEK_SET);
      return r;
    }
  rewind (r->fp);
  r->ft = WFM_FT_RAW;
  fill_nsamples (r);
  return r;

fail:
  if (r->fp)
    fclose (r->fp);
  free (r);
  return NULL;
}

void
wfm_reader_info (const wfm_reader_t *r, wfm_reader_info_t *info)
{
  info->file_type   = r->ft;
  info->sample_type = r->stype;
  info->mode        = r->mode;
  info->endian      = r->endian;
  info->fs          = r->fs;
  info->fc          = r->fc;
  info->num_samples = r->nsamples;
}

/* CSV: one "I,Q" per line; integer wire types are divided back by full-scale.
 */
static size_t
read_csv (wfm_reader_t *r, float _Complex *out, size_t max)
{
  double scale = (r->stype >= 2) ? SCALE[r->stype] : 1.0;
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
wfm_reader_read (wfm_reader_t *r, float _Complex *out, size_t max)
{
  if (max == 0)
    return 0;
  if (r->ft == WFM_FT_CSV)
    return read_csv (r, out, max);

  size_t elem = ELEM[r->stype], nc = comps (r->mode);
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
      float re = convert_elem (p, r->stype, r->endian);
      float im
          = (nc == 2) ? convert_elem (p + elem, r->stype, r->endian) : 0.0f;
      out[i] = re + im * (float _Complex)I;
      p += nc * elem;
    }
  return nsamp;
}

void
wfm_reader_close (wfm_reader_t *r)
{
  if (!r)
    return;
  if (r->fp)
    fclose (r->fp);
  free (r->scratch);
  free (r);
}
