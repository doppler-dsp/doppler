/**
 * @file acq_rate_pd.c
 * @brief What Doppler-rate compensation buys a burst CAPTURE, and what it
 *        costs in false alarms -- measured, with confidence bounds.
 *
 * `doppler_rate` alone only LIMITS the acquisition's coherent depth to
 * `f_epoch/sqrt(2*doppler_rate)` repetitions, so a carrier sweeping at that
 * rate costs integration gain. dp_burst_capture_create()'s
 * `rate_hypotheses`
 * compensates it instead: the engine de-chirps its coherent block at `Rh`
 * rates across `[-doppler_rate, +doppler_rate]`, the depth is bounded by the
 * residual `doppler_rate/(Rh-1)`, and the per-cell Pfa is split over the
 * hypotheses. This program is the evidence for all three halves of that
 * sentence.
 *
 *   Geometry   a Zadoff-Chu 127 preamble repeated 16 times, then 1000
 *              samples of random QPSK, at fs = 1 (rates are cycles/sample^2).
 *              The carrier SWEEPS through the whole burst, preamble and
 *              payload: frequency `f_mid + a*(i - centre)`, `a` drawn
 *              uniform over the rate span for the "uniform" rows. The
 *              burst lands at a uniform random offset, a continuous delay,
 *              in AWGN from the shipped `awgn`.
 *
 *   Pd         for each hypothesis count Rh, at ONE C/N0: the one where the
 *              most-compensated engine predicts a burst Pd of 0.9. The
 *              ENGINE scores hit when some dwell overlapping the preamble
 *              names its code phase; the CAPTURE scores hit when it emits a
 *              window whose preamble_start is within 3 samples of the
 *              truth. Both carry a Wilson 95% interval. `pred` is the
 *              engine's own pd_burst, which does NOT model the loss of a
 *              rate that falls between two hypotheses -- that is what the
 *              "midpoint" rows measure: every trial's rate exactly half a
 *              spacing from the nearest hypothesis, the worst case the
 *              depth bound is derived against.
 *
 *   Pfa        noise only, through the engine: the share of dwells that
 *              clear the gate, against the `pfa` asked for, with a Wilson
 *              95% interval. The sizer prices `Rh` times the cells; this
 *              measures whether that is enough.
 *
 * Usage:
 *   validate_acq_rate_pd           every hypothesis count, 1000 trials a row,
 *                                  200000 noise dwells a row
 *   validate_acq_rate_pd --check   the spot checks CTest runs, on 1, 5, 9 and
 *                                  auto (0) hypotheses and fewer trials:
 *                                  compensation delivers what it predicts,
 *                                  beats the capped depth, and holds Pfa
 *
 * Both modes assert the same claims.
 */
#include "doppler/acq/acq_core.h"
#include "doppler/awgn/awgn_core.h"
#include "doppler/burst_capture/burst_capture_core.h"
#include "doppler/dp_complex.h"
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "dp_preamble_test.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 127u
#define REPS 16u
#define PAYLOAD 1000u
#define BURST (REPS * N + PAYLOAD)
#define PFA 1e-3
#define PD 0.9
#define TOL 3.0
/* The rate span, cycles/sample^2: a carrier at the span's rate sweeps 0.0018
   of the sample rate across the preamble (under half the native Doppler
   span, 1/(2N)), at either sign. Uncompensated, the drift bound caps the depth
   at 5 repetitions of 16. */
#define RATE 9.0e-7

static float _Complex ZC[N];

static void
zadoff_chu (void)
{
  for (size_t k = 0; k < N; k++)
    ZC[k] = (float _Complex)cexp (-I * M_PI * 5.0 * (double)k * (double)(k + 1)
                                  / (double)N);
}

/* The preamble becomes ONE PERIOD of a repeated linear chirp from the shipped
   waveform generator (the `chirp` type): the instantaneous frequency sweeps
   -fs/4 .. +fs/4 over the period's N samples, untapered, the widest sweep the
   geometry holds. The downstream use is exactly this -- a repeated chirp -- so
   the rows below say the compensation is not a property of the Zadoff-Chu
   preamble the rest of this program measures. */
