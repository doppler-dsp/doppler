/**
 * @file test_carrier_nda_core.c
 * @brief Unit tests for the non-data-aided M-th-power carrier loop.
 *
 * Tests:
 *   1. Lifecycle / param validation / init==create parity
 *   2. Arm moving-average cadence — one output/sample, boxcar window sum
 *   3. The M-th-power discriminator: phase_error = scaled Im(z^M) (zero with
 *      positive slope at lock; period-2pi/M sawtooth), lock = scaled Re(z^M)
 *   4. Cold-start pull-in on an UNMODULATED carrier (no data)
 *   5. Cold-start pull-in on MODULATED data with NO symbol timing (the
 * headline)
 *   6. Reset reproducibility
 */
#include "carrier_nda/carrier_nda_core.h"
#include "mpsk/mpsk_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define TWOPI 6.283185307179586

static uint32_t
xs (uint32_t *st)
{
  uint32_t x = *st;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *st = x;
  return x;
}
static float
gauss (uint32_t *st)
{
  double r1 = (xs (st) + 1.0) / 4294967297.0;
  double r2 = (xs (st) + 1.0) / 4294967297.0;
  return (float)(sqrt (-2.0 * log (r1)) * cos (TWOPI * r2));
}

/* Run the loop over a built signal; report tracked freq + lock. */
static void
run (carrier_nda_state_t *c, const float complex *rx, size_t n, double *f,
     double *lk)
{
  float complex *o = malloc (n * sizeof (*o));
  carrier_nda_steps (c, rx, n, o, n);
  *f  = carrier_nda_get_norm_freq (c);
  *lk = carrier_nda_get_lock (c);
  free (o);
}

/* First sample index where |norm_freq - f0| falls within 10% of f0, probed
 * every 50 samples — a proxy for closed-loop settling time. Returns N if the
 * loop never settles within the run. */
static size_t
settle_idx (int n, double bn, double f0, const float complex *rx, size_t N)
{
  carrier_nda_state_t *c = carrier_nda_create (bn, 0.707, 0.0, 8, n, 4);
  float complex        o[50];
  size_t               idx = N;
  for (size_t i = 0; i + 50 <= N; i += 50)
    {
      carrier_nda_steps (c, rx + i, 50, o, 50);
      if (fabs (carrier_nda_get_norm_freq (c) - f0) < 0.1 * f0)
        {
          idx = i;
          break;
        }
    }
  carrier_nda_destroy (c);
  return idx;
}

