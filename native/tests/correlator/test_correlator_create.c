/* dp_correlator_create: the one constructor the Python class calls
   (docs/design/correlator-python.md sections 3 and 17).

   Two claims, each pinned here:

   - each row of the header's table builds that flavor, and builds it as
     the flavor's own constructor would: the state blobs are byte for byte
     the same, and so are the outputs for the same input;
   - a value the flavor would not honor is refused, NULL with a sentence
     naming the knob, never ignored.

   Not pinned, by design: the continuous search's NRZ polarity as a whole.
   Flipping every chip negates the reference, and every output a search
   gives is invariant to that sign; one chip mapped wrong is caught by the
   outputs. */

#include "doppler/correlator/correlator_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Every knob, at the default the header names. */
typedef struct
{
  const float _Complex   *ref;
  size_t                  ny, nx;
  dp_correlator_framing_t framing;
  size_t                  doppler_rows;
  int                     detect;
  size_t                  dwell, ny_out, nx_out;
  int                     col_out;
  float                   threshold;
  size_t                  noise_lo, noise_hi;
  int                     noise_mode;
  double                  sample_rate, chip_rate, symbol_rate;
  size_t                  reps;
  double                  cn0_dbhz, doppler_uncertainty, pfa, pd;
  double                  doppler_rate;
  size_t                  rate_hypotheses, code_only_epochs;
  int                     nthreads;
} knobs_t;

static knobs_t
defaults (const float _Complex *ref, size_t ny, size_t nx)
{
  knobs_t k          = { 0 };
  k.ref              = ref;
  k.ny               = ny;
  k.nx               = nx;
  k.framing          = DP_CORRELATOR_FRAMING_NONE;
  k.dwell            = 1;
  k.col_out          = -1;
  k.noise_hi         = SIZE_MAX;
  k.noise_mode       = DET_NOISE_MEAN;
  k.reps             = 1;
  k.cn0_dbhz         = DP_CORRELATOR_CN0_NONE;
  k.pfa              = 1e-3;
  k.pd               = 0.9;
  k.rate_hypotheses  = 1;
  k.code_only_epochs = 1;
  k.nthreads         = 1;
  return k;
}

static dp_correlator_state_t *
make (const knobs_t *k, const char **why)
{
  return dp_correlator_create (
      k->ref, k->ny, k->nx, k->framing, k->doppler_rows, k->detect, k->dwell,
      k->ny_out, k->nx_out, k->col_out, k->threshold, k->noise_lo, k->noise_hi,
      k->noise_mode, k->sample_rate, k->chip_rate, k->symbol_rate, k->reps,
      k->cn0_dbhz, k->doppler_uncertainty, k->pfa, k->pd, k->doppler_rate,
      k->rate_hypotheses, k->code_only_epochs, k->nthreads, why);
}

/* Complex Gaussian noise of amplitude `amp`, the same for a given seed. */
static void
fill_noise (float _Complex *x, size_t n, uint32_t seed, float amp)
{
  for (size_t i = 0; i < n; i++)
    x[i] = amp * dp_cgauss (&seed);
}

/* Hits the push comparisons saw, summed: a row whose detector never
   fires compares two empty lists, which says nothing about its ref. */
static size_t hits_seen;

/* Both built, the same flavor, the same bytes, and the same outputs for
   the same input: the dispatcher built what the flavor's own constructor
   builds.  A map is driven by `frames` frames of `frame` samples through
   execute(); a detector or a search by the `stream` samples of x through
   push(), in chunks.  The blob holds the running state and the sizing,
   not the reference, so the outputs are what see the reference.
   Consumes both objects. */
