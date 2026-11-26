/**
 * @file mpsk_receiver_ber.c
 * @brief Symbol-error-rate validation for the complex-baseband M-PSK receiver.
 *
 * Drives MpskReceiver with a rectangular (I&D-matched) BPSK/QPSK/8PSK signal
 * at a carrier offset and measures how far its symbol error rate sits from the
 * coherent bound. The receiver acquires the carrier non-data-aided (M-th
 * power) and recovers symbol timing (Gardner); nothing is given to it but the
 * samples.
 *
 * Usage:
 *   validate_mpsk_receiver_ber           full report at every M
 *   validate_mpsk_receiver_ber --check   the same, asserting the loss bound
 *   validate_mpsk_receiver_ber --pullin  symbol errors per window from the
 *                                        receiver's FIRST output, on both
 *                                        rectangle banks (see "--pullin")
 *
 * ## --pullin: what the settled window leaves out
 *
 * Every rate this validator and the MpskReceiver report quote is scored from
 * a settled point (`dp_ber_settle_syms`, 2000 symbols at bn 0.01/0.01), so
 * the errors the receiver makes while its timing loop pulls in are never
 * counted. The matched bank's rectangle arms changed under that loop in
 * #3362/#3364 (sampled taps -> coverage-weighted taps, `rc_tap_w`), and on
 * the async DSSS receiver the change shows up only in that transient. This
 * mode counts it: per window of PULLIN_W output symbols from output 0, the
 * symbol errors pooled over PULLIN_DRAWS draws, each draw a fresh symbol
 * stream, noise, carrier phase and a uniform fractional timing offset (so
 * the loop starts from a different error every draw).
 *
 * The two banks are held without a source edit. "corrected" is the bank as
 * built. "sampled" is the bank as it was before #3364: the half-open
 * rectangle sampled at each tap, same layout and the same single 1/e0
 * scale, written over the terminal stage's bank in place. The harness first
 * rebuilds the CORRECTED bank from rc_tap_w's formula and refuses to run if
 * it does not reproduce the as-built bank to float rounding, so the layout
 * the sampled bank is written in is the one the cascade really uses. The
 * root-raised cosine is unchanged by #3364 (rc_tap_w returns the sampled
 * pulse for it), so it is measured on the as-built bank only, as the
 * control.
 *
 * Alignment is the meter's, unchanged: a 256-symbol marker at the settling
 * point, detected under its false-alarm gate, fixes the lag and the absolute
 * rotation, and every other output is scored under that one alignment. A
 * draw that is still slipping in the head is therefore counted as errors,
 * which is what a caller would see. The output is raw counts; the report's
 * validate.py derives every figure from them.
 *
 * ## What changed, and why the old numbers are not comparable
 *
 * This validator used to measure a fixed 40 000 symbols, score them over a
 * `nout/4 .. 7*nout/8` window, and take the **minimum error count over a
 * +-30 lag and M-fold rotation search**. Every one of those is a documented
 * way to produce a confident wrong number:
 *
 *   - a window pinned to a FRACTION of the record measures the acquisition
 *     transient whenever settling is longer than the fraction (here the
 *     settling budget is 3000 symbols against a 10 000-symbol quarter, so it
 *     was inside it for the slower geometries);
 *   - a `min over (lag, rotation)` is an optimisation over the answer, not a
 *     measurement of the receiver. It false-PASSES on a lucky alignment and
 *     false-FLOORS when the true lag falls outside the span — this project has
 *     shipped both, including a committed "~12 dB floor" that was really 5 dB;
 *   - a fixed symbol count makes the precision depend on the very rate being
 *     measured. 40 000 symbols at SER 1e-3 is ~40 errors, ~16% relative, which
 *     reads as receiver variation and is not.
 *
 * All three are now the harness's job (`native/tests/dp_ber_test.h`): the
 * window comes from the receiver's own carrier lock and handover plus the
 * analytic loop budget, the alignment is DETECTED against a marker under a
 * false-alarm gate rather than searched, and the run stops on a fixed ERROR
 * count so the interval width is `1/sqrt(r)` regardless of the rate. The
 * result carries a 99% confidence interval and is cross-checked against EVM,
 * the blind M2M4 Es/N0 and theory before it is called a result at all.
 *
 * ## The operating point
 *
 * Each M is measured at its own **SER = 1e-3** anchor (6.8 / 10.3 / 15.7 dB),
 * so every constellation is asked the same question at the same place on its
 * curve. The gate is stated as an **implementation loss in dB** — the Es/N0
 * theory would need to produce the measured rate, subtracted from the Es/N0
 * actually applied — because a loss in dB is comparable across M and across
 * operating points while a ratio of rates is not.
 *
 * The assertion is on the interval's LOWER limit, so counting noise cannot
 * flake it: we fail only when the receiver is worse than the bound with 99%
 * confidence, not when a draw happened to land badly.
 */
