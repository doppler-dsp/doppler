/**
 * @file mpsk_ber_common.h
 * @brief Shared stimulus and measurement loop for the two M-PSK BER
 *        validators.
 *
 * `mpsk_receiver_ber.c` (complex baseband) and `mpsk_receiver_real_ber.c`
 * (real IF) measure the same thing on the same signal model through two front
 * ends, so the stimulus and the measurement live here once and each validator
 * is just a sweep over it. A second copy would drift, and the two would stop
 * being comparable — which is the entire point of running both.
 *
 * Everything statistical is `native/tests/dp_ber_test.h`: the settled window,
 * inverse binomial sampling, the exact confidence interval, the Pfa-gated
 * alignment, and the cross-checks against EVM / M2M4 / theory. Nothing here
 * re-derives any of it. In particular there is **no lag search and no rotation
 * search** — the alignment is detected from a known marker, so the reported
 * rate is a measurement rather than a minimisation over the answer.
 *
 * ## Operating point
 *
 * Every measurement anchors at **SER = 1e-3** (`dp_ber_esn0_db_for_ser`),
 * which per M is 6.8 / 10.3 / 15.7 dB. Anchoring at a fixed error rate rather
 * than a fixed Es/N0 asks "does this receiver meet its bound" at the same
 * place on the curve for every constellation; a fixed Es/N0 asks a different
 * question of each.
 *
 * ## Noise conventions — the two differ, and both matter
 *
 * Both set the **matched-filter-output** Es/N0, not an input SNR.
 *
 *   - **complex baseband.** A rectangular symbol of amplitude `A` over `sps`
 *     samples through the length-`sps` boxcar matched filter comes out with
 *     amplitude `sps*A`, while `sps` complex noise samples of per-quadrature
 *     variance `sigma^2` sum to per-quadrature variance `sps*sigma^2`. The
 *     output SNR is `sps*A^2 / (2*sigma^2)`, so `sigma =
 * A*sqrt(sps/(2*esn0))`.
 *   - **real IF.** Half the power of a real passband signal sits at the
 *     negative frequency and is discarded by the R2C halfband, so the symbol
 *     energy that survives is `A^2*sps/2` and the REAL noise variance is
 *     `A^2*sps/(4*esn0)`.
 *
 * A wrong convention here does not look like a wrong convention: it looks like
 * one path beating the matched-filter bound, or like implementation loss that
 * is not there. dp_ber_report()'s sanity gate catches both (it refuses an EVM
 * below `-(Es/N0)` and an M2M4 that disagrees with the stated Es/N0), which is
 * how these two formulas are checked on every run rather than trusted.
 *
 * ## Amplitude 0.5, not 1.0
 *
 * A cascade that plans a CIC bounds its input to +-1.0 and clips silently past
 * it, costing ~25 dB of EVM that no lock metric reveals. Both validators
 * assert `get_clipped() == 0` so a stimulus that outgrows the front end is a
 * loud failure rather than a mysterious floor.
 *
 * ## The symbol source is a plain PRNG, deliberately
 *
 * **Do not "fix" this to `pn_core`.** An MLS of register length L contains a
 * run of L identical bits, so at 1 bit/symbol BPSK sees L transition-free
 * symbols and the Gardner TED stalls — measured on the C receiver tests as 3
 * output symbols instead of 5998. A BER validator needs bounded RUN LENGTH,
 * which an MLS does not provide; i.i.d. uniform symbols do.
 */
#ifndef MPSK_BER_COMMON_H
#define MPSK_BER_COMMON_H

#include "dp_ber_test.h"
#include "dp_rng_test.h"
#include "mpsk_receiver/mpsk_receiver_core.h"
#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/** @brief Transmit amplitude; see the file docstring on CIC clipping. */
#define MPSK_BER_AMP 0.5

/** @brief Symbols per burst. Must comfortably exceed the settling budget
 * (3000 symbols at the bandwidths used here) or a burst contributes nothing
 * but transient — dp_ber_settle() would return a window start past the end. */
#define MPSK_BER_NSYM 24000u

/** @brief Cap on bursts, so a receiver that is working perfectly terminates.
 * Inverse binomial sampling stops on the ERROR count; this is the other end.
 */
#define MPSK_BER_MAX_BURSTS 60

/** @brief One measurement geometry. */
typedef struct
{
  int    real;       /**< 0 = complex baseband, 1 = real IF.            */
  int    m;          /**< Constellation order.                          */
  double sps;        /**< Samples per symbol at the receiver's input.   */
  size_t m_out;      /**< Terminal outputs per symbol.                  */
  double fc;         /**< Carrier, cycles/sample at the input rate.     */
  double foff;       /**< Offset the carrier loop must acquire.         */
  double bn_timing;  /**< Timing loop noise bandwidth, per symbol.      */
  double bn_carrier; /**< Carrier loop noise bandwidth, per symbol.     */
} mpsk_ber_cfg_t;

