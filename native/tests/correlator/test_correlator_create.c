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
#include "doppler/correlator/correlator_private.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include <math.h>
#include <stddef.h>
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
              = dp_correlator_execute (a, x + f * frame, frame, oa, cap, NULL);
          const size_t kb
              = dp_correlator_execute (b, x + f * frame, frame, ob, cap, NULL);
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

/* One refusal: NULL, and exactly @p sentence. The whole sentence must
   match: a refusal names exactly one knob, and a substring match would
   pass a sentence that lists several. */
#define refused(k, sentence) refused_at ((k), (sentence), __LINE__)
static int
refused_at (const knobs_t *k, const char *sentence, int line)
{
  const char            *why = NULL;
  dp_correlator_state_t *s   = make (k, &why);
  const int ok = s == NULL && why != NULL && strcmp (why, sentence) == 0;
  if (!ok)
    DP_TEST_ERR ("  REFUSAL-MISMATCH line %d\n    want: %s\n    got:  %s\n",
                 line, sentence, why ? why : "(no sentence)");
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
  DP_CHECK (refused (
      &k, "Correlator: framing must be 'none', 'continuous' or 'burst'"));
  k = defaults (NULL, 1, 31);
  DP_CHECK (refused (&k, "Correlator: ref is empty"));
  k = defaults (NRZ, 1, 0);
  DP_CHECK (refused (&k, "Correlator: ref is empty"));
  /* noise_mode out of range, on the flavors that read it: a detector and
     both searches (on a map, any non-default mode is refused anyway). */
  k            = defaults (NRZ, 1, 31);
  k.detect     = 1;
  k.noise_mode = 4;
  DP_CHECK (refused (
      &k, "Correlator: noise_mode must be 'mean', 'median', 'min' or 'max'"));
  k.noise_mode = -1;
  DP_CHECK (refused (
      &k, "Correlator: noise_mode must be 'mean', 'median', 'min' or 'max'"));
  k             = defaults (NRZ, 1, 31);
  k.framing     = DP_CORRELATOR_FRAMING_BURST;
  k.sample_rate = 1.0;
  k.noise_mode  = 4;
  DP_CHECK (refused (
      &k, "Correlator: noise_mode must be 'mean', 'median', 'min' or 'max'"));
  k             = defaults (NRZ, 1, 31);
  k.framing     = DP_CORRELATOR_FRAMING_CONTINUOUS;
  k.sample_rate = 4.0e6;
  k.chip_rate   = 1.0e6;
  k.cn0_dbhz    = 55.0;
  k.noise_mode  = 4;
  DP_CHECK (refused (
      &k, "Correlator: noise_mode must be 'mean', 'median', 'min' or 'max'"));

  /* framing 'none': a search's knobs, each alone. */
  k             = defaults (NRZ, 1, 31);
  k.sample_rate = 1.0;
  DP_CHECK (refused (&k, "Correlator: sample_rate is a search's; it needs "
                         "framing='continuous' or 'burst'"));
  k          = defaults (NRZ, 1, 31);
  k.cn0_dbhz = 50.0;
  DP_CHECK (refused (&k, "Correlator: cn0_dbhz is a search's; it needs "
                         "framing='continuous' or 'burst'"));
  k                     = defaults (NRZ, 1, 31);
  k.doppler_uncertainty = 1.0;
  DP_CHECK (refused (&k, "Correlator: doppler_uncertainty is a search's; it "
                         "needs framing='continuous' or 'burst'"));
  k     = defaults (NRZ, 1, 31);
  k.pfa = 1e-2;
  DP_CHECK (refused (&k, "Correlator: pfa is a search's; it needs "
                         "framing='continuous' or 'burst'"));
  k    = defaults (NRZ, 1, 31);
  k.pd = 0.5;
  DP_CHECK (refused (&k, "Correlator: pd is a search's; it needs "
                         "framing='continuous' or 'burst'"));
  k              = defaults (NRZ, 1, 31);
  k.doppler_rate = 1.0;
  DP_CHECK (refused (&k, "Correlator: doppler_rate is a search's; it needs "
                         "framing='continuous' or 'burst'"));
  k           = defaults (NRZ, 1, 31);
  k.chip_rate = 1.0;
  DP_CHECK (refused (&k, "Correlator: chip_rate is the continuous search's; "
                         "it needs framing='continuous'"));
  k             = defaults (NRZ, 1, 31);
  k.symbol_rate = 1.0;
  DP_CHECK (refused (&k, "Correlator: symbol_rate is the continuous search's; "
                         "it needs framing='continuous'"));
  k                  = defaults (NRZ, 1, 31);
  k.code_only_epochs = 2;
  DP_CHECK (refused (&k, "Correlator: code_only_epochs is the continuous "
                         "search's; it needs framing='continuous'"));
  k      = defaults (NRZ, 1, 31);
  k.reps = 2;
  DP_CHECK (refused (
      &k, "Correlator: reps is the burst search's; it needs framing='burst'"));
  k                 = defaults (NRZ, 1, 31);
  k.rate_hypotheses = 2;
  DP_CHECK (refused (&k, "Correlator: rate_hypotheses is the burst search's; "
                         "it needs framing='burst'"));

  /* A map's knobs a detector does not read, and the reverse. */
  k        = defaults (NRZ, 1, 31);
  k.detect = 1;
  k.nx_out = 64;
  DP_CHECK (refused (&k, "Correlator: nx_out shapes a map; a detector reports "
                         "hits (detect=True)"));
  k        = defaults (ref2, 4, 8);
  k.detect = 1;
  k.ny_out = 8;
  DP_CHECK (refused (&k, "Correlator: ny_out shapes a map; a detector reports "
                         "hits (detect=True)"));
  k         = defaults (ref2, 4, 8);
  k.detect  = 1;
  k.col_out = 0;
  DP_CHECK (refused (&k, "Correlator: col_out shapes a map; a detector "
                         "reports hits (detect=True)"));
  k           = defaults (ref2, 4, 8);
  k.threshold = 1.0f;
  DP_CHECK (refused (
      &k, "Correlator: threshold is a detector's; it needs detect=True"));
  k          = defaults (NRZ, 1, 31);
  k.noise_lo = 1;
  DP_CHECK (refused (
      &k, "Correlator: noise_lo is a detector's; it needs detect=True"));
  k          = defaults (NRZ, 1, 31);
  k.noise_hi = 4;
  DP_CHECK (refused (
      &k, "Correlator: noise_hi is a detector's; it needs detect=True"));
  k            = defaults (NRZ, 1, 31);
  k.noise_mode = DET_NOISE_MAX;
  DP_CHECK (refused (
      &k, "Correlator: noise_mode is a detector's; it needs detect=True"));

  /* One row: the 1-D flavor, which nx_out pads and nothing else shapes. */
  k        = defaults (NRZ, 1, 31);
  k.ny_out = 2;
  DP_CHECK (refused (
      &k,
      "Correlator: ny_out pads the rows of a 2-D ref, and ref has one row"));
  k         = defaults (NRZ, 1, 31);
  k.col_out = 0;
  DP_CHECK (refused (&k, "Correlator: col_out emits one lag of each row of a "
                         "2-D ref, and ref has one row"));

  /* Delay-Doppler. */
  k              = defaults (NRZ, 1, 31);
  k.doppler_rows = 8;
  k.detect       = 1;
  DP_CHECK (refused (&k, "Correlator: doppler_rows builds a delay-Doppler "
                         "map, which does not detect (detect=False)"));
  k              = defaults (ref2, 4, 8);
  k.doppler_rows = 8;
  DP_CHECK (refused (&k, "Correlator: doppler_rows correlates a 1-D code, and "
                         "ref has more than one row"));
  k              = defaults (NRZ, 1, 31);
  k.doppler_rows = 8;
  k.nx_out       = 64;
  DP_CHECK (refused (&k, "Correlator: nx_out shapes a map; the delay-Doppler "
                         "map's delay axis is native"));
  k              = defaults (NRZ, 1, 31);
  k.doppler_rows = 8;
  k.col_out      = 0;
  DP_CHECK (refused (&k, "Correlator: col_out shapes a map; the delay-Doppler "
                         "map's delay axis is native"));
  k              = defaults (NRZ, 1, 31);
  k.doppler_rows = 8;
  k.threshold    = 1.0f;
  DP_CHECK (refused (
      &k, "Correlator: threshold is a detector's; it needs detect=True"));

  /* The flavor's own refusals reach the caller with a sentence too. */
  k       = defaults (NRZ, 1, 31);
  k.dwell = 0;
  DP_CHECK (
      refused (&k, "Correlator: dwell must be at least 1 (frames a dump)"));
  k.detect = 1;
  DP_CHECK (
      refused (&k, "Correlator: dwell must be at least 1 (frames a dump)"));
  k         = defaults (ref2, 4, 8);
  k.col_out = 3; /* energy outside row 0: no known column */
  DP_CHECK (refused (
      &k,
      "Correlator: col_out needs a ref with energy in its first row only"));
  k              = defaults (NRZ, 1, 31);
  k.doppler_rows = 8;
  k.ny_out       = 4; /* fewer than doppler_rows */
  DP_CHECK (refused (&k, "Correlator: ny_out pads the Doppler rows; it must "
                         "be 0 (native) or at least doppler_rows"));

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
      DP_CHECK (refused (&k, "Correlator: a search sizes its own Doppler "
                             "grid; doppler_rows needs framing='none'"));
      k       = s;
      k.dwell = 2;
      DP_CHECK (refused (
          &k, "Correlator: dwell shapes a map; a search sizes its own dwell"));
      k        = s;
      k.ny_out = 2;
      DP_CHECK (refused (
          &k, "Correlator: ny_out shapes a map; a search reports hits"));
      k        = s;
      k.nx_out = 64;
      DP_CHECK (refused (
          &k, "Correlator: nx_out shapes a map; a search reports hits"));
      k         = s;
      k.col_out = 0;
      DP_CHECK (refused (
          &k, "Correlator: col_out shapes a map; a search reports hits"));
      k = s;
      /* A search honors a thread count, but through set_threads() after
         create: the sentence must send the caller there (section 17). */
      k.nthreads = 2;
      DP_CHECK (refused (
          &k, "Correlator: a search sizes its own thread pool at create; "
              "nthreads applies to framing='none', and a search's count is "
              "set after create with set_threads()"));
      DP_CHECK (refused (
          &k, "Correlator: a search sizes its own thread pool at create; "
              "nthreads applies to framing='none', and a search's count is "
              "set after create with set_threads()"));
      k.nthreads = 0;
      DP_CHECK (refused (
          &k, "Correlator: a search sizes its own thread pool at create; "
              "nthreads applies to framing='none', and a search's count is "
              "set after create with set_threads()"));
      k           = s;
      k.threshold = 3.0f;
      DP_CHECK (refused (&k,
                         "Correlator: a search sets its threshold from pfa; "
                         "threshold needs framing='none' and detect=True"));
      k          = s;
      k.noise_lo = 1;
      DP_CHECK (refused (&k, "Correlator: a search estimates its own noise; "
                             "noise_lo needs framing='none' and detect=True"));
      k          = s;
      k.noise_hi = 4;
      DP_CHECK (refused (&k, "Correlator: a search estimates its own noise; "
                             "noise_hi needs framing='none' and detect=True"));
      /* A search correlates one row. */
      k     = s;
      k.ny  = 4;
      k.nx  = 8;
      k.ref = ref2;
      DP_CHECK (refused (&k, "Correlator: a search correlates a 1-D ref, and "
                             "ref has more than one row"));
      /* No rate by omission (section 17). */
      k             = s;
      k.sample_rate = 0.0;
      DP_CHECK (refused (&k,
                         "Correlator: a search needs sample_rate; 0 is not "
                         "given (sample_rate=1.0 for normalized units)"));
      k             = s;
      k.sample_rate = NAN;
      DP_CHECK (refused (&k,
                         "Correlator: a search needs sample_rate; 0 is not "
                         "given (sample_rate=1.0 for normalized units)"));
    }

  /* The continuous search alone. */
  knobs_t c     = defaults (NRZ, 1, 31);
  c.framing     = DP_CORRELATOR_FRAMING_CONTINUOUS;
  c.sample_rate = 4.0e6;
  c.chip_rate   = 1.0e6;
  c.cn0_dbhz    = 55.0;
  k             = c;
  k.chip_rate   = 0.0;
  DP_CHECK (refused (
      &k,
      "Correlator: the continuous search needs chip_rate; 0 is not given"));
  k          = c;
  k.cn0_dbhz = DP_CORRELATOR_CN0_NONE;
  DP_CHECK (refused (&k, "Correlator: the continuous search sizes its dwell "
                         "from a design point; it needs a finite cn0_dbhz"));
  k      = c;
  k.reps = 2;
  DP_CHECK (refused (
      &k, "Correlator: reps is the burst search's; it needs framing='burst'"));
  k                 = c;
  k.rate_hypotheses = 2;
  DP_CHECK (refused (&k, "Correlator: rate_hypotheses is the burst search's; "
                         "it needs framing='burst'"));
  /* The reference is NRZ chips, exactly: never quantized. */
  float _Complex bent[31];
  memcpy (bent, NRZ, sizeof bent);
  bent[5] = 0.5f;
  k       = c;
  k.ref   = bent;
  DP_CHECK (refused (&k, "Correlator: the continuous search's ref is NRZ "
                         "chips, each exactly +1 or -1"));
  memcpy (bent, NRZ, sizeof bent);
  bent[30] = -1.0f + 0.01f * I;
  DP_CHECK (refused (&k, "Correlator: the continuous search's ref is NRZ "
                         "chips, each exactly +1 or -1"));
  /* The flavor's own refusal: a code period that is not whole samples. */
  k             = c;
  k.sample_rate = 3.3e6;
  DP_CHECK (refused (&k, "Correlator: the continuous search needs a code "
                         "period, sample_rate * len(ref) / chip_rate, that is "
                         "a whole number of samples, at least len(ref)"));

  /* The burst search alone. */
  knobs_t b     = defaults (NRZ, 1, 31);
  b.framing     = DP_CORRELATOR_FRAMING_BURST;
  b.sample_rate = 1.0;
  k             = b;
  k.chip_rate   = 1.0e6;
  DP_CHECK (refused (&k, "Correlator: chip_rate is the continuous search's; "
                         "it needs framing='continuous'"));
  k             = b;
  k.symbol_rate = 1.0e3;
  DP_CHECK (refused (&k, "Correlator: symbol_rate is the continuous search's; "
                         "it needs framing='continuous'"));
  k                  = b;
  k.code_only_epochs = 2;
  DP_CHECK (refused (&k, "Correlator: code_only_epochs is the continuous "
                         "search's; it needs framing='continuous'"));
  k     = b;
  k.pfa = 1.5; /* the flavor's own refusal */
  DP_CHECK (refused (&k, "Correlator: pfa must lie strictly between 0 and 1"));
  return 0;
}