#include "doppler/resamp/resamp_core.h"
#include "doppler/wfm/wfm_dsp.h"
#include "mpsk_ber_common.h"
#include <string.h>

/** @brief Tolerated implementation loss vs the coherent bound, dB. */
#define LOSS_DB 2.0

/** @brief Errors per point. ~7% relative standard error (`1/sqrt(r)`). */
#define TARGET_ERRORS 200uL

/* ── --pullin ───────────────────────────────────────────────────────────── */

/** @brief Symbols transmitted per draw: the settling point (2000) plus the
 *  marker plus a tail long enough to estimate the steady rate. */
#define PULLIN_NSYM 6000u
/** @brief Output symbols per window. */
#define PULLIN_W 50u
/** @brief Windows reported, from output 0. */
#define PULLIN_NWIN 116u
/** @brief Draws pooled per case. */
#define PULLIN_DRAWS 128
/** @brief Loop bandwidths: the MpskReceiver report's (bn 0.01 / 0.01). */
#define PULLIN_BN 0.01
/** @brief Input samples a symbol: the report's. */
#define PULLIN_SPS 8
/** @brief RRC roll-off and one-sided span, transmitter and receiver. */
#define PULLIN_BETA 0.35
#define PULLIN_SPAN 8

enum
{
  BANK_CORRECTED = 0,
  BANK_SAMPLED   = 1
};

/* The terminal stage of the matched cascade: where the rectangle bank lives.
   NULL when the last stage is not a polyphase resampler. */
static resamp_state_t *
pullin_terminal (dp_mpsk_receiver_state_t *rx)
{
  dp_RateConverter_state_t *rc   = rx->fe.c->rc;
  int                       last = rc->n_stages - 1;
  if (last < 0 || rc->stage_types[last] != RC_STAGE_RESAMP)
    return NULL;
  return (resamp_state_t *)rc->stage_ptrs[last];
}

/* One rectangle tap at `ts' symbols from the centre on a grid of `sps' a
   symbol, as each bank weighs it. CORRECTED is rc_tap_w's coverage weight
   (the share of [ts, ts + 1/sps) inside the symbol, times sps); SAMPLED is
   the half-open rectangle sampled at ts, which is what rc_pulse_h returned
   for every tap before #3364. */
static double
pullin_tap (int bank, double ts, double sps)
{
  if (bank == BANK_SAMPLED)
    return (ts >= -0.5 && ts < 0.5) ? 1.0 : 0.0;
  double hi = ts + 1.0 / sps < 0.5 ? ts + 1.0 / sps : 0.5;
  double lo = ts > -0.5 ? ts : -0.5;
  return hi > lo ? (hi - lo) * sps : 0.0;
}

/* Build `bank' (CORRECTED or SAMPLED) in rc_build_bank's layout: arm p lags
   p/num_phases of an input, tap t multiplies x[n-t], support one half
   symbol, one common 1/e0 scale with e0 the sum of arm 0's lag-free
   weights. Returns a malloc'd num_phases x nt bank. */
static float *
pullin_build (int bank, double sps, size_t np, size_t nt)
{
  float *b = calloc (np * nt, sizeof *b);
  if (!b)
    return NULL;
  double e0 = 0.0;
  for (size_t t = 0; t < nt; t++)
    e0 += pullin_tap (bank, -(double)t / sps + 0.5, sps);
  for (size_t p = 0; p < np; p++)
    for (size_t t = 0; t < nt; t++)
      {
        double ts = -(double)t / sps + 0.5 - ((double)p / (double)np) / sps;
        b[p * nt + t] = (float)(pullin_tap (bank, ts, sps) / e0);
      }
  return b;
}