static void
lfm_preamble (void)
{
  dp_wfm_synth_state_t *g = dp_xnn (
      dp_wfm_synth_create (WFM_SYNTH_CHIRP, 1.0, -0.25, WFM_SYNTH_SNR_CLEAN, 0,
                           1, 8, 7, 0, 0, 0.25));
  dp_wfm_synth_set_chirp_span (g, N);
  dp_wfm_synth_steps (g, ZC, N);
  dp_wfm_synth_destroy (g);
}

static dp_burst_capture_state_t *
make (size_t rh, double cn0)
{
  dp_burst_capture_state_t *s = dp_burst_capture_create (
      ZC, N, BURST, REPS, 1.0, cn0, 0.0, PFA, PD, 0, RATE, rh);
  if (!s)
    {
      DP_TEST_ERR ("capture at Rh=%zu refused\n", rh);
      abort ();
    }
  return s;
}

/* The C/N0 (per-sample SNR, dB, at fs = 1) at which a capture with `rh`
   hypotheses first predicts a burst Pd of `target`, on a quarter-dB grid:
   pd_burst rises with C/N0, so a bisection finds the step a scan would. */
static double
cn0_for (size_t rh, double target)
{
  int lo = 0, hi = 200; /* -30 dB .. +20 dB */
  while (lo < hi)
    {
      int                       mid = (lo + hi) / 2;
      dp_burst_capture_state_t *s   = make (rh, -30.0 + 0.25 * (double)mid);
      double                    p   = dp_burst_capture_get_pd_burst (s);
      dp_burst_capture_destroy (s);
      if (p >= target)
        hi = mid;
      else
        lo = mid + 1;
    }
  return -30.0 + 0.25 * (double)lo;
}

/* Wilson score interval for k successes in n trials, 95%. Unlike the normal
   approximation it stays inside [0, 1] and is honest at k = 0, which the
   false-alarm rows live at. */
static void
wilson (double k, double n, double *lo, double *hi)
{
  const double z = 1.959964, p = k / n, z2 = z * z;
  const double c = (p + z2 / (2.0 * n)) / (1.0 + z2 / n);
  const double h
      = z * sqrt (p * (1.0 - p) / n + z2 / (4.0 * n * n)) / (1.0 + z2 / n);
  *lo = c - h < 0.0 ? 0.0 : c - h;
  *hi = c + h > 1.0 ? 1.0 : c + h;
}

typedef struct
{
  size_t rh, depth;
  int    midpoint, autorow;
  double pred, eng, eng_lo, eng_hi, cap, cap_lo, cap_hi;
} pd_row_t;

/* The rate of trial `t`: uniform over the span, or -- the worst case for a
   hypothesis grid -- exactly half a spacing from the nearest hypothesis
   (a random gap, so no hypothesis is favoured). One hypothesis has no
   gap: the midpoint falls back to the span's edge. */
static double
trial_rate (size_t rh, int midpoint, uint32_t *st)
{
  if (!midpoint)
    return (2.0 * dp_uni (st) - 1.0) * RATE;
  if (rh < 2)
    return (dp_uni (st) < 0.5 ? -RATE : RATE);
  const double step = 2.0 * RATE / (double)(rh - 1);
  const size_t gap  = (size_t)(dp_uni (st) * (double)(rh - 1));
  return -RATE + ((double)(gap > rh - 2 ? rh - 2 : gap) + 0.5) * step;
}

/* `rh` is the count ASKED for; 0 is auto, and the row then carries the count
   the engine chose (the trials' midpoint rates need it too). */
