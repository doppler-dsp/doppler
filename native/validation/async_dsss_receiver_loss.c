/**
 * @file async_dsss_receiver_loss.c
 * @brief Where the receiver's implementation loss goes, stage by stage,
 *        at the certification's operating point.
 *
 * `async_dsss_receiver_ber.c` measures the whole receiver against the BPSK
 * bound and reports one number. This splits it: the SPEC stimulus is
 * demodulated at KNOWN truth -- the synth's own carrier, its chips from
 * sample 0, its symbol edges from dp_wfm_dsss_cont_edge() -- through more
 * and more of the shipped chain, and each stage is scored as a loss in dB,
 * the Es/N0 the bound needs for the stage's error rate subtracted from the
 * Es/N0 applied (dp_ber_esn0_db_for_ser):
 *
 *   S0   the ideal despread: the exact carrier wiped, the exact code
 *        removed, each true symbol integrated. The harness's own baseline.
 *   S1   the same despread cut into K partials a code period (ideal sums
 *        over the partials' windows), each true symbol the matched,
 *        overlap-weighted sum of the partials it touches. A partial that
 *        straddles a symbol edge carries both symbols: the cost of the cut
 *        alone, at K = the receiver's `segments` (and, in the sweep,
 *        against K).
 *   S1d  the shipped Dll at the receiver's `segments`, held at truth, its
 *        emitted partials combined as S1's. Equal to S1 when no period's
 *        own magnitude scales its partials (doppler#2495: that per-period
 *        normalization once cost 0.2 dB and made the error
 *        non-circular; the output is divided by a slowly tracked level
 *        now).
 *   S2   S1d's partials through the shipped RateConverter to the
 *        demodulator's `sps`, integrated and dumped over each true symbol
 *        at the best offset: the rate converter at oracle timing.
 *   S3   S2's stream through the shipped MpskReceiver at the receiver's own
 *        parameters, aligned to truth past its settling: the demodulator's
 *        loops.
 *   S4   (the sweep only) the whole AsyncDsssReceiver on the raw stimulus,
 *        through its own search, refine and hand-over, on the same bursts:
 *        what S4 adds over S3 is the front end's tracking. MEASURED, NOT
 *        GATED here: async_dsss_receiver_ber.c gates the whole receiver.
 *
 * Gated, in --check and the sweep alike: S0 reads the bound, the Dll, the
 * converter and the demodulator's loops each add no more than their bound
 * over the stage before, the Dll's error is circular, the cut is within
 * its bound, a finer cut never costs more, and S3 aligns on every burst.
 * The sweep's stage table, with --emit, is the certification's section
 * 2.7 (src/doppler/dsss/tests/validation/async_dsss_receiver/).
 *
 * The stimulus streams a code period at a time; only symbol-rate results
 * and the rate converter's output are kept.
 *
 * Usage:
 *   validate_async_dsss_receiver_loss           the sweep: 5 and 8 dB, the
 *                                               K curve, the whole receiver
 *   validate_async_dsss_receiver_loss --check   the regression point, 5 dB,
 *                                               K = 4 and 8
 *   ... --emit                                  also a `# loss` CSV block,
 *                                               one row a stage a point
 */
#include "async_dsss_receiver_common.h"
#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/dll/dll_core.h"
#include "doppler/dp_complex.h"
#include "doppler/mpsk_receiver/mpsk_receiver_core.h"
#include "doppler/wfm/wfm_dsp.h"
#include "dp_ber_test.h"
#include "dp_dsss_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEGMENTS 4u /* the receiver's partials a code period (make_rx)  */
#define SPS 8u      /* the receiver's demodulator samples a symbol      */
#define LO_SYM 200u /* symbols skipped at a burst's start: warm-up      */
#define HI_SYM 50u  /* and at its end: the rate converter's delay       */

/* The gates, the same in --check and the sweep. A stage's loss is compared
   with the one before it on the SAME symbols and noise, so the difference
   carries far less counting noise than either loss. --check at 5 dB scores
   about 60,000 symbols: about 370 errors at S0 and 400 to 500 at the stages
   after it. */
