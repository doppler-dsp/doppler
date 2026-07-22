/*
 * wfm_writer.c — output containers (raw / csv / BLUE-1000) + SigMF meta.
 *
 * Host is assumed little-endian (doppler's targets); big-endian output is
 * produced by reversing each element on the way out.
 */
#include "wfm/wfm_writer.h"

#include "wfm/wfm_keywords.h"

#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

/* per sample_type (wavegen order 0 cf32,1 cf64,2 ci32,3 ci16,4 ci8) */
static const size_t ELEM[5]  = { 4, 8, 4, 2, 1 };  /* bytes per I or Q */
static const size_t BPS[5]   = { 8, 16, 8, 4, 2 }; /* bytes per sample */
static const double SCALE[5] = { 0, 0, 2147483647.0, 32767.0, 127.0 };
static const char   FMTCH[5]
    = { 'F', 'D', 'L', 'I', 'B' }; /* BLUE format char */

#include "wfm_names.h" /* TYPE_NAMES / N_TYPES / MODE_NAMES (SSOT) */

struct wfm_writer
{
  FILE    *fp;
  int      owns_fp; /* close fp in wfm_writer_close (path-opened writers) */
  int      ft;
  int      stype;
  int      be;
  size_t   written; /* complex samples emitted */
  uint8_t *buf;     /* convert scratch */
  size_t   cap;
  float    peak;  /* running max |I|/|Q| (pre-clip); always tracked */
  uint64_t nclip; /* saturated I/Q components (only when `track`) */
  int      track; /* count clips (opt-in); peak is always on */
  float    gain;  /* output gain (headroom); 1.0 = no-op */
  uint8_t *kw;    /* encoded extended-header keywords, written at close */
  size_t   kwlen, kwcap;
};

/* Update the running peak (always) and, when opted in for an integer wire
 * type, the saturated-component count. One fused max in the write loop. */
static inline void
track_sample (wfm_writer_t *w, float re, float im)
{
  float ar = fabsf (re), ai = fabsf (im);
  float m = ar > ai ? ar : ai;
  if (m > w->peak)
    w->peak = m;
  if (w->track && w->stype >= 2)
    w->nclip += (uint64_t)(ar > 1.0f) + (uint64_t)(ai > 1.0f);
}

static long
qz (float v, double scale)
{
  if (v > 1.0f)
    v = 1.0f;
  if (v < -1.0f)
    v = -1.0f;
  return (long)(v * scale);
}

/* Copy sz host (LE) bytes of *src into *p, reversed when big-endian. */
static void
put (uint8_t **p, const void *src, size_t sz, int be)
{
  const uint8_t *s = src;
  for (size_t k = 0; k < sz; k++)
    (*p)[k] = be ? s[sz - 1 - k] : s[k];
  *p += sz;
}

static void
put_at (uint8_t *h, size_t off, const void *src, size_t sz, int be)
{
  uint8_t *p = h + off;
  put (&p, src, sz, be);
}

static int
grow (wfm_writer_t *w, size_t need)
{
  if (w->cap >= need)
    return 0;
  uint8_t *q = realloc (w->buf, need);
  if (!q)
    return -1;
  w->buf = q;
  w->cap = need;
  return 0;
}

/* Write a complete 512-byte BLUE/Platinum type-1000 Header Control Block.
   data_start = 512 for an attached file (data follows the header), 0 for a
   detached file (data is a separate .det starting at byte 0). detached sets
   the HCB `detached` field. All other standard fields are written (zero by
   default). The header is laid out in head_rep byte order (LE "EEEI" / BE
   "IEEE"). */