static pd_row_t
measure_pd (size_t rh, int midpoint, double cn0, int trials, uint32_t seed)
{
  pd_row_t r = { 0 };
  r.midpoint = midpoint;
  r.autorow  = (rh == 0);

  dp_burst_capture_state_t *s = make (rh, cn0);
  rh                          = dp_burst_capture_get_rate_hypotheses (s);
  r.rh                        = rh;
  r.pred                      = dp_burst_capture_get_pd_burst (s);
  r.depth                     = dp_burst_capture_get_doppler_bins (s);
  const size_t     lead_max   = 3u * r.depth * N;
  const size_t     len = lead_max + BURST + 2u * s->refine_span + 4u * N;
  float _Complex  *x   = dp_xmalloc (len * sizeof *x);
  float _Complex  *nz  = dp_xmalloc (len * sizeof *nz);
  float _Complex  *per = dp_xmalloc (N * sizeof *per);
  const size_t     cap = dp_burst_capture_push_max_out (s, len);
  float _Complex  *out = dp_xmalloc ((cap ? cap : 1) * sizeof *out);
  dp_awgn_state_t *g   = dp_xnn (
      dp_awgn_create (seed, dp_awgn_amplitude_for_snr ((float)cn0, 1.0f)));
  /* The carrier stays inside the native span for the whole preamble. */
  const double dur  = (double)(REPS * N);
  const double span = 1.0 / (2.0 * (double)N) - RATE * 0.5 * dur;
  uint32_t     st   = seed;
  int          hits = 0, eng = 0;

  for (int t = 0; t < trials; t++)
    {
      const size_t at
          = r.depth * N + (size_t)(dp_uni (&st) * (double)(2u * r.depth * N));
      const double f_mid = (2.0 * dp_uni (&st) - 1.0) * span;
      const double rate  = trial_rate (rh, midpoint, &st);
      const double frac  = dp_uni (&st);
      dp_preamble_shift (ZC, N, frac, per);
      memset (x, 0, len * sizeof *x);
      for (size_t i = 0; i < BURST; i++)
        {
          float _Complex v
              = i < REPS * N
                    ? per[i % N]
                    : (float _Complex)cexp (I * M_PI / 2.0
                                            * (double)(dp_xs32 (&st) >> 30));
          const double u = (double)i - 0.5 * dur; /* about the preamble */
          x[at + i] = v
                      * (float _Complex)cexp (
                          I * 2.0 * M_PI * (f_mid * u + 0.5 * rate * u * u));
        }
      dp_awgn_generate (g, len, nz, len);
      for (size_t i = 0; i < len; i++)
        x[i] += nz[i];

      dp_burst_capture_reset (s);
      (void)dp_burst_capture_push (s, x, len, out, cap);
      int hit = 0;
      for (size_t k = 0; k < dp_burst_capture_ready (s); k++)
        {
          const burst_capture_event_t *ev = dp_burst_capture_event_at (s, k);
          if (ev
              && fabs ((double)ev->preamble_start - ((double)at + frac))
                     <= TOL)
            hit = 1;
        }
      hits += hit;

      /* The ENGINE's own verdict: some dwell overlapping the preamble named
         its code phase (the raw hits the capture recorded before claiming
         anything). */
      const double truth = fmod ((double)at + frac, (double)N);
      int          e_hit = 0;
      for (size_t k = 0; k < s->det_len; k++)
        {
          const double ep = (double)s->det[k].epoch;
          double       d  = fabs (fmod (ep, (double)N) - truth);
          if (d > 0.5 * (double)N)
            d = (double)N - d;
          if (d <= TOL && ep > (double)at - (double)((r.depth + 1u) * N)
              && ep < (double)(at + REPS * N))
            e_hit = 1;
        }
      eng += e_hit;
    }
  r.eng = (double)eng / trials;
  r.cap = (double)hits / trials;
  wilson (eng, trials, &r.eng_lo, &r.eng_hi);
  wilson (hits, trials, &r.cap_lo, &r.cap_hi);
  dp_awgn_destroy (g);
  free (out);
  free (per);
  free (nz);
  free (x);
  dp_burst_capture_destroy (s);
  return r;
}

typedef struct
{
  size_t rh, depth;
  double dwells, hits, pfa, lo, hi;
} pfa_row_t;

/* Noise only, straight through the engine at the design C/N0: the share of
   dwells that clear the gate. The engine reports at most one hit a dwell
   (max_peaks 1), so hits/dwells is the per-dwell false-alarm rate the
   `pfa` parameter targets. */