/* S0, the ideal despread, reads the bound: its 99% interval (dp_ber_ci, on
   the interval and never the point estimate) comes within this of 0 dB. */
#define S0_ABS_DB 0.15
#define DLL_EXCESS_DB 0.1 /* S1d over S1: the Dll's partials are linear    */
#define IQ_ABS_DB 0.3     /* S1d's in-phase and quadrature error powers      */
#define RC_EXCESS_DB 0.1  /* S2 over S1d: the converter at oracle timing    */
/* The cut into K = 4 partials, S1 over S0 on the same symbols: 0.41 dB in
   the sweep (16 bursts at 5 dB) and 0.37 in --check, at e3dc58307. A bound
   by measurement plus a margin for another toolchain's draws, not derived:
   a cut past it is a change to the despread's geometry. */
#define CUT_MAX_DB 0.6
/* The demodulator's loops, S3 over S2: 0.23 dB in the sweep at 5 dB, 0.24
   in --check, at e3dc58307. Measured plus about three sigma of the
   difference's counting noise at --check's 400-odd errors a stage (each
   loss about 0.05 dB, so the difference at most 0.08). It catches a
   demodulator that costs a quarter dB more, or one that no longer aligns,
   not a small retune: a carrier loop five times wider reads +0.11 dB and
   passes. */
#define LOOPS_MAX_DB 0.5
/* The cut and the loops are gated where S0 has the errors to resolve them:
   at 5 dB. At 8 dB S0 counts about 40 and they are printed only. */
#define GATE_MIN_ERRORS 300

static const int K_CURVE[] = { 1, 2, 4, 8, 16, 32 };
#define N_K (sizeof K_CURVE / sizeof K_CURVE[0])

/* One stage's account over every burst. */
typedef struct
{
  double sym, err, si, si2, sq, sq2;
} stage_t;

static void
stage_add (stage_t *s, double z, double q)
{
  s->sym += 1;
  s->err += z < 0;
  s->si += z;
  s->si2 += z * z;
  s->sq += q;
  s->sq2 += q * q;
}

static double
stage_loss (const stage_t *s, double es)
{
  return es - dp_ber_esn0_db_for_ser (2, s->err / s->sym);
}

static double
stage_iq_db (const stage_t *s)
{
  double A = s->si / s->sym, vi = s->si2 / s->sym - A * A;
  double mq = s->sq / s->sym, vq = s->sq2 / s->sym - mq * mq;
  return 10.0 * log10 (vi / vq);
}

static void
stage_print (const char *name, const stage_t *s, double es)
{
  printf ("  %-26s BER %.3e (%6.0f errors)  loss %5.2f dB  I/Q error %+5.2f "
          "dB\n",
          name, s->err / s->sym, s->err, stage_loss (s, es), stage_iq_db (s));
}

/* One stage's row of the `# loss` block (--emit). */
static void
emit_row (double es, const char *name, int k, const stage_t *s,
          size_t unsettled)
{
  printf ("%.1f,%s,%d,%.0f,%.0f,%.4f,%.4f,%zu\n", es, name, k, s->sym, s->err,
          stage_loss (s, es), stage_iq_db (s), unsettled);
}

/* Score symbols y[lo, hi) against known +/-1 data d: one common phase. */
static void
score (stage_t *s, const double complex *y, const double *d, size_t lo,
       size_t hi)
{
  double complex c = 0;
  for (size_t k = lo; k < hi; k++)
    c += y[k] * d[k];
  if (cabs (c) == 0.0)
    return;
  double complex r = conj (c) / cabs (c);
  for (size_t k = lo; k < hi; k++)
    {
      double complex z = y[k] * r * d[k];
      stage_add (s, creal (z), cimag (z));
    }
}

/* Add partial value p, spanning samples [b0, b1), to every symbol it
   touches, weighted by the fraction of it inside: the matched combine. */
