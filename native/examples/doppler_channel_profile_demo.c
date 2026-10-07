/**
 * doppler_channel_profile_demo.c — one cosine period drives the Doppler.
 *
 * dp_doppler_channel_create(fs, fc, d0, ddot) is a straight line in time. A
 * real pass is not one: the range rate swings from closing to opening and
 * back, so the Doppler it imposes is a curve. dp_doppler_channel_execute_
 * profile() takes that curve as an array, one ppm value per input sample.
 *
 * The control here is ONE FULL PERIOD of a cosine,
 *
 *     d(t) = A cos(2 pi t / T),   A = 20 ppm,   T = 0.25 s,
 *
 * which no (d0, ddot) can express and which integrates in closed form, so
 * every claim below is checked against an exact answer:
 *
 *   - the carrier offset is fc * d(t): +/-50 kHz at 2.5 GHz;
 *   - the time base dilates by A/w sin(w t), so the code slips by Rc times
 * that and RETURNS TO ZERO when the period ends;
 *   - so the stream is as long at the end as at the start.
 *
 * This is the C twin of src/doppler/examples/doppler_channel_profile_demo.py,
 * and it is where the lifecycle a caller must manage is visible: create, size
 * the output from the INPUT length, feed blocks, own the buffers, destroy. The
 * Python binding does all of that for you. The capture is never held whole:
 * each block is generated, run, measured and dropped.
 *
 * The result must not depend on how the stream was fed, so it is run twice, in
 * blocks of two unrelated sizes, and the outputs are compared by hash.
 *
 * Exits non-zero if any check fails.
 *
 * Build:
 *   make build
 *   ./build/native/examples/doppler_channel_profile_demo
 */

#include <doppler/doppler_channel/doppler_channel_core.h>

#include "doppler/dp_complex.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CHIP_RATE 3.069e6 /* chips/s                                      */
#define SPC 8             /* samples per chip                             */
#define FS (CHIP_RATE * SPC)
#define FC 2.5e9        /* RF carrier: load-bearing, not metadata        */
#define AMP_PPM 20.0    /* +/-50 kHz at 2.5 GHz                          */
#define PERIOD_S 0.25   /* one cosine period                             */
#define SETTLE 64       /* output samples dropped while the filter fills */
#define BLOCK_A 262144u /* first feed size                               */
#define BLOCK_B 99991u  /* second feed size: prime, unrelated to the first */

static int failed;

static void
check (int ok, const char *what)
{
  printf ("  %-62s %s\n", what, ok ? "ok" : "FAIL");
  if (!ok)
    failed = 1;
}

/* What one pass over the whole period measured. */
typedef struct
{
  uint64_t hash;           /* FNV-1a over every output byte, in order        */
  size_t   n_out;          /* total output samples                           */
  double   worst_hz;       /* worst |measured - fc*d(t)| over the blocks     */
  double   peak_hz;        /* largest measured block offset                  */
  double   worst_slip;     /* worst |measured - theory| code slip, samples   */
  double   peak_slip;      /* largest theory slip, chips                     */
  double   last_offset_hz; /* get_offset_hz() after the final block     */
} pass_t;

