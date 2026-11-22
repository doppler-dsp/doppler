/**
 * @file ofdm_fft_error.c
 * @brief The float32 error of an OFDM render, per transform length: the
 *        modulator's unknown "how far the rendered samples sit from the exact
 *        values, per n_fft" (docs/design/ofdm.md, section 7).
 *
 * One class of n_fft points, every subcarrier a seeded unit-power QPSK point,
 * scale 1/sqrt(n_fft), no prefix and no window, so a body is exactly
 * scale * IDFT(grid). Each body is compared with the same sum in double:
 * written out directly up to 1024 points, and above that the library's cf64
 * transform, whose own error (log2 N * 2^-53) is nine orders under what is
 * measured. The measure is the relative l2 error of one body,
 * ||x - y|| / ||y||, reported as the rms and the maximum over the symbols.
 *
 * What runs depends on the length (pocketfft.c, pffft_cf32_ok): a length
 * that is a multiple of 16 and 5-smooth takes pffft's native-float transform;
 * any other takes pocketfft in double and rounds the result to float. The
 * report reads the error against that split; this harness decides nothing
 * beyond the two checks below.
 *
 * The bound for a power of two (test_ofdm_core.c, section 11, derives it):
 *   L eta / (1 - L eta) + 2u,  eta = mu + gamma_4 (sqrt 2 + mu), mu = 20u,
 * with L = log2 N; the 2u are the float scale's rounding and its multiply.
 * The bound for a length the double path renders: the double transform's
 * error, the rounding to float (u per sample) and the scale's two roundings,
 * 3u + L_d eta_d.
 *
 *   ofdm_fft_error            the full sweep, printed
 *   ofdm_fft_error --emit     the full sweep as CSV blocks for validate.py
 *   ofdm_fft_error --check    the regression subset ctest runs: N = 64
 *                             (pffft) and N = 704 (the double path), each
 *                             against its bound
 *   ofdm_fft_error --check --emit   that subset as CSV (the report's call)
 */
#include "doppler/fft/fft_core.h"
#include "doppler/ofdm/ofdm_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TWO_PI 6.283185307179586476925286766559

typedef struct
{
  unsigned n;       /* transform length */
  unsigned symbols; /* bodies measured */
  double   rms;     /* rms over the bodies of the relative l2 error */
  double   worst;   /* the largest */
  double   bound;   /* derived bound, or NAN where none is derived */
  int      direct;  /* 1: direct double sum; 0: the cf64 transform */
} row_t;

static double
u32 (void)
{
  return ldexp (1.0, -24);
}

/* The derived bound for a power of two, as test_ofdm_core.c section 11. */
static double
bound_pow2 (unsigned n)
{
  const double u   = u32 ();
  const double L   = log2 ((double)n);
  const double mu  = 20.0 * u;
  const double g4  = 4.0 * u / (1.0 - 4.0 * u);
  const double eta = mu + g4 * (sqrt (2.0) + mu);
  return L * eta / (1.0 - L * eta) + 2.0 * u;
}

/* The bound for a length rendered in double and rounded to float. */
static double
bound_double_path (unsigned n)
{
  const double ud  = ldexp (1.0, -53);
  const double L   = ceil (log2 ((double)n));
  const double g4  = 4.0 * ud / (1.0 - 4.0 * ud);
  const double eta = ud + g4 * (sqrt (2.0) + ud);
  return 3.0 * u32 () + L * eta / (1.0 - L * eta);
}

static int
is_pow2 (unsigned n)
{
  return n && !(n & (n - 1));
}