static int
same_object (dp_correlator_state_t *a, dp_correlator_state_t *b,
             dp_correlator_flavor_t flavor, const float _Complex *x,
             size_t frame, size_t frames, size_t stream)
{
  int ok = a && b && dp_correlator_flavor (a) == flavor
           && dp_correlator_flavor (b) == flavor;
  if (ok)
    {
      const size_t na = dp_correlator_state_bytes (a);
      ok              = na == dp_correlator_state_bytes (b);
      if (ok)
        {
          unsigned char *ba = malloc (na), *bb = malloc (na);
          dp_correlator_get_state (a, ba);
          dp_correlator_get_state (b, bb);
          ok = memcmp (ba, bb, na) == 0;
          free (ba);
          free (bb);
        }
    }
  if (ok && frame > 0)
    {
      const size_t    cap = dp_correlator_execute_max_out (a);
      float _Complex *oa  = calloc (cap, sizeof *oa);
      float _Complex *ob  = calloc (cap, sizeof *ob);
      for (size_t f = 0; ok && f < frames; f++)
        {
          const size_t ka
              = dp_correlator_execute (a, x + f * frame, frame, oa, cap);
          const size_t kb
              = dp_correlator_execute (b, x + f * frame, frame, ob, cap);
          ok = ka == kb && memcmp (oa, ob, ka * sizeof *oa) == 0;
        }
      free (oa);
      free (ob);
    }
  if (ok && frame == 0)
    {
      enum
      {
        CHUNK = 997,
        CAP   = 4096
      };
      static dp_correlator_hit_t ha[CAP], hb[CAP];
      for (size_t off = 0; ok && off < stream; off += CHUNK)
        {
          const size_t n = stream - off < CHUNK ? stream - off : CHUNK;
          memset (ha, 0, sizeof ha);
          memset (hb, 0, sizeof hb);
          const size_t ka = dp_correlator_push (a, x + off, n, ha, CAP);
          const size_t kb = dp_correlator_push (b, x + off, n, hb, CAP);
          ok              = ka == kb && memcmp (ha, hb, ka * sizeof *ha) == 0;
          hits_seen += ka;
        }
    }
  dp_correlator_destroy (a);
  dp_correlator_destroy (b);
  return ok;
}

/* A 31-chip m-sequence as NRZ samples (0 -> +1, 1 -> -1), and its chips. */
static uint8_t CHIPS[31];
static float _Complex NRZ[31];

static void
make_code (void)
{
  uint8_t reg = 0x1f;
  for (size_t i = 0; i < 31; i++)
    {
      CHIPS[i]         = reg & 1u;
      NRZ[i]           = CHIPS[i] ? -1.0f : 1.0f;
      const uint8_t fb = (uint8_t)(((reg >> 0) ^ (reg >> 2)) & 1u);
      reg              = (uint8_t)((reg >> 1) | (fb << 4));
    }
}

/* The code repeated at `spc` samples a chip from sample `start`, over
   noise: a stream every detector and search here detects in. */
static float _Complex *
code_stream (size_t n, size_t spc, size_t start, float amp)
{
  float _Complex *x = malloc (n * sizeof *x);
  fill_noise (x, n, 7u, 0.2f);
  for (size_t i = start; i < n; i++)
    x[i] += amp * NRZ[((i - start) / spc) % 31];
  return x;
}