int
main (void)
{
  int _fails = 0;

  /* ---------------------------------------------------------------- *
   * 1. Lifecycle, param validation, init==create parity              *
   * ---------------------------------------------------------------- */
  {
    carrier_nda_state_t *c = carrier_nda_create (0.01, 0.707, 0.01, 8, 4, 4);
    CHECK (c != NULL);
    if (!c)
      return 1;
    CHECK (c->lf.kp > 0.0 && c->lf.ki > 0.0);
    CHECK (fabs (carrier_nda_get_norm_freq (c) - 0.01) < 1e-12);
    CHECK (carrier_nda_get_m (c) == 4);
    CHECK (carrier_nda_get_n (c) == 4);
    CHECK (carrier_nda_get_sps (c) == 8);
    CHECK (c->arm_len == 2); /* sps/n = 8/4 */

    carrier_nda_state_t v;
    carrier_nda_init (&v, 0.01, 0.707, 0.01, 8, 4, 4);
    CHECK (v.lf.kp == c->lf.kp && v.lf.ki == c->lf.ki);
    CHECK (v.nco.phase_inc == c->nco.phase_inc);
    CHECK (v.arm_len == c->arm_len && v.lock_scale == c->lock_scale);
    carrier_nda_destroy (c);

    /* M in {2,4,8}; sps % n == 0; n > 0; sps > 0 */
    CHECK (carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 2) != NULL);
    CHECK (carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 8) != NULL);
    CHECK (carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 3) == NULL);
    CHECK (carrier_nda_create (0.01, 0.707, 0.0, 8, 3, 4) == NULL); /* 8%3 */
    CHECK (carrier_nda_create (0.01, 0.707, 0.0, 8, 0, 4) == NULL);
    CHECK (carrier_nda_create (0.01, 0.707, 0.0, 0, 4, 4) == NULL);
  }

  /* ---------------------------------------------------------------- *
   * 2. Arm moving-average: one output per input sample (no rate       *
   *    change), and the running window holds a boxcar sum of the last  *
   *    arm_len samples.                                                *
   * ---------------------------------------------------------------- */
  {
    int                  sps = 8, n = 4; /* arm_len = sps/n = 2 */
    carrier_nda_state_t *c = carrier_nda_create (0.01, 0.707, 0.0, sps, n, 4);
    int                  outs = 0;
    for (int i = 0; i < sps; i++) /* ramp 1..8 through the boxcar */
      {
        double pe, lk;
        if (carrier_nda_arm_step (c, (float)(i + 1) + 0.0f * I, &pe, &lk))
          outs++;
      }
    CHECK (outs == sps); /* one output per input sample (no decimation) */
    /* boxcar window = last arm_len=2 samples: 7 + 8 = 15 (running sum) */
    CHECK (fabs (crealf (c->arm.acc) - 15.0f) < 1e-4);
    CHECK (fabs (cimagf (c->arm.acc)) < 1e-6);
    carrier_nda_destroy (c);

    /* arm_len > BOXCAR_MAX_LEN is rejected (fixed in-struct ring) */
    CHECK (carrier_nda_create (0.01, 0.707, 0.0, 128, 1, 4) == NULL);
    carrier_nda_state_t *cmax
        = carrier_nda_create (0.01, 0.707, 0.0, BOXCAR_MAX_LEN, 1, 4);
    CHECK (cmax != NULL); /* arm_len == BOXCAR_MAX_LEN is allowed */
    carrier_nda_destroy (cmax);
  }

  /* ---------------------------------------------------------------- *
   * 3. The M-th-power discriminator characteristic                   *
   * ---------------------------------------------------------------- */
  {
    for (int mi = 0; mi < 3; mi++)
      {
        int    m     = (mi == 0) ? 2 : (mi == 1) ? 4 : 8;
        double scale = (m == 2) ? 1.0 : (m == 4) ? 0.619 : 0.412;
        double seg   = TWOPI / m;
        double pe0, lk0;
        carrier_nda_disc (1.0f + 0.0f * I, m, scale, &pe0, &lk0);
        CHECK (fabs (pe0) < 1e-9); /* e(0) = 0          */
        CHECK (lk0 > 0.0);         /* lock peaks at 0   */
        /* constant-gain property: phase_error slope at 0 is ~2 for all M */
        double h = 1e-3 / m, peh, pemh, lk;
        carrier_nda_disc ((float complex)cexp (I * h), m, scale, &peh, &lk);
        carrier_nda_disc ((float complex)cexp (-I * h), m, scale, &pemh, &lk);
        double slope = (peh - pemh) / (2.0 * h);
        CHECK (fabs (slope - 2.0) < 2e-2);
        /* sawtooth period 2pi/M: e(phi) == e(phi + 2pi/M) */
        double pa, pb;
        carrier_nda_disc ((float complex)cexp (I * 0.05), m, scale, &pa, &lk);
        carrier_nda_disc ((float complex)cexp (I * (0.05 + seg)), m, scale,
                          &pb, &lk);
        CHECK (fabs (pa - pb) < 1e-6);
      }
  }

  /* ---------------------------------------------------------------- *
   * 4. Cold-start pull-in on an UNMODULATED carrier (no data)        *
   * ---------------------------------------------------------------- */
  {
    size_t         N    = 40000;
    float complex *rx   = malloc (N * sizeof (*rx));
    double         f0   = 0.001;
    int            ms[] = { 2, 4, 8 };
    for (int mi = 0; mi < 3; mi++)
      {
        uint32_t ns = 5u;
        for (size_t k = 0; k < N; k++)
          rx[k] = (float complex)cexp (I * TWOPI * f0 * (double)k)
                  + 0.05f * gauss (&ns) + 0.05f * gauss (&ns) * I;
        carrier_nda_state_t *c
            = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, ms[mi]);
        double f, lk;
        run (c, rx, N, &f, &lk);
        CHECK (fabs (f - f0) < 5e-4);     /* acquired the bare carrier */
        CHECK (lk > 0.3 * c->lock_scale); /* locked (scaled metric)    */
        carrier_nda_destroy (c);
      }
    free (rx);
  }

  /* ---------------------------------------------------------------- *
   * 5. Cold-start on MODULATED data with NO symbol timing            *
   * ---------------------------------------------------------------- */
  {
    int            sps  = 8;
    size_t         nsym = 6000, N = nsym * (size_t)sps;
    float complex *rx   = malloc (N * sizeof (*rx));
    double         f0   = 0.001;
    int            ms[] = { 2, 4, 8 };
    for (int mi = 0; mi < 3; mi++)
      {
        int      m  = ms[mi];
        uint32_t ds = 99u, ns = 7u;
        for (size_t s = 0; s < nsym; s++)
          {
            float complex a
                = mpsk_constellation ((int)(xs (&ds) % (uint32_t)m), m);
            for (int i = 0; i < sps; i++)
              {
                size_t k = s * (size_t)sps + (size_t)i;
                rx[k]    = a * (float complex)cexp (I * TWOPI * f0 * (double)k)
                           + 0.1f * gauss (&ns) + 0.1f * gauss (&ns) * I;
              }
          }
        carrier_nda_state_t *c
            = carrier_nda_create (0.01, 0.707, 0.0, sps, 4, m);
        double f, lk;
        run (c, rx, N, &f, &lk);
        CHECK (fabs (f - f0) < 5e-4); /* locked despite NO timing  */
        CHECK (lk > 0.3 * c->lock_scale);
        carrier_nda_destroy (c);
      }
    free (rx);
  }

  /* ---------------------------------------------------------------- *
   * 6. Reset reproducibility                                         *
   * ---------------------------------------------------------------- */
  {
    size_t         N  = 8000;
    float complex *rx = malloc (N * sizeof (*rx));
    uint32_t       ns = 3u;
    for (size_t k = 0; k < N; k++)
      rx[k] = (float complex)cexp (I * TWOPI * 0.0012 * (double)k)
              + 0.05f * gauss (&ns) + 0.05f * gauss (&ns) * I;
    carrier_nda_state_t *c = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 4);
    double               f1, lk1, f2, lk2;
    run (c, rx, N, &f1, &lk1);
    carrier_nda_reset (c);
    run (c, rx, N, &f2, &lk2);
    CHECK (f1 == f2 && lk1 == lk2);
    carrier_nda_destroy (c);
    free (rx);
  }

  /* serializable state — whole-struct snapshot resumes the loop bit-for-bit.
   */
  {
    enum
    {
      L   = 600,
      CUT = 251,
      CAP = 600
    };
    float _Complex *rx   = malloc (L * sizeof (float _Complex));
    float _Complex *outA = malloc (CAP * sizeof (float _Complex));
    float _Complex *outB = malloc (CAP * sizeof (float _Complex));
    for (size_t i = 0; i < L; i++)
      rx[i] = cosf (0.02f * (float)i) + I * sinf (0.02f * (float)i);

    carrier_nda_state_t *a  = carrier_nda_create (0.01, 0.707, 0.0, 4, 2, 4);
    size_t               nA = carrier_nda_steps (a, rx, L, outA, CAP);
    carrier_nda_destroy (a);

    carrier_nda_state_t *r1   = carrier_nda_create (0.01, 0.707, 0.0, 4, 2, 4);
    size_t               nB   = carrier_nda_steps (r1, rx, CUT, outB, CAP);
    size_t               sb   = carrier_nda_state_bytes (r1);
    void                *blob = malloc (sb);
    carrier_nda_get_state (r1, blob);
    carrier_nda_destroy (r1);

    carrier_nda_state_t *r2 = carrier_nda_create (0.01, 0.707, 0.0, 4, 2, 4);
    CHECK (carrier_nda_set_state (r2, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF;
    CHECK (carrier_nda_set_state (r2, blob) == DP_ERR_INVALID);
    ((char *)blob)[0] ^= (char)0xFF;
    nB += carrier_nda_steps (r2, rx + CUT, L - CUT, outB + nB, CAP - nB);
    carrier_nda_destroy (r2);
    free (blob);

    CHECK (nA == nB);
    for (size_t i = 0; i < nA && i < nB; i++)
      CHECK (crealf (outA[i]) == crealf (outB[i])
             && cimagf (outA[i]) == cimagf (outB[i]));
    free (rx);
    free (outA);
    free (outB);
  }

  /* ---------------------------------------------------------------- *
   * 7. Bn is n-invariant: at a fixed bn the closed-loop settling time *
   *    is ~independent of n (arm dumps/symbol). Before the fix bn was  *
   *    applied per arm-dump, so the real-time loop bandwidth — and     *
   *    settling — scaled ~n x. Noiseless carrier → deterministic.      *
   * ---------------------------------------------------------------- */
  {
    size_t         N  = 40000;
    float complex *rx = malloc (N * sizeof (*rx));
    double         f0 = 0.0015;
    for (size_t k = 0; k < N; k++)
      rx[k] = (float complex)cexp (I * TWOPI * f0 * (double)k);
    size_t s1 = settle_idx (1, 0.005, f0, rx, N);
    size_t s2 = settle_idx (2, 0.005, f0, rx, N);
    size_t s4 = settle_idx (4, 0.005, f0, rx, N);
    CHECK (s1 < N && s2 < N && s4 < N); /* all settle */
    double smin
        = (double)(s1 < s2 ? (s1 < s4 ? s1 : s4) : (s2 < s4 ? s2 : s4));
    double smax
        = (double)(s1 > s2 ? (s1 > s4 ? s1 : s4) : (s2 > s4 ? s2 : s4));
    /* n-invariant: settling within ~2x across n (would be ~4x apart with
     * the old per-dump bn). */
    CHECK ((smax + 50.0) / (smin + 50.0) < 2.5);
    free (rx);
  }

  /* ---------------------------------------------------------------- *
   * 8. set_bn reconfigures BOTH the loop filter and the arm AGC       *
   *    bandwidth (agc.loop_bw locked to CARRIER_NDA_AGC_BW_RATIO*bn). *
   * ---------------------------------------------------------------- */
  {
    carrier_nda_state_t *c = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 4);
    CHECK (fabs (c->agc.loop_bw - CARRIER_NDA_AGC_BW_RATIO * 0.01) < 1e-15);
    double kp0 = c->lf.kp;
    carrier_nda_set_bn (c, 0.02);
    CHECK (carrier_nda_get_bn (c) == 0.02);
    CHECK (fabs (c->agc.loop_bw - CARRIER_NDA_AGC_BW_RATIO * 0.02) < 1e-15);
    CHECK (c->lf.kp != kp0); /* loop filter also reconfigured */
    carrier_nda_destroy (c);
  }

  /* ---------------------------------------------------------------- *
   * 9. Amplitude invariance over the operating range (input at or     *
   *    below the unit AGC reference): the arm AGC makes the loop gain  *
   *    independent of input scale, so 0.01x .. 1x converge to the SAME *
   *    carrier. (Cold input >~10 dB above unit is out of spec — the    *
   *    slow AGC cannot normalize it before the discriminator reacts;   *
   *    the front end is expected to deliver ~unit-scaled samples.)     *
   * ---------------------------------------------------------------- */
  {
    size_t         N  = 40000;
    float complex *rx = malloc (N * sizeof (*rx));
    double         f0 = 0.001;
    uint32_t       ns = 23u;
    for (size_t k = 0; k < N; k++)
      rx[k] = (float complex)cexp (I * TWOPI * f0 * (double)k)
              + 0.05f * gauss (&ns) + 0.05f * gauss (&ns) * I;
    double         scales[] = { 0.01, 0.1, 1.0 };
    double         fs[3];
    float complex *rs = malloc (N * sizeof (*rs));
    for (int si = 0; si < 3; si++)
      {
        for (size_t k = 0; k < N; k++)
          rs[k] = (float)scales[si] * rx[k];
        carrier_nda_state_t *c
            = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 4);
        double f, lk;
        run (c, rs, N, &f, &lk);
        CHECK (fabs (f - f0) < 5e-4);     /* converges at any in-range scale */
        CHECK (lk > 0.3 * c->lock_scale); /* lock metric scale-invariant     */
        fs[si] = f;
        carrier_nda_destroy (c);
      }
    CHECK (fabs (fs[0] - fs[1]) < 1e-4); /* all scales → same carrier */
    CHECK (fabs (fs[2] - fs[1]) < 1e-4);
    free (rs);
    free (rx);
  }

  /* telemetry attach — sample-rate probes plus the embedded arm AGC's
   * forwarded gain probe; attach/detach cascade through the AGC; blobs
   * stay attachment-independent (both attachments zeroed). */
  {
    enum
    {
      N = 1024
    };
    float complex rx[N], out[N];
    dp_tlm_rec_t  recs[8192];
    for (int i = 0; i < N; i++)
      rx[i] = (float complex)cexp (I * TWOPI * 0.005 * (double)i);
    dp_tlm_t            *tlm = dp_tlm_create (1 << 13);
    carrier_nda_state_t *c   = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 4);
    CHECK (tlm != NULL && c != NULL);
    CHECK (carrier_nda_set_telemetry (c, tlm, "car", 1) == DP_OK);
    CHECK (dp_tlm_lookup (tlm, "car.lock") == c->tlm.id_lock);
    CHECK (dp_tlm_lookup (tlm, "car.e") == c->tlm.id_e);
    CHECK (dp_tlm_lookup (tlm, "car.freq") == c->tlm.id_freq);
    CHECK (dp_tlm_lookup (tlm, "car.locked") == c->tlm.id_locked);
    CHECK (dp_tlm_lookup (tlm, "car.agc.gain_db") == c->agc.tlm.id_gain);
    CHECK (c->agc.tlm.ctx == tlm); /* forwarded attach armed the AGC */

    size_t k = carrier_nda_steps (c, rx, N, out, N);
    CHECK (k == N);
    size_t n_rec = dp_tlm_read (tlm, recs, 8192);
    /* four per sample + one AGC record per amortized gain update */
    CHECK (n_rec == 4 * N + N / AGC_DECIM_DEFAULT);
    /* Per-sample emit order is lock, e, freq, locked -- the last record is
     * the lockdet decision; the one before it mirrors the tracked carrier. */
    CHECK (recs[n_rec - 1].probe == (uint16_t)c->tlm.id_locked);
    CHECK (recs[n_rec - 1].value == (float)c->lockdet.locked);
    CHECK (recs[n_rec - 2].value == (float)(c->nco.norm_freq + c->lf.integ));

    /* Blobs zero BOTH attachments (deterministic) and set_state into an
     * attached instance preserves that instance's live attachments. */
    size_t sb = carrier_nda_state_bytes (c);
    void  *b1 = malloc (sb), *b2 = malloc (sb);
    carrier_nda_get_state (c, b1);
    carrier_nda_state_t *d = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 4);
    CHECK (d != NULL);
    CHECK (carrier_nda_set_telemetry (d, tlm, "car2", 4) == DP_OK);
    CHECK (carrier_nda_set_state (d, b1) == DP_OK);
    CHECK (d->tlm.ctx == tlm && d->agc.tlm.ctx == tlm);
    CHECK (d->tlm.id_e == dp_tlm_lookup (tlm, "car2.e"));
    carrier_nda_get_state (d, b2);
    CHECK (memcmp (b1, b2, sb) == 0); /* attachment-independent bytes */
    free (b1);
    free (b2);
    carrier_nda_destroy (d);

    /* Detach cascades to the embedded AGC. */
    CHECK (carrier_nda_set_telemetry (c, NULL, "car", 1) == DP_OK);
    CHECK (c->tlm.ctx == NULL && c->agc.tlm.ctx == NULL);
    (void)carrier_nda_steps (c, rx, N, out, N);
    CHECK (dp_tlm_read (tlm, recs, 8192) == 0);

    /* A full probe table fails the attach whole (AGC included). */
    char pname[DP_TLM_NAME_MAX];
    for (size_t i = 0; dp_tlm_probe_count (tlm) < DP_TLM_MAX_PROBES; i++)
      {
        (void)snprintf (pname, sizeof (pname), "fill%zu", i);
        (void)dp_tlm_probe (tlm, pname, 1);
      }
    CHECK (carrier_nda_set_telemetry (c, tlm, "nope", 1) == DP_ERR_INVALID);
    CHECK (c->tlm.ctx == NULL && c->agc.tlm.ctx == NULL);
    carrier_nda_destroy (c);
    dp_tlm_destroy (tlm);
  }

  if (_fails)
    {
      fprintf (stderr, "test_carrier_nda_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_carrier_nda_core PASSED\n");
  return 0;
}