/* A flavor's own refusal names its knob too: the dispatcher hands its
   `why` to the flavor's constructor, so a knob the flavor refuses (not
   the dispatcher) is still named, one sentence a knob. Each row breaks
   exactly one knob of a configuration that builds. */
static int
test_a_flavor_names_the_knob_it_refuses (void)
{
  float _Complex ref2[4 * 8], silent[31] = { 0 };
  for (size_t i = 0; i < 4 * 8; i++)
    ref2[i] = 1.0f + (float)i * 0.1f * I;
  knobs_t k, base;

  /* The maps and detectors (corr2d_create, corr_dd_create). */
  k       = defaults (NRZ, 1, 31);
  k.dwell = 0;
  DP_CHECK (
      refused (&k, "Correlator: dwell must be at least 1 (frames a dump)"));
  k        = defaults (NRZ, 1, 31);
  k.nx_out = 10;
  DP_CHECK (refused (&k, "Correlator: nx_out pads the lags; it must be 0 "
                         "(native) or at least len(ref)"));
  k        = defaults (NRZ, 1, 31);
  k.detect = 1;
  k.dwell  = 0;
  DP_CHECK (
      refused (&k, "Correlator: dwell must be at least 1 (frames a dump)"));
  k        = defaults (ref2, 4, 8);
  k.ny_out = 3;
  DP_CHECK (refused (&k, "Correlator: ny_out pads the rows; it must be 0 "
                         "(native) or at least the ref's rows"));
  k        = defaults (ref2, 4, 8);
  k.nx_out = 7;
  DP_CHECK (refused (&k, "Correlator: nx_out pads the lags; it must be 0 "
                         "(native) or at least len(ref)"));
  k         = defaults (ref2, 4, 8);
  k.col_out = 8;
  DP_CHECK (refused (
      &k, "Correlator: col_out must be -1 or a lag below len(ref[0])"));
  /* A padded length whose map no size_t can count: refused by name before
     a transform is planned (#2958). */
  k        = defaults (NRZ, 1, 31);
  k.nx_out = SIZE_MAX / 8;
  DP_CHECK (refused (&k, "Correlator: nx_out asks for a map no size_t can "
                         "hold"));
  k        = defaults (ref2, 4, 8);
  k.ny_out = SIZE_MAX / 16;
  DP_CHECK (refused (&k, "Correlator: ny_out asks for a map no size_t can "
                         "hold"));
  k         = defaults (ref2, 4, 8);
  k.col_out = 0;
  k.ny_out  = 8;
  DP_CHECK (refused (&k, "Correlator: col_out reads a native map; ny_out "
                         "pads it, so it must be 0 with col_out"));
  k         = defaults (ref2, 4, 8);
  k.col_out = 0;
  k.nx_out  = 16;
  DP_CHECK (refused (&k, "Correlator: col_out reads a native map; nx_out "
                         "pads it, so it must be 0 with col_out"));
  k         = defaults (ref2, 4, 8);
  k.col_out = 0; /* every row has energy: no single-row fast path */
  DP_CHECK (refused (
      &k,
      "Correlator: col_out needs a ref with energy in its first row only"));
  k              = defaults (NRZ, 1, 31);
  k.doppler_rows = 4;
  k.dwell        = 0;
  DP_CHECK (
      refused (&k, "Correlator: dwell must be at least 1 (frames a dump)"));
  /* A delay-Doppler map whose bytes wrap a size_t (doppler_rows x 31 x 8
     is 2^64 + 232 here): refused by name before anything is allocated,
     where the unchecked product allocated 232 bytes and the build wrote
     the whole frame past them (#2958). */
  k              = defaults (NRZ, 1, 31);
  k.doppler_rows = (size_t)74382032555280451ULL;
  DP_CHECK (refused (&k, "Correlator: doppler_rows asks for a map no "
                         "size_t can hold"));
  k              = defaults (NRZ, 1, 31);
  k.doppler_rows = 4;
  k.ny_out       = (size_t)74382032555280451ULL;
  DP_CHECK (refused (&k, "Correlator: ny_out asks for a map no size_t can "
                         "hold"));
  k              = defaults (NRZ, 1, 31);
  k.doppler_rows = 4;
  k.ny_out       = 3;
  DP_CHECK (refused (&k, "Correlator: ny_out pads the Doppler rows; it must "
                         "be 0 (native) or at least doppler_rows"));

  /* The continuous search (search_continuous_create and the shared
     search validation). */
  base             = defaults (NRZ, 1, 31);
  base.framing     = DP_CORRELATOR_FRAMING_CONTINUOUS;
  base.sample_rate = 4.0e6;
  base.chip_rate   = 1.0e6;
  base.cn0_dbhz    = 55.0;
  {
    dp_correlator_state_t *ok = make (&base, NULL);
    DP_REQUIRE (ok != NULL);
    dp_correlator_destroy (ok);
  }
  /* A period of ~2^61 samples (sample_rate * 31 / chip_rate): its bytes
     wrap 2^64 to almost nothing, which the replica's unchecked size once
     allocated and the fill loop wrote 2^61 samples past (#2958). */
  k             = base;
  k.sample_rate = 7.438203255528045e22;
  DP_CHECK (refused (&k, "Correlator: sample_rate asks for a code period no "
                         "search grid can hold"));
  k           = base;
  k.chip_rate = INFINITY;
  DP_CHECK (refused (&k, "Correlator: chip_rate must be finite and positive"));
  k             = base;
  k.sample_rate = 4.5e6; /* 139.5 samples a period */
  DP_CHECK (refused (&k, "Correlator: the continuous search needs a code "
                         "period, sample_rate * len(ref) / chip_rate, that is "
                         "a whole number of samples, at least len(ref)"));
  k     = base;
  k.pfa = 1.0;
  DP_CHECK (refused (&k, "Correlator: pfa must lie strictly between 0 and 1"));
  k    = base;
  k.pd = 0.0;
  DP_CHECK (refused (&k, "Correlator: pd must lie strictly between 0 and 1"));
  k                     = base;
  k.doppler_uncertainty = NAN;
  DP_CHECK (
      refused (&k, "Correlator: doppler_uncertainty must be finite and >= 0"));
  /* A uncertainty whose tile count no size_t holds: refused before any
     size is taken from it (x86 once read it as ONE tile). */
  k                     = base;
  k.doppler_uncertainty = 1e300;
  DP_CHECK (refused (&k, "Correlator: doppler_uncertainty asks for more "
                         "Doppler tiles than any search grid can hold"));
  k                  = base;
  k.code_only_epochs = 0;
  DP_CHECK (refused (&k, "Correlator: code_only_epochs must be at least 1"));
  k              = base;
  k.doppler_rate = -1.0;
  DP_CHECK (refused (&k, "Correlator: doppler_rate must be finite and >= 0"));

  /* The burst search (search_burst_create and the shared validation). */
  base             = defaults (NRZ, 1, 31);
  base.framing     = DP_CORRELATOR_FRAMING_BURST;
  base.sample_rate = 1.0;
  {
    dp_correlator_state_t *ok = make (&base, NULL);
    DP_REQUIRE (ok != NULL);
    dp_correlator_destroy (ok);
  }
  k                     = base;
  k.doppler_uncertainty = 1e300;
  DP_CHECK (refused (&k, "Correlator: doppler_uncertainty asks for more "
                         "Doppler tiles than any search grid can hold"));
  /* A depth whose grid's bytes do not fit a size_t (reps x 2 x 31 x 8):
     regrid's checked product refuses it before any allocation, and the
     create names reps. It passes the guard below (reps x 31 x 8 fits). */
  k      = base;
  k.reps = (size_t)37191016277640226ULL;
  DP_CHECK (refused (&k, "Correlator: reps asks for a coherent depth whose "
                         "search grid cannot be allocated"));
  /* The overflow the checked product closes (#2958): a burst at -20 dB-Hz
     over a 127-sample Zadoff-Chu preamble sizes depth 8, interpolation 2
     and 9078122083518481 tiles, inside the tile guard, and the surface's
     ELEMENT count 8 x 2 x 9078122083518481 x 127 wraps 2^64 to 1776. The
     unchecked build allocated those 1776 and wrote the whole grid past
     them; now the create refuses, naming doppler_uncertainty. */
  {
    float _Complex zc[127];
    for (int i = 0; i < 127; i++)
      zc[i] = cexpf (-(float)M_PI * 5.0f * (float)(i * (i + 1)) / 127.0f * I);
    knobs_t w             = defaults (zc, 1, 127);
    w.framing             = DP_CORRELATOR_FRAMING_BURST;
    w.sample_rate         = 1.0;
    w.reps                = 8;
    w.cn0_dbhz            = -20.0;
    w.doppler_uncertainty = 35740638124088.51;
    DP_CHECK (refused (&w, "Correlator: doppler_uncertainty asks for more "
                           "Doppler tiles than the search grid can "
                           "allocate"));
  }
  k      = base;
  k.reps = SIZE_MAX;
  DP_CHECK (refused (&k, "Correlator: reps asks for more periods than any "
                         "search grid can hold"));
  k             = base;
  k.sample_rate = INFINITY;
  DP_CHECK (
      refused (&k, "Correlator: sample_rate must be finite and positive"));
  k      = base;
  k.reps = 0;
  DP_CHECK (refused (
      &k,
      "Correlator: reps must be at least 1 (preamble repetitions a burst)"));
  k     = base;
  k.ref = silent;
  DP_CHECK (refused (
      &k, "Correlator: the burst search's ref needs finite, non-zero energy"));
  k          = base;
  k.cn0_dbhz = INFINITY;
  DP_CHECK (
      refused (&k, "Correlator: cn0_dbhz must be finite, or NaN for none"));
  k     = base;
  k.pfa = 0.0;
  DP_CHECK (refused (&k, "Correlator: pfa must lie strictly between 0 and 1"));
  k              = base;
  k.doppler_rate = INFINITY;
  DP_CHECK (refused (&k, "Correlator: doppler_rate must be finite and >= 0"));
  k                 = base;
  k.doppler_rate    = 1e-6;
  k.rate_hypotheses = 65;
  DP_CHECK (refused (
      &k, "Correlator: rate_hypotheses must be at most 64 (0 = auto)"));
  k                 = base;
  k.rate_hypotheses = 3;
  DP_CHECK (refused (&k, "Correlator: rate_hypotheses other than 1 needs "
                         "doppler_rate > 0, the span the hypotheses cover"));
  return 0;
}