static void
spread_partial (double complex *y, const size_t *E, size_t n_sym, size_t *k,
                size_t b0, size_t b1, double complex p)
{
  while (*k < n_sym && E[*k + 1] <= b0)
    (*k)++;
  for (size_t kk = *k; kk < n_sym && E[kk] < b1; kk++)
    {
      size_t a0 = b0 > E[kk] ? b0 : E[kk];
      size_t a1 = b1 < E[kk + 1] ? b1 : E[kk + 1];
      y[kk] += p * (double)(a1 - a0) / (double)(b1 - b0);
    }
}

/* S2: integrate the converter's stream over each true symbol, its sample m
   taken to span partial-time [m/rate - off, (m+1)/rate - off), a partial
   unit being TE/SEGMENTS samples. */
static void
rc_dump (double complex *y, const size_t *E, size_t n_sym,
         const float complex *q, size_t nq, double rate, double off)
{
  const double unit = (double)TE / (double)SEGMENTS;
  memset (y, 0, n_sym * sizeof *y);
  for (size_t m = 0, k = 0; m < nq; m++)
    {
      double t0 = ((double)m / rate - off) * unit;
      double t1 = ((double)(m + 1) / rate - off) * unit;
      if (t0 < 0)
        continue;
      while (k < n_sym && (double)E[k + 1] <= t0)
        k++;
      for (size_t kk = k; kk < n_sym && (double)E[kk] < t1; kk++)
        {
          double a0 = t0 > (double)E[kk] ? t0 : (double)E[kk];
          double a1 = t1 < (double)E[kk + 1] ? t1 : (double)E[kk + 1];
          y[kk] += q[m] * ((a1 - a0) / (t1 - t0));
        }
    }
}

/* The oracle offset: the one that maximizes the in-phase SNR over the
   first symbols, coarse then fine. */
static double
rc_offset (double complex *y, const size_t *E, size_t n_sym, const double *d,
           const float complex *q, size_t nq, double rate)
{
  size_t hi   = n_sym < 3000 ? n_sym : 3000;
  double best = -1.0, at = 0.0;
  for (int pass = 0; pass < 2; pass++)
    {
      double from = pass ? at - 0.25 : 0.0, to = pass ? at + 0.25 : 20.0;
      double step = pass ? 0.01 : 0.25;
      for (double off = from; off <= to; off += step)
        {
          rc_dump (y, E, hi, q, nq, rate, off);
          double complex c = 0;
          for (size_t k = LO_SYM; k < hi; k++)
            c += y[k] * d[k];
          double s = 0, s2 = 0, n = 0;
          for (size_t k = LO_SYM; k < hi; k++)
            {
              double z = creal (y[k] * conj (c) / cabs (c) * d[k]);
              s += z;
              s2 += z * z;
              n++;
            }
          double A = s / n, snr = A * A / (s2 / n - A * A);
          if (snr > best)
            {
              best = snr;
              at   = off;
            }
        }
    }
  return at;
}

typedef struct
{
  stage_t s0, s1[N_K], s1d, s2, s3, s4;
  size_t  s4_unsettled, s3_unaligned;
} point_t;

/* One burst at `es`, every stage, accumulated into `pt`. S1 runs at each
   of the `nk` cuts `ks`; S4 only when `whole`. */
