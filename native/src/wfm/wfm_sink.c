/*
 * wfm_sink.c — NATS PUB sink (Phase B).
 *
 * Thin glue over doppler's dp_pub_* layer: maps the wavegen wire-type index to
 * dp_sample_type_t, converts each cf32 block to that type into a
 * grow-on-demand scratch buffer, and publishes it. POSIX-only (links the
 * vendored nats.c).
 */
#include "wfm/wfm_sink.h"

#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "stream/stream.h"

/* wavegen wire-type index: 0 cf32, 1 cf64, 2 ci32, 3 ci16, 4 ci8. */
enum
{
  WT_CF32,
  WT_CF64,
  WT_CI32,
  WT_CI16,
  WT_CI8
};

struct wfm_stream_sink
{
  dp_pub_t *pub;
  int       wtype;
  void     *scratch; /* converted-sample buffer */
  size_t    cap;     /* scratch capacity in bytes */
  float     peak;    /* running max |I|/|Q| on integer paths (pre-clip) */
  uint64_t  nclip;   /* saturated components (only when `track`) */
  uint64_t  ntot;    /* integer components processed (fraction denominator) */
  int       track;   /* count clips (opt-in); peak always on */
  float     gain;    /* output gain (headroom); 1.0 = no-op (direct cf32) */
};

static long
qz (float v, double fs_val)
{
  if (v > 1.0f)
    v = 1.0f;
  if (v < -1.0f)
    v = -1.0f;
  return (long)(v * fs_val);
}

/* Track peak + opt-in clip on the integer convert paths (called per sample).
 */
static inline void
track_sample (wfm_stream_sink_t *s, float re, float im)
{
  float ar = fabsf (re), ai = fabsf (im);
  float m = ar > ai ? ar : ai;
  if (m > s->peak)
    s->peak = m;
  s->ntot += 2;
  if (s->track)
    s->nclip += (uint64_t)(ar > 1.0f) + (uint64_t)(ai > 1.0f);
}

/* Ensure scratch holds at least `need` bytes. */
static int
grow (wfm_stream_sink_t *s, size_t need)
{
  if (s->cap >= need)
    return 0;
  void *p = realloc (s->scratch, need);
  if (!p)
    return -1;
  s->scratch = p;
  s->cap     = need;
  return 0;
}

/* Strong definition — overrides the core's weak stub (wfm_sink_stub.c) when
 * libdoppler_stream is linked, signalling that the real stream sink is
 * present.
 */
int
wfm_stream_sink_available (void)
{
  return 1;
}

wfm_stream_sink_t *
wfm_stream_sink_open (const char *endpoint, int sample_type)
{
  /* map wavegen index → dp_sample_type_t */
  dp_sample_type_t dt;
  switch (sample_type)
    {
    case WT_CF32:
      dt = CF32;
      break;
    case WT_CF64:
      dt = CF64;
      break;
    case WT_CI32:
      dt = CI32;
      break;
    case WT_CI16:
      dt = CI16;
      break;
    case WT_CI8:
      dt = CI8;
      break;
    default:
      return NULL;
    }
  wfm_stream_sink_t *s = calloc (1, sizeof (*s));
  if (!s)
    return NULL;
  s->wtype = sample_type;
  s->gain  = 1.0f;
  s->pub   = dp_pub_create (endpoint, dt);
  if (!s->pub)
    {
      free (s);
      return NULL;
    }
  return s;
}

int
wfm_stream_sink_send (wfm_stream_sink_t *sink, const float _Complex *iq,
                      size_t n, double fs, double fc)
{
  if (!sink || (n && !iq))
    return -1;
  switch (sink->wtype)
    {
    case WT_CF32:
      {
        if (sink->gain == 1.0f) /* no headroom → direct, byte-identical */
          return dp_pub_send_cf32 (sink->pub, iq, n, fs, fc);
        if (grow (sink, n * sizeof (float _Complex)))
          return -1;
        float _Complex *o = sink->scratch;
        for (size_t i = 0; i < n; i++)
          o[i] = crealf (iq[i]) * sink->gain + cimagf (iq[i]) * sink->gain * I;
        return dp_pub_send_cf32 (sink->pub, o, n, fs, fc);
      }
    case WT_CF64:
      {
        if (grow (sink, n * sizeof (double _Complex)))
          return -1;
        double _Complex *o = sink->scratch;
        for (size_t i = 0; i < n; i++)
          o[i] = (double)(crealf (iq[i]) * sink->gain)
                 + (double)(cimagf (iq[i]) * sink->gain) * I;
        return dp_pub_send_cf64 (sink->pub, o, n, fs, fc);
      }
    case WT_CI32:
      {
        if (grow (sink, n * 2 * sizeof (int32_t)))
          return -1;
        int32_t *o = sink->scratch;
        for (size_t i = 0; i < n; i++)
          {
            float re = crealf (iq[i]) * sink->gain,
                  im = cimagf (iq[i]) * sink->gain;
            track_sample (sink, re, im);
            o[2 * i]     = (int32_t)qz (re, 2147483647.0);
            o[2 * i + 1] = (int32_t)qz (im, 2147483647.0);
          }
        return dp_pub_send_ci32 (sink->pub, o, n, fs, fc);
      }
    case WT_CI16:
      {
        if (grow (sink, n * 2 * sizeof (int16_t)))
          return -1;
        int16_t *o = sink->scratch;
        for (size_t i = 0; i < n; i++)
          {
            float re = crealf (iq[i]) * sink->gain,
                  im = cimagf (iq[i]) * sink->gain;
            track_sample (sink, re, im);
            o[2 * i]     = (int16_t)qz (re, 32767.0);
            o[2 * i + 1] = (int16_t)qz (im, 32767.0);
          }
        return dp_pub_send_ci16 (sink->pub, o, n, fs, fc);
      }
    default:
      { /* WT_CI8 */
        if (grow (sink, n * 2 * sizeof (int8_t)))
          return -1;
        int8_t *o = sink->scratch;
        for (size_t i = 0; i < n; i++)
          {
            float re = crealf (iq[i]) * sink->gain,
                  im = cimagf (iq[i]) * sink->gain;
            track_sample (sink, re, im);
            o[2 * i]     = (int8_t)qz (re, 127.0);
            o[2 * i + 1] = (int8_t)qz (im, 127.0);
          }
        return dp_pub_send_ci8 (sink->pub, o, n, fs, fc);
      }
    }
}

void
wfm_stream_sink_close (wfm_stream_sink_t *sink)
{
  if (sink)
    {
      if (sink->pub)
        dp_pub_destroy (sink->pub);
      free (sink->scratch);
      free (sink);
    }
}

void
wfm_stream_sink_track_clipping (wfm_stream_sink_t *sink, int on)
{
  if (sink)
    sink->track = on ? 1 : 0;
}

void
wfm_stream_sink_set_gain (wfm_stream_sink_t *sink, double gain)
{
  if (sink)
    sink->gain = (float)gain;
}

double
wfm_stream_sink_peak (const wfm_stream_sink_t *sink)
{
  return sink ? (double)sink->peak : 0.0;
}

double
wfm_stream_sink_clip_fraction (const wfm_stream_sink_t *sink)
{
  if (!sink || sink->ntot == 0)
    return 0.0;
  return (double)sink->nclip / (double)sink->ntot;
}