static void
null_sink (void *ctx, const float *surface, size_t rows, size_t cols,
           uint64_t samples_consumed)
{
  (void)ctx;
  (void)surface;
  (void)rows;
  (void)cols;
  (void)samples_consumed;
}

/* The search-only entry points on a flavor without a search record:
   each refuses with its own refusal value and touches nothing
   (docs/design/correlator-python.md section 18). One class reaches every
   member from every flavor, so these must not dereference `search`. */
static int
search_members_refuse (dp_correlator_state_t *s)
{
  float  f[4];
  double d[4];
  float _Complex z[4];
  int ok = 1;
  ok &= dp_correlator_configure_search_raw (s, 1, 1, NULL)
        == DP_ERR_UNSUPPORTED;
  ok &= dp_correlator_set_threads (s, 2) == DP_ERR_UNSUPPORTED;
  ok &= dp_correlator_set_max_peaks (s, 2, NULL) == DP_ERR_UNSUPPORTED;
  ok &= dp_correlator_set_carrier_freq_hz (s, 2.5e9) == DP_ERR_UNSUPPORTED;
  ok &= dp_correlator_set_telemetry (s, NULL, NULL, 1, NULL)
        == DP_ERR_UNSUPPORTED;
  ok &= dp_correlator_surface (s, f, 4) == SIZE_MAX;
  ok &= dp_correlator_surface_doppler_hz (s, d, 4) == SIZE_MAX;
  ok &= dp_correlator_surface_chip_phase (s, d, 4) == SIZE_MAX;
  ok &= dp_correlator_surface_complex (s, z, 4) == SIZE_MAX;
  ok &= dp_correlator_block_prompt (s, 0, 0, z, 4) == SIZE_MAX;
  ok &= dp_correlator_block_raw (s, z, 4) == SIZE_MAX;
  ok &= isnan (dp_correlator_psl_db (s));
  ok &= isnan (dp_correlator_gate_stat (s));
  ok &= isnan (dp_correlator_dsl_db (s));
  ok &= dp_correlator_position (s) == 0;
  ok &= dp_correlator_grid_bins (s) == 0;
  ok &= dp_correlator_set_surface_sink (s, null_sink, NULL, 1)
        == DP_ERR_UNSUPPORTED;
  int keep = 7;
  ok &= dp_correlator_get_keep_surface (s, &keep) == DP_ERR_UNSUPPORTED;
  ok &= keep == 7;
  return ok;
}

