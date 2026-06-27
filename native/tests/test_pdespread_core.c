/**
 * @file test_pdespread_core.c
 * @brief Unit tests for the partial-correlation despreader.
 *
 * Tests (all carrier-free, code-aligned, asynchronous data clock):
 *   1. NULL-code / zero-k guard
 *   2. Emits ~k partial prompts per code epoch
 *   3. The partials carry the data: a genie symbol despread (known timing)
 *      recovers the bits with zero errors
 *   4. Non-coherent code tracking holds under code Doppler + async data
 *   5. Reset reproducibility
 */
#include "pdespread/pdespread_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define SF 127
#define SPS 8
#define TE (SF * SPS)

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          _fails++;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

/* deterministic 0/1 spreading code (xorshift bits) */
static void
make_code (uint8_t *code)
{
  uint32_t st = 0x12345u;
  for (int i = 0; i < SF; i++)
    {
      st ^= st << 13;
      st ^= st >> 17;
      st ^= st << 5;
      code[i] = (st & 1u) ? 1u : 0u;
    }
}

/* Carrier-free DSSS, code at rate (1+dcode), data on an independent clock
 * Tsym = TE*(1+dsym) with phase phi. Noiseless. Returns the sample count. */
static size_t
gen (float complex *rx, const uint8_t *code, int nsym, double dcode,
     double dsym, double phi, int *data)
{
  double   Tsym = TE * (1.0 + dsym);
  size_t   N    = (size_t)(nsym * Tsym) + 2 * TE;
  uint32_t ds   = 99u;
  for (int i = 0; i < nsym + 6; i++)
    {
      ds ^= ds << 13;
      ds ^= ds >> 17;
      ds ^= ds << 5;
      data[i] = (ds & 1u) ? 1 : -1;
    }
  double cph = 0.0;
  for (size_t n = 0; n < N; n++)
    {
      long si = (long)floor (((double)n - phi) / Tsym);
      if (si < 0)
        si = 0;
      if (si >= nsym + 6)
        si = nsym + 5;
      int   ci = ((int)cph) % SF;
      float cs = (code[ci] & 1u) ? -1.0f : 1.0f;
      rx[n]    = (float)data[si] * cs;
      cph += (1.0 + dcode) / SPS;
    }
  return N;
}

int
main (void)
{
  int     _fails = 0;
  uint8_t code[SF];
  make_code (code);
  const int      K    = 4;
  int            nsym = 4000;
  size_t         Nmax = (size_t)(nsym * TE * 1.05) + 4 * TE;
  float complex *rx   = malloc (Nmax * sizeof (*rx));
  float complex *out  = malloc (Nmax * sizeof (*out));
  int           *data = malloc ((nsym + 8) * sizeof (int));

  /* 1. guards */
  CHECK (pdespread_create (NULL, 0, SPS, K, 0.0, 0.002, 0.707, 0.5) == NULL);
  CHECK (pdespread_create (code, SF, SPS, 0, 0.0, 0.002, 0.707, 0.5) == NULL);

  /* 2. emit ~K partials per epoch; 3. partials carry the data (genie BER) */
  double             phi  = 0.37 * TE;
  double             dsym = 3e-3, Tsym = TE * (1.0 + dsym);
  size_t             N = gen (rx, code, nsym, 0.0, dsym, phi, data);
  pdespread_state_t *p
      = pdespread_create (code, SF, SPS, K, 0.0, 0.002, 0.707, 0.5);
  CHECK (p != NULL);
  size_t np  = pdespread_steps (p, rx, N, out, Nmax);
  size_t nep = N / TE;
  CHECK (np >= (nep - 1) * (size_t)K && np <= (nep + 1) * (size_t)K);
  CHECK (pdespread_get_k (p) == (size_t)K);

  double *acc = calloc (nsym + 8, sizeof (double));
  for (size_t pp = 0; pp < np; pp++)
    {
      double t = (double)TE * ((double)pp + 0.5) / K; /* no code Doppler */
      long   s = (long)floor ((t - phi) / Tsym);
      if (s >= 0 && s < nsym)
        acc[s] += creal (out[pp]);
    }
  long err = 0, tot = 0;
  for (int s = 2; s < nsym - 2; s++)
    {
      int d = acc[s] >= 0 ? 1 : -1;
      if (d != data[s])
        err++;
      tot++;
    }
  CHECK (tot > 0 && err == 0); /* genie symbol despread on partials: BER 0 */
  pdespread_destroy (p);
  free (acc);

  /* 4. non-coherent code tracking under code Doppler + async data */
  double dcode = 1e-4;
  N            = gen (rx, code, nsym, dcode, dsym, phi, data);
  p            = pdespread_create (code, SF, SPS, K, 0.0, 0.002, 0.707, 0.5);
  np           = pdespread_steps (p, rx, N, out, Nmax);
  double mp    = 0.0;
  size_t h     = np / 2;
  for (size_t i = h; i < np; i++)
    mp += fabs (creal (out[i]));
  mp /= (double)(np - h);
  CHECK (fabs (pdespread_get_code_rate (p) - (1.0 + dcode)) < 5e-4);
  CHECK (fabs (pdespread_get_last_error (p)) < 0.3);
  CHECK (mp > 0.3);
  pdespread_destroy (p);

  /* 5. reset reproducibility */
  p         = pdespread_create (code, SF, SPS, K, 0.0, 0.002, 0.707, 0.5);
  size_t n1 = pdespread_steps (p, rx, N, out, Nmax);
  double r1 = pdespread_get_code_rate (p);
  pdespread_reset (p);
  size_t n2 = pdespread_steps (p, rx, N, out, Nmax);
  double r2 = pdespread_get_code_rate (p);
  CHECK (n1 == n2 && r1 == r2);
  pdespread_destroy (p);

  free (rx);
  free (out);
  free (data);
  if (_fails)
    {
      fprintf (stderr, "test_pdespread_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_pdespread_core PASSED\n");
  return 0;
}
