/* bench_async_dsss_pool_core.c -- the population's cost per input sample
 * (design section 6.4): one push() of one epoch through a pool of twelve
 * receivers with one emitter assigned and tracking, serial and across the
 * threads the pool is given. The budget is 100 ns per output sample per
 * core at the operating point, the working target half of that; the
 * number reported here is the whole population's, per input sample. */
#include "async_dsss_pool/async_dsss_pool_core.h"
#include "jm_bench.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SF 1023u
#define SPC 2u
#define TE (SF * SPC)
#define CHIP_RATE 5.0e6
#define FS (CHIP_RATE * (double)SPC)
#define SYM_RATE 2700.0
#define ITERATIONS 200
#define N_SLOTS 12
/* The code-only window the searcher aligns inside: 20 symbols of every
   270, holding the 31 whole epochs a depth of 16 needs at any chip phase
   (the pool refuses a searcher without one, or with a row past its
   receivers' pull-in). */
#define W_SYM 20u
#define F_SYM 270u
#define CODE_ONLY_EPOCHS 31u

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int i = 1; i < n; i++)
    if (t[i] < m)
      m = t[i];
  return m;
}

/* A clean DSSS capture built here rather than pulled from native/tests's
   dp_dsss_capture(), for the reason bench_async_dsss_receiver_core.c
   gives: a benchmark that includes a test header acquires the test tree's
   build wiring. Async BPSK data on the code at a carrier offset; the cost
   of a push does not depend on the noise. */
static size_t
build_capture (const uint8_t *code, size_t n_sym, double doppler_hz,
               float _Complex **out)
{
  const double    tsym = FS / SYM_RATE;
  const size_t    n    = (size_t)((double)n_sym * tsym) + 4 * TE;
  float _Complex *x    = malloc (n * sizeof *x);
  if (!x)
    return 0;
  uint32_t lfsr = 0xA57Bu;
  float    a    = 1.0f;
  size_t   sym  = (size_t)-1;
  for (size_t i = 0; i < n; i++)
    {
      size_t si = (size_t)((double)i / tsym);
      if (si != sym)
        {
          lfsr = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
          /* The window: the first W_SYM symbols of every F_SYM carry the
             code alone (+1), the synth's rule. */
          a   = (si % F_SYM < W_SYM) ? 1.0f : (lfsr & 1u) ? -1.0f : 1.0f;
          sym = si;
        }
      double ph = 2.0 * M_PI * doppler_hz / FS * (double)i;
      x[i]      = a * (code[(i / SPC) % SF] ? -1.0f : 1.0f)
                  * (float _Complex) (cos (ph) + I * sin (ph));
    }
  *out = x;
  return n;
}

int
main (void)
{
  /* A hashed 1023-chip code, the receiver bench's own way. */
  uint8_t  code[SF];
  uint32_t h = 0x9E3779B9u;
  for (size_t i = 0; i < SF; i++)
    {
      h ^= h << 13;
      h ^= h >> 17;
      h ^= h << 5;
      code[i] = (uint8_t)(h & 1u);
    }
  float _Complex *x;
  size_t          n = build_capture (code, 2700, 1500.0, &x);
  if (!n)
    return 1;

  jm_bench_t _bench = { 0 };
  printf ("=== async_dsss_pool benchmark ===\n");
  printf ("block = %u samples (one epoch), %d slots, %d iterations\n\n", TE,
          N_SLOTS, ITERATIONS);
  const int threads[2] = { 1, 0 }; /* serial, then every core */
  for (int k = 0; k < 2; k++)
    {
      async_dsss_pool_state_t *p = async_dsss_pool_create (
          code, SF, CHIP_RATE, SYM_RATE, SPC, 2, 47.0, 1e-2, 0.9, 6000.0,
          CODE_ONLY_EPOCHS, 0.0, 4, N_SLOTS, threads[k], 0.0, 2.0, 0.0, 4, 8,
          0, ASYNC_DSSS_RX_CELL_GAIN, ASYNC_DSSS_RX_CELL_PULLIN);
      if (!p)
        return 1;
      /* Warm: the emitter acquired and tracking before the timed pushes. */
      size_t pos = 0;
      for (; pos + TE <= n / 2; pos += TE)
        (void)async_dsss_pool_push (p, x + pos, TE);
      double          times[ITERATIONS];
      struct timespec t0, t1;
      size_t          sink = 0;
      for (int r = 0; r < ITERATIONS; r++)
        {
          if (pos + TE > n)
            pos = n / 2;
          clock_gettime (CLOCK_MONOTONIC, &t0);
          sink += async_dsss_pool_push (p, x + pos, TE);
          clock_gettime (CLOCK_MONOTONIC, &t1);
          times[r] = elapsed_sec (&t0, &t1);
          pos += TE;
        }
      char name[64];
      (void)snprintf (name, sizeof name, "push[%d slots, %s]", N_SLOTS,
                      threads[k] == 1 ? "serial" : "all cores");
      jm_bench_add (&_bench, name, times, ITERATIONS, (int)TE);
      double sec = min_sec (times, ITERATIONS);
      printf ("  %-28s %8.3f us/epoch  %7.2f ns/sample  (assigned %zu)\n",
              name, sec * 1e6, sec / (double)TE * 1e9, sink / ITERATIONS);
      async_dsss_pool_destroy (p);
    }
  jm_bench_write_json (&_bench, "async_dsss_pool");
  free (x);
  return 0;
}
