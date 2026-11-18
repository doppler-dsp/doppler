/**
 * @file test_costas_core.c
 * @brief Unit tests for the Costas carrier-tracking loop.
 *
 * Tests:
 *   1. Lifecycle / gain math / init==create parity
 *   2. Pull-in — locks a grid of carrier residuals (freq + lock + bits)
 *   3. 180° BPSK ambiguity is tolerated (output correct up to a global flip)
 *   4. Noise robustness — locks under AWGN
 *   5. Dynamic stress — tracks a frequency ramp (Doppler rate) with bounded
 * err
 *   6. Reset reproducibility
 */
#include "doppler/costas/costas_core.h"
#include "doppler/dp_complex.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Build a continuous BPSK-at-symbol-rate signal with carrier residual f0
 * (cycles/sample), optional per-sample frequency ramp (Doppler rate), and
 * optional AWGN sigma per component.  Returns the ±1 bits used in `bits`. */
static void
make_signal (float _Complex *rx, int *bits, size_t nsym, size_t tsamps,
             double f0, double ramp, float sigma, uint32_t seed)
{
  uint32_t bst = seed, nst = seed ^ 0x9e3779b9u;
  double   phase = 0.0, w = f0 * 2.0 * M_PI;
  size_t   k = 0;
  for (size_t s = 0; s < nsym; s++)
    {
      int b   = dp_bit (&bst);
      bits[s] = b;
      for (size_t i = 0; i < tsamps; i++, k++)
        {
          float _Complex c = cexpf ((float)phase * I);
          rx[k]            = (float)b * c;
          if (sigma > 0.0f)
            {
              /* Sequenced: CMPLXF's two arguments are
                 indeterminately sequenced too. gcc and clang happen to
                 agree here (real takes the first draw); pinned anyway,
                 because "they agree today" is not a guarantee. */
              float n_re = sigma * (float)dp_gauss (&nst);
              float n_im = sigma * (float)dp_gauss (&nst);
              rx[k] += CMPLXF (n_re, n_im);
            }
          phase += w;
          w += ramp * 2.0 * M_PI; /* frequency ramps each sample */
        }
    }
}

/* Run the loop over a built signal; report tracked freq, lock, and the
 * ambiguity-tolerant bit-error count over the converged tail. */
static void
run (dp_costas_state_t *c, const float _Complex *rx, const int *bits,
     size_t nsym, size_t tsamps, double *out_freq, double *out_lock,
     int *out_biterr)
{
  float _Complex *sym = malloc (nsym * sizeof (*sym));
  size_t          k   = dp_costas_steps (c, rx, nsym * tsamps, sym, nsym);
  *out_freq           = dp_costas_get_norm_freq (c);
  *out_lock           = dp_costas_get_lock_metric (c);
  /* bit errors over the converged tail (last half), ambiguity-tolerant */
  size_t tail0 = k / 2;
  int    err   = 0;
  for (size_t s = tail0; s < k; s++)
    {
      int dec = (crealf (sym[s]) >= 0.0f) ? 1 : -1;
      if (dec != bits[s])
        err++;
    }
  int n       = (int)(k - tail0);
  *out_biterr = err < n - err ? err : n - err; /* min(err, n-err) */
  free (sym);
}