/**
 * @brief Run one burst: build the stimulus, demodulate, capture the locks.
 *
 * Steps SAMPLE BY SAMPLE through the composition API rather than calling
 * `steps()` on the whole block, because the settling gate needs the receiver's
 * own lock indicators PER SYMBOL and the block API only exposes their final
 * value. That is not an optimisation to undo: `max(budget, carrier lock)` is
 * the difference between measuring the steady state and measuring the
 * acquisition transient.
 *
 * @param c        Geometry.
 * @param esn0_db  Matched-filter-output Es/N0.
 * @param seed     PRNG seed; vary it per burst.
 * @param nsym     Symbols to transmit.
 * @param truth    Out: `nsym` transmitted symbol indices (0..m-1).
 * @param out      Out: recovered symbols, capacity `nsym`.
 * @param lock_c   Out: per-recovered-symbol carrier lock flag.
 * @param clipped  Out: non-zero if the front end clipped.
 * @return         Symbols recovered.
 */
static inline size_t
mpsk_ber_burst (const mpsk_ber_cfg_t *c, double esn0_db, uint32_t seed,
                size_t nsym, uint8_t *truth, float complex *out,
                unsigned char *lock_c, int *clipped)
{
  double   esn0 = pow (10.0, esn0_db / 10.0);
  double   phi0 = mpsk_phi0 (c->m);
  size_t   isps = (size_t)c->sps;
  uint32_t st   = seed ? seed : 1u;
  size_t   nout = 0;
  /* Per the file docstring: the two front ends see different fractions of the
     transmitted energy, so the same Es/N0 is a different noise level. */
  double sigma = c->real ? MPSK_BER_AMP * sqrt (c->sps / (4.0 * esn0))
                         : MPSK_BER_AMP * sqrt (c->sps / (2.0 * esn0));

  /* One type, one set of accessors: the front end is a CONSTRUCTOR choice
     now, not a second state_t to cast a void* to. Every argument below is
     identical between the two calls, which is the collapse's thesis stated as
     code (docs/design/mpsk.md §8). */
  mpsk_receiver_state_t *rx
      = c->real ? mpsk_receiver_create_real (
                      c->m, c->sps, c->m_out, MPSK_RX_PULSE_IANDD, 0.35, 8,
                      c->bn_carrier, 0.707, c->bn_timing, 0.3, c->fc - c->foff,
                      0, MPSK_RX_NUM_PHASES, 1, MPSK_RX_AGC_BW_RATIO)
                : mpsk_receiver_create (
                      c->m, c->sps, c->m_out, MPSK_RX_PULSE_IANDD, 0.35, 8,
                      c->bn_carrier, 0.707, c->bn_timing, 0.3, c->fc - c->foff,
                      0, MPSK_RX_NUM_PHASES, 1, MPSK_RX_AGC_BW_RATIO);
  if (!rx)
    return 0;

  for (size_t k = 0; k < nsym; k++)
    {
      int    ki = (int)(dp_xs32 (&st) % (uint32_t)c->m);
      double th = 2.0 * MPSK_PI * (double)ki / (double)c->m + phi0;
      double sr = MPSK_BER_AMP * cos (th), si = MPSK_BER_AMP * sin (th);
      truth[k] = ki;
      for (size_t j = 0; j < isps; j++)
        {
          size_t        n  = k * isps + j;
          double        ph = 2.0 * MPSK_PI * c->fc * (double)n;
          float complex y;
          int           got;
          if (c->real)
            {
              /* Re{(sr + j si) e^{j ph}} — what an ADC behind an analogue
                 mixer actually delivers. */
              float x = (float)(sr * cos (ph) - si * sin (ph)
                                + sigma * dp_gauss (&st));
              got     = mpsk_receiver_step_real_ted (rx, x, &y,
                                                     RATESYNC_TED_GARDNER);
            }
          else
            {
              double        re   = sr * cos (ph) - si * sin (ph);
              double        im   = sr * sin (ph) + si * cos (ph);
              double        n_re = dp_gauss (&st);
              double        n_im = dp_gauss (&st);
              float complex x    = (float)(re + sigma * n_re)
                                   + (float)(im + sigma * n_im) * I;
              got = mpsk_receiver_step_ted (rx, x, &y, RATESYNC_TED_GARDNER);
            }
          if (got && nout < nsym)
            {
              out[nout]    = y;
              lock_c[nout] = (unsigned char)mpsk_receiver_get_locked (rx);
              nout++;
            }
        }
    }

  *clipped = mpsk_receiver_get_clipped (rx);
  mpsk_receiver_destroy (rx);
  return nout;
}

/** @brief Result of a full inverse-binomial measurement at one operating
 * point. */
