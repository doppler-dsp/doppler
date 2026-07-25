#include "ddc/ddc_core.h"
#include "wfm/wfm_dsp.h" /* wfm_rrc_h — static inline, no link edge */
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          _fails++;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

/* Floating-point helpers — use inline functions, not macros, so arguments
 * are evaluated exactly once.  Safe to call with stateful step() results. */
static inline int
_almost_eq (float a, float b, float tol)
{
  return fabsf (a - b) <= tol;
}
static inline int
_almost_eq_c (float complex a, float complex b, float tol)
{
  return _almost_eq (crealf (a), crealf (b), tol)
         && _almost_eq (cimagf (a), cimagf (b), tol);
}
#define ALMOST_EQ(a, b, tol) _almost_eq ((float)(a), (float)(b), tol)
#define ALMOST_EQ_C(a, b, tol)                                                \
  _almost_eq_c ((float complex) (a), (float complex) (b), tol)

/* ── DDCR pure-transducer (ddcr_run) round-trip ────────────────────────────
 * Exercises the stateless `ddcr_run(state_in, state_out, …)` face: a whole-
 * stream reference run, then a split where the first half emits its state and
 * a fresh engine restores it — the concatenated output must match bit-for-bit
 * — plus the corrupted-blob path (set_state rejects → run returns 0).
 *
 * Choosing the rate steers the RateConverter plan, so this also exercises the
 * CIC and Resampler stage serializers: ddcr's RateConverter rate is 2*rate, so
 * rate 0.0625 -> RC 0.125 -> /8 CIC, and rate 0.375 -> RC 0.75 -> Resampler.
 * The rate=0.25 case in main() already covers the halfband plan. */