static pfa_row_t
measure_pfa (size_t rh, double cn0, double dwells_wanted, uint32_t seed)
{
  pfa_row_t       r = { 0 };
  dp_acq_state_t *a
      = dp_acq_create_burst (ZC, N, REPS, 1.0, cn0, 0.0, PFA, PD, 0, RATE, rh);
  if (!a)
    {
      DP_TEST_ERR ("engine at Rh=%zu refused\n", rh);
      abort ();
    }
  r.rh                       = a->rate_hyps; /* what auto chose, else rh */
  r.depth                    = a->coherent_bins;
  const size_t     per_dwell = a->frame_n;
  const size_t     chunk     = 64u * per_dwell;
  float _Complex  *nz        = dp_xmalloc (chunk * sizeof *nz);
  acq_result_t     res[64];
  dp_awgn_state_t *g = dp_xnn (
      dp_awgn_create (seed, dp_awgn_amplitude_for_snr ((float)cn0, 1.0f)));
  while (r.dwells < dwells_wanted)
    {
      dp_awgn_generate (g, chunk, nz, chunk);
      size_t off = 0;
      while (off < chunk)
        {
          const size_t nh = dp_acq_push (a, nz + off, chunk - off, res, 64);
          r.hits += (double)nh;
          off += dp_acq_consumed (a);
          if (dp_acq_consumed (a) == 0)
            break;
        }
      r.dwells += 64.0;
    }
  r.pfa = r.hits / r.dwells;
  wilson (r.hits, r.dwells, &r.lo, &r.hi);
  dp_awgn_destroy (g);
  free (nz);
  dp_acq_destroy (a);
  return r;
}

typedef struct
{
  size_t rh, depth;
  double sig, rate;
  long   hits, nearest, edge, full, full_near;
} rate_row_t;

/* What the engine's doppler_rate_hz_s says, against the truth (the
   reviewer's probe of #2238, landed): the same synthesis as measure_pd -- a
   burst at a uniform offset, a continuous delay, f_mid uniform, the carrier
   sweeping at `rate` about the preamble's middle, QPSK payload, the shipped
   awgn at `sig` dB -- straight into an engine sized for `design`. A hit is
   scored only on the burst's own code phase. `nearest` counts hits naming the
   hypothesis nearest the truth, `edge` those naming +-R, and `full` /
   `full_near` the same restricted to hits whose dwell's block lay WHOLLY
   inside the preamble: a block straddling its edge integrates part noise and
   part payload, and names a rate from a surface that is not the preamble's. */
static rate_row_t
measure_rate (size_t rh, double design, double sig, double rate, int trials,
              uint32_t seed)
{
  rate_row_t      r = { 0 };
  dp_acq_state_t *a = dp_acq_create_burst (ZC, N, REPS, 1.0, design, 0.0, PFA,
                                           PD, 0, RATE, rh);
  if (!a)
    {
      DP_TEST_ERR ("engine at Rh=%zu refused\n", rh);
      abort ();
    }
  r.rh                      = rh;
  r.sig                     = sig;
  r.rate                    = rate;
  r.depth                   = a->coherent_bins;
  const size_t     D        = a->coherent_bins;
  const size_t     lead_max = 3u * D * N;
  const size_t     len      = lead_max + BURST + 4u * N;
  float _Complex  *x        = dp_xmalloc (len * sizeof *x);
  float _Complex  *nz       = dp_xmalloc (len * sizeof *nz);
  float _Complex  *per      = dp_xmalloc (N * sizeof *per);
  dp_awgn_state_t *g        = dp_xnn (
      dp_awgn_create (seed, dp_awgn_amplitude_for_snr ((float)sig, 1.0f)));
  const double dur  = (double)(REPS * N);
  const double span = 1.0 / (2.0 * (double)N) - RATE * 0.5 * dur;
  const double step = 2.0 * RATE / (double)(rh - 1);
  uint32_t     st   = seed;
  acq_result_t res[64];
  for (int t = 0; t < trials; t++)
    {
      const size_t at = D * N + (size_t)(dp_uni (&st) * (double)(2u * D * N));
      const double f_mid = (2.0 * dp_uni (&st) - 1.0) * span;
      const double frac  = dp_uni (&st);
      dp_preamble_shift (ZC, N, frac, per);
      memset (x, 0, len * sizeof *x);
      for (size_t i = 0; i < BURST; i++)
        {
          float _Complex v
              = i < REPS * N
                    ? per[i % N]
                    : (float _Complex)cexp (I * M_PI / 2.0
                                            * (double)(dp_xs32 (&st) >> 30));
          const double u = (double)i - 0.5 * dur;
          x[at + i] = v
                      * (float _Complex)cexp (
                          I * 2.0 * M_PI * (f_mid * u + 0.5 * rate * u * u));
        }
      dp_awgn_generate (g, len, nz, len);
      for (size_t i = 0; i < len; i++)
        x[i] += nz[i];
      dp_acq_reset (a);
      const double truth = fmod ((double)at + frac, (double)N);
      size_t       off   = 0;
      while (off < len)
        {
          const size_t nh = dp_acq_push (a, x + off, len - off, res, 64);
          for (size_t k = 0; k < nh; k++)
            {
              double d = fabs ((double)res[k].code_phase - truth);
              if (d > 0.5 * (double)N)
                d = (double)N - d;
              if (d > TOL)
                continue; /* not this burst's code phase */
              r.hits++;
              const double end = (double)res[k].samples_consumed;
              const int    in  = end - (double)(D * N) >= (double)at
                                 && end <= (double)(at + REPS * N);
              r.full += in;
              if (fabs (res[k].doppler_rate_hz_s - rate) <= 0.5 * step + 1e-15)
                {
                  r.nearest++;
                  r.full_near += in;
                }
              if (fabs (fabs (res[k].doppler_rate_hz_s) - RATE) < 1e-15)
                r.edge++;
            }
          const size_t c = dp_acq_consumed (a);
          if (c == 0)
            break;
          off += c;
        }
    }
  dp_awgn_destroy (g);
  free (per);
  free (nz);
  free (x);
  dp_acq_destroy (a);
  return r;
}