int
main (void)
{

  /* ---------------------------------------------------------------- *
   * 1. Lifecycle, gain math, init==create parity                     *
   * ---------------------------------------------------------------- */
  {
    dp_costas_state_t *c = dp_costas_create (0.05, 0.707, 0.01, 16, 0.0);
    DP_CHECK (c != NULL);
    if (!c)
      return 1;
    /* gains derive from the embedded loop_filter (bn,zeta,t=1) */
    DP_CHECK (c->lf.kp > 0.0 && c->lf.ki > 0.0);
    /* seeded NCO frequency == requested residual */
    DP_CHECK (fabs (dp_costas_get_norm_freq (c) - 0.01) < 1e-12);

    dp_costas_state_t v;
    dp_costas_init (&v, 0.05, 0.707, 0.01, 16, 0.0);
    DP_CHECK (v.lf.kp == c->lf.kp && v.lf.ki == c->lf.ki);
    DP_CHECK (v.nco.phase_inc == c->nco.phase_inc);
    dp_costas_destroy (c);
  }

  /* ---------------------------------------------------------------- *
   * 2. Pull-in — a grid of carrier residuals locks                   *
   * ---------------------------------------------------------------- */
  {
    const size_t    tsamps = 16, nsym = 4000;
    float _Complex *rx    = malloc (nsym * tsamps * sizeof (*rx));
    int            *bits  = malloc (nsym * sizeof (*bits));
    double          f0s[] = { 0.0, 0.001, 0.003, -0.004 };
    for (int t = 0; t < 4; t++)
      {
        make_signal (rx, bits, nsym, tsamps, f0s[t], 0.0, 0.0f, 12345u);
        dp_costas_state_t *c
            = dp_costas_create (0.05, 0.707, 0.0, tsamps, 0.0);
        double f, lk;
        int    be;
        run (c, rx, bits, nsym, tsamps, &f, &lk, &be);
        DP_CHECK (fabs (f - f0s[t]) < 2e-4); /* tracked the residual    */
        DP_CHECK (lk > 0.9);                 /* phase-locked            */
        DP_CHECK (be == 0);                  /* zero bit errors on tail */
        dp_costas_destroy (c);
      }
    free (rx);
    free (bits);
  }

  /* ---------------------------------------------------------------- *
   * 3. 180° BPSK ambiguity — output is correct up to a global flip   *
   * (run() already scores ambiguity-tolerant; assert a flipped seed) *
   * ---------------------------------------------------------------- */
  {
    const size_t    tsamps = 16, nsym = 3000;
    float _Complex *rx   = malloc (nsym * tsamps * sizeof (*rx));
    int            *bits = malloc (nsym * sizeof (*bits));
    make_signal (rx, bits, nsym, tsamps, 0.002, 0.0, 0.0f, 777u);
    dp_costas_state_t *c = dp_costas_create (0.05, 0.707, 0.0, tsamps, 0.0);
    double             f, lk;
    int                be;
    run (c, rx, bits, nsym, tsamps, &f, &lk, &be);
    DP_CHECK (be == 0); /* min(err, n-err)==0 even if globally inverted  */
    DP_CHECK (lk > 0.9);
    dp_costas_destroy (c);
    free (rx);
    free (bits);
  }

  /* ---------------------------------------------------------------- *
   * 4. Noise robustness — locks under AWGN (per-symbol SNR is high   *
   * after tsamps-fold coherent integration)                          *
   * ---------------------------------------------------------------- */
  {
    const size_t    tsamps = 16, nsym = 5000;
    float _Complex *rx   = malloc (nsym * tsamps * sizeof (*rx));
    int            *bits = malloc (nsym * sizeof (*bits));
    /* sigma=1.0 per component → ~ -3 dB per-sample SNR; +12 dB from the
     * 16-fold I&D → comfortably locked. That arithmetic is only true as of
     * the move to dp_rng_test.h: this file's own Box-Muller drew both of its
     * uniforms from a degenerate two-shift recurrence and delivered variance
     * 1.115, so the stated sigma was 0.47 dB optimistic. */
    make_signal (rx, bits, nsym, tsamps, 0.0015, 0.0, 1.0f, 2024u);
    dp_costas_state_t *c = dp_costas_create (0.03, 0.707, 0.0, tsamps, 0.0);
    double             f, lk;
    int                be;
    run (c, rx, bits, nsym, tsamps, &f, &lk, &be);
    DP_CHECK (fabs (f - 0.0015) < 5e-4);
    DP_CHECK (lk > 0.7);
    /* A BOUND, not `be == 0`. At ~9 dB Es/N0 over a 2500-symbol tail the
     * expected error count is ~0.1, so zero errors is a ~90%-per-seed
     * outcome, not a property: swept over seeds 2024..2043 this returns 1
     * error at two of them. `be == 0` passed only because the seed is
     * pinned, and would have flaked the first time libm rounded differently
     * on another runner. Three errors is BER 1.2e-3 — still decisively
     * "locked and decoding", and ~4e-6 likely to be exceeded by chance. */
    DP_CHECK (be <= 3);
    dp_costas_destroy (c);
    free (rx);
    free (bits);
  }

  /* ---------------------------------------------------------------- *
   * 5. Dynamic stress — track a frequency ramp (Doppler rate)        *
   * ---------------------------------------------------------------- */
  {
    const size_t    tsamps = 16, nsym = 6000;
    float _Complex *rx   = malloc (nsym * tsamps * sizeof (*rx));
    int            *bits = malloc (nsym * sizeof (*bits));
    /* start at 0, ramp the carrier to a final offset; 2nd-order loop
     * tracks a constant rate with bounded (small) steady error. */
    double ramp = 5e-9; /* cycles/sample per sample */
    make_signal (rx, bits, nsym, tsamps, 0.0, ramp, 0.0f, 99u);
    double             final_f0 = ramp * (double)(nsym * tsamps);
    dp_costas_state_t *c = dp_costas_create (0.06, 0.707, 0.0, tsamps, 0.0);
    double             f, lk;
    int                be;
    run (c, rx, bits, nsym, tsamps, &f, &lk, &be);
    /* Follows the moving carrier, two-sided. The loop holds the steady
       phase error e = x/ki (x = 2 pi ramp tsamps^2 rad/update^2), and its
       frequency estimate, the integrator alone, trails the carrier by the
       proportional term's share, kp e rad/update, kp x / (ki 2 pi tsamps)
       cycles/sample (about 1e-6 here, against a final offset of 4.8e-4).
       The integrator also holds an update's AVERAGE frequency, which trails
       the carrier's frequency at the last sample by half an update of ramp,
       ramp tsamps / 2 (0.04 of the lag here). With both in the prediction,
       what remains is measured at 0.4% of the lag (settling, and sin e
       against e at 6.8e-4 rad); the tolerance, a tenth of the lag, is 25
       times that, and a loop parked at 0, or one that stopped following,
       misses by orders. */
    const double x   = 2.0 * M_PI * ramp * (double)(tsamps * tsamps);
    const double lag = c->lf.kp * x / (c->lf.ki * 2.0 * M_PI * (double)tsamps);
    const double half = ramp * (double)tsamps / 2.0;
    DP_CHECK_MSG (fabs (f - (final_f0 - half - lag)) < 0.1 * lag,
                  "order 2 follows a ramp, trailing by its predicted lag");
    /* Negative control: the same predicate on a loop parked at 0 fails. */
    DP_CHECK_MSG (!(fabs (0.0 - (final_f0 - half - lag)) < 0.1 * lag),
                  "control: a loop parked at 0 fails the ramp predicate");
    DP_CHECK (lk > 0.85);
    DP_CHECK (be == 0);
    dp_costas_destroy (c);
    free (rx);
    free (bits);
  }

  /* ---------------------------------------------------------------- *
   * 6. Reset reproducibility — run #2 == run #1 after reset          *
   * ---------------------------------------------------------------- */
  {
    const size_t    tsamps = 16, nsym = 1500;
    float _Complex *rx   = malloc (nsym * tsamps * sizeof (*rx));
    int            *bits = malloc (nsym * sizeof (*bits));
    make_signal (rx, bits, nsym, tsamps, 0.002, 0.0, 0.0f, 55u);
    dp_costas_state_t *c = dp_costas_create (0.05, 0.707, 0.0, tsamps, 0.0);
    double             f1, lk1;
    int                be1;
    run (c, rx, bits, nsym, tsamps, &f1, &lk1, &be1);
    dp_costas_reset (c);
    double f2, lk2;
    int    be2;
    run (c, rx, bits, nsym, tsamps, &f2, &lk2, &be2);
    DP_CHECK (f1 == f2 && lk1 == lk2 && be1 == be2);
    dp_costas_destroy (c);
    free (rx);
    free (bits);
  }

  /* ---------------------------------------------------------------- *
   * 7. FLL assist widens pull-in — a residual too large for the bare *
   * PLL is acquired once the FLL assist is enabled.                  *
   * ---------------------------------------------------------------- */
  {
    const size_t    tsamps = 16, nsym = 6000;
    float _Complex *rx   = malloc (nsym * tsamps * sizeof (*rx));
    int            *bits = malloc (nsym * sizeof (*bits));
    /* ~0.8 rad/symbol residual — beyond the narrow PLL's pull-in. */
    double f0 = 0.008;
    make_signal (rx, bits, nsym, tsamps, f0, 0.0, 0.0f, 31u);
    double f, lk;
    int    be;

    /* Pure PLL (bn_fll = 0): fails to lock onto the large residual. */
    dp_costas_state_t *pll = dp_costas_create (0.01, 0.707, 0.0, tsamps, 0.0);
    run (pll, rx, bits, nsym, tsamps, &f, &lk, &be);
    int pll_locked = (fabs (f - f0) < 5e-4) && (lk > 0.9);
    DP_CHECK (!pll_locked); /* the bare PLL does NOT acquire it */
    dp_costas_destroy (pll);

    /* FLL-assisted (bn_fll > 0): the wide frequency discriminator pulls
     * the integrator on, and the loop locks. */
    dp_costas_state_t *fll = dp_costas_create (0.01, 0.707, 0.0, tsamps, 0.03);
    run (fll, rx, bits, nsym, tsamps, &f, &lk, &be);
    DP_CHECK (fabs (f - f0) < 5e-4); /* tracked the large residual */
    DP_CHECK (lk > 0.9);             /* locked */
    DP_CHECK (be == 0);              /* zero bit errors on the tail */
    dp_costas_destroy (fll);
    free (rx);
    free (bits);
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

    dp_costas_state_t *a  = dp_costas_create (0.01, 0.707, 0.0, 4, 0.0);
    size_t             nA = dp_costas_steps (a, rx, L, outA, CAP);
    dp_costas_destroy (a);

    dp_costas_state_t *r1   = dp_costas_create (0.01, 0.707, 0.0, 4, 0.0);
    size_t             nB   = dp_costas_steps (r1, rx, CUT, outB, CAP);
    size_t             sb   = dp_costas_state_bytes (r1);
    void              *blob = malloc (sb);
    dp_costas_get_state (r1, blob);
    dp_costas_destroy (r1);

    dp_costas_state_t *r2 = dp_costas_create (0.01, 0.707, 0.0, 4, 0.0);
    DP_CHECK (dp_costas_set_state (r2, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF;
    DP_CHECK (dp_costas_set_state (r2, blob) == DP_ERR_INVALID);
    ((char *)blob)[0] ^= (char)0xFF;
    nB += dp_costas_steps (r2, rx + CUT, L - CUT, outB + nB, CAP - nB);
    dp_costas_destroy (r2);
    free (blob);

    DP_CHECK (nA == nB);
    for (size_t i = 0; i < nA && i < nB; i++)
      DP_CHECK (crealf (outA[i]) == crealf (outB[i])
                && cimagf (outA[i]) == cimagf (outB[i]));
    free (rx);
    free (outA);
    free (outB);
  }

  /* verify-counted carrier lock decision: a phase-locked BPSK stream
   * declares after the default 8-symbol verify run; noise-only input
   * never does; reset drops the lock and keeps the configured rule. */
  {
    enum
    {
      TS = 16,
      NS = 256
    };
    float _Complex rx[TS * NS], out[NS];
    /* constant BPSK at a locked phase: metric EMA -> 1 quickly */
    for (int i = 0; i < TS * NS; i++)
      rx[i] = ((i / (TS * 4)) % 2 ? -1.0f : 1.0f) + 0.0f * I;
    dp_costas_state_t *c = dp_costas_create (0.05, 0.707, 0.0, TS, 0.0);
    DP_CHECK (c != NULL);
    DP_CHECK (dp_costas_get_locked (c) == 0); /* fresh: unlocked */
    DP_CHECK (c->lock.up_thresh == 0.85 && c->lock.down_thresh == 0.78);
    DP_CHECK (c->lock.n_up == 8 && c->lock.n_down == 32);
    (void)dp_costas_steps (c, rx, TS * NS, out, NS);
    DP_CHECK (dp_costas_get_locked (c) == 1);
    DP_CHECK (dp_costas_get_lock_metric (c) > 0.85);

    /* reset drops the decision but keeps the rule */
    dp_costas_reset (c);
    DP_CHECK (dp_costas_get_locked (c) == 0);
    DP_CHECK (c->lock.n_down == 32);

    /* configure_lock re-tunes; an unreachable declare threshold never
     * locks even on the clean stream */
    dp_costas_configure_lock (c, 2.0, 1.9, 8, 32);
    (void)dp_costas_steps (c, rx, TS * NS, out, NS);
    DP_CHECK (dp_costas_get_locked (c) == 0);
    dp_costas_destroy (c);

    /* noise only: |cos(theta)| EMA hovers near 2/pi ~ 0.64, well under
     * the 0.85 declare threshold -> never declares. dp_cgauss carries
     * E|z|^2 = 1 where the hand-rolled loop here carried 2; the metric is
     * amplitude-normalised, so the measured value moves by 3e-6. */
    uint32_t           st = 77u;
    dp_costas_state_t *n  = dp_costas_create (0.05, 0.707, 0.0, TS, 0.0);
    DP_CHECK (n != NULL);
    for (int i = 0; i < TS * NS; i++)
      {
        rx[i] = dp_cgauss (&st);
      }
    (void)dp_costas_steps (n, rx, TS * NS, out, NS);
    DP_CHECK (dp_costas_get_locked (n) == 0);
    DP_CHECK (dp_costas_get_lock_metric (n) < 0.85);
    dp_costas_destroy (n);
  }

  /* telemetry attach — seven records per dumped symbol; blobs stay
   * attachment-independent and a live attachment survives set_state. */
  {
    enum
    {
      TS = 16,
      NS = 64,
      L  = TS * NS
    };
    float _Complex rx[L], out[NS];
    dp_tlm_rec_t recs[1024];
    /* A carrier 1e-3 cycles a sample off: the loop pulls it in, so its
       discriminator (and the proportional term ".nco" carries) moves. */
    for (int i = 0; i < L; i++)
      rx[i] = ((i / (TS * 4)) % 2 ? -1.0f : 1.0f)
              * CMPLXF (cosf (2.0f * (float)M_PI * 1e-3f * (float)i),
                        sinf (2.0f * (float)M_PI * 1e-3f * (float)i));
    dp_tlm_t          *tlm = dp_tlm_create (4096);
    dp_costas_state_t *c   = dp_costas_create (0.05, 0.707, 0.0, TS, 0.0);
    DP_CHECK (tlm != NULL && c != NULL);
    DP_CHECK (dp_costas_set_telemetry (c, tlm, "car", 1) == DP_OK);
    DP_CHECK (dp_tlm_probe_id (tlm, "car.lock") == c->tlm.id_lock);
    DP_CHECK (dp_tlm_probe_id (tlm, "car.e") == c->tlm.id_e);
    DP_CHECK (dp_tlm_probe_id (tlm, "car.freq") == c->tlm.id_freq);
    DP_CHECK (dp_tlm_probe_id (tlm, "car.locked") == c->tlm.id_locked);
    DP_CHECK (dp_tlm_probe_id (tlm, "car.nco") == c->tlm.id_nco);
    DP_CHECK (dp_tlm_probe_id (tlm, "car.freq_rate") == c->tlm.id_freq_rate);
    DP_CHECK (dp_tlm_probe_id (tlm, "car.order") == c->tlm.id_order);
    /* The registry is exactly the header's list: seven probes, no more. */
    DP_CHECK (dp_tlm_probe_count (tlm) == 7);

    size_t k = dp_costas_steps (c, rx, L, out, NS);
    DP_CHECK (k == NS);
    size_t n_rec = dp_tlm_read (tlm, 512, recs, 512);
    /* lock, e, freq, locked, nco, freq_rate, order per symbol */
    DP_CHECK (n_rec == 7 * NS);
    /* The last records mirror the tracked state, in flush order; order 2
     * holds no rate. */
    DP_CHECK (recs[n_rec - 5].value == (float)c->nco.norm_freq);
    DP_CHECK (recs[n_rec - 4].value == (float)dp_costas_get_locked (c));
    DP_CHECK (recs[n_rec - 3].value == (float)dp_costas_get_nco_freq (c));
    DP_CHECK (recs[n_rec - 2].value == 0.0f);
    DP_CHECK (recs[n_rec - 1].value == 2.0f);
    /* Every symbol: ".nco" is the whole filter output, ".freq" (the
     * integrator) plus the proportional term on that symbol's ".e" --
     * kp*e radians a symbol, kp*e / (2 pi tsamps) cycles a sample. The
     * largest term is checked too, so a ".nco" that reported ".freq"
     * cannot pass on symbols whose error happened to be zero. */
    double worst = 0.0, biggest = 0.0;
    for (size_t j = 0; j + 6 < n_rec; j += 7)
      {
        const double prop
            = c->lf.kp * recs[j + 1].value / (2.0 * M_PI * (double)TS);
        const double gap = fabs ((double)recs[j + 4].value
                                 - ((double)recs[j + 2].value + prop));
        worst            = gap > worst ? gap : worst;
        biggest          = fabs (prop) > biggest ? fabs (prop) : biggest;
      }
    DP_CHECK_MSG (worst < 1e-7, "car.nco is car.freq plus kp*e");
    DP_CHECK_MSG (biggest > 1e-4, "the proportional term was exercised");
    if (worst >= 1e-7 || biggest <= 1e-4)
      DP_TEST_ERR ("  car.nco - (car.freq + kp*e): worst %.3g, largest "
                   "term %.3g\n",
                   worst, biggest);

    /* At order 3 ".freq_rate" is the rate integrator and ".order" reads 3,
       every symbol: a ramp moves the rate, so a probe that reported 0 or
       the raw per-update state cannot pass. */
    {
      DP_CHECK (dp_costas_set_order (c, 3, 0.05) == DP_OK);
      float _Complex ramp[L];
      for (int i = 0; i < L; i++)
        {
          const double ph
              = 2.0 * M_PI
                * (1e-3 * (double)i + 0.5 * 2e-8 * (double)i * (double)i);
          ramp[i] = CMPLXF ((float)cos (ph), (float)sin (ph));
        }
      (void)dp_tlm_read (tlm, 1024, recs, 1024);
      DP_CHECK (dp_costas_steps (c, ramp, L, out, NS) == NS);
      n_rec = dp_tlm_read (tlm, 1024, recs, 1024);
      DP_CHECK (n_rec == 7 * NS);
      DP_CHECK (recs[n_rec - 2].value == (float)dp_costas_get_norm_rate (c));
      DP_CHECK (recs[n_rec - 1].value == 3.0f);
      DP_CHECK_MSG (fabs (dp_costas_get_norm_rate (c)) > 1e-9,
                    "the ramp moved the order-3 rate the probe reports");
      DP_CHECK (dp_costas_set_order (c, 2, 0.05) == DP_OK);
    }

    /* Blobs zero the attachment (deterministic) and set_state into an
     * attached instance preserves that instance's live attachment. */
    size_t sb = dp_costas_state_bytes (c);
    void  *b1 = malloc (sb), *b2 = malloc (sb);
    dp_costas_get_state (c, b1);
    dp_costas_state_t *d = dp_costas_create (0.05, 0.707, 0.0, TS, 0.0);
    DP_CHECK (d != NULL);
    DP_CHECK (dp_costas_set_telemetry (d, tlm, "car2", 2) == DP_OK);
    DP_CHECK (dp_costas_set_state (d, b1) == DP_OK);
    DP_CHECK (d->tlm.ctx == tlm);
    DP_CHECK (d->tlm.id_e == dp_tlm_probe_id (tlm, "car2.e"));
    dp_costas_get_state (d, b2);
    DP_CHECK (memcmp (b1, b2, sb) == 0); /* attachment-independent bytes */
    free (b1);
    free (b2);
    dp_costas_destroy (d);

    /* Detach: probe sites revert to the single-branch cost. */
    DP_CHECK (dp_costas_set_telemetry (c, NULL, "car", 1) == DP_OK);
    DP_CHECK (c->tlm.ctx == NULL);
    (void)dp_costas_steps (c, rx, L, out, NS);
    DP_CHECK (dp_tlm_read (tlm, 512, recs, 512) == 0);

    /* A full probe table fails the attach whole. */
    char pname[DP_TLM_NAME_MAX];
    for (size_t i = 0; dp_tlm_probe_count (tlm) < DP_TLM_MAX_PROBES; i++)
      {
        (void)snprintf (pname, sizeof (pname), "fill%zu", i);
        (void)dp_tlm_probe (tlm, pname, 1);
      }
    DP_CHECK (dp_costas_set_telemetry (c, tlm, "nope", 1) == DP_ERR_INVALID);
    DP_CHECK (c->tlm.ctx == NULL);
    dp_costas_destroy (c);
    dp_tlm_destroy (tlm);
  }

  {
    /* An overlong prefix is refused with nothing registered, and the
     * longest prefix that fits (21 bytes: "<prefix>.freq_rate" is 31)
     * attaches all seven probes. */
    dp_tlm_t          *tlm = dp_tlm_create (256);
    dp_costas_state_t *c   = dp_costas_create (0.05, 0.707, 0.01, 16, 0.0);
    DP_CHECK (tlm != NULL && c != NULL);
    if (tlm && c)
      {
        char over[23], fit[22];
        memset (over, 'c', 22);
        over[22] = '\0';
        memset (fit, 'c', 21);
        fit[21] = '\0';
        DP_CHECK (dp_costas_set_telemetry (c, tlm, over, 1) == DP_ERR_INVALID);
        DP_CHECK (c->tlm.ctx == NULL && dp_tlm_probe_count (tlm) == 0);
        DP_CHECK (dp_costas_set_telemetry (c, tlm, fit, 1) == DP_OK);
        DP_CHECK (dp_tlm_probe_count (tlm) == 7 && c->tlm.ctx == tlm);
        DP_CHECK (dp_costas_set_telemetry (c, tlm, "car", 1) == DP_OK);
      }
    dp_costas_destroy (c);
    dp_tlm_destroy (tlm);
  }

  /* ── every byte of the blob is determined: the state is serialized
     whole, and a loop built in place on the stack (as the DSSS receivers'
     hand-offs do) used to carry the 4-byte hole after have_prev, which
     dp_costas_init never wrote. Two states poisoned differently and
     initialised the same must give the same blob. ── */
  {
    dp_costas_state_t a, b;
    memset (&a, 0xA5, sizeof a);
    memset (&b, 0x5A, sizeof b);
    dp_costas_init (&a, 0.01, 0.707, 0.01, 8, 0.0);
    dp_costas_init (&b, 0.01, 0.707, 0.01, 8, 0.0);
    const size_t   sb = dp_costas_state_bytes (&a);
    unsigned char *ba = malloc (sb), *bb = malloc (sb);
    DP_REQUIRE (ba != NULL && bb != NULL);
    memset (ba, 0xA5, sb);
    memset (bb, 0x5A, sb);
    dp_costas_get_state (&a, ba);
    dp_costas_get_state (&b, bb);
    DP_CHECK (memcmp (ba, bb, sb) == 0);
    free (ba);
    free (bb);
  }

  /* A bad loop parameter is refused at create and by every setter, and a
     refused retune changes nothing -- bn, zeta and the gains -- so the
     getter never reports a bandwidth the loop is not running. A finite bn
     of 1e200 overflows the gains to NaN, which the predicate now refuses
     (doppler#2112). */
  {
    DP_CHECK (dp_costas_create (NAN, 0.707, 0.01, 16, 0.0) == NULL);
    DP_CHECK (dp_costas_create (1e200, 0.707, 0.01, 16, 0.0) == NULL);
    dp_costas_state_t *c = dp_costas_create (0.05, 0.707, 0.01, 16, 0.0);
    DP_CHECK (c != NULL);
    if (c)
      {
        /* The whole object, not its loop filter: a refusal changes no byte. */
        const size_t   cb     = dp_costas_state_bytes (c);
        unsigned char *before = malloc (cb), *after = malloc (cb);
        dp_costas_get_state (c, before);
        const double bn = c->bn, zeta = c->zeta;
        DP_CHECK (dp_costas_configure (c, NAN, 0.707) == DP_ERR_INVALID);
        DP_CHECK (dp_costas_configure (c, 0.05, INFINITY) == DP_ERR_INVALID);
        DP_CHECK (dp_costas_configure (c, 1e200, 0.707) == DP_ERR_INVALID);
        /* Twice: a refusal must not compound (CarrierNda rescaled its
           unchanged gains by 1/2pi on each). */
        DP_CHECK (dp_costas_set_bn (c, NAN) == DP_ERR_INVALID);
        DP_CHECK (dp_costas_set_bn (c, NAN) == DP_ERR_INVALID);
        dp_costas_get_state (c, after);
        DP_CHECK (memcmp (after, before, cb) == 0);
        free (before);
        free (after);
        DP_CHECK (c->bn == bn && c->zeta == zeta);
        /* The control: a good retune is taken. */
        DP_CHECK (dp_costas_configure (c, 0.02, 1.0) == DP_OK && c->bn == 0.02
                  && c->zeta == 1.0);
        dp_costas_destroy (c);
      }
  }

  /* #2646: bn_fll outside dp_loop_filter_fll_bn_ok() is refused, at
     create and live; the getter keeps the value the loop is running. */
  DP_CHECK (dp_costas_create (0.05, 0.707, 0.0, 64, -0.01) == NULL);
  DP_CHECK (dp_costas_create (0.05, 0.707, 0.0, 64, NAN) == NULL);
  DP_CHECK (dp_costas_create (0.05, 0.707, 0.0, 64, INFINITY) == NULL);
  {
    dp_costas_state_t *f = dp_costas_create (0.05, 0.707, 0.0, 64, 0.01);
    DP_REQUIRE (f != NULL);
    DP_CHECK (dp_costas_set_bn_fll (f, -0.5) == DP_ERR_INVALID);
    DP_CHECK (dp_costas_set_bn_fll (f, NAN) == DP_ERR_INVALID);
    DP_CHECK (dp_costas_set_bn_fll (f, INFINITY) == DP_ERR_INVALID);
    DP_CHECK (dp_costas_get_bn_fll (f) == 0.01 && f->k_fll == 0.04);
    DP_CHECK (dp_costas_set_bn_fll (f, 0.0) == DP_OK);
    DP_CHECK (dp_costas_get_bn_fll (f) == 0.0 && f->k_fll == 0.0);
    dp_costas_destroy (f);
  }

  /* ---------------------------------------------------------------- *
   * #2142: a forged verdict is refused, the object untouched *
   * ---------------------------------------------------------------- */
  {
    /* The embedded detector's verdict is 0 or 1. */
    dp_costas_state_t *c = dp_costas_create (0.05, 0.707, 0.01, 16, 0.0);
    DP_CHECK (c != NULL);
    const size_t lk    = sizeof (dp_state_hdr_t)
                         + offsetof (dp_costas_state_t, lock)
                         + offsetof (dp_lockdet_state_t, locked);
    const int    seven = 7;
    DP_STATE_FORGE_TEST (dp_costas, c, COSTAS_STATE_MAGIC,
                         COSTAS_STATE_VERSION, lk, &seven, sizeof seven);

    /* The embedded loop filter's gains are the ones its bn, zeta and t give
       (#2641): a forged NaN gain is refused, as LoopFilter's restore does. */
    const size_t kp_off = sizeof (dp_state_hdr_t)
                          + offsetof (dp_costas_state_t, lf)
                          + offsetof (dp_loop_filter_state_t, kp);
    const double nan_g  = NAN;
    DP_STATE_FORGE_TEST (dp_costas, c, COSTAS_STATE_MAGIC,
                         COSTAS_STATE_VERSION, kp_off, &nan_g, sizeof nan_g);

    /* What create() and the setters refuse, a blob may not carry (#2022):
       a live loop with no samples per symbol, retained bn/zeta that are
       not the loop's, an FLL assist set_bn_fll() refuses, and a derived FLL
       gain that is not 4 * bn_fll. Each forge changes one field. */
    {
      const size_t H      = sizeof (dp_state_hdr_t);
      const size_t zero_t = 0;
      const double bn2 = 0.02, z2 = 0.5, kf = 1.0;
      DP_STATE_FORGE_TEST (
          dp_costas, c, COSTAS_STATE_MAGIC, COSTAS_STATE_VERSION,
          H + offsetof (dp_costas_state_t, tsamps), &zero_t, sizeof zero_t);
      DP_STATE_FORGE_TEST (
          dp_costas, c, COSTAS_STATE_MAGIC, COSTAS_STATE_VERSION,
          H + offsetof (dp_costas_state_t, bn), &bn2, sizeof bn2);
      DP_STATE_FORGE_TEST (
          dp_costas, c, COSTAS_STATE_MAGIC, COSTAS_STATE_VERSION,
          H + offsetof (dp_costas_state_t, zeta), &z2, sizeof z2);
      /* A NaN assist would also break k_fll == 4 * bn_fll, so the forge
         keeps the pair consistent ({-0.5, -2.0}, adjacent in the struct):
         only set_bn_fll's own rule refuses a negative bandwidth. */
      const double fll_pair[2] = { -0.5, -2.0 };
      DP_STATE_FORGE_TEST (
          dp_costas, c, COSTAS_STATE_MAGIC, COSTAS_STATE_VERSION,
          H + offsetof (dp_costas_state_t, bn_fll), fll_pair, sizeof fll_pair);
      DP_STATE_FORGE_TEST (
          dp_costas, c, COSTAS_STATE_MAGIC, COSTAS_STATE_VERSION,
          H + offsetof (dp_costas_state_t, k_fll), &kf, sizeof kf);
    }

    /* The all-zero state is a state: a receiver embeds Costas loops by
       value and holds them zeroed until it seeds them, and its blob carries
       them that way (tsamps 0, detector counts 0). It must round-trip. */
    dp_costas_state_t zero;
    memset (&zero, 0, sizeof zero);
    unsigned char *blob = malloc (dp_costas_state_bytes (&zero));
    DP_CHECK (blob != NULL);
    if (blob)
      {
        dp_costas_get_state (&zero, blob);
        DP_CHECK (dp_costas_set_state (c, blob) == DP_OK);
        free (blob);
      }
    dp_costas_destroy (c);
  }

  /* ---------------------------------------------------------------- *
   * 10. Order 3 (docs/design/costas.md section 3)                    *
   * ---------------------------------------------------------------- */
  /* 10a. A frequency ramp: order 2 holds the steady phase error x/ki
     (dp_state_filter_steady_error, the closed form a caller sizes from);
     order 3 holds none. Noise-free, one prompt at a time so the mean error
     over the converged tail is read from the loop itself. */
  {
    const size_t    tsamps = 16, nsym = 8000, tail = 2000;
    const double    ramp = 2e-8, bn = 0.05; /* cycles/sample^2 */
    float _Complex *rx   = malloc (nsym * tsamps * sizeof (*rx));
    int            *bits = malloc (nsym * sizeof (*bits));
    float _Complex sym;
    make_signal (rx, bits, nsym, tsamps, 0.0, ramp, 0.0f, 4242u);
    /* the ramp per update, radians/update^2 (the loop's own units) */
    const double x         = 2.0 * M_PI * ramp * (double)(tsamps * tsamps);
    double       mean_e[2] = { 0.0, 0.0 }, expect2 = 0.0, learned = 0.0;
    for (int o = 0; o < 2; o++)
      {
        dp_costas_state_t *c = dp_costas_create (bn, 0.707, 0.0, tsamps, 0.0);
        DP_REQUIRE (c != NULL);
        if (o == 1)
          DP_REQUIRE (dp_costas_set_order (c, 3, bn) == DP_OK);
        else
          expect2 = dp_state_filter_steady_error (2, c->lf.ki, 0.0, 2, x);
        for (size_t s = 0; s < nsym; s++)
          {
            DP_REQUIRE (dp_costas_steps (c, rx + s * tsamps, tsamps, &sym, 1)
                        == 1);
            if (s >= nsym - tail)
              mean_e[o] += dp_costas_get_last_error (c) / (double)tail;
          }
        if (o == 1)
          learned = dp_costas_get_norm_rate (c);
        dp_costas_destroy (c);
      }
    /* A loop seeded with the ramp's rate (and the carrier's starting
       frequency, 0) holds it from the first update: its error over the
       first updates, where an unseeded loop is still learning the rate. */
    double       seeded_e = 0.0, seeded_rate = 0.0, unseeded_e = 0.0;
    const size_t head = 200;
    for (int sd = 0; sd < 2; sd++)
      {
        dp_costas_state_t *c = dp_costas_create (bn, 0.707, 0.0, tsamps, 0.0);
        DP_REQUIRE (c != NULL);
        DP_REQUIRE (dp_costas_set_order (c, 3, bn) == DP_OK);
        if (sd)
          DP_REQUIRE (dp_costas_set_norm_rate (c, ramp) == DP_OK);
        double peak = 0.0;
        for (size_t s = 0; s < head; s++)
          {
            DP_REQUIRE (dp_costas_steps (c, rx + s * tsamps, tsamps, &sym, 1)
                        == 1);
            peak = fmax (peak, fabs (dp_costas_get_last_error (c)));
          }
        if (sd)
          seeded_e = peak, seeded_rate = dp_costas_get_norm_rate (c);
        else
          unseeded_e = peak;
        dp_costas_destroy (c);
      }
    /* norm_rate's units, two-sided against a known ramp: a scaling wrong
       in both the setter and the getter (a factor 2 pi, tsamps or
       tsamps^2) misses by orders, where set-then-get cannot see it. The
       learned rate converges noise-free to within 1.5e-5 of the ramp, the
       held seed to within 1e-6; 1e-3 sits orders under any units error. */
    DP_CHECK_MSG (fabs (learned / ramp - 1.0) < 1e-3,
                  "order 3 learns the ramp's rate, in cycles/sample^2");
    /* Negative control: the rate in the filter's own per-update units (a
       missing 2 pi tsamps^2) fails the same predicate. */
    DP_CHECK_MSG (
        !(fabs (learned * 2.0 * M_PI * (double)(tsamps * tsamps) / ramp - 1.0)
          < 1e-3),
        "control: the rate in per-update units fails the units predicate");
    DP_CHECK_MSG (fabs (seeded_rate / ramp - 1.0) < 1e-3,
                  "a rate seeded at the ramp's is held");
    /* Seeded, the loop meets the ramp with no learning transient: its
       peak error over the first updates is a tenth of the unseeded loop's
       or less (measured 1.0e-4 against 4.2e-3 rad). */
    DP_CHECK_MSG (seeded_e < 0.1 * unseeded_e,
                  "a seeded rate removes the ramp's learning transient");
    /* e = sin(phase error) in this discriminator; the error is mrad. */
    DP_CHECK_MSG (fabs (mean_e[0] / sin (expect2) - 1.0) < 0.05,
                  "order 2 lags a ramp by x/ki");
    DP_CHECK_MSG (fabs (mean_e[1]) < 0.02 * expect2,
                  "order 3 holds a ramp with no steady phase error");
    free (rx);
    free (bits);
  }

  /* 10e. Order 2 is the loop it always was, to rounding. An independent
     order-2 recurrence, written here from the loop's definition (the
     header's section 1 and docs/design/costas.md section 1), not from
     costas_update(): the wipe-off and integrate-and-dump into its own
     prompt, lf's two lines (integ += ki e; the control is integ + kp e) and
     the steer (the NCO frequency from integ, a kp e phase step).

     Not bit for bit: this is a second implementation in another
     translation unit, and a compiler may contract a multiply-add into one
     rounding in one and not the other (clang and arm64 do). So the
     reference's filter and steer run on the library's own prompt each
     update, which keeps the two errors e within a few float roundings of
     each other: under -ffast-math each translation unit may compute e's
     cabsf and divide differently (arm64 does). So the frequency estimate
     itself is not held: its bound would rest on that premise, which no x86
     run can check, and it sees only one of the two sabotages, which the
     phase and the prompts catch by more. Each remaining quantity is held to
     a bound derived from the run, not picked:
       - the NCO phase word: one count per sample at most from the steer's
         rounding, so N (tsamps + 1) counts;
       - the reference's own prompt, accumulated in float from its own NCO:
         2^-23 xmax (tsamps + 2) for the accumulation, plus xmax times the
         phase bound in radians.
     A real defect moves them by orders more (the two sabotages: a wrong
     gain, and a frequency read before the step). The FLL assist is off:
     its own pins are section 7. */
  {
    const size_t    tsamps = 16, nsym = 3000, ns = nsym * tsamps;
    float _Complex *rx   = malloc (ns * sizeof (*rx));
    int            *bits = malloc (nsym * sizeof (*bits));
    float _Complex *got  = malloc (nsym * sizeof (*got));
    make_signal (rx, bits, nsym, tsamps, 0.0021, 4e-9, 0.7f, 6061u);
    dp_costas_state_t *c = dp_costas_create (0.03, 0.707, 0.002, tsamps, 0.0);
    DP_REQUIRE (c != NULL && rx && bits && got);
    const double  kp = c->lf.kp, ki = c->lf.ki;
    dp_lo_state_t nco;
    dp_lo_init (&nco, 0.002);
    double integ       = 0.002 * 2.0 * M_PI * (double)tsamps;
    float _Complex acc = 0.0f;
    double xmax = 0.0, dprompt = 0.0;
    for (size_t i = 0; i < ns; i++)
      if (cabsf (rx[i]) > xmax)
        xmax = cabsf (rx[i]);
    size_t       k     = 0;
    const size_t n_got = dp_costas_steps (c, rx, ns, got, nsym);
    for (size_t i = 0; i < ns; i++)
      {
        acc += rx[i] * conjf (lo_step (&nco));
        if ((i + 1) % tsamps)
          continue;
        DP_REQUIRE (k < n_got);
        /* The reference's prompt against the library's; the error from the
           library's, so both filters see the same e. */
        const float _Complex mine = acc / (float)tsamps;
        const double d
            = cabs ((double _Complex)mine - (double _Complex)got[k]);
        if (d > dprompt)
          dprompt = d;
        const float _Complex P = got[k] * (float)tsamps;
        const float  re = crealf (P), im = cimagf (P);
        const float  a = cabsf (P) + COSTAS_EPS;
        const double e = (double)(((re >= 0.0f) ? im : -im) / a);
        integ += ki * e;
        dp_lo_set_norm_freq (&nco, integ / (double)tsamps / (2.0 * M_PI));
        nco.phase += nco_norm_phase_to_word ((kp * e) / (2.0 * M_PI));
        k++;
        acc = 0.0f;
      }
    DP_CHECK (k == n_got);
    const double n        = (double)k;
    const double b_counts = n * (double)(tsamps + 1);
    const double b_prompt = 0x1p-23 * xmax * (double)(tsamps + 2)
                            + xmax * 2.0 * M_PI * b_counts / 4294967296.0;
    const double d_counts = fabs ((double)(int32_t)(c->nco.phase - nco.phase));
    DP_CHECK_MSG (d_counts <= b_counts,
                  "order 2's NCO phase is its recurrence's");
    DP_CHECK_MSG (dprompt <= b_prompt, "order 2's prompts are its "
                                       "recurrence's");
    if (d_counts > b_counts || dprompt > b_prompt
        || getenv ("DP_COSTAS_PIN_DIAG"))
      DP_TEST_ERR ("10e: phase %.0f of %.0f counts, "
                   "prompt %.3e of %.3e\n",
                   d_counts, b_counts, dprompt, b_prompt);
    dp_costas_destroy (c);
    free (rx);
    free (bits);
    free (got);
  }

  /* 10g. Order 3 is the loop docs/design/costas.md section 1 defines, to
     rounding: 10e's construction for the third-order filter. An independent
     order-3 recurrence, written here: its three gains are NOT the library's
     sf but the ones whose exact noise bandwidth is bn, found by bisecting the
     gain scale th (kp = b th, ki = a th^2, kr = th^3, the shape the header
     names) on dp_state_filter_loop_bandwidth(), the general Lyapunov route,
     where the library's gains come from its closed form sf_bn3; and the
     filter's three lines (rate += kr e; integ += rate + ki e; the steer from
     integ, a kp e phase step). As in 10e, the reference runs on the
     library's own prompt each update, and the NCO phase word and its own
     prompts are held to the same bounds from the run's length; a wrong kp,
     ki or kr moves the phase by orders. The stimulus is a 1e-4 cycles/sample
     frequency step from the seed, with a ramp, so all three paths act. */
  {
    const size_t    tsamps = 16, nsym = 3000, ns = nsym * tsamps;
    const double    bn = 0.03, f_seed = 0.002;
    float _Complex *rx   = malloc (ns * sizeof (*rx));
    int            *bits = malloc (nsym * sizeof (*bits));
    float _Complex *got  = malloc (nsym * sizeof (*got));
    make_signal (rx, bits, nsym, tsamps, f_seed + 1e-4, 4e-9, 0.7f, 6063u);
    dp_costas_state_t *c = dp_costas_create (bn, 0.707, f_seed, tsamps, 0.0);
    DP_REQUIRE (c != NULL && rx && bits && got);
    DP_REQUIRE (dp_costas_set_order (c, 3, bn) == DP_OK);
    /* The reference's gains: the scale whose loop has noise bandwidth bn. */
    const double sa = STATE_FILTER_SHAPE_A, sb = STATE_FILTER_SHAPE_B;
    double       lo = 0.0, hi = 0.5;
    for (int it = 0; it < 200; it++)
      {
        const double th = 0.5 * (lo + hi);
        const double bw = dp_state_filter_loop_bandwidth (
            3, sb * th, sa * th * th, th * th * th, 0.0, 1.0);
        if (isfinite (bw) && bw < bn)
          lo = th;
        else
          hi = th;
      }
    const double th = 0.5 * (lo + hi);
    const double kp = sb * th, ki = sa * th * th, kr = th * th * th;
    DP_CHECK_MSG (fabs (kp / c->sf.kp - 1.0) < 1e-9
                      && fabs (ki / c->sf.ki - 1.0) < 1e-9
                      && fabs (kr / c->sf.kr - 1.0) < 1e-9,
                  "order 3's gains are the ones whose bandwidth is bn");
    dp_lo_state_t nco;
    dp_lo_init (&nco, f_seed);
    double integ = f_seed * 2.0 * M_PI * (double)tsamps, rate = 0.0;
    float _Complex acc = 0.0f;
    double xmax = 0.0, dprompt = 0.0;
    for (size_t i = 0; i < ns; i++)
      if (cabsf (rx[i]) > xmax)
        xmax = cabsf (rx[i]);
    size_t       k     = 0;
    const size_t n_got = dp_costas_steps (c, rx, ns, got, nsym);
    for (size_t i = 0; i < ns; i++)
      {
        acc += rx[i] * conjf (lo_step (&nco));
        if ((i + 1) % tsamps)
          continue;
        DP_REQUIRE (k < n_got);
        const float _Complex mine = acc / (float)tsamps;
        const double d
            = cabs ((double _Complex)mine - (double _Complex)got[k]);
        if (d > dprompt)
          dprompt = d;
        const float _Complex P = got[k] * (float)tsamps;
        const float  re = crealf (P), im = cimagf (P);
        const float  a = cabsf (P) + COSTAS_EPS;
        const double e = (double)(((re >= 0.0f) ? im : -im) / a);
        rate += kr * e;
        integ += rate + ki * e;
        dp_lo_set_norm_freq (&nco, integ / (double)tsamps / (2.0 * M_PI));
        nco.phase += nco_norm_phase_to_word ((kp * e) / (2.0 * M_PI));
        k++;
        acc = 0.0f;
      }
    DP_CHECK (k == n_got);
    const double n        = (double)k;
    const double b_counts = n * (double)(tsamps + 1);
    const double b_prompt = 0x1p-23 * xmax * (double)(tsamps + 2)
                            + xmax * 2.0 * M_PI * b_counts / 4294967296.0;
    const double d_counts = fabs ((double)(int32_t)(c->nco.phase - nco.phase));
    DP_CHECK_MSG (d_counts <= b_counts,
                  "order 3's NCO phase is its recurrence's");
    DP_CHECK_MSG (dprompt <= b_prompt, "order 3's prompts are its "
                                       "recurrence's");
    if (d_counts > b_counts || dprompt > b_prompt
        || getenv ("DP_COSTAS_PIN_DIAG"))
      DP_TEST_ERR ("10g: phase %.0f of %.0f counts, "
                   "prompt %.3e of %.3e\n",
                   d_counts, b_counts, dprompt, b_prompt);
    dp_costas_destroy (c);
    free (rx);
    free (bits);
    free (got);
  }

  /* 10b. The switch: what carries, what does not move. */
  {
    const size_t    tsamps = 16, nsym = 3000;
    float _Complex *rx   = malloc (nsym * tsamps * sizeof (*rx));
    int            *bits = malloc (nsym * sizeof (*bits));
    float _Complex *out  = malloc (nsym * sizeof (*out));
    float _Complex *ref  = malloc (nsym * sizeof (*ref));
    make_signal (rx, bits, nsym, tsamps, 0.002, 0.0, 0.0f, 31u);
    dp_costas_state_t *c = dp_costas_create (0.05, 0.707, 0.0, tsamps, 0.0);
    DP_REQUIRE (c != NULL);
    const size_t half = (nsym / 2) * tsamps;
    (void)dp_costas_steps (c, rx, half, out, nsym);
    const double   integ2 = c->lf.integ, f2 = dp_costas_get_norm_freq (c);
    const uint32_t ph2 = c->nco.phase;
    DP_CHECK (dp_costas_get_order (c) == 2);
    DP_CHECK (dp_costas_get_norm_rate (c) == 0.0);
    /* 2 to 3: the estimate carries, the rate starts at 0, the NCO and the
       lock are untouched. */
    const double lock2 = c->lock_metric;
    DP_CHECK (dp_costas_set_order (c, 3, 0.02) == DP_OK);
    DP_CHECK (dp_costas_get_order (c) == 3);
    DP_CHECK (c->sf.integ == integ2 && c->sf.rate == 0.0);
    DP_CHECK (dp_costas_get_norm_freq (c) == f2 && c->nco.phase == ph2);
    DP_CHECK (c->lock_metric == lock2);
    DP_CHECK (dp_costas_get_bn (c) == 0.02 && c->sf.bn == 0.02);
    /* a seeded rate is the rate held */
    DP_CHECK (dp_costas_set_norm_rate (c, 1e-9) == DP_OK);
    DP_CHECK (fabs (dp_costas_get_norm_rate (c) - 1e-9) < 1e-21);
    /* Run at order 3 so its estimate moves off the one it was given (the
       order-2 filter, inactive, keeps the old one), then 3 to 2: the
       estimate carries back; the rate is dropped. */
    (void)dp_costas_steps (c, rx + half, 200 * tsamps, out, nsym);
    const double integ3 = c->sf.integ;
    DP_REQUIRE (integ3 != c->lf.integ);
    DP_CHECK (dp_costas_set_order (c, 2, 0.05) == DP_OK);
    DP_CHECK (c->lf.integ == integ3 && dp_costas_get_norm_rate (c) == 0.0);
    dp_costas_destroy (c);

    /* 2 to 3 to 2 with no update between is the order-2 loop: same
       output, bit for bit, from there on. */
    dp_costas_state_t *a = dp_costas_create (0.05, 0.707, 0.0, tsamps, 0.0);
    dp_costas_state_t *b = dp_costas_create (0.05, 0.707, 0.0, tsamps, 0.0);
    DP_REQUIRE (a && b);
    (void)dp_costas_steps (a, rx, half, out, nsym);
    (void)dp_costas_steps (b, rx, half, ref, nsym);
    DP_CHECK (dp_costas_set_order (b, 3, 0.05) == DP_OK);
    DP_CHECK (dp_costas_set_order (b, 2, 0.05) == DP_OK);
    size_t na = dp_costas_steps (a, rx + half, half, out, nsym);
    size_t nb = dp_costas_steps (b, rx + half, half, ref, nsym);
    DP_CHECK (na == nb && memcmp (out, ref, na * sizeof *out) == 0);
    dp_costas_destroy (a);
    dp_costas_destroy (b);
    free (rx);
    free (bits);
    free (out);
    free (ref);
  }

  /* 10f. The entry points at order 3 (docs/design/costas.md 3.1, 3.2). */
  {
    const size_t    tsamps = 16, nsym = 600;
    const double    ramp = 2e-8, bn = 0.05, f_seed = 3e-4;
    float _Complex *rx   = malloc (nsym * tsamps * sizeof (*rx));
    int            *bits = malloc (nsym * sizeof (*bits));
    float _Complex sym;
    DP_REQUIRE (rx && bits);
    make_signal (rx, bits, nsym, tsamps, f_seed, ramp, 0.0f, 515u);
    dp_costas_state_t *c = dp_costas_create (bn, 0.707, f_seed, tsamps, 0.0);
    DP_REQUIRE (c && dp_costas_set_order (c, 3, bn) == DP_OK);
    DP_REQUIRE (dp_costas_steps (c, rx, nsym * tsamps, &sym, 1) >= 1);
    DP_REQUIRE (c->sf.rate != 0.0 && dp_costas_get_norm_freq (c) != f_seed);

    /* get_nco_freq reads the ACTIVE filter's kp: at order 3 the third-order
       gain, which differs from the retained order-2 one. */
    DP_REQUIRE (c->sf.kp != c->lf.kp && c->last_error != 0.0);
    DP_CHECK (dp_costas_get_nco_freq (c)
              == c->nco.norm_freq
                     + c->sf.kp * c->last_error
                           / (2.0 * M_PI * (double)tsamps));
    /* Negative control: the order-2 gain's command is not what it returns. */
    DP_CHECK (dp_costas_get_nco_freq (c)
              != c->nco.norm_freq
                     + c->lf.kp * c->last_error
                           / (2.0 * M_PI * (double)tsamps));

    /* set_order 3 to 3: a retune that keeps both estimates. */
    {
      const double integ = c->sf.integ, rate = c->sf.rate;
      DP_CHECK (dp_costas_set_order (c, 3, 0.04) == DP_OK);
      DP_CHECK (dp_costas_get_order (c) == 3 && c->sf.bn == 0.04
                && dp_costas_get_bn (c) == 0.04);
      DP_CHECK (c->sf.integ == integ && c->sf.rate == rate);
    }
    /* configure at order 3: both filters retuned, estimates kept. */
    {
      const double integ = c->sf.integ, rate = c->sf.rate;
      DP_CHECK (dp_costas_configure (c, 0.03, 0.8) == DP_OK);
      DP_CHECK (c->sf.bn == 0.03 && c->lf.bn == 0.03 && c->lf.zeta == 0.8);
      DP_CHECK (c->sf.integ == integ && c->sf.rate == rate);
      DP_CHECK (dp_costas_get_order (c) == 3);
    }
    /* reset at order 3: keeps the order, rewinds the frequency estimate to
       the create-time seed, zeroes the rate. */
    dp_costas_reset (c);
    DP_CHECK (dp_costas_get_order (c) == 3 && c->sf.rate == 0.0);
    DP_CHECK (dp_costas_get_norm_freq (c) == f_seed);
    DP_CHECK (c->sf.integ == f_seed * 2.0 * M_PI * (double)tsamps);
    /* set_norm_freq at order 3: the same, at a new seed. */
    DP_REQUIRE (dp_costas_steps (c, rx, 200 * tsamps, &sym, 1) >= 1);
    DP_REQUIRE (c->sf.rate != 0.0);
    dp_costas_set_norm_freq (c, -2e-4);
    DP_CHECK (dp_costas_get_order (c) == 3 && c->sf.rate == 0.0);
    DP_CHECK (dp_costas_get_norm_freq (c) == -2e-4);
    dp_costas_destroy (c);

    /* A switch in the middle of an integrate-and-dump: the dump in progress
       and the NCO are untouched, so the next prompt is the one a loop that
       never switched emits; the switch acts from the update after. */
    dp_costas_state_t *a = dp_costas_create (bn, 0.707, f_seed, tsamps, 0.0);
    dp_costas_state_t *b = dp_costas_create (bn, 0.707, f_seed, tsamps, 0.0);
    DP_REQUIRE (a && b);
    /* Each call's output buffer holds the max_out it passes: 50 whole
       symbols before the switch into 64 slots, then one prompt each. */
    enum
    {
      HEAD = 50
    };
    float _Complex oa[64], ob[64], pa, pb;
    const size_t mid = HEAD * tsamps + tsamps / 2;
    DP_CHECK (dp_costas_steps (a, rx, mid, oa, 64) == HEAD);
    DP_CHECK (dp_costas_steps (b, rx, mid, ob, 64) == HEAD);
    DP_CHECK (memcmp (oa, ob, HEAD * sizeof oa[0]) == 0); /* twins so far */
    DP_REQUIRE (a->acc_n == tsamps / 2);
    const float _Complex acc = a->acc;
    DP_CHECK (dp_costas_set_order (a, 3, bn) == DP_OK);
    DP_CHECK (a->acc == acc && a->acc_n == tsamps / 2);
    DP_CHECK (a->nco.phase == b->nco.phase
              && a->nco.phase_inc == b->nco.phase_inc);
    DP_CHECK (dp_costas_steps (a, rx + mid, tsamps / 2, &pa, 1) == 1);
    DP_CHECK (dp_costas_steps (b, rx + mid, tsamps / 2, &pb, 1) == 1);
    DP_CHECK_MSG (memcmp (&pa, &pb, sizeof pa) == 0,
                  "a mid-dump switch leaves the dump in progress alone");
    dp_costas_destroy (a);
    dp_costas_destroy (b);
    free (rx);
    free (bits);
  }

  /* 10c. Refusals change nothing. */
  {
    dp_costas_state_t *c = dp_costas_create (0.05, 0.707, 0.001, 16, 0.0);
    DP_REQUIRE (c != NULL);
    dp_costas_state_t before = *c;
    DP_CHECK (dp_costas_set_order (c, 4, 0.05) == DP_ERR_INVALID);
    DP_CHECK (dp_costas_set_order (c, 3, 0.2) == DP_ERR_INVALID); /* bn*t */
    DP_CHECK (dp_costas_set_order (c, 3, NAN) == DP_ERR_INVALID);
    DP_CHECK (dp_costas_set_norm_rate (c, 1e-9) == DP_ERR_INVALID); /* o2 */
    DP_CHECK (memcmp (&before, c, sizeof before) == 0);
    /* the FLL assist and order 3 do not combine, in either order */
    DP_CHECK (dp_costas_set_bn_fll (c, 0.01) == DP_OK);
    before = *c;
    DP_CHECK (dp_costas_set_order (c, 3, 0.05) == DP_ERR_INVALID);
    DP_CHECK (memcmp (&before, c, sizeof before) == 0);
    DP_CHECK (dp_costas_set_bn_fll (c, 0.0) == DP_OK);
    DP_CHECK (dp_costas_set_order (c, 3, 0.05) == DP_OK);
    before = *c;
    DP_CHECK (dp_costas_set_bn_fll (c, 0.01) == DP_ERR_INVALID);
    DP_CHECK (dp_costas_configure (c, 0.2, 0.707) == DP_ERR_INVALID);
    DP_CHECK (memcmp (&before, c, sizeof before) == 0);
    dp_costas_destroy (c);
  }

  /* 10d. The blob at order 3: a mid-stream split resumes bit for bit, and
     a snapshot no live loop could hold is refused untouched. */
  {
    const size_t    tsamps = 16, nsym = 2000;
    float _Complex *rx   = malloc (nsym * tsamps * sizeof (*rx));
    int            *bits = malloc (nsym * sizeof (*bits));
    float _Complex *out  = malloc (nsym * sizeof (*out));
    float _Complex *ref  = malloc (nsym * sizeof (*ref));
    make_signal (rx, bits, nsym, tsamps, 0.001, 1e-8, 0.3f, 8u);
    dp_costas_state_t *a = dp_costas_create (0.04, 0.707, 0.0, tsamps, 0.0);
    dp_costas_state_t *b = dp_costas_create (0.04, 0.707, 0.0, tsamps, 0.0);
    DP_REQUIRE (a && b);
    DP_REQUIRE (dp_costas_set_order (a, 3, 0.04) == DP_OK);
    const size_t half = (nsym / 2) * tsamps;
    (void)dp_costas_steps (a, rx, half, out, nsym);
    unsigned char *blob = malloc (dp_costas_state_bytes (a));
    DP_REQUIRE (blob != NULL);
    dp_costas_get_state (a, blob);
    DP_CHECK (dp_costas_set_state (b, blob) == DP_OK);
    DP_CHECK (dp_costas_get_order (b) == 3);
    size_t na = dp_costas_steps (a, rx + half, half, out, nsym);
    size_t nb = dp_costas_steps (b, rx + half, half, ref, nsym);
    DP_CHECK (na == nb && memcmp (out, ref, na * sizeof *out) == 0);
    free (blob);

    const size_t  H      = sizeof (dp_state_hdr_t);
    const int32_t four   = 4;
    const double  bn_off = 0.03, fll[2] = { 0.01, 0.04 };
    DP_STATE_FORGE_TEST (
        dp_costas, a, COSTAS_STATE_MAGIC, COSTAS_STATE_VERSION,
        H + offsetof (dp_costas_state_t, order), &four, sizeof four);
    /* the active filter's bn is not the retained one */
    DP_STATE_FORGE_TEST (dp_costas, a, COSTAS_STATE_MAGIC,
                         COSTAS_STATE_VERSION,
                         H + offsetof (dp_costas_state_t, sf)
                             + offsetof (dp_state_filter_state_t, bn),
                         &bn_off, sizeof bn_off);
    /* an FLL assist at order 3 (bn_fll, k_fll kept consistent) */
    DP_STATE_FORGE_TEST (
        dp_costas, a, COSTAS_STATE_MAGIC, COSTAS_STATE_VERSION,
        H + offsetof (dp_costas_state_t, bn_fll), fll, sizeof fll);
    /* Every branch of costas_state_ok, each a finite wrong blob: order 0
       on a seeded loop (only the all-zero Costas is order 0); order 3 with
       the all-zero filter (a selected filter was initialised); a filter
       not at one update (t != 1); the pad after the order not zero. */
    {
      const int                            zero_order = 0, one = 1;
      const double                         t2 = 2.0;
      static const dp_state_filter_state_t zsf;
      DP_STATE_FORGE_TEST (dp_costas, a, COSTAS_STATE_MAGIC,
                           COSTAS_STATE_VERSION,
                           H + offsetof (dp_costas_state_t, order),
                           &zero_order, sizeof zero_order);
      DP_STATE_FORGE_TEST (
          dp_costas, a, COSTAS_STATE_MAGIC, COSTAS_STATE_VERSION,
          H + offsetof (dp_costas_state_t, sf), &zsf, sizeof zsf);
      /* a filter valid on its own at t = 2 (gains, bn consistent), so only
         Costas's one-update rule refuses it */
      {
        dp_state_filter_state_t sf2;
        dp_state_filter_init (&sf2, a->bn, t2);
        DP_REQUIRE (dp_state_filter_state_ok (&sf2) && sf2.bn == a->bn);
        DP_STATE_FORGE_TEST (
            dp_costas, a, COSTAS_STATE_MAGIC, COSTAS_STATE_VERSION,
            H + offsetof (dp_costas_state_t, sf), &sf2, sizeof sf2);
      }
      DP_STATE_FORGE_TEST (
          dp_costas, a, COSTAS_STATE_MAGIC, COSTAS_STATE_VERSION,
          H + offsetof (dp_costas_state_t, _pad_order), &one, sizeof one);
      /* At order 2 the inactive filter is the all-zero one or a valid
         one: a filter whose bn its gains do not match is neither. */
      dp_costas_state_t *o2 = dp_costas_create (0.04, 0.707, 0.0, 16, 0.0);
      DP_REQUIRE (o2 != NULL);
      const double bad_bn = 0.07;
      DP_REQUIRE (dp_costas_set_order (o2, 3, 0.04) == DP_OK);
      DP_REQUIRE (dp_costas_set_order (o2, 2, 0.04) == DP_OK);
      DP_STATE_FORGE_TEST (dp_costas, o2, COSTAS_STATE_MAGIC,
                           COSTAS_STATE_VERSION,
                           H + offsetof (dp_costas_state_t, sf)
                               + offsetof (dp_state_filter_state_t, bn),
                           &bad_bn, sizeof bad_bn);
      dp_costas_destroy (o2);
    }
    dp_costas_destroy (a);
    dp_costas_destroy (b);
    free (rx);
    free (bits);
    free (out);
    free (ref);
  }

  DP_TEST_END ("test_costas_core");
}