static int
test_each_row_builds_its_flavor (void)
{
  float _Complex ref2[4 * 8] = { 0 };
  for (size_t i = 0; i < 4 * 8; i++)
    ref2[i] = (float)((i * 7u) % 5u) - 2.0f + 0.5f * I;
  enum
  {
    NX = 4096
  };
  static float _Complex x[NX];
  fill_noise (x, NX, 1u, 1.0f);
  const char *why = "unset";

  /* 1-D map, padded through nx_out: the 1-D flavor is create_2d at one
     row, its n_out the one padding knob (section 17). */
  knobs_t k = defaults (NRZ, 1, 31);
  k.dwell   = 2;
  k.nx_out  = 64;
  DP_CHECK (same_object (make (&k, &why),
                         dp_correlator_create_1d (NRZ, 31, 2, 1, 64),
                         DP_CORRELATOR_1D, x, 31, 6, 0));
  DP_CHECK (why == NULL); /* a success clears it */

  /* A (1, n) reference is the same 1-D flavor. */
  k = defaults (ref2, 1, 8);
  DP_CHECK (same_object (make (&k, NULL),
                         dp_correlator_create_1d (ref2, 8, 1, 1, 0),
                         DP_CORRELATOR_1D, x, 8, 4, 0));

  /* 2-D map, every 2-D knob away from its default. */
  k        = defaults (ref2, 4, 8);
  k.dwell  = 3;
  k.ny_out = 8;
  k.nx_out = 16;
  DP_CHECK (same_object (make (&k, NULL),
                         dp_correlator_create_2d (ref2, 4, 8, 3, 1, 8, 16, -1),
                         DP_CORRELATOR_2D, x, 32, 6, 0));

  /* 2-D map, a known column (a single-row reference: section 17's
     col_out callers). */
  float _Complex single[4 * 8] = { 0 };
  memcpy (single, ref2, 8 * sizeof *single);
  k         = defaults (single, 4, 8);
  k.col_out = 3;
  DP_CHECK (same_object (make (&k, NULL),
                         dp_correlator_create_2d (single, 4, 8, 1, 1, 0, 0, 3),
                         DP_CORRELATOR_2D, x, 32, 3, 0));

  /* Delay-Doppler. */
  k              = defaults (NRZ, 1, 31);
  k.doppler_rows = 8;
  k.ny_out       = 16;
  k.dwell        = 2;
  DP_CHECK (
      same_object (make (&k, NULL),
                   dp_correlator_create_delay_doppler (NRZ, 31, 8, 2, 1, 16),
                   DP_CORRELATOR_DELAY_DOPPLER, x, 31 * 8, 4, 0));

  /* The detectors and searches, on a stream holding the code. */
  float _Complex *chips1 = code_stream (31 * 40, 1, 5, 1.0f);
  float _Complex *chips4 = code_stream (124 * 200, 4, 37, 1.0f);

  size_t before = hits_seen;
  k             = defaults (NRZ, 1, 31);
  k.detect      = 1;
  k.threshold   = 4.0f;
  k.noise_lo    = 3;
  k.noise_hi    = 20;
  k.noise_mode  = DET_NOISE_MEDIAN;
  DP_CHECK (same_object (make (&k, NULL),
                         dp_correlator_create_detector_1d (
                             NRZ, 31, 1, 3, 20, DET_NOISE_MEDIAN, 4.0f, 1),
                         DP_CORRELATOR_DETECTOR_1D, chips1, 0, 0, 31 * 40));
  DP_CHECK (hits_seen > before);

  /* 2-D detector: the code in row 0 of a 4 x 31 reference. */
  float _Complex code2[4 * 31] = { 0 };
  memcpy (code2, NRZ, sizeof NRZ);
  before      = hits_seen;
  k           = defaults (code2, 4, 31);
  k.detect    = 1;
  k.threshold = 2.0f;
  DP_CHECK (
      same_object (make (&k, NULL),
                   dp_correlator_create_detector_2d (
                       code2, 4, 31, 1, 0, SIZE_MAX, DET_NOISE_MEAN, 2.0f, 1),
                   DP_CORRELATOR_DETECTOR_2D, chips1, 0, 0, 31 * 40));
  DP_CHECK (hits_seen > before);

  /* Continuous search: the NRZ reference reaches the chip constructor as
     the chips it maps from. */
  before                = hits_seen;
  k                     = defaults (NRZ, 1, 31);
  k.framing             = DP_CORRELATOR_FRAMING_CONTINUOUS;
  k.sample_rate         = 4.0e6;
  k.chip_rate           = 1.0e6;
  k.symbol_rate         = 1.0e3;
  k.cn0_dbhz            = 55.0;
  k.doppler_uncertainty = 5.0e3;
  k.noise_mode          = DET_NOISE_MEDIAN;
  DP_CHECK (same_object (make (&k, NULL),
                         dp_correlator_create_continuous (
                             CHIPS, 31, 4.0e6, 1.0e6, 1.0e3, 55.0, 5.0e3, 1e-3,
                             0.9, DET_NOISE_MEDIAN, 1, 0.0),
                         DP_CORRELATOR_SEARCH_CONTINUOUS, chips4, 0, 0,
                         124 * 200));
  DP_CHECK (hits_seen > before);

  /* symbol_rate 0 is no known data clock, which the continuous search
     honors (section 17). */
  k.symbol_rate = 0.0;
  DP_CHECK (same_object (make (&k, NULL),
                         dp_correlator_create_continuous (
                             CHIPS, 31, 4.0e6, 1.0e6, 0.0, 55.0, 5.0e3, 1e-3,
                             0.9, DET_NOISE_MEDIAN, 1, 0.0),
                         DP_CORRELATOR_SEARCH_CONTINUOUS, chips4, 0, 0,
                         124 * 200));

  /* Burst search, with no design point (cn0_dbhz's default), the code at
     four samples a chip as its template. */
  float _Complex tmpl[124];
  for (size_t i = 0; i < 124; i++)
    tmpl[i] = NRZ[i / 4];
  before                = hits_seen;
  k                     = defaults (tmpl, 1, 124);
  k.framing             = DP_CORRELATOR_FRAMING_BURST;
  k.sample_rate         = 4.0e6;
  k.reps                = 2;
  k.doppler_uncertainty = 5.0e3;
  DP_CHECK (same_object (
      make (&k, NULL),
      dp_correlator_create_burst (tmpl, 124, 2, 4.0e6, DP_CORRELATOR_CN0_NONE,
                                  5.0e3, 1e-3, 0.9, DET_NOISE_MEAN, 0.0, 1),
      DP_CORRELATOR_SEARCH_BURST, chips4, 0, 0, 124 * 200));
  DP_CHECK (hits_seen > before);

  /* A search honors detect = 1, so the rule accepts it (section 17). */
  k.detect = 1;
  DP_CHECK (same_object (
      make (&k, NULL),
      dp_correlator_create_burst (tmpl, 124, 2, 4.0e6, DP_CORRELATOR_CN0_NONE,
                                  5.0e3, 1e-3, 0.9, DET_NOISE_MEAN, 0.0, 1),
      DP_CORRELATOR_SEARCH_BURST, chips4, 0, 0, 124 * 200));
  free (chips1);
  free (chips4);
  return 0;
}