int
wfm_blue_write_hcb (FILE *fp, int sample_type, int endian, double fs,
                    double fc, double data_start, size_t total_samples,
                    int detached)
{
  (void)fc; /* no standard centre-frequency field in a type-1000 HCB */
  if (!fp || sample_type < 0 || sample_type > 4)
    return -1;
  int     be     = endian ? 1 : 0;
  uint8_t h[512] = { 0 }; /* every standard field defaults to 0 */
  int32_t i32;
  double  f64;

  /* ── main header (bytes 0..255) ─────────────────────────────────────── */
  memcpy (h + 0, "BLUE", 4);               /* version_control */
  memcpy (h + 4, be ? "IEEE" : "EEEI", 4); /* head_rep */
  memcpy (h + 8, be ? "IEEE" : "EEEI", 4); /* data_rep */
  i32 = detached ? 1 : 0;
  put_at (h, 12, &i32, 4, be); /* detached */
  /* protected (16), pipe (20), ext_start (24), ext_size (28) = 0 */
  put_at (h, 32, &data_start, 8, be); /* data_start (bytes) */
  f64 = (double)(total_samples * BPS[sample_type]);
  put_at (h, 40, &f64, 8, be); /* data_size (bytes) */
  i32 = 1000;
  put_at (h, 48, &i32, 4, be); /* type = 1000 (1-D vector) */
  h[52] = 'C';                 /* format mode: complex */
  h[53] = FMTCH[sample_type];  /* format type: B/I/L/F/D */
  /* flagmask (54), timecode (56), inlet (64), outlets (66), outmask (68),
     pipeloc (72), pipesize (76), in_byte (80), out_byte (88),
     outbytes[8] (96), keylength (160), keywords (164) = 0 */

  /* ── type-1000 adjunct (bytes 256..511) ─────────────────────────────── */
  f64 = 0.0;
  put_at (h, 256, &f64, 8, be); /* xstart */
  f64 = (fs != 0.0) ? 1.0 / fs : 0.0;
  put_at (h, 264, &f64, 8, be); /* xdelta = 1/fs */
  i32 = 1;
  put_at (h, 272, &i32, 4, be); /* xunits = 1 (seconds/time) */
  /* subsize (276) = 0 (type-2000 only) */

  return fwrite (h, 1, 512, fp) == 512 ? 0 : -1;
}

wfm_writer_t *
wfm_writer_open (FILE *fp, wfm_filetype_t ft, int sample_type, int endian,
                 double fs, double fc, size_t total_samples)
{
  (void)fc;
  if (!fp || sample_type < 0 || sample_type > 4 || ft < 0 || ft > 3)
    return NULL;
  wfm_writer_t *w = calloc (1, sizeof (*w));
  if (!w)
    return NULL;
  w->fp    = fp;
  w->ft    = ft;
  w->stype = sample_type;
  w->be    = endian ? 1 : 0;
  w->gain  = 1.0f;
  if (ft == WFM_FT_BLUE
      && wfm_blue_write_hcb (fp, sample_type, w->be, fs, fc, 512.0,
                             total_samples, 0))
    {
      free (w);
      return NULL;
    }
  return w;
}

/* CSV: one complex sample per line. */
static size_t
write_csv (wfm_writer_t *w, const float _Complex *iq, size_t n)
{
  for (size_t i = 0; i < n; i++)
    {
      float re = crealf (iq[i]) * w->gain, im = cimagf (iq[i]) * w->gain;
      int   ok;
      track_sample (w, re, im);
      if (w->stype == 0)
        ok = fprintf (w->fp, "%0.9f,%0.9f\n", (double)re, (double)im) > 0;
      else if (w->stype == 1)
        ok = fprintf (w->fp, "%0.17g,%0.17g\n", (double)re, (double)im) > 0;
      else
        ok = fprintf (w->fp, "%ld,%ld\n", qz (re, SCALE[w->stype]),
                      qz (im, SCALE[w->stype]))
             > 0;
      if (!ok)
        return i;
    }
  return n;
}

