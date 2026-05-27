#include "awgn/awgn_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N_STAT  65536   /* samples for statistical checks */
#define N_SMALL 256

#define CHECK(cond) \
    do { if (!(cond)) { \
        fprintf(stderr, "  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        _fails++; \
    } else { \
        printf("  PASS  %s\n", #cond); \
    } } while (0)

static int _fails = 0;

/* ------------------------------------------------------------------
 * test_lifecycle: create, destroy, NULL safety.
 * ------------------------------------------------------------------ */
static void
test_lifecycle (void)
{
  printf ("\n-- Lifecycle --\n");
  awgn_state_t *g = awgn_create (0, 1.0f);
  CHECK (g != NULL);
  awgn_destroy (g);
  awgn_destroy (NULL); /* must be a no-op */
  CHECK (1);           /* no crash */
}

/* ------------------------------------------------------------------
 * test_amplitude_property: get/set without disturbing RNG.
 * ------------------------------------------------------------------ */
static void
test_amplitude_property (void)
{
  printf ("\n-- Amplitude property --\n");
  awgn_state_t *g = awgn_create (1, 2.0f);
  CHECK (awgn_get_amplitude (g) == 2.0f);
  awgn_set_amplitude (g, 0.5f);
  CHECK (awgn_get_amplitude (g) == 0.5f);
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
  awgn_generate (g, N_SMALL, buf);
  int all_zero = 1;
  for (int i = 0; i < N_SMALL; i++)
    if (buf[i] != 0.0f + 0.0f * I)
      all_zero = 0;
  CHECK (all_zero);
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

  awgn_generate (g, N_SMALL, a);
  awgn_reset (g);
  awgn_generate (g, N_SMALL, b);

  CHECK (memcmp (a, b, N_SMALL * sizeof *a) == 0);
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

  awgn_generate (g, N_SMALL, a);
  awgn_reseed (g, 2);
  awgn_generate (g, N_SMALL, b);

  int differs = 0;
  for (int i = 0; i < N_SMALL; i++)
    if (a[i] != b[i])
      differs = 1;
  CHECK (differs);

  /* reseed back to 1 should reproduce stream a */
  awgn_reseed (g, 1);
  float complex c[N_SMALL];
  awgn_generate (g, N_SMALL, c);
  CHECK (memcmp (a, c, N_SMALL * sizeof *a) == 0);
  awgn_destroy (g);
}

/* ------------------------------------------------------------------
 * test_statistics: mean ≈ 0, variance ≈ amplitude² per component.
 * ------------------------------------------------------------------ */
static void
test_statistics (void)
{
  printf ("\n-- Statistics (N=%d) --\n", N_STAT);
  const float amp = 2.0f;
  awgn_state_t *g = awgn_create (7, amp);

  float complex *buf = malloc (N_STAT * sizeof *buf);
  awgn_generate (g, N_STAT, buf);

  double sum_re = 0, sum_im = 0;
  double sum_re2 = 0, sum_im2 = 0;
  for (int i = 0; i < N_STAT; i++)
    {
      double re = (double)crealf (buf[i]);
      double im = (double)cimagf (buf[i]);
      sum_re  += re;
      sum_im  += im;
      sum_re2 += re * re;
      sum_im2 += im * im;
    }
  double mean_re = sum_re / N_STAT;
  double mean_im = sum_im / N_STAT;
  double var_re  = sum_re2 / N_STAT - mean_re * mean_re;
  double var_im  = sum_im2 / N_STAT - mean_im * mean_im;

  /* Mean within ±3σ/√N of 0 (3*amp/√65536 ≈ 0.023 for amp=2) */
  double mean_tol = 3.0 * amp / sqrt ((double)N_STAT);
  CHECK (fabs (mean_re) < mean_tol);
  CHECK (fabs (mean_im) < mean_tol);

  /* Variance within 2% of amp² */
  double var_tol = 0.02 * amp * amp;
  CHECK (fabs (var_re - amp * amp) < var_tol);
  CHECK (fabs (var_im - amp * amp) < var_tol);

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
  awgn_generate (g, N_SMALL, full);
  awgn_destroy (g);

  /* Two halves */
  g = awgn_create (99, 1.0f);
  size_t half = N_SMALL / 2;
  awgn_generate (g, half, part);
  awgn_generate (g, half, part + half);
  awgn_destroy (g);

  CHECK (memcmp (full, part, N_SMALL * sizeof *full) == 0);
}

int
main (void)
{
  test_lifecycle ();
  test_amplitude_property ();
  test_zero_amplitude ();
  test_reset_reproducible ();
  test_reseed ();
  test_statistics ();
  test_split_block ();

  printf ("\n");
  if (_fails)
    {
      fprintf (stderr, "%d failed, fix before commit\n", _fails);
      return 1;
    }
  printf ("All tests passed\n");
  return 0;
}