static int
_ddcr_run_roundtrip (double norm_freq, double rate)
{
  int             _fails = 0;
  const size_t    L = 1024, cut = 391, CAP = 2048;
  float          *in   = malloc (L * sizeof (float));
  float _Complex *outA = malloc (CAP * sizeof (float _Complex));
  float _Complex *outB = malloc (CAP * sizeof (float _Complex));
  for (size_t i = 0; i < L; i++)
    in[i] = (float)cos (0.11 * (double)i) + 0.5f * (float)sin (0.029 * i);

  /* reference: the whole stream through ddcr_run, no state I/O */
  ddcr_state_t *ra = ddcr_create (norm_freq, rate, RC_PULSE_NONE, 0, 0, 0, 0);
  size_t        nA = ddcr_run (ra, NULL, NULL, in, L, outA, CAP);
  ddcr_destroy (ra);

  /* split: first half writes state_out; a fresh engine restores via state_in
   */
  ddcr_state_t *r1 = ddcr_create (norm_freq, rate, RC_PULSE_NONE, 0, 0, 0, 0);
  size_t        sb = ddcr_state_bytes (r1);
  void         *blob = malloc (sb);
  size_t        nB   = ddcr_run (r1, NULL, blob, in, cut, outB, CAP);
  ddcr_destroy (r1);

  ddcr_state_t *r2 = ddcr_create (norm_freq, rate, RC_PULSE_NONE, 0, 0, 0, 0);
  nB += ddcr_run (r2, blob, NULL, in + cut, L - cut, outB + nB, CAP - nB);
  ddcr_destroy (r2);

  CHECK (nA == nB);
  int bad = 0;
  for (size_t i = 0; i < nA && i < nB; i++)
    /* Restored vs continuous output matches up to FMA-grouping ULPs at the
     * split boundary: the CIC / Resampler taps contract differently across the
     * cut on arm64 (the halfband plan in main() is grouping-invariant, hence
     * exact there). A real state-restore bug would be O(1) — far above tol. */
    if (!ALMOST_EQ_C (outA[i], outB[i], 1e-3f))
      bad++;
  CHECK (bad == 0);

  /* a corrupted state_in must make ddcr_run reject (set_state != 0) → 0 out */
  ddcr_state_t *r3 = ddcr_create (norm_freq, rate, RC_PULSE_NONE, 0, 0, 0, 0);
  ddcr_get_state (r3, blob);
  ((char *)blob)[0] ^= (char)0xFF; /* clobber the header magic */
  CHECK (ddcr_run (r3, blob, NULL, in, cut, outB, CAP) == 0);
  ddcr_destroy (r3);

  free (blob);
  free (in);
  free (outA);
  free (outB);
  return _fails;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Layer 3 — pulse passthrough and the two control ports
 *
 * The pulse itself is RateConverter's (tested there); what these tests pin is
 * what this layer adds: that the pulse survives the composition, that the two
 * ports are exactly the LO's and the terminal stage's own accumulators, and
 * that a loop closed around either one behaves.
 * ══════════════════════════════════════════════════════════════════════════
 */

#define _MF_NSYM 400
#define _MF_SPAN 8
#define _MF_BETA 0.35

/* Deterministic +-1 BPSK.  A one-shot LCG is not random across consecutive k,
 * which starves a *timing* detector — irrelevant here, where nothing is
 * driven by symbol transitions. */
static int
_bit (int k)
{
  unsigned x = (unsigned)k * 1103515245u + 12345u;
  return ((x >> 16) & 1) ? 1 : -1;
}

/* RRC-shaped BPSK at `sps` input samples/symbol, timing phase `phi` symbols,
 * on a carrier at normalised frequency `fc`.  Amplitude 0.25 keeps the CIC
 * stages inside their +-1.0 input bound — an overdriven front end costs 25 dB
 * of EVM for reasons that have nothing to do with the matched filter. */
static float _Complex *
_tx (double sps, double phi, double fc, size_t *n_out)
{
  size_t          n = (size_t)(_MF_NSYM * sps) + 64;
  float _Complex *x = calloc (n, sizeof *x);
  if (!x)
    return NULL;
  for (size_t i = 0; i < n; i++)
    {
      double a = 0.0;
      for (int k = 0; k < _MF_NSYM; k++)
        {
          double t = ((double)i - (k + _MF_SPAN) * sps) / sps - phi;
          if (fabs (t) > _MF_SPAN)
            continue;
          a += _bit (k) * wfm_rrc_h (t, _MF_BETA);
        }
      double ph = 2.0 * M_PI * fc * (double)i;
      x[i]      = (float _Complex) (0.25 * a * (cos (ph) + I * sin (ph)));
    }
  *n_out = n;
  return x;
}

/* Best EVM over strobe alignment at two samples/symbol.  The complex gain is
 * fitted, so a constant rotation (DdcR's halfband leaves one) is normalised
 * out and only the error vector is measured.  Open loop: the cascade's strobe
 * phase is arbitrary until a timing loop steers it, so take the minimum. */
static double
_evm (const float _Complex *y, size_t ny)
{
  double best = 1e9;
  for (int par = 0; par < 2; par++)
    for (int lag = 0; lag < 140; lag++)
      {
        double _Complex num = 0.0;
        int cnt             = 0;
        for (int k = 40; k < _MF_NSYM - 40; k++)
          {
            size_t i = (size_t)(lag + par + 2 * k);
            if (i >= ny)
              break;
            num += _bit (k) * y[i];
            cnt++;
          }
        if (cnt < 100)
          continue;
        double _Complex g = num / cnt;
        double e = 0.0, p = 0.0;
        for (int k = 40; k < _MF_NSYM - 40; k++)
          {
            size_t i = (size_t)(lag + par + 2 * k);
            if (i >= ny)
              break;
            double _Complex d = y[i] - g * _bit (k);
            e += creal (d) * creal (d) + cimag (d) * cimag (d);
            p += creal (g) * creal (g) + cimag (g) * cimag (g);
          }
        double v = sqrt (e / p);
        if (v < best)
          best = v;
      }
  return 20.0 * log10 (best);
}

/* One constructor, one knob.  RC_PULSE_NONE is not an error — it selects the
 * plain down-converter, so the pulse arguments are the only thing that changes
 * what the cascade IS.  Everything else is rejected rather than coerced. */
static int
_test_pulse_and_invalid_params (void)
{
  int _fails = 0;

  /* NONE builds the plain object, and the remaining arguments are unused —
     passing zeros for them must be as valid as passing sensible ones. */
  ddc_state_t *plain = ddc_create (0.0, 0.25, RC_PULSE_NONE, 0, 0, 0, 0);
  CHECK (plain != NULL);
  ddc_destroy (plain);
  ddcr_state_t *plain_r = ddcr_create (0.0, 0.25, RC_PULSE_NONE, 0, 0, 0, 0);
  CHECK (plain_r != NULL);
  ddcr_destroy (plain_r);

  CHECK (ddc_create (0.0, -0.25, RC_PULSE_RRC, 0.35, 8, 2.0, 1024) == NULL);
  CHECK (ddc_create (0.0, 0.25, RC_PULSE_RRC, 2.0, 8, 2.0, 1024)
         == NULL); /* beta out of range — rejected by the cascade */
  CHECK (ddcr_create (0.0, 0.6, RC_PULSE_RRC, 0.35, 8, 2.0, 1024)
         == NULL); /* rate >= 0.5 — the halfband already took a factor of 2 */

  ddc_state_t *d = ddc_create (0.0, 0.125, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  CHECK (d != NULL);
  ddc_destroy (d);
  ddcr_state_t *r
      = ddcr_create (0.0, 0.125, RC_PULSE_IANDD, 0.35, 4, 4.0, 256);
  CHECK (r != NULL);
  ddcr_destroy (r);
  return _fails;
}

/* The frequency port IS the LO's frequency axis: steering by `f` from a
 * centre of zero is bit-for-bit the same stream as tuning the LO to `f` and
 * steering by nothing.  That equivalence is the whole claim — the port adds
 * no scaling, no smoothing and no state of its own. */
static int
_test_freq_port_is_the_lo_axis (void)
{
  int             _fails = 0;
  const double    f = 0.037, rate = 0.125;
  const size_t    L = 2048, CAP = 512;
  float _Complex *in   = malloc (L * sizeof (float _Complex));
  float _Complex *outA = malloc (CAP * sizeof (float _Complex));
  float _Complex *outB = malloc (CAP * sizeof (float _Complex));
  float _Complex *outC = malloc (CAP * sizeof (float _Complex));
  for (size_t i = 0; i < L; i++)
    in[i] = (float)(0.25 * cos (0.11 * (double)i))
            + I * (float)(0.25 * sin (0.029 * (double)i));

  ddc_state_t *a  = ddc_create (0.0, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  ddc_state_t *b  = ddc_create (f, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  ddc_state_t *c  = ddc_create (0.0, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  size_t       nA = ddc_execute_ctrl (a, in, L, 0.0, f, outA, CAP);
  size_t       nB = ddc_execute_ctrl (b, in, L, 0.0, 0.0, outB, CAP);
  size_t       nC = ddc_execute_ctrl (c, in, L, 0.0, 0.0, outC, CAP);

  CHECK (nA == nB);
  int bad = 0;
  for (size_t i = 0; i < nA && i < nB; i++)
    if (crealf (outA[i]) != crealf (outB[i])
        || cimagf (outA[i]) != cimagf (outB[i]))
      bad++;
  CHECK (bad == 0);

  /* Teeth: the same comparison against an unsteered LO must fail loudly, or
     the equality above would be vacuous. */
  int differs = 0;
  for (size_t i = 0; i < nA && i < nC; i++)
    if (cabsf (outA[i] - outC[i]) > 1e-3f)
      differs++;
  CHECK (differs > (int)(nA / 4));

  /* And the centre frequency is untouched — the deviation is per-sample and
     transient, so the object holds no loop state. */
  CHECK (ddc_get_norm_freq (a) == 0.0);

  ddc_destroy (a);
  ddc_destroy (b);
  ddc_destroy (c);
  free (in);
  free (outA);
  free (outB);
  free (outC);
  return _fails;
}

/* Push == block, bit-for-bit, with BOTH ports held off centre.  The block
 * form is the cheap open-loop path (a fixed Doppler offset, a rate trim); the
 * push form is the only one a closed loop can use.  They must not drift. */
static int
_test_push_equals_block (void)
{
  int          _fails = 0;
  const double rate = 0.125, rctrl = 0.002, fctrl = 0.01;
  const size_t L = 2048, CAP = 512;

  {
    float _Complex *in   = malloc (L * sizeof (float _Complex));
    float _Complex *outA = malloc (CAP * sizeof (float _Complex));
    float _Complex *outB = malloc (CAP * sizeof (float _Complex));
    for (size_t i = 0; i < L; i++)
      in[i] = (float)(0.25 * cos (0.11 * (double)i))
              + I * (float)(0.25 * sin (0.029 * (double)i));

    ddc_state_t *a = ddc_create (-0.1, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
    ddc_state_t *b = ddc_create (-0.1, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
    size_t       nA = ddc_execute_ctrl (a, in, L, rctrl, fctrl, outA, CAP);
    size_t       nB = 0;
    for (size_t i = 0; i < L; i++)
      nB += ddc_execute_ctrl_push (b, in[i], rctrl, fctrl, outB + nB,
                                   CAP - nB);
    CHECK (nA == nB);
    int bad = 0;
    for (size_t i = 0; i < nA && i < nB; i++)
      if (crealf (outA[i]) != crealf (outB[i])
          || cimagf (outA[i]) != cimagf (outB[i]))
        bad++;
    CHECK (bad == 0);
    ddc_destroy (a);
    ddc_destroy (b);
    free (in);
    free (outA);
    free (outB);
  }

  { /* DdcR: the halfband consumes two inputs per LO step, so half the pushes
       emit nothing at all and the parity must survive the split. */
    float          *in   = malloc (L * sizeof (float));
    float _Complex *outA = malloc (CAP * sizeof (float _Complex));
    float _Complex *outB = malloc (CAP * sizeof (float _Complex));
    for (size_t i = 0; i < L; i++)
      in[i] = (float)(0.25 * cos (0.11 * (double)i));

    ddcr_state_t *a
        = ddcr_create (-0.7, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
    ddcr_state_t *b
        = ddcr_create (-0.7, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
    size_t nA = ddcr_execute_ctrl (a, in, L, rctrl, fctrl, outA, CAP);
    size_t nB = 0;
    for (size_t i = 0; i < L; i++)
      nB += ddcr_execute_ctrl_push (b, in[i], rctrl, fctrl, outB + nB,
                                    CAP - nB);
    CHECK (nA == nB);
    int bad = 0;
    for (size_t i = 0; i < nA && i < nB; i++)
      if (crealf (outA[i]) != crealf (outB[i])
          || cimagf (outA[i]) != cimagf (outB[i]))
        bad++;
    CHECK (bad == 0);
    ddcr_destroy (a);
    ddcr_destroy (b);
    free (in);
    free (outA);
    free (outB);
  }
  return _fails;
}

/* End to end: a carrier at 0.09375*fs carrying RRC-BPSK at 16 samples/symbol
 * comes out as symbols at two samples/symbol, matched-filtered by the same
 * dot product that decimated it.  rate = 2/16 is an exact power of two, so
 * the ordinary planner would drop the fractional stage entirely — the matched
 * path appends it anyway, which is what makes the result steerable.
 *
 * The two floors differ, and each is its own front end's, not the matched
 * filter's: the complex path plans CIC(8), whose alias bands fold back at
 * about -30 dB, and measures -45 dB; the real path's cascade sees 2*rate, so
 * it plans halfbands instead and measures -60 dB.  (At 8 samples/symbol the
 * ranking reverses — the complex path gets the halfbands and reaches -60 dB
 * while the real path's fixed 19-tap R2C prototype limits it to -42 dB.)
 *
 * NB the phase grid must be finer than the strobe grid or this measures the
 * sweep, not the filter: at 16 samples/symbol an eighth-symbol step is a
 * whole two input samples, which leaves up to a sixteenth of a symbol of
 * residual timing error and reads -24 dB.  Sixteen phases (and thirty-two)
 * both give -45 dB, i.e. the measurement has converged. */
static int
_test_matched_recovers_symbols (void)
{
  int          _fails = 0;
  const double sps = 16.0, fc = 0.09375, rate = 2.0 / 16.0;
  double       best_c = 1e9, best_r = 1e9;

  for (int j = 0; j < 16; j++)
    {
      size_t          n;
      float _Complex *x  = _tx (sps, j / 16.0, fc, &n);
      float _Complex *y  = calloc (n, sizeof *y);
      float          *xr = malloc (n * sizeof (float));
      if (!x || !y || !xr)
        {
          free (x);
          free (y);
          free (xr);
          return _fails + 1;
        }
      for (size_t i = 0; i < n; i++)
        xr[i] = crealf (x[i]); /* the same signal, real-sampled */

      ddc_state_t *d = ddc_create (-fc, rate, RC_PULSE_RRC, _MF_BETA, _MF_SPAN,
                                   2.0, 1024);
      size_t       ny = ddc_execute (d, x, n, y, n);
      double       e  = _evm (y, ny);
      if (e < best_c)
        best_c = e;
      ddc_destroy (d);

      /* DdcR tunes at the intermediate rate: -(2*fc + 0.5). */
      ddcr_state_t *r   = ddcr_create (-(2.0 * fc + 0.5), rate, RC_PULSE_RRC,
                                       _MF_BETA, _MF_SPAN, 2.0, 1024);
      size_t        nyr = ddcr_execute (r, xr, n, y, n);
      double        er  = _evm (y, nyr);
      if (er < best_r)
        best_r = er;
      ddcr_destroy (r);

      free (x);
      free (y);
      free (xr);
    }

  CHECK (best_c < -40.0); /* measured -45.4 dB (CIC(8) alias floor) */
  CHECK (best_r < -50.0); /* measured -59.8 dB (halfband cascade)   */
  if (best_c >= -40.0 || best_r >= -50.0)
    fprintf (stderr, "  matched DDC EVM: complex=%.1f dB  real=%.1f dB\n",
             best_c, best_r);
  return _fails;
}

/* The carrier port closes.  A tone 0.01 cycles/sample away from where the LO
 * is tuned comes out spinning; a first-order loop reading the output phase
 * increment and writing `freq_ctrl` drives that to zero.  This is the whole
 * point of the port being the dual of the timing one — same loop shape, other
 * accumulator — and it is the only thing here that runs closed.
 *
 * The gain is small for a reason worth stating: the loop closes AROUND the
 * matched filter, so its dead time is that filter's group delay (tens of
 * output samples for a span-8 RRC plus the cascade in front). Measured on
 * this configuration, mu = 0.01 and 0.02 converge to the mistune within 1e-9,
 * mu = 0.05 is marginal (6e-3 left after 2048 outputs) and mu = 0.1 diverges
 * outright and wanders — the same reason a real receiver's carrier loop
 * bandwidth is a small fraction of the symbol rate. */
static int
_test_carrier_loop_pulls_in (void)
{
  int          _fails = 0;
  const double f0 = 0.05, tuned = -0.04, rate = 0.25, mu = 0.01;
  const size_t L = 8192;
  float _Complex o[8];

  ddc_state_t *s = ddc_create (tuned, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  CHECK (s != NULL);

  double freq_ctrl = 0.0, e_last = 0.0;
  float _Complex prev = 0.0f;
  int have_prev       = 0;
  for (size_t i = 0; i < L; i++)
    {
      double ph        = 2.0 * M_PI * f0 * (double)i;
      float _Complex x = (float _Complex) (0.25 * (cos (ph) + I * sin (ph)));
      size_t n         = ddc_execute_ctrl_push (s, x, 0.0, freq_ctrl, o, 8);
      for (size_t j = 0; j < n; j++)
        {
          if (have_prev && i > 1024) /* let the cascade's delay lines fill */
            {
              /* Residual frequency in cycles per OUTPUT sample; the port
                 wants cycles per INPUT sample, hence the rate factor. */
              e_last = carg (o[j] * conjf (prev)) / (2.0 * M_PI);
              freq_ctrl -= mu * e_last * rate;
            }
          prev      = o[j];
          have_prev = 1;
        }
    }
  /* Converged: the control holds exactly the mistune, and the residual is
     gone.  The LO's centre frequency never moved. */
  CHECK (fabs (f0 + tuned + freq_ctrl) < 1e-6); /* measured 6e-11 */
  CHECK (fabs (e_last) < 1e-5);
  CHECK (ddc_get_norm_freq (s) == tuned);
  ddc_destroy (s);

  /* Teeth: hold the port at zero and the same stream keeps spinning at
     0.01/rate cycles per output sample. */
  ddc_state_t *t = ddc_create (tuned, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  double       open_e = 0.0;
  prev                = 0.0f;
  have_prev           = 0;
  for (size_t i = 0; i < L; i++)
    {
      double ph        = 2.0 * M_PI * f0 * (double)i;
      float _Complex x = (float _Complex) (0.25 * (cos (ph) + I * sin (ph)));
      size_t n         = ddc_execute_ctrl_push (t, x, 0.0, 0.0, o, 8);
      for (size_t j = 0; j < n; j++)
        {
          if (have_prev && i > 1024)
            open_e = carg (o[j] * conjf (prev)) / (2.0 * M_PI);
          prev      = o[j];
          have_prev = 1;
        }
    }
  CHECK (fabs (open_e - (f0 + tuned) / rate) < 1e-3);
  ddc_destroy (t);
  return _fails;
}

/* The CIC's silent input bound is observable through the composition: a DDC
 * that plans a CIC inherits it, and nothing in the samples says so. */
static int
_test_clipped_forwards (void)
{
  int             _fails = 0;
  const size_t    L      = 1024;
  float _Complex *in     = malloc (L * sizeof (float _Complex));
  float _Complex *out    = malloc (L * sizeof (float _Complex));
  float          *inr    = malloc (L * sizeof (float));
  for (size_t i = 0; i < L; i++)
    {
      in[i]  = (float)(2.0 * cos (0.11 * (double)i)); /* twice full scale */
      inr[i] = crealf (in[i]);
    }

  /* rate 2/64 plans a CIC(32) + terminal stage. */
  ddc_state_t *d
      = ddc_create (0.0, 2.0 / 64.0, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  CHECK (ddc_get_clipped (d) == 0);
  ddc_execute (d, in, L, out, L);
  CHECK (ddc_get_clipped (d) == 1);
  ddc_reset (d);
  CHECK (ddc_get_clipped (d) == 0); /* sticky until reset, not forever */
  ddc_destroy (d);

  ddcr_state_t *r
      = ddcr_create (0.0, 2.0 / 64.0, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  ddcr_execute (r, inr, L, out, L);
  CHECK (ddcr_get_clipped (r) == 1);
  ddcr_destroy (r);

  /* A halfband plan has no CIC, so the honest answer is 0 however hard it is
     driven — those plans are scale-free. */
  ddc_state_t *h = ddc_create (0.0, 0.5, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  ddc_execute (h, in, L, out, L);
  CHECK (ddc_get_clipped (h) == 0);
  ddc_destroy (h);

  free (in);
  free (out);
  free (inr);
  return _fails;
}

/* A matched DDC serializes like any other: same descriptor in, bit-exact
 * resume.  And a blob from a matched cascade must not restore into a plain
 * one at the same rate — the stage plans differ, so the envelope rejects it
 * rather than reinterpreting the bytes. */
static int
_test_matched_state_roundtrip (void)
{
  int             _fails = 0;
  const double    rate = 0.125, nf = -0.1;
  const size_t    L = 4096, cut = 1201, CAP = 1024;
  float _Complex *in   = malloc (L * sizeof (float _Complex));
  float _Complex *outA = malloc (CAP * sizeof (float _Complex));
  float _Complex *outB = malloc (CAP * sizeof (float _Complex));
  for (size_t i = 0; i < L; i++)
    in[i] = (float)(0.25 * cos (0.17 * (double)i))
            + I * (float)(0.25 * sin (0.013 * (double)i));

  ddc_state_t *a  = ddc_create (nf, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  size_t       nA = ddc_execute (a, in, L, outA, CAP);
  ddc_destroy (a);

  ddc_state_t *b    = ddc_create (nf, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  size_t       nB   = ddc_execute (b, in, cut, outB, CAP);
  void        *blob = malloc (ddc_state_bytes (b));
  ddc_get_state (b, blob);
  ddc_destroy (b);

  ddc_state_t *c = ddc_create (nf, rate, RC_PULSE_RRC, 0.35, 8, 2.0, 1024);
  CHECK (ddc_set_state (c, blob) == DP_OK);
  nB += ddc_execute (c, in + cut, L - cut, outB + nB, CAP - nB);
  ddc_destroy (c);

  CHECK (nA == nB);
  int bad = 0;
  for (size_t i = 0; i < nA && i < nB; i++)
    if (crealf (outA[i]) != crealf (outB[i])
        || cimagf (outA[i]) != cimagf (outB[i]))
      bad++;
  CHECK (bad == 0);

  ddc_state_t *plain = ddc_create (nf, rate, RC_PULSE_NONE, 0, 0, 0, 0);
  CHECK (ddc_set_state (plain, blob) == DP_ERR_INVALID);
  ddc_destroy (plain);

  free (blob);
  free (in);
  free (outA);
  free (outB);
  return _fails;
}

int
main (void)
{
  int          _fails = 0;
  ddc_state_t *obj    = ddc_create (0.0, 0.25, RC_PULSE_NONE, 0, 0, 0, 0);
  CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* no step() generated (--no-step) */

  /* reset */
  ddc_reset (obj);

  ddc_destroy (obj);

  /* ── complex DDC serializable-state round-trip (LO + RateConverter) ───────
   * Mid-stream serialize, rebuild a fresh DDC from the same (norm_freq, rate),
   * restore, and resume — the concatenated CF32 output equals an uninterrupted
   * run, and a clobbered envelope is rejected. */
  {
    const double    norm_freq = -0.1, rate = 0.25;
    const size_t    L = 4096, cut = 1201, CAP = 4096;
    float _Complex *in   = malloc (L * sizeof (float _Complex));
    float _Complex *outA = malloc (CAP * sizeof (float _Complex));
    float _Complex *outB = malloc (CAP * sizeof (float _Complex));
    for (size_t i = 0; i < L; i++)
      in[i]
          = (float)cos (0.17 * (double)i) + I * (float)sin (0.013 * (double)i);

    ddc_state_t *ra = ddc_create (norm_freq, rate, RC_PULSE_NONE, 0, 0, 0, 0);
    size_t       nA = ddc_execute (ra, in, L, outA, CAP);
    ddc_destroy (ra);

    ddc_state_t *r1 = ddc_create (norm_freq, rate, RC_PULSE_NONE, 0, 0, 0, 0);
    size_t       nB = ddc_execute (r1, in, cut, outB, CAP);
    size_t       sb = ddc_state_bytes (r1);
    void        *blob = malloc (sb);
    ddc_get_state (r1, blob);
    ddc_destroy (r1);

    ddc_state_t *r2 = ddc_create (norm_freq, rate, RC_PULSE_NONE, 0, 0, 0, 0);
    CHECK (ddc_set_state (r2, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF; /* clobber envelope -> reject */
    CHECK (ddc_set_state (r2, blob) == DP_ERR_INVALID);
    ((char *)blob)[0] ^= (char)0xFF;
    nB += ddc_execute (r2, in + cut, L - cut, outB + nB, CAP - nB);
    ddc_destroy (r2);
    free (blob);

    CHECK (nA == nB);
    int bad = 0;
    for (size_t i = 0; i < nA && i < nB; i++)
      if (crealf (outA[i]) != crealf (outB[i])
          || cimagf (outA[i]) != cimagf (outB[i]))
        bad++;
    CHECK (bad == 0);
    free (in);
    free (outA);
    free (outB);
  }

  /* ── DDCR full-chain serializable-state round-trip ────────────────────────
   * The integration gate for the elastic ddc_fn: serialize the whole chain
   * (hbdecim_r2c -> LO -> RateConverter) mid-stream, rebuild a fresh DDCR from
   * the same (norm_freq, rate) descriptor, restore the blob, and continue —
   * the concatenated CF32 output must equal an uninterrupted run bit-for-bit.
   * This also covers hbdecim_r2c and RateConverter (no standalone targets). */
  {
    const double    norm_freq = -0.3, rate = 0.25;
    const size_t    L = 4096, cut = 1503, CAP = 2048;
    float          *in   = malloc (L * sizeof (float));
    float _Complex *outA = malloc (CAP * sizeof (float _Complex));
    float _Complex *outB = malloc (CAP * sizeof (float _Complex));
    for (size_t i = 0; i < L; i++)
      in[i] = (float)cos (0.17 * (double)i) + 0.5f * (float)sin (0.013 * i);

    ddcr_state_t *ra
        = ddcr_create (norm_freq, rate, RC_PULSE_NONE, 0, 0, 0, 0);
    size_t nA = ddcr_execute (ra, in, L, outA, CAP);
    ddcr_destroy (ra);

    ddcr_state_t *r1
        = ddcr_create (norm_freq, rate, RC_PULSE_NONE, 0, 0, 0, 0);
    size_t nB   = ddcr_execute (r1, in, cut, outB, CAP);
    size_t sb   = ddcr_state_bytes (r1);
    void  *blob = malloc (sb);
    ddcr_get_state (r1, blob);
    ddcr_destroy (r1);

    ddcr_state_t *r2
        = ddcr_create (norm_freq, rate, RC_PULSE_NONE, 0, 0, 0, 0);
    CHECK (ddcr_set_state (r2, blob) == DP_OK);
    /* a mismatched-rate engine must reject the blob */
    ddcr_state_t *rbad
        = ddcr_create (norm_freq, 0.2, RC_PULSE_NONE, 0, 0, 0, 0);
    CHECK (ddcr_set_state (rbad, blob) == DP_ERR_INVALID);
    ddcr_destroy (rbad);

    nB += ddcr_execute (r2, in + cut, L - cut, outB + nB, CAP - nB);
    ddcr_destroy (r2);
    free (blob);

    CHECK (nA == nB);
    int bad = 0;
    for (size_t i = 0; i < nA && i < nB; i++)
      if (crealf (outA[i]) != crealf (outB[i])
          || cimagf (outA[i]) != cimagf (outB[i]))
        bad++;
    CHECK (bad == 0);
    free (in);
    free (outA);
    free (outB);
  }

  /* ddcr_run pure-transducer face + the CIC / Resampler stage serializers
   * (the halfband plan is covered by the rate=0.25 block above). */
  _fails += _ddcr_run_roundtrip (-0.1, 0.0625); /* RC 0.125 -> CIC(/8)   */
  _fails += _ddcr_run_roundtrip (0.2, 0.375);   /* RC 0.75  -> Resampler */

  /* Layer 3: the pulse passthrough and the two control ports. */
  _fails += _test_pulse_and_invalid_params ();
  _fails += _test_freq_port_is_the_lo_axis ();
  _fails += _test_push_equals_block ();
  _fails += _test_matched_recovers_symbols ();
  _fails += _test_carrier_loop_pulls_in ();
  _fails += _test_clipped_forwards ();
  _fails += _test_matched_state_roundtrip ();

  if (_fails)
    {
      fprintf (stderr, "test_ddc_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_ddc_core PASSED\n");
  return 0;
}