/* Put `bank' into the receiver's terminal stage, after proving the layout:
   the CORRECTED rebuild must equal the as-built bank to float rounding, or
   the cascade is not the one this reconstruction describes (a folded droop
   compensator, a different support) and the A/B would be meaningless.
   Returns 0 on success, -1 on a refusal. */
static int
pullin_set_bank (dp_mpsk_receiver_state_t *rx, int bank, double *max_dev)
{
  resamp_state_t *rs = pullin_terminal (rx);
  if (!rs)
    return -1;
  double sps = rx->fe.c->rc->bank_sps;
  size_t np = rs->num_phases, nt = rs->num_taps;
  float *ref = pullin_build (BANK_CORRECTED, sps, np, nt);
  if (!ref)
    return -1;
  double dev = 0.0;
  for (size_t i = 0; i < np * nt; i++)
    {
      double d = fabs ((double)ref[i] - (double)rs->bank[i]);
      dev      = d > dev ? d : dev;
    }
  free (ref);
  *max_dev = dev;
  if (!(dev < 1e-6))
    return -1;
  if (bank == BANK_CORRECTED)
    return 0;
  float *b = pullin_build (bank, sps, np, nt);
  if (!b)
    return -1;
  memcpy (rs->bank, b, np * nt * sizeof *b);
  free (b);
  return 0;
}

/* One draw's stimulus: `nsym' symbols at PULLIN_SPS, delayed by `d' of a
   symbol, carrier offset `foff' cycles/sample and phase `ph0', AWGN at a
   matched-filter Es/N0 of `esn0_db'. The rectangle is AREA-sampled, so a
   fractional delay produces a fractional transition (a point-sampled
   rectangle could only move by whole samples). The RRC is point-sampled
   from the canonical wfm_rrc_h. Both carry energy A^2*sps a symbol, so one
   noise formula serves both (mpsk_ber_common.h). */
static void
pullin_stimulus (int pulse, int m, const uint8_t *truth, size_t nsym, double d,
                 double foff, double ph0, double esn0_db, uint32_t *st,
                 float complex *x)
{
  const double sps  = (double)PULLIN_SPS;
  const double phi0 = mpsk_phi0 (m);
  const double sigma
      = MPSK_BER_AMP * sqrt (sps / (2.0 * pow (10.0, esn0_db / 10.0)));
  size_t n = nsym * PULLIN_SPS;
  for (size_t i = 0; i < n; i++)
    {
      double complex s = 0.0;
      if (pulse == MPSK_RX_PULSE_IANDD)
        {
          double t0 = (double)i / sps - d, t1 = (double)(i + 1) / sps - d;
          long   k0 = (long)floor (t0), k1 = (long)floor (t1);
          for (long k = k0; k <= k1; k++)
            {
              if (k < 0 || k >= (long)nsym)
                continue;
              double a  = t0 > (double)k ? t0 : (double)k;
              double b  = t1 < (double)(k + 1) ? t1 : (double)(k + 1);
              double th = 2.0 * MPSK_PI * truth[k] / m + phi0;
              s += (b - a) * sps * cexp (I * th);
            }
        }
      else
        {
          double t  = (double)i / sps - d;
          long   kc = (long)floor (t);
          for (long k = kc - PULLIN_SPAN; k <= kc + PULLIN_SPAN + 1; k++)
            {
              if (k < 0 || k >= (long)nsym)
                continue;
              double th = 2.0 * MPSK_PI * truth[k] / m + phi0;
              s += wfm_rrc_h (t - (double)k, PULLIN_BETA) * cexp (I * th);
            }
        }
      s *= MPSK_BER_AMP * cexp (I * (2.0 * MPSK_PI * foff * (double)i + ph0));
      double nr = dp_gauss (st);
      double ni = dp_gauss (st);
      x[i]      = (float)(creal (s) + sigma * nr)
                  + (float)(cimag (s) + sigma * ni) * I;
    }
}