static int
test_members_off_their_flavor (void)
{
  float _Complex ref2[4 * 8];
  fill_noise (ref2, 4 * 8, 3u, 1.0f);
  dp_correlator_state_t *maps[] = {
    dp_correlator_create_1d (NRZ, 31, 1, 1, 0),
    dp_correlator_create_2d (ref2, 4, 8, 1, 1, 0, 0, -1),
    dp_correlator_create_delay_doppler (NRZ, 31, 8, 1, 1, 0),
  };
  dp_correlator_state_t *dets[] = {
    dp_correlator_create_detector_1d (NRZ, 31, 1, 0, SIZE_MAX, DET_NOISE_MEAN,
                                      0.0f, 1),
    dp_correlator_create_detector_2d (ref2, 4, 8, 1, 0, SIZE_MAX,
                                      DET_NOISE_MEAN, 0.0f, 1),
  };
  for (size_t i = 0; i < 3; i++)
    {
      DP_REQUIRE (maps[i] != NULL);
      DP_CHECK (search_members_refuse (maps[i]));
      DP_CHECK (dp_correlator_last_corr_status (maps[i])
                == DP_ERR_UNSUPPORTED);
      /* set_ref: the 1-D and 2-D maps serve it, the delay-Doppler map
         refuses (its tables were sized from its code at create). */
      {
        float _Complex r[4 * 31] = { 0 };
        r[0]                     = 1.0f;
        DP_CHECK (
            dp_correlator_set_ref (maps[i], r)
            == (dp_correlator_flavor (maps[i]) == DP_CORRELATOR_DELAY_DOPPLER
                    ? DP_ERR_UNSUPPORTED
                    : DP_OK));
      }
      DP_CHECK (dp_correlator_push_max_out (maps[i], 1000) == 0);
      DP_CHECK (dp_correlator_execute_max_out (maps[i]) > 0);
      dp_correlator_destroy (maps[i]);
    }
  for (size_t i = 0; i < 2; i++)
    {
      DP_REQUIRE (dets[i] != NULL);
      DP_CHECK (search_members_refuse (dets[i]));
      DP_CHECK (dp_correlator_last_corr_status (dets[i]) == DP_OK);
      {
        float _Complex r[4 * 31] = { 0 };
        r[0]                     = 1.0f;
        DP_CHECK (dp_correlator_set_ref (dets[i], r) == DP_OK);
      }
      /* execute() refuses a detector, so nothing sizes a map for it. */
      DP_CHECK (dp_correlator_execute_max_out (dets[i]) == 0);
      DP_CHECK (dp_correlator_execute_all_max_out (dets[i], 64) == 0);
      DP_CHECK (dp_correlator_execute_peaks_max_out (dets[i], 64) == 0);
      dp_correlator_destroy (dets[i]);
    }

  /* A search's frame length n is 0: the batch sizers must not divide by
     it, and execute_max_out sizes nothing. */
  float _Complex tmpl[124];
  for (size_t i = 0; i < 124; i++)
    tmpl[i] = NRZ[i / 4];
  dp_correlator_state_t *b = dp_correlator_create_burst (
      tmpl, 124, 2, 4.0e6, DP_CORRELATOR_CN0_NONE, 5.0e3, 1e-3, 0.9,
      DET_NOISE_MEAN, 0.0, 1);
  dp_correlator_state_t *c = dp_correlator_create_continuous (
      CHIPS, 31, 4.0e6, 1.0e6, 0.0, 55.0, 5.0e3, 1e-3, 0.9, DET_NOISE_MEAN, 1,
      0.0);
  DP_REQUIRE (b != NULL && c != NULL);
  dp_correlator_state_t *searches[] = { b, c };
  for (size_t i = 0; i < 2; i++)
    {
      DP_CHECK (dp_correlator_execute_max_out (searches[i]) == 0);
      DP_CHECK (dp_correlator_execute_all_max_out (searches[i], 1000) == 0);
      DP_CHECK (dp_correlator_execute_peaks_max_out (searches[i], 1000) == 0);
      DP_CHECK (dp_correlator_last_corr_status (searches[i])
                == DP_ERR_UNSUPPORTED);
      {
        /* A search has no forward plan for a reference: refused, never
           run through a NULL plan. */
        float _Complex r[124] = { 0 };
        DP_CHECK (dp_correlator_set_ref (searches[i], r)
                  == DP_ERR_UNSUPPORTED);
      }
      /* Both searches decide dwells, so both take a sink. */
      DP_CHECK (
          dp_correlator_set_surface_sink (searches[i], null_sink, NULL, 1)
          == DP_OK);
      DP_CHECK (dp_correlator_set_surface_sink (searches[i], NULL, NULL, 0)
                == DP_OK);
    }
  /* keep_surface is the continuous search's: the burst refuses the read
     although its sink just set the field, and the continuous serves it. */
  int keep = 7;
  DP_CHECK (b->search->keep_surface == 1);
  DP_CHECK (dp_correlator_get_keep_surface (b, &keep) == DP_ERR_UNSUPPORTED);
  DP_CHECK (keep == 7);
  DP_CHECK (dp_correlator_get_keep_surface (c, &keep) == DP_OK);
  DP_CHECK (keep == 1);

  /* The burst search refuses what only the continuous one honors (the
     carrier coupling, telemetry, the surface and block read-outs), and
     serves surface_complex, which the map-equivalence test reads on it.
     Each refusal is checked where the call would otherwise succeed:
     buffers with room, a decided dwell for surface(), a whole block for
     block_*(). */
  enum
  {
    BIG = 1 << 16
  };
  static float  fb[BIG];
  static double db[BIG];
  static float _Complex zb[BIG];
  DP_CHECK (dp_correlator_set_carrier_freq_hz (b, 2.5e9)
            == DP_ERR_UNSUPPORTED);
  DP_CHECK (dp_correlator_set_telemetry (b, NULL, NULL, 1, NULL)
            == DP_ERR_UNSUPPORTED);
  DP_CHECK (dp_correlator_surface_doppler_hz (b, db, BIG) == SIZE_MAX);
  DP_CHECK (dp_correlator_surface_chip_phase (b, db, BIG) == SIZE_MAX);
  /* ...which the continuous search serves: the same calls succeed. */
  DP_CHECK (dp_correlator_set_carrier_freq_hz (c, 2.5e9) == DP_OK);
  DP_CHECK (dp_correlator_set_telemetry (c, NULL, NULL, 1, NULL) == DP_OK);
  DP_CHECK (dp_correlator_surface_doppler_hz (c, db, BIG) > 0);
  DP_CHECK (dp_correlator_surface_chip_phase (c, db, BIG)
            == c->search->lag_bins);

  /* A wideband burst search, its blocks gathered and its surface kept:
     where block_*() and surface() would hand data out, they refuse. Five
     tiles of depth D = 4: the grid a 55 dB-Hz design sizes here, and the
     one that gathers blocks (tiles and D > 1). */
  const double           span = 4.0e6 / (2.0 * 124.0);
  dp_correlator_state_t *w
      = dp_correlator_create_burst (tmpl, 124, 8, 4.0e6, 55.0, 3.5 * span,
                                    1e-3, 0.9, DET_NOISE_MEAN, 0.0, 1);
  /* Tiled, and a coherent depth pinned above 1: the grid that gathers
     blocks (60 dB-Hz alone sizes D = 1). */
  DP_REQUIRE (w != NULL);
  DP_REQUIRE (w->search->window_bins > 1 && w->search->coherent_bins > 1
              && w->search->blk_raw != NULL);
  w->search->keep_surface   = 1;
  float _Complex *xs        = code_stream (124 * 400, 4, 11, 1.0f);
  int             saw_block = 0, saw_surface = 0;
  for (size_t off = 0; off < 124 * 400; off += 124)
    {
      dp_correlator_hit_t h[64];
      (void)dp_correlator_push (w, xs + off, 124, h, 64);
      if (w->search->blk_raw && w->search->samples_consumed > 0
          && w->search->blk_epoch == 0)
        {
          saw_block = 1;
          DP_CHECK (dp_correlator_block_raw (w, zb, BIG) == SIZE_MAX);
          DP_CHECK (dp_correlator_block_prompt (w, 0, 0, zb, BIG) == SIZE_MAX);
        }
      if (w->search->stat_surface && w->search->surface_at > 0)
        {
          saw_surface = 1;
          DP_CHECK (dp_correlator_surface (w, fb, BIG) == SIZE_MAX);
        }
    }
  DP_REQUIRE_MSG (saw_block, "the wideband burst gathered no whole block");
  DP_REQUIRE_MSG (saw_surface, "the burst kept no decided surface");
  free (xs);
  dp_correlator_destroy (w);
  /* set_max_peaks and set_threads are honored by both searches. */
  DP_CHECK (dp_correlator_set_max_peaks (b, 2, NULL) == 0);
  DP_CHECK (dp_correlator_set_threads (b, 1) == DP_OK);
  DP_CHECK (dp_correlator_grid_bins (b) > 0);
  dp_correlator_destroy (b);
  dp_correlator_destroy (c);
  return 0;
}

