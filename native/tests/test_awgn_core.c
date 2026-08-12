#define DP_TEST_VERBOSE 1
#include "awgn/awgn_core.h"
#include "dp_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N_STAT 65536 /* samples for statistical checks */
#define N_SMALL 256

/* ------------------------------------------------------------------
 * test_lifecycle: create, destroy, NULL safety.
 * ------------------------------------------------------------------ */
static void
test_lifecycle (void)
{
  printf ("\n-- Lifecycle --\n");
  awgn_state_t *g = awgn_create (0, 1.0f);
  DP_CHECK (g != NULL);
  awgn_destroy (g);
  awgn_destroy (NULL); /* must be a no-op */
  DP_CHECK (1);        /* no crash */
}

/* ------------------------------------------------------------------
 * test_amplitude_property: get/set without disturbing RNG.
 * ------------------------------------------------------------------ */
static void
test_amplitude_property (void)
{
  printf ("\n-- Amplitude property --\n");
  awgn_state_t *g = awgn_create (1, 2.0f);
  DP_CHECK (awgn_get_amplitude (g) == 2.0f);
  awgn_set_amplitude (g, 0.5f);
  DP_CHECK (awgn_get_amplitude (g) == 0.5f);
  awgn_destroy (g);
}

/* ------------------------------------------------------------------
 * test_zero_amplitude: all outputs must be exactly 0+0j.
 * ------------------------------------------------------------------ */
static void
test_zero_amplitude (void)
{
  printf ("\n-- Zero amplitude --\n");
  awgn_state_t *g = awgn_create (0, 0.0f);
  float complex buf[N_SMALL];
  awgn_generate (g, N_SMALL, buf, N_SMALL);
  int all_zero = 1;
  for (int i = 0; i < N_SMALL; i++)
    if (buf[i] != 0.0f + 0.0f * I)
      all_zero = 0;
  DP_CHECK (all_zero);
  awgn_destroy (g);
}

/* ------------------------------------------------------------------
 * test_reset_reproducible: reset reproduces identical output.
 * ------------------------------------------------------------------ */
static void
test_reset_reproducible (void)
{
  printf ("\n-- Reset reproducible --\n");
  awgn_state_t *g = awgn_create (42, 1.0f);
  float complex a[N_SMALL], b[N_SMALL];

  awgn_generate (g, N_SMALL, a, N_SMALL);
  awgn_reset (g);
  awgn_generate (g, N_SMALL, b, N_SMALL);

  DP_CHECK (memcmp (a, b, N_SMALL * sizeof *a) == 0);
  awgn_destroy (g);
}

/* ------------------------------------------------------------------
 * test_reseed: different seeds produce different streams.
 * ------------------------------------------------------------------ */
static void
test_reseed (void)
{
  printf ("\n-- Reseed --\n");
  awgn_state_t *g = awgn_create (1, 1.0f);
  float complex a[N_SMALL], b[N_SMALL];

  awgn_generate (g, N_SMALL, a, N_SMALL);
  awgn_reseed (g, 2);
  awgn_generate (g, N_SMALL, b, N_SMALL);

  int differs = 0;
  for (int i = 0; i < N_SMALL; i++)
    if (a[i] != b[i])
      differs = 1;
  DP_CHECK (differs);

  /* reseed back to 1 should reproduce stream a */
  awgn_reseed (g, 1);
  float complex c[N_SMALL];
  awgn_generate (g, N_SMALL, c, N_SMALL);
  DP_CHECK (memcmp (a, c, N_SMALL * sizeof *a) == 0);
  awgn_destroy (g);
}

/* ------------------------------------------------------------------
 * test_statistics: mean ≈ 0, variance ≈ amplitude² per component.
 * ------------------------------------------------------------------ */
static void
test_statistics (void)
{
  printf ("\n-- Statistics (N=%d) --\n", N_STAT);
  const float   amp = 2.0f;
  awgn_state_t *g   = awgn_create (7, amp);

  float complex *buf = malloc (N_STAT * sizeof *buf);
  awgn_generate (g, N_STAT, buf, N_STAT);

  double sum_re = 0, sum_im = 0;
  double sum_re2 = 0, sum_im2 = 0;
  for (int i = 0; i < N_STAT; i++)
    {
      double re = (double)crealf (buf[i]);
      double im = (double)cimagf (buf[i]);
      sum_re += re;
      sum_im += im;
      sum_re2 += re * re;
      sum_im2 += im * im;
    }
  double mean_re = sum_re / N_STAT;
  double mean_im = sum_im / N_STAT;
  double var_re  = sum_re2 / N_STAT - mean_re * mean_re;
  double var_im  = sum_im2 / N_STAT - mean_im * mean_im;

  /* Mean within ±3σ/√N of 0 (3*amp/√65536 ≈ 0.023 for amp=2) */
  double mean_tol = 3.0 * amp / sqrt ((double)N_STAT);
  DP_CHECK (fabs (mean_re) < mean_tol);
  DP_CHECK (fabs (mean_im) < mean_tol);

  /* Variance within 2% of amp² */
  double var_tol = 0.02 * amp * amp;
  DP_CHECK (fabs (var_re - amp * amp) < var_tol);
  DP_CHECK (fabs (var_im - amp * amp) < var_tol);

  free (buf);
  awgn_destroy (g);
}

/* ------------------------------------------------------------------
 * test_split_block: split into two calls == one contiguous call.
 * ------------------------------------------------------------------ */
