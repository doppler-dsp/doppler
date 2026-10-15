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
 *   Preambles  the grid above is ZC's. Two more families repeat its claims
 *              at their own design points, capped against five hypotheses:
 *              a repeated LFM chirp and a PN m-sequence of the same length
 *              (dp_mls_poly(7)), both from the shipped waveform generator.
 *              The compensation is a property of the sweep, not of the ZC
 *              sequence, and these rows are what says so.
 *
 * Usage:
 *   validate_acq_rate_pd           every hypothesis count, 1000 trials a row,
 *                                  200000 noise dwells a row
 *   validate_acq_rate_pd --check   the spot checks CTest runs, on 1, 5, 9 and
 *                                  auto (0) hypotheses and fewer trials:
 *                                  compensation delivers what it predicts,
 *                                  beats the capped depth, and holds Pfa
 *   validate_acq_rate_pd --pfa-check
 *                                  the noise rows alone, at the dwells that
 *                                  resolve the error each row must catch
 *                                  (pfa_check()), in parallel; CTest runs
 *                                  it as validate_acq_rate_pd_pfa
 *   validate_acq_rate_pd --pfa-smoke
 *                                  the same parallel rows on two chunks of
 *                                  dwells, each checked against itself run
 *                                  alone; CTest runs it as
 *                                  validate_acq_rate_pd_pfa_smoke, and so
 *                                  does `make test-tsan`
 *
 * The first two modes assert the same claims.
 */
#include "doppler/acq/acq_core.h"
#include "doppler/awgn/awgn_core.h"
#include "doppler/burst_capture/burst_capture_core.h"
#include "doppler/dp_complex.h"
#include "doppler/dp_parallel.h"
#include "doppler/wfm/wfm_core.h"
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "dp_preamble_test.h"
#include "dp_rng_test.h"
#include "dp_swept_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdint.h>
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

/* The preamble becomes ONE PERIOD of the shipped PN generator (the `pn`
   type) on the maximal-length polynomial dp_mls_poly(7): 127 BPSK chips at
   one sample a chip, the Zadoff-Chu period's length, so the rows below
   differ from ZC's in the sequence alone. A binary m-sequence is the other
   preamble family in use, and its periodic autocorrelation is two-valued
   (N at lag 0, -1 at every other lag) where ZC's is ideal. Returns whether
   the generated period has exactly that autocorrelation, which is what
   makes it one m-sequence period and not a shorter or unbalanced one. */
static int
pn_preamble (void)
{
  dp_wfm_synth_state_t *g = dp_xnn (
      dp_wfm_synth_create (WFM_SYNTH_PN, 1.0, 0.0, WFM_SYNTH_SNR_CLEAN, 0, 1,
                           1, 7, dp_mls_poly (7), 0, 0.0));
  dp_wfm_synth_steps (g, ZC, N);
  dp_wfm_synth_destroy (g);
  for (size_t lag = 0; lag < N; lag++)
    {
      double r = 0.0;
      for (size_t k = 0; k < N; k++)
        r += crealf (ZC[k]) * crealf (ZC[(k + lag) % N]);
      if (fabs (r - (lag ? -1.0 : (double)N)) > 1e-3)
        return 0;
    }
  return 1;
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
          x[at + i] = v;
        }
      /* The sweep is about the PREAMBLE's middle; the payload continues it. */
      dp_swept_rotate (x + at, BURST, 1.0, f_mid, rate, 0.5 * dur);
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
  size_t   rh, depth;
  double   dwells, hits, pfa, lo, hi;
  uint64_t noise; /* FNV-1a over the noise drawn, when asked for */
} pfa_row_t;

/* FNV-1a over a buffer's bytes, continuing from `h`. */
static uint64_t
fnv1a (uint64_t h, const void *buf, size_t n)
{
  const unsigned char *b = buf;
  for (size_t i = 0; i < n; i++)
    h = (h ^ b[i]) * 1099511628211ull;
  return h;
}