/* One case, every draw: prints one line per window, pooled over draws. */
static int
pullin_case (int bank, int pulse, int m, size_t m_out, double esn0_db,
             uint32_t seed0)
{
  const size_t   nsym             = PULLIN_NSYM;
  const size_t   nsamp            = nsym * PULLIN_SPS;
  uint8_t       *truth            = malloc (nsym);
  float complex *x                = malloc (nsamp * sizeof *x);
  float complex *out              = malloc (nsym * sizeof *out);
  unsigned long  err[PULLIN_NWIN] = { 0 }, sym[PULLIN_NWIN] = { 0 };
  int            refused = 0, clipped = 0, rc = 0;
  double         lock_sum = 0.0, dev = 0.0, bank_sps = 0.0;
  int            lock_n = 0;
  /* Per draw: errors and symbols scored in the HEAD [0, settle) -- what the
     report leaves out -- and in the TAIL past the marker, plus where the
     carrier lock declared (-1: never). Kept per draw so the A/B can be read
     PAIRED: every case runs the same seeds. */
  unsigned long hd_e[PULLIN_DRAWS] = { 0 }, hd_s[PULLIN_DRAWS] = { 0 };
  unsigned long tl_e[PULLIN_DRAWS] = { 0 }, tl_s[PULLIN_DRAWS] = { 0 };
  long          lk[PULLIN_DRAWS];
  int           ok[PULLIN_DRAWS] = { 0 };
  const size_t  settle = (size_t)dp_ber_settle_syms (PULLIN_BN, PULLIN_BN);
  /* The report's offset convention: inside the carrier loop's bound, cycles
     per SYMBOL, converted to per SAMPLE here and nowhere else. */
  const double foff
      = dp_test_freq_offset_inside_bw (PULLIN_BN, m, 1.0) / PULLIN_SPS;

  if (!truth || !x || !out)
    {
      rc = 1;
      goto done;
    }
  for (int dr = 0; dr < PULLIN_DRAWS; dr++)
    {
      uint32_t st = seed0 + 7919u * (uint32_t)dr;
      for (size_t k = 0; k < nsym; k++)
        truth[k] = (uint8_t)(dp_xs32 (&st) % (uint32_t)m);
      double d   = dp_uni (&st);
      double ph0 = 2.0 * MPSK_PI * dp_uni (&st);
      pullin_stimulus (pulse, m, truth, nsym, d, foff, ph0, esn0_db, &st, x);

      dp_mpsk_receiver_state_t *rx = dp_mpsk_receiver_create (
          m, (double)PULLIN_SPS, m_out, pulse, PULLIN_BETA, PULLIN_SPAN,
          PULLIN_BN, 0.0, PULLIN_BN, 0.0, 0.0, 0, 0, 1, 0.0);
      if (!rx)
        {
          rc = 1;
          goto done;
        }
      bank_sps = rx->fe.c->rc->bank_sps;
      if (pulse == MPSK_RX_PULSE_IANDD
          && pullin_set_bank (rx, bank, &dev) != 0)
        {
          fprintf (stderr,
                   "pullin: the corrected rebuild does not reproduce the "
                   "as-built bank (max dev %.3g); refusing the A/B\n",
                   dev);
          dp_mpsk_receiver_destroy (rx);
          rc = 1;
          goto done;
        }
      size_t nout    = 0;
      long   lock_at = -1;
      for (size_t i = 0; i < nsamp; i++)
        {
          float complex y = 0.0f;
          if (mpsk_receiver_step_ted (rx, x[i], &y, RATESYNC_TED_GARDNER)
              && nout < nsym)
            {
              if (lock_at < 0 && dp_mpsk_receiver_get_locked (rx))
                lock_at = (long)nout;
              out[nout++] = y;
            }
        }
      clipped |= dp_mpsk_receiver_get_clipped (rx);
      dp_mpsk_receiver_destroy (rx);
      lk[dr] = lock_at;
      if (lock_at >= 0)
        {
          lock_sum += (double)lock_at;
          lock_n++;
        }

      dp_ber_meter_state_t *mt = dp_ber_meter_create (m, 0, DP_BER_CONF);
      dp_ber_meter_set_truth (mt, truth, nsym);
      if (!dp_ber_meter_align (mt, out, nout, settle, DP_BER_SYNC_SYMS, 0,
                               (int)DP_BER_LAG_SPAN, DP_BER_SYNC_PFA))
        {
          refused++;
          dp_ber_meter_destroy (mt);
          continue;
        }
      for (size_t w = 0; w < PULLIN_NWIN; w++)
        {
          size_t e0 = dp_ber_meter_get_errors (mt);
          size_t s  = dp_ber_meter_score (mt, out, nout, w * PULLIN_W,
                                          (w + 1) * PULLIN_W);
          size_t de = dp_ber_meter_get_errors (mt) - e0;
          err[w] += de;
          sym[w] += s;
          if ((w + 1) * PULLIN_W <= settle)
            {
              hd_e[dr] += de;
              hd_s[dr] += s;
            }
          else if (w * PULLIN_W >= settle + DP_BER_SYNC_SYMS)
            {
              tl_e[dr] += de;
              tl_s[dr] += s;
            }
        }
      ok[dr] = 1;
      dp_ber_meter_destroy (mt);
    }

  {
    static const char *bn[] = { "corrected", "sampled" };
    const char        *pn   = pulse == MPSK_RX_PULSE_IANDD ? "iandd" : "rrc";
    for (size_t w = 0; w < PULLIN_NWIN; w++)
      printf ("W,%s,%s,%d,%zu,%.4f,%d,%d,%d,%.1f,%d,%.6f,%zu,%lu,%lu\n",
              bn[bank], pn, m, m_out, esn0_db, PULLIN_DRAWS, refused, clipped,
              lock_n ? lock_sum / lock_n : -1.0, lock_n, bank_sps,
              w * PULLIN_W, err[w], sym[w]);
    for (int dr = 0; dr < PULLIN_DRAWS; dr++)
      printf ("D,%s,%s,%d,%zu,%.4f,%d,%d,%ld,%lu,%lu,%lu,%lu\n", bn[bank], pn,
              m, m_out, esn0_db, dr, ok[dr], lk[dr], hd_e[dr], hd_s[dr],
              tl_e[dr], tl_s[dr]);
  }
done:
  free (truth);
  free (x);
  free (out);
  return rc;
}