static void
burst (point_t *pt, const uint8_t *code, double es, uint32_t seed, double secs,
       const int *ks, size_t nk, int whole)
{
  uint32_t st    = trial_rng (seed);
  double   cn0   = cn0_of (es);
  double   car   = (2.0 * dp_uni (&st) - 1.0) * SPAN;
  size_t   n_blk = blocks_for ((size_t)(secs * FS)), N = n_blk * TE;
  size_t   n_bits           = symbol_at (N) + 2;
  uint8_t *bits             = draw_bits (&st, n_bits);
  dp_wfm_synth_state_t *syn = dp_dsss_cont_emitter (
      code, SF, SPC, FS, car, CPS, seed, WFM_DSSS_DATA_BITS, bits, n_bits);
  dp_awgn_state_t *g = make_noise (seed, cn0);

  size_t n_sym = 0;
  while ((double)SPC * (double)dp_wfm_dsss_cont_edge (n_sym + 1, CPS)
         <= (double)N)
    n_sym++;
  size_t *E = dp_xmalloc ((n_sym + 1) * sizeof *E);
  double *d = dp_xmalloc (n_sym * sizeof *d);
  for (size_t k = 0; k <= n_sym; k++)
    E[k] = SPC * (size_t)dp_wfm_dsss_cont_edge (k, CPS);
  for (size_t k = 0; k < n_sym; k++)
    d[k] = bits[k] ? -1.0 : 1.0;

  double complex *y0 = calloc (n_sym, sizeof *y0);
  double complex *y1[N_K];
  size_t          k1[N_K];
  double complex  acc1[N_K];
  for (size_t i = 0; i < nk; i++)
    {
      y1[i]   = calloc (n_sym, sizeof *y1[i]);
      k1[i]   = 0;
      acc1[i] = 0;
    }
  double complex *y1d = calloc (n_sym, sizeof *y1d);

  dp_dll_state_t *dll
      = dp_dll_create (code, SF, SPC, 0.0, 0.002, 0.707, 0.5, SEGMENTS);
  dp_dll_hold_here (dll); /* held at truth: tracking is not under test */
  dp_dll_set_coast (dll, 1);
  const double rate
      = (double)SPS * SYM_RATE / (CHIP_RATE * (double)SEGMENTS / (double)SF);
  dp_RateConverter_state_t *rc = dp_RateConverter_create (rate, 0);
  /* The receiver's own demodulator, by its named parameters. */
  dp_mpsk_receiver_state_t *mr = dp_mpsk_receiver_create (
      2, (double)SPS, (size_t)mpsk_rx_derive_m_out ((double)SPS, 0),
      MPSK_RX_PULSE_IANDD, ASYNC_DSSS_RX_DEMOD_RRC_BETA,
      ASYNC_DSSS_RX_DEMOD_RRC_SPAN, ASYNC_DSSS_RX_DEMOD_BN_CARRIER,
      ASYNC_DSSS_RX_DEMOD_ZETA, ASYNC_DSSS_RX_DEMOD_BN_TIMING,
      ASYNC_DSSS_RX_DEMOD_LOCK_THRESH, 0.0, 0, MPSK_RX_NUM_PHASES, 1,
      MPSK_RX_AGC_BW_RATIO);
  dp_async_dsss_receiver_state_t *rx = whole ? make_rx (code, cn0) : NULL;

  size_t         pcap = dp_dll_steps_max_out (dll) + SEGMENTS;
  float complex *prt  = dp_xmalloc (pcap * sizeof *prt);
  size_t         qcap = (size_t)((double)N / TE * SEGMENTS * rate) + 64;
  float complex *q    = calloc (qcap, sizeof *q);
  size_t         nq = 0, np = 0, k1d = 0, k0 = 0;
  float complex *sig   = dp_xmalloc (TE * sizeof *sig);
  float complex *nz    = dp_xmalloc (TE * sizeof *nz);
  float complex *w     = dp_xmalloc (TE * sizeof *w);
  size_t         y3cap = n_sym + 64, n3 = 0;
  float complex *y3    = calloc (y3cap, sizeof *y3);
  size_t         y4cap = n_sym + TE, n4 = 0, first4 = SIZE_MAX;
  float complex *y4  = whole ? calloc (y4cap, sizeof *y4) : NULL;
  unsigned char *lk4 = whole ? calloc (y4cap, 1) : NULL;
  double complex ph = 1.0, rot = cexp (-I * 2.0 * M_PI * car / FS);

  for (size_t b = 0; b < n_blk; b++)
    {
      dp_wfm_synth_steps (syn, sig, TE);
      dp_awgn_generate (g, TE, nz, TE);
      for (size_t i = 0; i < TE; i++)
        {
          sig[i] += nz[i];
          w[i] = (float complex) (sig[i] * ph); /* the exact carrier wiped */
          ph *= rot;
        }
      ph /= cabs (ph);
      /* S0 and S1: the ideal despread, sample by sample. A block is one
         code period, so its chips start at code[0]. */
      for (size_t i = 0; i < TE; i++)
        {
          size_t         n = b * TE + i;
          double complex v = w[i] * (code[i / SPC] ? -1.0 : 1.0);
          while (k0 < n_sym && E[k0 + 1] <= n)
            k0++;
          if (k0 < n_sym)
            y0[k0] += v;
          for (size_t ki = 0; ki < nk; ki++)
            {
              double len = (double)TE / ks[ki];
              acc1[ki] += v;
              size_t j  = (size_t)floor ((double)i / len);
              size_t b1 = (size_t)ceil (len * (double)(j + 1));
              if (i + 1 == b1 || i + 1 == TE)
                {
                  size_t b0 = (size_t)ceil (len * (double)j);
                  spread_partial (y1[ki], E, n_sym, &k1[ki], b * TE + b0,
                                  b * TE + b1, acc1[ki]);
                  acc1[ki] = 0;
                }
            }
        }
      /* S1d: the shipped Dll's partials, the j-th spanning S1's j-th. */
      size_t got = dp_dll_steps (dll, w, TE, prt, pcap);
      for (size_t j = 0; j < got; j++, np++)
        {
          double len = (double)TE / SEGMENTS;
          spread_partial (y1d, E, n_sym, &k1d, (size_t)ceil (len * np),
                          (size_t)ceil (len * (np + 1)), prt[j]);
        }
      /* S2, S3: the converter, then the demodulator. */
      size_t nr = dp_RateConverter_execute (rc, prt, got, q + nq, qcap - nq);
      n3 += dp_mpsk_receiver_steps (mr, q + nq, nr, y3 + n3, y3cap - n3);
      nq += nr;
      /* S4: the whole receiver on the raw stimulus. */
      if (whole)
        {
          size_t room = y4cap - n4;
          size_t k    = dp_async_dsss_receiver_steps (rx, sig, TE, y4 + n4,
                                                      room < TE ? room : TE);
          unsigned char up = dp_async_dsss_receiver_get_locked (rx);
          for (size_t j = 0; j < k; j++)
            lk4[n4 + j] = up;
          if (k && first4 == SIZE_MAX)
            first4 = b;
          n4 += k;
        }
    }

  size_t hi = n_sym > LO_SYM + HI_SYM ? n_sym - HI_SYM : n_sym;
  score (&pt->s0, y0, d, LO_SYM, hi);
  for (size_t ki = 0; ki < nk; ki++)
    score (&pt->s1[ki], y1[ki], d, LO_SYM, hi);
  score (&pt->s1d, y1d, d, LO_SYM, hi);
  double complex *y2  = calloc (n_sym, sizeof *y2);
  double          off = rc_offset (y2, E, n_sym, d, q, nq, rate);
  rc_dump (y2, E, n_sym, q, nq, rate, off);
  score (&pt->s2, y2, d, LO_SYM, hi);

  /* S3, S4: aligned to truth past the loops' settling, as the BER
     validator's bursts are (dp_ber_settle, dp_ber_sync). */
  {
    unsigned char *all = dp_xmalloc (n3 ? n3 : 1);
    memset (all, 1, n3 ? n3 : 1);
    int    ok     = 0;
    size_t settle = dp_ber_settle (mr->l.timing.bn, mr->l.bn_carrier, NULL,
                                   all, n3, &ok);
    dp_ber_marker_t mk
        = { NULL, DP_BER_SYNC_SYMS, settle + DP_BER_LAG_SPAN, 0, 0 };
    dp_ber_sync_t sy = dp_ber_sync (y3, n3, bits, n_bits, &mk, 2,
                                    DP_BER_LAG_SPAN, DP_BER_SYNC_PFA);
    if (ok && sy.ok)
      for (size_t i = mk.t0 + mk.n - sy.lag; i < n3 && i + sy.lag < hi; i++)
        {
          float complex z
              = y3[i]
                * cexpf (-I
                         * (float)(sy.phase + M_PI * bits[i + sy.lag]
                                   + mpsk_phi0 (2)));
          stage_add (&pt->s3, crealf (z), cimagf (z));
        }
    else
      pt->s3_unaligned++;
    free (all);
  }
  if (whole)
    {
      int    ok     = 0;
      size_t settle = dp_ber_settle (rx->rx->l.timing.bn, rx->rx->l.bn_carrier,
                                     NULL, lk4, n4, &ok);
      size_t base   = symbol_at (first4 == SIZE_MAX ? 0 : first4 * TE);
      dp_ber_marker_t mk
          = { NULL, DP_BER_SYNC_SYMS, settle + DP_BER_LAG_SPAN, 0, 0 };
      dp_ber_sync_t sy = dp_ber_sync (y4, n4, bits + base, n_bits - base, &mk,
                                      2, DP_BER_LAG_SPAN, DP_BER_SYNC_PFA);
      if (ok && sy.ok)
        for (size_t i = mk.t0 + mk.n - sy.lag; i < n4; i++)
          {
            float complex z
                = y4[i]
                  * cexpf (-I
                           * (float)(sy.phase + M_PI * bits[base + i + sy.lag]
                                     + mpsk_phi0 (2)));
            stage_add (&pt->s4, crealf (z), cimagf (z));
          }
      else
        pt->s4_unsettled++;
      dp_async_dsss_receiver_destroy (rx);
      free (y4);
      free (lk4);
    }
  /* The pattern is sized to the run, so the synth never ran dry: every
     stage scored data, not the silence after it (doppler#1718). */
  DP_CHECK_MSG (!dp_wfm_synth_data_ended (syn),
                "the burst's data covered its run");

  for (size_t ki = 0; ki < nk; ki++)
    free (y1[ki]);
  free (y0);
  free (y1d);
  free (y2);
  free (y3);
  free (q);
  free (prt);
  free (sig);
  free (nz);
  free (w);
  free (E);
  free (d);
  free (bits);
  dp_mpsk_receiver_destroy (mr);
  dp_RateConverter_destroy (rc);
  dp_dll_destroy (dll);
  dp_awgn_destroy (g);
  dp_wfm_synth_destroy (syn);
}