/* Noise only, straight through the engine at the design C/N0: the share of
   dwells that clear the gate. The engine reports at most one hit a dwell
   (max_peaks 1), so hits/dwells is the per-dwell false-alarm rate the
   `pfa` parameter targets. With `fingerprint` set, the row also hashes
   every sample of noise it drew, so a smoke run can compare what two
   schedules fed the engine; it is off where it would cost (the full rows
   hash gigabytes).

   The noise source is made BEFORE the engine, which changes no count (a
   seeded generator draws the same stream whenever it is made) and is what
   lets --pfa-smoke see the rows' first use of the AWGN sine table under
   TSan. Made after it, the engine's own synchronization orders the rows'
   first dp_awgn_create calls, so TSan reports nothing: a plain flag back
   in awgn_core.c went unreported 3 runs in 3 that way, and is reported 3 in
   3 this way (doppler#2324, #2328). */
static pfa_row_t
measure_pfa (size_t rh, double cn0, double dwells_wanted, uint32_t seed,
             int fingerprint)
{
  pfa_row_t        r = { .noise = 14695981039346656037ull };
  dp_awgn_state_t *g = dp_xnn (
      dp_awgn_create (seed, dp_awgn_amplitude_for_snr ((float)cn0, 1.0f)));
  dp_acq_state_t *a
      = dp_acq_create_burst (ZC, N, REPS, 1.0, cn0, 0.0, PFA, PD, 0, RATE, rh);
  if (!a)
    {
      DP_TEST_ERR ("engine at Rh=%zu refused\n", rh);
      abort ();
    }
  r.rh                      = a->rate_hyps; /* what auto chose, else rh */
  r.depth                   = a->coherent_bins;
  const size_t    per_dwell = a->frame_n;
  const size_t    chunk     = 64u * per_dwell;
  float _Complex *nz        = dp_xmalloc (chunk * sizeof *nz);
  acq_result_t    res[64];
  while (r.dwells < dwells_wanted)
    {
      dp_awgn_generate (g, chunk, nz, chunk);
      if (fingerprint)
        r.noise = fnv1a (r.noise, nz, chunk * sizeof *nz);
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

/* --pfa-check: the per-dwell Pfa rows CI holds (doppler#2324), at the design
   point where nine hypotheses predict PD, each not significantly above the
   target. A row goes red when its Wilson 95% lower bound passes PFA: 120 hits
   or more in 100032 dwells, 29 or more in 20032.

   Only Rh = 1 DETECTS. Pricing the native cells instead of the interpolated
   ones (doppler#2218) doubles the base rate. At the rates the full run
   measures (200000 dwells a row, doppler#2287), the exact Poisson odds at
   these counts are:

     row    rate   dwells   on target: false red   doubled: escapes
     1      0.95x  100000   0.76%                  ~0
     3      0.60x  100000   ~0                     49%
     auto   0.65x  100000   ~0                     18%
     5      0.52x   20000   ~0                     95%
     9      0.22x   20000   ~0                     100%

   So 3 and auto are BACKSTOPS: a doubling only just crosses the target
   there, and resolving 1.2-1.3x would take two to four times the dwells. 5
   and 9 hold "under the target" as they are. The rows are seeded, so these
   odds say which draw CI runs, not how often a run flakes; on that draw the
   #2218 sabotage turns 1 and 3 red. The exact guards for #2218 are the C pin
   (test_acq_core.c's _acq_pfa_split_check) and `make lint-acq-pfa-split`,
   which holds every pricing site to acq_band_cells().

   Each row is its own engine, noise and seed, so the rows run in parallel
   and their counts do not depend on the schedule; the heaviest go first.
   --pfa-smoke holds that claim: the same fan-out on two chunks of dwells,
   each row equal to itself run alone in dwells, hits and every sample of
   noise drawn. It is labelled `threaded`, so `make test-tsan` runs it, and
   a race in what the rows share (the lazily built tables, doppler#2328)
   halts it there. */
typedef struct
{
  size_t    rh;
  double    dwells;
  uint32_t  seed;
  double    cn0;
  int       fingerprint;
  pfa_row_t row;
} pfa_job_t;

static void
pfa_job (size_t i, void *ctx)
{
  pfa_job_t *j = (pfa_job_t *)ctx + i;
  j->row = measure_pfa (j->rh, j->cn0, j->dwells, j->seed, j->fingerprint);
}

static int
pfa_check (int smoke)
{
  zadoff_chu ();
  const double cn0    = cn0_for (9, PD);
  pfa_job_t    jobs[] = {
    { 0, 1.0e5, 7000u, cn0, 0, { 0 } }, { 3, 1.0e5, 7003u, cn0, 0, { 0 } },
    { 9, 2.0e4, 7009u, cn0, 0, { 0 } }, { 5, 2.0e4, 7005u, cn0, 0, { 0 } },
    { 1, 1.0e5, 7001u, cn0, 0, { 0 } },
  };
  const size_t n = sizeof jobs / sizeof *jobs;
  if (smoke)
    for (size_t i = 0; i < n; i++)
      {
        jobs[i].dwells      = 128.0; /* two chunks of 64 */
        jobs[i].fingerprint = 1;
      }
  dp_parallel_for (n, pfa_job, jobs, 0);
  if (smoke)
    {
      for (size_t i = 0; i < n; i++)
        {
          const pfa_row_t *r = &jobs[i].row;
          const pfa_row_t  a
              = measure_pfa (jobs[i].rh, cn0, jobs[i].dwells, jobs[i].seed, 1);
          printf ("ask %zu: Rh %zu D %zu, %.0f dwells, %.0f hits, noise "
                  "%016llx\n",
                  jobs[i].rh, r->rh, r->depth, r->dwells, r->hits,
                  (unsigned long long)r->noise);
          DP_CHECK_MSG (r->rh == a.rh && r->depth == a.depth
                            && r->dwells == a.dwells && r->hits == a.hits,
                        "a row run in parallel counts what it counts alone");
          DP_CHECK_MSG (r->noise == a.noise,
                        "a row run in parallel draws the noise it draws "
                        "alone, sample for sample");
        }
      DP_TEST_END ("validate_acq_rate_pd --pfa-smoke");
    }
  printf ("noise only at C/N0 %.2f dB (where Rh = 9 predicts Pd %g): the "
          "share of dwells above the gate, target pfa %g\n",
          cn0, PD, PFA);
  printf ("%4s %3s %3s %9s %7s %9s %9s %9s\n", "ask", "Rh", "D", "dwells",
          "hits", "pfa", "lo95", "hi95");
  for (size_t i = 0; i < n; i++)
    {
      const pfa_row_t *r = &jobs[i].row;
      printf ("%4zu %3zu %3zu %9.0f %7.0f %9.2e %9.2e %9.2e\n", jobs[i].rh,
              r->rh, r->depth, r->dwells, r->hits, r->pfa, r->lo, r->hi);
      DP_CHECK_MSG (r->lo <= PFA,
                    "per-dwell Pfa is not significantly above the target");
    }
  DP_TEST_END ("validate_acq_rate_pd --pfa-check");
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
          x[at + i] = v;
        }
      dp_swept_rotate (x + at, BURST, 1.0, f_mid, rate, 0.5 * dur);
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

/* What a captured EVENT's doppler_rate_hz_s says, against the truth
   (doppler#2219): refine's winning hypothesis, from the whole preamble on
   refine's own grid. The burst synthesis of measure_rate, into a capture
   built at `design`, the carrier sweeping at `base` plus `off` of refine's
   own grid step; `hits` counts events placed within TOL of the true start
   and `nearest` those naming refine's grid point nearest the truth. */
static rate_row_t
measure_event_rate (size_t rh, double design, double sig, double base,
                    double off, int trials, uint32_t seed)
{
  rate_row_t                r = { 0 };
  dp_burst_capture_state_t *s = make (rh, design);
  r.rh                        = rh;
  r.sig                       = sig;
  r.depth                     = dp_burst_capture_get_doppler_bins (s);
  const size_t R              = s->rate_ph_n;
  const double step           = 2.0 * RATE / (double)(R - 1);
  /* The truth: `base` moved by `off` of THIS capture's own grid step, so an
     off-grid row stays off the grid whatever R refine's sizing chooses. */
  /* `base` in [-1, 1] of the span, snapped to the nearest point of THIS
     capture's grid (0 is a grid point only for odd R), then moved by `off`
     of its step. */
  const size_t hb   = (size_t)lround (0.5 * (base + 1.0) * (double)(R - 1));
  const double rate = dp_acq_rate_hypothesis (RATE, R, hb) + off * step;
  r.rate            = rate;
  const size_t     lead_max = 3u * r.depth * N;
  const size_t     len      = lead_max + BURST + 2u * s->refine_span + 4u * N;
  float _Complex  *x        = dp_xmalloc (len * sizeof *x);
  float _Complex  *nz       = dp_xmalloc (len * sizeof *nz);
  float _Complex  *per      = dp_xmalloc (N * sizeof *per);
  const size_t     cap      = dp_burst_capture_push_max_out (s, len);
  float _Complex  *out      = dp_xmalloc ((cap ? cap : 1) * sizeof *out);
  dp_awgn_state_t *g        = dp_xnn (
      dp_awgn_create (seed, dp_awgn_amplitude_for_snr ((float)sig, 1.0f)));
  const double dur  = (double)(REPS * N);
  const double span = 1.0 / (2.0 * (double)N) - RATE * 0.5 * dur;
  uint32_t     st   = seed;
  for (int t = 0; t < trials; t++)
    {
      const size_t at
          = r.depth * N + (size_t)(dp_uni (&st) * (double)(2u * r.depth * N));
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
          x[at + i] = v;
        }
      dp_swept_rotate (x + at, BURST, 1.0, f_mid, rate, 0.5 * dur);
      dp_awgn_generate (g, len, nz, len);
      for (size_t i = 0; i < len; i++)
        x[i] += nz[i];
      dp_burst_capture_reset (s);
      (void)dp_burst_capture_push (s, x, len, out, cap);
      for (size_t k = 0; k < dp_burst_capture_ready (s); k++)
        {
          const burst_capture_event_t *ev = dp_burst_capture_event_at (s, k);
          if (!ev
              || fabs ((double)ev->preamble_start - ((double)at + frac)) > TOL)
            continue;
          r.hits++;
          if (fabs (ev->doppler_rate_hz_s - rate) <= 0.5 * step + 1e-15)
            r.nearest++;
          if (fabs (fabs (ev->doppler_rate_hz_s) - RATE) < 1e-15)
            r.edge++;
          break;
        }
    }
  dp_awgn_destroy (g);
  free (out);
  free (per);
  free (nz);
  free (x);
  dp_burst_capture_destroy (s);
  return r;
}

int
main (int argc, char **argv)
{
  if (argc > 1 && strcmp (argv[1], "--pfa-check") == 0)
    return pfa_check (0);
  if (argc > 1 && strcmp (argv[1], "--pfa-smoke") == 0)
    return pfa_check (1);
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
      prow[i] = measure_pfa (RH[i], cn0, dwells, 99u + (uint32_t)i, 0);
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

  /* The EVENT's rate, against the truth (burst_capture_event_t's
     doppler_rate_hz_s, doppler#2219): refine's winning hypothesis from the
     whole preamble. Three truths: on the grid at 0 and at the span's edge,
     and OFF it, 0.4 of refine's grid step above 0 -- the real case, a
     carrier whose rate no hypothesis matches. The step is the capture's
     own (2 * RATE / (R - 1) for refine's R), so the row stays 0.4 off
     whatever R refine's sizing chooses. Gated where the contract
     claims it, 23 dB over the design point; the design point and 3 dB over
     are printed. */
  printf ("\nevent rate: Rh = 9, captured events placed within %.0f "
          "samples\n",
          TOL);
  printf ("%7s %9s %6s | %6s %7s %7s\n", "sig dB", "true rate", "events",
          "near", "near %", "+-R %");
  rate_row_t er[9];
  size_t     ner = 0;
  {
    const double sigs[3]  = { cn0, cn0 + 3.0, cn0 + 23.0 };
    const double bases[3] = { 0.0, 1.0, 0.0 }; /* of the span */
    const double offs[3]  = { 0.0, 0.0, 0.4 };
    for (int i = check ? 2 : 0; i < 3; i++)
      for (int k = 0; k < 3; k++)
        {
          er[ner] = measure_event_rate (
              9, cn0, sigs[i], bases[k], offs[k], check ? 100 : 200,
              9777u + 10u * (uint32_t)i + (uint32_t)k);
          const rate_row_t *q = &er[ner++];
          printf ("%7.2f %+9.1e %6ld | %6ld %6.1f%% %6.1f%%\n", q->sig,
                  q->rate, q->hits, q->nearest,
                  q->hits ? 100.0 * (double)q->nearest / (double)q->hits : 0.0,
                  q->hits ? 100.0 * (double)q->edge / (double)q->hits : 0.0);
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
             significance test: the BROAD backstop for a refine or capture
             regression worth 0.015-0.04 of Pd, which the interval checks
             above (+-0.04) cannot see. The two refine refinements it was
             first written for -- the Doppler cells widened by the shift a
             rate puts between the detecting dwell and the preamble, and a
             rate grid denser than the engine's, each worth about 0.02 here
             -- have exact guards now, deterministic scenarios in
             test_burst_capture_core.c where removing either names the wrong
             repetition (doppler#2219). This pin is for whatever else moves
             the seeded estimate. */
          if (!rows[k].midpoint)
            DP_CHECK_MSG (rows[k].cap >= rows[k].pred - 0.015,
                          "compensated capture is within 0.015 of its "
                          "pd_burst on the seeded trials");
        }
    const pd_row_t *base = &rows[0], *top = &rows[0];
    for (size_t k = 0; k < nrows; k++)
      if (!rows[k].midpoint && !rows[k].autorow && rows[k].rh >= top->rh)
        top = &rows[k];
    /* The capped capture delivers ITS pd_burst too, the same promise the
       compensated rows keep. Refine searches its rate grid whenever a rate
       is declared (doppler#2219), so one acquisition hypothesis no longer
       collapses refine's sum: keyed on the engine's count, this row's
       capture read 0.555 against its prediction of 0.848 (and its engine's
       0.946). */
    DP_CHECK_MSG (base->cap_hi >= base->pred,
                  "the capped capture delivers its pd_burst");
    /* And compensation is worth having for what it claims, DEPTH: 3 dB
       under the design point, where the capped engine is underpowered, the
       compensated engine detects more, outside both intervals. Engine Pd,
       because depth is the engine's; at the design point the capped engine
       already reads ~0.95 and a capture comparison measured refine, not
       depth (doppler#2219). */
    pd_row_t     und[3];
    const double cu = cn0 - 3.0;
    {
      const size_t urh[3] = { 1, 0, top->rh };
      printf ("\ndepth, 3 dB under the design point (C/N0 %.2f dB)\n", cu);
      printf ("%3s %3s %6s | %6s %6s %6s | %6s %6s %6s\n", "Rh", "D", "pred",
              "engine", "lo95", "hi95", "captur", "lo95", "hi95");
      for (size_t i = 0; i < 3; i++)
        {
          und[i] = measure_pd (urh[i], 0, cu, trials, 4242u + (uint32_t)i);
          printf ("%3zu %3zu %6.3f | %6.3f %6.3f %6.3f | %6.3f %6.3f %6.3f\n",
                  und[i].rh, und[i].depth, und[i].pred, und[i].eng,
                  und[i].eng_lo, und[i].eng_hi, und[i].cap, und[i].cap_lo,
                  und[i].cap_hi);
        }
    }
    DP_CHECK_MSG (und[2].eng_lo > und[0].eng_hi,
                  "compensation's depth beats the capped depth 3 dB under "
                  "the design point, outside the intervals");
    DP_CHECK_MSG (und[1].eng_lo > und[0].eng_hi,
                  "auto's depth beats the capped depth 3 dB under the "
                  "design point, outside the intervals");
    /* And the depth reaches the CAPTURE: the compensated capture places
       more than the capped one there, outside the intervals. Gated for
       auto, which separates in this run and the full one (--check: 0.620
       [0.558, 0.678] against 0.456 [0.395, 0.518]; full: 0.629 [0.599,
       0.658] against 0.465 [0.434, 0.496]). Not for n = 9. On the library's
       rotation it separates in both runs too (--check 0.628 [0.567, 0.686],
       full 0.592 [0.561, 0.622]), but on the private phase ramp this
       stimulus replaced, its 250-trial interval overlapped the capped one
       (0.538 against 0.554): whether it separates at --check depends on the
       draw, so it is printed, not claimed. The capture's loss against its
       engine (0.704 -> 0.628 here, 0.685 -> 0.592 in the full run) is
       doppler#2341's. */
    DP_CHECK_MSG (und[1].cap_lo > und[0].cap_hi,
                  "auto's depth reaches the capture 3 dB under the design "
                  "point, outside the intervals");
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
    /* The event's rate, where its contract claims it: 23 dB over the
       design point, refine's whole-preamble winner is the grid point nearest
       the truth. A handful of events is not a measurement, so the row must
       have them. */
    for (size_t k = 0; k < ner; k++)
      if (er[k].sig > cn0 + 20.0)
        {
          DP_CHECK_MSG (er[k].hits >= 40,
                        "enough captured events to read the rate from");
          DP_CHECK_MSG (er[k].nearest * 100 >= er[k].hits * 95,
                        "an event well above design names the nearest "
                        "rate hypothesis");
        }
    /* Auto (rate_hypotheses = 0) is worth having: it picks FEWER hypotheses
       than the most compensated explicit count -- the cost of a dwell is
       proportional to the count -- and delivers what that count does, no
       worse outside the intervals (its depth against the capped one is
       checked 3 dB under, above). */
    for (size_t k = 0; k < nrows; k++)
      if (rows[k].autorow)
        {
          DP_CHECK_MSG (rows[k].rh < top->rh,
                        "auto picks FEWER hypotheses than the maximum "
                        "explicit count");
          DP_CHECK_MSG (rows[k].cap_hi >= top->cap_lo,
                        "auto is no worse than the most compensated count");
        }
    /* Pfa: every row, the uncompensated one included, is not significantly
       above the target. Rh = 1 read about twice it until the gate was
       priced over the interpolated cells its maximum ranges over
       (doppler#2218); the rows with Rh > 1 sit under it, because each
       hypothesis is priced as a full look (#2287). */
    for (size_t i = 0; i < NRH; i++)
      DP_CHECK_MSG (prow[i].lo <= PFA,
                    "per-dwell Pfa is not significantly above the target");
  }

  /* The same claims on a repeated LFM preamble. ZC stays the geometry above;
     here the preamble is swapped for the generator's chirp and the design
     point recomputed for it (cn0_for), then the capped depth against five
     hypotheses. Two rows and one Pfa row: the claim is "the compensation is
     not specific to ZC", which two counts and the false-alarm bound show;
     the full grid above is ZC's. As above, the capped capture delivers its
     own pd_burst at the design point, and the depth comparison is the
     engines' 3 dB under it. Full run (1000 trials, the library's rotation):
     the capped capture 0.924 against its pd_burst of 0.842 (engine 0.982);
     3 dB under, engine Pd 0.471 capped against 0.760 at N = 5. */
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
    DP_CHECK_MSG (lrow[0].cap_hi >= lrow[0].pred,
                  "LFM: the capped capture delivers its pd_burst");
    pd_row_t lu[2];
    for (size_t i = 0; i < 2; i++)
      {
        lu[i]
            = measure_pd (lrh[i], 0, cn0_l - 3.0, trials, 5252u + (uint32_t)i);
        printf ("%3zu %3zu %6.3f | %6.3f %6.3f %6.3f | %6.3f %6.3f %6.3f"
                "  (3 dB under)\n",
                lu[i].rh, lu[i].depth, lu[i].pred, lu[i].eng, lu[i].eng_lo,
                lu[i].eng_hi, lu[i].cap, lu[i].cap_lo, lu[i].cap_hi);
      }
    DP_CHECK_MSG (lu[1].eng_lo > lu[0].eng_hi,
                  "LFM: compensation's depth beats the capped depth 3 dB "
                  "under the design point, outside the intervals");
    DP_CHECK_MSG (lu[1].cap_lo > lu[0].cap_hi,
                  "LFM: compensation's depth reaches the capture 3 dB "
                  "under the design point, outside the intervals");
    pfa_row_t lp = measure_pfa (5, cn0_l, dwells, 6161u, 0);
    printf ("LFM Pfa, Rh %zu D %zu: %.2e (95%%: %.2e - %.2e)\n", lp.rh,
            lp.depth, lp.pfa, lp.lo, lp.hi);
    DP_CHECK_MSG (lp.lo <= PFA,
                  "LFM: per-dwell Pfa is not significantly above the target");
  }

  /* And on a PN preamble, the binary m-sequence family: the same two counts,
     the same claims, the design point recomputed for it. Neither this block
     nor LFM's guards the de-chirp itself: with it removed and the depth
     kept, both depth checks stay green, because depth is what they measure.
     test_acq_core.c's sweep test holds it exactly (the winning hypothesis
     at +-1e7 Hz/s, either sign, :1147), and ZC's reported-rate rows above
     hold it statistically. */
  {
    DP_CHECK_MSG (pn_preamble (),
                  "the PN preamble is one m-sequence period: its periodic "
                  "autocorrelation is N at lag 0 and -1 at every other");
    const double cn0_p  = cn0_for (9, PD);
    const size_t prh[2] = { 1, 5 };
    pd_row_t     prw[2];
    printf ("\nrepeated PN (m-sequence, dp_mls_poly(7)), C/N0 %.2f dB\n",
            cn0_p);
    printf ("%3s %3s %6s | %6s %6s %6s | %6s %6s %6s\n", "Rh", "D", "pred",
            "engine", "lo95", "hi95", "captur", "lo95", "hi95");
    for (size_t i = 0; i < 2; i++)
      {
        prw[i] = measure_pd (prh[i], 0, cn0_p, trials, 7171u + (uint32_t)i);
        printf ("%3zu %3zu %6.3f | %6.3f %6.3f %6.3f | %6.3f %6.3f %6.3f\n",
                prw[i].rh, prw[i].depth, prw[i].pred, prw[i].eng,
                prw[i].eng_lo, prw[i].eng_hi, prw[i].cap, prw[i].cap_lo,
                prw[i].cap_hi);
      }
    DP_CHECK_MSG (prw[1].cap_hi >= prw[1].pred,
                  "PN: compensated capture delivers its pd_burst");
    DP_CHECK_MSG (prw[0].cap_hi >= prw[0].pred,
                  "PN: the capped capture delivers its pd_burst");
    pd_row_t pu[2];
    for (size_t i = 0; i < 2; i++)
      {
        pu[i]
            = measure_pd (prh[i], 0, cn0_p - 3.0, trials, 7272u + (uint32_t)i);
        printf ("%3zu %3zu %6.3f | %6.3f %6.3f %6.3f | %6.3f %6.3f %6.3f"
                "  (3 dB under)\n",
                pu[i].rh, pu[i].depth, pu[i].pred, pu[i].eng, pu[i].eng_lo,
                pu[i].eng_hi, pu[i].cap, pu[i].cap_lo, pu[i].cap_hi);
      }
    DP_CHECK_MSG (pu[1].eng_lo > pu[0].eng_hi,
                  "PN: compensation's depth beats the capped depth 3 dB "
                  "under the design point, outside the intervals");
    DP_CHECK_MSG (pu[1].cap_lo > pu[0].cap_hi,
                  "PN: compensation's depth reaches the capture 3 dB "
                  "under the design point, outside the intervals");
    pfa_row_t pp = measure_pfa (5, cn0_p, dwells, 8181u, 0);
    printf ("PN Pfa, Rh %zu D %zu: %.2e (95%%: %.2e - %.2e)\n", pp.rh,
            pp.depth, pp.pfa, pp.lo, pp.hi);
    DP_CHECK_MSG (pp.lo <= PFA,
                  "PN: per-dwell Pfa is not significantly above the target");
  }
  DP_TEST_END ("validate_acq_rate_pd");
}