int
main (int argc, char **argv)
{
  const int    check  = (argc > 1 && strcmp (argv[1], "--check") == 0);
  const int    trials = check ? 250 : 1000;
  const double dwells = check ? 1.0e4 : 2.0e5;
  zadoff_chu ();

  /* The design point: where the most-compensated engine predicts PD. The
     spot check keeps both ends and the middle, and the worst case (a rate
     between two hypotheses) at the middle; the sweep, every count. */
  const size_t  RH_ALL[]   = { 1, 3, 5, 9, 0 };
  const size_t  RH_CHECK[] = { 1, 5, 9, 0 };
  const size_t *RH         = check ? RH_CHECK : RH_ALL;
  const size_t  NRH        = check ? sizeof RH_CHECK / sizeof *RH_CHECK
                                   : sizeof RH_ALL / sizeof *RH_ALL;
  /* The last entry is 0 = auto: the design point is the last EXPLICIT count's,
     and auto is then measured at it. */
  const double cn0 = cn0_for (RH[NRH - 2], PD);
  printf ("Zadoff-Chu %u x %u then %u samples of QPSK, fs = 1, rate span "
          "+-%g cycles/sample^2 (the carrier sweeps %.4f of fs across the "
          "preamble)\npfa %g, pd %g; every row at C/N0 %.2f dB, where Rh = "
          "%zu predicts a burst Pd of %g; %d trials a row, Wilson 95%%\n\n",
          N, REPS, PAYLOAD, RATE, RATE * (double)(REPS * N), PFA, PD, cn0,
          RH[NRH - 2], PD, trials);

  printf ("%3s %3s %-12s %6s | %6s %6s %6s | %6s %6s %6s\n", "Rh", "D", "rate",
          "pred", "engine", "lo95", "hi95", "captur", "lo95", "hi95");
  pd_row_t rows[12];
  size_t   nrows = 0;
  for (size_t i = 0; i < NRH; i++)
    for (int mid = 0; mid < 2; mid++)
      {
        if ((RH[i] == 1 || RH[i] == 0 || (check && RH[i] != 5)) && mid)
          continue;
        pd_row_t r    = measure_pd (RH[i], mid, cn0, trials,
                                    4242u + 17u * (uint32_t)i + (uint32_t)mid);
        rows[nrows++] = r;
        printf ("%3zu %3zu %-12s %6.3f | %6.3f %6.3f %6.3f | %6.3f %6.3f "
                "%6.3f\n",
                r.rh, r.depth,
                r.autorow ? "uniform auto"
                : mid     ? "midpoint"
                          : "uniform",
                r.pred, r.eng, r.eng_lo, r.eng_hi, r.cap, r.cap_lo, r.cap_hi);
      }

  printf ("\nnoise only: share of dwells above the gate (target pfa %g)\n",
          PFA);
  printf ("%3s %3s %9s %7s %9s %9s %9s\n", "Rh", "D", "dwells", "hits", "pfa",
          "lo95", "hi95");
  pfa_row_t prow[12];
  for (size_t i = 0; i < NRH; i++)
    {
      prow[i] = measure_pfa (RH[i], cn0, dwells, 99u + (uint32_t)i);
      printf ("%3zu %3zu %9.0f %7.0f %9.2e %9.2e %9.2e\n", prow[i].rh,
              prow[i].depth, prow[i].dwells, prow[i].hits, prow[i].pfa,
              prow[i].lo, prow[i].hi);
    }

  /* The reported rate, against the truth (acq_result_t::doppler_rate_hz_s).
     The contract: right from a block wholly inside the preamble, and only
     well above the design point; a block straddling the preamble's edge
     often names +-R. The full run prints the design point and 3 dB over as
     well, which the contract does not claim; the +23 dB row is gated. */
  printf ("\nreported rate: Rh = 9, hits on the burst's code phase\n");
  printf ("%7s %9s %6s | %6s %7s %7s | %6s %9s\n", "sig dB", "true rate",
          "hits", "near", "near %", "+-R %", "full", "full near");
  rate_row_t rr[8];
  size_t     nrr = 0;
  {
    const double sigs[3] = { cn0, cn0 + 3.0, cn0 + 23.0 };
    for (int i = check ? 2 : 0; i < 3; i++)
      for (int k = 0; k < 2; k++)
        {
          rr[nrr] = measure_rate (9, cn0, sigs[i], k ? RATE : 0.0,
                                  check ? 100 : 200,
                                  777u + 10u * (uint32_t)i + (uint32_t)k);
          const rate_row_t *q = &rr[nrr++];
          printf ("%7.2f %+9.1e %6ld | %6ld %6.1f%% %6.1f%% | %6ld %8.1f%%\n",
                  q->sig, q->rate, q->hits, q->nearest,
                  q->hits ? 100.0 * (double)q->nearest / (double)q->hits : 0.0,
                  q->hits ? 100.0 * (double)q->edge / (double)q->hits : 0.0,
                  q->full,
                  q->full ? 100.0 * (double)q->full_near / (double)q->full
                          : 0.0);
        }
  }

  /* The same claims in both modes: a sweep that asserted nothing would fail
     `make validate` for it (doppler#1566), and the full rows are a longer
     look at what the spot check pins. */
  {
    /* The compensated engine and capture deliver what pd_burst promises,
       in the worst case for a hypothesis grid as well as the average:
       the promise sits inside the 95% interval or below it. */
    for (size_t k = 0; k < nrows; k++)
      if (rows[k].rh > 1)
        {
          DP_CHECK_MSG (rows[k].eng_hi >= rows[k].pred,
                        "compensated engine delivers its pd_burst");
          DP_CHECK_MSG (rows[k].cap_hi >= rows[k].pred,
                        "compensated capture delivers its pd_burst");
          /* The trials are seeded, so a second run draws the SAME 250
             bursts and the point estimate is a regression pin, not a
             significance test. It is what holds refine's two rate
             refinements -- the Doppler cells widened by the shift a rate
             puts between the detecting dwell and the preamble, and a rate
             grid denser than the engine's -- because each is worth about
             0.02 here and a 95% interval (+-0.04) cannot see 0.02. Taken
             away, the uniform rows read 0.880-0.884 against a pin of
             pred - 0.015 = 0.889-0.892; with them, 0.904-0.912. */
          if (!rows[k].midpoint)
            DP_CHECK_MSG (rows[k].cap >= rows[k].pred - 0.015,
                          "compensated capture is within 0.015 of its "
                          "pd_burst on the seeded trials");
        }
    /* And it is worth having: the most compensated capture beats the
       capped depth, outside both intervals. */
    const pd_row_t *base = &rows[0], *top = &rows[0];
    for (size_t k = 0; k < nrows; k++)
      if (!rows[k].midpoint && !rows[k].autorow && rows[k].rh >= top->rh)
        top = &rows[k];
    DP_CHECK_MSG (top->cap_lo > base->cap_hi,
                  "compensation beats the capped depth, outside the "
                  "intervals");
    /* The reported rate: the rows 23 dB over the design point are the ones
       the contract claims. From a block wholly inside the preamble the
       winning hypothesis is the nearest to the truth (measured 111/111 and
       121/121 at 200 trials); a handful of such blocks is not a measurement,
       so the row must have them. */
    for (size_t k = 0; k < nrr; k++)
      if (rr[k].sig > cn0 + 20.0)
        {
          DP_CHECK_MSG (rr[k].full >= 40,
                        "enough full-block hits to read the rate from");
          DP_CHECK_MSG (rr[k].full_near * 100 >= rr[k].full * 95,
                        "a full block well above design names the nearest "
                        "rate hypothesis");
        }
    /* Auto (rate_hypotheses = 0) is worth having: it picks FEWER hypotheses
       than the most compensated explicit count -- the cost of a dwell is
       proportional to the count -- and delivers what that count does, no
       worse outside the intervals, and beats the capped depth. */
    for (size_t k = 0; k < nrows; k++)
      if (rows[k].autorow)
        {
          DP_CHECK_MSG (rows[k].rh < top->rh,
                        "auto picks FEWER hypotheses than the maximum "
                        "explicit count");
          DP_CHECK_MSG (rows[k].cap_hi >= top->cap_lo,
                        "auto is no worse than the most compensated count");
          DP_CHECK_MSG (rows[k].cap_lo > base->cap_hi,
                        "auto beats the capped depth, outside the "
                        "intervals");
        }
    /* Pfa: the hypotheses are priced. The rows with Rh > 1 are not
       significantly above the target. (Rh = 1 is the engine as it ran
       before compensation existed, and reads about twice the target at
       this geometry -- reported above, not this program's to hold.) */
    for (size_t i = 0; i < NRH; i++)
      if (prow[i].rh > 1)
        DP_CHECK_MSG (prow[i].lo <= PFA,
                      "per-dwell Pfa is not significantly above the "
                      "target");
  }

  /* The same claims on a repeated LFM preamble. ZC stays the geometry above;
     here the preamble is swapped for the generator's chirp and the design
     point recomputed for it (cn0_for), then the capped depth against five
     hypotheses. Two rows and one Pfa row: the claim is "the compensation is
     not specific to ZC", which two counts and the false-alarm bound show;
     the full grid above is ZC's. Capture Pd, capped against N = 5: 0.552
     against 0.924 in this --check run (250 trials), 0.564 against 0.921 in
     the reviewer's 1000-trial run. */
  {
    lfm_preamble ();
    const double cn0_l  = cn0_for (9, PD);
    const size_t lrh[2] = { 1, 5 };
    pd_row_t     lrow[2];
    printf ("\nrepeated LFM (-fs/4 .. +fs/4), C/N0 %.2f dB\n", cn0_l);
    printf ("%3s %3s %6s | %6s %6s %6s | %6s %6s %6s\n", "Rh", "D", "pred",
            "engine", "lo95", "hi95", "captur", "lo95", "hi95");
    for (size_t i = 0; i < 2; i++)
      {
        lrow[i] = measure_pd (lrh[i], 0, cn0_l, trials, 5151u + (uint32_t)i);
        printf ("%3zu %3zu %6.3f | %6.3f %6.3f %6.3f | %6.3f %6.3f %6.3f\n",
                lrow[i].rh, lrow[i].depth, lrow[i].pred, lrow[i].eng,
                lrow[i].eng_lo, lrow[i].eng_hi, lrow[i].cap, lrow[i].cap_lo,
                lrow[i].cap_hi);
      }
    DP_CHECK_MSG (lrow[1].cap_hi >= lrow[1].pred,
                  "LFM: compensated capture delivers its pd_burst");
    DP_CHECK_MSG (lrow[1].cap_lo > lrow[0].cap_hi,
                  "LFM: compensation beats the capped depth, outside the "
                  "intervals");
    pfa_row_t lp = measure_pfa (5, cn0_l, dwells, 6161u);
    printf ("LFM Pfa, Rh %zu D %zu: %.2e (95%%: %.2e - %.2e)\n", lp.rh,
            lp.depth, lp.pfa, lp.lo, lp.hi);
    DP_CHECK_MSG (lp.lo <= PFA,
                  "LFM: per-dwell Pfa is not significantly above the target");
  }
  DP_TEST_END ("validate_acq_rate_pd");
}
