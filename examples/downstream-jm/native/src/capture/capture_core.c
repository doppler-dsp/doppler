/**
 * @file capture_core.c
 * @brief Glue over doppler's `wfm_reader` — no DSP of its own.
 *
 * Every function here forwards into `libdoppler.a`. The only state this
 * example adds beyond doppler's own handle is the pair of numbers a headerless
 * capture cannot carry (`fs`, `fc`) and a note saying where they came from.
 */
#include "capture/capture_core.h"

#include <stdlib.h>

/* doppler's public C API — from the installed/built headers, not vendored. */
#include "wfm_reader/wfm_reader_core.h"

/**
 * @brief The hand-written state: doppler's handle, plus provenance.
 *
 * `no_state = "true"` in the manifest is what hands ownership of this struct
 * to us. jm never sees the layout; it only passes the pointer around.
 */
struct capture_state
{
  wfm_reader_state_t *r;    /**< doppler's opaque reader.        */
  double              fs;   /**< resolved sample rate (Hz).      */
  double              fc;   /**< resolved centre frequency (Hz). */
  int                 meta; /**< ::capture_meta_t provenance.    */
};

/**
 * @brief Wrap an already-open doppler reader.
 *
 * Takes ownership of @p r: on failure it is destroyed, so neither constructor
 * can leak a handle on the allocation path.
 */
static capture_state_t *
capture_wrap (wfm_reader_state_t *r, double fs, double fc, int meta)
{
  capture_state_t *s;

  if (r == NULL)
    return NULL;

  s = calloc (1, sizeof *s);
  if (s == NULL)
    {
      wfm_reader_destroy (r);
      return NULL;
    }

  s->r    = r;
  s->fs   = fs;
  s->fc   = fc;
  s->meta = meta;
  return s;
}

capture_state_t *
capture_create (const char *path)
{
  wfm_reader_info_t   info;
  wfm_reader_state_t *r;
  int                 meta;

  /* cf32/little-endian are the hints doppler falls back on for a headerless
     file; for BLUE and SigMF they are ignored and the file wins. */
  r = wfm_reader_create (path, 0, 0);
  if (r == NULL)
    return NULL;

  wfm_reader_info (r, &info);

  /* A self-describing capture states its sample rate; a headerless one leaves
     it at 0, and reporting "file" for that would be the exact lie the
     RawCapture view exists to avoid. */
  meta = (info.fs > 0.0) ? CAPTURE_META_FILE : CAPTURE_META_NONE;

  return capture_wrap (r, info.fs, info.fc, meta);
}

capture_state_t *
capture_open_raw (const char *path, int sample_type, int endian, double fs,
                  double fc)
{
  wfm_reader_state_t *r;

  /* The enum indices are the manifest's string_enum order, declared to match
     doppler's own encoding (0 cf32..4 ci8; 0 le, 1 be) — so the value crosses
     straight through, with no translation table to fall out of sync. */
  r = wfm_reader_create (path, sample_type, endian);

  /* Whatever the file turned out to be, the caller supplied these, and saying
     so is the whole point of this constructor. */
  return capture_wrap (r, fs, fc, CAPTURE_META_SUPPLIED);
}

void
capture_destroy (capture_state_t *state)
{
  if (state == NULL)
    return;
  wfm_reader_destroy (state->r);
  free (state);
}

void
capture_reset (capture_state_t *state)
{
  /* Rewind to the first sample. The metadata came from the file (or from the
     caller) and is unaffected — only the read cursor moves. */
  wfm_reader_reset (state->r);
}

size_t
capture_read_max_out (capture_state_t *state)
{
  return wfm_reader_read_max_out (state->r);
}

size_t
capture_read (capture_state_t *state, size_t n, float complex *out,
              size_t max_out)
{
  return wfm_reader_read (state->r, n, out, max_out);
}

double
capture_get_fs (const capture_state_t *state)
{
  return state->fs;
}

double
capture_get_fc (const capture_state_t *state)
{
  return state->fc;
}

size_t
capture_get_num_samples (const capture_state_t *state)
{
  wfm_reader_info_t info;

  wfm_reader_info (state->r, &info);
  return info.num_samples;
}

int
capture_get_metadata_source (const capture_state_t *state)
{
  return state->meta;
}