/* Push every chunk with a result array exactly push_max_out(chunk) long:
   push must take the whole chunk (a short array would make it stop on a
   frame boundary) and never report more hits than the bound.  Chunks of
   every length 1 .. 3 frames walk the carry across each boundary. */
static int
bound_holds (dp_correlator_state_t *s, const float _Complex *x, size_t len,
             size_t frame)
{
  int    ok  = 1;
  size_t off = 0, step = 1;
  while (off < len && ok)
    {
      const size_t         n     = step < len - off ? step : len - off;
      const size_t         bound = dp_correlator_push_max_out (s, n);
      dp_correlator_hit_t *hits  = malloc ((bound ? bound : 1) * sizeof *hits);
      const size_t got = dp_correlator_push (s, x + off, n, hits, bound);
      ok               = got <= bound && dp_correlator_consumed (s) == n;
      if (!ok)
        DP_TEST_ERR ("  push of %zu at %zu: %zu hits, bound %zu, took "
                     "%zu\n",
                     n, off, got, bound, dp_correlator_consumed (s));
      hits_seen += got;
      free (hits);
      off += n;
      step = step % (3 * frame) + 1;
    }
  return ok;
}

static int
test_push_never_passes_its_bound (void)
{
  /* A detector at threshold 0: every dump is a hit, one a frame. */
  float _Complex        *x1  = code_stream (31 * 60, 1, 5, 1.0f);
  size_t                 was = hits_seen;
  dp_correlator_state_t *det = dp_correlator_create_detector_1d (
      NRZ, 31, 1, 0, SIZE_MAX, DET_NOISE_MEAN, 0.0f, 1);
  DP_REQUIRE (det != NULL);
  DP_CHECK (bound_holds (det, x1, 31 * 60, 31));
  DP_CHECK (hits_seen > was);
  dp_correlator_destroy (det);
  free (x1);

  /* A search listing up to three peaks a dwell, on two emitters at code
     phases far apart, so a dwell makes more than one hit. */
  float _Complex *x4 = code_stream (124 * 300, 4, 37, 1.0f);
  for (size_t i = 61; i < 124 * 300; i++)
    x4[i] += NRZ[((i - 61) / 4) % 31];
  dp_correlator_state_t *c = dp_correlator_create_continuous (
      CHIPS, 31, 4.0e6, 1.0e6, 0.0, 55.0, 5.0e3, 1e-3, 0.9, DET_NOISE_MEAN, 1,
      0.0);
  DP_REQUIRE (c != NULL && dp_correlator_set_max_peaks (c, 3, NULL) == 0);
  was = hits_seen;
  DP_CHECK (bound_holds (c, x4, 124 * 300, c->search->frame_n));
  DP_CHECK (hits_seen > was + 1);
  dp_correlator_destroy (c);
  free (x4);
  return 0;
}