/* raw / blue body: interleaved I/Q in the wire type + byte order. */
static size_t
write_binary (wfm_writer_t *w, const float _Complex *iq, size_t n)
{
  size_t elem = ELEM[w->stype], be = w->be;
  if (grow (w, n * 2 * elem))
    return 0;
  uint8_t *p = w->buf;
  for (size_t i = 0; i < n; i++)
    {
      float re = crealf (iq[i]) * w->gain, im = cimagf (iq[i]) * w->gain;
      track_sample (w, re, im);
      switch (w->stype)
        {
        case 0:
          { /* cf32 */
            put (&p, &re, 4, be);
            put (&p, &im, 4, be);
            break;
          }
        case 1:
          { /* cf64 */
            double dr = re, di = im;
            put (&p, &dr, 8, be);
            put (&p, &di, 8, be);
            break;
          }
        case 2:
          { /* ci32 */
            int32_t vr = (int32_t)qz (re, SCALE[2]);
            int32_t vi = (int32_t)qz (im, SCALE[2]);
            put (&p, &vr, 4, be);
            put (&p, &vi, 4, be);
            break;
          }
        case 3:
          { /* ci16 */
            int16_t vr = (int16_t)qz (re, SCALE[3]);
            int16_t vi = (int16_t)qz (im, SCALE[3]);
            put (&p, &vr, 2, be);
            put (&p, &vi, 2, be);
            break;
          }
        default:
          { /* ci8 */
            int8_t vr = (int8_t)qz (re, SCALE[4]);
            int8_t vi = (int8_t)qz (im, SCALE[4]);
            put (&p, &vr, 1, be);
            put (&p, &vi, 1, be);
            break;
          }
        }
    }
  size_t bytes = n * 2 * elem;
  return fwrite (w->buf, 1, bytes, w->fp) / (2 * elem);
}

size_t
wfm_writer_write (wfm_writer_t *w, const float _Complex *iq, size_t n)
{
  if (!w || (n && !iq))
    return 0;
  size_t done
      = (w->ft == WFM_FT_CSV) ? write_csv (w, iq, n) : write_binary (w, iq, n);
  w->written += done;
  return done;
}

int
wfm_writer_add_keyword (wfm_writer_t *w, const char *tag, char type,
                        const void *value, size_t count)
{
  if (!w || w->ft != WFM_FT_BLUE) /* only BLUE has an extended header */
    return -1;
  size_t esz = wfm_kw_elem_size (type);
  if (esz == 0 || !tag || !value || count == 0)
    return -1;
  size_t need = wfm_kw_entry_size (strlen (tag), count * esz);
  if (w->kwlen + need > w->kwcap)
    {
      size_t   ncap = w->kwcap ? w->kwcap * 2 : 256;
      uint8_t *p;
      while (ncap < w->kwlen + need)
        ncap *= 2;
      p = (uint8_t *)realloc (w->kw, ncap);
      if (!p)
        return -1;
      w->kw    = p;
      w->kwcap = ncap;
    }
  size_t got = wfm_kw_encode (w->kw + w->kwlen, w->kwcap - w->kwlen, tag, type,
                              value, count, w->be);
  if (got == 0)
    return -1;
  w->kwlen += got;
  return 0;
}

/* Append the buffered keywords as the extended header and patch ext_start /
   ext_size into the HCB. Called from close, after the data is complete: BLUE
   §3.3 explicitly allows the extended header to follow the data section, which
   is the only workable order for a stream whose length is unknown up front.
   The extended header must begin on a 512-byte boundary, so the gap is zero-
   filled -- also §3.3's suggested use for that slack. */
static int
write_ext_header (wfm_writer_t *w)
{
  if (fseek (w->fp, 0, SEEK_END) != 0)
    return -1;
  long end = ftell (w->fp);
  if (end < 0)
    return -1;
  long pad = (512 - (end % 512)) % 512;
  for (long i = 0; i < pad; i++)
    if (fputc (0, w->fp) == EOF)
      return -1;
  long start = end + pad;
  if (fwrite (w->kw, 1, w->kwlen, w->fp) != w->kwlen)
    return -1;

  int32_t  blocks = (int32_t)(start / 512);
  int32_t  size   = (int32_t)w->kwlen;
  uint8_t  b[8];
  uint8_t *p = b;
  put (&p, &blocks, 4, w->be); /* ext_start, in 512-byte blocks */
  put (&p, &size, 4, w->be);   /* ext_size, in bytes */
  if (fseek (w->fp, 24, SEEK_SET) != 0 || fwrite (b, 1, 8, w->fp) != 8)
    return -1;
  return fseek (w->fp, 0, SEEK_END);
}