static void
test_split_block (void)
{
  printf ("\n-- Split-block identity --\n");
  float complex full[N_SMALL], part[N_SMALL];

  /* Full block */
  awgn_state_t *g = awgn_create (99, 1.0f);
  awgn_generate (g, N_SMALL, full, N_SMALL);
  awgn_destroy (g);

  /* Two halves */
  g           = awgn_create (99, 1.0f);
  size_t half = N_SMALL / 2;
  awgn_generate (g, half, part, half);
  awgn_generate (g, half, part + half, half);
  awgn_destroy (g);

  DP_CHECK (memcmp (full, part, N_SMALL * sizeof *full) == 0);
}

/* ------------------------------------------------------------------
 * test_oneshot: awgn() matches awgn_create+generate+destroy.
 * ------------------------------------------------------------------ */
static void
test_oneshot (void)
{
  printf ("\n-- One-shot awgn() --\n");

  float complex ref[N_SMALL];
  awgn_state_t *g = awgn_create (42, 0.7f);
  DP_CHECK (g != NULL);
  awgn_generate (g, N_SMALL, ref, N_SMALL);
  awgn_destroy (g);

  float complex out[N_SMALL];
  DP_CHECK (awgn (42, 0.7f, N_SMALL, out) == 0);
  DP_CHECK (memcmp (ref, out, N_SMALL * sizeof *out) == 0);
}

/* Advance the RNG, serialize, restore into a fresh generator, and the noise
 * stream continues bit-for-bit; a clobbered envelope rejects. */
static void
test_state_roundtrip (void)
{
  printf ("\n-- Serializable state round-trip --\n");
  enum
  {
    M = 64
  };
  float complex ref[M], got[M];

  awgn_state_t *a = awgn_create (123, 1.0f);
  awgn_generate (a, M, ref, M); /* advance past the seed state */
  size_t sb   = awgn_state_bytes (a);
  void  *blob = malloc (sb);
  awgn_get_state (a, blob);
  awgn_generate (a, M, ref, M); /* reference continuation */

  awgn_state_t *b = awgn_create (123, 1.0f);
  DP_CHECK (awgn_set_state (b, blob) == DP_OK);
  ((char *)blob)[0] ^= (char)0xFF;
  DP_CHECK (awgn_set_state (b, blob) == DP_ERR_INVALID);
  awgn_generate (b, M, got, M);
  DP_CHECK (memcmp (ref, got, sizeof ref) == 0);

  awgn_destroy (a);
  awgn_destroy (b);
  free (blob);
}

/* ------------------------------------------------------------------
 * test_stream_pinned: the sequence is PINNED, not merely self-consistent.
 *
 * gh-690: this generator shipped with two implementations selected at run
 * time, and they produced different noise from the same seed — the scalar
 * state s[0..3] and the eight AVX-512 streams vs[0..3][0..7] come from
 * different SplitMix64 draws. Every AWGN-derived number was therefore
 * platform-dependent, silently.
 *
 * Nothing here caught it, and test_reset_reproducible() is why: it compares
 * a stream to ITSELF after awgn_reset(), on one path, on one machine. That
 * passes identically under either implementation. Self-consistency is not
 * reproducibility, and only an external reference can tell them apart.
 *
 * So: reference values, recorded from the scalar path, which is the one
 * every shipped Linux build has always run. A future re-vectorisation is
 * welcome and has to reproduce these.
 *
 * The RNG and the LUT are integer and table lookups, so the real and
 * imaginary parts are exactly reproducible; the tolerance is for logf, which
 * libm does not guarantee to the last ulp across platforms. It is far below
 * the ~0.3 spacing between consecutive samples, so a WRONG stream cannot
 * hide inside it — which is the property that matters, and the reason a
 * tolerance is honest here rather than a loophole.
 * ------------------------------------------------------------------ */
static void
test_stream_pinned (void)
{
  printf ("\n-- Stream pinned (gh-690) --\n");
  static const float want_re[]
      = { -0.268593192f, -0.054472364f, -0.578596532f, -1.609373569f };
  static const float want_im[]
      = { 0.581977606f, -0.171774969f, -0.357516527f, -1.250267267f };

  awgn_state_t *g = awgn_create (42, 1.0f);
  float complex out[4];
  awgn_generate (g, 4, out, 4);
  for (size_t i = 0; i < 4; i++)
    {
      DP_CHECK_NEAR (crealf (out[i]), want_re[i], 1e-5);
      DP_CHECK_NEAR (cimagf (out[i]), want_im[i], 1e-5);
    }
  awgn_destroy (g);

  /* The head being right does not mean the state update is. */
  awgn_state_t *h = awgn_create (12345, 1.0f);
  float complex buf[4096];
  double        acc_re = 0.0, acc_im = 0.0;
  for (int r = 0; r < 64; r++)
    {
      awgn_generate (h, 4096, buf, 4096);
      for (size_t i = 0; i < 4096; i++)
        {
          acc_re += (double)crealf (buf[i]);
          acc_im += (double)cimagf (buf[i]);
        }
    }
  DP_CHECK_NEAR (acc_re, 17.390397, 0.05);
  DP_CHECK_NEAR (acc_im, -47.646037, 0.05);
  awgn_destroy (h);
}

int
main (void)
{
  test_lifecycle ();
  test_amplitude_property ();
  test_zero_amplitude ();
  test_reset_reproducible ();
  test_stream_pinned ();
  test_reseed ();
  test_statistics ();
  test_split_block ();
  test_oneshot ();
  test_state_roundtrip ();

  printf ("\n");
  DP_TEST_END ("test_awgn_core");
}