/* keep_surface is a mutator's value (#2022): it rides the search blob
   (version 2), so a fresh engine restored from a blob keeps what the
   source was asked to keep -- and a blob that says anything but 0 or 1 is
   refused, the target untouched. */
static int
test_keep_surface_travels (void)
{
  dp_correlator_state_t *a = dp_correlator_create_continuous (
      CHIPS, 31, 4.0e6, 1.0e6, 0.0, 55.0, 5.0e3, 1e-3, 0.9, DET_NOISE_MEAN, 1,
      0.0);
  dp_correlator_state_t *b = dp_correlator_create_continuous (
      CHIPS, 31, 4.0e6, 1.0e6, 0.0, 55.0, 5.0e3, 1e-3, 0.9, DET_NOISE_MEAN, 1,
      0.0);
  DP_REQUIRE (a != NULL && b != NULL);
  DP_REQUIRE (dp_correlator_set_keep_surface (a, 1) == DP_OK);
  int keep = 7;
  DP_REQUIRE (dp_correlator_get_keep_surface (b, &keep) == DP_OK);
  DP_REQUIRE (keep == 0);
  const size_t   nb   = dp_correlator_state_bytes (a);
  unsigned char *blob = malloc (nb);
  dp_correlator_get_state (a, blob);
  DP_CHECK (dp_correlator_set_state (b, blob) == DP_OK);
  DP_CHECK (dp_correlator_get_keep_surface (b, &keep) == DP_OK);
  DP_CHECK (keep == 1);

  /* A blob whose keep_surface is 2 is refused, and the target is the
     same bytes after as before. */
  dp_correlator_state_t *c = dp_correlator_create_continuous (
      CHIPS, 31, 4.0e6, 1.0e6, 0.0, 55.0, 5.0e3, 1e-3, 0.9, DET_NOISE_MEAN, 1,
      0.0);
  DP_REQUIRE (c != NULL);
  unsigned char *before = malloc (nb), *after = malloc (nb);
  dp_correlator_get_state (c, before);
  const uint16_t two = 2;
  memcpy (blob + DP_CORRELATOR_SEARCH_EXTRA_OFF
              + offsetof (search_extra_t, keep_surface),
          &two, sizeof two);
  DP_CHECK (dp_correlator_set_state (c, blob) == DP_ERR_INVALID);
  dp_correlator_get_state (c, after);
  DP_CHECK (memcmp (before, after, nb) == 0);
  DP_CHECK (dp_correlator_get_keep_surface (c, &keep) == DP_OK);
  DP_CHECK (keep == 0);
  free (before);
  free (after);
  free (blob);
  dp_correlator_destroy (a);
  dp_correlator_destroy (b);
  dp_correlator_destroy (c);
  return 0;
}

