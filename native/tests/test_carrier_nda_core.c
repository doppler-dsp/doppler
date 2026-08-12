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
#include "dp_rng_test.h"
#include "dp_test.h"
#include "mpsk/mpsk_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TWOPI 6.283185307179586

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

  /* ---------------------------------------------------------------- *
   * 1. Lifecycle, param validation, init==create parity              *
   * ---------------------------------------------------------------- */
  {
    carrier_nda_state_t *c = carrier_nda_create (0.01, 0.707, 0.01, 8, 4, 4);
    DP_CHECK (c != NULL);
    if (!c)
      return 1;
    DP_CHECK (c->lf.kp > 0.0 && c->lf.ki > 0.0);
    DP_CHECK (fabs (carrier_nda_get_norm_freq (c) - 0.01) < 1e-12);
    DP_CHECK (carrier_nda_get_m (c) == 4);
    DP_CHECK (carrier_nda_get_n (c) == 4);
    DP_CHECK (carrier_nda_get_sps (c) == 8);
    DP_CHECK (c->arm_len == 2); /* sps/n = 8/4 */

    carrier_nda_state_t v;
    carrier_nda_init (&v, 0.01, 0.707, 0.01, 8, 4, 4);
    DP_CHECK (v.lf.kp == c->lf.kp && v.lf.ki == c->lf.ki);
    DP_CHECK (v.nco.phase_inc == c->nco.phase_inc);
    DP_CHECK (v.arm_len == c->arm_len && v.m == c->m);
    carrier_nda_destroy (c);

    /* M in {2,4,8}; sps % n == 0; n > 0; sps > 0 */
    DP_CHECK (carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 2) != NULL);
    DP_CHECK (carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 8) != NULL);
    DP_CHECK (carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 3) == NULL);
    DP_CHECK (carrier_nda_create (0.01, 0.707, 0.0, 8, 3, 4)
              == NULL); /* 8%3 */
    DP_CHECK (carrier_nda_create (0.01, 0.707, 0.0, 8, 0, 4) == NULL);
    DP_CHECK (carrier_nda_create (0.01, 0.707, 0.0, 0, 4, 4) == NULL);
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
    DP_CHECK (outs == sps); /* one output per input sample (no decimation) */
    /* boxcar window = last arm_len=2 samples: 7 + 8 = 15 (running sum) */
    DP_CHECK (fabs (crealf (c->arm.acc) - 15.0f) < 1e-4);
    DP_CHECK (fabs (cimagf (c->arm.acc)) < 1e-6);
    carrier_nda_destroy (c);

    /* arm_len > BOXCAR_MAX_LEN is rejected (fixed in-struct ring) */
    DP_CHECK (carrier_nda_create (0.01, 0.707, 0.0, 128, 1, 4) == NULL);
    carrier_nda_state_t *cmax
        = carrier_nda_create (0.01, 0.707, 0.0, BOXCAR_MAX_LEN, 1, 4);
    DP_CHECK (cmax != NULL); /* arm_len == BOXCAR_MAX_LEN is allowed */
    carrier_nda_destroy (cmax);
  }

  /* ---------------------------------------------------------------- *
   * 3. The M-th-power discriminator characteristic                   *
   * ---------------------------------------------------------------- */
  {
    for (int mi = 0; mi < 3; mi++)
      {
        int    m   = (mi == 0) ? 2 : (mi == 1) ? 4 : 8;
        double seg = TWOPI / m;
        double pe0, lk0;
        carrier_nda_disc (1.0f + 0.0f * I, m, &pe0, &lk0);
        DP_CHECK (fabs (pe0) < 1e-9); /* e(0) = 0          */
        DP_CHECK (lk0 > 0.0);         /* lock peaks at 0   */
        /* constant-gain property: phase_error slope at 0 is ~2 for all M */
        double h = 1e-3 / m, peh, pemh, lk;
        carrier_nda_disc ((float complex)cexp (I * h), m, &peh, &lk);
        carrier_nda_disc ((float complex)cexp (-I * h), m, &pemh, &lk);
        double slope = (peh - pemh) / (2.0 * h);
        DP_CHECK (fabs (slope - 2.0) < 2e-2);
        /* sawtooth period 2pi/M: e(phi) == e(phi + 2pi/M) */
        double pa, pb;
        carrier_nda_disc ((float complex)cexp (I * 0.05), m, &pa, &lk);
        carrier_nda_disc ((float complex)cexp (I * (0.05 + seg)), m, &pb, &lk);
        DP_CHECK (fabs (pa - pb) < 1e-6);
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
          {
            /* Sequenced: two draws in one expression are indeterminately
               sequenced, and gcc takes the imaginary operand first while
               clang takes the real one. gcc's order is pinned. */
            float n_im = 0.05f * (float)dp_gauss (&ns);
            float n_re = 0.05f * (float)dp_gauss (&ns);
            rx[k] = (float complex)cexp (I * TWOPI * f0 * (double)k) + n_re
                    + n_im * I;
          }
        carrier_nda_state_t *c
            = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, ms[mi]);
        double f, lk;
        run (c, rx, N, &f, &lk);
        DP_CHECK (fabs (f - f0) < 5e-4); /* acquired the bare carrier */
        DP_CHECK (lk > 0.3);             /* locked (normalised: ~1)   */
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
                = mpsk_constellation ((int)(dp_xs32 (&ds) % (uint32_t)m), m);
            for (int i = 0; i < sps; i++)
              {
                size_t k    = s * (size_t)sps + (size_t)i;
                float  n_im = 0.1f * (float)dp_gauss (&ns); /* gcc's order */
                float  n_re = 0.1f * (float)dp_gauss (&ns);
                rx[k] = a * (float complex)cexp (I * TWOPI * f0 * (double)k)
                        + n_re + n_im * I;
              }
          }
        carrier_nda_state_t *c
            = carrier_nda_create (0.01, 0.707, 0.0, sps, 4, m);
        double f, lk;
        run (c, rx, N, &f, &lk);
        DP_CHECK (fabs (f - f0) < 5e-4); /* locked despite NO timing  */
        DP_CHECK (lk > 0.3);
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
      {
        float n_im = 0.05f * (float)dp_gauss (&ns); /* gcc's order */
        float n_re = 0.05f * (float)dp_gauss (&ns);
        rx[k] = (float complex)cexp (I * TWOPI * 0.0012 * (double)k) + n_re
                + n_im * I;
      }
    carrier_nda_state_t *c = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 4);
    double               f1, lk1, f2, lk2;
    run (c, rx, N, &f1, &lk1);
    carrier_nda_reset (c);
    run (c, rx, N, &f2, &lk2);
    DP_CHECK (f1 == f2 && lk1 == lk2);
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
    DP_CHECK (carrier_nda_set_state (r2, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF;
    DP_CHECK (carrier_nda_set_state (r2, blob) == DP_ERR_INVALID);
    ((char *)blob)[0] ^= (char)0xFF;
    nB += carrier_nda_steps (r2, rx + CUT, L - CUT, outB + nB, CAP - nB);
    carrier_nda_destroy (r2);
    free (blob);

    DP_CHECK (nA == nB);
    for (size_t i = 0; i < nA && i < nB; i++)
      DP_CHECK (crealf (outA[i]) == crealf (outB[i])
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
    DP_CHECK (s1 < N && s2 < N && s4 < N); /* all settle */
    double smin
        = (double)(s1 < s2 ? (s1 < s4 ? s1 : s4) : (s2 < s4 ? s2 : s4));
    double smax
        = (double)(s1 > s2 ? (s1 > s4 ? s1 : s4) : (s2 > s4 ? s2 : s4));
    /* n-invariant: settling within ~2x across n (would be ~4x apart with
     * the old per-dump bn). */
    DP_CHECK ((smax + 50.0) / (smin + 50.0) < 2.5);
    free (rx);
  }

  /* ---------------------------------------------------------------- *
   * 8. set_bn reconfigures the loop filter.                           *
   * ---------------------------------------------------------------- */
  {
    carrier_nda_state_t *c   = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 4);
    double               kp0 = c->lf.kp;
    carrier_nda_set_bn (c, 0.02);
    DP_CHECK (carrier_nda_get_bn (c) == 0.02);
    DP_CHECK (c->lf.kp != kp0);
    carrier_nda_destroy (c);
  }

  /* ---------------------------------------------------------------- *
   * 9. Amplitude invariance, at ANY scale and at every M.             *
   *                                                                    *
   *    There is no AGC in this loop; carrier_nda_disc divides out its   *
   *    own |z|^M, so input scale must not reach the loop at all. This   *
   *    is a range gate as much as a gain gate: forming |z|^M explicitly *
   *    (which the pre-gh-657 form did, because an arm AGC pinned |z|=1  *
   *    and hid it) dies at BOTH ends in float — M=8 returned exactly 0  *
   *    below |z|=0.032, where the eps guard on |z|^8 trips, and NaN     *
   *    above |z|=1e4, where |z|^8 passes FLT_MAX. The scales below      *
   *    straddle both, so this fails if the divide is ever un-hoisted.   *
   * ---------------------------------------------------------------- */
  {
    /* Discriminator level first: exact, and it localises a failure to the
     * detector rather than to the loop wrapped around it. */
    const int    ms[] = { 2, 4, 8 };
    const double th   = 0.11; /* off lock, so pe and lock are both nonzero */
    for (int mi = 0; mi < 3; mi++)
      {
        double        ref_pe, ref_lk;
        float complex z1 = (float)cos (th) + (float)sin (th) * I;
        carrier_nda_disc (z1, ms[mi], &ref_pe, &ref_lk);
        DP_CHECK (fabs (ref_pe) > 0.1 && fabs (ref_lk) > 0.1);
        const double amps[] = { 1e-5, 1e-3, 3.2e-2, 1.0, 1e3, 1e5, 1e8 };
        for (size_t ai = 0; ai < sizeof amps / sizeof *amps; ai++)
          {
            double        A = amps[ai];
            float complex z
                = (float)(A * cos (th)) + (float)(A * sin (th)) * I;
            double pe, lk;
            carrier_nda_disc (z, ms[mi], &pe, &lk);
            DP_CHECK (fabs (pe - ref_pe) <= 1e-5 * fabs (ref_pe));
            DP_CHECK (fabs (lk - ref_lk) <= 1e-5 * fabs (ref_lk));
          }
        /* Degenerate input yields zero, never a NaN into the loop filter. */
        double pe0, lk0;
        carrier_nda_disc (0.0f + 0.0f * I, ms[mi], &pe0, &lk0);
        DP_CHECK (pe0 == 0.0 && lk0 == 0.0);
      }
  }
  {
    size_t         N  = 40000;
    float complex *rx = malloc (N * sizeof (*rx));
    double         f0 = 0.001;
    uint32_t       ns = 23u;
    for (size_t k = 0; k < N; k++)
      {
        float n_im = 0.05f * (float)dp_gauss (&ns); /* gcc's order */
        float n_re = 0.05f * (float)dp_gauss (&ns);
        rx[k]      = (float complex)cexp (I * TWOPI * f0 * (double)k) + n_re
                     + n_im * I;
      }
    /* 1e-4 .. 1e4 — eight decades, centred on the unit scale the retired
     * AGC used to be the only way to reach. */
    double scales[] = { 1e-4, 1e-2, 1.0, 1e2, 1e4 };
    enum
    {
      NS = sizeof scales / sizeof *scales
    };
    float complex *rs = malloc (N * sizeof (*rs));
    for (int m = 4; m <= 8; m += 4) /* QPSK and the 8PSK worst case */
      {
        double fs[NS];
        for (int si = 0; si < NS; si++)
          {
            for (size_t k = 0; k < N; k++)
              rs[k] = (float)scales[si] * rx[k];
            carrier_nda_state_t *c
                = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, m);
            double f, lk;
            run (c, rs, N, &f, &lk);
            DP_CHECK (fabs (f - f0) < 5e-4); /* converges at any scale */
            DP_CHECK (lk > 0.3);             /* lock metric level-invariant */
            fs[si] = f;
            carrier_nda_destroy (c);
          }
        for (int si = 0; si < NS; si++)
          DP_CHECK (fabs (fs[si] - fs[NS / 2]) < 1e-4); /* same carrier */
      }
    free (rs);
    free (rx);
  }

  /* telemetry attach — four sample-rate probes; blobs stay
   * attachment-independent (the attachment is zeroed in the bytes). */
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
    DP_CHECK (tlm != NULL && c != NULL);
    DP_CHECK (carrier_nda_set_telemetry (c, tlm, "car", 1) == DP_OK);
    DP_CHECK (dp_tlm_probe_id (tlm, "car.lock") == c->tlm.id_lock);
    DP_CHECK (dp_tlm_probe_id (tlm, "car.e") == c->tlm.id_e);
    DP_CHECK (dp_tlm_probe_id (tlm, "car.freq") == c->tlm.id_freq);
    DP_CHECK (dp_tlm_probe_id (tlm, "car.locked") == c->tlm.id_locked);
    /* The retired arm AGC used to forward a fifth probe here (gh-657). */
    DP_CHECK (dp_tlm_probe_count (tlm) == 4);

    size_t k = carrier_nda_steps (c, rx, N, out, N);
    DP_CHECK (k == N);
    size_t n_rec = dp_tlm_read (tlm, 8192, recs, 8192);
    DP_CHECK (n_rec == 4 * N); /* four per sample, nothing else */
    /* Per-sample emit order is lock, e, freq, locked -- the last record is
     * the lockdet decision; the one before it mirrors the tracked carrier. */
    DP_CHECK (recs[n_rec - 1].probe == (uint16_t)c->tlm.id_locked);
    DP_CHECK (recs[n_rec - 1].value == (float)c->lockdet.locked);
    DP_CHECK (recs[n_rec - 2].value
              == (float)(c->nco.norm_freq + c->lf.integ));

    /* Blobs zero the attachment (deterministic) and set_state into an
     * attached instance preserves that instance's live attachment. */
    size_t sb = carrier_nda_state_bytes (c);
    void  *b1 = malloc (sb), *b2 = malloc (sb);
    carrier_nda_get_state (c, b1);
    carrier_nda_state_t *d = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 4);
    DP_CHECK (d != NULL);
    DP_CHECK (carrier_nda_set_telemetry (d, tlm, "car2", 4) == DP_OK);
    DP_CHECK (carrier_nda_set_state (d, b1) == DP_OK);
    DP_CHECK (d->tlm.ctx == tlm);
    DP_CHECK (d->tlm.id_e == dp_tlm_probe_id (tlm, "car2.e"));
    carrier_nda_get_state (d, b2);
    DP_CHECK (memcmp (b1, b2, sb) == 0); /* attachment-independent bytes */
    free (b1);
    free (b2);
    carrier_nda_destroy (d);

    DP_CHECK (carrier_nda_set_telemetry (c, NULL, "car", 1) == DP_OK);
    DP_CHECK (c->tlm.ctx == NULL);
    (void)carrier_nda_steps (c, rx, N, out, N);
    DP_CHECK (dp_tlm_read (tlm, 8192, recs, 8192) == 0);

    /* A full probe table fails the attach whole. */
    char pname[DP_TLM_NAME_MAX];
    for (size_t i = 0; dp_tlm_probe_count (tlm) < DP_TLM_MAX_PROBES; i++)
      {
        (void)snprintf (pname, sizeof (pname), "fill%zu", i);
        (void)dp_tlm_probe (tlm, pname, 1);
      }
    DP_CHECK (carrier_nda_set_telemetry (c, tlm, "nope", 1) == DP_ERR_INVALID);
    DP_CHECK (c->tlm.ctx == NULL);
    carrier_nda_destroy (c);
    dp_tlm_destroy (tlm);
  }

  DP_TEST_END ("test_carrier_nda_core");
}