int
wfm_writer_close (wfm_writer_t *w)
{
  int rc = 0;
  if (w)
    {
      /* patch BLUE data_size from the actual count when the stream seeks */
      if (w->ft == WFM_FT_BLUE && fseek (w->fp, 40, SEEK_SET) == 0)
        {
          double   data_size = (double)(w->written * BPS[w->stype]);
          uint8_t  b[8];
          uint8_t *p = b;
          put (&p, &data_size, 8, w->be);
          if (fwrite (b, 1, 8, w->fp) != 8)
            rc = -1;
          fseek (w->fp, 0, SEEK_END);
          if (w->kwlen && write_ext_header (w) != 0)
            rc = -1;
        }
      if (w->owns_fp && w->fp)
        fclose (w->fp); /* path-opened writers own their FILE */
      free (w->kw);
      free (w->buf);
      free (w);
    }
  return rc;
}

/* Path-opening + FILE-owning variant for the generated `Writer` handle (jm
 * kind="handle"): the handle wants create(path,…) -> writer*, and close must
 * release the FILE (the old CPython capsule owned it). Opens the file,
 * delegates to wfm_writer_open, and marks the FILE owned so wfm_writer_close
 * fclose's it. */
wfm_writer_t *
wfm_writer_open_path (const char *path, wfm_filetype_t ft, int sample_type,
                      int endian, double fs, double fc, size_t total_samples,
                      double headroom)
{
  FILE *fp = fopen (path, "wb");
  if (!fp)
    return NULL;
  wfm_writer_t *w
      = wfm_writer_open (fp, ft, sample_type, endian, fs, fc, total_samples);
  if (!w)
    {
      fclose (fp);
      return NULL;
    }
  w->owns_fp = 1;
  /* headroom dB → a single output gain (10^(-H/20)); 0 dB is a no-op. Folded
   * in here so headroom is a plain ctor arg, not a create_post over a non-ctor
   * param the generated create-call would mis-pass. */
  if (headroom != 0.0)
    wfm_writer_set_gain (w, pow (10.0, -headroom / 20.0));
  return w;
}

void
wfm_writer_track_clipping (wfm_writer_t *w, int on)
{
  if (w)
    w->track = on ? 1 : 0;
}

void
wfm_writer_set_gain (wfm_writer_t *w, double gain)
{
  if (w)
    w->gain = (float)gain;
}

double
wfm_writer_peak (const wfm_writer_t *w)
{
  return w ? (double)w->peak : 0.0;
}

double
wfm_writer_clip_fraction (const wfm_writer_t *w)
{
  if (!w || w->written == 0)
    return 0.0;
  return (double)w->nclip / (double)(2 * w->written);
}

/* SigMF "cf32_le"-style datatype string (ci8 has no endian suffix). */
static void
sigmf_datatype (int stype, int be, char *out, size_t cap)
{
  static const char *const base[5] = { "cf32", "cf64", "ci32", "ci16", "ci8" };
  if (stype == 4)
    snprintf (out, cap, "ci8");
  else
    snprintf (out, cap, "%s_%s", base[stype], be ? "be" : "le");
}