/* Run the whole period once, feeding `block` input samples at a time. */
static int
run_pass (size_t block, size_t n_total, pass_t *r, int verbose)
{
  const double w = 2.0 * M_PI / PERIOD_S;

  /* A profile is ABSOLUTE: the channel is created with no Doppler of its own
   * and the array is the whole story. */
  dp_doppler_channel_state_t *ch
      = dp_doppler_channel_create (FS, FC, 0.0, 0.0);
  if (!ch)
    return -1;

  /* The output buffer is sized from the INPUT length, because the caller has
   * not yet looked at the profile: dp_..._execute_profile_max_out() allows the
   * 2x expansion the profile validation permits. */
  size_t          cap = dp_doppler_channel_execute_profile_max_out (ch, block);
  float _Complex *in  = malloc (block * sizeof *in);
  double         *ppm = malloc (block * sizeof *ppm);
  float _Complex *out = malloc (cap * sizeof *out);
  if (!in || !ppm || !out)
    {
      free (in);
      free (ppm);
      free (out);
      dp_doppler_channel_destroy (ch);
      return -1;
    }
  for (size_t i = 0; i < block; i++)
    in[i] = 1.0f; /* DC: any phase in the output is the channel's alone */

  r->hash       = 1469598103934665603ULL;
  r->n_out      = 0;
  r->worst_hz   = 0.0;
  r->peak_hz    = 0.0;
  r->worst_slip = 0.0;
  r->peak_slip  = 0.0;

  size_t n_in         = 0;
  float _Complex prev = 0.0f;
  int  have_prev      = 0;
  long seen           = 0; /* outputs seen so far, for the settle skip */

  if (verbose)
    printf ("\n  %8s  %12s  %12s  %10s  %10s\n", "t (ms)", "offset (Hz)",
            "theory (Hz)", "slip (chips)", "theory");

  for (size_t off = 0; off < n_total; off += block)
    {
      size_t n = (n_total - off < block) ? n_total - off : block;
      for (size_t i = 0; i < n; i++)
        ppm[i]
            = AMP_PPM * cos (2.0 * M_PI * (double)(off + i) / (double)n_total);

      /* A refusal is negative, never a short count (doppler#1869). */
      int64_t got_s
          = dp_doppler_channel_execute_profile (ch, in, n, ppm, n, out, cap);
      if (got_s < 0)
        {
          (void)fprintf (stderr, "execute_profile refused (%d)\n", (int)got_s);
          return 1;
        }
      size_t got = (size_t)got_s;

      /* Mean phase step over this block's outputs, and the mean of the theory
       * fc*d(t) over the SAME steps, so the comparison is exact in the
       * averaging rather than approximated by a window formula. */
      double acc = 0.0, th = 0.0;
      long   steps = 0;
      for (size_t k = 0; k < got; k++)
        {
          const unsigned char *b = (const unsigned char *)&out[k];
          for (size_t q = 0; q < sizeof out[k]; q++)
            {
              r->hash ^= b[q];
              r->hash *= 1099511628211ULL;
            }
          if (seen++ < SETTLE)
            continue; /* |y| ramps from 0 while the filter fills */
          if (have_prev)
            {
              acc += atan2 ((double)cimagf (out[k] * conjf (prev)),
                            (double)crealf (out[k] * conjf (prev)));
              th += cos (w * ((double)(seen - 1) - 0.5) / FS);
              steps++;
            }
          prev      = out[k];
          have_prev = 1;
        }
      r->n_out += got;
      n_in += n;

      if (steps > 0)
        {
          double meas = acc / (double)steps * FS / (2.0 * M_PI);
          double thy  = FC * AMP_PPM * 1e-6 * th / (double)steps;
          double e    = fabs (meas - thy);
          if (e > r->worst_hz)
            r->worst_hz = e;
          if (fabs (meas) > r->peak_hz)
            r->peak_hz = fabs (meas);

          /* Slip, in samples: the input consumed less the output produced. */
          double slip    = (double)n_in - (double)r->n_out;
          double slip_th = AMP_PPM * 1e-6 * ((double)n_total / (2.0 * M_PI))
                           * sin (2.0 * M_PI * (double)n_in / (double)n_total);
          double se      = fabs (slip - slip_th);
          if (se > r->worst_slip)
            r->worst_slip = se;
          if (fabs (slip_th) / SPC > r->peak_slip)
            r->peak_slip = fabs (slip_th) / SPC;

          if (verbose && ((off / block) % 4 == 0))
            printf ("  %8.2f  %12.1f  %12.1f  %10.3f  %10.3f\n",
                    (double)n_in / FS * 1e3, meas, thy, slip / SPC,
                    slip_th / SPC);
        }
    }

  r->last_offset_hz = dp_doppler_channel_get_offset_hz (ch);

  free (in);
  free (ppm);
  free (out);
  dp_doppler_channel_destroy (ch);
  return 0;
}

