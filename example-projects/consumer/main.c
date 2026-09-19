/*
 * main.c — a minimal downstream consumer of the doppler C library.
 *
 * Runs the local oscillator at a quarter of the sample rate, where its output
 * is known exactly: the phasor turns 90 degrees per sample, cycling
 * 1, j, -1, -j. Exit status 0 means the library linked, ran and produced
 * those samples -- not merely that it linked.
 *
 * It uses only the pure-C core, so it builds and runs on every platform
 * doppler publishes for, Windows included, and links against the core alone
 * with just the math library. It writes no file.
 *
 * Builds against either link mode; see CMakeLists.txt (find_package) and the
 * pkg-config commands in docs/install/c.md.
 */
#include "dp_complex.h"
#include <lo/lo_core.h>
#include <math.h>
#include <stdio.h>

#define N 8

int
main (void)
{
  static const float want_re[4] = { 1.0f, 0.0f, -1.0f, 0.0f };
  static const float want_im[4] = { 0.0f, 1.0f, 0.0f, -1.0f };

  lo_state_t *lo = lo_create (0.25);
  if (!lo)
    {
      fprintf (stderr, "lo_create failed\n");
      return 1;
    }
  float _Complex x[N];
  lo_steps (lo, N, x, N);
  lo_destroy (lo);

  int bad = 0;
  for (int i = 0; i < N; i++)
    if (fabsf (crealf (x[i]) - want_re[i % 4]) > 1e-5f
        || fabsf (cimagf (x[i]) - want_im[i % 4]) > 1e-5f)
      bad++;
  printf ("lo at fs/4: %d of %d samples off the 1, j, -1, -j cycle\n", bad, N);
  return bad != 0;
}