static row_t
measure (unsigned n, unsigned symbols, uint32_t seed)
{
  row_t r = { n, symbols, 0, 0, NAN, n <= 1024u };
  if (is_pow2 (n))
    r.bound = bound_pow2 (n);

  uint8_t         *map = malloc (n);
  float _Complex  *pts = malloc ((size_t)n * symbols * sizeof *pts);
  float _Complex  *out = malloc ((size_t)n * symbols * sizeof *out);
  double _Complex *g   = malloc ((size_t)n * sizeof *g);
  double _Complex *y   = malloc ((size_t)n * sizeof *y);
  double _Complex *tw  = malloc ((size_t)n * sizeof *tw);
  if (!map || !pts || !out || !g || !y || !tw)
    {
      fprintf (stderr, "ofdm_fft_error: out of memory at n = %u\n", n);
      exit (2);
    }
  memset (map, 1, n);
  const float a = (float)sqrt (0.5);
  for (size_t i = 0; i < (size_t)n * symbols; i++)
    {
      const uint32_t b = dp_xs32 (&seed);
      pts[i] = a * ((b & 1u ? 1.0f : -1.0f) + (b & 2u ? 1.0f : -1.0f) * I);
    }

  const double     scale = 1.0 / sqrt ((double)n);
  dp_ofdm_state_t *o     = dp_ofdm_create ();
  if (dp_ofdm_add_class (o, n, map, n, 0u, 0u, 0u, scale, 1u) != 0
      || dp_ofdm_add_run (o, 0u, symbols, OFDM_CP_DEFAULT, 0u, 0u) != 0
      || dp_ofdm_modulate (o, pts, (size_t)n * symbols, out,
                           (size_t)n * symbols)
             != (size_t)n * symbols)
    {
      fprintf (stderr, "ofdm_fft_error: the render refused n = %u\n", n);
      exit (2);
    }
  dp_ofdm_destroy (o);

  /* exp (+j 2 pi t / n) for each residue t, taken once */
  for (unsigned t = 0; t < n; t++)
    tw[t] = cexp (I * TWO_PI * (double)t / (double)n);
  dp_fft_state_t *ref  = r.direct ? NULL : dp_fft_create (n, +1, 1);
  double          sum2 = 0;
  for (unsigned s = 0; s < symbols; s++)
    {
      /* the grid in transform order: index k + n/2 holds subcarrier k */
      for (unsigned i = 0; i < n; i++)
        {
          const int k = (int)i - (int)(n / 2);
          g[(unsigned)((k + (int)n) % (int)n)]
              = (double _Complex)pts[(size_t)s * n + i];
        }
      if (r.direct)
        for (unsigned m = 0; m < n; m++)
          {
            double _Complex acc = 0;
            for (unsigned k = 0; k < n; k++)
              acc += g[k] * tw[((uint64_t)k * m) % n];
            y[m] = acc;
          }
      else
        dp_fft_execute_cf64 (ref, g, n, y, n);
      double num = 0, den = 0;
      for (unsigned m = 0; m < n; m++)
        {
          const double _Complex want = scale * y[m];
          const double _Complex d
              = (double _Complex)out[(size_t)s * n + m] - want;
          num += creal (d) * creal (d) + cimag (d) * cimag (d);
          den += creal (want) * creal (want) + cimag (want) * cimag (want);
        }
      const double e = sqrt (num / den);
      sum2 += e * e;
      if (e > r.worst)
        r.worst = e;
    }
  r.rms = sqrt (sum2 / symbols);
  if (ref)
    dp_fft_destroy (ref);
  free (map);
  free (pts);
  free (out);
  free (g);
  free (y);
  free (tw);
  return r;
}

static void
print_rows (const row_t *rows, size_t n_rows, int emit)
{
  if (emit)
    {
      printf ("# fft_error\n");
      printf ("n_fft,symbols,reference,rel_rms,rel_max,bound\n");
      for (size_t i = 0; i < n_rows; i++)
        printf ("%u,%u,%s,%.6e,%.6e,%.6e\n", rows[i].n, rows[i].symbols,
                rows[i].direct ? "direct" : "cf64", rows[i].rms, rows[i].worst,
                rows[i].bound);
      return;
    }
  printf ("%7s %8s %9s %12s %12s %12s\n", "n_fft", "symbols", "reference",
          "rel_rms", "rel_max", "bound");
  for (size_t i = 0; i < n_rows; i++)
    printf ("%7u %8u %9s %12.3e %12.3e %12.3e\n", rows[i].n, rows[i].symbols,
            rows[i].direct ? "direct" : "cf64", rows[i].rms, rows[i].worst,
            rows[i].bound);
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

  if (check)
    {
      /* The regression subset: one length per path, each against the bound
       * derived for it. Control: the bound of the double path is below what
       * the pffft length measures, so the two bounds are not one number. */
      row_t rows[2] = { measure (64u, 8u, 2026u), measure (704u, 8u, 2027u) };
      rows[1].bound = bound_double_path (704u);
      print_rows (rows, 2, emit);
      DP_CHECK_MSG (rows[0].worst <= rows[0].bound,
                    "N = 64 (pffft) within the derived radix-2 bound");
      DP_CHECK_MSG (rows[1].worst <= rows[1].bound,
                    "N = 704 (double path) within its derived bound");
      DP_CHECK_MSG (rows[1].bound < rows[0].bound,
                    "control: the two paths have different bounds");
      if (emit)
        DP_TEST_EMIT_END ("ofdm_fft_error --check");
      DP_TEST_END ("ofdm_fft_error --check");
    }

  /* The full sweep: every power of two to OFDM_N_FFT_MAX, then the wlan
   * transform 64 M at the odd M up to 15, which are not powers of two. The
   * symbol count keeps about 2^20 samples a row. */
  static const unsigned odd_m[] = { 3, 5, 7, 9, 11, 13, 15 };
  row_t                 rows[32];
  size_t                n_rows = 0;
  uint32_t              seed   = 1u;
  for (unsigned n = 2; n <= OFDM_N_FFT_MAX; n *= 2)
    {
      const unsigned sym = n >= 1024u ? (1u << 20) / n : 64u;
      rows[n_rows++]     = measure (n, sym < 8u ? 8u : sym, seed++);
    }
  for (size_t i = 0; i < sizeof odd_m / sizeof *odd_m; i++)
    {
      const unsigned n = 64u * odd_m[i];
      rows[n_rows]     = measure (n, 64u, seed++);
      n_rows++;
    }
  print_rows (rows, n_rows, emit);
  /* The same assertions as --check, on every power of two the sweep took. */
  for (size_t i = 0; i < n_rows; i++)
    if (is_pow2 (rows[i].n))
      DP_CHECK_MSG (rows[i].worst <= rows[i].bound,
                    "a power of two within the derived bound");
  if (emit)
    DP_TEST_EMIT_END ("ofdm_fft_error");
  DP_TEST_END ("ofdm_fft_error");
}