static void
count_sink (void *ctx, const float *surface, size_t rows, size_t cols,
            uint64_t samples_consumed)
{
  (void)surface;
  (void)rows;
  (void)cols;
  (void)samples_consumed;
  ++*(size_t *)ctx;
}

/* An attached sink keeps firing across a restore of a keep_surface = 0
   blob, and across set_keep_surface(0): the blob carries keep_surface, not
   the sink, which is the caller's and stays attached. Each case pushes the
   same stream after the change and requires the sink to be handed a
   dwell. */
static int
test_a_sink_survives_a_restore (void)
{
  dp_correlator_state_t *fresh = dp_correlator_create_continuous (
      CHIPS, 31, 4.0e6, 1.0e6, 0.0, 55.0, 5.0e3, 1e-3, 0.9, DET_NOISE_MEAN, 1,
      0.0);
  dp_correlator_state_t *c = dp_correlator_create_continuous (
      CHIPS, 31, 4.0e6, 1.0e6, 0.0, 55.0, 5.0e3, 1e-3, 0.9, DET_NOISE_MEAN, 1,
      0.0);
  DP_REQUIRE (fresh != NULL && c != NULL);
  const size_t   nb   = dp_correlator_state_bytes (fresh);
  unsigned char *blob = malloc (nb);
  DP_REQUIRE (blob != NULL);
  dp_correlator_get_state (fresh, blob); /* keep_surface = 0 */
  const size_t        n = 124 * 400;
  float _Complex     *x = code_stream (n, 4, 11, 1.0f);
  dp_correlator_hit_t hits[DP_CORRELATOR_MAX_PEAKS];

  size_t fired = 0;
  DP_REQUIRE (dp_correlator_set_surface_sink (c, count_sink, &fired, 1)
              == DP_OK);
  DP_REQUIRE (dp_correlator_set_state (c, blob) == DP_OK);
  int keep = 7;
  DP_REQUIRE (dp_correlator_get_keep_surface (c, &keep) == DP_OK);
  DP_REQUIRE (keep == 0); /* the blob's value, restored */
  for (size_t off = 0; off < n;)
    {
      (void)dp_correlator_push (c, x + off, n - off, hits,
                                DP_CORRELATOR_MAX_PEAKS);
      const size_t took = dp_correlator_consumed (c);
      DP_REQUIRE (took > 0);
      off += took;
    }
  DP_CHECK_MSG (fired > 0, "the sink fell silent after a restore");
  /* The rule (dp_correlator_surface()): a surface is kept while
     keep_surface OR a sink is set, so with keep_surface 0 and the sink
     attached, surface() hands out the last dwell, not "nothing yet". */
  const size_t nsurf = c->search->n_surf;
  float       *surf  = malloc (nsurf * sizeof *surf);
  DP_REQUIRE (surf != NULL);
  DP_CHECK (dp_correlator_surface (c, surf, nsurf) == nsurf);
  free (surf);

  const size_t before = fired;
  DP_REQUIRE (dp_correlator_set_keep_surface (c, 0) == DP_OK);
  for (size_t off = 0; off < n;)
    {
      (void)dp_correlator_push (c, x + off, n - off, hits,
                                DP_CORRELATOR_MAX_PEAKS);
      const size_t took = dp_correlator_consumed (c);
      DP_REQUIRE (took > 0);
      off += took;
    }
  DP_CHECK_MSG (fired > before,
                "the sink fell silent after set_keep_surface(0)");
  free (x);
  free (blob);
  dp_correlator_destroy (fresh);
  dp_correlator_destroy (c);
  return 0;
}

