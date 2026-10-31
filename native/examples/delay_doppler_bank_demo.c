/**
 * delay_doppler_bank_demo.c — find a code on an unknown carrier.
 *
 * A code delayed by 37 samples arrives on a carrier 1.3 code bins off
 * nominal, in noise.  dp_corr2d_bank_detect() searches the carrier on a
 * coarse grid (half bins), picks the candidates, refines each at its own
 * cell on a fine grid (quarter bins) and keeps what clears the confirm
 * threshold (docs/design/corr2d-hypotheses.md section 6).  What a C caller
 * manages, and the Python binding does for you:
 *
 *   - create once; the bank never touches the dwell, so execute() is free
 *     to run between bank calls;
 *   - every output is the caller's, at least dp_corr2d_bank_detect_max_out()
 *     long: one record per map cell is the most there can be;
 *   - thresholds are powers in the map's units, set from noise alone;
 *   - destroy once.
 *
 * Build and run:
 *   cmake --build build
 *   ./build/native/examples/delay_doppler_bank_demo
 */

#include <doppler/awgn/awgn_core.h>
#include <doppler/corr2d/corr2d_core.h>

#include "doppler/dp_complex.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum
{
  NX    = 128, /* code length, samples per period */
  NY    = 16,  /* periods per frame (slow time)   */
  N     = NY * NX,
  DELAY = 37 /* the code's delay, samples        */
};

static const double CARRIER_BINS = 1.3;   /* the carrier, 1/NX units   */
static const double SNR_DB       = -15.0; /* per sample, over the noise */

/* A +-1 code from a fixed xorshift32, so every run is the same. */
static void
make_code (float _Complex *code)
{
  unsigned s = 0x2683u;
  for (size_t p = 0; p < NX; p++)
    {
      s ^= s << 13;
      s ^= s >> 17;
      s ^= s << 5;
      code[p] = (s >> 31) ? 1.0f : -1.0f;
    }
}

/* The strongest coarse cell of one frame. */
static float
coarse_peak (dp_corr2d_state_t *dd, const float _Complex *x, float *pw,
             uint32_t *ix)
{
  if (dp_corr2d_bank_power_max (dd, x, N, 2, -4, 5, NULL, 0, pw, ix, N) != N)
    return -1.0f;
  float top = 0.0f;
  for (size_t k = 0; k < N; k++)
    top = pw[k] > top ? pw[k] : top;
  return top;
}

int
main (void)
{
  float _Complex code[NX];
  make_code (code);

  /* unit-power complex noise from the library's own generator */
  dp_awgn_state_t *awgn  = dp_awgn_create (2683, (float)sqrt (0.5));
  float _Complex  *noise = malloc (N * sizeof *noise);
  float _Complex  *x     = malloc (N * sizeof *x);

  dp_corr2d_state_t *dd
      = dp_corr2d_create_delay_doppler (code, NX, NY, 1, 1, 0);
  const size_t cap  = dp_corr2d_bank_detect_max_out (dd, N);
  float       *pw   = malloc (cap * sizeof *pw);
  uint32_t    *ix   = malloc (cap * sizeof *ix);
  float       *pow_ = malloc (cap * sizeof *pow_);
  int32_t     *g    = malloc (cap * sizeof *g);
  uint32_t    *rate = malloc (cap * sizeof *rate);
  uint32_t    *row  = malloc (cap * sizeof *row);
  uint32_t    *col  = malloc (cap * sizeof *col);

  /* Thresholds from noise alone: the strongest coarse cell in 50 noise
     frames, with margin, for the candidate; the confirm a little above. */
  float floor_ = 0.0f;
  for (int f = 0; f < 50; f++)
    {
      dp_awgn_generate (awgn, N, noise, N);
      const float p = coarse_peak (dd, noise, pw, ix);
      floor_        = p > floor_ ? p : floor_;
    }
  const float t_candidate = 1.2f * floor_, t_confirm = 1.5f * floor_;

  /* the frame: the delayed code on its carrier, plus noise */
  dp_awgn_generate (awgn, N, noise, N);
  const double amp = pow (10.0, SNR_DB / 20.0);
  for (size_t m = 0; m < N; m++)
    {
      const double a = 2.0 * M_PI * CARRIER_BINS / NX * (double)m;
      x[m]           = (float)amp * code[(m % NX + NX - DELAY) % NX]
                           * ((float)cos (a) + (float)sin (a) * I)
                       + noise[m];
    }

  const float  peak = coarse_peak (dd, x, pw, ix);
  const size_t nrec
      = dp_corr2d_bank_detect (dd, x, N, 2, 4, -4, 5, t_candidate, t_confirm,
                               NULL, 0, pow_, g, rate, row, col, cap);
  printf ("record  delay  Doppler row  carrier (bins)  refined / coarse\n");
  for (size_t k = 0; k < nrec && nrec != SIZE_MAX; k++)
    printf ("  %zu      %3u      %3u          %+5.2f           %.3f\n", k,
            col[k], row[k], g[k] / 4.0, (double)(pow_[k] / peak));

  /* One signal, one record: at the code's delay, on the quarter-bin carrier
     nearest 1.3 bins, and stronger than the coarse grid saw it. */
  const int ok = nrec == 1 && col[0] == DELAY && g[0] == 5 && pow_[0] > peak;
  printf ("%s\n", ok ? "OK" : "FAILED");

  dp_corr2d_destroy (dd);
  dp_awgn_destroy (awgn);
  free (noise), free (x), free (pw), free (ix), free (pow_), free (g);
  free (rate), free (row), free (col);
  return ok ? 0 : 1;
}