char *
wfm_sigmf_meta_json (int sample_type, int endian, double fs, double fc,
                     const wfm_segment_t *segs, size_t n_segs)
{
  cJSON *root = cJSON_CreateObject ();
  if (!root)
    return NULL;

  cJSON *g = cJSON_AddObjectToObject (root, "global");
  char   dt[16];
  sigmf_datatype (sample_type, endian, dt, sizeof dt);
  cJSON_AddStringToObject (g, "core:datatype", dt);
  cJSON_AddNumberToObject (g, "core:sample_rate", fs);
  cJSON_AddStringToObject (g, "core:version", "1.0.0");
  cJSON_AddStringToObject (g, "core:description", "doppler wfmgen");
  cJSON_AddStringToObject (g, "core:author", "doppler wfmgen");

  cJSON *caps = cJSON_AddArrayToObject (root, "captures");
  cJSON *cap0 = cJSON_CreateObject ();
  cJSON_AddNumberToObject (cap0, "core:sample_start", 0);
  cJSON_AddNumberToObject (cap0, "core:frequency", fc);
  cJSON_AddItemToArray (caps, cap0);

  cJSON *anns = cJSON_AddArrayToObject (root, "annotations");
  /* One annotation per SOURCE per rendered INSTANCE, at the exact drawn
   * position: wfm_compose_spans() replays the ranged draws (repeats
   * instancing, jittered delays/gaps, intrinsic dsss on-times), so the
   * sidecar's sample_start/sample_count are ground truth for the capture —
   * usable directly to score a detector. (The old walker advanced by the
   * scalar num+off once per segment: wrong for ranged scenes, blind to
   * repeats.) */
  size_t      n_spans = wfm_compose_spans (segs, n_segs, NULL, 0);
  wfm_span_t *spans   = n_spans ? malloc (n_spans * sizeof *spans) : NULL;
  if (spans)
    (void)wfm_compose_spans (segs, n_segs, spans, n_spans);
  for (size_t sp = 0; spans && sp < n_spans; sp++)
    {
      const wfm_segment_t *s     = &segs[spans[sp].seg];
      size_t               start = spans[sp].start + spans[sp].delay;
      for (size_t k = 0; k < s->n_sources; k++)
        {
          const wfm_source_t *src = &s->sources[k];
          cJSON              *a   = cJSON_CreateObject ();
          cJSON_AddNumberToObject (a, "core:sample_start", (double)start);
          cJSON_AddNumberToObject (a, "core:sample_count",
                                   (double)spans[sp].on);
          /* Occupied band: a chirp spans f_start..f_end; a modulated source
           * (pn/bpsk/qpsk) is ~fs/sps wide about its centre; tone/noise are a
           * line at the offset. */
          double bw, center;
          if (src->type == WFM_SYNTH_CHIRP)
            {
              double lo = src->freq < src->f_end ? src->freq : src->f_end;
              double hi = src->freq < src->f_end ? src->f_end : src->freq;
              cJSON_AddNumberToObject (a, "core:freq_lower_edge", fc + lo);
              cJSON_AddNumberToObject (a, "core:freq_upper_edge", fc + hi);
            }
          else
            {
              bw = (src->type >= 2 && src->sps > 0) ? s->fs / (double)src->sps
                                                    : 0.0;
              center = fc + src->freq;
              cJSON_AddNumberToObject (a, "core:freq_lower_edge",
                                       center - bw / 2.0);
              cJSON_AddNumberToObject (a, "core:freq_upper_edge",
                                       center + bw / 2.0);
            }
          if (src->type >= 0 && src->type < N_TYPES)
            cJSON_AddStringToObject (a, "core:label", TYPE_NAMES[src->type]);
          cJSON_AddNumberToObject (a, "wfmgen:snr", src->snr);
          if (src->snr_mode >= 0 && src->snr_mode < 4)
            cJSON_AddStringToObject (a, "wfmgen:snr_mode",
                                     MODE_NAMES[src->snr_mode]);
          cJSON_AddNumberToObject (a, "wfmgen:sps", src->sps);
          cJSON_AddNumberToObject (a, "wfmgen:seed", src->seed);
          cJSON_AddNumberToObject (a, "wfmgen:pn_length", src->pn_length);
          cJSON_AddNumberToObject (a, "wfmgen:pn_poly", src->pn_poly);
          /* The "dsss" core:label can't distinguish a synchronous burst
           * (integer chips/symbol, framed) from a continuous asynchronous
           * stream — only symbol_rate does. Emit it for a continuous source so
           * a scorer knows the outer symbol clock; mirror the JSON face's
           * data-source label ("none" for code-only, omitted for the
           * data-modulated default). Burst dsss and every other type omit
           * these keys (symbol_rate <= 0). */
          if (src->type == WFM_SYNTH_DSSS && src->symbol_rate > 0.0)
            {
              cJSON_AddNumberToObject (a, "wfmgen:symbol_rate",
                                       src->symbol_rate);
              if (src->dsss_code_only)
                cJSON_AddStringToObject (a, "wfmgen:data", "none");
            }
          cJSON_AddItemToArray (anns, a);
        }
    }
  free (spans);

  char *out = cJSON_PrintUnformatted (root);
  cJSON_Delete (root);
  return out;
}