typedef struct
{
  dp_ber_report_t rep;       /**< The gated report; print with dp_ber_print. */
  int             bursts;    /**< Bursts consumed.                           */
  int             clipped;   /**< Any burst clipped the front end.           */
  int             unsettled; /**< Any burst never reached a settled window.  */
} mpsk_ber_result_t;

/**
 * @brief Measure the error rate at one operating point, stopping on ERRORS.
 *
 * Runs bursts until `target_errors` have been counted (or the burst cap is
 * hit), accumulating across them. The precision of the result is then fixed by
 * the error target alone — `1/sqrt(r)` relative — rather than by the rate
 * being measured, which is the whole reason to sample this way: 20 000 symbols
 * at SER 1e-3 yields ~20 errors and ~22% relative error, big enough to read as
 * real seed-to-seed variation in the receiver.
 *
 * Each burst independently: derives its settled window from the receiver's own
 * carrier lock and handover plus the analytic budget, detects its alignment
 * against a marker taken from truth well inside that window, and scores only
 * what is left. A burst that never settles contributes nothing and is counted
 * in `unsettled` rather than silently averaged in.
 *
 * @note The timing detector is not exposed at the object level (only the
 *       carrier lock and the handover are), so its `5/bn_timing` term reaches
 *       the window through dp_ber_settle()'s analytic budget rather than
 *       through a measured indicator. That is the documented NULL case.
 */
static inline mpsk_ber_result_t
mpsk_ber_measure (const mpsk_ber_cfg_t *c, double esn0_db,
                  unsigned long target_errors, uint32_t seed0)
{
  mpsk_ber_result_t r;
  dp_ber_t          acc;
  size_t            nsym  = MPSK_BER_NSYM;
  uint8_t          *truth = malloc (nsym);
  float complex    *out   = malloc (nsym * sizeof (*out));
  unsigned char    *lc    = malloc (nsym);
  size_t            lo = 0, hi = 0;
  int               settled_any = 0;
  /* The alignment of the LAST scored burst, carried out so dp_ber_report()
     can see that one was detected. Passing NULL instead makes the report
     short-circuit its whole sanity chain on `aligned == 0` and never evaluate
     the EVM / theory / M2M4 gates at all -- which is a silently vacuous
     verdict, exactly what this harness exists to prevent. Every burst that
     contributed symbols passed the same Pfa gate, so any one of them is a
     faithful representative. */
  dp_ber_sync_t last_sync;

  r.bursts    = 0;
  r.clipped   = 0;
  r.unsettled = 0;
  dp_ber_init (&acc, c->m, target_errors);
  if (!truth || !out || !lc)
    {
      free (truth);
      free (out);
      free (lc);
      r.rep = dp_ber_report (&acc, esn0_db, NULL, 0, 0, 0, DP_BER_CONF);
      return r;
    }

  while (!dp_ber_enough (&acc) && r.bursts < MPSK_BER_MAX_BURSTS)
    {
      int    clip = 0, ok = 0;
      size_t n
          = mpsk_ber_burst (c, esn0_db, seed0 + 7919u * (uint32_t)r.bursts,
                            nsym, truth, out, lc, &clip);
      r.bursts++;
      r.clipped |= clip;
      if (n < 1000)
        {
          r.unsettled++;
          continue;
        }
      {
        size_t settle
            = dp_ber_settle (c->bn_timing, c->bn_carrier, NULL, lc, n, &ok);
        dp_ber_marker_t mk;
        dp_ber_sync_t   sy;
        if (!ok || settle + DP_BER_LAG_SPAN + DP_BER_SYNC_SYMS + 500 >= n)
          {
            r.unsettled++;
            continue;
          }
        /* The marker sits a full lag-span past the settling point, so it is
           inside the settled part of the record whatever the (still unknown)
           lag turns out to be, and scoring starts after it ends — the symbols
           that fixed the alignment are never also scored. */
        mk.sym    = NULL;
        mk.n      = DP_BER_SYNC_SYMS;
        mk.t0     = settle + (size_t)DP_BER_LAG_SPAN;
        mk.period = 0;
        mk.reps   = 0;
        sy = dp_ber_sync (out, n, truth, nsym, &mk, c->m, DP_BER_LAG_SPAN,
                          DP_BER_SYNC_PFA);
        if (!sy.ok)
          {
            r.unsettled++;
            continue;
          }
        {
          long e = (long)(mk.t0 + mk.n) - sy.lag;
          lo     = (e > 0 && (size_t)e > settle) ? (size_t)e : settle;
          hi     = n;
          dp_ber_score (&acc, out, lo, hi, truth, nsym, &mk, &sy);
          last_sync   = sy;
          settled_any = 1;
        }
      }
    }

  free (truth);
  free (out);
  free (lc);
  r.rep = dp_ber_report (&acc, esn0_db, settled_any ? &last_sync : NULL, lo,
                         hi, settled_any, DP_BER_CONF);
  return r;
}

#endif /* MPSK_BER_COMMON_H */