/* One refusal: NULL, and a sentence that names `knob`. */
static int
refused (const knobs_t *k, const char *knob)
{
  const char            *why = NULL;
  dp_correlator_state_t *s   = make (k, &why);
  const int ok = s == NULL && why != NULL && strstr (why, knob) != NULL;
  if (!ok)
    DP_TEST_ERR ("  expected a refusal naming '%s'; got %s: %s\n", knob,
                 s ? "an object" : "NULL", why ? why : "(no sentence)");
  dp_correlator_destroy (s);
  /* NULL is a valid `why`: the same refusal, nothing written. */
  s                  = make (k, NULL);
  const int null_too = s == NULL;
  dp_correlator_destroy (s);
  return ok && null_too;
}

static int
test_a_value_not_honored_is_refused (void)
{
  float _Complex ref2[4 * 8];
  for (size_t i = 0; i < 4 * 8; i++)
    ref2[i] = 1.0f + (float)i * 0.1f * I;
  knobs_t k;

  /* Shared. */
  k         = defaults (NRZ, 1, 31);
  k.framing = (dp_correlator_framing_t)3;
  DP_CHECK (refused (&k, "framing"));
  k = defaults (NULL, 1, 31);
  DP_CHECK (refused (&k, "ref"));
  k = defaults (NRZ, 1, 0);
  DP_CHECK (refused (&k, "ref"));
  /* noise_mode out of range, on the flavors that read it: a detector and
     both searches (on a map, any non-default mode is refused anyway). */
  k            = defaults (NRZ, 1, 31);
  k.detect     = 1;
  k.noise_mode = 4;
  DP_CHECK (refused (&k, "noise_mode"));
  k.noise_mode = -1;
  DP_CHECK (refused (&k, "noise_mode"));
  k             = defaults (NRZ, 1, 31);
  k.framing     = DP_CORRELATOR_FRAMING_BURST;
  k.sample_rate = 1.0;
  k.noise_mode  = 4;
  DP_CHECK (refused (&k, "noise_mode"));
  k             = defaults (NRZ, 1, 31);
  k.framing     = DP_CORRELATOR_FRAMING_CONTINUOUS;
  k.sample_rate = 4.0e6;
  k.chip_rate   = 1.0e6;
  k.cn0_dbhz    = 55.0;
  k.noise_mode  = 4;
  DP_CHECK (refused (&k, "noise_mode"));

  /* framing 'none': a search's knobs, each alone. */
  k             = defaults (NRZ, 1, 31);
  k.sample_rate = 1.0;
  DP_CHECK (refused (&k, "sample_rate"));
  k          = defaults (NRZ, 1, 31);
  k.cn0_dbhz = 50.0;
  DP_CHECK (refused (&k, "cn0_dbhz"));
  k                     = defaults (NRZ, 1, 31);
  k.doppler_uncertainty = 1.0;
  DP_CHECK (refused (&k, "doppler_uncertainty"));
  k     = defaults (NRZ, 1, 31);
  k.pfa = 1e-2;
  DP_CHECK (refused (&k, "pfa"));
  k    = defaults (NRZ, 1, 31);
  k.pd = 0.5;
  DP_CHECK (refused (&k, "pd"));
  k              = defaults (NRZ, 1, 31);
  k.doppler_rate = 1.0;
  DP_CHECK (refused (&k, "doppler_rate"));
  k           = defaults (NRZ, 1, 31);
  k.chip_rate = 1.0;
  DP_CHECK (refused (&k, "chip_rate"));
  k             = defaults (NRZ, 1, 31);
  k.symbol_rate = 1.0;
  DP_CHECK (refused (&k, "symbol_rate"));
  k                  = defaults (NRZ, 1, 31);
  k.code_only_epochs = 2;
  DP_CHECK (refused (&k, "code_only_epochs"));
  k      = defaults (NRZ, 1, 31);
  k.reps = 2;
  DP_CHECK (refused (&k, "reps"));
  k                 = defaults (NRZ, 1, 31);
  k.rate_hypotheses = 2;
  DP_CHECK (refused (&k, "rate_hypotheses"));

  /* A map's knobs a detector does not read, and the reverse. */
  k        = defaults (NRZ, 1, 31);
  k.detect = 1;
  k.nx_out = 64;
  DP_CHECK (refused (&k, "nx_out"));
  k        = defaults (ref2, 4, 8);
  k.detect = 1;
  k.ny_out = 8;
  DP_CHECK (refused (&k, "ny_out"));
  k         = defaults (ref2, 4, 8);
  k.detect  = 1;
  k.col_out = 0;
  DP_CHECK (refused (&k, "col_out"));
  k           = defaults (ref2, 4, 8);
  k.threshold = 1.0f;
  DP_CHECK (refused (&k, "threshold"));
  k          = defaults (NRZ, 1, 31);
  k.noise_lo = 1;
  DP_CHECK (refused (&k, "noise_lo"));
  k          = defaults (NRZ, 1, 31);
  k.noise_hi = 4;
  DP_CHECK (refused (&k, "noise_hi"));
  k            = defaults (NRZ, 1, 31);
  k.noise_mode = DET_NOISE_MAX;
  DP_CHECK (refused (&k, "noise_mode"));

  /* One row: the 1-D flavor, which nx_out pads and nothing else shapes. */
  k        = defaults (NRZ, 1, 31);
  k.ny_out = 2;
  DP_CHECK (refused (&k, "ny_out"));
  k         = defaults (NRZ, 1, 31);
  k.col_out = 0;
  DP_CHECK (refused (&k, "col_out"));

  /* Delay-Doppler. */
  k              = defaults (NRZ, 1, 31);
  k.doppler_rows = 8;
  k.detect       = 1;
  DP_CHECK (refused (&k, "detect"));
  k              = defaults (ref2, 4, 8);
  k.doppler_rows = 8;
  DP_CHECK (refused (&k, "doppler_rows"));
  k              = defaults (NRZ, 1, 31);
  k.doppler_rows = 8;
  k.nx_out       = 64;
  DP_CHECK (refused (&k, "nx_out"));
  k              = defaults (NRZ, 1, 31);
  k.doppler_rows = 8;
  k.col_out      = 0;
  DP_CHECK (refused (&k, "col_out"));
  k              = defaults (NRZ, 1, 31);
  k.doppler_rows = 8;
  k.threshold    = 1.0f;
  DP_CHECK (refused (&k, "threshold"));

  /* The flavor's own refusals reach the caller with a sentence too. */
  k       = defaults (NRZ, 1, 31);
  k.dwell = 0;
  DP_CHECK (refused (&k, "dwell"));
  k.detect = 1;
  DP_CHECK (refused (&k, "dwell"));
  k         = defaults (ref2, 4, 8);
  k.col_out = 3; /* energy outside row 0: no known column */
  DP_CHECK (refused (&k, "col_out"));
  k              = defaults (NRZ, 1, 31);
  k.doppler_rows = 8;
  k.ny_out       = 4; /* fewer than doppler_rows */
  DP_CHECK (refused (&k, "ny_out"));

  /* Both searches: the map's and the detector's knobs, each alone. */
  for (int f = 1; f <= 2; f++)
    {
      knobs_t s                  = defaults (NRZ, 1, 31);
      s.framing                  = (dp_correlator_framing_t)f;
      s.sample_rate              = 4.0e6;
      s.chip_rate                = f == 1 ? 1.0e6 : 0.0;
      s.cn0_dbhz                 = f == 1 ? 55.0 : DP_CORRELATOR_CN0_NONE;
      const char            *why = "unset";
      dp_correlator_state_t *ok  = make (&s, &why);
      DP_REQUIRE_MSG (ok != NULL, why ? why : "the search's base refused");
      dp_correlator_destroy (ok);

      k              = s;
      k.doppler_rows = 4;
      DP_CHECK (refused (&k, "doppler_rows"));
      k       = s;
      k.dwell = 2;
      DP_CHECK (refused (&k, "dwell"));
      k        = s;
      k.ny_out = 2;
      DP_CHECK (refused (&k, "ny_out"));
      k        = s;
      k.nx_out = 64;
      DP_CHECK (refused (&k, "nx_out"));
      k         = s;
      k.col_out = 0;
      DP_CHECK (refused (&k, "col_out"));
      k = s;
      /* A search honors a thread count, but through set_threads() after
         create: the sentence must send the caller there (section 17). */
      k.nthreads = 2;
      DP_CHECK (refused (&k, "nthreads"));
      DP_CHECK (refused (&k, "set_threads()"));
      k.nthreads = 0;
      DP_CHECK (refused (&k, "set_threads()"));
      k           = s;
      k.threshold = 3.0f;
      DP_CHECK (refused (&k, "threshold"));
      k          = s;
      k.noise_lo = 1;
      DP_CHECK (refused (&k, "noise_lo"));
      k          = s;
      k.noise_hi = 4;
      DP_CHECK (refused (&k, "noise_hi"));
      /* A search correlates one row. */
      k     = s;
      k.ny  = 4;
      k.nx  = 8;
      k.ref = ref2;
      DP_CHECK (refused (&k, "row"));
      /* No rate by omission (section 17). */
      k             = s;
      k.sample_rate = 0.0;
      DP_CHECK (refused (&k, "sample_rate"));
      k             = s;
      k.sample_rate = NAN;
      DP_CHECK (refused (&k, "sample_rate"));
    }

  /* The continuous search alone. */
  knobs_t c     = defaults (NRZ, 1, 31);
  c.framing     = DP_CORRELATOR_FRAMING_CONTINUOUS;
  c.sample_rate = 4.0e6;
  c.chip_rate   = 1.0e6;
  c.cn0_dbhz    = 55.0;
  k             = c;
  k.chip_rate   = 0.0;
  DP_CHECK (refused (&k, "chip_rate"));
  k          = c;
  k.cn0_dbhz = DP_CORRELATOR_CN0_NONE;
  DP_CHECK (refused (&k, "cn0_dbhz"));
  k      = c;
  k.reps = 2;
  DP_CHECK (refused (&k, "reps"));
  k                 = c;
  k.rate_hypotheses = 2;
  DP_CHECK (refused (&k, "rate_hypotheses"));
  /* The reference is NRZ chips, exactly: never quantized. */
  float _Complex bent[31];
  memcpy (bent, NRZ, sizeof bent);
  bent[5] = 0.5f;
  k       = c;
  k.ref   = bent;
  DP_CHECK (refused (&k, "chips"));
  memcpy (bent, NRZ, sizeof bent);
  bent[30] = -1.0f + 0.01f * I;
  DP_CHECK (refused (&k, "chips"));
  /* The flavor's own refusal: a code period that is not whole samples. */
  k             = c;
  k.sample_rate = 3.3e6;
  DP_CHECK (refused (&k, "sample_rate"));

  /* The burst search alone. */
  knobs_t b     = defaults (NRZ, 1, 31);
  b.framing     = DP_CORRELATOR_FRAMING_BURST;
  b.sample_rate = 1.0;
  k             = b;
  k.chip_rate   = 1.0e6;
  DP_CHECK (refused (&k, "chip_rate"));
  k             = b;
  k.symbol_rate = 1.0e3;
  DP_CHECK (refused (&k, "symbol_rate"));
  k                  = b;
  k.code_only_epochs = 2;
  DP_CHECK (refused (&k, "code_only_epochs"));
  k     = b;
  k.pfa = 1.5; /* the flavor's own refusal */
  DP_CHECK (refused (&k, "pfa"));
  return 0;
}

int
main (void)
{
  make_code ();
  (void)test_each_row_builds_its_flavor ();
  (void)test_a_value_not_honored_is_refused ();
  DP_TEST_END ("test_correlator_create");
}