/* The --pullin sweep: rectangle on both banks and the RRC on the built one,
   m_out 4 and 8, every M at its SER = 1e-3 anchor and at 20 dB. */
static int
pullin_main (void)
{
  int    ms[3]  = { 2, 4, 8 };
  size_t mos[2] = { 4, 8 };
  int    rc     = 0;
  printf ("# pullin: W=%u symbols, %d draws, settle=%u symbols, sps=%d, "
          "bn=%.2f/%.2f\n",
          PULLIN_W, PULLIN_DRAWS,
          (unsigned)dp_ber_settle_syms (PULLIN_BN, PULLIN_BN), PULLIN_SPS,
          PULLIN_BN, PULLIN_BN);
  /* Two record kinds, told apart by the first field. */
  printf ("# W,bank,pulse,m,m_out,esn0_db,draws,refused,clipped,"
          "lock_sym_mean,locked_draws,bank_sps,win_start,errors,symbols\n");
  printf ("# D,bank,pulse,m,m_out,esn0_db,draw,aligned,lock_sym,head_errors,"
          "head_symbols,tail_errors,tail_symbols\n");
  for (int mi = 0; mi < 3; mi++)
    {
      double es[2]
          = { dp_ber_esn0_db_for_ser (ms[mi], DP_BER_TARGET_SER), 20.0 };
      for (int oi = 0; oi < 2; oi++)
        for (int ei = 0; ei < 2; ei++)
          {
            uint32_t seed = 4242u + 101u * (uint32_t)(mi * 4 + oi * 2 + ei);
            /* The same seed on every bank and pulse: an A/B on identical
               draws, so the difference is the bank and not the luck. */
            rc |= pullin_case (BANK_CORRECTED, MPSK_RX_PULSE_IANDD, ms[mi],
                               mos[oi], es[ei], seed);
            rc |= pullin_case (BANK_SAMPLED, MPSK_RX_PULSE_IANDD, ms[mi],
                               mos[oi], es[ei], seed);
            rc |= pullin_case (BANK_CORRECTED, MPSK_RX_PULSE_RRC, ms[mi],
                               mos[oi], es[ei], seed);
          }
    }
  return rc;
}