/* The out= sentences a C caller reads through `why` (the binding checks
   out= itself, so Python never reaches these): each batch and bank method
   given a buffer one value short names it, and a too-short frame names
   the frame. */
static int
test_out_refusals_are_named (void)
{
  dp_correlator_state_t *c = dp_correlator_create_1d (NRZ, 31, 1, 1, 0);
  DP_REQUIRE (c != NULL);
  float _Complex x[62], z[62];
  float    f[62];
  uint32_t u[62], u2[62];
  float    f2[62];
  for (size_t i = 0; i < 62; i++)
    x[i] = NRZ[i % 31];
  const char *why = NULL;
  DP_CHECK (dp_correlator_execute_all (c, x, 62, z, 61, &why) == SIZE_MAX);
  DP_CHECK (why
            && strcmp (why, "execute_all: out= must hold every map (frames "
                            "x n_out values)")
                   == 0);
  why = NULL;
  DP_CHECK (dp_correlator_execute_coherent (c, x, 62, z, 30, &why)
            == SIZE_MAX);
  DP_CHECK (why
            && strcmp (why, "execute_coherent: out= must hold one map (n_out "
                            "values)")
                   == 0);
  why = NULL;
  DP_CHECK (dp_correlator_execute_power_sum (c, x, 62, f, 30, &why)
            == SIZE_MAX);
  DP_CHECK (why
            && strcmp (why, "execute_power_sum: out= must hold one map "
                            "(n_out values)")
                   == 0);
  why = NULL;
  DP_CHECK (dp_correlator_execute_power_max (c, x, 62, f, u, 30, &why)
            == SIZE_MAX);
  DP_CHECK (why
            && strcmp (why, "execute_power_max: out= must hold one map "
                            "(n_out values)")
                   == 0);
  why = NULL;
  DP_CHECK (dp_correlator_execute_peaks (c, x, 62, f, u, u2, f2, 1, &why)
            == SIZE_MAX);
  DP_CHECK (why
            && strcmp (why, "execute_peaks: out= must hold one record per "
                            "frame")
                   == 0);
  why = NULL;
  /* A bank's refused argument says DP_ERR_INVALID in *status, the cause
     the binding switches on (just-makeit#2584). */
  int status = 0;
  DP_CHECK (dp_correlator_bank_power_max (c, x, 31, 1, 0, 0, NULL, 0, f, u, 30,
                                          &status, &why)
            == SIZE_MAX);
  DP_CHECK (status == DP_ERR_INVALID);
  DP_CHECK (why
            && strcmp (why, "bank_power_max: out= must hold one map (n_out "
                            "values)")
                   == 0);
  why = NULL;
  int32_t i32[62];
  status = 0;
  DP_CHECK (dp_correlator_bank_detect (c, x, 31, 1, 1, 0, 0, 0.0f, 0.0f, NULL,
                                       0, f, i32, u, u2, u2, 30, &status, &why)
            == SIZE_MAX);
  DP_CHECK (status == DP_ERR_INVALID);
  DP_CHECK (why
            && strcmp (why, "bank_detect: out= must hold one record per map "
                            "cell")
                   == 0);
  why = NULL;
  DP_CHECK (dp_correlator_bank_power_max (c, x, 30, 1, 0, 0, NULL, 0, f, u, 31,
                                          NULL, &why)
            == SIZE_MAX);
  DP_CHECK (why
            && strcmp (why, "bank_power_max: takes exactly one frame (ny*nx "
                            "samples)")
                   == 0);
  /* Off a map, why stays NULL: the binding raises the declared sentence. */
  dp_correlator_state_t *d = dp_correlator_create_detector_1d (
      NRZ, 31, 1, 0, SIZE_MAX, DET_NOISE_MEAN, 0.0f, 1);
  DP_REQUIRE (d != NULL);
  why = NULL;
  DP_CHECK (dp_correlator_execute_all (d, x, 62, z, 62, &why) == SIZE_MAX);
  DP_CHECK (why == NULL);
  /* Off a map a bank says DP_ERR_UNSUPPORTED, its sentence the binding's. */
  status = 0;
  DP_CHECK (dp_correlator_bank_power_max (d, x, 31, 1, 0, 0, NULL, 0, f, u, 31,
                                          &status, &why)
                == SIZE_MAX
            && status == DP_ERR_UNSUPPORTED && why == NULL);
  status = 0;
  DP_CHECK (dp_correlator_bank_detect (d, x, 31, 1, 1, 0, 0, 0.0f, 0.0f, NULL,
                                       0, f, i32, u, u2, u2, 31, &status, &why)
                == SIZE_MAX
            && status == DP_ERR_UNSUPPORTED && why == NULL);
  /* A call that succeeds leaves *status as it was. */
  status = 12345;
  DP_CHECK (dp_correlator_bank_power_max (c, x, 31, 1, 0, 0, NULL, 0, f, u, 31,
                                          &status, &why)
                == 31
            && status == 12345);
  status = 12345;
  DP_CHECK (dp_correlator_bank_detect (c, x, 31, 1, 1, 0, 0, 1e30f, 1e30f,
                                       NULL, 0, f, i32, u, u2, u2, 31, &status,
                                       &why)
                == 0
            && status == 12345);
  dp_correlator_destroy (d);
  dp_correlator_destroy (c);
  return 0;
}

int
main (void)
{
  make_code ();
  (void)test_each_row_builds_its_flavor ();
  (void)test_a_value_not_honored_is_refused ();
  (void)test_a_flavor_names_the_knob_it_refuses ();
  (void)test_members_off_their_flavor ();
  (void)test_push_never_passes_its_bound ();
  (void)test_keep_surface_travels ();
  (void)test_a_sink_survives_a_restore ();
  (void)test_out_refusals_are_named ();
  DP_TEST_END ("test_correlator_create");
}