int
main (void)
{
  const size_t n_total = (size_t)llround (FS * PERIOD_S);

  printf ("=== DopplerChannel profile: one cosine period ===\n");
  printf (
      "A = %.0f ppm  T = %.2f s  fc = %.1f GHz  Rc = %.3f Mcps  spc = %d\n",
      AMP_PPM, PERIOD_S, FC / 1e9, CHIP_RATE / 1e6, SPC);
  printf ("%zu input samples, fed in blocks of %u and of %u\n", n_total,
          BLOCK_A, BLOCK_B);

  pass_t a, b;
  if (run_pass (BLOCK_A, n_total, &a, 1) != 0
      || run_pass (BLOCK_B, n_total, &b, 0) != 0)
    {
      fprintf (stderr, "doppler_channel_profile_demo: a pass failed\n");
      return 1;
    }

  const double peak = FC * AMP_PPM * 1e-6;
  printf ("\nchecks:\n");
  check (a.hash == b.hash && a.n_out == b.n_out,
         "the stream does not depend on how it was fed (hash equal)");
  printf ("      carrier offset peak %.1f Hz of %.1f, worst error %.2f Hz\n",
          a.peak_hz, peak, a.worst_hz);
  /* Measured 1.3 Hz of 50 kHz; 5e-5 of the peak is 2.5 Hz, so a 0.1% scale
   * error in the carrier (50 Hz) and a 0.05% one (25 Hz) both fail. */
  check (a.worst_hz < 5e-5 * peak, "carrier offset follows fc*d(t) to 0.005%");
  printf ("      code slip peak %.2f chips, worst error %.2f samples\n",
          a.peak_slip, a.worst_slip);
  check (a.worst_slip < 2.0,
         "code slip follows Rc * integral(d dt) to 2 samples");
  printf ("      %zu out for %zu in\n", a.n_out, n_total);
  check (llabs ((long long)a.n_out - (long long)n_total) <= 2,
         "a full period is net-zero dilation: the stream length returns");
  check (fabs (a.last_offset_hz - peak) < 1.0,
         "offset_hz after the last block is fc * the profile's last d");

  /* A bad call writes nothing, leaves the clocks alone and returns a
   * negative DP_ERR_*: 0 stays a valid, empty answer (doppler#1869), so the
   * sign alone tells a refusal from an empty result. */
  {
    dp_doppler_channel_state_t *ch
        = dp_doppler_channel_create (FS, FC, 0.0, 0.0);
    float _Complex in[16], out[64];
    double ppm[16];
    for (int i = 0; i < 16; i++)
      {
        in[i]  = 1.0f;
        ppm[i] = 10.0;
      }
    int64_t valid
        = dp_doppler_channel_execute_profile (ch, in, 16, ppm, 16, out, 64);
    double  t_valid = dp_doppler_channel_get_elapsed_s (ch);
    int64_t short_len
        = dp_doppler_channel_execute_profile (ch, in, 16, ppm, 15, out, 64);
    ppm[15] = -2.0e6; /* a scale of zero or less: time stopped */
    int64_t reversed
        = dp_doppler_channel_execute_profile (ch, in, 16, ppm, 16, out, 64);
    check (valid > 0, "the same call, with a valid profile, is accepted");
    check (short_len == DP_ERR_INVALID && reversed == DP_ERR_INVALID
               && dp_doppler_channel_get_elapsed_s (ch) == t_valid,
           "a length mismatch or a time-reversing sample is refused, "
           "writes nothing and leaves the clocks");
    dp_doppler_channel_destroy (ch);
  }

  printf ("\n%s\n", failed ? "FAILED" : "all checks passed");
  return failed;
}
