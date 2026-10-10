/*
 * asan_canary.c -- a bad access `make test-asan` must report, or it has not
 * looked (#2130).
 *
 * Reads one float _Complex element past the end of a heap buffer, through
 * crealf(), which is how doppler's kernels read samples (dot_cf32's
 * crealf(w[j])). GCC's AddressSanitizer does not instrument complex-typed
 * accesses. Measured on 2026-10-10 (#2130's table): gcc 15.2 reports neither
 * this read, a cimagf() read nor a whole-complex load, and gcc 13.3 (CI's
 * image) misses this read and the whole load; a complex store goes
 * unreported too. The same bytes read through a float * are caught, and
 * clang reports all three complex reads. A test-asan built with gcc
 * therefore passed while resamp read 19 samples past its delay line.
 *
 * Not a ctest test: run as one in a plain build it would read garbage and
 * exit 0. test-asan builds it with the suite's own compiler and flags, runs
 * it, and fails unless ASan reports a heap-buffer-overflow -- so a change of
 * compiler that blinds the gate turns the gate red instead.
 */
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char **argv)
{
  (void)argv;
  float _Complex *b = calloc (64, sizeof *b);
  if (!b)
    return 2;
  /* argc keeps the index opaque to the optimiser: 64 when run bare. */
  volatile float r = crealf (b[63 + argc]);
  printf ("asan_canary: read %g past the end, unreported\n", (double)r);
  free (b);
  return 0;
}