int
main (int argc, char **argv)
{
  int check = 0, emit = 0;
  for (int i = 1; i < argc; i++)
    {
      check |= strcmp (argv[i], "--check") == 0;
      emit |= strcmp (argv[i], "--emit") == 0;
    }

  uint8_t code[SF];
  dp_dsss_gold_1023 (code);
  /* --check: 5 dB, six bursts of 4 s, the cut at the receiver's K and the
     next finer one, so "a finer cut never costs more" runs per push. The
     sweep: 5 and 8 dB, sixteen bursts of 6 s, the K curve, and the whole
     receiver on the same bursts. */
  static const double esn0s[] = { 5.0, 8.0 };
  static const int    k_rx[]  = { (int)SEGMENTS, 2 * (int)SEGMENTS };
  const int          *ks      = check ? k_rx : K_CURVE;
  size_t              nk      = check ? 2 : N_K;
  size_t              n_es    = check ? 1 : 2;
  uint32_t            nb      = check ? 6u : 16u;
  double              secs    = check ? 4.0 : 6.0;
  size_t              kseg    = 0;
  for (size_t i = 0; i < nk; i++)
    if (ks[i] == (int)SEGMENTS)
      kseg = i;

  point_t *pts[2] = { NULL, NULL };
  for (size_t e = 0; e < n_es; e++)
    {
      const double es = esn0s[e];
      point_t     *pt = pts[e] = calloc (1, sizeof *pt);
      for (uint32_t seed = 1; seed <= nb; seed++)
        burst (pt, code, es, seed, secs, ks, nk, !check);

      printf ("Es/N0 %.1f dB, %u bursts of %.0f s, the receiver's K = %u, "
              "sps = %u:\n",
              es, nb, secs, SEGMENTS, SPS);
      stage_print ("S0  ideal despread", &pt->s0, es);
      for (size_t i = 0; i < nk; i++)
        {
          char name[40];
          snprintf (name, sizeof name, "S1  ideal, K = %d%s", ks[i],
                    i == kseg ? " (rx)" : "");
          stage_print (name, &pt->s1[i], es);
        }
      stage_print ("S1d shipped Dll, held", &pt->s1d, es);
      stage_print ("S2  + RateConverter", &pt->s2, es);
      stage_print ("S3  + MpskReceiver", &pt->s3, es);
      if (!check)
        {
          stage_print ("S4  whole receiver", &pt->s4, es);
          printf ("  S4: %zu of %u bursts did not settle (not scored)\n",
                  pt->s4_unsettled, nb);
        }

      const double l0  = stage_loss (&pt->s0, es);
      const double l1  = stage_loss (&pt->s1[kseg], es);
      const double l1d = stage_loss (&pt->s1d, es);
      const double l2  = stage_loss (&pt->s2, es);
      printf ("  the cut %.2f dB, the Dll %+.2f dB, the converter %+.2f dB, "
              "the demodulator's loops %+.2f dB\n",
              l1 - l0, l1d - l1, l2 - l1d, stage_loss (&pt->s3, es) - l2);

      /* The gates, each written as x <= bound so a NaN fails. */
      if (pt->s0.err > 0)
        {
          /* The bound's loss at each end of S0's interval: the fewest errors
             read the least loss. */
          const dp_ber_ci_t ci = dp_ber_ci ((unsigned long)pt->s0.err,
                                            (unsigned long)pt->s0.sym, 0.99);
          const double      l0_lo = es - dp_ber_esn0_db_for_ser (2, ci.lo);
          const double      l0_hi = es - dp_ber_esn0_db_for_ser (2, ci.hi);
          DP_CHECK_MSG (l0_lo <= S0_ABS_DB && l0_hi >= -S0_ABS_DB,
                        "the ideal despread reads the BPSK bound");
        }
      DP_CHECK_MSG (l1d - l1 <= DLL_EXCESS_DB,
                    "the shipped Dll's partials cost nothing over the ideal "
                    "partials: no period's own magnitude scales it (#2495)");
      DP_CHECK_MSG (fabs (stage_iq_db (&pt->s1d)) <= IQ_ABS_DB,
                    "the Dll's despread error is circular");
      DP_CHECK_MSG (l2 - l1d <= RC_EXCESS_DB,
                    "the RateConverter costs nothing at oracle timing");
      if (pt->s0.err >= GATE_MIN_ERRORS)
        {
          DP_CHECK_MSG (l1 - l0 <= CUT_MAX_DB,
                        "the cut into the receiver's K partials costs no "
                        "more than it measured");
          DP_CHECK_MSG (stage_loss (&pt->s3, es) - l2 <= LOOPS_MAX_DB,
                        "the demodulator's loops cost no more than they "
                        "measured");
        }
      DP_CHECK_MSG (pt->s3_unaligned == 0,
                    "the demodulator's output aligns to truth on every "
                    "burst");
      for (size_t i = 0; i + 1 < nk; i++)
        DP_CHECK_MSG (stage_loss (&pt->s1[i + 1], es)
                          <= stage_loss (&pt->s1[i], es) + 0.05,
                      "a finer cut never costs more");
    }

  /* --emit: every stage at every point, for the certification's table. */
  if (emit)
    {
      printf ("\n# loss\nesn0_db,stage,k,symbols,errors,loss_db,iq_db,"
              "unsettled\n");
      for (size_t e = 0; e < n_es; e++)
        {
          const point_t *pt = pts[e];
          const double   es = esn0s[e];
          const int      K  = (int)SEGMENTS;
          emit_row (es, "S0", 0, &pt->s0, 0);
          for (size_t i = 0; i < nk; i++)
            emit_row (es, "S1", ks[i], &pt->s1[i], 0);
          emit_row (es, "S1d", K, &pt->s1d, 0);
          emit_row (es, "S2", K, &pt->s2, 0);
          emit_row (es, "S3", K, &pt->s3, 0);
          if (pt->s4.sym > 0)
            emit_row (es, "S4", K, &pt->s4, pt->s4_unsettled);
        }
      printf ("\n");
    }
  for (size_t e = 0; e < n_es; e++)
    free (pts[e]);
  DP_TEST_END ("validate_async_dsss_receiver_loss");
}