int
main (int argc, char **argv)
{
  if (argc > 1 && strcmp (argv[1], "--pullin") == 0)
    return pullin_main ();
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int ms[3] = { 2, 4, 8 };
  int rc    = 0;

  printf ("MpskReceiver symbol error rate vs the coherent bound\n");
  printf ("  complex baseband, rectangular pulse, I&D matched filter\n");
  printf ("  each M at its own SER=1e-3 anchor, %lu errors per point\n\n",
          TARGET_ERRORS);

  for (int mi = 0; mi < 3; mi++)
    {
      mpsk_ber_cfg_t    c;
      mpsk_ber_result_t r;
      double            esn0_db, loss_lo;
      char              label[48];

      c.real  = 0;
      c.m     = ms[mi];
      c.sps   = 8.0;
      c.m_out = 8;
      c.fc    = 0.0;
      /* An offset INSIDE the loop's acquisition bound, so the carrier loop
         has real work to do and the measurement says something about it.
         Seeded exactly on truth the loop never leaves its initial state and
         any conclusion drawn about it is void; asserted OUTSIDE the bound the
         test measures luck.

         The bound carries the `m`: the NDA discriminator is an M-th power, so
         it is `bn_carrier / m` per symbol. `0.5 * bn / sps` stood here, which
         is u = 0.5*m -- the same number at every order while asking a 4x
         harder question at 8PSK than at BPSK, and landing the 8PSK row of
         this very sweep at u = 4.0, the measured limit with no margin. */
      c.bn_timing  = 0.01;
      c.bn_carrier = 0.005;
      c.foff = dp_test_freq_offset_inside_bw (c.bn_carrier, c.m, 1.0) / c.sps;
      /* 8PSK used to hand over to a decision-directed discriminator here,
         on the reasoning that its decision margin is only +-pi/8 and the
         M-th-power discriminator's own phase noise eats into it. This
         harness is where that reasoning was measured and did not survive:
         turning the handover off moved 8PSK from 0.44 dB to 0.53 dB of loss
         -- 0.09 dB, against a settling window it MORE THAN DOUBLED, since
         the handover fired around symbol 8500. The receiver no longer has
         one (doppler#877), so the M=8 row below is the NDA discriminator's
         own number and should read ~0.53 dB. */

      esn0_db = dp_ber_esn0_db_for_ser (c.m, DP_BER_TARGET_SER);
      r = mpsk_ber_measure (&c, esn0_db, TARGET_ERRORS, 2024u + (unsigned)mi);

      snprintf (label, sizeof label, "M=%d @%.1f dB", c.m, esn0_db);
      dp_ber_print (label, &r.rep);
      printf ("%-24s   %d burst(s), %d unsettled, clipped=%d\n\n", "",
              r.bursts, r.unsettled, r.clipped);

      if (!check)
        continue;

      /* The gate: the implementation loss implied by the interval's LOWER
         limit (the receiver's best defensible rate) must clear LOSS_DB. Using
         the limit rather than the point estimate is what stops counting noise
         flaking this. */
      loss_lo = esn0_db - dp_ber_esn0_db_for_ser (c.m, r.rep.ser.lo);
      if (r.clipped)
        {
          printf ("  M=%d FAIL: front end clipped\n", c.m);
          rc = 1;
        }
      else if (!r.rep.sane)
        {
          printf ("  M=%d FAIL: %s\n", c.m, r.rep.why);
          rc = 1;
        }
      else if (!r.rep.enough)
        {
          printf ("  M=%d FAIL: only %lu of %lu errors in %d bursts\n", c.m,
                  r.rep.ser.errors, TARGET_ERRORS, r.bursts);
          rc = 1;
        }
      /* !(<=) not (>): a NaN loss (rate outside the bracket) must FAIL. */
      else if (!(loss_lo <= LOSS_DB))
        {
          printf ("  M=%d FAIL: loss %.2f dB (99%% lower limit) > %.1f dB\n",
                  c.m, loss_lo, LOSS_DB);
          rc = 1;
        }
      else
        printf ("  M=%d OK: loss %.2f dB (point %.2f), bound %.1f dB\n\n", c.m,
                loss_lo, r.rep.loss_db, LOSS_DB);
    }

  if (check)
    printf (rc ? "mpsk_receiver_ber FAILED\n" : "mpsk_receiver_ber PASSED\n");
  return rc;
}
