/* STREAM-style triad a = b + s*c over 3 x 256 MB float arrays: the memory
 * bandwidth ceiling, 1 thread and all threads (#3047). */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define N (64u * 1024u * 1024u)

static double
run (float *a, const float *b, const float *c, int nt)
{
  double best = 1e9;
  for (int r = 0; r < 6; r++)
    {
      double t0 = omp_get_wtime ();
#pragma omp parallel for num_threads(nt) schedule(static)
      for (size_t i = 0; i < N; i++)
        a[i] = b[i] + 1.5f * c[i];
      double t = omp_get_wtime () - t0;
      if (t < best)
        best = t;
    }
  return 3.0 * N * sizeof (float) / best / 1e9;
}

int
main (void)
{
  float *a = malloc (N * sizeof *a), *b = malloc (N * sizeof *b),
        *c = malloc (N * sizeof *c);
  int nmax = omp_get_max_threads ();
#pragma omp parallel for num_threads(nmax) schedule(static)
  for (size_t i = 0; i < N; i++)
    a[i] = 0, b[i] = 1, c[i] = 2; /* first touch spread over threads */
  int ts[] = { 1, 2, 4, 8, nmax };
  for (int i = 0; i < 5; i++)
    if (ts[i] <= nmax)
      printf ("threads=%2d triad %6.1f GB/s\n", ts[i], run (a, b, c, ts[i]));
  return a[N / 2] == 4.0f ? 0 : 1;
}
